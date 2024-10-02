#define _GNU_SOURCE
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <unistd.h>

// for testing code
#include <sys/mman.h>

//#define DEBUG


struct memory_record {
    long pf_major;
    long pf_minor;
    long vm_rss;
    long vm_pss;
    long vm_pte;
    long vm_size;
    long vm_swap;
    long vm_shared;
};

void parse_rusage(struct memory_record *r) {
    struct rusage ru;
    getrusage(RUSAGE_SELF, &ru);
    r->pf_minor = ru.ru_minflt;
    r->pf_major = ru.ru_majflt;
}

static char proc_text[1024 * 128];

static size_t read_file_into_proc_text(const char *path) {
    int fd = open(path, O_RDONLY);
    if (fd < 0) {
        perror(path);
        abort();
    }
    ssize_t bytes = 0;
    ssize_t cur_bytes;
    do {
        cur_bytes = read(fd, proc_text + bytes, sizeof(proc_text) - bytes);
        if (cur_bytes < 0) {
            perror(path);
            abort();
        }
        bytes += cur_bytes;
    } while (cur_bytes != 0 && bytes < sizeof(proc_text));
    if (bytes == sizeof(proc_text)) {
        fprintf(stderr, "file %s too big for buffer\n", path);
    }
    close(fd);
    return bytes;
}

/* FIXME: calculate real RSS from smaps */
static void parse_smaps(struct memory_record *r) {
    r->vm_rss = 0;
    r->vm_pss = 0;
    r->vm_size = 0;
    r->vm_swap = 0;
    r->vm_shared = 0;
    size_t bytes = read_file_into_proc_text("/proc/self/smaps");
    FILE *fh = fmemopen(proc_text, bytes, "r");
    char line[512];
    while (fgets(line, sizeof line, fh)) {
        char label[64]; long value;
        if (2 == sscanf(line, "%32[^:]: %ld", label, &value)) {
            if (!strcmp(label, "Rss")) {
                r->vm_rss += value * 1024; // kB
            } else if (!strcmp(label, "Pss")) {
                r->vm_pss += value * 1024; // kB
            } else if (!strcmp(label, "Size")) {
#ifdef DEBUG
                printf("found size %ld\n", value);
#endif
                r->vm_size += value * 1024; // kB
            } else if (!strcmp(label, "Swap")) {
                r->vm_swap += value * 1024; // kB
            } else if (!strcmp(label, "Shared_Clean") ||
                       !strcmp(label, "Shared_Dirty")) {
                r->vm_shared += value * 1024; // kB
            }
        }
#ifdef DEBUG
        else { printf("ignoring <%s>\n", line); }
#endif
    }
    fclose(fh);
}

static void parse_status(struct memory_record *r) {
    size_t bytes = read_file_into_proc_text("/proc/self/status");
    FILE *fh = fmemopen(proc_text, bytes, "r");
    char line[1024];
    while (fgets(line, sizeof line, fh)) {
        char label[33]; long value;
        if (2 == sscanf(line, "%32[^:]: %ld", label, &value)) {
            if (!strcmp(label, "VmRSS")) {
                r->vm_rss = value * 1024; // kB
            } else if (!strcmp(label, "VmPTE")) {
                r->vm_pte = value * 1024; // kB
            } else if (!strcmp(label, "VmSize")) {
                r->vm_size = value * 1024; // kB
            } else if (!strcmp(label, "VmSwap")) {
                r->vm_swap = value * 1024; // kB
            }
        }
    }
    fclose(fh);
}

void record_memory_record(struct memory_record *r) {
    parse_rusage(r);
    parse_status(r);
    parse_smaps(r); // deliberately after parse_status() since more precise for RSS etc.
}

void print_memory_record(FILE *out, struct memory_record *r1, struct memory_record *r2) {
#define FIELD(name, label) \
    do { \
        if (!r1) { \
            fprintf(out, "%30s: %10ld\n", label, r2->name); \
        } else { \
            fprintf(out, "%30s: %10ld (%+10ld)\n", label, r2->name, r2->name - r1->name); \
        } \
    } while (0)

    FIELD(pf_major, "major page faults");
    FIELD(pf_minor, "minor page faults");
    FIELD(vm_rss, "Resident Set Size (bytes)");
    FIELD(vm_shared, "Shared (bytes)");
    FIELD(vm_pte, "Page Table Entries (bytes)");
    FIELD(vm_size, "Virtual Memory Size (bytes)");
    FIELD(vm_swap, "Swap (bytes)");
#undef FIELD
}

/* run code to encourage everything we use for memory monitoring to be loaded */
void force_load() {
    struct memory_record temp1, temp2;
    record_memory_record(&temp1);
    record_memory_record(&temp2);
    FILE *dev_null = fopen("/dev/null", "w");
    print_memory_record(dev_null, NULL, &temp1);
    print_memory_record(dev_null, &temp1, &temp2);
    fclose(dev_null);
}

void print_maps(FILE *out) {
    size_t bytes = read_file_into_proc_text("/proc/self/maps");
    FILE *fh = fmemopen(proc_text, bytes, "r");
    long last_from = 0, last_to = 0;
    char last_name[120] = {0};
    char line[512];
    fprintf(out, "%-25s %s\n", "addresses", "usage");
    fprintf(out, "%-25s %s\n", "---------", "-----");

    while (fgets(line, sizeof line, fh)) {
        long from, to;
        char name_with_whitespace[120];
        char name[120] = {0};
        char r, w, x, p;
        int count = sscanf(line,
            "%lx-%lx %c%c%c%c %*x %*x:%*x %*d%120[^\n]", 
            &from, &to,
            &r, &w, &x, &p,
            name_with_whitespace
        );
        if (count != 7) {
            fprintf(stderr, "error parsing '%s' in /proc/self/maps\n", line);
            continue;
        }
        sscanf(name_with_whitespace, "%s", name);
        if (0 == strcmp(name, "")) {
            strcpy(name, "[dynamic allocation]");
        }
        if (r == '-' && w == '-')
            continue;
        if (0 == strcmp(name, last_name) && last_to == from) {
            last_to = to;
        } else {
            if (last_from != 0) {
                fprintf(out, "%012lx-%012lx %s\n", last_from, last_to - 1, last_name);
            }
            last_from = from;
            last_to = to;
            strcpy(last_name, name);
        }
    }
    if (last_from != 0) {
        fprintf(out, "%012lx-%012lx %s\n", last_from, last_to - 1, last_name);
    }

}

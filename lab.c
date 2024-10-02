#define _GNU_SOURCE
#include "util.h"
#include <stdio.h>      // for printf
#include <stdlib.h>     // for atoi (and malloc() which you'll likely use)
#include <sys/mman.h>   // for mmap() which you'll likely use
#include <stdalign.h>


alignas(4096) volatile char global_array[4096*32];

void labStuff(int which) {
    if (which == 0) {
        /* do nothing */
    } else if (which == 1) {
	global_array[4096*1 + 1] =0;
	global_array[4096*1 + 2] = 0;

    } else if (which == 2) {
	volatile int * int_array = malloc(1000000);
	int_array[0] =1;
	int_array[10000] = 1;

    } else if (which == 3) {
	volatile int * int_array = malloc(1048576-3520);
	for(int i = 0; i< 32; i++){
		int_array[i*4092 + 1] = 1;
	}
    } else if (which == 4) {
	volatile char *ptr;
	ptr = mmap((void *) 0x5555557BC000, 4096, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE, -1, 0);
	ptr[1] = 0;
    } else if (which == 5){
	volatile char *ptr;
	ptr = mmap((void *) 0x5655555BC000, 4096, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE, -1, 0);
	ptr[1]=0;
	}
}

int main(int argc, char **argv) {
    int which = 0;
    if (argc > 1) {
        which = atoi(argv[1]);
    } else {
        fprintf(stderr, "Usage: %s NUMBER\n", argv[0]);
        return 1;
    }
    printf("Memory layout:\n");
    print_maps(stdout);
    printf("\n");
    printf("Initial state:\n");
    force_load();
    struct memory_record r1, r2;
    record_memory_record(&r1);
    print_memory_record(stdout, NULL, &r1);
    printf("---\n");

    printf("Running labStuff(%d)...\n", which);

    labStuff(which);

    printf("---\n");
    printf("Afterwards:\n");
    record_memory_record(&r2);
    print_memory_record(stdout, &r1, &r2);
    print_maps(stdout);
    return 0;
}

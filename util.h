#ifndef UTIL_H_
#define UTIL_H_

#include <stdio.h>      // for FILE

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

// record memory usage statistics
void record_memory_record(struct memory_record *r);

// print out recorded memory usage statistics in "to" to 'out'
// if from is not NULL, then also print the difference in each statistic between 'from' and 'to'
void print_memory_record(FILE *out, struct memory_record *from, struct memory_record *to);

// Make sure that parts of this program related to recording memory statistics are loaded
// into memory, to avoid "noise" in memory statistics from them being loaded for the first
// time.
//
// (Presently, this works by recording memory statistics and printing them out, but discarding
// that output rather than showing it.)
void force_load();

// output memory layout of the program
void print_maps(FILE *out);

#endif

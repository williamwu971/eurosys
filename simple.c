#include "ralloc.hpp"
#include <x86intrin.h>
#include <stdio.h>

inline
uint64_t readTSC(int front, int back) {
    if (front)_mm_lfence();  // optionally wait for earlier insns to retire before reading the clock
    uint64_t tsc = __rdtsc();
    if (back)_mm_lfence();  // optionally block later instructions until rdtsc retires
    return tsc;
}

int main(int argc, char **argv) {


    if (argc != 3) return 1;

    uint64_t n = atoi(argv[1]);
    uint64_t size = atoi(argv[2]);
    uint64_t max = 2 * 1024 * 1024 * 1024ULL;

    if (n * size * 4 > max) {

        printf("too many/big\n");
        return 1;

    }

    uint64_t *tscs = (uint64_t *) malloc(sizeof(uint64_t) * n);
    RP_init("simple", max);


    for (uint64_t i = 0; i < n; i++) {


        uint64_t pt0 = readTSC(1, 1);

        char *a = (char *) RP_malloc(size);
        a[0] = 'a';


        uint64_t pt1 = readTSC(1, 1);
        tscs[i] = pt1 - pt0;

    }


    FILE *f = fopen("simple_tscs.txt", "w");

    for (uint64_t i = 0; i < n; i++) {

        fprintf(f, "%lu\n", tscs[i]);


    }

    return 0;


}

#include <x86intrin.h>
//#include "ralloc.hpp"

#include <stdio.h>
#include <sys/mman.h>
#include <stdlib.h>
#include <stdint.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
//#include <sys/select.h>

//#include <iostream>
#include <assert.h>

#define MAP_HUGE_2MB    (21 << MAP_HUGE_SHIFT)
#define MAP_HUGE_1GB    (30 << MAP_HUGE_SHIFT)

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
    uint64_t FILESIZE = 2 * 1024 * 1024 * 1024ULL;


    int fd = open("/pmem0/kernel", O_RDWR | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);

    off_t offt = lseek(fd, (__off_t) FILESIZE - 1, SEEK_SET);
    assert(offt != -1);

    ssize_t result = write(fd, "", 1);
    assert(result != -1);

    char *addr = (char *) mmap(0, FILESIZE, PROT_READ | PROT_WRITE, 0x80003, fd, 0);
    //char *addr = (char *) mmap(0, FILESIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    assert(addr != MAP_FAILED);

//    madvise(addr, FILESIZE, MADV_NOHUGEPAGE);


    if (n * size * 4 > FILESIZE) {

        printf("too many/big\n");
        return 1;

    }

    uint64_t *tscs = (uint64_t *) malloc(sizeof(uint64_t) * n);


    for (uint64_t i = 0; i < n; i++) {


        uint64_t pt0 = readTSC(1, 1);

        char *a = addr + i * size;
        a[0] = 'a';


        uint64_t pt1 = readTSC(1, 1);
        tscs[i] = pt1 - pt0;

    }


    FILE *f = fopen("kernel_tscs.txt", "w");

    for (uint64_t i = 0; i < n; i++) {

        fprintf(f, "%lu\n", tscs[i]);


    }

    char buf[128];
    fgets(buf,128,stdin);

    return 0;


}

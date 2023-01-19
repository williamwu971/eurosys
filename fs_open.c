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
#include <string.h>
#include <errno.h>
//#include <sys/select.h>

//#include <iostream>
#include <assert.h>

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

    uint64_t *tscs = (uint64_t *) malloc(sizeof(uint64_t) * n);
//    char *addr = NULL;


    for (uint64_t i = 0; i < n; i++) {

        char fd_buf[128];
        sprintf(fd_buf, "/pmem0/fs_open_%lu", i);

        uint64_t pt0 = readTSC(1, 1);

        int fd = open(fd_buf, O_RDWR | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);
        off_t offt = lseek(fd, (__off_t) size - 1, SEEK_SET);
        assert(offt != -1);

        ssize_t result = write(fd, "", 1);
        assert(result != -1);

        uint64_t pt1 = readTSC(1, 1);

        char *addr = (char *) mmap(0, size, PROT_READ | PROT_WRITE, 0x80003, fd, 0);
        assert(addr != MAP_FAILED);

        close(fd);

//        madvise(addr, size, MADV_NOHUGEPAGE);

//        addr[i * size] = 'a';
        memset(addr, 'a', size);
//        addr += size;
//        munmap(addr, size);


        tscs[i] = pt1 - pt0;

    }


    FILE *f = fopen("fs_open_tscs.txt", "w");

    fprintf(f, "#fs_open\n");
    for (uint64_t i = 0; i < n; i++) {
        fprintf(f, "%lu\n", tscs[i]);
    }

    fclose(f);

    return 0;


}

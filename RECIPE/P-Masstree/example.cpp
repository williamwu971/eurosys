#include <iostream>
#include <chrono>
#include <random>
//#include "tbb/tbb.h"
#include "ralloc.hpp"
#include <omp.h>

#include <x86intrin.h>
#include <csignal>
#include <libpmem.h>

inline
uint64_t readTSC(int front, int back) {
    if (front)_mm_lfence();  // optionally wait for earlier insns to retire before reading the clock
    uint64_t tsc = __rdtsc();
    if (back)_mm_lfence();  // optionally block later instructions until rdtsc retires
    return tsc;
}

static constexpr uint64_t CACHE_LINE_SIZE = 64;

static inline void fence() {
    asm volatile("" : : : "memory");
}

static inline void mfence() {
    asm volatile("sfence":: :"memory");
}

static inline void clflush(char *data, int len, bool front, bool back) {
    volatile char *ptr = (char *) ((unsigned long) data & ~(CACHE_LINE_SIZE - 1));
    if (front)
        mfence();
    for (; ptr < data + len; ptr += CACHE_LINE_SIZE) {
#ifdef CLFLUSH
        asm volatile("clflush %0" : "+m" (*(volatile char *)ptr));
#elif CLFLUSH_OPT
        asm volatile(".byte 0x66; clflush %0" : "+m" (*(volatile char *)(ptr)));
#elif CLWB
        asm volatile(".byte 0x66; xsaveopt %0" : "+m" (*(volatile char *)(ptr)));
#endif
    }
    if (back)
        mfence();
}

using namespace std;

#include "masstree.h"

int bench_start() {

    int res = 1;

    char command[4096];
    sprintf(command, "sudo /home/blepers/linux-huge/tools/perf/perf stat "
                     "-e cycle_activity.stalls_l1d_miss "
                     "-e cycle_activity.stalls_l2_miss "
                     "-e cycle_activity.stalls_l3_miss "
                     "-e cycle_activity.stalls_mem_any "
                     "-e cycle_activity.stalls_total "
                     "-e resource_stalls.sb "
                     "-p %d -o bench.stat --append -g >> perf_stat.out 2>&1 &",
            getpid());
    res &= system(command);
    res &= system("/mnt/sdb/xiaoxiang/pcm/build/bin/pcm-memory -all >pcm-memory.log 2>&1 &");
    sleep(1);

    return res;
}

int bench_end() {

    int res = 1;
    res &= system("sudo killall -s INT perf");
    res &= system("sudo pkill --signal SIGHUP -f pcm-memory");

    sleep(1);

    return res;
}

void run(char **argv) {
    std::cout << "Simple Example of P-Masstree" << std::endl;

    uint64_t n = std::atoll(argv[1]);
    uint64_t *keys = new uint64_t[n];
//    uint64_t *tscs = new uint64_t[n];

    // Generate keys
    for (uint64_t i = 0; i < n; i++) {
        keys[i] = i + 1;
    }

    int num_thread = atoi(argv[2]);
    omp_set_num_threads(num_thread);
//    tbb::task_scheduler_init init(num_thread);

    printf("operation,n,ops/s\n");
    int no_flush = strcmp(getenv("masstree_no_flush"), "1") == 0;

    int size = atoi(getenv("masstree_size"));
    if (size < 8) size = 8;
    size = size / 8 * 8;

    int pmem = strcmp(getenv("masstree_pmem"), "1") == 0;
    if (pmem) {
        RP_init("kv", 160 * 1024 * 1024 * 1024ULL);
    }

    printf("n:<%lu> num_thread:<%d> no_flush:<%d> pmem:<%d>\n", n, num_thread, no_flush, pmem);

    masstree::masstree *tree = new masstree::masstree();

    {
        // Build tree
        auto starttime = std::chrono::system_clock::now();
//        tbb::parallel_for(tbb::blocked_range<uint64_t>(0, n), [&](const tbb::blocked_range<uint64_t> &range) {

#pragma omp parallel
        {


            auto t = tree->getThreadInfo();
            for (uint64_t i = omp_get_thread_num(); i < n; i += num_thread) {

                uint64_t *value = nullptr;

                if (pmem) {
                    value = static_cast<uint64_t *>(RP_malloc(size));
                } else {
                    value = static_cast<uint64_t *> (malloc(size));
                }

                for (uint64_t idx = 0; idx < size / sizeof(uint64_t); idx++) {
                    value[idx] = keys[i];
                }
                if (!no_flush) {
                    clflush(reinterpret_cast<char *>(value), size, false, true);
                }

//                uint64_t pt0 = readTSC(1, 1);
//                tree->put(keys[i], &keys[i], t);
                tree->put(keys[i], value, t);
//                uint64_t pt1 = readTSC(1, 1);


//                tscs[i] = pt1 - pt0;
            }
        }

//        });
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(
                std::chrono::system_clock::now() - starttime);
        printf("Throughput: insert,%ld,%f ops/us\n", n, (n * 1.0) / duration.count());
        printf("Elapsed time: insert,%ld,%f sec\n", n, duration.count() / 1000000.0);

//        FILE *f = fopen("perf.csv", "a");
//        fprintf(f, "%d,%d,%d,%lu,%f,%f\n", flush, size, pmem,
//                n, (n * 1.0) / duration.count(), duration.count() / 1000000.0);
    }

    {
        bench_start();
        // update
        auto starttime = std::chrono::system_clock::now();
#pragma omp parallel
        {
            auto t = tree->getThreadInfo();
            auto buffer = static_cast<uint64_t *> (malloc(size));
            memset(buffer, 12, size);


            for (uint64_t i = omp_get_thread_num(); i < n; i += num_thread) {

                uint64_t *value = nullptr;

                if (pmem) {
                    value = static_cast<uint64_t *>(RP_malloc(size));
                } else {
                    value = static_cast<uint64_t *> (malloc(size));
                }

//                for (uint64_t idx = 0; idx < size / sizeof(uint64_t); idx++) {
//                    value[idx] = keys[i];
//                }


                buffer[0] = keys[i];
//                pmem_memcpy_persist(value, buffer, size);
                memcpy(value, buffer, size);


                if (!no_flush) {
                    clflush(reinterpret_cast<char *>(value), size, false, true);
                    //                pmem_persist(value, size);
                }


//                uint64_t pt0 = readTSC(1, 1);
//                tree->put(keys[i], &keys[i], t);
                void *val = tree->put_and_return(keys[i], value, t);
//                uint64_t pt1 = readTSC(1, 1);


//                tscs[i] = pt1 - pt0;

                if (pmem) {
                    RP_free(val);
                } else {
                    free(val);
                }
            }
        }
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(
                std::chrono::system_clock::now() - starttime);

        bench_end();

        printf("Throughput: update,%ld,%f ops/us\n", n, (n * 1.0) / duration.count());
        printf("Elapsed time: update,%ld,%f sec\n", n, duration.count() / 1000000.0);

        FILE *f = fopen("perf.csv", "a");
        fprintf(f, "update,%d,%d,%d,%lu,%f,%f\n", no_flush, size, pmem,
                n, (n * 1.0) / duration.count(), duration.count() / 1000000.0);
    }


    {
        // Lookup
        auto starttime = std::chrono::system_clock::now();
#pragma omp parallel
        {
            auto t = tree->getThreadInfo();
            for (uint64_t i = omp_get_thread_num(); i < n; i += num_thread) {
                uint64_t *ret = reinterpret_cast<uint64_t *> (tree->get(keys[i], t));
                if (*ret != keys[i]) {
                    std::cout << "wrong value read: " << *ret << " expected:" << keys[i] << std::endl;
                    throw;
                }
            }
        }
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(
                std::chrono::system_clock::now() - starttime);
        printf("Throughput: lookup,%ld,%f ops/us\n", n, (n * 1.0) / duration.count());
        printf("Elapsed time: lookup,%ld,%f sec\n", n, duration.count() / 1000000.0);
    }

    delete[] keys;


//    FILE *f = fopen("kv_tscs.txt", "w");
//
//    for (uint64_t i = 0; i < n; i++) {
//
//        fprintf(f, "%lu\n", tscs[i]);
//
//
//    }

//    delete[] tscs;

}

int main(int argc, char **argv) {
    if (argc != 3) {
        printf("usage: %s [n] [nthreads]\nn: number of keys (integer)\nnthreads: number of threads (integer)\n",
               argv[0]);
        return 1;
    }

    run(argv);
    return 0;
}

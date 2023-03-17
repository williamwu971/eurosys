#include <iostream>
#include <chrono>
#include <random>
//#include "tbb/tbb.h"
//#include "ralloc.hpp"
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
//    sprintf(command, "sudo /home/blepers/linux-huge/tools/perf/perf stat "
//                     "-e cycle_activity.stalls_l1d_miss "
//                     "-e cycle_activity.stalls_l2_miss "
//                     "-e cycle_activity.stalls_l3_miss "
//                     "-e cycle_activity.stalls_mem_any "
//                     "-e cycle_activity.stalls_total "
//                     "-e resource_stalls.sb "
//                     "-p %d -o bench.stat --append -g >> perf_stat.out 2>&1 &",
//            getpid());
//    res &= system(command);

    sprintf(command, "sudo /home/blepers/linux-huge/tools/perf/perf record "
                     "-e mem-loads:ppu -e mem-stores:ppu -d " // user space symbols only
                     "-p %d -o bench.record &", // don't use call-graph
            getpid());
    res &= system(command);

//    sprintf(command, "sudo /home/blepers/linux-huge/tools/perf/perf mem record "
//                     "-p -o bench.mem -D >> perf_mem.out 2>&1 &"
//    );
//    res &= system(command);
//
//    sprintf(command, "sudo /home/blepers/linux-huge/tools/perf/perf c2c record "
//                     "-o bench.c2c >> perf_c2c.out 2>&1 &"
//    );
//    res &= system(command);

//    res &= system("/mnt/sdb/xiaoxiang/pcm/build/bin/pcm-memory -all >pcm-memory.log 2>&1 &");
    sleep(1);

    return res;
}

int bench_end() {

    int res = 1;
    res &= system("sudo killall -s INT perf");
//    res &= system("sudo pkill --signal SIGHUP -f pcm-memory");

    sleep(1);

    return res;
}

void parse_bandwidth(double elapsed);

uint64_t pmem_read;
uint64_t pmem_write;
uint64_t dram_read;
uint64_t dram_write;
double pmem_read_gb;
double pmem_write_gb;
double pmem_read_bw;
double pmem_write_bw;
double dram_read_gb;
double dram_write_gb;
double dram_read_bw;
double dram_write_bw;

static inline void *custom_memcpy(char *dest, const char *src, size_t n, int no_flush) {

    memcpy(dest, src, n);
    if (!no_flush) {
//            clflush(reinterpret_cast<char *>(dest + idx), 64, false, true);
        pmem_persist(dest, n);
    }

    return NULL;

    for (size_t idx = 0; idx < n; idx += 64) {
        memcpy(dest + idx, src + idx, 64);
        if (!no_flush) {
//            clflush(reinterpret_cast<char *>(dest + idx), 64, false, true);
            pmem_persist(dest + idx, 64);
        }
    }
    return NULL;
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
        RP_init("kv", 100 * 1024 * 1024 * 1024ULL);
    }

    printf("n:<%lu> num_thread:<%d> no_flush:<%d> size:<%d> pmem:<%d>\n", n, num_thread, no_flush, size, pmem);

    masstree::masstree *tree = new masstree::masstree();

    {
        printf("Insert\n");
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
        pthread_mutex_t stats_lock;
        pthread_mutex_init(&stats_lock, nullptr);
        void *global_lowest = nullptr;
        void *global_highest = nullptr;

        printf("Update\n");
        bench_start();
        // update
        auto starttime = std::chrono::system_clock::now();
#pragma omp parallel
        {
            auto t = tree->getThreadInfo();
            auto buffer = static_cast<uint64_t *> (malloc(size));
            memset(buffer, 12, size);

            void *lowest = nullptr;
            void *highest = nullptr;

            for (uint64_t i = omp_get_thread_num(); i < n; i += num_thread) {

                uint64_t *value = nullptr;

                if (pmem) {
                    value = static_cast<uint64_t *>(RP_malloc(size));
                } else {
                    value = static_cast<uint64_t *> (malloc(size));
                }

                if (lowest != nullptr) {

                    if (value < lowest) lowest = value;
                    if (value > highest) highest = value;

                } else {
                    lowest = value;
                    highest = value;
                }

//                for (uint64_t idx = 0; idx < size / sizeof(uint64_t); idx++) {
//                    value[idx] = keys[i];
//                }


                buffer[0] = keys[i];
//                pmem_memcpy_persist(value, buffer, size);

                custom_memcpy((char *) value, (char *) buffer, size, no_flush);

//                uint64_t pt0 = readTSC(1, 1);
//                tree->put(keys[i], &keys[i], t);
                void *val = tree->put_and_return(keys[i], value, t);
//                uint64_t pt1 = readTSC(1, 1);


//                tscs[i] = pt1 - pt0;

//                if (pmem) {
//                    RP_free(val);
//                } else {
//                    free(val);
//                }
            }


            pthread_mutex_lock(&stats_lock);
            if (global_lowest == nullptr) {
                global_lowest = lowest;
                global_highest = highest;
            } else {
                if (lowest < global_lowest) global_lowest = lowest;
                if (highest > global_highest) global_highest = highest;
            }
            pthread_mutex_unlock(&stats_lock);
        }
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(
                std::chrono::system_clock::now() - starttime);

        bench_end();
//        parse_bandwidth(duration.count() / 1000000.0);

        printf("Throughput: update,%ld,%f ops/us\n", n, (n * 1.0) / duration.count());
        printf("Elapsed time: update,%ld,%f sec\n", n, duration.count() / 1000000.0);
        printf("lowest/highest: %lu %lu\n",(uint64_t) global_lowest, (uint64_t)global_highest);

//        FILE *f = fopen("perf.csv", "a");
//        fprintf(f, "update,%d,%d,%d,%lu,%f,%f,", no_flush, size, pmem,
//                n, (n * 1.0) / duration.count(), duration.count() / 1000000.0);
//
//        fprintf(f, "%.2f,%.2f,%.2f,%.2f,", pmem_read_gb, pmem_read_bw, pmem_write_gb, pmem_write_bw);
//        fprintf(f, "%.2f,%.2f,%.2f,%.2f,", dram_read_gb, dram_read_bw, dram_write_gb, dram_write_bw);
//        fprintf(f, "\n");
//        fclose(f);

    }


    {
        printf("Lookup\n");
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

void parse_bandwidth(double elapsed) {


    int scanned_channel;

    while (true) {

        scanned_channel = 0;

        FILE *file = fopen("/mnt/sdb/xiaoxiang/pcm.txt", "r");
        pmem_read = 0;
        pmem_write = 0;
        dram_read = 0;
        dram_write = 0;

        char buffer[256];
        int is_first_line = 1;
        while (fgets(buffer, 256, file) != nullptr) {

            if (is_first_line) {
                is_first_line = 0;
                continue;
            }

            uint64_t skt, channel, pmmReads, pmmWrites, elapsedTime, dramReads, dramWrites;
            sscanf(buffer, "%lu %lu %lu %lu %lu %lu %lu",
                   &skt, &channel, &pmmReads, &pmmWrites, &elapsedTime, &dramReads, &dramWrites
            );

            scanned_channel++;
            pmem_read += pmmReads;
            pmem_write += pmmWrites;
            dram_read += dramReads;
            dram_write += dramWrites;
        }

        if (scanned_channel >= 16) {
            break;
        } else {
            puts("pcm.txt parse failed");
        }
    }

    pmem_read_gb = (double) pmem_read / 1024.0f / 1024.0f / 1024.0f;
    pmem_write_gb = (double) pmem_write / 1024.0f / 1024.0f / 1024.0f;

    pmem_read_bw = pmem_read_gb / elapsed;
    pmem_write_bw = pmem_write_gb / elapsed;

    dram_read_gb = (double) dram_read / 1024.0f / 1024.0f / 1024.0f;
    dram_write_gb = (double) dram_write / 1024.0f / 1024.0f / 1024.0f;

    dram_read_bw = dram_read_gb / elapsed;
    dram_write_bw = dram_write_gb / elapsed;


    printf("PR: %7.2fgb %7.2fgb/s ", pmem_read_gb, pmem_read_bw);
    printf("PW: %7.2fgb %7.2fgb/s ", pmem_write_gb, pmem_write_bw);

    printf("\n");

//    printf("elapsed: %.2f ", elapsed);

    printf("DR: %7.2fgb %7.2fgb/s ", dram_read_gb, dram_read_bw);
    printf("DW: %7.2fgb %7.2fgb/s ", dram_write_gb, dram_write_bw);

    printf("\n");

}

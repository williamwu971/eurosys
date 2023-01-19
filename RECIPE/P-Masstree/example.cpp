#include <iostream>
#include <chrono>
#include <random>
#include "tbb/tbb.h"
#include "ralloc.hpp"

#include <x86intrin.h>

inline
uint64_t readTSC(int front, int back) {
    if (front)_mm_lfence();  // optionally wait for earlier insns to retire before reading the clock
    uint64_t tsc = __rdtsc();
    if (back)_mm_lfence();  // optionally block later instructions until rdtsc retires
    return tsc;
}

using namespace std;

#include "masstree.h"

void run(char **argv) {
    std::cout << "Simple Example of P-Masstree" << std::endl;

    uint64_t n = std::atoll(argv[1]);
    uint64_t *keys = new uint64_t[n];
    uint64_t *tscs = new uint64_t[n];

    RP_init("kv",2*1024*1024*1024ULL);

    // Generate keys
    for (uint64_t i = 0; i < n; i++) {
        keys[i] = i + 1;
    }

    int num_thread = atoi(argv[2]);
    tbb::task_scheduler_init init(num_thread);

    printf("operation,n,ops/s\n");
    masstree::masstree *tree = new masstree::masstree();

    {
        // Build tree
        auto starttime = std::chrono::system_clock::now();
        tbb::parallel_for(tbb::blocked_range<uint64_t>(0, n), [&](const tbb::blocked_range<uint64_t> &range) {
            auto t = tree->getThreadInfo();
            for (uint64_t i = range.begin(); i != range.end(); i++) {

		uint64_t pt0 = readTSC(1,1);
                tree->put(keys[i], &keys[i], t);
		uint64_t pt1 = readTSC(1,1);


		tscs[i] = pt1-pt0;
            }
        });
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(
                std::chrono::system_clock::now() - starttime);
        printf("Throughput: insert,%ld,%f ops/us\n", n, (n * 1.0) / duration.count());
        printf("Elapsed time: insert,%ld,%f sec\n", n, duration.count() / 1000000.0);
    }

    {
        // Lookup
        auto starttime = std::chrono::system_clock::now();
        tbb::parallel_for(tbb::blocked_range<uint64_t>(0, n), [&](const tbb::blocked_range<uint64_t> &range) {
            auto t = tree->getThreadInfo();
            for (uint64_t i = range.begin(); i != range.end(); i++) {
                uint64_t *ret = reinterpret_cast<uint64_t *> (tree->get(keys[i], t));
                if (*ret != keys[i]) {
                    std::cout << "wrong value read: " << *ret << " expected:" << keys[i] << std::endl;
                    throw;
                }
            }
        });
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(
                std::chrono::system_clock::now() - starttime);
        printf("Throughput: lookup,%ld,%f ops/us\n", n, (n * 1.0) / duration.count());
        printf("Elapsed time: lookup,%ld,%f sec\n", n, duration.count() / 1000000.0);
    }

    delete[] keys;


    
        FILE* f = fopen("kv_tscs.txt","w");

        for (uint64_t i = 0;i<n;i++){

                fprintf(f,"%lu\n",tscs[i]);


        }

    delete[] tscs;

}

int main(int argc, char **argv) {
    if (argc != 3) {
        printf("usage: %s [n] [nthreads]\nn: number of keys (integer)\nnthreads: number of threads (integer)\n", argv[0]);
        return 1;
    }

    run(argv);
    return 0;
}

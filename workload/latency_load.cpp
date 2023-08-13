#include <iostream>
#include <unistd.h>
#include "thread.h"


volatile bool should_exit = false;

int main() {
    std::vector<std::thread> cfs_workers;
    
    should_exit = false;
    for (int i = 0; i < 10; i ++) {
        cfs_workers.push_back(std::thread([i]{
        // cpu_set_t cpuSet;
        // CPU_ZERO(&cpuSet);
        // CPU_SET(i % 12, &cpuSet);
        // sched_setaffinity(gettid(), sizeof(cpuSet), &cpuSet);

        while (!should_exit) {

        }
        // int cnt = 1;
        // for (unsigned long long j = 0; j < 18446744073709551615ULL; j ++) {
        //   cnt *= 2;
        // }
        }));
    }

    fprintf(stderr, "create cfs\n");
    
    auto t = new CosThread([] {
        sleep(1);
        struct timespec ts;
        clock_gettime(CLOCK_MONOTONIC, &ts);
        uint64_t tss = ts.tv_sec * 1000000000ULL + ts.tv_nsec;
        printf("%ld\n", tss);
    });
        
    fprintf(stderr, "join cos\n");
    t->join();

    fprintf(stderr, "exit cfs\n");
    should_exit = true;
    for (int i = 0; i < cfs_workers.size(); i ++) {
        cfs_workers[i].join();
    }
}

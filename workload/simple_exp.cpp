#include <stdio.h>
#include <iostream>
#include <sys/types.h>
#include <unistd.h>
#include <vector>

#include <gtest/gtest.h>

#include "thread.h"

TEST(SimpleTest, TestOne) {
  CosThread t([] {
    fprintf(stderr, "hello world!\n");
    sleep(1);
    fprintf(stderr, "fantastic nap!\n");
  });

  t.join();
}

TEST(SimpleTest, TestMany) {
    std::vector<std::unique_ptr<CosThread>> threads;

    for (int i = 0; i < 5; i++) {
        threads.emplace_back(
            new CosThread([] {
                sleep(1);
                fprintf(stderr, "thread[TID %d], scheduler: %d.\n", gettid(), sched_getscheduler(gettid()));
            }));
    }
    
    for(auto& t : threads)  t->join();
}

void spin_for(uint64_t duration) {
  while (duration > 0) {
    std::chrono::high_resolution_clock::time_point a =
        std::chrono::high_resolution_clock::now();
    std::chrono::high_resolution_clock::time_point b;

    // Try to minimize the contribution of arithmetic/Now() overhead.
    for (int i = 0; i < 150; i++) {
      b = std::chrono::high_resolution_clock::now();
    }

    uint64_t t =
        std::chrono::duration_cast<std::chrono::nanoseconds>(b - a).count();

    // Don't count preempted time
    if (t < 100000) {
      if (duration > t)
        duration -= t;
      else
        duration = 0;
    }
  }
}

TEST(SimpleTest, TestBusy) {
    std::vector<std::unique_ptr<CosThread>> threads;

    for (int i = 0; i < 5; i++) {
        threads.emplace_back(
        new CosThread([] {
          spin_for(100 * 1000);
        }));
  }

  for (auto& t : threads) t->join();
}

int main(int argc,char *argv[])
{
    testing::InitGoogleTest(&argc,argv);
    return RUN_ALL_TESTS();
}

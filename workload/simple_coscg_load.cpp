#include <cos_cgroup.h>
#include <thread.h>
#include <vector>

int main() {
    auto coscg = new CosCgroup;
    coscg->adjust_rate(5);

    std::vector<CosThread*> load_thread_vec(0);
    // for (int i = 0; i < 20; i++) {
        load_thread_vec.push_back(
            new CosThread([&] {
                int tid = gettid();
                coscg->add_thread(tid);
                for (int i = 0; i < 100000; i++) {
                    // struct timespec ts;
                    // ts.tv_sec = 0;
                    // ts.tv_nsec = 1000 *100;  // sleep for 100us
                    // nanosleep(&ts, NULL);
                    printf("thread %d %d\n", tid, sched_getscheduler(tid));
                }
            })
        );
    // }
    
    
    for(auto& t : load_thread_vec)  t->join();
}
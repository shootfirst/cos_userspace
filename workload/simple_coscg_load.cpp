#include <cos_cgroup.h>

int main() {
    CosCgroup coscg = new CosCgroup;
    coscg->adjust_rate(80);

    std::vector<CosThread*> load_thread_vec(0);
    for (int i = 0; i < 20; i++) {
        load_thread_vec.push_back(
            new CosThread([&] {
                coscg->add_thread(gettid());
                for (int i = 0; i < 1000; i++) {
                    struct timespec ts;
                    ts.tv_sec = 0;
                    ts.tv_nsec = 1000 *100;  // sleep for 100us
                    nanosleep(&ts, NULL);
                    printf("thread %d %d\n", tid, sched_getscheduler(tid));
                }
            })
        );
    }
    
    
    for(auto& t : load_thread_vec)  t->join();
}
#include <iostream>
#include <unistd.h>
#include "thread.h"

int main() {
    std::vector<CosThread*> load_thread_vec(0);

    for (int i = 0; i < 2; i++) {
        // auto t = CosThread([] {
        //     for (int i = 0; i < 10; i++) {
        //         sleep(1);
        //         printf("thread %d\n", gettid());
        //     }
        // });

        load_thread_vec.push_back(
            new CosThread([] {
                int tid = gettid();
                for (int i = 0; i < 2; i++) {
                    sleep(1);
                    printf("thread %d %d\n", tid, sched_getscheduler(tid));
                }
            })
        );
    }
    
    
    for(auto& t : load_thread_vec)  t->join();

    // printf("thread %d\n", gettid());
    // struct sched_param param = {.sched_priority = 0};
	// sched_setscheduler(gettid(), SCHED_COS, &param);
    // sleep(1);
    // printf("thread %d\n", gettid());

	
}

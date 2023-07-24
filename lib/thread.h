#include <functional>

class CosThread {
    CosThread(function<void()> work) {
        // thread_ = std::thread([this, w = std::move(work)] {
        //     tid_ = getpid();
        //     struct sched_param param = {.sched_priority = 0};
	    //     sched_setscheduler(tid, 8, &param);
        //     sleep(1);
        //     std::move(w)();
        // });
    }

    std::thread thread_;
    u_int32_t tid_;
};
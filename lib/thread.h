#include <functional>
#include <thread>
#include <unistd.h>

#define SCHED_COS 8

class CosThread {
public:
    CosThread(std::function<void()> work) {
        thread_ = std::thread([this, w = std::move(work)] {
            tid_ = gettid();
            struct sched_param param = {.sched_priority = 0};
	        sched_setscheduler(tid_, SCHED_COS, &param);
            // sleep(1);
            std::move(w)();
        });
    }

    CosThread(std::function<void(int)> work, int arg) {
        thread_ = std::thread([this, w = std::move(work), arg] {
            tid_ = gettid();
            struct sched_param param = {.sched_priority = 0};
	        sched_setscheduler(tid_, SCHED_COS, &param);
            // sleep(1);
            std::move(w)(arg);
        });
    }

    void join() {
        thread_.join();
    }

    void joinable() {
        thread_.joinable();
    }

    u_int32_t tid() {
        return tid_;
    }

    explicit CosThread(const CosThread&) = delete;
    CosThread& operator=(const CosThread&) = delete;
    ~CosThread() = default;

private:
    std::thread thread_;
    u_int32_t tid_;
};
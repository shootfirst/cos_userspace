#include <queue>
#include "lord.h"

struct FifoTask {
public:
    FifoTask (int pid) : pid_(pid) {}

private:
    int pid_;
};

class FifoRq {
public:
    FifoRq() : rq_() {}

    void enqueue(FifoTask task) {

    }

    void dequeue() {

    }

    FifoTask peek() {

    }

    void remove_from_rq(u_int32_t tid) {

    }

private:
    std::queue<FifoTask> rq_;
};

class FifoLord : Lord {

public:
    FifoLord(int lord_cpu) : Lord(lord_cpu), fifo_rq_() {}

    virtual void schedule() {
        FifoTask* next = fifo_rq_.peek();

    }

private:

    virtual void consume_msg_task_runnable(cos_msg msg) {
        u_int32_t tid = msg.pid;
        if (!alive_tasks_.count(tid)) {
            return ; // TODO
        }

        auto new_task = new FifoTask(tid);
        fifo_rq_.enqueue(new_task);
    }

    virtual void consume_msg_task_blocked(cos_msg msg) {
        u_int32_t tid = msg.pid;
        if (!alive_tasks_.count(tid)) {
            LOG(ERROR) << "task blocked before task new, kernel BUGGGGGG!";
            exit(1);
        }
        
        // TODO
        if(task is queued) {
            fifo_rq_.remove_from_rq(tid);
        } else if (task is oncpu) {
            update cpu state
        }
    }

    virtual void consume_msg_task_new(cos_msg msg) {
        u_int32_t tid = msg.pid;
        if (alive_tasks_.count(tid)) {
            LOG(ERROR) << "same new_thread message, kernel BUGGGGGG!";
            exit(1);
        }

        auto new_task = new FifoTask(tid);
        alive_tasks_[tid] = new_task;

        if (msg.runnable) {
            fifo_rq_.enqueue(new_task);
        }
    }

    virtual void consume_msg_task_dead(cos_msg msg) {
        u_int32_t tid = msg.pid;
        if (!alive_tasks_.count(tid)) {
            LOG(ERROR) << "task dead before task new, kernel BUGGGGGG!";
            exit(1);
        }
        
        // TODO
        if(task is queued) {
            fifo_rq_.remove_from_rq(tid);
        } else if (task is oncpu) {
            update cpu state
        }
        alive_tasks_.erase(tid);
    }

    virtual void consume_msg_task_preempt(cos_msg msg) {
        u_int32_t tid = msg.pid;
        if (!alive_tasks_.count(tid)) {
            LOG(ERROR) << "task dead before task new, kernel BUGGGGGG!";
            exit(1);
        }
        
        // TODO
        if(task is queued) {
            fifo_rq_.remove_from_rq(tid);
        } else if (task is oncpu) {
            update cpu state
        }
    }

    FifoRq fifo_rq_;
    std::unordered_map<u_int32_t, FifoTask*> alive_tasks_;
};
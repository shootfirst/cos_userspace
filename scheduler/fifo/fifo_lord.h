#include <queue>
#include "lord.h"

struct FifoTask {
    FifoTask (int pid) : pid(pid) {}
    int pid;
};

class FifoRq {
public:
    FifoRq() : rq_() {}

    void enqueue(u_int32_t tid) {
        auto t = new FifoTask(tid);
        rq_.push(t);
    }

    void dequeue() {
        if (rq_.empty())    return ;
        // remember to delete
        auto t = rq_.front();
        rq_.pop();
        delete t;
    }

    FifoTask* peek() {
        if (rq_.empty())    return nullptr;
        return rq_.front();
    }

    void remove_from_rq(u_int32_t tid) {
        // remember to delete
        for (auto it = rq_.begin(); it != rq_.end();) {
            if ((*it) && (*it)->pid == tid) {
                delete *it;
                it = rq_->second.erase(it);
                return;
            } else {
                it++;
            }
        }
    }

private:
    std::queue<FifoTask*> rq_;
};

class FifoLord : Lord {

public:
    FifoLord(int lord_cpu) : Lord(lord_cpu), fifo_rq_() {}

    virtual void schedule() {
        FifoTask* next = fifo_rq_.peek();
        
        // do shoot
        int cpu = 7;
        cpu_set_t mask;  
        CPU_ZERO(&mask);    
        CPU_SET(cpu, &mask);  
        sa->area[cpu].pid = next->pid;
        shoot_task(sizeof(mask), &mask);

        fifo_rq_.dequeue();
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
            LOG(ERROR) << "task preempt before task new, kernel BUGGGGGG!";
            exit(1);
        }
        
        // TODO
        if(task is queued) {
            // We could be kQueued from a TASK_NEW that was immediately preempted.
            // For fifo, we do nothing.
        } else if (task is oncpu) {
            update cpu state
            fifo_rq_.enqueue(tid);
        }
    }

    FifoRq fifo_rq_;
    std::unordered_map<u_int32_t, FifoTask*> alive_tasks_;
};
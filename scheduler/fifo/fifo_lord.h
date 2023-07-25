#include <queue>
#include "lord.h"

enum class FifoRunState {
    Queued,
    OnCpu,
    Other
};

// struct CpuState {
//     u_int32_t pid;
//     bool availible;
// };

struct FifoTask {
    FifoTask (int pid) : pid(pid), state(FifoRunState::Other) {}
    FifoRunState state;
    int pid;
};

class FifoRq {
public:
    FifoRq() {}

    void enqueue(u_int32_t tid) {
        rq_.push(tid);
    }

    void dequeue() {
        if (rq_.empty())    return ;
        rq_.pop();
    }

    u_int32_t peek() {
        if (rq_.empty())    return 0;
        return rq_.front();
    }

    void remove_from_rq(u_int32_t tid) {
        for (auto it = rq_.begin(); it != rq_.end();) {
            if ((*it) == tid) {
                it = rq_.erase(it);
                return;
            } else {
                it++;
            }
        }

        LOG(ERROR) << "should not remove an unqueued task from the runqueue!";
        exit(1);
    }

private:
    std::queue<u_int32_t> rq_;
};

class FifoLord : Lord {

public:
    FifoLord(int lord_cpu) : Lord(lord_cpu), fifo_rq_() {
        // cpu_states_ = (CpuState*)malloc(sizeof(CpuState) * cpu_num_);
        // for (int i = 0; i < cpu_num_; i ++) {
        //     cpu_states_[i].pid = 0;
        //     cpu_states_[i].availible = true;
        // }
    }

    virtual void schedule() {
        for (int cpu = 0; cpu < cpu_num_; cpu ++) {
            // no preempt
            int tid = fifo_rq_.peek();
            if (tid == 0)   return ; // there is no task in the runqueue

            FifoTask* next = *(alive_tasks_.find(tid));
            if (!next) {
                LOG(ERROR) << "task is picked before task new or after task dead, kernel BUGGGGGG!";
                exit(1);
            }
            
            // do shoot
            cpu_set_t mask;
            CPU_ZERO(&mask);
            CPU_SET(cpu, &mask);
            sa->area[cpu].pid = next->pid;
            shoot_task(sizeof(mask), &mask);

            next->state = FifoRunState::OnCpu;
            // cpu_states_[cpu].pid = next->pid;
            // cpu_states_[cpu].availible = false;

            fifo_rq_.dequeue();
        }
    }

private:

    virtual void consume_msg_task_runnable(cos_msg msg) {
        u_int32_t tid = msg.pid;
        auto it = alive_tasks_.find(tid);
        if (!it) {
            return ; // TODO
        }

        auto task = *it;
        if (!task) {
            LOG(ERROR) << "task is null in runnable!";
            exit(1);
        }

        if (task->state == FifoRunState::Queued) {
            return ; // TODO
        }
        fifo_rq_.enqueue(tid);
        task->state = FifoRunState::Queued;
    }

    virtual void consume_msg_task_blocked(cos_msg msg) {
        u_int32_t tid = msg.pid;
        auto it = alive_tasks_.find(tid);
        if (!it) {
            LOG(ERROR) << "task blocked before task new, kernel BUGGGGGG!";
            exit(1);
        }
        
        auto task = *it;
        if (!task) {
            LOG(ERROR) << "task is null in blocked!";
            exit(1);
        }

        // TODO
        if(task->state == FifoRunState::Queued) {
            fifo_rq_.remove_from_rq(tid);
        } else if (task->state == FifoRunState::OnCpu) {
            // update cpu state
        }
        task->state = FifoRunState::Other;
    }

    virtual void consume_msg_task_new(cos_msg msg) {
        u_int32_t tid = msg.pid;
        if (alive_tasks_.count(tid)) {
            LOG(ERROR) << "same new_thread message, kernel BUGGGGGG!";
            exit(1);
        }

        auto new_task = new FifoTask(tid);
        alive_tasks_[tid] = new_task;

        fifo_rq_.enqueue(tid);
        new_task->state = FifoRunState::Queued;
    }

    virtual void consume_msg_task_new_blocked(cos_msg msg) {
        u_int32_t tid = msg.pid;
        if (alive_tasks_.count(tid)) {
            LOG(ERROR) << "same new_thread message, kernel BUGGGGGG!";
            exit(1);
        }

        auto new_task = new FifoTask(tid);
        alive_tasks_[tid] = new_task;
    }

    virtual void consume_msg_task_dead(cos_msg msg) {
        u_int32_t tid = msg.pid;
        auto it = alive_tasks_.find(tid);
        if (!it) {
            LOG(ERROR) << "task dead before task new, kernel BUGGGGGG!";
            exit(1);
        }

        auto task = *it;
        if (!task) {
            LOG(ERROR) << "task is null in dead!";
            exit(1);
        }
        
        // TODO
        if(task->state == FifoRunState::Queued) {
            fifo_rq_.remove_from_rq(tid);
        } else if(task->state == FifoRunState::OnCpu) {
            // update cpu state
        }
        
        alive_tasks_.erse(it);
        // remember to delete
        delete task;
    }

    virtual void consume_msg_task_preempt(cos_msg msg) {
        u_int32_t tid = msg.pid;
        auto it = alive_tasks_.find(tid);
        if (!it) {
            LOG(ERROR) << "task preempt before task new, kernel BUGGGGGG!";
            exit(1);
        }

        auto task = *it;
        if (!task) {
            LOG(ERROR) << "task is null in dead!";
            exit(1);
        }
        
        if(task->state == FifoRunState::Queued) {
            // We could be Queued from a TASK_NEW that was immediately preempted.
            // For fifo, we do nothing.
        } else if(task->state == FifoRunState::OnCpu) {
            fifo_rq_.enqueue(tid);
            new_task->state = FifoRunState::Queued;
        }

        // if (msg.preempt_by_cfs) {
        //     cpu_states_[msg.cpu_id].availible = false;
        // }
        // } else {
        //     cpu_states_[msg.cpu_id].availible = false;
        // }
    }

    FifoRq fifo_rq_;
    std::unordered_map<u_int32_t, FifoTask*> alive_tasks_;
    // CpuState* cpu_states_;
};
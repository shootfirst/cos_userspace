#include <list>
#include <unordered_map>
#include <deque>
#include <cassert>
#include "lord.h"


static const int cpu_num = sysconf(_SC_NPROCESSORS_CONF);

enum class FifoRunState {
    Queued,
    OnCpu,
    Blocked
};

enum class CpuState {
    IDLE,
    COS,
    NOCOS,
};

struct FifoTask {
    FifoTask (int pid) : pid(pid), state(FifoRunState::Blocked) {}
    FifoRunState state;
    int pid;
    int cpu_id = -1;
};

class FifoRq {
public:
    FifoRq() {}

    void enqueue(u_int32_t tid) {
        rq_.push_back(tid);
    }

    void dequeue() {
        if (rq_.empty())    return ;
        rq_.pop_front();
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
    std::list<u_int32_t> rq_;
};

class FifoLord : public Lord {

public:
    FifoLord(int lord_cpu) : Lord(lord_cpu), fifo_rq_() {
        cpu_states_.reserve(cpu_num);
        for (int i = 0; i < cpu_num; i ++) {
            cpu_states_[i] = CpuState::IDLE;
        }
    }

    virtual void schedule() {
        std::vector<std::pair<int, CpuState>> old_cpu_states;
        std::vector<std::pair<int, cos_shoot_arg>> assigned;
        cpu_set_t assigned_mask;
	    CPU_ZERO(&assigned_mask);
        for (int cpu = 0; cpu < cpu_num; cpu ++) {
            if (cpu == lord_cpu_)    continue ;
            if (cpu_states_[cpu] == CpuState::COS) {
                continue ;
            } 
            // no preempt
            int tid = fifo_rq_.peek();
            if (tid == 0)   break ;

            if (!alive_tasks_.count(tid)) {
                LOG(ERROR) << "task is picked before task new or after task dead, kernel BUGGGGGG!";
                exit(1);
            }
            FifoTask* next = alive_tasks_[tid];

            // do shoot
            cos_shoot_arg arg{next->pid, 0};
            assigned.push_back(std::make_pair(cpu, arg));
            LOG(WARNING) << "shoot " << arg.pid << "  at " << cpu;
            CPU_SET(cpu, &assigned_mask);  

            next->state = FifoRunState::OnCpu;
            next->cpu_id = cpu;
            cpu_states_[cpu] = CpuState::COS;
            old_cpu_states.push_back(std::make_pair(cpu, cpu_states_[cpu]));

            fifo_rq_.dequeue();
        }

        if (assigned.empty()) {
            return;
        }
        
        sa_->commit_shoot_message(assigned, seq_);
        int shoot_err = shoot_task(sizeof(assigned_mask), &assigned_mask);

        if (shoot_err) {
            LOG(INFO) << "shoot failed!";

            // revert the task states
            for (auto arg : assigned) {
                fifo_rq_.enqueue(arg.second.pid);
                FifoTask* task = alive_tasks_[arg.second.pid];
                task->state = FifoRunState::Queued;
            }

            // revert the cpu states
            for (auto cs : old_cpu_states) {
                cpu_states_[cs.first] = cs.second;
            }

        } else {
            std::vector<std::pair<int, int>> fail = sa_->check_shoot_state(cpu_num);

            for(auto f : fail) {
                // revert the task states
                fifo_rq_.enqueue(f.second);
                FifoTask* task = alive_tasks_[f.second];
                task->state = FifoRunState::Queued;

                // revert the cpu states
                for (auto cs : old_cpu_states) {
                    if (cs.first == f.first) {
                        cpu_states_[f.first] = cs.second;
                        break;
                    }
                }
            }
        }
    }

private:

    virtual void consume_msg_task_runnable(cos_msg msg) {
        u_int32_t tid = msg.pid;
        if (!alive_tasks_.count(tid)) {
            return ;
        }

        auto task = alive_tasks_[tid];
        if (task == nullptr) {
            LOG(ERROR) << "task is null in runnable!";
            exit(1);
        }

        if (task->state == FifoRunState::Queued) {
            LOG(WARNING) << "task enqueue twice!";
            return ;
        }
        fifo_rq_.enqueue(tid);
        task->state = FifoRunState::Queued;
    }

    virtual void consume_msg_task_blocked(cos_msg msg) {
        u_int32_t tid = msg.pid;
        if (!alive_tasks_.count(tid)) {
            return ;
        }

        auto task = alive_tasks_[tid];
        if (task == nullptr) {
            LOG(ERROR) << "task is null in blocked!";
            exit(1);
        }

        if(task->state == FifoRunState::Queued) {
            fifo_rq_.remove_from_rq(tid);
        } else if (task->state == FifoRunState::OnCpu) {
            cpu_states_[task->cpu_id] = CpuState::IDLE;
        }
        task->state = FifoRunState::Blocked;
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
        LOG(INFO) << "task " << msg.pid << " new.";
    }

    virtual void consume_msg_task_new_blocked(cos_msg msg) {
        
        u_int32_t tid = msg.pid;
        if (alive_tasks_.count(tid)) {
            LOG(ERROR) << "same new_thread message, kernel BUGGGGGG!";
            exit(1);
        }

        auto new_task = new FifoTask(tid);
        alive_tasks_[tid] = new_task;
        LOG(INFO) << "task " << msg.pid << " new blocked.";
    }

    virtual void consume_msg_task_dead(cos_msg msg) {
        
        u_int32_t tid = msg.pid;
        if (!alive_tasks_.count(tid)) {
            LOG(ERROR) << "task dead before task new, kernel BUGGGGGG!";
            exit(1);
        }

        auto task = alive_tasks_[tid];
        if (task == nullptr) {
            LOG(ERROR) << "task is null in dead!";
            exit(1);
        }

        if(task->state == FifoRunState::Queued) {
            fifo_rq_.remove_from_rq(tid);
        } else if(task->state == FifoRunState::OnCpu) {
            cpu_states_[task->cpu_id] = CpuState::IDLE;
        }

        alive_tasks_.erase(tid);
        delete task;

        LOG(INFO) << "task " << msg.pid << " dead.";
    }


    // by cos
    virtual void consume_msg_task_preempt_cos(cos_msg msg) {
        u_int32_t tid = msg.pid;
        if (!alive_tasks_.count(tid)) {
            LOG(ERROR) << "task preempt before task new, kernel BUGGGGGG!";
            exit(1);
        }
        auto task = alive_tasks_[tid];
        if (task == nullptr) {
            LOG(ERROR) << "task is null in dead!";
            exit(1);
        }

        assert(task->state != FifoRunState::Blocked);
        if (task->state == FifoRunState::Queued) {
            // printf("%d: %d\n", task->pid, task->state);
            return;
        }
        fifo_rq_.enqueue(tid);
        task->state = FifoRunState::Queued;
        assert(cpu_states_[task->cpu_id] == CpuState::COS);
        // LOG(INFO) << "task " << msg.pid << " cos.";
    }

    // by cfs
    virtual void consume_msg_task_preempt(cos_msg msg) {
        u_int32_t tid = msg.pid;
        if (!alive_tasks_.count(tid)) {
            LOG(ERROR) << "task preempt before task new, kernel BUGGGGGG!";
            exit(1);
        }
        auto task = alive_tasks_[tid];
        if (task == nullptr) {
            LOG(ERROR) << "task is null in dead!";
            exit(1);
        }

        assert(task->state != FifoRunState::Blocked);
        if (task->state == FifoRunState::Queued) {
            // printf("%d: %d\n", task->pid, task->state);
            return;
        }
        fifo_rq_.enqueue(tid);
        task->state = FifoRunState::Queued;
        cpu_states_[task->cpu_id] = CpuState::NOCOS;
        
        LOG(INFO) << "task " << msg.pid << " cfs.";
    }

    FifoRq fifo_rq_;
    std::unordered_map<u_int32_t, FifoTask*> alive_tasks_;
    std::vector<CpuState> cpu_states_;
};

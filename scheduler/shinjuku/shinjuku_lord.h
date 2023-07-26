#include <list>
#include <unordered_map>
#include <cassert>
#include "lord.h"

const int cpu_num = sysconf(_SC_NPROCESSORS_CONF);

u_int64_t preempt_time_slice = 1000 * 50;

enum class ShinjukuRunState {
    Queued,
    OnCpu,
    Blocked
};

enum class ThreadType {
    IDLE,
    COS,
    CFS,
};

struct CpuState {
    ThreadType type;
    u_int32_t pid;
};

struct ShinjukuTask {
    ShinjukuTask(int pid) : pid(pid), state(ShinjukuRunState::Blocked), last_shoot_time(0), cpu_id(-1) {}

    u_int64_t last_shoot_time;
    ShinjukuRunState state;
    int pid;
    int cpu_id;
};

class ShinjukuRq {

public:
    ShinjukuRq() {}

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

class ShinjukuLord : public Lord {

public:
    ShinjukuLord(int lord_cpu) : Lord(lord_cpu), Shinjuku_rq_() {
        cpu_states_.reserve(cpu_num);
        for (int i = 0; i < cpu_num; i ++) {
            cpu_states_[i].type = ThreadType::IDLE;
            cpu_states_[i].pid = 0;
        }
    }

    virtual void schedule() {

        std::vector<std::pair<int, cos_shoot_arg>> assigned;
        cpu_set_t assigned_mask;
	    CPU_ZERO(&assigned_mask);
        std::vector<int> idle_mask, cos_mask, cfs_mask;
        struct timespec ts;
        clock_gettime(CLOCK_MONOTONIC, &ts);
        u_int64_t current_time = ts.tv_sec * 1000000000ULL + ts.tv_nsec;


        for (int cpu = 0; cpu < cpu_num; cpu++) {

            if (cpu == lord_cpu_) {
                continue;
            }    

            
            if (cpu_states_[cpu].type == ThreadType::IDLE) {

                idle_mask.push_back(cpu);

            } else if (cpu_states_[cpu].type == ThreadType::COS) {

                auto task = alive_tasks_[cpu_states_[cpu].pid];
                assert(task->state == ShinjukuRunState::OnCpu);

                if (current_time - task->last_shoot_time >= preempt_time_slice) {
                    cos_mask.push_back(cpu);
                }

            } else if (cpu_states_[cpu].type == ThreadType::CFS) {

                cfs_mask.push_back(cpu);

            }

        }

        while (true) {

            int tid = Shinjuku_rq_.peek();
            if (tid == 0) {
                break;
            }

            int cpu = -1;
            if (!idle_mask.empty()) {
                cpu = idle_mask.back();
                idle_mask.pop_back();
            } else if (!cos_mask.empty()) {
                cpu = cos_mask.back();
                cos_mask.pop_back();
            } else if (!cfs_mask.empty()) {
                cpu = cfs_mask.back();
                cfs_mask.pop_back();
            } else {
                break;
            }

            if (!alive_tasks_.count(tid)) {
                LOG(ERROR) << "task is picked before task new or after task dead, kernel BUGGGGGG!";
                exit(1);
            }
            ShinjukuTask* next = alive_tasks_[tid];

            // do shoot
            cos_shoot_arg arg{next->pid, 0};
            assigned.push_back(std::make_pair(cpu, arg));
            LOG(WARNING) << "shoot " << arg.pid << "  at " << cpu;
            CPU_SET(cpu, &assigned_mask);  

            next->state = ShinjukuRunState::OnCpu;
            next->cpu_id = cpu;
            next->last_shoot_time = current_time;

            cpu_states_[cpu].pid = next->pid;
            cpu_states_[cpu].type = ThreadType::COS;

            Shinjuku_rq_.dequeue();
        }

        sa_->commit_shoot_message(assigned);
        shoot_task(sizeof(assigned_mask), &assigned_mask);

    }

private:

    virtual void consume_msg_task_runnable(cos_msg msg) {
        u_int32_t tid = msg.pid;
        if (!alive_tasks_.count(tid)) {
            return;
        }

        auto task = alive_tasks_[tid];
        if (task == nullptr) {
            LOG(ERROR) << "task is null in runnable!";
            exit(1);
        }

        if (task->state == ShinjukuRunState::Queued) {
            LOG(WARNING) << "task enqueue twice!";
            return;
        }

        Shinjuku_rq_.enqueue(tid);
        task->state = ShinjukuRunState::Queued;
    }

    virtual void consume_msg_task_blocked(cos_msg msg) {
        u_int32_t tid = msg.pid;
        if (!alive_tasks_.count(tid)) {
            return;
        }

        auto task = alive_tasks_[tid];
        if (task == nullptr) {
            LOG(ERROR) << "task is null in blocked!";
            exit(1);
        }

        if (task->state == ShinjukuRunState::Queued) {
            Shinjuku_rq_.remove_from_rq(tid);
        } else if (task->state == ShinjukuRunState::OnCpu) {
            cpu_states_[task->cpu_id].type = ThreadType::IDLE;
            cpu_states_[task->cpu_id].pid = 0;
        }

        task->state = ShinjukuRunState::Blocked;
    }

    virtual void consume_msg_task_new(cos_msg msg) {
        LOG(INFO) << "task " << msg.pid << " new.";

        u_int32_t tid = msg.pid;
        if (alive_tasks_.count(tid)) {
            LOG(ERROR) << "same new_thread message, kernel BUGGGGGG!";
            exit(1);
        }

        auto new_task = new ShinjukuTask(tid);
        alive_tasks_[tid] = new_task;

        Shinjuku_rq_.enqueue(tid);
        new_task->state = ShinjukuRunState::Queued;
    }

    virtual void consume_msg_task_new_blocked(cos_msg msg) {
        LOG(INFO) << "task " << msg.pid << " new blocked.";

        u_int32_t tid = msg.pid;
        if (alive_tasks_.count(tid)) {
            LOG(ERROR) << "same new_thread message, kernel BUGGGGGG!";
            exit(1);
        }

        auto new_task = new ShinjukuTask(tid);
        alive_tasks_[tid] = new_task;
    }

    virtual void consume_msg_task_dead(cos_msg msg) {
        LOG(INFO) << "task " << msg.pid << " dead.";

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

        assert(task->state == ShinjukuRunState::Blocked);

        alive_tasks_.erase(tid);
        // remember to delete
        delete task;
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

        assert(task->state == ShinjukuRunState::OnCpu);
        Shinjuku_rq_.enqueue(tid);
        task->state = ShinjukuRunState::Queued;
        cpu_states_[task->cpu_id].type = ThreadType::CFS;
        
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

        assert(task->state == ShinjukuRunState::OnCpu);
        Shinjuku_rq_.enqueue(tid);
        task->state = ShinjukuRunState::Queued;
        assert(cpu_states_[task->cpu_id].type == ThreadType::COS);

    }

    ShinjukuRq Shinjuku_rq_;
    std::unordered_map<u_int32_t, ShinjukuTask*> alive_tasks_;
    std::vector<CpuState> cpu_states_;
};

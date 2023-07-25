#include "message_queue.h"
#include "shoot_area.h"
#include "cos.h"

class Lord {

public: 

	Lord(int lord_cpu) {
		if (set_lord(lord_cpu)) {
			LOG(ERROR) << "set lord fail!";
            exit(1);
		}
		LOG(INFO) << "set lord success!";

		lord_cpu_ = lord_cpu;
		cpu_num_ = sysconf(_SC_NPROCESSORS_CONF);
		LOG(INFO) << "cpu nums: " << cpu_num_;

    	mq_ = new MessageQueue;
    	sa_ = new ShootArea;
	}


    void consume_message() {
        cos_msg msg;
        while (!mq_->empty()) {
            msg = mq_->consume_msg();

            switch (msg.type) {
	        case MSG_TASK_RUNNABLE: 
	        	consume_msg_task_runnable(msg);
	        	break;
	        case MSG_TASK_BLOCKED:
	        	consume_msg_task_blocked(msg);
	        	break;
	        case MSG_TASK_NEW:
	        	consume_msg_task_new(msg);
	        	break;
	        case MSG_TASK_DEAD:
	        	consume_msg_task_dead(msg);
	        	break;
	        case MSG_TASK_PREEMPT:
	        	consume_msg_task_preempt(msg);
	        	break;
			case MSG_TASK_NEW_BLOCKED:
				consume_msg_task_new_blocked(msg);
	        	break;
			case MSG_TASK_COS_PREEMPT:
				consume_msg_task_preempt_cos(msg);
	        default:
				LOG(WARNING) << "unknown cos_msg type  " << msg.type << "!";
	        	break;
	        }
        }

		return;
    }

    virtual void schedule() = 0;
	
protected:

    virtual void consume_msg_task_runnable(cos_msg msg) = 0;
    virtual void consume_msg_task_blocked(cos_msg msg) = 0;
    virtual void consume_msg_task_new(cos_msg msg) = 0;
    virtual void consume_msg_task_new_blocked(cos_msg msg) = 0;
    virtual void consume_msg_task_dead(cos_msg msg) = 0;
    virtual void consume_msg_task_preempt(cos_msg msg) = 0;
    virtual void consume_msg_task_preempt_cos(cos_msg msg) = 0;

    int lord_cpu_ = -1;
    int cpu_num_ = -1;
    MessageQueue *mq_ = nullptr;
    ShootArea *sa_ = nullptr;
};

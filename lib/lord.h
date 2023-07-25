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

		lord_cpu_ = lord_cpu;
		cpu_num_ = sysconf(_SC_NPROCESSORS_CONF);
    	mq_ = new MessageQueue;
    	sa_ = new ShootArea;
	}


    void consume_message() {
        cos_msg msg;
        while (!mq_->empty()) {
            msg = mq_->consume();

            switch (msg->type) {
	        case MSG_TASK_RUNNABLE: 
	        	consume_msg_task_runnable(msg);
	        	break;
	        case MSG_TASK_BLOCKED:
	        	consume_msg_task_blocked(msg);
	        	break;
	        case MSG_TASK_NEW:
	        	consume_msg_task_new(msg);
				LOG(WARNING) << "do not support cos msg  " << MSG_TASK_NEW << "!";
	        	break;
	        case MSG_TASK_DEAD:
	        	consume_msg_task_dead(msg);
				LOG(WARNING) << "do not support cos msg  " << MSG_TASK_DEAD << "!";
	        	break;
	        case MSG_TASK_PREEMPT:
	        	consume_msg_task_preempt(msg);
				LOG(WARNING) << "do not support cos msg  " << MSG_TASK_PREEMPT << "!";
	        	break;
	        default:
				LOG(WARNING) << "unknown cos_msg type  " << msg->type << "!";
	        	break;
	        }
        }

		return;
    }

private:

    virtual void consume_msg_task_runnable(cos_msg msg) = 0;
    virtual void consume_msg_task_blocked(cos_msg msg) = 0;
    virtual void consume_msg_task_new(cos_msg msg) = 0;
    virtual void consume_msg_task_dead(cos_msg msg) = 0;
    virtual void consume_msg_task_preempt(cos_msg msg) = 0;

    virtual void schedule() = 0;

    int lord_cpu_ = -1;
    int cpu_num_ = -1;
    MessageQueue *mq_ = nullptr;
    ShootArea *sa_ = nullptr;
};

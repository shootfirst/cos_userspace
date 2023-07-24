#include "message_queue.h"
#include "shoot_area.h"
#include "cos.h"

class Lord {

public: 

	Lord(int lord_cpu) {
		
	}


    void consume_message(cos_msg msg) {
        cos_msg msg;
        while (!mq_->empty()) {
            msg = mq->consume();

            switch (msg->type) {
	        case MSG_TASK_RUNNABLE: 
	        	consume_msg_task_runnable(msg);
	        	break;
	        case MSG_TASK_BLOCKED:
	        	consume_msg_task_blocked(msg);
	        	break;
	        case MSG_TASK_NEW:
	        	consume_msg_task_new(msg);
	        	printf("do not support cos msg %d\n", MSG_TASK_NEW);
	        	break;
	        case MSG_TASK_DEAD:
	        	consume_msg_task_dead(msg);
	        	printf("do not support cos msg %d\n", MSG_TASK_DEAD);
	        	break;
	        case MSG_TASK_PREEMPT:
	        	consume_msg_task_preempt(msg);
	        	printf("do not support cos msg %d\n", MSG_TASK_PREEMPT);
	        	break;
	        default:
	        	printf("unknown cos_msg type %d!\n", msg->type);
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

    int lord_cpu_;
    cpu_set_t enclave_cpu_mask_;
    MessageQueue *mq_;
    ShootArea *sa_;
};

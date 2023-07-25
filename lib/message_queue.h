#include <fcntl.h>
#include <stdint.h>
#include <sys/mman.h>

#include "cos.h"
#include "../kernel/message_queue.h"

#define smp_mb()        asm volatile("mfence":::"memory")

class MessageQueue {
public:

    MessageQueue() {
        mq_fd_ = create_mq();
        if (mq_fd_ < 0) {
            LOG(ERROR) << "create message queue fd fail!";
            exit(1);
        }
        mq_ = static_cast<cos_message_queue*>(mmap(nullptr, _MQ_MAP_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, mq_fd_, 0));
        if (mq_ == MAP_FAILED) {  
            LOG(ERROR) << "mmap message queue fail!";
            exit(1);
        }
    }

    message consume_msg() {
        if (empty())   return {}; // queue empty
        message ret = mq_->data[mq_->tail % _MQ_DATA_SIZE];
        smp_mb();
        mq_->tail++;
        smp_mb();
        return ret;
    }

    bool empty() {
        return (mq_->tail == mq_->head);
    }

private:
    int mq_fd_;
    cos_message_queue * mq_;
};

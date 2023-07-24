#include <stdint.h>
#include "../kernel/message_queue.h"

#define smp_mb()        asm volatile("mfence":::"memory")
#define smp_rmb()       asm volatile("lfence":::"memory")
#define smp_wmb()       asm volatile("sfence":::"memory")

class MessageQueue {
public:
    MessageQueue() {

    }

    message consume_msg() {
        
    }

private:
    int mq_fd_;
    cos_message_queue *q
};



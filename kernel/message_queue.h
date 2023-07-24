#define _MSG_TASK_FIRST	1
enum {
	MSG_TASK_RUNNABLE  = _MSG_TASK_FIRST,
	MSG_TASK_BLOCKED,
	MSG_TASK_NEW,
	MSG_TASK_DEAD,
	MSG_TASK_PREEMPT,
};

#define _MQ_SIZE 511

struct cos_msg {
	u_int32_t pid;
	u_int32_t type;
};

struct cos_message_queue {
	u_int32_t head;
	u_int32_t tail;
	struct cos_msg data[_MQ_SIZE];
};
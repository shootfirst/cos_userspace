# include <iostream>
#include <sched.h>
# include<unistd.h>

int main() {
	int tid = getpid();
	printf("%d\n", tid);
	struct sched_param param = {.sched_priority = 0};
	sched_setscheduler(tid, 8, &param);
	// perror("");
	// 底层获取1号cpu的runqueue，将你加进去，发送消息
	sleep(1);
	for (int i = 0; i < 10000; i++)
		printf("%d %d\n", i, sched_getscheduler(tid));
}

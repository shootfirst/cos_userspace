#ifndef COS_H
#define COS_H

#include <sys/syscall.h> 

#define SYSCALL_SET_LORD        452
#define SYSCALL_CREATE_MQ       453
#define SYSCALL_INIT_SHOOT      454
#define SYSCALL_SHOOT_TASK      455

int set_lord(int cpu_id) {
	return syscall(SYSCALL_SET_LORD, cpu_id); 
}

int create_mq() {
	return syscall(SYSCALL_CREATE_MQ); 
}

int init_shoot() {
	return syscall(SYSCALL_INIT_SHOOT); 
}

int shoot_task(size_t cpusetsize, cpu_set_t *mask) {
	return syscall(SYSCALL_SHOOT_TASK, cpusetsize, mask); 
}

#endif // !COS_H

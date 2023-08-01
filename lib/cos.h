#ifndef COS_H
#define COS_H

#include <sys/syscall.h> 

#define SYSCALL_SET_LORD        452
#define SYSCALL_CREATE_MQ       453
#define SYSCALL_INIT_SHOOT      454
#define SYSCALL_SHOOT_TASK      455

#define SYSCALL_COSCG_CREATE    456
#define SYSCALL_COSCG_CTL       457
#define SYSCALL_COSCG_RATE      458
#define SYSCALL_COSCG_DELETE    459

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


int coscg_create() {
	return syscall(SYSCALL_COSCG_CREATE); 
}

int coscg_ctl(int coscg_id, pid_t pid, int mode) {
	return syscall(SYSCALL_COSCG_CTL, coscg_id, pid, mode); 
}

int coscg_rate(int coscg_id, int rate) {
	return syscall(SYSCALL_COSCG_RATE, coscg_id, rate);
}

int coscg_delete(int coscg_id) {
	return syscall(SYSCALL_COSCG_DELETE, coscg_id);
}

#endif // !COS_H

# include <iostream>
#include <sched.h>
# include<unistd.h>
# include<sys/syscall.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <fcntl.h>


struct cos_msg {
	u_int32_t pid;
	u_int32_t type;
};

struct cos_message_queue {
	u_int32_t head;
	u_int32_t tail;
	struct cos_msg data[511];
};

struct cos_shoot_arg {
	u_int32_t pid;
	u_int32_t info;
};

struct cos_shoot_area {
	struct cos_shoot_arg area[512];
};

int set_lord(int cpu_id) {
	return syscall(452, cpu_id); 
}

int create_mq() {
	return syscall(453); 
}

int init_shoot() {
	return syscall(454); 
}

void shoot_task(size_t cpusetsize, cpu_set_t *mask) {
	syscall(455, cpusetsize, mask); 
}

int main() {
	int lord_cpu = 6;
	int shoot_cpu = 7;
	int tid = getpid();
	printf("%d\n", tid);

	// set lord--------------------------------------------
	int res = set_lord(lord_cpu);
	perror("");
	if (res != 0) {
		printf("fail\n");
		return 0;
	}

	// create mq-------------------------------------------
	int fd = create_mq();
	perror("");
	auto mq = static_cast<cos_message_queue*>(
      mmap(nullptr, 4096, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0));
	  perror(NULL);
	if (mq == MAP_FAILED) {  
    	printf("mmap failed\n");
		return 0;
    }

	// init shoot------------------------------------------
	int fd1 = init_shoot();
	perror("");
	auto sa = static_cast<cos_shoot_area*>(
      mmap(nullptr, 4096, PROT_READ | PROT_WRITE, MAP_SHARED, fd1, 0));
	  perror(NULL);
	if (sa == MAP_FAILED) {  
    	printf("mmap failed1\n");
		return 0;
    }
	// printf("get shoot area %d\n", sa->area[cpu_id].pid);

	// read msg--------------------------------------------
	u_int32_t volatile t = 0;
	do {
		// printf("wuhu %d\n", tid);
		// sleep(1);
		t = mq->head;
	} while (t == 0);
	int pid = mq->data[0].pid;
	printf("%d get msg successfully\n", pid);
	
	// shoot-----------------------------------------------
	cpu_set_t mask;  
	CPU_ZERO(&mask);    
    CPU_SET(shoot_cpu, &mask);  
	sa->area[shoot_cpu].pid = pid;

	shoot_task(sizeof(mask), &mask);
	// shoot_task(pid);
	sleep(1);
	shoot_task(sizeof(mask), &mask);
	printf("success shoot !%d\n", sched_getscheduler(tid));
}

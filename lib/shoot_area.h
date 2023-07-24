#include <stdint.h>
#include "../kernel/shoot_area.h"

#define smp_mb()        asm volatile("mfence":::"memory")
#define smp_rmb()       asm volatile("lfence":::"memory")
#define smp_wmb()       asm volatile("sfence":::"memory")

class ShootArea {
public:
    ShootArea() {

    }

    void commit_shoot_message(vector<cos_shoot_arg> args) {

    }

private:
    int sa_fd_;
};
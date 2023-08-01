#ifndef COS_CGROUP_H
#define COS_CGROUP_H

#include <unordered_set>

#include "cos.h"
#include "../cos_cgroup.h"

class CosCgroup {

    CosCgroup() {

    }

    int adjust_rate(int rate) {

    }

    int add_thread(u_int32_t pid) {

    }

    int erase_thread(u_int32_t pid) {

    }

    ~CosCgroup() {

    }

private:
    int coscg_id_;
    std::unordered_set<u_int32_t> threads_;
    int rate_;

}
#endif // !COSCGROUP_H

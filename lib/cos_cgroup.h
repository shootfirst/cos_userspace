#ifndef COS_CGROUP_H
#define COS_CGROUP_H

#include <iostream>
#include "cos.h"
#include "../kernel/cos_cgroup.h"

class CosCgroup {
public:
    CosCgroup() {
        coscg_id_ = coscg_create();
        if (coscg_id_ < 0) {
            printf("create coscg failed\n");
            exit(1);
        }
    }

    int adjust_rate(int rate) {
        int res = coscg_rate(coscg_id_, rate);
        if (res < 0) {
            printf("coscg adjust rate failed\n");
            return res;
        }
        rate_ = rate;
    }

    int add_thread(u_int32_t pid) {
        int res = coscg_ctl(coscg_id_, pid, _COS_CGROUP_TASK_ADD);
        if (res < 0) {
            printf("coscg add thread failed\n");
            return res;
        }
        return 0;
    }

    int erase_thread(u_int32_t pid) {
        int res = coscg_ctl(coscg_id_, pid, _COS_CGROUP_TASK_DELETE);
        if (res < 0) {
            printf("coscg erase thread failed\n");
            return res;
        }
        return 0;
    }

    ~CosCgroup() {
        coscg_delete(coscg_id_);
    }

private:
    int coscg_id_ = -1;
    int rate_ = 0;
};
#endif // !COSCGROUP_H

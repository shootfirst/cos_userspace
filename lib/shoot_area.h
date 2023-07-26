#include <vector>
#include <utility>

#include "../kernel/shoot_area.h"
#include "cos.h"

class ShootArea {

public:
    ShootArea() {
        sa_fd_ = init_shoot();
        if (sa_fd_ < 0) {
            LOG(ERROR) << "create shoot area fd fail!";
            exit(1);
        }

        sa_ = static_cast<cos_shoot_area*>(mmap(nullptr, _SHOOT_AREA_MAP_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, sa_fd_, 0));
        if (sa_ == MAP_FAILED) {
            LOG(ERROR) << "mmap shoot area fail!";
            exit(1);
        }
		LOG(INFO) << "create shoot area success!";
    }

    void commit_shoot_message(std::vector<std::pair<int, cos_shoot_arg>> &args, u_int16_t seq) {
        for (auto cpu2arg : args) {
            // printf("cpu %d pid %d\n", cpu2arg.first, cpu2arg.second.pid);
            sa_->area[cpu2arg.first] = cpu2arg.second;
        }
        sa_->seq = seq;
    }

    std::vector<std::pair<int, int>> check_shoot_state(int cpu_nums) {
        std::vector<std::pair<int, int>> fail;
        for (int i = 0; i < cpu_nums; i ++) {
            if (sa_->area[i].info == _SA_ERROR) {
                fail.push_back(std::make_pair(i, sa_->area[i].pid));
            }
            sa_->area[i].info = _SA_RIGHT;
        }
        return fail;
    }

private:

    int sa_fd_;
    cos_shoot_area *sa_;
};
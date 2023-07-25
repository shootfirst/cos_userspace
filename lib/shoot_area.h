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

    void commit_shoot_message(std::vector<std::pair<int, cos_shoot_arg>> args) {
        for (auto cpu2arg : args) {
            sa_->area[cpu2arg.first] = cpu2arg.second;
        }
    }

private:

    int sa_fd_;
    cos_shoot_area *sa_;
};
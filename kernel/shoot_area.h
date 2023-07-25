#define _SHOOT_AREA_MAP_SIZE 4096
#define _SHOOT_AREA_SIZE 512
struct cos_shoot_arg {
	u_int32_t pid;
	u_int32_t info;
};

struct cos_shoot_area {
	struct cos_shoot_arg area[_SHOOT_AREA_SIZE];
};
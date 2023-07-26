#define _SHOOT_AREA_MAP_SIZE 4096
#define _SHOOT_AREA_SIZE 511
#define _SA_RIGHT 0
#define _SA_ERROR 1
struct cos_shoot_arg {
	u_int32_t pid;
	u_int32_t info;
};

struct cos_shoot_area {
	u_int64_t seq;
	struct cos_shoot_arg area[_SHOOT_AREA_SIZE];
};
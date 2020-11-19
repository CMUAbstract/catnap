#ifndef __PERIOD__
#define __PERIOD__

#include <libos/mode.h>
#include <libos/global.h>

#define MAX_PERIOD_NUM 3

typedef struct {
	T_t T_ub;
	T_t T_lb;
	T_t T;
	T_t Ts[MODE_NUM];
	T_t (*degrade_T)();
} period_t;

enum {P_SUCCESS=0, P_FAIL=1};

#define PERIOD(name, lb, ub, ...) PERIOD_(name, lb, ub, ##__VA_ARGS__, 2, 1)

#define PERIOD_(name, lb, ub, rule, n, ...) PERIOD ## n(name, lb, ub, rule)

#define PERIOD1(name, lb, ub, ...) \
	PERIOD2(name, lb, ub, T = (T < (UINT32_MAX >> 1)) ? T * 2 : UINT32_MAX);

#define PERIOD2(name, lb, ub, rule) \
	T_t degrade_T_ ## name();\
	__nv period_t name = {.T_ub = ub, .T_lb = lb,\
	.T = lb, .degrade_T = &degrade_T_ ## name};\
	T_t degrade_T_ ## name()\
	{\
		if (name.T == name.T_ub)\
			return P_FAIL;\
		T_t T = name.T;\
		rule;\
		if (T <= name.T_ub) {\
			name.T = T;\
		} else {\
			name.T = name.T_ub;\
		}\
		return P_SUCCESS;\
	}

#define save_period_config(p) \
	p->Ts[get_mode()] = p->T;

extern period_t* period_list[MAX_PERIOD_NUM];
extern unsigned period_list_it;

void init_periods();
void load_period_config(unsigned mode);

#endif

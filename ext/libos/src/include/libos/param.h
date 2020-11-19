#ifndef __PARAM__
#define __PARAM__

#include <libos/global.h>
#include <libos/mode.h>

#define MAX_PARAM_NUM 3

typedef struct {
	unsigned param_ub;
	unsigned param_lb;
	unsigned param;
	unsigned params[MODE_NUM];
	unsigned (*degrade_param)();
} param_t;

enum {PARAM_SUCCESS=0, PARAM_FAIL=1};

#define PARAM(name, lb, ub, ...) PARAM_(name, lb, ub, ##__VA_ARGS__, 2, 1)

#define PARAM_(name, lb, ub, rule, n, ...) PARAM ## n(name, lb, ub, rule)

#define PARAM1(name, lb, ub, ...) \
	PARAM2(name, lb, ub, param = param / 2);

#define PARAM2(name, lb, ub, rule) \
	unsigned degrade_param_ ## name();\
	__nv param_t name = {.param_ub = ub, .param_lb = lb,\
	.param = ub, .degrade_param = &degrade_param_ ## name};\
	unsigned degrade_param_ ## name()\
	{\
		if (name.param == name.param_lb)\
			return PARAM_FAIL;\
		unsigned param = name.param;\
		rule;\
		if (param >= name.param_lb) {\
			name.param = param;\
		} else {\
			name.param = name.param_lb;\
		}\
		return PARAM_SUCCESS;\
	}

#define save_param_config(p) \
	p->params[get_mode()] = p->param;

extern param_t* param_list[MAX_PARAM_NUM];
extern unsigned param_list_it;

void init_params();
void load_param_config(unsigned mode);

#endif

#include <libos/param.h>
#include <libmsp/mem.h>

__nv param_t* param_list[MAX_PARAM_NUM];
__nv unsigned param_list_it = 0;

void load_param_config(unsigned mode) {
	for (unsigned i = 0; i < param_list_it; ++i) {
		param_t *p = param_list[i];
		p->param = p->params[mode];
	}
}

void init_params() {
	// Init params as large as possible
	for (unsigned i = 0; i < param_list_it; ++i) {
		for (unsigned j = 0; j < MODE_NUM; ++j) {
			param_list[i]->params[j] = param_list[i]->param_ub;
		}
	}
}

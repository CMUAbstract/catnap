#ifndef __FUNCS__
#define __FUNCS__

#include <libos/mode.h>
#include <libos/global.h>

// This can be embedded in Event struct,
// but we are making it separate so that the
// hacky macros are more manageable
#define MAX_FUNC_NUM 3

typedef struct {
	uint8_t* funcs[MAX_FUNC_NUM];
	unsigned funcs_num;
	unsigned func_of_mode[MODE_NUM];
} funcs_t;

// We only declare here.. let the python do all
// the fancy works for us
#define FUNCS(name, ...) \
	__nv funcs_t name;
	// Declare functions
// Below does not work. Not sure why but the
// compiler thinks function addresses are unknown
// in link time.
// Let the python do this for us.
//	unsigned name ## _test[MAX_FUNC_NUM] = {__VA_ARGS__};

#endif

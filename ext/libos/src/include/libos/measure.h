#ifndef __MEASURE__
#define __MEASURE__
#include <libos/global.h>

#define INT_THRES 1000


unsigned is_vdd_ok();
unsigned get_VCAP();
unsigned get_dout();
uint32_t get_R_using_dout();

#endif

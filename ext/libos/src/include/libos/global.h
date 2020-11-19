#ifndef __GLOBAL__
#define __GLOBAL__
#include <stdint.h>

typedef uint32_t T_t;

#define ENERGY_MEASURE 1
#define HIGH_VOLTAGE 1
// QuickRecall = 1 is slower for some reason
// I think it is because the power system is inefficient
// (or because there is BLE attached, etc...)
// So testing with QUICKRECALL = 1 is not correct
#define QUICKRECALL 0

#endif

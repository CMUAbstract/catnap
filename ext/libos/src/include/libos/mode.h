#ifndef __MODE__
#define __MODE__
#include <libos/global.h>

#define MODE_NUM 4 // TEMP: 0-2000, 2000-4000, 4000-6000, 6000-Continuous
#define MODE_THRES_HIGH 750

extern unsigned cur_mode;
extern unsigned mode_thres[MODE_NUM - 1];
unsigned estimate_mode(unsigned cr);
void switch_mode(unsigned mode);
//void load_event_config(unsigned mode);

#define get_mode() cur_mode

#endif

#ifndef __SCHED__
#define __SCHED__

#include <libos/global.h>
#include <libos/event.h>

#define U_AMP_FACTOR 100
#define UTIL_STEP 10
#define EVENT_UTIL_MIN 10 // At most only 50% should be reserved for tasks
extern unsigned event_util_max; // if this is U_AMP_FACTOR, it means 100% of the utilization can be used by events

enum {SCHEDULE_FAIL, SCHEDULE_SUCCESS, SCHEDULE_RETRY};
enum {DEGRADE_SUCCESS=0, DEGRADE_FAIL=1, DEGRADE_NOT_READY=2};

unsigned is_schedulable(uint32_t charge_rate);
void calculate_C(uint32_t charge_rate);
unsigned calculate_L_b();
unsigned scheduling_start();
T_t get_next_e_and_t(Event** ret_event);
unsigned degrade_event();
uint8_t schedule_one_time_event(Event* e);
unsigned degrade_task();
unsigned decrease_slack();

extern unsigned event_set_degradation_start;

#endif

#ifndef __EVENT__
#define __EVENT__
#include <libos/fifo.h>
#include <libos/mode.h>
#include <libos/period.h>
#include <libos/param.h>
#include <libos/funcs.h>
#include <libos/global.h>

#define MAX_EVENT_NUM 3

#define WORD_MAX UINT16_MAX

void adjust_nextT(T_t time);
void rollback_nextT();
void backup_nextT();

enum {PERIODIC=0, APERIODIC=1, BURSTY=2};

typedef struct {
	void (*event_func)(unsigned p);
	//unsigned T;
	T_t (*get_period)();
	T_t nextT;
	T_t shadow_nextT; // for future scheduling
	uint32_t reservedE[MODE_NUM];
	// Be careful, this always needs to be populated before use!
	uint32_t charge_time;
	uint8_t* input_fifo;
	funcs_t* funcs;
	unsigned event_type; // 0: periodic, 1: aperiodic, 2: bursty-periodic
	unsigned scalar;
	period_t* base_period;
	param_t* param; // Only one param for now

	period_t* period_in_burst;
	unsigned burst_num;
	unsigned burst_cnt;
	unsigned burst_cnt_shadow;

	T_t mask_timer; // Only used for aperiodic events.
	//Mask further registering of the same event until expires.
} Event;

// Periodic events
extern Event* event_list[MAX_EVENT_NUM];
extern unsigned event_list_it;

// Periodic + aperiodic events
extern Event* event_list_all[MAX_EVENT_NUM];
extern unsigned event_list_all_it;

void deregister_event(Event* e); 
unsigned event_active(Event* e);

#define EVENT_PERIODIC(name, funcs, period, multiplicand, ...) \
	EVENT_PERIODIC_(name, funcs, period, multiplicand, ##__VA_ARGS__, 2, 1)

#define EVENT_PERIODIC_(name, funcs, period, multiplicand, _param, n, ...) \
	EVENT_PERIODIC ## n(name, funcs, period, multiplicand, _param)

#define EVENT_PERIODIC1(name, funcs, period, multiplicand, ...) \
	EVENT(name, funcs, period, multiplicand, 0)

#define EVENT_PERIODIC2(name, funcs, period, multiplicand, _param) \
	EVENT(name, funcs, period, multiplicand, 0, _param)


#define EVENT_APERIODIC(name, funcs, period, multiplicand, ...) \
	EVENT_APERIODIC_(name, funcs, period, multiplicand, ##__VA_ARGS__, 2, 1)

#define EVENT_APERIODIC_(name, funcs, period, multiplicand, _param, n, ...) \
	EVENT_APERIODIC ## n(name, funcs, period, multiplicand, _param)

#define EVENT_APERIODIC1(name, funcs, period, multiplicand, ...) \
	EVENT(name, funcs, period, multiplicand, 1)

#define EVENT_APERIODIC2(name, funcs, period, multiplicand, _param) \
	EVENT(name, funcs, period, multiplicand, 1, _param)

#define EVENT(name, funcs, period, multiplicand, is_one_time, ...) \
	T_t get_period_ ## name() \
	{\
		return ((T_t)multiplicand) * period.T;\
	}\
	EVENT_(name, funcs, period, multiplicand, is_one_time, ##__VA_ARGS__, 2, 1)

#define EVENT_(name, funcs, period, multiplicand, is_one_time, _param, n, ...) \
	EVENT ## n(name, funcs, period, multiplicand, is_one_time, _param)

#define EVENT1(name, _funcs, period, multiplicand, is_one_time, ...) \
	__nv Event name = {.funcs = &_funcs, \
	.get_period = &get_period_ ## name,\
	.reservedE = {0}, .event_type = is_one_time,\
	.scalar = multiplicand, .base_period = &period, .param = NULL, .mask_timer = 0};

#define EVENT2(name, _funcs, period, multiplicand, is_one_time, _param) \
	__nv Event name = {.funcs = &_funcs, \
	.get_period = &get_period_ ## name,\
	.reservedE = {0}, .event_type = is_one_time,\
	.scalar = multiplicand, .base_period = &period, .param = &_param, .mask_timer = 0};

#define BUF sec_to_tick(0.1)
// Rate limiting
#define POST_EVENT(event) \
	if (event.mask_timer < TA0R + BUF) {\
		schedule_one_time_event(&event);\
	}

#define DISABLE_EVENT(event) \
	deregister_event(&event);

void load_param_config(unsigned mode);
Event* get_next_event(T_t *nextT);

#endif

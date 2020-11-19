#ifndef __OS__
#define __OS__
#include <msp430.h>
#include <stddef.h>
#include <libos/event.h>
#include <libos/comp.h>
#include <libos/global.h>
void os_main();
void init();

enum {POST_SUCCESS=0, POST_FULL=1, POST_INVALID=2};

//#define POST(t) \
//	uint8_t _isFail = push_taskQ(t);\
//	if (_isFail) {\
//		reduce_QoS(cur_event);\
//	}
//#define POST(t) push_taskQ(t);
#define POST(t) push_taskQ(&t);

#define CR_WINDOW_SIZE 3
#define AMP_FACTOR 100
// Counter of conservative
// mode upper change
// Only change mode to upper
// when this many upper mode has been seen
// lower whenever the lower mode has been
// seen
#define MODE_CHANGE_CNT 10

void init_hw();
void undo_input_fifo();
void commit_input_fifo();
void configure_next_wakeup();
uint32_t get_charge_rate_average();
uint32_t get_charge_rate_worst();
extern Event* cur_event;
extern Event* pre_event;
extern volatile uint8_t event_running;

extern unsigned no_task_running;
extern unsigned cr_window_it;
extern unsigned cr_window_ready;
extern uint32_t cr_window[CR_WINDOW_SIZE];
extern unsigned cr_test[CR_WINDOW_SIZE];

extern unsigned v_charge_start;
extern unsigned v_charge_end;
extern unsigned t_charge_start;
extern unsigned t_charge_end;

// Detect falling edge of P4.1
#define init_vdd_fail_detection()\
	P4DIR &= ~BIT1;\
	P4IES |= BIT1;\
	P4IFG &= ~BIT1;\
	P4IE |= BIT1;

#endif

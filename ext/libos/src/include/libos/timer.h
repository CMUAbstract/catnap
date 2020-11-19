#ifndef __TIMER__
#define __TIMER__
#include <libos/scheduler.h>
#include <libos/global.h>

// You might think SEC_TO_TICK_RATIO should be 10,000
// since VLOCLK is supposed to be 10kHz. This is not true,
// because VLOCLK sucks so bad its freq can be anywhere
// near 10kHz. Thus, I use the experimentally chosen value
// Also, it can vary a lot depending on the voltage, so good luck!!
// Note: If you want accuracy, 1) use LF Xtal (external crystal) or
// 2) use REF0 clock (signicantly more current compared to VLO).
// Note: tick should be larger than 1024,
// meaning that the finest resolution is
// 1024/1063 sec
// TODO: Why? I forgot
// Because of the scheduler event degrading!!
// Need fix if we want smaller tick for
// timer-based sleep
//#define SEC_TO_TICK_RATIO 8511
#define SEC_TO_TICK_RATIO 1063
//#define SEC_TO_TICK_RATIO 4255
#define sec_to_tick(sec) ((uint32_t)sec*(uint32_t)SEC_TO_TICK_RATIO)
#define MEASURE_PERIOD sec_to_tick(0.1)
#define timer_interrupt_enable() TA0CCTL0 |= CCIE;
#define timer_interrupt_disable() TA0CCTL0 &= ~CCIE;
#define get_time() global_clock + TA0R;
#define init_global_clock() global_clock = 0;
#define incr_global_clock(t) global_clock += t;
#define HALT_TIMER() \
	TA0CTL = MC_0 | TASSEL_1 | TACLR;
#define DELAY 1

void configure_wakeup();

extern uint32_t global_clock;

#endif

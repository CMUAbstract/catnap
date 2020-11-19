#include <msp430.h>
#include <libos/timer.h>
#include <libmsp/mem.h>


__nv uint8_t timer_restarted;
__nv uint32_t global_clock = 0;

/*
 * Configure timer interrupt after time
 */
void configure_wakeup(unsigned time) {
	// Set and fire Timer A
	TA0CCR0 = time;
	// TODO: Dynamically set for a longer
	// range..
	//TA0CTL = MC_1 | TASSEL_1 | TACLR | ID_1;
	TA0CTL = MC_1 | TASSEL_1 | TACLR | ID_3;

	timer_restarted = 1;
}

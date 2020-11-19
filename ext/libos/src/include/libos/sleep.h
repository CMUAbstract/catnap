#ifndef __SLEEP__
#define __SLEEP__

//TODO: Save the GPIO and reset it for low power sleep

#define low_power_sleep_on_exit() \
	P1OUT &= ~BIT0; \
	__bis_SR_register_on_exit(GIE | LPM3_bits); 

#define low_power_sleep() \
	P1OUT &= ~BIT0; \
	__bis_SR_register(GIE | LPM3_bits); 

#define low_power_wakeup_on_exit() \
	__bic_SR_register_on_exit(LPM3_bits); 

#endif

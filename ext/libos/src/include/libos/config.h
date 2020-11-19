#ifndef __CONFIG__
#define __CONFIG__

#define disable_watchdog() WDTCTL = WDTPW + WDTHOLD
#define unlock_gpio() PM5CTL0 &= ~LOCKLPM5
#define reset_gpio() \
	P1OUT = 0;\
	P1DIR = 0xFF;\
	P2OUT = 0;\
	P2DIR = 0xFF;\
	P3OUT = 0;\
	P3DIR = 0xFF;\
	P4OUT = 0;\
	P4DIR = 0xFF;\
	P5OUT = 0;\
	P5DIR = 0xFF;\
	P6OUT = 0;\
	P6DIR = 0xFF;\
	P7OUT = 0;\
	P7DIR = 0xFF;\
	P8DIR = 0xFF;\
	P8OUT = 0;\
	PJOUT = 0;\
	PJDIR = 0xFFFF;

// DC0: 8MHz
// It shouldn't affect the sleep time, but we'll see
// To make it 1MHz, make DCOFSEL_0
#define init_clock()\
	CSCTL0_H = CSKEY_H;\
	CSCTL1 = DCOFSEL_6;\
	CSCTL2 = SELM__DCOCLK | SELS__DCOCLK | SELA__VLOCLK;\
	CSCTL3 = DIVA__1 | DIVS__1 | DIVM__1;\
	CSCTL4 = LFXTOFF | HFXTOFF;\
	CSCTL0_H = 0;

#endif

#include <libos/measure.h>
#include <libmsp/mem.h>
#include <libmsp/sleep.h>
#include <libos/os.h>
#include <libio/console.h>
#include <msp430.h>

#define NOISE_THRES 10
#define RETRY 5
#define WINDOW_SIZE 5
#define R_AVERAGE 10

__nv unsigned window_it = 0;
__nv unsigned window_ready = 0;
__nv unsigned window[WINDOW_SIZE];

unsigned read_dout_ADC() {

	ADC12CTL0 &= ~ADC12ENC;           // Disable conversions

	ADC12CTL1 = ADC12SHP; // SAMPCON is sourced from the sampling timer
	ADC12MCTL0 = ADC12VRSEL_1 | ADC12INCH_15; // VR+: VREF, VR- = AVSS, // Channel A14
	ADC12CTL0 |= ADC12SHT03 | ADC12ON; // sample and hold time 32 ADC12CLK

	while( REFCTL0 & REFGENBUSY );

	REFCTL0 = REFVSEL_0 | REFON;            //Set reference voltage(VR+) to 1.2
	// If this works as I expect,
	// N = 4096 * (Vin + 0.5*(1.2/4096)) / (1.2) = 4096*Vin/1.2 + 0.5

	__delay_cycles(1000);                   // Delay for Ref to settle
	ADC12CTL0 |= ADC12ENC;                  // Enable conversions

	ADC12CTL0 |= ADC12SC;                   // Start conversion
	ADC12CTL0 &= ~ADC12SC;                  // We only need to toggle to start conversion
	while (ADC12CTL1 & ADC12BUSY) ;

	unsigned output = ADC12MEM0;

	ADC12CTL0 &= ~ADC12ENC;                 // Disable conversions
	ADC12CTL0 &= ~(ADC12ON);                // Shutdown ADC12
	REFCTL0 &= ~REFON;
	return output;
}

unsigned read_VCAP_ADC() {

	ADC12CTL0 &= ~ADC12ENC;           // Disable conversions

	ADC12CTL1 = ADC12SHP; // SAMPCON is sourced from the sampling timer
	ADC12MCTL0 = ADC12VRSEL_1 | ADC12INCH_15; // VR+: VREF, VR- = AVSS, // Channel A15
	ADC12CTL0 |= ADC12SHT03 | ADC12ON; // sample and hold time 32 ADC12CLK

	while( REFCTL0 & REFGENBUSY );

	//REFCTL0 = REFVSEL_2 | REFON;            //Set reference voltage to 2.5
	REFCTL0 = REFVSEL_1 | REFON;            //Set reference voltage(VR+) to 2.0
	// If this works as I expect,
	// N = 4096 * (Vin + 0.5*(2.0/4096)) / (2.0) = 4096*Vin/2.0 + 0.5

	__delay_cycles(1000);                   // Delay for Ref to settle
	ADC12CTL0 |= ADC12ENC;                  // Enable conversions

	ADC12CTL0 |= ADC12SC;                   // Start conversion
	ADC12CTL0 &= ~ADC12SC;                  // We only need to toggle to start conversion
	while (ADC12CTL1 & ADC12BUSY) ;

	unsigned output = ADC12MEM0;

	ADC12CTL0 &= ~ADC12ENC;                 // Disable conversions
	ADC12CTL0 &= ~(ADC12ON);                // Shutdown ADC12
	REFCTL0 &= ~REFON;
	return output;
}


unsigned read_INT() {

	ADC12CTL0 &= ~ADC12ENC;           // Disable conversions

	ADC12CTL1 = ADC12SHP; // SAMPCON is sourced from the sampling timer
	ADC12MCTL0 = ADC12VRSEL_1 | ADC12INCH_14; // VR+: VREF, VR- = AVSS, // Channel A14
	ADC12CTL0 |= ADC12SHT03 | ADC12ON; // sample and hold time 32 ADC12CLK

	while( REFCTL0 & REFGENBUSY );

	//REFCTL0 = REFVSEL_2 | REFON;            //Set reference voltage to 2.5
	REFCTL0 = REFVSEL_0 | REFON;            //Set reference voltage(VR+) to 1.2
	// If this works as I expect,
	// N = 4096 * (Vin + 0.5*(1.2/4096)) / (1.2) = 4096*Vin/1.2 + 0.5

	__delay_cycles(1000);                   // Delay for Ref to settle
	ADC12CTL0 |= ADC12ENC;                  // Enable conversions

	ADC12CTL0 |= ADC12SC;                   // Start conversion
	ADC12CTL0 &= ~ADC12SC;                  // We only need to toggle to start conversion
	while (ADC12CTL1 & ADC12BUSY) ;

	unsigned output = ADC12MEM0;

	ADC12CTL0 &= ~ADC12ENC;                 // Disable conversions
	ADC12CTL0 &= ~(ADC12ON);                // Shutdown ADC12
	REFCTL0 &= ~REFON;
	return output;
}

unsigned get_dout() {
	// Config the ADC on the comparator pin
	P3SEL0 |= BIT2;	
	P3SEL1 |= BIT2;	

	// P4.2 is DSET
	P4DIR |= BIT2;
	P4OUT |= BIT2;
	// wait for 50us
	// Heuristic
	//P1OUT |= BIT2;
	__delay_cycles(500); // around 50us
	//P1OUT &= ~BIT2;

	unsigned v = read_dout_ADC();

	P4OUT &= ~BIT2;

	return v;
}

unsigned is_vdd_ok() {
	// Config the ADC on the comparator pin
	P3SEL0 |= BIT2;	
	P3SEL1 |= BIT2;	

	// P4.3 is DSET
	P4DIR |= BIT3;
	P4OUT |= BIT3;
	// wait for 50us
	// Heuristic
	__delay_cycles(500); // around 50us

	unsigned v = read_INT();

	P4OUT &= ~BIT3;

	if (v > INT_THRES) {
		return 1;
	} else {
		return 0;
	}
}

unsigned get_VCAP() {
	// Config the ADC on the comparator pin
	P3SEL0 |= BIT3;	
	P3SEL1 |= BIT3;	

	// P4.2 is DSET
	P4DIR |= BIT2;
	P4OUT |= BIT2;
	// wait for 50us
	// Heuristic
	__delay_cycles(50); // around 50us

	unsigned n = read_VCAP_ADC();
	P4OUT &= ~BIT2;

	// V = (n - 0.5)*2.0/4096 * 100
	// 100 is to not use float
	// ~ n * 200/4096 ~ n / 34
	//PRINTF("VCAP(n): %u\r\n", n);
	//unsigned v = n / 20;
	uint32_t v = (((uint32_t)n) * 25) / 512;

	
	return (unsigned)v;
}

uint32_t get_R_using_dout() {
	//unsigned sum = get_dout();
	// This reads a lot of zeros..
	// So, lets not use all, but
	// use max
	unsigned sum = 0;
	for (unsigned i = 0; i < R_AVERAGE; ++i) {
		unsigned v = get_dout();
		//sum += v;
		if (v > sum) {
			sum = v;
		}
	}
	uint32_t e = (uint32_t)sum*(uint32_t)sum*AMP_FACTOR; // AMP FACTOR is to keep the old code..

	e /= 5000; // Some magic number with experiment

	return e;
}

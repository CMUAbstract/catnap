#include <msp430.h>
#include <libos/jit.h>
#include <libos/funcs.h>
#include <libos/os.h>
#include <libos/event.h>
#include <libos/task.h>
#include <libos/timer.h>
#include <libos/config.h>
#include <libos/comp.h>
#include <libmsp/mem.h>
#include <libmsp/gpio.h>
#include <libdsp/DSPLib.h>
#include <libmsp/temp.h>
#ifdef LOGIC
#define LOG(...)
#define PRINTF(...)
#define BLOCK_PRINTF(...)
#define BLOCK_PRINTF_BEGIN(...)
#define BLOCK_PRINTF_END(...)
#define INIT_CONSOLE(...)
#else
#include <libio/console.h>
#endif
#include <libmspuartlink/uartlink.h>

//#define SAMPLES 256
#define SAMPLES 256

#define WINDOW_SIZE 10
__nv float filter[WINDOW_SIZE] = {
	0.1, 0.1, 0.1, 0.1, 0.1,	
	0.1, 0.1, 0.1, 0.1, 0.1,	
};

__ro_nv unsigned golden[SAMPLES - (WINDOW_SIZE - 1)] = {
#include "input_mic_result.txt"
};

#define RADIO_PAYLOAD_LEN 4
#define MIC_EMUL 1

#if MIC_EMUL == 1
__ro_nv int input_data[SAMPLES] = {
#include "input_mic.txt"	
};
#endif

typedef enum __attribute__((packed)) {
    RADIO_CMD_SET_ADV_PAYLOAD = 0,
} radio_cmd_t;

typedef struct __attribute__((packed)) {
    radio_cmd_t cmd;
    uint8_t payload[RADIO_PAYLOAD_LEN];
} radio_pkt_t;

// Events and tasks definition

// Period for running fft. variable
PERIOD(p1, sec_to_tick(0.5), sec_to_tick(0.5));
PERIOD(p2, sec_to_tick(2), sec_to_tick(2));

PARAM(sample_size, 16, SAMPLES);

FUNCS(f_mic, event_mic);
FUNCS(f_tx, event_transmit);
FUNCS(f_fft, task_fft);
FUNCS(f_temp, event_temp);

EVENT_PERIODIC(e_mic, f_mic, p2, 1);
EVENT_APERIODIC(e_tx, f_tx, p2, 1);
EVENT_PERIODIC(e_temp, f_temp, p1, 1);

TASK(t_fft, f_fft, sample_size);

// INOUT buf declaration
typedef struct {
	int buf[SAMPLES];
	uint32_t timestamp;
} mic_fft_buf;

typedef struct {
	float buf[SAMPLES - ((WINDOW_SIZE - 1))];
	//unsigned result;
	uint32_t timestamp;
} fft_tx_buf;

FIFO(e_mic, t_fft, mic_fft_buf);
FIFO(t_fft, e_tx, fft_tx_buf);

void lpf(const float *buf, const float *filt, float *result, unsigned size, unsigned param)
{
	float local_result[SAMPLES - (WINDOW_SIZE - 1)];
	unsigned step = size / param; // This should be an integer
	for (unsigned i = 0; i < size - (WINDOW_SIZE - 1); ++i) {
		local_result[i] = 0;
		if (i % step == 0) {
			// Do calculation (loop perforation)
			for (unsigned j = 0; j < WINDOW_SIZE; ++j) {
				local_result[i] += buf[i + j] * filt[j];
			}
		} else {
			// Stupid sample and hold
			local_result[i] = local_result[i-1];
		}
		result[i] = local_result[i];
	}
}

// Events and tasks declaration from here
void task_fft(unsigned param)
{
	P1OUT |= BIT4;
	// Pull result from e_mic
	mic_fft_buf* in = POP_FIFO(e_mic, t_fft);

	float buf[SAMPLES];
	// Heuristically, we choose 0.75V as a bias.
	//int bias = 75;

	// Convert to Q15
	// Undersample (without LPF..)
	//unsigned step = SAMPLES / param;
	for (unsigned i = 0; i < SAMPLES; ++i) {
		//buf[i] = ((float)(in->buf[i] - bias) / 100);
		buf[i] = ((float)(in->buf[i]) / 100);
		//buf[i] = ((float)(in->buf[i * step] - bias) / 100);
	}
	// FFT
	fft_tx_buf *res = GET_EMPTY_FIFO(t_fft, e_tx);
	lpf(buf, filter, res->buf, SAMPLES, param);

	// Interpolate!!!


	// Approximate with linear interpolation
	//unsigned step = SAMPLES / param;
	//for (unsigned i = 0; i < SAMPLES; ++i) {
	//	if (i % step == 0) {
	//		// Exact calculation every 'step'
	//		buf[i] = _Q15((float)(in->buf[i] - bias) / 100);
	//	} else {
	//		// Interpolate!
	//	}
	//}

	//fft_tx_buf *res = GET_EMPTY_FIFO(t_fft, e_tx);
	//fft(buf, res->buf);

	// Push fifo as ready
	PUSH_FIFO(t_fft, e_tx, res);

	// Post e_tx
	POST_EVENT(e_tx);

	P1OUT &= ~BIT4;
}

void init_adc() {
	ADC12CTL0 &= ~ADC12ENC;           // Disable conversions

	ADC12CTL1 = ADC12SHP;
	//ADC12MCTL0 = ADC12VRSEL_1 | ADC12INCH_14; // Channel A14
	ADC12MCTL0 = ADC12VRSEL_0 | ADC12INCH_14; // Channel A14 / REFV = 3.3V
	ADC12CTL0 |= ADC12SHT03 | ADC12ON;

	//while( REFCTL0 & REFGENBUSY );

	//REFCTL0 = REFVSEL_2 | REFON;            //Set reference voltage to 2.5
	//REFCTL0 = REFVSEL_0 | REFON;            //Set reference voltage(VR+) to 1.2

	__delay_cycles(1000);                   // Delay for Ref to settle
	ADC12CTL0 |= ADC12ENC;                  // Enable conversions
}

unsigned read_mic(void){
	ADC12CTL0 |= ADC12SC;                   // Start conversion
	ADC12CTL0 &= ~ADC12SC;                  // We only need to toggle to start conversion
	while (ADC12CTL1 & ADC12BUSY) ;

	unsigned n = ADC12MEM0;

	// V = (n - 0.5)*3.3/4096 * 100
	// 100 is not to use float
	// ~ n * 330 / 4096

	unsigned output = (uint16_t)(((uint32_t)n * 330) / 4096);

	return output;
}

void shutdown_adc() {
	ADC12CTL0 &= ~ADC12ENC;                 // Disable conversions
	ADC12CTL0 &= ~(ADC12ON);                // Shutdown ADC12
	REFCTL0 &= ~REFON;
}

void event_mic(unsigned param)
{
	P1OUT |= BIT5;

	mic_fft_buf* out = GET_EMPTY_FIFO(e_mic, t_fft);

	// Config the ADC on the comparator pin
	P3SEL0 |= BIT2;	
	P3SEL1 |= BIT2;	

	// Power the mic
	P3DIR |= BIT4;
	P3OUT |= BIT4;
	// Wait for a bit for mic warmup
	init_adc();

	for (unsigned i = 0; i < SAMPLES; ++i) {
	// Read mic
		out->buf[i] = read_mic();
#if MIC_EMUL == 1
		out->buf[i] = input_data[i];
#endif
		P1OUT &= ~BIT5;
		P1OUT |= BIT5;
	}

	// Power off the mic
	//P3OUT &= ~BIT4;
	shutdown_adc();

	PUSH_FIFO(e_mic, t_fft, out);
	POST(t_fft);

	P1OUT &= ~BIT5;
}

void event_transmit(unsigned param)
{
	P1DIR |= BIT2;
	P1OUT |= BIT2;

	fft_tx_buf* in = POP_FIFO(t_fft, e_tx);

	// Transmit through UART!
	// Calculate accuracy
	unsigned err_total = 0;
	for (unsigned i = 0; i < SAMPLES - ((WINDOW_SIZE - 1)); ++i) {
		int err = (golden[i] - (unsigned)(100 * in->buf[i]));
		// Check overflow
		if (err_total < UINT16_MAX - err*err) {
			err_total += err*err;
		} else {
			PRINTF("ERR overflow!\r\n");
		}
	}
	PRINTF("======= ERR: %u\r\n", err_total);

#if 0

	unsigned data[RADIO_PAYLOAD_LEN];

	for (int j = 0; j < RADIO_PAYLOAD_LEN; ++j) {
		data[j] = 0x77;
	}
	radio_pkt_t packet;

	packet.cmd = RADIO_CMD_SET_ADV_PAYLOAD;
	memcpy(packet.payload, data, RADIO_PAYLOAD_LEN);

	// 2.5 TX mode (this is done by uartlink_open_tx,
	// but doing here again here before turning on the
	// chip because I am not sure what happens when
	// CC2650 is on and this is low--Not enthusiastic enough
	// to fix uartlink_open_tx itself)
	P2SEL1 |= BIT5;

	// Turn on the CC2650
	P1DIR |= BIT2;
	P1OUT |= BIT2;
	// For some reason, waiting for a bit helps
	for (unsigned i = 0; i < 65535; ++i);

	// Sending three times redundantly
	for (unsigned i = 0; i < 3; ++i) {
		uartlink_open_tx();
		// send size + 1 (size of packet.cmd)
		uartlink_send(data, RADIO_PAYLOAD_LEN + 1);
		uartlink_close();
	}

	// Turn off CC2650 (again, for less obvious reason
	// waiting helps)
	P1OUT &= ~BIT2;
	for (unsigned i = 0; i < 6553; ++i);

	// 2.5 to GPIO mode or it will keep supplying current
	// to CC2650
	P2SEL1 &= ~BIT5;
	P2SEL0 &= ~BIT5;
	P2DIR |= BIT5;
	P2OUT &= ~BIT5;
#endif
	P1OUT &= ~BIT2;
}

void event_temp(unsigned param)
{
	P2OUT |= BIT5;
	int temp = msp_sample_temperature();
	PRINTF("%u\r\n", temp);
	P2OUT &= ~BIT5;
}

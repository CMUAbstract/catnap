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
#define SAMPLE_PERIOD 5
#define SAMPLES 300
//#define SAMPLE_PERIOD 2
//#define SAMPLES 126
#define WINDOW_SIZE 20
#define RESULT_SIZE SAMPLES / WINDOW_SIZE
__nv float filter[WINDOW_SIZE] = {
	0.05, 0.05, 0.05, 0.05, 0.05,
	0.05, 0.05, 0.05, 0.05, 0.05,
	0.05, 0.05, 0.05, 0.05, 0.05,
	0.05, 0.05, 0.05, 0.05, 0.05,
};

__ro_nv unsigned golden[RESULT_SIZE] = {
#include "input_mic_result.txt"
};

#define RADIO_PAYLOAD_LEN 4
#define MIC_EMUL 1

#if MIC_EMUL == 1
__ro_nv int input_data[SAMPLES] = {
#include "input_mic.txt"	
};
#endif

void init_hw() {}

typedef enum __attribute__((packed)) {
    RADIO_CMD_SET_ADV_PAYLOAD = 0,
} radio_cmd_t;

typedef struct __attribute__((packed)) {
    radio_cmd_t cmd;
    uint8_t payload[RADIO_PAYLOAD_LEN];
} radio_pkt_t;

// Events and tasks definition

// Period for running fft. variable
PERIOD(p2, sec_to_tick(SAMPLE_PERIOD), sec_to_tick(SAMPLE_PERIOD));

PARAM(sample_size, 1, WINDOW_SIZE);

FUNCS(f_mic, event_mic);
FUNCS(f_tx, event_transmit);
FUNCS(f_fft, task_fft);

EVENT_PERIODIC(e_mic, f_mic, p2, 1);
EVENT_APERIODIC(e_tx, f_tx, p2, 1);

TASK(t_fft, f_fft, sample_size);

// INOUT buf declaration
typedef struct {
	int buf[SAMPLES];
	uint32_t timestamp;
} mic_fft_buf;

typedef struct {
	int buf[RESULT_SIZE];
	//unsigned result;
	uint32_t timestamp;
} fft_tx_buf;

FIFO(e_mic, t_fft, mic_fft_buf);
FIFO(t_fft, e_tx, fft_tx_buf);

void down_sample(const float *buf, const float *filt, float *result, unsigned size, unsigned param)
{
	unsigned step = WINDOW_SIZE / param; // This should be an integer
	for (unsigned i = 0; i < RESULT_SIZE; ++i) {
		result[i] = 0;
		for (unsigned j = 0; j < WINDOW_SIZE; j += step) {
			result[i] += (buf[i * WINDOW_SIZE + j] * filt[j]) * step;
		}
	}
}

// Events and tasks declaration from here
void task_fft(unsigned param)
{
	P1OUT |= BIT4;
	// Pull result from e_mic
	mic_fft_buf* in = POP_FIFO(e_mic, t_fft);
	fft_tx_buf *res = GET_EMPTY_FIFO(t_fft, e_tx);

	if (param > 1) {
		float buf[SAMPLES];
		float res_buf[RESULT_SIZE];

		unsigned step = WINDOW_SIZE / param; // This should be an integer
		// Convert to float
		for (unsigned i = 0; i < SAMPLES; i += step) {
			buf[i] = ((float)(in->buf[i]) / 100);
		}

		// Downsample with LPF
		down_sample(buf, filter, res_buf, SAMPLES, param);

		// Convert back to int
		for (unsigned i = 0; i < RESULT_SIZE; ++i) {
			res->buf[i] = (int)(res_buf[i] * 100);
		}
	} else {
		// If param == 1, you don't need to do all the calc.
		// It is subsampling without the filt
		for (unsigned i = 0; i < RESULT_SIZE; ++i) {
			res->buf[i] = in->buf[i * WINDOW_SIZE];
		}

	}

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
	P3OUT &= ~BIT4;
	shutdown_adc();

	PUSH_FIFO(e_mic, t_fft, out);
	POST(t_fft);

	P1OUT &= ~BIT5;
}

void event_transmit(unsigned param)
{
	P2OUT |= BIT5;

	fft_tx_buf* in = POP_FIFO(t_fft, e_tx);

	// Transmit through UART!
	for (unsigned i = 0; i < RESULT_SIZE; ++i) {
		PRINTF("%u ", in->buf[i]);
	}
	PRINTF("\r\n");
	// Calculate accuracy
	//unsigned err_total = 0;
	//for (unsigned i = 0; i < SAMPLES - ((WINDOW_SIZE - 1)); ++i) {
	//	int err = (golden[i] - (unsigned)(100 * in->buf[i]));
	//	// Check overflow
	//	if (err_total < UINT16_MAX - err*err) {
	//		err_total += err*err;
	//	} else {
	//		PRINTF("ERR overflow!\r\n");
	//	}
	//}
	//PRINTF("======= ERR: %u\r\n", err_total);

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
	P2OUT &= ~BIT5;
}

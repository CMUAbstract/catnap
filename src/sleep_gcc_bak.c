#include <msp430.h>
#include <libos/jit.h>
#include <libos/os.h>
#include <libos/event.h>
#include <libos/task.h>
#include <libos/timer.h>
#include <libos/config.h>
#include <libos/comp.h>
#include <libos/shared.h>
#include <libmsp/mem.h>
#include <libmsp/gpio.h>
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
#include <loopcnt.h>


void task_0() {
	P1OUT |= BIT4;
	for (unsigned i = 0; i < 40000; ++i) {
		for (unsigned j = 0; j < 5; ++j) {
			P1OUT |= BIT0;
			P1OUT &= ~BIT0;
		}
	}
	P1OUT &= ~BIT4;
}

SHARED(unsigned, count_0);
SHARED(unsigned, arr, 10);
// TODO: Temp. Compiler or a python should do this
void clear_all_isWritten() {
	MANUAL_CLEAR(count_0);
	MANUAL_CLEAR(arr, 10);
}
unsigned count_1 = 0;

void task_1() {
	P1OUT |= BIT2;
	for (unsigned i = 0; i < 40000; ++i) {
		P1OUT |= BIT0;
		P1OUT &= ~BIT0;
	}
	P1OUT &= ~BIT2;
}

// Temp
Task t0 = {.task_func = &task_0};
Task t1 = {.task_func = &task_1};

void event_0() {
	P1OUT |= BIT5;
	WRITE(count_0, count_0 + 1);
	//count_0++;
	if (count_0 == 1) {
		POST(&t0);
		count_0 = 0;
	}
	for (unsigned i = 0; i < 10; ++i) {

		P1OUT |= BIT0;
		P1OUT &= ~BIT0;

		WRITE(arr[i], arr[i] + 1);
	}
	P1OUT &= ~BIT5;
}
void event_1() {
	P2OUT |= BIT5;
	count_1++;

	// super expensive event
	for (unsigned i = 0; i < 10000; ++i) {
		P1OUT |= BIT0;
		P1OUT &= ~BIT0;
	}

	//	if (count_1 == 1) {
	//		POST(&t0);
	//		count_1 = 0;
	//	}
	P2OUT &= ~BIT5;
}
//void event_2() {
//	P1OUT |= BIT4;
//	// deliberately schedule sleep
//	// TODO: We should do LPM3 sleep, but for now,
//	// for simplicity, try LPM0 (whill this work??)
//	// TODO: This is currently not working. Make it work!
//	msp_sleep(100);
//	P1OUT &= ~BIT4;
//}

__nv Event e0 = {.event_func = &event_1, .T = sec_to_tick(3),
	//	.reservedV = V_1_08};
		 .reservedE = 5*E_QUANTA};
//.reservedE = E_QUANTA};
__nv Event e1 = {.event_func = &event_0, .T = sec_to_tick(0.8),
	//	.reservedV = V_1_25};
		 .reservedE = E_QUANTA};

int main() {
	// My python inserts
	// init and restore code here

	// TODO: Temp
	event_list_it = 0;
	event_list[event_list_it++] = &e0;
	event_list[event_list_it++] = &e1;
	for (unsigned i = 0; i < event_list_it; ++i) {
		event_list[i]->nextT = event_list[i]->T;
	}

	PRINTF("event1 reservedE: %u\r\n", e1.reservedE);
	os_main();
	// temp. If not used, I think the main
	// removes the include of io.h, causing broken link in init()
	PRINTF("test");
	// same here
	//level_to_reg(1);
	return 0;
}

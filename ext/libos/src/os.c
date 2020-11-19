#include <libos/os.h>
#include <libos/event.h>
#include <libos/param.h>
#include <libos/task.h>
#include <libos/config.h>
#include <libio/console.h>
#include <libmsp/clock.h>
#include <libmsp/mem.h>
#include <libos/comp.h>
#include <libos/sleep.h>
#include <libos/measure.h>
#include <libos/jit.h>

#include <libos/timer.h>
#include <libos/scheduler.h>

// Arbitrary fixed delay
// This is to make the timer work
// more smoothly
#define DEFAULT_CR 300
#define CHARGE_TIME_THRES 100
#define V_THRES 2

__nv unsigned no_task_running = 0;

__nv unsigned v_start = 0;
__nv Event* cur_event;
__nv Event* pre_event = NULL;
__nv unsigned nextT;
__nv volatile uint8_t event_running = 0;

__nv unsigned cr_window_it = 0;
__nv unsigned cr_window_ready = 0;
__nv uint32_t cr_window[CR_WINDOW_SIZE];
__nv unsigned cr_test[CR_WINDOW_SIZE];

__nv unsigned cr_counter = 0;

__nv unsigned vdd_ok = 1;
__nv unsigned prev_vdd_ok = 1;

__nv unsigned estimated_mode_pre = 0;
__nv unsigned mode_change_cnt[MODE_NUM] = {0};

__nv unsigned v_charge_start = 0;
__nv unsigned v_charge_end = 0;
__nv unsigned t_charge_start = 0;
__nv unsigned t_charge_end = 0;

__nv unsigned charge_start_time = 0;

void change_mode(uint32_t charge_rate);

void update_comp()
{
	// TODO: This may actually dynamically
	// update lower thres for efficiency (?)

	uint32_t energy_budget = level_to_volt[lower_thres]
		* level_to_volt[lower_thres] - 10000;

	uint32_t worst_case_energy = 0;
	for (unsigned i = 0; i < event_list_all_it; ++i) {
		// For bursty, this gets a bit conservative
		Event* e = event_list_all[i];
		if (e->event_type == BURSTY) {
			worst_case_energy += e->reservedE[get_mode()] * e->burst_num;
		} else {
			worst_case_energy += e->reservedE[get_mode()];
		}
	}
	
	//PRINTF("E use: %u/%u\r\n", (unsigned)worst_case_energy, (unsigned)energy_budget);
	if (worst_case_energy > energy_budget) {
		PRINTF("Event not schedulable!\r\n");
	}
}

void calculate_energy_use(Event* e, unsigned v_before_event,
		unsigned v_after_event)
{
	uint32_t used_E;
	if (v_before_event < v_after_event) {
		used_E = 0;
	} else {
		used_E = v_before_event*v_before_event
			- v_after_event*v_after_event;
	}

	//PRINTF("vb: %u, va: %u\r\n", v_before_event, v_after_event);
	// Always remember the worst-case energy
	if (used_E > e->reservedE[get_mode()]) {
		// Update all reservedE with the same param with current
		for (unsigned i = 0; i < MODE_NUM; ++i) {
			if (!e->param) {
				// If no param, than update all reservedE
				e->reservedE[i] = used_E;
			} else {
				if (e->param->params[i] == e->param->params[get_mode()]) {
					// If the param for the mode is same as this mode
					e->reservedE[i] = used_E;
				}
			}
		}
		//PRINTF("vb: %u, va: %u\r\n", v_before_event, v_after_event);
		PRINTF("E: %u for %x\r\n", (unsigned)(used_E & 0xFFFF), (unsigned)e);

		update_comp();
	}
}

unsigned calculate_charge_rate()
{
	unsigned rate_changed = 0;
	unsigned cap_full = 0;
	uint32_t charge_rate;

	PRINTF("vs: %u, ve: %u\r\n", v_charge_start, v_charge_end);
	if (!v_charge_start) {
		goto calculate_charge_rate_cleanup;
	}

	if (t_charge_end < t_charge_start) {
		PRINTF("skip0\r\n");
		goto calculate_charge_rate_cleanup;
	}
	unsigned charge_time = t_charge_end - t_charge_start;

	// V does not go up above 1.9xV
	// Thus, it may look like it is not charging properly
	// We consider this case as "LARGE INCOMING ENERGY!"
	if (v_charge_end >= V_NEARLY_MAX) {
		//PRINTF("I think we have high power\r\n");
		charge_rate = 1000;
	} else {
	// if less than 0.05V diff, do not use the result
		if (v_charge_start + V_THRES >= v_charge_end) {
			PRINTF("charge too small\r\n");

			goto calculate_charge_rate_cleanup;
		}

		// If too short, it is incorrect
		if (charge_time < CHARGE_TIME_THRES) {
			PRINTF("time too short\r\n");
			goto calculate_charge_rate_cleanup;
		}

		// TODO: Maybe this can get optimized a lot because if v_after_start = 0, the
		// value is always constant unless the thres changes.
		// Calc charge rate
		uint32_t charged_energy = v_charge_end * v_charge_end
			- v_charge_start * v_charge_start;
		PRINTF("charged_E: %u %u\r\n", (unsigned)(charged_energy >> 16), (unsigned)(charged_energy & 0xFFFF));
		PRINTF("charged_t: %u\r\n", charge_time);

		// Amp factor to avoid charge_rate being 0
		charge_rate = AMP_FACTOR * charged_energy / charge_time;
	}

	// Push it to averaging window
	cr_window[cr_window_it] = charge_rate;
	if (cr_window_it == CR_WINDOW_SIZE - 1) {
		cr_window_ready = 1;
		cr_window_it = 0;
	} else {
		cr_window_it++;
	}

	//uint32_t avg_charge_rate = get_charge_rate_average();
	uint32_t worst_charge_rate = get_charge_rate_worst();
	PRINTF("cr: %u %u\r\n", (unsigned)(worst_charge_rate >> 16), (unsigned)(worst_charge_rate & 0xFFFF));

	// Change mode if necessary
	change_mode(worst_charge_rate);

	// Calculate schedulability
	unsigned schedulable = is_schedulable(worst_charge_rate);
	while (!schedulable) {
		rate_changed = 1;
		unsigned degradable = degrade_event();
		// not degradable..give up
		if (degradable == DEGRADE_FAIL)
			break;
		if (degradable == DEGRADE_NOT_READY)
			break;
		PRINTF("DEGRADE\r\n");
		schedulable = is_schedulable(worst_charge_rate);
	}

calculate_charge_rate_cleanup:
	v_charge_start = 0;
	v_charge_end = 0;

	return rate_changed;
}

void change_mode(uint32_t charge_rate)
{
	// TODO: This should be refined..
	unsigned mode = estimate_mode(charge_rate);
	//PRINTF("mode: %u %u\r\n", mode, get_mode());

	if (mode <= get_mode()) {
		// Clear mode change counts
		for (unsigned i = 0; i < MODE_NUM; ++i) {
			mode_change_cnt[i] = 0;
		}
		// Conservatively, lowering happens
		// immediately
		if (mode < get_mode()) {
			switch_mode(mode);
		}
		return;
	}

	// Possible mode increasing.
	// Conservatively, increasing mode
	// happens only when sure (MODE_CHANGE_CNT times
	// consecutive observation)
	if (get_mode() == MODE_NUM - 1) {
		// If already at the highest level,
		// no increasing
		return;
	}

	unsigned next_mode = MODE_NUM + 1;
	// Increment counter for cur_mode < x <= mode
	for (unsigned i = get_mode() + 1; i <= mode; ++i) {
		mode_change_cnt[i]++;
		if (mode_change_cnt[i] == MODE_CHANGE_CNT) {
			next_mode = i;
		}
	}
	// Clear counter for mode < x
	for (unsigned i = mode + 1; i < MODE_NUM; ++i) {
		mode_change_cnt[i] = 0;
	}

	// If mode is not changing
	if (next_mode == MODE_NUM + 1) {
		return;
	}

	// If mode is changing
	if (next_mode != get_mode()) {
		// Clear mode change counts
		for (unsigned i = 0; i < MODE_NUM; ++i) {
			mode_change_cnt[i] = 0;
		}
		switch_mode(next_mode);
	}
}

uint32_t get_charge_rate_average()
{
	if (!cr_window_ready) {
		return DEFAULT_CR;
	}

	uint32_t result = 0;
	for (unsigned i = 0; i < CR_WINDOW_SIZE; ++i) {
		result += (cr_window[i] / CR_WINDOW_SIZE);	
	}

	return result;
}

uint32_t get_charge_rate_worst()
{
	// Return the second-worst (heuristic)
	uint32_t worst = UINT32_MAX;
	uint32_t second_worst = UINT32_MAX;
	for (unsigned i = 0; i < CR_WINDOW_SIZE; ++i) {
		if (second_worst > cr_window[i]) {
			if (worst > cr_window[i]) {
				second_worst = worst;
				worst = cr_window[i];	
			} else {
				second_worst = cr_window[i];
			}
		}
	}
	
	//PRINTF("SW: %u\r\n", (unsigned)(second_worst & 0xFFFF));
	//PRINTF("W: %u\r\n", (unsigned)(worst & 0xFFFF));
	if (cr_window_ready) {
		return second_worst;
	}
	return worst;
}

void configure_next_wakeup()
{
	// Find the next event that should be fired
	unsigned nextT_min;
	Event* event_next = get_next_event(&nextT_min);
	PRINTF("event_next: %x\r\n", (unsigned) event_next);
	PRINTF("T: %x\r\n", (unsigned)nextT_min);

	if (!event_next) {
		return;
	}

	// Set global vars accordingly
	cur_event = event_next;
	nextT = nextT_min;

	// In case of an one-time-event, deregister it
	// Aperiodic event will never have a nextT
	// larger than UINT16_MAX (always 0), so this is fine.
	if (cur_event->event_type == APERIODIC) {
		deregister_event(cur_event);
	}

	// Configure timer
	configure_wakeup(nextT + DELAY);

	P1OUT |= BIT2;
	P1OUT &= ~BIT2;
	P2OUT |= BIT6;
	P2OUT &= ~BIT6;
	P2OUT |= BIT6;
	P2OUT &= ~BIT6;
	P2OUT |= BIT6;
	P2OUT &= ~BIT6;
	P1OUT |= BIT2;
	P1OUT &= ~BIT2;
}

void init()
{
	disable_watchdog();
	reset_gpio();
	unlock_gpio();
	init_hw();
	init_clock();
	//init_global_clock();
	__enable_interrupt();
	timer_interrupt_enable();
	INIT_CONSOLE();
	init_jit();

	P1DIR |= BIT5;
	P2DIR |= BIT5;
	P2DIR |= BIT6;
	P1DIR |= BIT4;
	P1DIR |= BIT2;
	P1DIR |= BIT0;

#ifdef LOGIC
#else
	unsigned pc;
	__asm__ volatile ("MOV &0x4000, %0":"=m"(pc));
	PRINTF(".%x.", pc);
	__asm__ volatile ("MOV &0x4008, %0":"=m"(pc));
	PRINTF(".%x.\r\n", pc);
#endif
}

void os_main()
{
	// Init event list with highest QoS
	init_events();
	// Init task list with highest QoS
	init_tasks();

	// Init period list and param list
	init_periods();
	init_params();

	// Init timer
	configure_next_wakeup();
	while (1) {
		if (taskQ_isEmpty()) {
			// sleep when no task to do
			no_task_running = 1;
			low_power_sleep();
			no_task_running = 0;
		}
		else {
			Task *t = pop_taskQ();
			param_t *p = t->param;
			unsigned param = (p == NULL? 0 : p->param);
			(*(t->task_func))(param);
		}
	}
}

void __attribute__((interrupt(TIMER0_A0_VECTOR)))timerISRHandler(void)
{
	P2OUT |= BIT6;
	P2OUT &= ~BIT6;
	P2OUT |= BIT6;
	P2OUT &= ~BIT6;

	// Timer repeatedly fires so halt on interupt
	HALT_TIMER();
	TA0CCTL0 &= ~CCIFG;
	// GIE is automatically disabled,
	// but disabling comp interrupt manually
	// because any PRINTF for debugging
	// might re-enable GIE
	CEINT &= ~CEIE;
	timer_interrupt_disable();

	// Measure voltage before event execution
	unsigned v_before_event = get_VCAP();
	unsigned v_after_event;

	unsigned rate_changed = 0;
	// Set charge end point
	// Calculate charge rate only when it was sleeping.
	// i.e., mode == SLEEPING
	// OR
	// mode == OPERATING && no task running!
	//PRINTF("mode: %u, task?: %u\r\n", mode, no_task_running);
	//if (mode == SLEEPING || no_task_running) {
	// If no task is running, we change to sleep anyways
	if (mode == SLEEPING || mode == IDLING || no_task_running) {
		//PRINTF("charge start\r\n");
		v_charge_end = v_before_event;
		t_charge_end = nextT;

		rate_changed = calculate_charge_rate();
	}

	// Manual checkpoint -- if power fails inside the event,
	// restart as if there was no event at all
	// Only checkpoint when the system is executing (not sleeping!!)
	// because when the system is sleeping, there is already a checkpoint
	// that was taken before sleep
	if (mode == OPERATING || mode == IDLING) {
		checkpoint_gpio();
		checkpoint_before_event();
	}

	// Back up nextTs in case power fails
	// during event execution (then we need to rollback time)
	backup_nextT();
	// Update all the nextTs
	adjust_nextT(nextT);
	// Update global clock
	// incr_global_clock(nextT);

	//PRINTF("run: %x\r\n", (unsigned) cur_event);
	Event* pre_event = cur_event;
	PRINTF("run?: %x %x\r\n", (unsigned)(cur_event->nextT >> 16), (unsigned)(cur_event->nextT & 0xFFFF));

	// Check if is this the time to fire an event
	if (!cur_event->nextT) {
		cur_event->nextT = cur_event->get_period();
		PRINTF("GET PERIODI: %x %x\r\n", (unsigned)((cur_event->get_period() >> 16)), (unsigned)(cur_event->get_period() & 0xFFFF));
		// Configure next wakeup
		// The event execution time is also considered
		// Note: This changes the cur_event!
		configure_next_wakeup();

		// If the corresponding event_func is null (NOP)
		// skip it -- TODO: I think this is not used anymore..
		// consider deleting it!!
		if (pre_event->event_func) {
			unsigned param;
			if (pre_event->param) {
				param = pre_event->param->param;
			} else {
				param = 0;
			}
			event_running = 1;
			// If it is bursty, count the num of execution
			pre_event->burst_cnt_shadow = pre_event->burst_cnt;
			if (pre_event->event_type == BURSTY) {
				if (pre_event->burst_cnt < pre_event->burst_num - 1) {
					pre_event->burst_cnt_shadow++;
				} else {
					pre_event->burst_cnt_shadow = 0;
				}
			}
			(*(pre_event->event_func))(param);
			event_running = 0;
			// On scuccessful event finish, clear backup
			commit_task_fifo();
			//PRINTF("commit in os\r\n");
			commit_input_fifo();
			// Commit burst cnt
			pre_event->burst_cnt = pre_event->burst_cnt_shadow;
		}
		// Measure voltage after event execution
		v_after_event = get_VCAP();

		// Again, if event_func is nop, no need to calculate,
		// not using any energy
		if (pre_event->event_func) {
			// Calculate energy used by e
			// Don't do this when you spent a lot of
			// energy on charge_rate_calculation
			if (!rate_changed) {
				calculate_energy_use(pre_event, v_before_event,
						v_after_event);
			}
		}
	} else {
		configure_next_wakeup();
		v_after_event = get_VCAP();
	}

	// The status after the event
	if (!no_task_running || !taskQ_isEmpty()) {
		// If there are task to run,
		// the system should run the task.
		if (mode == SLEEPING) {
			// 1. If it was sleeping, keep sleep

			// 1. Charging, set charge start point
			v_charge_start = v_after_event;
			t_charge_start = TA0R; // TA0R starts from here
		} else if (mode == IDLING || mode == OPERATING) {
			// Start executing only if e > e_thres
			mode = OPERATING;

			// reconfigure COMP_E
			CECTL0 = CEIMEN | CEIMSEL_13;
			SET_LOWER_COMP();
			// The voltage before the event
			// (after entering the handler) might have
			// been higher than the threshold.
			// In this case, CEIFG must have been wrongly set
			// if the event made the voltage go below
			// the thres. So we clear it and
			// manually check and set it by looking at
			// CEOUT.
			CEINT &= ~CEIFG;
			if (CECTL1 & CEOUT) {
				CEINT |= CEIFG;
			}
			low_power_wakeup_on_exit();

			if (CEINT & CEIFG) {
				// If the system will immediately enter charging on exit
				v_charge_start = v_after_event;
				t_charge_start = TA0R; // TA0R starts from here
			}
		}
	} else {
		// If there are no task to execute
		// it is charge start point
		v_charge_start = v_after_event;
		t_charge_start = TA0R; // TA0R starts from here

		if (mode == IDLING) {
			// Keep idling
		} else if (mode == SLEEPING) {
			// Keep sleeping
		} else if (mode == OPERATING) {
			// Depending on the current operating point,
			// either going to sleeping or idling
			// will help measuring energy

			// Here, we need to checkpoint!
			checkpoint_gpio();
			checkpoint_before_event();


			mode = SLEEPING;
			// reconfigure COMP_E
			CECTL0 = CEIPEN | CEIPSEL_13;
			//SET_UPPER_THRES(V_1_31);
			SET_UPPER_COMP();
			// Go to sleeping with immediately checking the
			// flag: this will automatically jump to idling if
			// needed to
			CEINT &= ~CEIFG;
			if (CECTL1 & CEOUT) {
				CEINT |= CEIFG;
			}
		}
	}

	__disable_interrupt(); // Because PRINTF enables interrupt
	// If we do not re-disable interrupts, comparator fires as soon as CEINT |= CEIE
	CEINT |= CEIE; // Re-enable checkpoint interrupt
	timer_interrupt_enable();

	P2OUT |= BIT6;
	P2OUT &= ~BIT6;
}

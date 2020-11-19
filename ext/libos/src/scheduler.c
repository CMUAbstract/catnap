#include <libos/scheduler.h>
#include <libos/period.h>
#include <libos/param.h>
#include <libos/funcs.h>
#include <libos/task.h>
#include <libos/comp.h>
#include <stddef.h>
#include <libos/os.h>
#include <libos/timer.h>
#include <libio/console.h>
#include <libos/jit.h>
#include <libos/event.h>

__nv unsigned not_degradable[MODE_NUM] = {0};

//__nv unsigned event_util_max = U_AMP_FACTOR;
__nv unsigned event_util_maxs[MODE_NUM] = {
	U_AMP_FACTOR,	
	U_AMP_FACTOR,	
	U_AMP_FACTOR,	
	U_AMP_FACTOR,	
	U_AMP_FACTOR,	
	U_AMP_FACTOR,	
	U_AMP_FACTOR,	
	U_AMP_FACTOR,	
	U_AMP_FACTOR,	
	U_AMP_FACTOR,	
};

// Degrade param for task
// This can be merged with degrade_param function,
// but I am too lazy to do that
unsigned degrade_task_param()
{
	// Pick the param which is associated with the 
	// most frequent task in the task queue (heuristic)
	// Find the task that is the most frequent is current queue.
	unsigned task_cnt[MAX_TASK_NUM] = {0};
	
	cnt_task_in_taskQ(task_cnt);

	// find max occurance
	unsigned max_i;
	unsigned max_cnt;
	while (1) {
		max_cnt = 0;
		max_i = task_list_all_it;
		for (unsigned i = 0; i < task_list_all_it; ++i) {
			if (task_cnt[i] > max_cnt) {
				max_i = i;
				max_cnt = task_cnt[i];
			}
		}

		if (max_i == task_list_all_it) {
			PRINTF("Degrade task param fail\r\n");
			return DEGRADE_FAIL;
		}

		// Try degrading
		Task *t = task_list_all[max_i];
		param_t *p = t->param;
		if (p == NULL) {
			// This task has no param to degrade
			task_cnt[max_i] = 0;
		} else {
			unsigned old_param = p->param;
			unsigned res = p->degrade_param();
			if (res == PARAM_FAIL) {
				// This param is already maximally degraded
				PRINTF("Cannot degrade further\r\n");
				task_cnt[max_i] = 0;
			} else {
				// Degrade success!
				PRINTF("Degrade param: %x, %u\r\n", (unsigned)p, p->param);
				// RESET reservedE for the corresponding events
				for (unsigned i = 0; i < event_list_all_it; ++i) {
					Event* e = event_list_all[i];
					if (p == e->param) {
						e->reservedE[get_mode()] = 0;
					}
				}
				// Save this config
				save_param_config(p);

				return DEGRADE_SUCCESS;
			}
		}
	}
}

unsigned degrade_task()
{
	// Find the task that is the most frequent is current queue.
	unsigned task_cnt[MAX_TASK_NUM] = {0};
	
	cnt_task_in_taskQ(task_cnt);

	// find max occurance
	unsigned max_i;
	unsigned max_cnt;
	while (1) {
		max_cnt = 0;
		max_i = task_list_all_it;
		for (unsigned i = 0; i < task_list_all_it; ++i) {
			if (task_cnt[i] > max_cnt) {
				max_i = i;
				max_cnt = task_cnt[i];
			}
		}

		if (max_i == task_list_all_it) {
			PRINTF("Degrade task fail\r\n");
			return DEGRADE_FAIL;
		}

		// Try degrading
		Task *t = task_list_all[max_i];
		funcs_t *f = t->funcs;
		if (f->func_of_mode[get_mode()] == f->funcs_num - 1) {
			// If func for the current mode is already
			// in its lowest quality
			task_cnt[max_i] = 0;
		} else {
			// Degrade success!
			f->func_of_mode[get_mode()]++;
			PRINTF("Degrade task: %x, %u\r\n", (unsigned)t, f->func_of_mode[get_mode()]);
			unsigned new_lv = f->func_of_mode[get_mode()];
			t->task_func = f->funcs[new_lv];

			// Do not reset param because it will
			// mess up the event schedulability!

			// Save event config & param config already done

			return DEGRADE_SUCCESS;
		}
	}
}

unsigned decrease_slack()
{
	if (event_util_maxs[get_mode()] - UTIL_STEP < EVENT_UTIL_MIN) {
		return DEGRADE_FAIL;
	}

	// TODO: This needs to be idempotent because
	// it is getting called by an event (depends on how
	// we design the behavior on power failure)
	event_util_maxs[get_mode()] -= UTIL_STEP;
	PRINTF("U_MAX decreased: %u\r\n", event_util_maxs[get_mode()]);

	return DEGRADE_SUCCESS;
}

// Degrade event code
// This needs e->charge_time, which
// gets calculated by calculate_C method.
// Only call it after the function is
// called at least once (i.e., after is_schedulable)
// It does not need immediate R value
// because anyway every C is linear to R, which
// cancels its effect
unsigned degrade_event_code()
{
	// Pick the event with the
	// maximum utilization
	// This is a heuristic

	uint32_t Us[MAX_EVENT_NUM] = {0};

	for (unsigned i = 0; i < event_list_all_it; ++i) {
		Event* e = event_list_all[i];

		uint32_t tmp = e->charge_time;
		if (e->event_type == BURSTY) {
			tmp *= e->burst_num;
		}
		tmp *= U_AMP_FACTOR; // Amplify
		tmp /= (uint32_t)e->get_period();
		Us[i] = tmp + 1;
	}

	uint32_t U_max;
	unsigned max_i;
	while (1) {
		U_max = 0;
		max_i = event_list_all_it;
		for (unsigned i = 0; i < event_list_all_it; ++i) {
			if (Us[i] > U_max) {
				max_i = i;
				U_max = Us[i];
			}
		}

		if (max_i == event_list_all_it) {
			PRINTF("Degrade event fail\r\n");
			return DEGRADE_FAIL;
		}

		// Try degrading
		Event *e = event_list_all[max_i];
		funcs_t *f = e->funcs;
		if (f->func_of_mode[get_mode()] == f->funcs_num - 1) {
			// If func for the current mode is already
			// in its lowest quality
			Us[max_i] = 0;
		} else {
			// Degrade success!
			f->func_of_mode[get_mode()]++;
			PRINTF("Degrade event: %x, %u\r\n", (unsigned)e, f->func_of_mode[get_mode()]);
			unsigned new_lv = f->func_of_mode[get_mode()];
			e->event_func = f->funcs[new_lv];

			// RESET reservedE for the corresponding events
			// This will temporarily make it look like the
			// system is schedulable (because the corresponding E
			// is cleared)
			// However, if the E decrease is not sufficient,
			// It might re-enter here..
			e->reservedE[get_mode()] = 0;
			// Also, reset param to highest
			e->param->params[get_mode()] = e->param->param_ub;
			e->param->param = e->param->param_ub;

			// Save event config & param config already done

			return DEGRADE_SUCCESS;
		}
	}
}


// Degrade param
// because anyway every C is linear to R, which
// cancels its effect
unsigned degrade_param()
{
	// Pick the param which is associated with the 
	// maximum utilization

	uint32_t Us[MAX_PARAM_NUM] = {0};

	for (unsigned i = 0; i < event_list_all_it; ++i) {
		Event* e = event_list_all[i];
		unsigned j = 0;
		for (;j < param_list_it; ++j) {
			if (param_list[j] == e->param)
				break;
		}

		uint32_t tmp = e->charge_time;
		if (e->event_type == BURSTY) {
			tmp *= e->burst_num;
		}
		tmp *= U_AMP_FACTOR; // Amplify
		tmp /= (uint32_t)e->get_period();
		Us[j] += tmp + 1; // To prohibit it from being 0
	}

	uint32_t U_max;
	unsigned max_i;
	while (1) {
		U_max = 0;
		max_i = param_list_it;
		for (unsigned i = 0; i < param_list_it; ++i) {
			if (Us[i] > U_max) {
				max_i = i;
				U_max = Us[i];
			}
		}

		if (max_i == param_list_it) {
			PRINTF("Degrade param fail\r\n");
			return DEGRADE_FAIL;
		}
		PRINTF("Try degrading %u\r\n", max_i);

		// Try degrading
		param_t* p = param_list[max_i];
		unsigned old_param = p->param;
		unsigned res = p->degrade_param();
		if (res == PARAM_FAIL) {
			Us[max_i] = 0;
		} else {
			// Degrade success!
			PRINTF("Degrade param: %x, %u\r\n", (unsigned)p, p->param);
			// RESET reservedE for the corresponding events
			// This will temporarily make it look like the
			// system is schedulable (because the corresponding E
			// is cleared)
			// However, if the E decrease is not sufficient,
			// It might re-enter here..
			for (unsigned i = 0; i < event_list_all_it; ++i) {
				Event* e = event_list_all[i];
				// I don't remember what I was doing with this..
				//unsigned j = 0;
				//for (;j < param_list_it; ++j) {
				//	if (param_list[j] == e->param) {
				//		e->reservedE[get_mode()] = 0;
				//	}
				//}
				if (p == e->param) {
					e->reservedE[get_mode()] = 0;
				}
			}

			// Save this config
			save_param_config(p);

			return DEGRADE_SUCCESS;
		}
	}
}

// Degrade period
// This needs e->charge_time, which
// gets calculated by calculate_C method.
// Only call it after the function is
// called at least once (i.e., after is_schedulable)
// It does not need immediate R value
// because anyway every C is linear to R, which
// cancels its effect
unsigned degrade_T()
{
	// Pick the T by decreasing which
	// gives the maximum utilization decrease

	// Degrading does not happen a lot, so lets
	// use float without guilt.
	float gradients[MAX_PERIOD_NUM] = {0};

	for (unsigned i = 0; i < event_list_all_it; ++i) {
		Event* e = event_list_all[i];
		unsigned j = 0;
		for (;j < period_list_it; ++j) {
			if (period_list[j] == e->base_period)
				break;
		}
		// Calculate C * scalar / (T)^2
		T_t T = e->get_period();
		if (e->event_type == BURSTY) {
			gradients[j] += (float)(e->scalar * e->charge_time) * e->burst_num
				/ (float)((T) * (T));
		} else {
			gradients[j] += (float)(e->scalar * e->charge_time)
				/ (float)((T) * (T));
		}
		//PRINTF("E: %x, C: %u, T: %u\r\n", (unsigned)(e), (unsigned)(e->charge_time), T);
	}

	float gradient_max;
	unsigned max_i;
	while (1) {
		gradient_max = 0;
		max_i = period_list_it;
		for (unsigned i = 0; i < period_list_it; ++i) {
			if (gradients[i] > gradient_max) {
				max_i = i;
				gradient_max = gradients[i];
			}
		}

		if (max_i == period_list_it) {
			PRINTF("Degrade T fail\r\n");
			return DEGRADE_FAIL;
		}

		// Try degrading
		period_t* p = period_list[max_i];
		T_t old_T = p->T;
		unsigned res = p->degrade_T();
		if (res == P_FAIL) {
			gradients[max_i] = 0;
		} else {
			// Degrade success!
			PRINTF("Degrade T: %x, %u\r\n", (unsigned)p, p->T);
			// Save this config
			save_period_config(p);
			return DEGRADE_SUCCESS;
		}
	}
}

// Degrade events when events are not schedulable
unsigned degrade_event()
{
	// Only degrade when cr is ready
	// Otherwise, wait.. we might find the
	// right Ts for the current cr
	//if (!cr_window_ready) {
	//	return DEGRADE_SUCCESS;
	//}

	// 0. Check if degrading might be possible in this level first.
	// If not, no use trying
	if (not_degradable[get_mode()]) {
		PRINTF("Not degradable!\r\n");
		return DEGRADE_FAIL;
	}


	// 1. Try increasing T for events
	if (degrade_T() == DEGRADE_SUCCESS) {
		return DEGRADE_SUCCESS;
	}

	// 2. Try decreasing param for events
	if (degrade_param() == DEGRADE_SUCCESS) {
		return DEGRADE_SUCCESS;
	}

	// 3. Try degrading events
	if (degrade_event_code() == DEGRADE_SUCCESS) {
		return DEGRADE_SUCCESS;
	}

	// If not degradable, wait for a bit
	// until the cr_window is filled.
	// If not degradable even then,
	// it is not degradable
	if (cr_window_ready) {
		not_degradable[get_mode()] = 1;
	}
	return DEGRADE_FAIL;
}


// Calculate the utilization
// and see if the events are
// schedulable
unsigned is_schedulable(uint32_t charge_rate)
{
	// Calculate charge time
	calculate_C(charge_rate);

	uint32_t U = 0; // Utilization
	for (unsigned i = 0; i < event_list_all_it; ++i) {
		uint32_t tmp;
		Event* event = event_list_all[i];
		if (event->event_type == BURSTY) {
			tmp = event->charge_time * event->burst_num;
		} else {
			tmp = event->charge_time;
		}
		tmp *= U_AMP_FACTOR; // Amplify
		tmp /= (uint32_t)event->get_period();
		//PRINTF("charge time: %u T: %u\r\n", (unsigned)event->charge_time, event->get_period());
		U += tmp;
	}
	PRINTF("U: %x %x\r\n", (unsigned)(U >> 16), (unsigned)(U & 0xFFFF));
	if (U <= event_util_maxs[get_mode()]) {
		return 1;
	} else {
		return 0;
	}
}

// Calculate C
void calculate_C(uint32_t charge_rate)
{
	for (unsigned i = 0; i < event_list_all_it; ++i) {
		Event* event = event_list_all[i];
		event->charge_time = (event->reservedE[get_mode()]*AMP_FACTOR / charge_rate);
		event->charge_time += 1; // Just so that it is never 0 (for degrading)
	}
}

uint8_t schedule_one_time_event(Event* e)
{
	//if (!event_active(e)) {
	//	return POST_INVALID;
	//}
	// Just push an event with a very small (zero?)
	// nextT to the list

	// Scheduler should re-run to reschedule this
	// prior to next wakeup point!
	// But will it be okay? what if the events gets
	// invoked in the middle?
	// To avoid that, this is heartbreaking,
	// but stop the timer here..

	unsigned elapsed_time = TA0R;
	HALT_TIMER();
	// update the nextTs to current time
	adjust_nextT(elapsed_time);
	// nextT for e to a very small value
	// Or even 0?
	e->nextT = 0;
	event_list[event_list_it++] = e;

	if (e->event_type == APERIODIC) {
		e->mask_timer = e->get_period();
	}

	// Don't let the timer fire immediately during configuration
	if (!event_running) {
		timer_interrupt_disable();
	}
	configure_next_wakeup();
	// One-time event will fire here
	if (!event_running) {
		timer_interrupt_enable();
	}

	return POST_SUCCESS;
}

#include <msp430.h>
#include <libos/task.h>
#include <stddef.h>
#include <libio/console.h>
#include <libos/os.h>
#include <libos/scheduler.h>
#include <libmsp/mem.h>

__nv unsigned task_it_tail_shadow = 0;
__nv unsigned task_it_tail = 0;
__nv unsigned task_it_head = 0;
__nv Task* task_Q[MAX_TASK_NUM];
__nv Task* task_list_all[MAX_TASK_NUM];
__nv unsigned task_list_all_it = 0;

#define TASK_Q_CHECK_INTERVAL 10
__nv unsigned post_cnt = 0;

// Return the cnt of each task inside the queue
// using the cnt array
void cnt_task_in_taskQ(unsigned* task_cnt)
{
	unsigned i = task_it_head;
	while (1) {
		Task *t = task_Q[i];
		for (unsigned j = 0; j < task_list_all_it; ++j) {
			if (task_list_all[j] == t) {
				// Count occurance
				task_cnt[j]++;
				break;
			}
		}
		i++;
		if (i == MAX_TASK_NUM) {
			i = 0;
		}
		if (i == task_it_tail) {
			break;
		}
	}
}


uint8_t push_taskQ(Task* t) {
	unsigned task_it_tail_next;
	if (task_it_tail == MAX_TASK_NUM - 1) {
		task_it_tail_next = 0;
	}
	else {
		task_it_tail_next = task_it_tail_shadow + 1;
	}
	
	// Check if the queue is full
	if (task_it_tail_next == task_it_head) {
		PRINTF("Queue full! %u\r\n", post_cnt);
		// On queue full, degrade if post_cnt == 0
		if (!post_cnt) {
			// First, try degrading param
			unsigned res = degrade_task_param();

			// Second, try degrading task
			if (res == DEGRADE_FAIL) {
				res = degrade_task();
			}
			// Then, if it did not work, try decreasing slack
			// As you can see, the order is arbitrary
			if (res == DEGRADE_FAIL) {
				res = decrease_slack();
			}
			if (res == DEGRADE_FAIL) {
				PRINTF("Task is getting lost!\r\n");
			}
		}

		// Increase post_cnt to TASK_Q_CHECK_INTERVAL,
		// in order to wait for a certain number of task push
		// and see if the system is resolving itself
		// before degrading again
		post_cnt++;
		if (post_cnt == TASK_Q_CHECK_INTERVAL) {
			post_cnt = 0;
		}

		return POST_FULL;
	}
	else {
		post_cnt = 0;

		task_Q[task_it_tail_shadow] = t;
		task_it_tail_shadow = task_it_tail_next;
		return POST_SUCCESS;
	}
}

// This is queue, so pop from the beginning
Task* pop_taskQ() {
	if (task_it_tail == task_it_head) {
		PRINTF("Task empty!");
		return NULL;
	}
	else {
		Task* t = task_Q[task_it_head];
		if (task_it_head == MAX_TASK_NUM - 1) {
			task_it_head = 0;
		}
		else {
			task_it_head++;
		}
		return t;
	}
}

void init_tasks()
{
	for (unsigned i = 0; i < task_list_all_it; ++i) {
		Task *t = task_list_all[i];
		// Set initially to highest quality
		funcs_t *f = t->funcs;
		t->task_func = f->funcs[0];

		// For all mode, initialize with
		// highest quality
		for (unsigned j = 0; j < MODE_NUM; ++j) {
			f->func_of_mode[j] = 0;
		}
	}
}

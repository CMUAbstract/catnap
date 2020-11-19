#ifndef __TASK__
#define __TASK__
#include <libos/fifo.h>
#include <libos/param.h>
#include <libos/funcs.h>
#include <libos/global.h>

#define MAX_TASK_NUM 3

typedef struct {
	void (*task_func)(unsigned p);
	funcs_t* funcs;
	param_t* param; // Only one param for now
}	Task;

extern Task* task_Q[MAX_TASK_NUM];
extern unsigned task_it_tail;
extern unsigned task_it_head;
extern unsigned task_it_tail_shadow;

uint8_t push_taskQ(Task* t);
Task* pop_taskQ();
unsigned task_active(Task* t);
void cnt_task_in_taskQ(unsigned* task_cnt);

#define TASK(name, funcs, ...) \
	TASK_(name, funcs, ##__VA_ARGS__, 2, 1)

#define TASK_(name, funcs, _param, n, ...)\
	TASK ## n(name, funcs, _param)

#define TASK1(name, _funcs, ...) \
	__nv Task name = {.funcs = &_funcs, .param = NULL};

#define TASK2(name, _funcs, _param) \
	__nv Task name = {.funcs = &_funcs, .param = &_param};

#define taskQ_isEmpty() task_it_tail == task_it_head

#define commit_task_fifo() \
	task_it_tail = task_it_tail_shadow;

#define undo_task_fifo() \
	task_it_tail_shadow = task_it_tail;

extern Task* task_list_all[MAX_TASK_NUM];
extern unsigned task_list_all_it;
void init_tasks();

#endif

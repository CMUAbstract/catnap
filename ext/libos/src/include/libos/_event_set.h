#ifndef __EVENT_SET__
#define __EVENT_SET__

#include <libos/event.h>
#include <libos/task.h>
#include <libos/mode.h>
#include <libmsp/mem.h>

#define MAX_EVENTS_IN_SET 3
#define MAX_EVENT_SET_LEVEL 3
#define MAX_EVENT_SET_NUM 3

typedef struct {
	Event* events[MAX_EVENT_SET_LEVEL][MAX_EVENTS_IN_SET];
	Task* tasks[MAX_EVENT_SET_LEVEL][MAX_EVENTS_IN_SET];
	unsigned level_it;
	unsigned events_it[MAX_EVENT_SET_LEVEL];
	unsigned tasks_it[MAX_EVENT_SET_LEVEL];
	unsigned mode_level_table[MODE_NUM];
} Event_set;

extern Event_set* event_sets[MAX_EVENT_SET_NUM];
extern unsigned event_sets_it;
extern unsigned event_list_updating_start;

void init_event_sets();
void set_event_list();
void load_event_set_config(unsigned mode);

unsigned is_in_event_set(Event* e, Event_set* set);

#endif

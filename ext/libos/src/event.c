#include <libos/os.h>
#include <libos/event.h>
#include <libio/console.h>
#include <libmsp/mem.h>
#include <libos/timer.h>

__nv Event* event_list_all[MAX_EVENT_NUM];
__nv unsigned event_list_all_it = 0;

__nv Event* event_list[MAX_EVENT_NUM];
__nv unsigned event_list_it = 0;

//__nv Event* event_list_set[MAX_EVENT_SET_NUM][MAX_EVENT_NUM];
//__nv Event* event_set_all[MAX_EVENT_SET_NUM][MAX_EVENT_NUM];
//__nv unsigned event_set_set_it = 0;
//__nv unsigned event_set_event_it[MAX_EVENT_SET_NUM] = {0};
//__nv unsigned cur_event_set = 0;
//__nv unsigned pre_event_set = 0;
// Memorize which event sets were appropriate
// for the specific mode
//__nv unsigned event_sets[MODE_NUM] = {0};

void deregister_event(Event* e)
{
	// Search for the index of e in event_list
	int it = -1;
	for (int i = 0; i < event_list_it; ++i) {
		if (event_list[i] == e) {
			it = i;
			break;
		}
	}

	if (it == -1) {
		//PRINTF("Deregistering non-existing event!\r\n");
		return;
	}

	// We do not need to keep the order of the array,
	// so we do smart pop that is O(1)
	event_list[it] = event_list[event_list_it - 1];
	event_list_it--;
}

void init_events()
{
	for (unsigned i = 0; i < event_list_all_it; ++i) {
		Event *e = event_list_all[i];
		// Set initially to highest quality
		funcs_t *f = e->funcs;
		e->event_func = f->funcs[0];

		// For all mode, initialize with
		// highest quality
		for (unsigned j = 0; j < MODE_NUM; ++j) {
			f->func_of_mode[j] = 0;
		}

		// If this is a periodic event,
		// init nextT correctly
		// To start immediately, make it 0
		if (e->event_type == PERIODIC
				|| e->event_type == BURSTY) {
			e->nextT = 0;
		}
	}
}

void load_event_config(unsigned mode) {
	for (unsigned i = 0; i < event_list_all_it; ++i) {
		Event *e = event_list_all[i];
		funcs_t *f = e->funcs;
		unsigned lvl = f->func_of_mode[mode];
		e->event_func = f->funcs[lvl];
	}
}

// Return next event through return and nextT
// through arg
Event* get_next_event(T_t *nextT)
{
	// Find the next event that should be fired
	T_t nextT_min = UINT32_MAX;
	Event* event_next = NULL;

	for (unsigned i = 0; i < event_list_it; ++i) {
		Event* event = event_list[i];
		//PRINTF("e: %x\r\n", (unsigned) event);
		// EDF
		//PRINTF("e: %x T: %u\r\n", (unsigned)event, event->nextT);
		if (nextT_min > event->nextT 
				|| ((nextT_min == event->nextT)
					&& (event_next->get_period() > event->get_period()))) {
			// event_next is not null if nextT_min == event->nextT
			nextT_min = event->nextT;
			event_next = event;
		}
	}

	if (nextT_min > UINT16_MAX) {
		*nextT = UINT16_MAX - DELAY;
	} else {
		*nextT = nextT_min;
	}
	return event_next;
}

inline void adjust_nextT(T_t time)
{
	// We do this for all the events (not only the active ones)
	// to rate-limit aperiodic events efficiently.
	// It is not common to disable periodic events, so
	// this shouldn't be too much overhead.
	// Also, calculating nextT for disabled event is still
	// correct because nextT gets cleared when scheduled (POST_EVENT)
	for (unsigned i = 0; i < event_list_all_it; ++i) {
		if (event_list[i]->nextT > time) {
			event_list[i]->nextT -= time;
		}
		else {
			event_list[i]->nextT = 0;
		}

		// For rate limiting
		if (event_list[i]->event_type == APERIODIC) {
			if (event_list[i]->mask_timer > time) {
				event_list[i]->mask_timer -= time;
			}
			else {
				event_list[i]->mask_timer = 0;
			}
		}
	}
}

inline void backup_nextT()
{
	for (unsigned i = 0; i < event_list_it; ++i) {
		event_list[i]->shadow_nextT = event_list[i]->nextT;
	}
}

inline void rollback_nextT()
{
	for (unsigned i = 0; i < event_list_it; ++i) {
		event_list[i]->nextT = event_list[i]->shadow_nextT;
	}
}

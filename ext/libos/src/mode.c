#include <libos/mode.h>
#include <libos/period.h>
#include <libos/event.h>
#include <libmsp/mem.h>
#include <libio/console.h>

// -1 because the last one does not have an
// upper bound
__nv unsigned mode_thres[MODE_NUM - 1] = {
	250,
	500,
	MODE_THRES_HIGH,
};

__nv unsigned pre_mode = MODE_NUM + 1;
__nv unsigned cur_mode = 0;

unsigned estimate_mode(unsigned cr) {
	unsigned mode = 0;
	// Select mode
	for (unsigned i = 0; i < MODE_NUM; ++i) {
		mode = i;
		if (i < MODE_NUM - 1) {
			if (cr < mode_thres[i]) {
				return mode;
			}
		}
	}
	return MODE_NUM - 1;
}

void switch_mode(unsigned mode) {
	// Load event configs
	cur_mode = mode;
	if (pre_mode != cur_mode) {
		PRINTF("Switch to mode %u\r\n", cur_mode);
		load_period_config(cur_mode);
		load_param_config(cur_mode);
		load_event_config(cur_mode);
	}
	pre_mode = cur_mode;
}

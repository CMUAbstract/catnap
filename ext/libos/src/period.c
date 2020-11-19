#include <libos/period.h>
#include <libmsp/mem.h>
#include <libio/console.h>

__nv period_t* period_list[MAX_PERIOD_NUM];
__nv unsigned period_list_it = 0;

void load_period_config(unsigned mode)
{
	for (unsigned i = 0; i < period_list_it; ++i) {
		period_t *p = period_list[i];
		p->T = p->Ts[mode];
	}
}

void init_periods()
{
	// Init Ts as small as possible
	for (unsigned i = 0; i < period_list_it; ++i) {
		for (unsigned j = 0; j < MODE_NUM; ++j) {
			PRINTF("INIT PERIOD: %x %x\r\n", (unsigned)((period_list[i]->T_lb >> 16)), (unsigned)(period_list[i]->T_lb & 0xFFFF));
			period_list[i]->Ts[j] = period_list[i]->T_lb;
		}
	}
}

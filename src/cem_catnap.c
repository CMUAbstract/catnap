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

// Events and tasks definition

PERIOD(p1, sec_to_tick(10), sec_to_tick(10));
FUNCS(f_starter, event_starter);
EVENT_PERIODIC(e_starter, f_starter, p1, 1);

FUNCS(f_main, task_main);

TASK(t_main, f_main);

#define LOOP_CEM 200

void init_hw(){}


void event_starter(unsigned param)
{
	P1DIR |= BIT5;
	P1OUT |= BIT5;
	P1OUT &= ~BIT5;
	POST(t_main);
	DISABLE_EVENT(e_starter);
}




/*************** CEM CODE ****************/
#define NIL 0 // like NULL, but for indexes, not real pointers

#define DICT_SIZE         512
#define BLOCK_SIZE         64

#define NUM_LETTERS_IN_SAMPLE        2
#define LETTER_MASK             0x00FF
#define LETTER_SIZE_BITS             8
#define NUM_LETTERS (LETTER_MASK + 1)
typedef unsigned index_t;
typedef unsigned letter_t;
typedef unsigned sample_t;
// We need to mask capy interrupt before doing
// mandatory checkpoint. This is because this checkpoint SHOULD
// always succeed. There is no protection if this fails
// (even with double buffering, because the control
// flow should never go back)
// Worst case, the checkpoint will finish (hopefully, because
// if interrupt fired in here, we know we had more than threshold
// voltage when entering the checkpoint -- this can be safely
// assumed by tuning the threshold)
// but we will enter cold start.
//#define PROTECT_START() \
//	COMP_VBANK(INT) &= ~COMP_VBANK(IE);\
//	chkpt_mask = 1;\
//	checkpoint();\
//	COMP_VBANK(INT) |= COMP_VBANK(IE);
//
//
//#define PROTECT_END() \
//	chkpt_mask = 0;

#define LOG(...)

// NOTE: can't use pointers, since need to ChSync, etc
typedef struct _node_t {
	letter_t letter; // 'letter' of the alphabet
	index_t sibling; // this node is a member of the parent's children list
	index_t child;   // link-list of children
} node_t;

typedef struct _dict_t {
	node_t nodes[DICT_SIZE];
	unsigned node_count;
} dict_t;

typedef struct _log_t {
	index_t data[BLOCK_SIZE];
	unsigned count;
	unsigned sample_count;
} log_t;
void print_log(log_t *log)
{
	PRINTF("%u/%u\r\n",
			log->sample_count, log->count);
	//unsigned i;
	//BLOCK_PRINTF_BEGIN();
	//BLOCK_PRINTF("rate: samples/block: %u/%u\r\n",
	//		log->sample_count, log->count);
	//BLOCK_PRINTF("compressed block:\r\n");
	//for (i = 0; i < log->count; ++i) {
	//	BLOCK_PRINTF("%04x ", log->data[i]);
	//	if (i > 0 && ((i + 1) & (8 - 1)) == 0){
	//	}
	//	BLOCK_PRINTF("\r\n");
	//}
	//if ((log->count & (8 - 1)) != 0){
	//}
	//BLOCK_PRINTF("\r\n");
	//BLOCK_PRINTF_END();
}

static sample_t acquire_sample(letter_t prev_sample)
{
	//letter_t sample = rand() & 0x0F;
	letter_t sample = (prev_sample + 1) & 0x03;
	return sample;
}

void init_dict(dict_t *dict)
{
	letter_t l;

	LOG("init dict\r\n");
	dict->node_count = 0;

	for (l = 0; l < NUM_LETTERS; ++l) {
		node_t *node = &dict->nodes[l];
		node->letter = l;
		node->sibling = 0;
		node->child = 0;

		dict->node_count++;
		LOG("init dict: node count %u\r\n", dict->node_count);
	}
}

index_t find_child(letter_t letter, index_t parent, dict_t *dict)
{
	node_t *parent_node = &dict->nodes[parent];

	LOG("find child: l %u p %u c %u\r\n", letter, parent, parent_node->child);

	if (parent_node->child == NIL) {
		LOG("find child: not found (no children)\r\n");
		return NIL;
	}

	index_t sibling = parent_node->child;
	while (sibling != NIL) {
		node_t *sibling_node = &dict->nodes[sibling];

		LOG("find child: l %u, s %u l %u s %u\r\n", letter,
				sibling, sibling_node->letter, sibling_node->sibling);

		if (sibling_node->letter == letter) { // found
			LOG("find child: found %u\r\n", sibling);
			return sibling;
		} else {
			sibling = sibling_node->sibling;
		}
	}

	LOG("find child: not found (no match)\r\n");
	return NIL; 
}

void add_node(letter_t letter, index_t parent, dict_t *dict)
{
	if (dict->node_count == DICT_SIZE) {
		PRINTF("add node: table full\r\n");
		while(1); // bail for now
	}
	// Initialize the new node
	node_t *node = &dict->nodes[dict->node_count];

	node->letter = letter;
	node->sibling = NIL;
	node->child = NIL;

	index_t node_index = dict->node_count++;

	index_t child = dict->nodes[parent].child;

	LOG("add node: i %u l %u, p: %u pc %u\r\n",
			node_index, letter, parent, child);

	if (child) {
		LOG("add node: is sibling\r\n");

		// Find the last sibling in list
		index_t sibling = child;
		node_t *sibling_node = &dict->nodes[sibling];
		while (sibling_node->sibling != NIL) {
			LOG("add node: sibling %u, l %u s %u\r\n",
					sibling, letter, sibling_node->sibling);
			sibling = sibling_node->sibling;
			sibling_node = &dict->nodes[sibling];
		}
		// Link-in the new node
		LOG("add node: last sibling %u\r\n", sibling);
		dict->nodes[sibling].sibling = node_index;
	} else {
		LOG("add node: is only child\r\n");
		dict->nodes[parent].child = node_index;
	}
}

void append_compressed(index_t parent, log_t *log)
{
	LOG("append comp: p %u cnt %u\r\n", parent, log->count);
	log->data[log->count++] = parent;
}

__nv unsigned debug_cntr = 0;

void task_main(unsigned param)
{
	static volatile __nv dict_t dict;
	static volatile __nv log_t log;
	//	test_func();
	while (1) {
		P2DIR |= BIT5;
		P2OUT |= BIT5;
		P2OUT &= ~BIT5;
		for (unsigned ii = 0; ii < LOOP_CEM; ++ii) {
			log.count = 0;
			log.sample_count = 0;
			init_dict(&dict);

			// Initialize the pointer into the dictionary to one of the root nodes
			// Assume all streams start with a fixed prefix ('0'), to avoid having
			// to letterize this out-of-band sample.
			letter_t letter = 0;

			unsigned letter_idx = 0;
			index_t parent, child;
			sample_t sample, prev_sample = 0;

			log.sample_count = 1; // count the initial sample (see above)
			log.count = 0; // init compressed counter

			while (1) {
				child = (index_t)letter; // relyes on initialization of dict
				LOG("compress: parent %u\r\n", child); // naming is odd due to loop

				if (letter_idx == 0) {
					sample = acquire_sample(prev_sample);
					prev_sample = sample;
				}

				LOG("letter index: %u\r\n", letter_idx);
				letter_idx++;
				if (letter_idx == NUM_LETTERS_IN_SAMPLE)
					letter_idx = 0;
				do {
					unsigned letter_idx_tmp = (letter_idx == 0) ? NUM_LETTERS_IN_SAMPLE : letter_idx - 1; 

					unsigned letter_shift = LETTER_SIZE_BITS * letter_idx_tmp;
					letter = (sample & (LETTER_MASK << letter_shift)) >> letter_shift;
					LOG("letterize: sample %x letter %x (%u)\r\n",
							sample, letter, letter);

					log.sample_count++;
					LOG("sample count: %u\r\n", log.sample_count);
					parent = child;
					child = find_child(letter, parent, &dict);
				} while (child != NIL);

				append_compressed(parent, &log);
				add_node(letter, parent, &dict);

				if (log.count == BLOCK_SIZE) {
					break;
				}
			}
		}
#ifndef LOGIC
		//PROTECT_START();
		print_log(&log);
		//PROTECT_END();
#endif
#ifdef LOGIC
		//PROTECT_START();
	//BLOCK_PRINTF_BEGIN();
	//BLOCK_PRINTF("rate: samples/block: %u/%u\r\n",
	//		log->sample_count, log->count);
	//BLOCK_PRINTF("compressed block:\r\n");
	//for (i = 0; i < log->count; ++i) {
	//	BLOCK_PRINTF("%04x ", log->data[i]);
	//	if (i > 0 && ((i + 1) & (8 - 1)) == 0){
	//	}
	//	BLOCK_PRINTF("\r\n");
	//}
	//if ((log->count & (8 - 1)) != 0){
	//}
		//GPIO(PORT_DBG, OUT) |= BIT(PIN_AUX_2);
		//GPIO(PORT_DBG, OUT) &= ~BIT(PIN_AUX_2);
		//PROTECT_END();
#endif
		debug_cntr++;
	}
	return 0;
}

#ifndef __FIFO__
#define __FIFO__

#include <libio/console.h>
#include <libos/global.h>

#define FIFO_SIZE 3
#define FULL_CNT 5 // If full, wait until this counter and degrade

#define FIFO(src, dst, struc)\
	__nv struc src ## _ ## dst ## _fifo[FIFO_SIZE]; \
	__nv unsigned src ## _ ## dst ## _it_tail_next = 0;\
	__nv unsigned src ## _ ## dst ## _it_tail = 0;\
	__nv unsigned src ## _ ## dst ## _it_head = 0;\
	__nv unsigned src ## _ ## dst ## _it_head_shadow = 0;\
	__nv unsigned src ## _ ## dst ## _it_tail_shadow = 0;\
	__nv unsigned src ## _ ## dst ## _full_cnt = 0;\

// Get an instance to fill
#define GET_EMPTY_FIFO(src, dst) \
	&src ## _ ## dst ## _fifo[src ## _ ## dst ## _it_tail_shadow];\

#define POP_FIFO(src, dst) \
	&src ## _ ## dst ## _fifo[src ## _ ## dst ## _it_head_shadow];\
	if (src ## _ ## dst ## _it_head_shadow == FIFO_SIZE - 1) {\
		src ## _ ## dst ## _it_head_shadow = 0;\
	} else {\
		src ## _ ## dst ## _it_head_shadow++;\
	}\
	//PRINTF("POP: %u %u\r\n", src ## _ ## dst ## _it_head_shadow, src ## _ ## dst ## _it_tail_shadow);\

// Do not need the second argument,
// But just to feel better
#define	PUSH_FIFO(src, dst, dummy_input) \
	src ## _ ## dst ## _fifo[src ## _ ## dst ## _it_tail_shadow].timestamp = restore_cnt;\
	if (src ## _ ## dst ## _it_tail_shadow == FIFO_SIZE - 1) {\
		src ## _ ## dst ## _it_tail_next = 0;\
	} else {\
		src ## _ ## dst ## _it_tail_next = src ## _ ## dst ## _it_tail_shadow + 1;\
	}\
	if (src ## _ ## dst ##_it_tail_next == src ## _ ## dst ## _it_head_shadow) {\
		src ## _ ## dst ## _full_cnt++;\
		if (src ## _ ## dst ## _full_cnt == FULL_CNT) {\
			degrade_event();\
			src ## _ ## dst ## _full_cnt = 0;\
		}\
		src ## _ ## dst ## _it_tail_next = src ## _ ## dst ## _it_tail_shadow;\
	} else {\
		src ## _ ## dst ## _it_tail_shadow = src ## _ ## dst ## _it_tail_next;\
	}\
	//PRINTF("PUSH: %u %u\r\n", src ## _ ## dst ## _it_head_shadow, src ## _ ## dst ## _it_tail_next);\

#define UNDO_FIFO(src, dst) \
	src ## _ ## dst ## _it_tail_shadow = src ## _ ## dst ## _it_tail;\
	src ## _ ## dst ## _it_head_shadow = src ## _ ## dst ## _it_head;\

#define COMMIT_FIFO(src, dst) \
	src ## _ ## dst ## _it_tail = src ## _ ## dst ## _it_tail_shadow;\
	src ## _ ## dst ## _it_head = src ## _ ## dst ## _it_head_shadow;\

#define IS_FIFO_EMPTY(src, dst) \
	is_fifo_empty(src ## _ ## dst ## _it_tail_shadow, src ## _ ## dst ## _it_head_shadow)
	//src ## _ ## dst ## _it_tail_shadow == src ## _ ## dst ## _it_head_shadow


uint8_t is_fifo_empty(unsigned src, unsigned dst);

#endif

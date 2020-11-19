#if 0
#ifndef __FIFO__
#define __FIFO__

#define FIFO(type, name, size) \
	type name[size];\
	size_t name ## _fifo_it_tail_shadow = 0;\
	size_t name ## _fifo_it_tail = 0;\
	size_t name ## _fifo_it_tail_next = 0;\
	size_t name ## _fifo_it_head = 0;

// This is not idempotent!!
#define FIFO_push(name, var) \
	if (name ## _fifo_it_tail == sizeof(name) / sizeof(name[0]) - 1) {\
		name ## _fifo_it_tail_next = 0;\
	} else {\
		name ## _fifo_it_tail_next = name ## _fifo_it_tail + 1;\
	}\
	if (name ## _fifo_it_tail_next == name ## _fifo_it_head) {\
	} else {\
		name[name ## _fifo_it_tail] = var;\
		name ## _fifo_it_tail = name ## _fifo_it_tail_next;\
	}

#define FIFO_pop(name) \

#define FIFO_len(name) \

#define FIFO_isEmpty(name) \


#endif

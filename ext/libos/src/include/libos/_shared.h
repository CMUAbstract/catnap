#ifndef __SHARED__
#define __SHARED__
#include <stdint.h>
#include <stddef.h>

// This is not fundamental.
// If we use a compiler, we can do much
// better
// TODO: Can we use __COUNTER__ macro
// to check and prevent overflows?
#define BACKUP_SIZE 100

extern uint8_t* backup_addr_src[BACKUP_SIZE];
extern uint8_t* backup_addr_dst[BACKUP_SIZE];
extern size_t backup_size[BACKUP_SIZE];
extern unsigned backup_iter;
extern unsigned undo_counter;

// This can be much more elagant, if we use a compiler,
// but for now...
extern uint8_t* isWrittens[BACKUP_SIZE];
extern unsigned isWrittens_size[BACKUP_SIZE];

// These are temp. It should be automated by
// a compiler frontend or at least a python script
#define MANUAL_CLEAR(name, ...) MANUAL_CLEAR_(name, ##__VA_ARGS__, 2, 1)
#define MANUAL_CLEAR_(name, i, n, ...) MANUAL_CLEAR##n(name, i)
#define MANUAL_CLEAR1(name, ...) \
	isWritten_ ## name = 0;

#define MANUAL_CLEAR2(name, i) \
	for (size_t iter = 0; iter < i; ++iter) {\
		isWritten_ ## name[iter] = 0;\
	}


#define SHARED(type, name, ...) SHARED_(type, name, ##__VA_ARGS__, 2, 1)
#define SHARED_(type, name, i, n, ...) SHARED##n(type, name, i)
#define SHARED1(type, name, ...) \
	__nv type name; \
	__nv type bak_ ## name;\
	__nv unsigned isWritten_ ## name = 0;

#define SHARED2(type, name, i) \
	__nv type name[i]; \
	__nv type bak_ ## name[i];\
	__nv unsigned isWritten_ ## name[i] = {0};

#define WRITE(dst, src) \
	if (isWritten_ ## dst != undo_counter) {\
		bak_ ## dst = dst;\
		backup_addr_dst[backup_iter] = &dst;\
		backup_addr_src[backup_iter] = &src;\
		backup_size[backup_iter] = sizeof(dst);\
		backup_iter++;\
		isWritten_ ## dst = undo_counter;\
	}\
	dst = src;

// Avoid backup_iter == 0
// TODO: clear_all_isWritten
// should be in the main.c written by the compiler
#define ulog_commit() \
	task_it_tail = task_it_tail_shadow;\
	incr_undo_counter();

#define incr_undo_counter() \
	undo_counter++;\
	if (!undo_counter) {\
		clear_all_isWritten();\
		undo_counter++;\
	}

#endif

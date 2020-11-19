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

#define LOOP_CF 200


__nv unsigned rand_seed = 38;
unsigned my_rand()
{
	// For some reason, rand() does not work with InK.
	// I am too lazy to debug, so I use my_rand instead
	rand_seed += 17;
	return rand_seed % 23;
}


void event_starter(unsigned param)
{
	P1DIR |= BIT5;
	P1OUT |= BIT5;
	P1OUT &= ~BIT5;
	POST(t_main);
	DISABLE_EVENT(e_starter);
}




/*************** CF CODE ****************/

//#define NUM_BUCKETS 256 // must be a power of 2
#define NUM_BUCKETS 128 // must be a power of 2
//#define NUM_BUCKETS 64 // must be a power of 2
#define MAX_RELOCATIONS 8
#define CONT_POWER 0
typedef uint16_t value_t;
typedef uint16_t hash_t;
typedef uint16_t fingerprint_t;
typedef uint16_t index_t; // bucket index

#define NUM_KEYS (NUM_BUCKETS / 4) // shoot for 25% occupancy
#define INIT_KEY 0x1
void print_filter(fingerprint_t *filter)
{
    unsigned i;
	BLOCK_PRINTF_BEGIN();
	for (i = 0; i < NUM_BUCKETS; ++i) {
		BLOCK_PRINTF("%04x ", filter[i]);
		if (i > 0 && (i + 1) % 8 == 0){
			BLOCK_PRINTF("\r\n");
		}
	}
	BLOCK_PRINTF_END();
}

static hash_t djb_hash(uint8_t* data, unsigned len)
{
	uint32_t hash = 5381;
	unsigned int i;

	for(i = 0; i < len; data++, i++)
		hash = ((hash << 5) + hash) + (*data);

	return hash & 0xFFFF;
}

static index_t hash_fp_to_index(fingerprint_t fp)
{
	hash_t hash = djb_hash((uint8_t *)&fp, sizeof(fingerprint_t));
	return hash & (NUM_BUCKETS - 1); // NUM_BUCKETS must be power of 2
}

static index_t hash_key_to_index(value_t fp)
{
	hash_t hash = djb_hash((uint8_t *)&fp, sizeof(value_t));
	return hash & (NUM_BUCKETS - 1); // NUM_BUCKETS must be power of 2
}

static fingerprint_t hash_to_fingerprint(value_t key)
{
	return djb_hash((uint8_t *)&key, sizeof(value_t));
}

static value_t generate_key(value_t prev_key)
{
	// insert pseufo-random integers, for testing
	// If we use consecutive ints, they hash to consecutive DJB hashes...
	// NOTE: we are not using rand(), to have the sequence available to verify
	// that that are no false negatives (and avoid having to save the values).
	return (prev_key + 1) * 17;
}

static bool insert(fingerprint_t *filter, value_t key)
{
	fingerprint_t fp1, fp2, fp_victim, fp_next_victim;
	index_t index_victim, fp_hash_victim;
	unsigned relocation_count = 0;

	fingerprint_t fp = hash_to_fingerprint(key);

	index_t index1 = hash_key_to_index(key);

	index_t fp_hash = hash_fp_to_index(fp);
	index_t index2 = index1 ^ fp_hash;

	LOG("insert: key %04x fp %04x h %04x i1 %u i2 %u\r\n",
			key, fp, fp_hash, index1, index2);

	fp1 = filter[index1];
	LOG("insert: fp1 %04x\r\n", fp1);
	if (!fp1) { // slot 1 is free
		filter[index1] = fp;
	} else {
		fp2 = filter[index2];
		LOG("insert: fp2 %04x\r\n", fp2);
		if (!fp2) { // slot 2 is free
			filter[index2] = fp;
		} else { // both slots occupied, evict
			if (my_rand() & 0x80) { // don't use lsb because it's systematic
				index_victim = index1;
				fp_victim = fp1;
			} else {
				index_victim = index2;
				fp_victim = fp2;
			}

			LOG("insert: evict [%u] = %04x\r\n", index_victim, fp_victim);
			filter[index_victim] = fp; // evict victim

			do { // relocate victim(s)
				fp_hash_victim = hash_fp_to_index(fp_victim);
				index_victim = index_victim ^ fp_hash_victim;

				fp_next_victim = filter[index_victim];
				filter[index_victim] = fp_victim;

				LOG("insert: moved %04x to %u; next victim %04x\r\n",
						fp_victim, index_victim, fp_next_victim);

				fp_victim = fp_next_victim;
			} while (fp_victim && ++relocation_count < MAX_RELOCATIONS);

			if (fp_victim) {
				//PRINTF("insert: lost fp %04x\r\n", fp_victim);
				return false;
			}
		}
	}

	return true;
}

static bool lookup(fingerprint_t *filter, value_t key)
{
	fingerprint_t fp = hash_to_fingerprint(key);

	index_t index1 = hash_key_to_index(key);

	index_t fp_hash = hash_fp_to_index(fp);
	index_t index2 = index1 ^ fp_hash;

	LOG("lookup: key %04x fp %04x h %04x i1 %u i2 %u\r\n",
			key, fp, fp_hash, index1, index2);

	return filter[index1] == fp || filter[index2] == fp;
}

__nv volatile fingerprint_t filter[NUM_BUCKETS];
void task_main(unsigned p)
{
	unsigned i;
	value_t key;

	while (1) {
		P2DIR |= BIT5;
		P2OUT |= BIT5;
		P2OUT &= ~BIT5;
		PRINTF("start\r\n");
		unsigned inserts, members;

		for (unsigned ii = 0; ii < LOOP_CF; ++ii) {
			for (i = 0; i < NUM_BUCKETS; ++i)
				filter[i] = 0;

			key = INIT_KEY;
			inserts = 0;
			for (i = 0; i < NUM_KEYS; ++i) {
				key = generate_key(key);
				bool success = insert(filter, key);
				LOG("insert: key %04x success %u\r\n", key, success);
				if (!success) {
					;
					PROTECT_BEGIN();
					PRINTF("insert failed\r\n");
					//PRINTF("insert: key %04x failed\r\n", key);
					PROTECT_END();
				}

				inserts += success;

			}
			LOG("inserts/total: %u/%u\r\n", inserts, NUM_KEYS);

			key = INIT_KEY;
			members = 0;
			for (i = 0; i < NUM_KEYS; ++i) {
				key = generate_key(key);
				bool member = lookup(filter, key);
				LOG("lookup: key %04x member %u\r\n", key, member);
				if (!member) {
					fingerprint_t fp = hash_to_fingerprint(key);
					//PRINTF("lookup: key %04x fp %04x not member\r\n", key, fp);
				}
				members += member;
			}
			LOG("members/total: %u/%u\r\n", members, NUM_KEYS);
		}
		PRINTF("%u %u %u\r\n", inserts, members, NUM_KEYS);
	}

	return 0;
}

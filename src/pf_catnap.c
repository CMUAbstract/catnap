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
#include <math.h>

#define NUM_P 10
#define PI 3.1415926535

// Events and tasks definition
PERIOD(p2, sec_to_tick(20), sec_to_tick(20));

PARAM(particle_size, 1, NUM_P);

FUNCS(f_mag, event_mag);
FUNCS(f_pf, task_pf);
FUNCS(f_tx, event_tx);

EVENT_PERIODIC(e_mag, f_mag, p2, 1);
EVENT_APERIODIC(e_tx, f_tx, p2, 1);

TASK(t_pf, f_pf, particle_size);

// INOUT buf declaration
typedef struct {
	float mag;
	uint32_t timestamp;
} mag_pf_buf;

FIFO(e_mag, t_pf, mag_pf_buf);

void init_hw() {}
volatile __attribute__((section(".upper.nv_vars")))
float mag_field[73] = {36960.71,37357.74,37969.55,38625.57,39225.48,39781.55,40403.22,41242.4,42431.98,44048.22,46108.29,48592.46,51471.57,54726.83,58356.15,62367.87,66766.78,71541.53,76659.4,82067.03,87686.61,93402.63,99036.04,104320.71,108894.88,112325.79,114170.35,114070.66,111845.35,107541.78,101425.88,93919.94,85526.42,76767.15,68144.97,60109.47,53021.94,47123.28,42514.98,39146.98,36809.1,35150.77,33766.77,32328.82,30693.65,28934.35,27279.33,25977.77,25139.19,24645.29,24215.54,23591.42,22730.94,21926.1,21760.47,22800.33,25151.66,28370.73,31802.58,34913.44,37417.6,39248.17,40457.82,41129.32,41332.37,41131.45,40600.59,39829.5,38927.63,38033.54,37314.88,36930.53,36960.71};

volatile __attribute__((section(".upper.nv_vars")))
float ps[NUM_P];
volatile __attribute__((section(".upper.nv_vars")))
float ps2[NUM_P];
volatile __attribute__((section(".upper.nv_vars")))
float w[NUM_P];
// Double buffering
__nv float *cur_ps;
__nv float *prev_ps;

#define RAND_MAX 65535

__nv uint32_t seed = 7738;

void print_float(float f)
{
	if (f < 0) {
		PRINTF("-");
		f = -f;
	}
	PRINTF("%u.",(unsigned)f);

	// Print decimal -- 3 digits
	PRINTF("%u", (((unsigned)(f * 1000)) % 1000));
}


/************* All the math stuffs of my own ***************/
/* ------------------------------my_rand()-------------------------------------
	 my_rand, implementation of random number generater described by S. Park
	 and K. Miller, in Communications of the ACM, Oct 88, 31:10, p. 1192-1201.
	 ---------------------------------------------------------------------------- */
unsigned my_rand()
{
	uint32_t a = 16807L, m = 2147483647L, q = 127773L, r = 2836L;

	uint32_t lo, hi, test;

	hi = seed / q;
	lo = seed % q;
	test = a * lo - r * hi;

	if (test > 0) seed = test;        /* test for overflow */
	else seed = test + m;
	return (unsigned)(seed % 65536);
}

float my_sqrt(float x) {
	float z = x / 2;
	for (unsigned i = 0; i < 1000; ++i) {
		z -= (z*z - x) / (2*z);
		if ((z*z - x) < x * 0.01) {
			return z;
		}
	}
	return z;
}

/**************** Math ends here **************/

// Return between -180 ~ 180
float get_random_pos()
{
	//float r = (((float)rand()) / RAND_MAX) * 360;
	unsigned r = my_rand();
	float res = (((float)r) / RAND_MAX) * 360;
	return (res - 180);
}

#define T_SENSE 1.0 // every 1 (second?)
#define VEL 2.0
#define SENSE_ERR 5.0
#define MOVE_ERR 1.0

__nv float z1;
__nv float z1_ready = 0;

float get_random_gauss(float mu, float sigma)
{
	if (z1_ready) {
		z1_ready = 0;
		return z1 * sigma + mu;
	}

	float u1 = ((float)(my_rand())) / RAND_MAX;
	float u2 = ((float)(my_rand())) / RAND_MAX;

	float z0 = sqrt(-2.0 * log(u1)) * cos(2*PI*u2);
	z1 = sqrt(-2.0 * log(u1)) * sin(2*PI*u2);
	z1_ready = 1;
	return z0 * sigma + mu;
}

void move(float *pos)
{
	(*pos) += VEL * T_SENSE;
	(*pos) += get_random_gauss(0.0, MOVE_ERR) * T_SENSE;
	(*pos) = fmod(((*pos) + 180), 360) - 180;
	return pos;
}

#define STEP 5
// Read the mag value according to the pos
// from the map
float read_mag(const float pos)
{
	float p = pos + 180;
	
	// Linear interpolation
	float val0 = mag_field[((int)p) / STEP];
	float x = p - (((int)p) / STEP)*STEP;
	float val1 = mag_field[((int)p) / STEP + 1];

	return (val0 * (STEP - x) + val1 * x) / STEP;
}

float sense(const float pos)
{
	float val = read_mag(pos)	;
	val += get_random_gauss(0.0, SENSE_ERR);
	return val;
}

float gaussian(float mu, float sigma, float x)
{
	return exp(- ((mu - x) * (mu - x) / 100000) / (sigma * sigma) / 2.0) / sqrt(2.0 * PI * (sigma * sigma));
}

float calc_weight(const float mag, const float pos)
{
	float prob = 1.0;
	
	// Get the mag of the pos
	float my_mag = read_mag(pos);

	prob *= gaussian(my_mag, 5, mag);

	return prob;
}

__nv float p_golden;

// For emulation
__nv unsigned ran_before = 0;
void event_mag()
{
	P1DIR |= BIT4;
	P1OUT |= BIT4;
	if (!ran_before) {
		// Set initial pos of the golden
		p_golden = get_random_pos();
	}

	// Emulated sense (this should be an actual mag data sense)
	float mag = sense(p_golden);

	// Emulate the golden position moving
	move(&p_golden);

	print_float(p_golden);
	PRINTF(": ");

	mag_pf_buf* out = GET_EMPTY_FIFO(e_mag, t_pf);
	out->mag = mag;
	PUSH_FIFO(e_mag, t_pf, out);

	POST(t_pf);
	P1OUT &= ~BIT4;
}

// Events and tasks declaration from here
void task_pf(unsigned param)
{
	P2DIR |= BIT5;
	P2OUT |= BIT5;
	// Pull result from e_mic
	mag_pf_buf* in = POP_FIFO(e_mag, t_pf);
	float mag = in->mag;

	if (!ran_before) {
		// Init particles
		// TODO: This should run after a power failure as well.
		ran_before = 1;
		for (unsigned i = 0; i < param; ++i) {
			ps[i] = get_random_pos();
		}
		cur_ps = ps;
		prev_ps = ps2;
	}

	// Move the particles
	for (unsigned i = 0; i < param; ++i) {
		move(&cur_ps[i]);
	}

	// Generate particle weight
	for (unsigned i = 0; i < param; ++i) {
		w[i] = calc_weight(mag, cur_ps[i]);
	}

	// Normalize weights
	float sum = 0;
	for (unsigned i = 0; i < param; ++i) {
		sum += w[i]*w[i];
	}
	sum = sqrt(sum);
	float max_w = 0;
	for (unsigned i = 0; i < param; ++i) {
		w[i] /= sum;
		if (w[i] > max_w) {
			max_w = w[i];
		}
	}

	unsigned idx = (unsigned)((((float)my_rand()) / RAND_MAX) * param);
	float beta = 0.0;
	for (unsigned i = 0; i < param; ++i) {
		beta += (((float)my_rand()) / RAND_MAX) * max_w * 2;
		while (beta > w[idx]) {
			beta -= w[idx];
			idx = (idx + 1) % param;
		}
		prev_ps[i] = cur_ps[idx];
	}

	float *tmp = prev_ps;
	prev_ps = cur_ps;
	cur_ps = tmp;

	POST_EVENT(e_tx);

	P2OUT &= ~BIT5;
}

void event_tx()
{
	// Print for testing
	for (unsigned i = 0; i < 10; ++i) {
		print_float(cur_ps[i]);
		PRINTF(" ");
	}
	PRINTF("\r\n");
}

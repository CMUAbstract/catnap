#include <libos/comp.h>
#include <msp430.h>

__nv volatile energy_t baseline_E = 0;
__nv volatile unsigned lower_thres = DEFAULT_LOWER_THRES;
__nv volatile unsigned upper_thres = DEFAULT_UPPER_THRES;
__nv volatile unsigned max_thres = DEFAULT_NEARY_MAX_THRES;

// Volt * 100
__ro_nv const unsigned level_to_volt[NUM_LEVEL] = {
	100, 101, 105, 106, 108,
	109, 112, 116, 117, 118,
	125, 131, 132, 137, 140,
	143, 148, 150, 156, 162,
	164, 168, 171, 175, 179,
	181, 187, 193, 195, 203,
	210, 218, 226, 234, 242
};

// Pre-calculated (V^2-1) * 10000
// Note that E for V_1_08 is 1664 (E_QUANTA)
__ro_nv const energy_t level_to_E[NUM_LEVEL] = {
	0, 201, 1025, 1236, 1664,
	1881, 2544, 3456, 3689, 3924,
	5625, 7161, 7424, 8769, 9600,
	10449, 11904, 12500, 14336, 16244,
	16896, 18224, 19241, 20625, 22041,
	22761, 24969, 27249, 28025, 31209,
	34100, 37524, 41076, 44756, 48564
};
//__ro_nv const unsigned level_to_E[NUM_LEVEL] = {
//	0, 1, 25, 36, 64,
//	81, 144, 256, 289, 324,
//	625, 961, 1024, 1369, 1600,
//	1849, 2304, 3500, 3136, 3844,
//	4096, 4624, 5041, 5625, 6241,
//	6561, 7569, 8649, 9025, 10609,
//	12100, 13924, 15876, 17956, 20164
//};

__ro_nv const unsigned level_to_reg[NUM_LEVEL] = {
	CEREFL_2 | CEREF0_15 | CEREF1_16, // 0
	CEREFL_1 | CEREF0_26 | CEREF1_27, // 1
	CEREFL_1 | CEREF0_27 | CEREF1_28, // 2
	CEREFL_2 | CEREF0_16 | CEREF1_17, // 3
	CEREFL_1 | CEREF0_28 | CEREF1_29, // 4
	CEREFL_3 | CEREF0_13 | CEREF1_14, // 5
	CEREFL_1 | CEREF0_29 | CEREF1_30, // 6
	CEREFL_1 | CEREF0_30 | CEREF1_31, // 7
	CEREFL_3 | CEREF0_14 | CEREF1_15, // 8
	CEREFL_2 | CEREF0_18 | CEREF1_19, // 9
	CEREFL_2 | CEREF0_19 | CEREF1_20, // 10
	CEREFL_2 | CEREF0_20 | CEREF1_21, // 11
	CEREFL_3 | CEREF0_16 | CEREF1_17, // 12
	CEREFL_2 | CEREF0_21 | CEREF1_22, // 13
	CEREFL_3 | CEREF0_17 | CEREF1_18, // 14
	CEREFL_2 | CEREF0_22 | CEREF1_23, // 15
	CEREFL_3 | CEREF0_18 | CEREF1_19, // 16
	CEREFL_2 | CEREF0_23 | CEREF1_24, // 17
	CEREFL_2 | CEREF0_24 | CEREF1_25, // 18
	CEREFL_2 | CEREF0_25 | CEREF1_26, // 19
	CEREFL_3 | CEREF0_20 | CEREF1_21, // 20
	CEREFL_2 | CEREF0_26 | CEREF1_27, // 21
	CEREFL_3 | CEREF0_21 | CEREF1_22, // 22
	CEREFL_2 | CEREF0_27 | CEREF1_28, // 23
	CEREFL_3 | CEREF0_22 | CEREF1_23, // 24
	CEREFL_2 | CEREF0_28 | CEREF1_29, // 25
	CEREFL_2 | CEREF0_29 | CEREF1_30, // 26
	CEREFL_2 | CEREF0_30 | CEREF1_31, // 27
	CEREFL_3 | CEREF0_24 | CEREF1_25, // 28
	CEREFL_3 | CEREF0_25 | CEREF1_26, // 29
	CEREFL_3 | CEREF0_26 | CEREF1_27, // 30
	CEREFL_3 | CEREF0_27 | CEREF1_28, // 31
	CEREFL_3 | CEREF0_28 | CEREF1_29, // 32
	CEREFL_3 | CEREF0_29 | CEREF1_30, // 33
	CEREFL_3 | CEREF0_30 | CEREF1_31 // 34
};

// Return the level (voltage) so that
// (V-1)^2 * 10000 > e
energy_t get_ceiled_level(energy_t e) {
	// linear search. Because I am lazy
	for (unsigned i = 0; i < NUM_LEVEL; ++i) {
		// TODO: This should be pre-calculated
		energy_t e2 = level_to_E[i];
		if (e2 > e) {
			return i;
		}
	}
	return INVALID_LEVEL;
}

// Given a lower level (voltage), return a
// upper level so that
// (V_u^2-1) * 100000 ~ (V_l^2-1) * 100000 + E_OPERATING
energy_t get_upper_level(unsigned lower_level) {
	energy_t upper_e = level_to_E[lower_level] + E_OPERATING;

	return get_ceiled_level(upper_e);
}
#if 0
unsigned level_to_reg(unsigned level) {
	unsigned result;
	// Implemented in a crude way to be compatible with
	// possible future change of macro

	// Trigger calculation:
	// {1.0, 1.2, 2.0, 2.5} * {REF0_VAL} / 32
	// e.g. - case 58: 1.2 * 27 / 32 = 1.0125V
	switch (level) {
		case 1:
			result = CEREFL_1 | CEREF0_0 | CEREF1_1;
			break;
		case 2:
			result = CEREFL_1 | CEREF0_1 | CEREF1_2;
			break;
		case 3:
			result = CEREFL_1 | CEREF0_2 | CEREF1_3;
			break;
		case 4:
			result = CEREFL_1 | CEREF0_3 | CEREF1_4;
			break;
		case 5:
			result = CEREFL_1 | CEREF0_4 | CEREF1_5;
			break;
		case 6:
			result = CEREFL_1 | CEREF0_5 | CEREF1_6;
			break;
		case 7:
			result = CEREFL_1 | CEREF0_6 | CEREF1_7;
			break;
		case 8:
			result = CEREFL_1 | CEREF0_7 | CEREF1_8;
			break;
		case 9:
			result = CEREFL_1 | CEREF0_8 | CEREF1_9;
			break;
		case 10:
			result = CEREFL_1 | CEREF0_9 | CEREF1_10;
			break;
		case 11:
			result = CEREFL_1 | CEREF0_10 | CEREF1_11;
			break;
		case 12:
			result = CEREFL_1 | CEREF0_11 | CEREF1_12;
			break;
		case 13:
			result = CEREFL_1 | CEREF0_12 | CEREF1_13;
			break;
		case 14:
			result = CEREFL_1 | CEREF0_13 | CEREF1_14;
			break;
		case 15:
			result = CEREFL_1 | CEREF0_14 | CEREF1_15;
			break;
		case 16:
			result = CEREFL_1 | CEREF0_15 | CEREF1_16;
			break;
		case 17:
			result = CEREFL_1 | CEREF0_16 | CEREF1_17;
			break;
		case 18:
			result = CEREFL_1 | CEREF0_17 | CEREF1_18;
			break;
		case 19:
			result = CEREFL_1 | CEREF0_18 | CEREF1_19;
			break;
		case 20:
			result = CEREFL_1 | CEREF0_19 | CEREF1_20;
			break;
		case 21:
			result = CEREFL_1 | CEREF0_20 | CEREF1_21;
			break;
		case 22:
			result = CEREFL_1 | CEREF0_21 | CEREF1_12;
			break;
		case 23:
			result = CEREFL_1 | CEREF0_22 | CEREF1_23;
			break;
		case 24:
			result = CEREFL_1 | CEREF0_23 | CEREF1_24;
			break;
		case 25:
			result = CEREFL_1 | CEREF0_24 | CEREF1_25;
			break;
		case 26:
			result = CEREFL_1 | CEREF0_25 | CEREF1_26;
			break;
		case 27:
			result = CEREFL_1 | CEREF0_26 | CEREF1_27;
			break;
		case 28:
			result = CEREFL_1 | CEREF0_27 | CEREF1_28;
			break;
		case 29:
			result = CEREFL_1 | CEREF0_28 | CEREF1_29;
			break;
		case 30:
			result = CEREFL_1 | CEREF0_29 | CEREF1_30;
			break;
		case 31:
			result = CEREFL_1 | CEREF0_30 | CEREF1_31;
			break;

		case 32:
			result = CEREFL_2 | CEREF0_0 | CEREF1_1;
			break;
		case 33:
			result = CEREFL_2 | CEREF0_1 | CEREF1_2;
			break;
		case 34:
			result = CEREFL_2 | CEREF0_2 | CEREF1_3;
			break;
		case 35:
			result = CEREFL_2 | CEREF0_3 | CEREF1_4;
			break;
		case 36:
			result = CEREFL_2 | CEREF0_4 | CEREF1_5;
			break;
		case 37:
			result = CEREFL_2 | CEREF0_5 | CEREF1_6;
			break;
		case 38:
			result = CEREFL_2 | CEREF0_6 | CEREF1_7;
			break;
		case 39:
			result = CEREFL_2 | CEREF0_7 | CEREF1_8;
			break;
		case 40:
			result = CEREFL_2 | CEREF0_8 | CEREF1_9;
			break;
		case 41:
			result = CEREFL_2 | CEREF0_9 | CEREF1_10;
			break;
		case 42:
			result = CEREFL_2 | CEREF0_10 | CEREF1_11;
			break;
		case 43:
			result = CEREFL_2 | CEREF0_11 | CEREF1_12;
			break;
		case 44:
			result = CEREFL_2 | CEREF0_12 | CEREF1_13;
			break;
		case 45:
			result = CEREFL_2 | CEREF0_13 | CEREF1_14;
			break;
		case 46:
			result = CEREFL_2 | CEREF0_14 | CEREF1_15;
			break;
		case 47:
			result = CEREFL_2 | CEREF0_15 | CEREF1_16;
			break;
		case 48:
			result = CEREFL_2 | CEREF0_16 | CEREF1_17;
			break;
		case 49:
			result = CEREFL_2 | CEREF0_17 | CEREF1_18;
			break;
		case 50:
			result = CEREFL_2 | CEREF0_18 | CEREF1_19;
			break;
		case 51:
			result = CEREFL_2 | CEREF0_19 | CEREF1_20;
			break;
		case 52:
			result = CEREFL_2 | CEREF0_20 | CEREF1_21;
			break;
		case 53:
			result = CEREFL_2 | CEREF0_21 | CEREF1_12;
			break;
		case 54:
			result = CEREFL_2 | CEREF0_22 | CEREF1_23;
			break;
		case 55:
			result = CEREFL_2 | CEREF0_23 | CEREF1_24;
			break;
		case 56:
			result = CEREFL_2 | CEREF0_24 | CEREF1_25;
			break;
		case 57:
			result = CEREFL_2 | CEREF0_25 | CEREF1_26;
			break;
		case 58:
			result = CEREFL_2 | CEREF0_26 | CEREF1_27;
			break;
		case 59:
			result = CEREFL_2 | CEREF0_27 | CEREF1_28;
			break;
		case 60:
			result = CEREFL_2 | CEREF0_28 | CEREF1_29;
			break;
		case 61:
			result = CEREFL_2 | CEREF0_29 | CEREF1_30;
			break;
		case 62:
			result = CEREFL_2 | CEREF0_30 | CEREF1_31;
			break;

		case 63:
			result = CEREFL_3 | CEREF0_0 | CEREF1_1;
			break;
		case 64:
			result = CEREFL_3 | CEREF0_1 | CEREF1_2;
			break;
		case 65:
			result = CEREFL_3 | CEREF0_2 | CEREF1_3;
			break;
		case 66:
			result = CEREFL_3 | CEREF0_3 | CEREF1_4;
			break;
		case 67:
			result = CEREFL_3 | CEREF0_4 | CEREF1_5;
			break;
		case 68:
			result = CEREFL_3 | CEREF0_5 | CEREF1_6;
			break;
		case 69:
			result = CEREFL_3 | CEREF0_6 | CEREF1_7;
			break;
		case 70:
			result = CEREFL_3 | CEREF0_7 | CEREF1_8;
			break;
		case 71:
			result = CEREFL_3 | CEREF0_8 | CEREF1_9;
			break;
		case 72:
			result = CEREFL_3 | CEREF0_9 | CEREF1_10;
			break;
		case 73:
			result = CEREFL_3 | CEREF0_10 | CEREF1_11;
			break;
		case 74:
			result = CEREFL_3 | CEREF0_11 | CEREF1_12;
			break;
		case 75:
			result = CEREFL_3 | CEREF0_12 | CEREF1_13;
			break;
		case 76:
			result = CEREFL_3 | CEREF0_13 | CEREF1_14;
			break;
		case 77:
			result = CEREFL_3 | CEREF0_14 | CEREF1_15;
			break;
		case 78:
			result = CEREFL_3 | CEREF0_15 | CEREF1_16;
			break;
		case 79:
			result = CEREFL_3 | CEREF0_16 | CEREF1_17;
			break;
		case 80:
			result = CEREFL_3 | CEREF0_17 | CEREF1_18;
			break;
		case 81:
			result = CEREFL_3 | CEREF0_18 | CEREF1_19;
			break;
		case 82:
			result = CEREFL_3 | CEREF0_19 | CEREF1_20;
			break;
		case 83:
			result = CEREFL_3 | CEREF0_20 | CEREF1_21;
			break;
		case 84:
			result = CEREFL_3 | CEREF0_21 | CEREF1_12;
			break;
		case 85:
			result = CEREFL_3 | CEREF0_22 | CEREF1_23;
			break;
		case 86:
			result = CEREFL_3 | CEREF0_23 | CEREF1_24;
			break;
		case 87:
			result = CEREFL_3 | CEREF0_24 | CEREF1_25;
			break;
		case 88:
			result = CEREFL_3 | CEREF0_25 | CEREF1_26;
			break;
		case 89:
			result = CEREFL_3 | CEREF0_26 | CEREF1_27;
			break;
		case 90:
			result = CEREFL_3 | CEREF0_27 | CEREF1_28;
			break;
		case 91:
			result = CEREFL_3 | CEREF0_28 | CEREF1_29;
			break;
		case 92:
			result = CEREFL_3 | CEREF0_29 | CEREF1_30;
			break;
		case 93:
			result = CEREFL_3 | CEREF0_30 | CEREF1_31;
			break;

	}
	return result;
}
#endif

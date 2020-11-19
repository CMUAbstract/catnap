#define checkpoint() \
	__asm__ volatile ("MOV 50(R1), &0x4000");\
	__asm__ volatile ("MOV 48(R1), &0x4008");\
	__asm__ volatile ("MOVX.A 0(R1), &0x4010");\
	__asm__ volatile ("MOVX.A 4(R1), &0x4014");\
	__asm__ volatile ("MOVX.A 8(R1), &0x4018");\
	__asm__ volatile ("MOVX.A 12(R1), &0x401c");\
	__asm__ volatile ("MOVX.A 16(R1), &0x4020");\
	__asm__ volatile ("MOVX.A 20(R1), &0x4024");\
	__asm__ volatile ("MOVX.A 24(R1), &0x4028");\
	__asm__ volatile ("MOVX.A 28(R1), &0x402c");\
	__asm__ volatile ("MOVX.A 32(R1), &0x4030");\
	__asm__ volatile ("MOVX.A 36(R1), &0x4034");\
	__asm__ volatile ("MOVX.A 40(R1), &0x4038");\
	__asm__ volatile ("MOVX.A 44(R1), &0x403c");\
	__asm__ volatile ("ADD #52, R1");\
	__asm__ volatile ("MOVX.A R1, &0x4004");\
	__asm__ volatile ("SUB #52, R1");\

#define checkpoint_before_event() \
	__asm__ volatile ("MOV 62(R1), &0x4000");\
	__asm__ volatile ("MOV 60(R1), &0x4008");\
	__asm__ volatile ("MOVX.A 12(R1), &0x4010");\
	__asm__ volatile ("MOVX.A 16(R1), &0x4014");\
	__asm__ volatile ("MOVX.A 20(R1), &0x4018");\
	__asm__ volatile ("MOVX.A 24(R1), &0x401c");\
	__asm__ volatile ("MOVX.A 28(R1), &0x4020");\
	__asm__ volatile ("MOVX.A 32(R1), &0x4024");\
	__asm__ volatile ("MOVX.A 36(R1), &0x4028");\
	__asm__ volatile ("MOVX.A 40(R1), &0x402c");\
	__asm__ volatile ("MOVX.A 44(R1), &0x4030");\
	__asm__ volatile ("MOVX.A 48(R1), &0x4034");\
	__asm__ volatile ("MOVX.A 52(R1), &0x4038");\
	__asm__ volatile ("MOVX.A 56(R1), &0x403c");\
	__asm__ volatile ("ADD #64, R1");\
	__asm__ volatile ("MOVX.A R1, &0x4004");\
	__asm__ volatile ("SUB #64, R1");\


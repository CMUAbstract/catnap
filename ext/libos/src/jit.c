#include <libos/jit.h>
#include <libos/task.h>
#include <libos/os.h>
#include <libos/comp.h>
#include <libos/sleep.h>
#include <libos/timer.h>
#include <libio/console.h>
#include <libos/global.h>

__nv unsigned mode = OPERATING;
__nv unsigned chkpt_mask = 0;
__nv unsigned p1out_bak;
__nv unsigned p2out_bak;
__nv unsigned p3out_bak;
__nv unsigned p4out_bak;
__nv unsigned p5out_bak;
__nv unsigned p6out_bak;
__nv unsigned p7out_bak;
__nv unsigned p8out_bak;
__nv unsigned pjout_bak;
__nv unsigned p1DIR_bak;
__nv unsigned p2DIR_bak;
__nv unsigned p3DIR_bak;
__nv unsigned p4DIR_bak;
__nv unsigned p5DIR_bak;
__nv unsigned p6DIR_bak;
__nv unsigned p7DIR_bak;
__nv unsigned p8DIR_bak;
__nv unsigned pjDIR_bak;
__nv unsigned restore_cnt = 0;
//__nv unsigned is_reboot;
/*
* Restore checkpoint if not the first execution.
*/
void restore() {
	restore_cnt++;
	restore_gpio();
	// Idea for restarting
	// 1) Let's start immediately from the next event
	// that was supposed to run
	// This means that we call adjust_nextT()
	// 2) If prev event has not finished,
	// Let us rollback to the time before it started
	// TODO: task queue and FIFO should be
	// undo-ed or something in this case!

	if (event_running) {
		// Rolling back time if died while running event
		rollback_nextT();
		event_running = 0;

		// restore task queue & fifos
		undo_task_fifo();
		undo_input_fifo();
	} else {
		// Start immediately the next event in the queue
		T_t elapsed_time_while_dead;
		get_next_event(&elapsed_time_while_dead);
		adjust_nextT(elapsed_time_while_dead);

		// Commit any uncommitted work
		// These will not have any effect if everything is
		// already committed
		commit_task_fifo();
		//PRINTF("commit in restore\r\n");
		commit_input_fifo();
		pre_event->burst_cnt = pre_event->burst_cnt_shadow;
	}
	configure_next_wakeup();

	// this should be unset after restoring..
	// chkpt_mask_init = 0;

	// Before restoring the regs,
	// Clear LPM3_bits on R2
	// (To handle the case of 
	// power failure while sleeping)
	// 208 = 2b'1101
	__asm__ volatile ("BIC #208, &0x4008");
	//PRINTF("R2: %x\r\n", *((unsigned *)(0x4008)));
	// First, restore R1
	// and increment it by 2		
	__asm__ volatile ("MOVX.A &0x4004, R1");
	__asm__ volatile ("MOVX.A &0x4010, R4");
	__asm__ volatile ("MOVX.A &0x4014, R5");
	__asm__ volatile ("MOVX.A &0x4018, R6");
	__asm__ volatile ("MOVX.A &0x401c, R7");
	__asm__ volatile ("MOVX.A &0x4020, R8");
	__asm__ volatile ("MOVX.A &0x4024, R9");
	__asm__ volatile ("MOVX.A &0x4028, R10");
	__asm__ volatile ("MOVX.A &0x402c, R11");
	__asm__ volatile ("MOVX.A &0x4030, R12");
	__asm__ volatile ("MOVX.A &0x4034, R13");
	__asm__ volatile ("MOVX.A &0x4038, R14");
	__asm__ volatile ("MOVX.A &0x403c, R15");
	__asm__ volatile ("MOV &0x4008, R2");
	__asm__ volatile ("MOV &0x4000, R0");
}

/*
 *	Initialize comparator to be fired
 * when energy is low
 */
void init_jit() {
	// for debugging purpose
	P1OUT &= ~BIT2;
	P1DIR |= BIT2;
	P2DIR |= BIT6;

	// set up for checkpoint trigger
	// P3.1 is the input pin for Vcap
	P3SEL1 |= BIT1;
	P3SEL0 |= BIT1;
	CECTL3 |= CEPD3; // Disable buffer (for low power, might not needed)

	// when boot, Vcap drop suddenly.
	// wait until it is filled for reliable execution
	// If the system starts running with a small spike,
	// increase the threshold for wakeup
	// Also, this code prevents the system from running
	// as soon as it gets flashed
	mode = SLEEPING;
	// reconfigure COMP_E
	CECTL0 = CEIPEN | CEIPSEL_13;
	//SET_UPPER_THRES(V_1_31);
	SET_UPPER_COMP();
	CECTL1 = CEPWRMD_2 | CEON; // Ultra low power mode, on
	//is_reboot = 1;

	// Let the comparator output settle before checking or setting up interrupt
	while (!CERDYIFG);
	// clear int flag and enable int
	CEINT &= ~(CEIFG | CEIIFG);
	CEINT |= CEIE;

	P2OUT |= BIT6;
	P2OUT &= ~BIT6;
	P1OUT |= BIT2;
	P1OUT &= ~BIT2;
	P2OUT |= BIT6;
	P2OUT &= ~BIT6;
	// and sleep!
	low_power_sleep();
	P2OUT |= BIT6;
	P2OUT &= ~BIT6;
	P2OUT |= BIT6;
	P2OUT &= ~BIT6;
	P1OUT |= BIT2;
	P1OUT &= ~BIT2;
	P2OUT |= BIT6;
	P2OUT &= ~BIT6;
	P2OUT |= BIT6;
	P2OUT &= ~BIT6;

	chkpt_mask = 0;
}


/*
 * Handler for checkpointing & sleep
 * TODO: currently it is using a single comaprator, dynamically reconfiguring it.
 * However, if we can use multiple channel, it will be much easier.
 */
void __attribute__((interrupt(COMP_E_VECTOR)))compISRHandler(void)
{
	// Manually disable checkpoint and timer interrupt because
	// PRINTF within here may enable GIE
	timer_interrupt_disable();
	CEINT &= ~CEIE;
	// when 1.0125V is met while operating
	if (mode == OPERATING) {
		P2OUT |= BIT6;
		P2OUT &= ~BIT6;
		P2OUT |= BIT6;
		P2OUT &= ~BIT6;

		mode = SLEEPING;
		// reconfigure COMP_E
		CECTL0 = CEIPEN | CEIPSEL_13;
		SET_UPPER_COMP();

		// checkpoint!!
		if (!chkpt_mask) {
			checkpoint_gpio();
			checkpoint();
		}
#if QUICKRECALL == 1
		P1OUT |= BIT0;
		// Fake invocation to make the stack size the same
		calculate_charge_rate();
		while(1) {
			P1OUT |= BIT0;
			P1OUT &= ~BIT0;
		}
#endif

		// Set possible charge start point
		// If not already set
		// (This is to avoid corner cases where the
		// system which was initially discharging use
		// up too much energy on an event and immediately
		// trigger the handler on exit, where the hanlder will
		// overwrite the correct start point without the
		// null check
#if ENERGY_MEASURE == 1
		if (!v_charge_start) {
			v_charge_start = level_to_volt[lower_thres];
			t_charge_start = TA0R;
		}
#endif

		// and go to sleep!!
		low_power_sleep_on_exit();

		P2OUT |= BIT6;
		P2OUT &= ~BIT6;
	}
#if ENERGY_MEASURE == 0
	// Dummy for easy testing (to make the stack size same)
	else if (mode == 100) {
		calculate_charge_rate();
	}
#endif
	// when 1.3125V is met while sleeping
	else if (mode == SLEEPING) {
		P2OUT |= BIT6;
		P2OUT &= ~BIT6;
		P2OUT |= BIT6;
		P2OUT &= ~BIT6;
		P2OUT |= BIT6;
		P2OUT &= ~BIT6;


		if (no_task_running && taskQ_isEmpty()) {
			mode = IDLING;

			// reconfigure COMP_E
			CECTL0 = CEIPEN | CEIPSEL_13;
			SET_MAX_UPPER_COMP();
		} else {
			mode = OPERATING;

			// reconfigure COMP_E
			CECTL0 = CEIMEN | CEIMSEL_13;
			SET_LOWER_COMP();
		}

#if ENERGY_MEASURE == 1
		// Set charge end point
		if (!v_charge_end) {
			v_charge_end = level_to_volt[upper_thres];
			t_charge_end = TA0R;
			// If the event changed the mode from
			// Operating -> Sleep, 
			// This if can get that.
			// If so, it means that actually
			// this is not a valid charge curve, so skip
			if (v_charge_end < v_charge_start) {
				v_charge_end = 0;
			}
		}

		if (v_charge_end) {
			calculate_charge_rate();
		}
#endif
		if (mode == IDLING) {
			if (!v_charge_start) {
				v_charge_start = level_to_volt[upper_thres];
				t_charge_start = TA0R;
				PRINTF("vs2: %u\r\n", v_charge_start);
			}
		}

		low_power_wakeup_on_exit();
		P2OUT |= BIT6;
		P2OUT &= ~BIT6;
		P1OUT |= BIT2;
		P1OUT &= ~BIT2;
		P2OUT |= BIT6;
		P2OUT &= ~BIT6;
		P1OUT |= BIT2;
		P1OUT &= ~BIT2;
		P2OUT |= BIT6;
		P2OUT &= ~BIT6;
	} else if (mode == IDLING){
		// Had nothing to do, so was charged to...
		v_charge_end = level_to_volt[max_thres];
		t_charge_end = TA0R;

		calculate_charge_rate();

		// If the system reached here,
		// it means there are no tasks to run.
		// We measured the incoming energy,
		// and now we should not measure more
		// in this region (it is too noisy).
		// We go to OPERATING mode
		// to handle checkpoint if needed...
		// If there is an event, it will tell us what to do
		mode = OPERATING;
		CECTL0 = CEIMEN | CEIMSEL_13;
		SET_LOWER_COMP();

		P1OUT |= BIT2;
		P1OUT &= ~BIT2;
		P1OUT |= BIT2;
		P1OUT &= ~BIT2;
	}
	__disable_interrupt();
	timer_interrupt_enable();
	CEINT |= CEIE;
}

void checkpoint_manual() {
	__asm__ volatile ("MOV 0(R1), &0x4000");//r0
	__asm__ volatile ("MOV R2, &0x4008");//r2
	// if restore happens here, it means
	// possibly bit 5 of SR is set (CPUOFF)
	// which means checkpoint happened while sleeping
	__asm__ volatile ("MOVX.A R4, &0x4010");//r4
	__asm__ volatile ("MOVX.A R5, &0x4014");//r5
	__asm__ volatile ("MOVX.A R6, &0x4018");
	__asm__ volatile ("MOVX.A R7, &0x401c");
	__asm__ volatile ("MOVX.A R8, &0x4020");
	__asm__ volatile ("MOVX.A R9, &0x4024");
	__asm__ volatile ("MOVX.A R10, &0x4028");
	__asm__ volatile ("MOVX.A R11, &0x402c");
	__asm__ volatile ("MOVX.A R12, &0x4030");
	__asm__ volatile ("MOVX.A R13, &0x4034");
	__asm__ volatile ("MOVX.A R14, &0x4038");
	__asm__ volatile ("MOVX.A R15, &0x403c");
	__asm__ volatile ("ADD #4, R1");
	__asm__ volatile ("MOVX.A R1, &0x4004"); //r1
	__asm__ volatile ("SUB #4, R1");
}

/*
 * PendSV handler for libtask on Cortex-M (M4F/M7).
 *
 * Needs taskcm_spfr / taskcm_spto as non-static globals in context_cm.c:
 *	uint8_t **taskcm_spfr;	(nil on first switch from MSP to PSP)
 *	uint8_t  *taskcm_spto;
 * PendSV_Handler is an alias of taskcm_handlependsv.
 */
	.syntax unified
	.thumb
	.text

	.global	taskcm_handlependsv
	.global	PendSV_Handler
	.type	taskcm_handlependsv, %function
	.type	PendSV_Handler, %function

	.thumb_func
PendSV_Handler:
	.thumb_func
taskcm_handlependsv:
	LDR	r3, =taskcm_spfr
	LDR	r2, [r3]		/* r2 = spfr */

	/*
	 * If this is the first context switch, called from the main stack (MSP),
	 * we do not need to save the context since we never return to MSP.
	 * Instead, jump to the 1: label.
	 */
	CBZ	r2, restore


	/* Save registers to the PSP stack;
	 * additional registers are pushed in hardware
	 */
	MRS	r0, psp		/* move psp to register */

#if defined (__VFP_FP__) && !defined(__SOFTFP__)
	/* Currently, the `lr` register contains the EXC_RETURN value,
	 * that was set by the controller when the PendSV handler got called.
	 * If EXC_RETURN.FType (bit 4) is zero, this means
	 * the FPU was in use at the time the current task was interrupted, and
	 * its state has to be saved when switching context.
	 * As explained in Cortex-M4(F) Lazy Stacking and Context Switching,
	 * p. 17, s16-s31 must be saved to the stack manually,
	 * using the VSTM instruction.
	 * The use of this instruction will trigger saving of s0-s15 registers,
	 * which is done by the controller automatically.
	 */
	TST	lr, #(1<<4)
	IT	eq
	VSTMdbeq r0!, {s16-s31}
#endif

	/* save r4-r11, and the EXC_RETURN value (from `lr`)
	 */
	STMdb	r0!, {r4-r11, lr}

	STR	r0, [r2]		/* *spfr = new top of saved stack; psp is set below */

restore:
	/*
	 * Load registers with contents of the PSP stack;
	 * additional registers are popped in hardware
	 */
	LDR	r3, =taskcm_spto
	LDR	r0, [r3]

	/*
	 * Restore r4-r11, and the EXC_RETURN value (in `lr`)
	 */
	LDMia	r0!, {r4-r11, lr}

#if defined (__VFP_FP__) && !defined(__SOFTFP__)
	/* The LDM instruction above also restored `lr`,
	 * which now again contains the EXC_RETURN value
	 * from the moment the task's context got saved last time.
	 * If EXC_RETURN.FType is 0, FP context was in use;
	 * in this case, registers s16-s31 must be restored manually
	 * (registers s0-s15 will automatically be restored by the controller).
	 */
	TST	lr, #(1<<4)
	IT	eq
	VLDMiaeq r0!, {s16-s31}
#endif

	MSR	psp, r0
	ISB
	BX	lr		/* return from exception; EXC_RETURN is in lr */

	.ltorg
	.size	taskcm_handlependsv, . - taskcm_handlependsv

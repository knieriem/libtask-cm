#include <u.h>
#include <libc.h>

#include "task.h"
#include "taskimpl.h"


/* interrupt control and state register */
#define ICSR (*(volatile uint32_t *) 0xE000ED04)

typedef struct Stackframe Stackframe;

struct Stackframe {
	/* we explicity push these registers */
	struct {
		uint32_t r4;
		uint32_t r5;
		uint32_t r6;
		uint32_t r7;
		uint32_t r8;
		uint32_t r9;
		uint32_t r10;
		uint32_t r11;
		uint32 excret;	/* pend sv handler's lr */
	} sw_frame;

	/* these registers are pushed by the hardware */
	struct {
		uint32_t r0;
		uint32_t r1;
		uint32_t r2;
		uint32_t r3;
		uint32_t r12;
		void *lr;
		void *pc;
		uint32_t psr;
	} hw_frame;
};

static void
exittask(void)
{
	taskexit(0);
}

void
taskcm_setuptask(Task *t, void (*handler)(void*), void* arg)
{
	Stackframe *f;

	/* set sp to the start of the stack (highest address) */
	t->sp = (uint8_t *)t->stk + t->stksize;

	/* initialize the start of the stack as if it had been
	 * pushed via a context switch
	 */
	t->sp -= sizeof(Stackframe);
	memset(t->sp, 0, sizeof(Stackframe));

	f = (Stackframe*) t->sp;
	f->sw_frame.excret = 0xFFFFFFFD;
	f->hw_frame.r0 = (uintptr)arg;
	f->hw_frame.lr = exittask;
	f->hw_frame.pc = handler;
	f->hw_frame.psr = 0x21000000;	/* default PSR value */
}

static	void **spfr;	/* nil on first switch from MSP to PSP */
static	void *spto;
void
taskcm_handlependsv(void)
{
	if (spfr == nil) {
		/* This is the first context switch, called from the main stack (MSP);
		 * in that case we do not need to save the context
		 * since we never return to MSP.
		 */
		goto restore;
	}

	/* Save registers to the PSP stack;
	 * additional registers are pushed in hardware
	 */
	__asm__(
		"MRS %0, psp\n"	/* move psp to register */

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
		"TST lr, #(1<<4)	\n"
		"	IT eq	\n"
		"	VSTMdbeq %0!, {s16-s31}	\n"
#endif

		/* save r4-r11, and the EXC_RETURN value (from `lr`) */
		"STMdb %0!, {r4-r11, lr}	\n"

		"MSR psp, %0	\n"	/* move register to psp */
		: "=r" (*spfr)
	);

restore:
	/* Load registers with contents of the PSP stack;
	 * additional registers are popped in hardware
	 */
	__asm__(
		/* Restore r4-r11, and the EXC_RETURN value (in `lr`) */
		"LDMia %0!, {r4-r11, lr}	\n"

#if defined (__VFP_FP__) && !defined(__SOFTFP__)
		/* The LDM instruction above also restored `lr`,
		 * which now again contains the EXC_RETURN value
		 * from the moment the task's context got saved last time.
		 * If EXC_RETURN.FType is 0, FP context was in use;
		 * in this case, registers s16-s31 must be restored manually
		 * (registers s0-s15 will automatically be restored by the controller).
		 */
		"TST lr, #(1<<4)	\n"
		"	IT eq	\n"
		"	VLDMiaeq %0!, {s16-s31}	\n"
#endif

		"MSR psp, %0\n"
		:
		: "r" (spto)
	);
}

void
taskcm_contextswitch(Task *from, Task *to)
{
	if (from != nil) {
		spfr = &from->sp;
	}
	spto = to->sp;
	taskrunning = to;

	/* manually trigger pend_sv */
	ICSR |= (1 << 28);

	__asm__("nop");
	__asm__("nop");
	__asm__("nop");
	__asm__("nop");
}

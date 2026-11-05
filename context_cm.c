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
		uint32_t excret;	/* pend sv handler's lr */
	} sw_frame;

	/* these registers are pushed by the hardware */
	struct {
		uint32_t r0;
		uint32_t r1;
		uint32_t r2;
		uint32_t r3;
		uint32_t r12;
		uint32_t lr;
		uint32_t pc;
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
	f->hw_frame.r0 = (uintptr_t)arg;
	f->hw_frame.lr = (uint32_t)(uintptr_t)exittask;
	f->hw_frame.pc = (uint32_t)(uintptr_t)handler;
	f->hw_frame.psr = 0x21000000;	/* default PSR value */
}

uchar **taskcm_spfr;	/* nil on first switch from MSP to PSP */
uchar *taskcm_spto;

void
taskcm_contextswitch(Task *from, Task *to)
{
	if (from != nil) {
		taskcm_spfr = &from->sp;
	}
	taskcm_spto = to->sp;
	taskrunning = to;

	/* manually trigger pend_sv */
	ICSR |= (1 << 28);

	/* ensure write is complete and pipeline is flushed */
	__asm__ volatile ("dsb\n\tisb\n" : : : "memory");
}

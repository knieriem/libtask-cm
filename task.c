/* Copyright (c) 2005 Russ Cox, MIT; see COPYRIGHT */

#include <u.h>
#include <libc.h>

#include "task.h"
#include "taskimpl.h"

Taskconf *taskconf;

int	taskcount;
int	tasknswitch;
int	taskexitval;
Task	*taskrunning;

Tasklist	taskrunqueue;

static int taskidgen;

static void*
alignptr(void *p, int *sz)
{
	uintptr p0, pa;

	p0 = (uintptr)p;
	pa = (p0 + 7) & ~(uintptr)7;
	*sz -= pa-p0;
	return (void*)pa;
}

static int
setuptask(Task *t, void (*fn)(void*), void *arg, void *stk, int stksize)
{
	if (stk == nil) {
		return -1;
	}
	stk = alignptr(stk, &stksize);
	if (stksize <= 0) {
		return -1;
	}

	t->stk = stk;
	t->stksize = stksize;
	t->id = ++taskidgen;

	if (taskconf->memfill != nil) {
		(*taskconf->memfill)(stk, stksize);
	}

	/* setup stack with initial context */
	taskcm_setuptask(t, fn, arg);

	return 0;
}

int
taskcreate(Task *t, void (*fn)(void*), void *arg, void *stk, uint stksize)
{
	int id;

	if (setuptask(t, fn, arg, stk, stksize) == -1) {
		return -1;
	}
	taskcount++;
	id = t->id;
	taskready(t);
	return id;
}

void
taskready(Task *t)
{
	t->ready = 1;
	addtask(&taskrunqueue, t);
}

int
taskyield(void)
{
	int n;
	
	n = tasknswitch;
	taskready(taskrunning);
	taskswitch();
	return tasknswitch - n - 1;
}

int
anyready(void)
{
	return taskrunqueue.head != nil;
}

void
taskexit(int val)
{
	taskexitval = val;
//	taskrunning->exiting = 1;
	taskcount--;
	taskswitch();
}


static Task *sq;
static int32 sqbasetime;

void
taskswitch(void)
{
	int32 now;
	int32 left;
	Task *t;

//	taskdebug("scheduler enter");
	for(;;){
		if (sq != nil) {
			now = taskconf->usec() - taskconf->usec_uncertainty;

			/* add a task that is done sleeping to the front of the runqueue */
			if (now-sqbasetime >= sq->delay) {
				t = sq;
				sqbasetime += t->delay;
				sq = t->next;
				addtasktofront(&taskrunqueue, t);
			}
		}

//		if(taskcount == 0)
//			exit(taskexitval);
		t = taskrunqueue.head;
		if(t == nil){
			left = 0;
			if (sq != nil) {
				left = sq->delay - (now - sqbasetime);
			}
			taskconf->pauseusecs(left);
			continue;
		}
		deltask(&taskrunqueue, t);
		t->ready = 0;
		if (t == taskrunning) {
			/* t was already running, don't perform a switch */
			break;
		}
		tasknswitch++;
		taskcm_contextswitch(taskrunning, t);
		break;
	}
}

void**
taskdata(void)
{
	return &taskrunning->udata;
}


/*
 * hooray for linked lists
 */
void
addtask(Tasklist *l, Task *t)
{
	if(l->tail){
		l->tail->next = t;
		t->prev = l->tail;
	}else{
		l->head = t;
		t->prev = nil;
	}
	l->tail = t;
	t->next = nil;
}

void
addtasktofront(Tasklist *l, Task *t)
{
	if (l->head) {
		l->head->prev = t;
		t->next = l->head;
	} else {
		l->tail = t;
		t->next = nil;
	}
	l->head = t;
	t->prev = nil;
}

void
deltask(Tasklist *l, Task *t)
{
	if(t->prev)
		t->prev->next = t->next;
	else
		l->head = t->next;
	if(t->next)
		t->next->prev = t->prev;
	else
		l->tail = t->prev;
}

unsigned int
taskid(void)
{
	return taskrunning->id;
}


static void
addsleeptask(Task *t, int32 us)
{
	Task *q, *qprev;
	int32 now;

	now = taskconf->usec();
	t->delay = us;

	if (sq == nil) {
		/* set new base time */
		sqbasetime = now;
	}

	/* add to sleep queue */
	qprev = nil;
	for (q=sq; q != nil; qprev = q, q = q->next) {
		if ((t->delay - q->delay) < 0) {
			/* t will finish earlier than the next - insert here */
			break;
		} else {
			/* t will finish later - adjust delay */
			t->delay -= q->delay;
		}
	}
	if (q != nil) {
		/* cut delay time between this sleep task and the next */
		q->delay -= t->delay;
	}
	t->next = q;
	if (qprev == nil) {
		sq = t;
	} else {
		qprev->next = t;
	}
}

void
taskdelay(int32 us)
{
	addsleeptask(taskrunning, us);
	taskswitch();
}

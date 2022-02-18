/* Copyright (c) 2005 Russ Cox, MIT; see COPYRIGHT */

#ifndef _TASK_H_
#define _TASK_H_ 1

#ifdef __cplusplus
extern "C" {
#endif

/*
 * configuration
 */

typedef const struct Taskconf Taskconf;

struct Taskconf {
	int32 (*usec)(void);
	int32 usec_uncertainty;
	void	(*pauseusecs)(int32);
};
extern	Taskconf	*taskconf;


/*
 * basic procs and threads
 */

typedef struct Task Task;
typedef struct Tasklist Tasklist;

int		anyready(void);
int		taskcreate(Task*, void (*f)(void *arg), void *arg, void *stk, uint stacksize);
void		taskexit(int);
int		taskyield(void);
void		taskswitch(void);
void		needstack(int);
void		taskname(char*, ...);
void		taskstate(char*, ...);
void	taskdelay(int32);
unsigned int	taskid(void);

int	taskstkusage(Task *t, int *free);

struct Task
{
	Task	*next;
	Task	*prev;
	Task	*allnext;
	Task	*allprev;
	uint	id;
	void *sp;
	uchar	*stk;
	uint	stksize;
	int	exiting;
	int	ready;
	void	*udata;
	int32	delay;
};

struct Tasklist	/* used internally */
{
	Task	*head;
	Task	*tail;
};


/*
 * sleep and wakeup (condition variables)
 */
typedef struct Rendez Rendez;

struct Rendez
{
	Tasklist waiting;
};

void	tasksleep(Rendez*);
int	taskwakeup(Rendez*);
int	taskwakeupall(Rendez*);


/*
 * platform specific context switching
 */
extern	void	taskcm_handlependsv(void);



#ifdef __cplusplus
}
#endif
#endif


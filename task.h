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
	int32_t (*usec)(void);
	int32_t usec_uncertainty;
	void	(*pauseusecs)(int32_t);

	/* Memfill: fill memory with a pattern */
	void (*memfill)(uint32_t *p, int size);
};
extern	Taskconf	*taskconf;


/*
 * basic procs and threads
 */

typedef struct Task Task;
typedef struct Tasklist Tasklist;

int		anyready(void);
int		taskcreate(void (*f)(void *arg), void *arg, void *stk, unsigned int stacksize);
void		taskexit(int);
int		taskyield(void);
void		taskswitch(void);
void		needstack(int);
void		taskname(char*, ...);
void		taskstate(char*, ...);
void	taskdelay(int32_t);
unsigned int	taskid(void);

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


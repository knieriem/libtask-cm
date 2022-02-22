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

void	taskready(Task*);
void	taskswitch(void);

void	addtask(Tasklist*, Task*);
void	addtasktofront(Tasklist*, Task*);
void	deltask(Tasklist*, Task*);

extern Task	*taskrunning;


extern	void	taskcm_setuptask(Task *, void (*handler)(void*), void *arg);
extern	void	taskcm_contextswitch(Task *from, Task *to);

`Libtask-cm` is a simple coroutine library for ARM Cortex-M controllers.
Its API and basic structure is based on [libtask].
From the original README:

> Scheduling is cooperative.  Only one task runs at a time,
> and it cannot be rescheduled without explicitly giving up
> the CPU.

Parts of another coroutine library, [cmcm], have been added for Cortex-M support,
extended for being used with controllers containing a floating point unit.
To reduce the number of context switches when tasks are sleeping,
[parts of TinyGo's scheduler] - translated to C - have been integrated.

[libtask]: https://swtch.com/libtask/
[cmcm]: https://github.com/scttnlsn/cmcm
[parts of TinyGo's scheduler]: https://github.com/tinygo-org/tinygo/blob/3883550c446fc8b14115366284316290f3d3c4b1/src/runtime/scheduler.go#L79-L135

`Libtask-cm` has been created for use-cases where a solution like FreeRTOS
is not needed.
For instance, a single-threaded,
state-machine based firmware needs to be extended by a driver that goes to sleep,
or that runs many I²C requests in a loop,
which would block the main application.
In such a case, utilizing `libtask-cm` the driver and the main application would
each run in their own task, with their own stack, scheduled cooperatively.

Differences from the original libtask:

-	There is no `taskmain()`. Programs use the normal `main()` entry point.
	In the main context, still using the MSP stack, initial tasks can be created,
	and the first task can be scheduled using `taskswitch()`.

-	No dynamic memory allocation.  
	`taskcreate`'s signature has been adjusted, by adding a `*stack` argument.

-	There is no scheduler task; instead, next task is always determined during
	`taskyield()`, reducing the number of context switches
	In case there is one main task and a few driver tasks, that normally are asleep,
	no context switches occur 

-	`taskdelay` takes a microsecond, not a millisecond, argument,
	and doesn't have a return value anymore

-	Channels, and QLocks have been left out (may be added later, if needed);
	networking too

-	Support for pc operating systems has been stripped
	(since the focus here is on Cortex-M)


## Basic task manipulation

int taskcreate(void (*f)(void *arg), void *arg, void *stack, unsigned int stacksize);

>	Create a new task running `f(arg)` on a stack of size stacksize.

int taskyield(void);
	
>	Explicitly give up the CPU.  The current task will be scheduled
>	again once all the other currently-ready tasks have a chance
>	to run.  Returns the number of other tasks that ran while the
>	current task was waiting. Zero means there are no other tasks
>	trying to run; in this case, CPU is not given up (no context switch).

void taskdelay(int32 us)

>	Explicitly give up the CPU for at least us microseconds.
>	Other tasks continue to run during this time.

void** taskdata(void);

>	Return a pointer to a single per-task void* pointer.
>	You can use this as a per-task storage place.

unsigned int taskid(void);

>	Return the unique task id for the current task.


### Condition variables

void tasksleep(Rendez*);  
int taskwakeup(Rendez*);  
int taskwakeupall(Rendez*);  

>	A `Rendez` is a condition variable. You can declare a new one by
>	just allocating memory for it (or putting it in another structure)
>	and then zeroing the memory.  Tasksleep(r) _sleeps on r_, giving
>	up the CPU.  Multiple tasks can sleep on a single Rendez.
>	When another task comes along and calls taskwakeup(r), 
>	the first task sleeping on r (if any) will be woken up.
>	Taskwakeupall(r) wakes up all the tasks sleeping on r.
>	They both return the actual number of tasks awakened.


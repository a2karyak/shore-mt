/*<std-header orig-src='shore'>

 $Id: sthread_core_pthread.cpp,v 1.6 2001/06/06 23:18:22 bolo Exp $

SHORE -- Scalable Heterogeneous Object REpository

Copyright (c) 1994-99 Computer Sciences Department, University of
                      Wisconsin -- Madison
All Rights Reserved.

Permission to use, copy, modify and distribute this software and its
documentation is hereby granted, provided that both the copyright
notice and this permission notice appear in all copies of the
software, derivative works or modified versions, and any portions
thereof, and that both notices appear in supporting documentation.

THE AUTHORS AND THE COMPUTER SCIENCES DEPARTMENT OF THE UNIVERSITY
OF WISCONSIN - MADISON ALLOW FREE USE OF THIS SOFTWARE IN ITS
"AS IS" CONDITION, AND THEY DISCLAIM ANY LIABILITY OF ANY KIND
FOR ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.

This software was developed with support by the Advanced Research
Project Agency, ARPA order number 018 (formerly 8230), monitored by
the U.S. Army Research Laboratory under contract DAAB07-91-C-Q518.
Further funding for this work was provided by DARPA through
Rome Research Laboratory Contract No. F30602-97-2-0247.

*/

#include "w_defines.h"

/*  -- do not edit anything above this line --   </std-header>*/


/*
 *   NewThreads is Copyright 1992, 1993, 1994, 1995, 1996, 1997, 1998 by:
 *
 *	Josef Burger	<bolo@cs.wisc.edu>
 *	Dylan McNamee	<dylan@cse.ogi.edu>
 *      Ed Felten       <felten@cs.princeton.edu>
 *
 *   All Rights Reserved.
 *
 *   NewThreads may be freely used as long as credit is given
 *   to the above authors and the above copyright is maintained.
 */

#include <w.h>
#include "sthread.h"
#include <w_stream.h>
#include <pthread.h>
#include "stcore_pthread.h"



#define	USER_STACK_SIZE		(sthread_t::default_stack)
#define	DEFAULT_STACK_SIZE	(USER_STACK_SIZE)


unsigned	sthread_t::stack_size() const
{
	/* XXX give the main thread a !=0 stack size for now */
	return _core->stack_size ? _core->stack_size : default_stack;
}

#ifndef PTHREAD_SEMAPHORE
/* Mimic the posix semaphores so it just works.  They release
   waiters when the count is > 0, sleep if <= 0 */

static	int	sem_init(sthread_core_t::sem_t *sem, int, int count)
{
	/* XXX could bitch if shared was true, but it is just for
	   local compatability */

	sem->count = count;
	pthread_mutex_init(&sem->lock, NULL);
	pthread_cond_init(&sem->wake, NULL);

	return 0;
}

static	void	sem_destroy(sthread_core_t::sem_t *sem)
{
	pthread_mutex_destroy(&sem->lock);
	pthread_cond_destroy(&sem->wake);
}

static	inline	void	sem_post(sthread_core_t::sem_t *sem)
{
	pthread_mutex_lock(&sem->lock);
	sem->count++;
	if (sem->count > 0)
		pthread_cond_signal(&sem->wake);
	pthread_mutex_unlock(&sem->lock);
}

static	inline	void	sem_wait(sthread_core_t::sem_t *sem)
{
	pthread_mutex_lock(&sem->lock);
	while (sem->count <= 0)
		pthread_cond_wait(&sem->wake, &sem->lock);
	sem->count--;
	pthread_mutex_unlock(&sem->lock);
}
#endif


static void *pthread_core_start(void *_arg)
{
	sthread_core_t	*me = (sthread_core_t *) _arg;

	sem_wait(&me->sched);
	me->is_virgin = 0;
	(me->start_proc)(me->start_arg);
	return 0;
}


int sthread_core_init(sthread_core_t *core,
		      void (*proc)(void *), void *arg,
		      unsigned stack_size)
{
	int	n;

	/* Get a life; XXX magic number */
	if (stack_size > 0 && stack_size < 1024)
		return -1;

	core->is_virgin = 1;

	core->start_proc = proc;
	core->start_arg = arg;

	core->stack_size = stack_size;

	core->sched_terminate = false;

	/* The system stack is ready to run, other threads are nascent */
	n = sem_init(&core->sched, 0, 0);
	if (n == -1)
		return -1;
	
	if (stack_size > 0) {
		/* A real thread */
		n = pthread_create(&core->pthread,
			NULL,
			pthread_core_start, core);
		if (n == -1) {
			w_rc_t e= RC(fcOS);
			cerr << "pthread_create():" << endl << e << endl;
			sem_destroy(&core->sched);
			return -1;
		}
	}
	else {
		/* A more elegant solution would be to make a
		   "fake" stack using the kernel stack origin
		   and stacksize limit.   This could also allow
		   the main() stack to have a thread-package size limit,
		   to catch memory hogs in the main thread. */

		/* The system stack is never virgin */
		core->is_virgin = 0;

		core->pthread = pthread_self();
	}

	return 0;
}


extern "C" {
/* A callback to c++ land used by errors in the c-only core code */

void sthread_core_fatal()
{
	W_FATAL(fcINTERNAL);
}
}


void sthread_core_exit(sthread_core_t* core)
{
	void	*join_value;

	/* must wait for the thread and then harvest its's thread
	   and sched semaphore */

	if (core->stack_size > 0) {
		/* Release the victim thread to exit. */
		core->sched_terminate = true;
		sem_post(&core->sched);

		pthread_join(core->pthread, &join_value);
		/* And the thread is gone */
	}
	sem_destroy(&core->sched);
}


void sthread_core_set_use_float(sthread_core_t *, int)
{
}



/* Let the native threads package use its bounds stuff */

int	sthread_core_stack_ok(const sthread_core_t *, int)
{
	return 1;
}

ostream &operator<<(ostream &o, const sthread_core_t &core)
{
	o << "core: ";
	if (core.stack_size == 0)
		W_FORM(o)("[ system thread %#lx ]", (long) core.pthread);
	else
		W_FORM(o)("[ thread %#lx ] size=%d",  
			(long) core.pthread, core.stack_size);
	if (core.is_virgin)
		o << ", virgin-core";
	return o;
}
	

void	sthread_core_switch(sthread_core_t *from, sthread_core_t *to)
{
	/* Release the next pthread thread to run */
	sem_post(&to->sched);

	/* Wait to be released ourself. */
	sem_wait(&from->sched);

	if (from->sched_terminate)
		pthread_exit(0);
}

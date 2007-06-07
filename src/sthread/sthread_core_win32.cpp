/*<std-header orig-src='shore'>

 $Id: sthread_core_win32.cpp,v 1.13 1999/06/07 19:06:13 kupsch Exp $

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
#include <windows.h>
#include "stcore_win32.h"

#if defined(_MT) && !defined(WIN32_THREADS_ONLY)
#include <process.h>
typedef unsigned START_RETURNS;
#else
/* Don't do any C-Library like things for the new thread. */
typedef	DWORD	START_RETURNS;
#endif



#define	USER_STACK_SIZE		(sthread_t::default_stack)
#define	DEFAULT_STACK_SIZE	(USER_STACK_SIZE)


unsigned	sthread_t::stack_size() const
{
	/* XXX give the main thread a !=0 stack size for now */
	return _core->stack_size ? _core->stack_size : default_stack;
}


START_RETURNS	__stdcall win32_core_start(void *_arg)
{
	sthread_core_t	*me = (sthread_core_t *) _arg;
	DWORD what;
	what = WaitForSingleObject(me->sched, INFINITE);
	if (what != WAIT_OBJECT_0) {
		w_rc_t e = RC(fcWIN32);
		cerr << "Sthread core start: WaitForSingleObject():"
			<< endl << e << endl;
		W_COERCE(e);
	}
	me->is_virgin = 0;
	(me->start_proc)(me->start_arg);
	return 0;
}


int sthread_core_init(sthread_core_t *core,
		      void (*proc)(void *), void *arg,
		      unsigned stack_size)
{
	/* Get a life; XXX magic number */
	if (stack_size > 0 && stack_size < 1024)
		return -1;

	core->is_virgin = 1;

	core->start_proc = proc;
	core->start_arg = arg;

	core->stack_size = stack_size;

	/* XXX magic value 8, needs research, perhaps max # threads or CPUs? */
	core->sched = CreateSemaphore(NULL, 0, 8, NULL);
	if (core->sched == INVALID_HANDLE_VALUE) {
		w_rc_t e = RC(fcWIN32);
		cerr << "CreateSemaphore():" << endl << e << endl;;
		return -1;
	}	

	
	if (stack_size > 0) {
		/* A real thread */
#if defined(_MT) && !defined(WIN32_THREADS_ONLY)
		core->win32_thread = (HANDLE) _beginthreadex(NULL, 0,
			win32_core_start, core,
			0,
			&(core->thread_id));
#else
		core->win32_thread = CreateThread(NULL, 0,
			win32_core_start, core,
			0,
			&(core->thread_id));
#endif
		if (core->win32_thread ==  INVALID_HANDLE_VALUE) {
			w_rc_t e= RC(fcWIN32);
			cerr << "CreateThread():" << endl << e << endl;
			CloseHandle(core->sched);
			core->sched = INVALID_HANDLE_VALUE;
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
	
		/* bizarre semantics  of Psuedo Handle versus Real Handle;
		   is there some way to get the handle of the main thread
		   of this process? */
		core->win32_thread = GetCurrentThread();
		core->thread_id = GetCurrentThreadId();
	}

	core->go_away = false;

	return 0;
}


void sthread_core_exit(sthread_core_t* core)
{
	/* must wait for the thread and then harvest its's thread
	   and sched semaphore */
	/* CUrrently, I don't know if the threads package can
	   stand having the main thread exit ... just as in win32,
	   it is the "master control thread" ... when it goes,
	   the process exits. */

	if (core->stack_size > 0) {
#ifdef kill_at_all_cost
	        TerminateThread(core->win32_thread, 1);
#else
		/* Wake the thread up after telling it to die. */
		core->go_away = true;
		BOOL ok;
		ok = ReleaseSemaphore(core->sched, 1, NULL);
		if (!ok) {
			w_rc_t e = RC(fcWIN32);
			cerr << "Sthread core exit: ReleaseSemaphore():"
				<< endl << e << endl;
			W_COERCE(e);
		}
#endif
		DWORD what;
		what = WaitForSingleObject(core->win32_thread, INFINITE);
		if (what != WAIT_OBJECT_0) {
			w_rc_t e = RC(fcWIN32);
			cerr << "Sthread core exit: WaitForSingleObject():"
				<< endl << e << endl;
			W_COERCE(e);
		}
		CloseHandle(core->win32_thread);
	}
	CloseHandle(core->sched);
}


void sthread_core_set_use_float(sthread_core_t* p, int flag)
{
}



/* Let the native threads package use its bounds stuff */

int	sthread_core_stack_ok(const sthread_core_t *, int )
{
	return 1;
}

ostream &operator<<(ostream &o, const sthread_core_t &core)
{
	o << "core: ";
	if (core.stack_size == 0)
		W_FORM(o)("[ system thread id=%#lx handle=%#lx ]",
			(long) core.thread_id,
			(long) core.win32_thread);
	else
		W_FORM(o)("[ id=%#lx handle=%#lx ] size=%d", 
			(long) core.thread_id,
			(long) core.win32_thread, 
			core.stack_size);
	if (core.is_virgin)
		o << ", virgin-core";
	return o;
}
	

void	sthread_core_switch(sthread_core_t *from, sthread_core_t *to)
{
	/* Release the next Win32 thread to run */
	BOOL ok;
	ok = ReleaseSemaphore(to->sched, 1, NULL);
	if (!ok) {
		w_rc_t e = RC(fcWIN32);
		cerr << "Sthread core switch: ReleaseSemaphore():"
			<< endl << e << endl;
		W_COERCE(e);
	}

	/* Wait to be released. */
	DWORD	what;
	what = WaitForSingleObject(from->sched, INFINITE);
	if (what != WAIT_OBJECT_0) {
		w_rc_t e = RC(fcWIN32);
		cerr << "Sthread core switch: WaitForSingleObject():"
			<< endl << e << endl;
		W_COERCE(e);
	}
	if (from->go_away) {
#if defined(_MT) && !defined(WIN32_THREADS_ONLY)
		_endthreadex(0);
#else
		ExitThread(0);
#endif
	}
}

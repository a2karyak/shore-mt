/*<std-header orig-src=SHORE>

 $Id: sthread_core_win32f.cpp,v 1.1 2002/01/06 07:56:38 bolo Exp $

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

  -- do not edit anything above this line --   </std-header>*/

/*
 *   NewThreads is Copyright 1992, 1993, 1994, 1995, 1996, 1997, 1998,
 *   1999, 2000, 2001 by:
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
#include "stcore_win32f.h"
// #include <winnt.h>

/*
 * Fiber's have a "parameter" which are passed to them and which they can
 * reference via GetFiberData().   I use this parameter as the address of
 * the thread core for that Fiber.   This allows easy Fiber->core->thread
 * mapping.   If we went to multiple CPUs, this could be used for
 * determining the current Fiber.  However, Fibers have some limitations
 * about which threads can run them, so this isn't something to worry about.
 */

/* XXX
 * It is possible to control whether a Fiber switches the floating point 
 * context or not.  However, this is a Fiber-creation time switch, not
 * a runtime switch.  Actually it makes little sense for NewThreads/sthreads
 * to allow that parameter to be switched, since it really is a constructor
 * specified operation that MUST happen before the thread is ever started,
 * and can not change afterwards.   Once the thread cores move to c++ and
 * have constructors this will be in better shape and perhaps we can take
 * advantage of it here.
 */


#define	USER_STACK_SIZE		(sthread_t::default_stack)

#define	DEFAULT_STACK_SIZE	(USER_STACK_SIZE)


unsigned	sthread_t::stack_size() const
{
	/* XXX give the main thread a !=0 stack size for now */
	return _core->stack_size ? _core->stack_size : default_stack;
}


static void	__stdcall win32_core_start(void *_arg)
{
	sthread_core_t	*me = (sthread_core_t *) _arg;

	me->is_virgin = 0;
	me->sp0 = &_arg;
	(me->start_proc)(me->start_arg);
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

	if (stack_size > 0) {
		/* A real thread */
		core->fiber = CreateFiber(stack_size, win32_core_start, core);
		if (!core->fiber) {
			w_rc_t e= RC(fcWIN32);
			cerr << "CreateFiber():" << endl << e << endl;
			return -1;
		}
#ifdef CORE_DEBUG
		cout << "CreateFiber: " << core->fiber << endl;
#endif
	}
	else {
		/* A more elegant solution would be to make a
		   "fake" stack using the kernel stack origin
		   and stacksize limit.   This could also allow
		   the main() stack to have a thread-package size limit,
		   to catch memory hogs in the main thread. */

		/* The system stack is never virgin */
		core->is_virgin = 0;

#if 0
		/* XXX left for bolo debugging, code not available */
		extern void dump_win32_cpu(ostream &);
		dump_win32_cpu(cout);
#endif
		core->fiber = ConvertThreadToFiber(&core);
		if (!core->fiber) {
			w_rc_t e = RC(fcWIN32);
			cerr << "ConvertThreadToFiber():" << endl
				<< e << endl;
			return -1;
		}
#ifdef CORE_DEBUG
		cout << "RootFiber: " << core->fiber << endl;
#endif
	}

	return 0;
}


void sthread_core_exit(sthread_core_t* core)
{
	if (core->stack_size > 0) {
		DeleteFiber(core->fiber);
		core->fiber = 0;
	}
	else {
		/* it is the main thread */
		/* XXX this/can only be called when shutting down, since once
		   the Fiber context is removed no more context switches
		   can happen.   However this is necessary to clean up
		   resources. */

#ifdef notyet
		/* XXX notyet because the function doesn't exist in
		   earlier versions of NT which support Fibers.   Leaving it
		   unused allows Fiber cores to work on earlier NT platforms.
		   I'll enable this eventually when it is more common than
		   not and my Win32 development box supports it :) */

		bool	ok;
		ok = ConvertFiberToThread();
		if (!ok) {
			w_rc_t	e = RC(fcWIN32);
			cerr << "ConvertFiberToThread():" << endl << e << endl;
			W_COERCE(e);
		}
		/* Fiber context deleted when Fiber is converted */
		core->fiber = 0;
#endif
	}
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
		W_FORM(o)("[ system thread fiber=%#lx ]",
			(long) core.fiber);
	else
		W_FORM(o)("[ fiber=%#lx sp0=%#lx] size=%d", 
			(long) core.fiber,
			(long) core.sp0,
			core.stack_size);
	if (core.is_virgin)
		o << ", virgin-core";
	return o;
}
	

void	sthread_core_switch(sthread_core_t *from, sthread_core_t *to)
{
#if 1
	/* XXX The Win32 docs say never to SwitchToFiber(GetCurrentFiber()).
	   The test for that is commented out here since NewThreads/sthreads
	   never does that -- the context switch is a no-op and it is optimized
	   out */

	void	*onFiber = GetCurrentFiber();
	if (onFiber == to->fiber) {
		cerr << "warning: sthread_core_switch(" << W_ADDR(to) << "): "
		     << "to fiber is current fiber"
		     << endl;
	}
#endif
	   
	SwitchToFiber(to->fiber);
}

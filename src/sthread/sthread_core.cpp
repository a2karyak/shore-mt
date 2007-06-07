/*<std-header orig-src='shore'>

 $Id: sthread_core.cpp,v 1.37 1999/08/03 15:55:49 bolo Exp $

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
 *   NewThreads is Copyright 1992, 1993, 1994, 1995, 1996, 1997 by:
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

/*
 * The base thread functionality of Shore Threads is derived
 * from the NewThreads implementation wrapped up as c++ objects.
 */

#define STHREAD_CORE_C

#include <w.h>
#include "sthread.h"
#include "stcore.h"

#ifdef PURIFY
#include <purify.h>
#endif


static void sthread_core_stack_setup(sthread_core_t *core);


/*
   Bolo's note about "zone"s and "redzone"s on thread stacks.  I use the
   "zone" terminology for two things:
   1) A zone of unused space between thread stacks.  This keeps
   stacks far enough apart that purify doesn't think one thread
   is using another thread's stack.
   2) A redzone at the "far" end of the thread stack.  This is used
   to check for stack overflow at runtime.  An intrusion of the stack
   pointer into this region, or a corruption of the pattern stored
   in this region is noted as a stack overflow.
 */

#if defined(PURIFY) || defined(PURIFY_ZERO)
/* This is not just PURIFY_ZERO, 'cause the system just will not work
  under purify without it. */

/* If stacks are too close together, purify thinks that any data is in
   a, beyond, or ahead of the thread stack.  Placing a buffer
   zone on each end of the stack prevents most of this problem.  This
   allows purifying the code without bogus purify events. */

#define	ZONE_STACK_SIZE		(16*1024)
#else
#define	ZONE_STACK_SIZE		0
#endif

#define	USER_STACK_SIZE		(sthread_t::default_stack)

#define	DEFAULT_STACK_SIZE	(2*ZONE_STACK_SIZE + USER_STACK_SIZE)


unsigned	sthread_t::stack_size() const
{
	/* XXX give the main thread a !=0 stack size for now */
	return _core->stack_size ? _core->stack_size : default_stack;
}


#ifdef STHREAD_FASTNEW_STACK
/* Fastnewing thread stacks really drives up memory use without
   any definite performance benefit. */
struct thread_stack {
public:
    char	data[DEFAULT_STACK_SIZE];
    W_FASTNEW_CLASS_PTR_DECL(thread_stack);
};
W_FASTNEW_STATIC_PTR_DECL(thread_stack);
#endif
// force sthread_init_t to be constructed after thread_stack fastnew
static sthread_init_t sthread_init;


int sthread_core_init(sthread_core_t *core,
		      void (*proc)(void *), void *arg,
		      unsigned stack_size)
{
#ifdef STHREAD_FASTNEW_STACK
	if (thread_stack::_w_fastnew == 0) {
		W_FASTNEW_STATIC_PTR_DECL_INIT(thread_stack, 4);
		if (thread_stack::_w_fastnew == 0)
			W_FATAL(fcOUTOFMEMORY);
	}
#endif

	/* Get a life; XXX magic number */
	if (stack_size > 0 && stack_size < 1024) {
		return -1;
	}

	core->is_virgin = 1;

	core->start_proc = proc;
	core->start_arg = arg;

	core->use_float = 1;

#ifdef STHREAD_FASTNEW_STACK
	if (stack_size == USER_STACK_SIZE) {
		thread_stack *ts = new thread_stack;
		if (!ts) {
			cerr << "sthread_core_init failed" <<endl;
			return -1;
		}
		core->stack = ts->data + ZONE_STACK_SIZE;
	}
	else if (stack_size > 0) {
		char *ts = new char[stack_size + 2*ZONE_STACK_SIZE];
		if (!ts) {
			cerr << "sthread_core_init failed" <<endl;
			return -1;
		}
		core->stack = ts + ZONE_STACK_SIZE;
	}
	else
		core->stack = 0;
#else
	if (stack_size > 0) {
		char *ts = new char[stack_size + 2*ZONE_STACK_SIZE];
		if (!ts) {
			cerr << "sthread_core_init failed" <<endl;
			return -1;
		}
		core->stack = ts + ZONE_STACK_SIZE;
	}
	else
		core->stack = 0;
#endif
	core->stack_size = stack_size;

	sthread_core_stack_setup(core);

	if (stack_size > 0) {
		core->stack0 = (core->stack +
				((stack_grows_up ? 32 :
				  core->stack_size - 32)));
		core->save_sp = core->stack0;
	}
	else {
		/* A more elegant solution would be to make a
		   "fake" stack using the kernel stack origin
		   and stacksize limit.   This could also allow
		   the main() stack to have a thread-package size limit,
		   to catch memory hogs in the main thread. */
		core->stack0 = core->save_sp = core->stack;

		/* The system stack is never virgin */
		core->is_virgin = 0;
	}

	return 0;
}


/* A callback to c++ land used by errors in the c-only core code */

void sthread_core_fatal()
{
	W_FATAL(fcINTERNAL);
}


void sthread_core_exit(sthread_core_t* core)
{
#ifdef STHREAD_FASTNEW_STACK
	if (core->stack_size == USER_STACK_SIZE) {
		thread_stack *ts = (thread_stack *) (core->stack - ZONE_STACK_SIZE);
		delete ts;
	}
	else if (core->stack_size > 0) {
		char	*ts = core->stack - ZONE_STACK_SIZE;
		delete [] ts;
	}
#else
	if (core->stack_size > 0) {
		char	*ts = core->stack - ZONE_STACK_SIZE;
		delete [] ts;
	}
#endif
}


void sthread_core_set_use_float(sthread_core_t* p, int flag)
{
	p->use_float = flag ? 1 : 0;
}


/*
 * address_in_stack(core, addr)
 *
 * Indicate if the specified address is in the stack of the thread core
 */  

bool address_in_stack(sthread_core_t &core, void *address)
{
	if (core.stack_size == 0)
		return false;

	return ((char *)address >= core.stack &&
		(char *)address < core.stack + core.stack_size);
}



/* ZONE_USED / ZONE_DIV == portion of stack with redzone pattern */
#define	ZONE_DIV	8
#define	ZONE_USED	1

static inline void redzone(const sthread_core_t *core, char **start, int *size)
{
	int	zone_size = (ZONE_USED * core->stack_size) / ZONE_DIV;
	char	*zone;

	zone = core->stack;

	if (stack_grows_up)
		zone += (ZONE_DIV - ZONE_USED) * zone_size;

	*start = zone;
	*size = zone_size;
}


#define ZONE_PATTERN	0xdeaddead 

/*
   Install a pattern on the last 3/4 of the stack, so we can examine
   it to see if anything has been written into the area
 */  

static void sthread_core_stack_setup(sthread_core_t *core)
{
	if (core->stack_size == 0)
		return;

#if defined(W_DEBUG)   || STHREAD_STACK_CHECK > 1
#ifdef PURIFY
	/* Stack checks don't work when purify is used, it does
	   it for us. */
	if (purify_is_running())
		return;
#endif

	char		*zone;
	int		zone_size;
	int		*izone;
	unsigned	i;

	redzone(core, &zone, &zone_size);
	izone = (int *)zone;
	
	for (i = 0; i < zone_size/sizeof(int); i++)
		izone[i] = ZONE_PATTERN;
#endif
}


/*
   Determine if the thread stack is ok:
   1) stack pointer within region
   2) redzone secure
 */  

int	sthread_core_stack_ok(const sthread_core_t *core, int onstack)
{
	char	*zone;
	int	zone_size;
	char	*sp;		/* bogus manner of retrieving current sp */
	int	overflow;

	if (core->stack_size == 0) 
		return 1;

	redzone(core, &zone, &zone_size);

	if (onstack)
		sp = (char *) &sp;
	else
		sp = core->save_sp;

	if (stack_grows_up)
		overflow = (sp > zone)?1 : 0;
	else
		overflow = (sp < (zone + zone_size))?1 : 0;

	if (overflow!=0) {
		cerr << "obvious overflow" << endl;
#define  want_more_info
#ifdef want_more_info			
		W_FORM(cerr)("core %#x, zone %#x, sp %#x\n",
			  (long)core,
			  (long)zone,
			    sp);

#endif
		return 0;
	}

#if defined(W_DEBUG) || STHREAD_STACK_CHECK > 1
	/* Only do expensive checking on demand. */
	if (onstack == 1)
		return	1;

#ifdef PURIFY
	/* stack checks don't work if purify is running */
	if (purify_is_running())
		return 1;
#endif
	unsigned int	*izone = (unsigned int *)zone;
	unsigned int	i;
	
	for (i = 0; i < zone_size / sizeof(int); i++)
		if (izone[i] != ZONE_PATTERN) {

#ifdef want_more_info			
		W_FORM(cerr)
		("zone [%#x - %#x] corrupt at addr %#x, word %d of %d\n",
		      (long)zone, 
		      ((char *)zone + zone_size), 
			&izone[i], 
		      i, zone_size/sizeof(int)
		);
		W_FORM(cerr)("corrupt zone: word contains %#x\n", izone[i]);
#endif
			return 0;
		}
#endif

	return 1;
}

/* XXX this must be done on-stack.  Would need to add an 'onstack' to 
   allow arbitrary use, since the thread core has no idea if this is
   the core that is being run on. */
int	sthread_core_stack_frame_ok(const sthread_core_t *core,
				    unsigned frame, int onstack)
{
	char	*zone;
	int	zone_size;
	char	*sp;		/* bogus manner of retrieving current sp */
	int	ok = 1;


	/* The system stack can always expand */
	if (core->stack_size == 0) 
		return 1;

	redzone(core, &zone, &zone_size);

	if (onstack)
		sp = (char *) &sp;
	else
		sp = core->save_sp;
	
	if (stack_grows_up)
		ok = (sp + frame) < zone;
	else
		ok = (sp - frame) > (zone + zone_size);

	return ok;
}


ostream &operator<<(ostream &o, const sthread_core_t &core)
{
	o << "core: ";
	if (core.stack_size == 0)
		W_FORM(o)("[ system stack, sp=%#lx ]", (long) core.save_sp);
	else {
		W_FORM(o)("[ %s=%#lx ... sp=%#lx ... %s=%#lx ] size=%d, used=%d",  
		       stack_grows_up ? "bottom" : "max",
		       (long)core.stack,
		       (long)core.save_sp,
		       stack_grows_up ? "max" : "bottom",
		       (long)(core.stack + core.stack_size -1),
		       core.stack_size,
		       stack_grows_up ?
		       (long)core.save_sp - (long)core.stack :
		       (long)core.stack + core.stack_size - (long)core.save_sp
		       );
	}

	if (core.is_virgin)
		o << ", virgin-core";
	return o;
}


/*<std-header orig-src='shore' incl-file-exclusion='STHREAD_GCC3_H'>

 $Id: sthread_gcc3.h,v 1.1 2003/12/29 20:50:26 bolo Exp $

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

#ifndef STHREAD_GCC3_H
#define STHREAD_GCC3_H

#include "w_defines.h"

/*  -- do not edit anything above this line --   </std-header>*/

/*
 *   NewThreads is Copyright 1992, 1993, 1994, 1995, 1996, 1997, 1998,
 *                           1999, 2000, 2001, 2002, 2003 by:
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

/* This is an experiment in having newthreads interact with the
   gcc exception handling mechanism.   The best solution would be
   to have sthreads maintain the gcc exception handling context
   on a per-thread basis.   However that is not practicable because
   the GNU people have provided a poor (actually no) interface for
   conext management.   What is provided is compiled into the gcc
   runtime, not indirected into a thread-package-specific interface.

   What this exception handling does is to copy in/out the contents
   of the static gcc exception context provided for single threads.
   If newthreads is running on a single thread of a multiple threaded
   process it will copy in/out the dynamic context for the underlying
   CPU thread.

   In addition to swapping the static context, there is a per-thread
   "chain" of try/catch handlers and "cleanups" which are pointed
   to by the thread.  Each NewThread exception context starts with
   an emtpy one of these (base_dynamic_handler) which is pointed
   to by the per-thread exception context.  The main thread's
   exception context just uses whatever was provided by the system */

#if W_GCC_THIS_VER < W_GCC_VER(3,0)
#error	"gcc3 Exception handling not supported prior to gcc-3.0"
#endif

/*
 * The nice thing about gcc3 exception handling is that it looked like
 * there are actual user-visible exception interfaces .. including
 * a user-visible API to use.   Way better than the gcc2 approach where
 * it was all hidden under the covers.
 * 
 * Unfortunately, it is just better APIed within GCC, there is no external
 * interface for it still.   So, just like the gcc2 handling, it is
 * a bit of duplicating info that should be available.
 *
 * The thread interaction problem mentioned in sthread_gcc.h still exists.
 */

#ifdef notyet
/* XXX if everything was visible, we could just use the real structures! */
#include <unwind-cxx.h>
typedef struct __cxa_eh_globals	gcc_exception_t;
#else
struct gcc_exception_t {
	void		*caughtExceptions;
	unsigned	uncaughtExceptions;

	gcc_exception_t()
	: caughtExceptions(0), uncaughtExceptions(0)
	{ }
};
#endif

/* XXX could use _fast_ variant if a earlier call to the slow
   function can be generated in the thread system startup. */

/* Get the pointer to the space allocated for this CPUs exception context. */
extern "C" gcc_exception_t *__cxa_get_globals();


inline void sthread_exception_switch(gcc_exception_t	*from,
				     gcc_exception_t	*to)
{
	gcc_exception_t	&context = *__cxa_get_globals();	

	*from = context;
	context = *to;
}

/*<std-footer incl-file-exclusion='STHREAD_GCC3_H'>  -- do not edit anything below this line -- */

#endif          /*</std-footer>*/

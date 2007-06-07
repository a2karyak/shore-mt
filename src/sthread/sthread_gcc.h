/*<std-header orig-src='shore' incl-file-exclusion='STHREAD_GCC_H'>

 $Id: sthread_gcc.h,v 1.2 2003/12/29 20:50:26 bolo Exp $

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

#ifndef STHREAD_GCC_H
#define STHREAD_GCC_H

#include "w_defines.h"

/*  -- do not edit anything above this line --   </std-header>*/

/*
 *   NewThreads is Copyright 1992, 1993, 1994, 1995, 1996, 1997, 1998,
 *                           1999, 2000, 2001, 2002 by:
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

/* EGCS has excpetion handling which has been tested, earlier ones
   have no testing. */
   
#if W_GCC_THIS_VER < W_GCC_VER(2,90)
#error	"Exception handling not supported prior to EGCS (2.90)"
#endif

#if W_GCC_THIS_VER >= W_GCC_VER(2,95)
/*
 * Older gcc's with exception handling, such as
 *
 *	gcc version egcs-2.91.66 19990314 (egcs-1.1.2 release)
 *
 * have a smaller 3 word context (no table_index).  The first
 * gcc which shore supports which has a larger context are the
 * gcc-2.95 series.
 */
#define	GCC_CONTEXT_HAS_TABLE
#endif

struct gcc_exception_t {
	struct eh_cleanup {
		eh_cleanup	*next;
		void		(*func)(void *);
		void		*arg;
	};

	/* derive from this for actual contexts which need state */
	struct eh_handler {
		eh_handler	*next;
		eh_cleanup	*cleanups;

		eh_handler()
		: next(0),
		  cleanups(0)
		{
		}
	};
	
	struct eh_context {
		void			*handler_label;
		eh_handler		*dynamic_handler_chain;
		void			*info;
#ifdef GCC_CONTEXT_HAS_TABLE
		void			*table_index;
#endif

		eh_context()
		: handler_label(0),
		  dynamic_handler_chain(0),
		  info(0)
#ifdef GCC_CONTEXT_HAS_TABLE
		  , table_index(0)
#endif
		{
		}

		inline eh_context &operator=(const eh_context &r)
		{
#if 1
			/* inline memcpy for speed */
			handler_label = r.handler_label;
			dynamic_handler_chain = r.dynamic_handler_chain;
			info = r.info;
#ifdef GCC_CONTEXT_HAS_TABLE
			table_index = r.table_index;
#endif
#else
			memcpy(this, &r, sizeof(*this));
#endif
			return *this;
		}

		ostream &print(ostream &o) const
		{
			o << "eh_context(@" << this
			  << ", hl = " << handler_label
			  << ", dhc = " << dynamic_handler_chain
			  << ", info = " << info
#ifdef GCC_CONEXT_HAS_TABLE
			  << ", table = " << table_index
#endif
			  << ')';
			return o;
		}
	};

	eh_context		context;
	eh_handler		top_elt;


	gcc_exception_t()
	{
		context.dynamic_handler_chain = &top_elt;
	}

	ostream &print(ostream &o) const;
};

ostream &operator<<(ostream &o, const gcc_exception_t::eh_context &ex)
{
	return ex.print(o);
}

ostream &gcc_exception_t::print(ostream &o) const
{
	return o << context;
}

ostream &operator<<(ostream &o, const gcc_exception_t &ex)
{
	return ex.print(o);
}


/* It is actually a 'void *', but this works. */
extern "C" gcc_exception_t::eh_context *__get_eh_context();


inline void sthread_exception_switch(gcc_exception_t	*from,
				     gcc_exception_t	*to)
{
	    gcc_exception_t::eh_context &context = *__get_eh_context();

	    from->context = context;
	    context = to->context;
}

/*<std-footer incl-file-exclusion='STHREAD_GCC_H'>  -- do not edit anything below this line -- */

#endif          /*</std-footer>*/

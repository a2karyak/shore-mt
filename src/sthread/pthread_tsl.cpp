/*<std-header orig-src='shore'>

 $Id: pthread_tsl.cpp,v 1.1 2002/01/04 06:31:41 bolo Exp $

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
 * The Testandset package is Copyright 1993, 1994, 1995, 1997,
 * 1998, 1999, 2000, 2001 by:
 *
 *	Josef Burger	<bolo@cs.wisc.edu>
 *
 * All rights reserved.
 *
 * The Testandset package may be freely used as long as credit is given
 * to the author and the above copyright is maintained. 
 */

/*
 * testandset interface to aix atomic operation stuff
 */

#include "tsl.h"
#include <w_base.h>
#include <iostream.h>
#include <unix_error.h>	

#if 1
/* There is no unlocked examine functionality available with Posix
   spinlocks.   Examine is used for examining the state and NOT to
   do synchronization with.   By preference it should not affect the
   state of the spinlock any, because that could change observed
   behavior if a debugging tool uses examine().   This is a hack around
   that problem.   It looks at the memory location to see if it is
   locked.   testandset_init() also verifies that the unlocked state is
   what this code expects. */

#define	TSL_EXAMINE_READ_ONLY
#endif

extern "C" {

unsigned tsl(tslcb_t *addr, int )
{
	int	n;

	n = pthread_spin_trylock(&addr->lock);
	if (n == -1 && errno != EBUSY) {
		w_rc_t	e= RC(fcOS);
		cerr << "pthread_spin_trylock():" << endl
		     << e << endl;
		W_COERCE(e);
	}

	return n == 0 ? 0 : 1;
}

void tsl_release(tslcb_t *addr)
{
	int	n;

	n = pthread_spin_unlock(&addr->lock);
	if (n == -1) {
		w_rc_t	e= RC(fcOS);
		cerr << "pthread_spin_unlock():" << endl
		     << e << endl;
		W_COERCE(e);
	}
}

unsigned tsl_examine(tslcb_t *addr)
{
	unsigned	busy = 0;

#ifdef TSL_EXAMINE_READ__ONLY
	/* To be good citizens, the only way to determine if a posix
	   spinlock is set is to try locking it.  That of course
	   is an invasive operation, unlink the tsl_() semantics that
	   examine extracts what value is there now ... for
	   non-synchronization purposes.    On most boxes
	   we can cheat a bit, but it will probably get us in the end.

	   XXX if you are here, just change the above #if 1 to a #if 0
	   and it will work on any platform.  */

	busy = (addr->lock == 0);
#else
	int	n;

	n = pthread_spin_trylock(&addr->lock);
	if (n == -1 && errno != EBUSY) {
		w_rc_t	e= RC(fcOS);
		cerr << "pthread_spin_trylock():" << endl
		     << e << endl;
		W_COERCE(e);
	}
	busy = (n == 0);

	if (n == 0) {
		/* We got the lock ... now release it */
		n = pthread_spin_unlock(&addr->lock);
		if (n == -1) {
			w_rc_t	e= RC(fcOS);
			cerr << "pthread_spin_unlock():" << endl
			     << e << endl;
			W_COERCE(e);
		}
	}
#endif
	return busy;
}

void tsl_init(tslcb_t *addr)
{
	int	n;

	/* shared, since spinlocks are used by shore for diskrw. */ 
	n = pthread_spin_init(&addr->lock, 1);
	if (n == -1) {
		w_rc_t	e= RC(fcOS);
		cerr << "pthread_spin_init():" << endl
		     << e << endl;
		W_COERCE(e);
	}
#ifdef TSL_EXAMINE_READ_ONLY
	/* verify that earlier assumptions are true on this system */
	if (addr->lock == 0) {
		cerr << "tsl_init(): read-only hack isn't compatible with"
		     << " this platform." << endl
		     << "\tdisable it in posix_tsl.cpp" << endl;
		W_FATAL(fcINTERNAL);
	}
#endif
}

void	tsl_finish(tslcb_t *addr)
{
	int	n;

	n = pthread_spin_destroy(&addr->lock);
	if (n == -1) {
		w_rc_t	e= RC(fcOS);
		cerr << "pthread_spin_destroy():" << endl
		     << e << endl;
		W_COERCE(e);
	}
}

}

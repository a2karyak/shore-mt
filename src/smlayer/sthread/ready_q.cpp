/*<std-header orig-src='shore'>

 $Id: ready_q.cpp,v 1.8 1999/06/07 19:06:01 kupsch Exp $

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

#include <w.h>
#include <w_debug.h>
#include "sthread.h"
#include "ready_q.h"


ready_q_t::ready_q_t() : count(0)
{
    for (int i = 0; i < nq; i++)  {
	head[i] = tail[i] = 0;
    }
}

bool			
ready_q_t::contains_only(sthread_t *t) const	
{ 
     return (count == 0) ||
	((count == 1) && t && (t->status() == sthread_t::t_ready));
}


/*
 *  Dequeue and return a thread from the highest priority queue
 *  that is not empty. If all queues are empty, return 0.
 */

sthread_t *ready_q_t::get()
{
    sthread_t* t = 0;
    /*
     *  Find the highest queue that is not empty
     */
    int	i;
    for (i = nq-1; i >= 0 && !head[i]; i--)
	;
    if (i >= 0)  {
	/*
	 *  We found one. Dequeue the first entry.
	 */
	t = head[i];
	w_assert1(count > 0);
	--count;
	head[i] = t->_ready_next;
	if(head[i]==0) tail[i] = 0;
	t->_ready_next = 0;
    }
    return t;
}


/*
 *  Insert t into the queue of its priority.
 */

#ifdef W_DEBUG
void ready_q_stop() {}
#endif

void
ready_q_t::put(sthread_t* t)
{
    t->_ready_next = 0;
    int i = t->priority();	// i index into the proper queue
    w_assert3(i >= 0 && i < nq);
    ++count;
#ifdef W_DEBUG
    if(count > 1) {
	// a place to stop in dbg
	ready_q_stop();
    }
#endif
    if (head[i])  {
	w_assert3(tail[i]->_ready_next == 0);
	tail[i] = tail[i]->_ready_next = t;
    } else {
	head[i] = tail[i] = t;
    }
}

/*
 *  dequeue t, change its priority to p, and re-queue it
 *  that is not empty. If all queues are empty, return 0.
 */

void
ready_q_t::change_priority(sthread_t* target, sthread_t::priority_t p)
{
    sthread_t* t = 0, *tp=0;
    /*
     *  Find the first (highest) queue that is not empty
     */
    for (register int i = nq-1 ;i >= 0; i--) {
	/*
	 *  We found a nonempty queue.
	 *  Search it for our target thread.
	 */
	for(tp=0, t=head[i]; t; tp=t ,t=t->_ready_next) {
	    if(t==target) {
		/*
		 * FOUND
		 * tp points to previous item on the list
		 * dequeue it.
		 */
		if(tp) {
		    tp->_ready_next = t->_ready_next;
		    if (t == tail[i])
			    tail[i] = tp;
		} else {
		    w_assert3(t == head[i]);
		    head[i] = t->_ready_next;
		    if(head[i]==0) tail[i] = 0;
		}
		--count;
		t->_ready_next = 0;

		// Set the new priority level
		{
		    // set status so that set_priority doesn't complain
		    sthread_t::status_t old_status = t->status();
		    t->_status = sthread_t::t_defunct;
		    W_COERCE( t->set_priority(p) );
		    t->_status = old_status;
		}

		/*
		 * put it back, at the new priority
		 */
		put(t);
		return;
	    }
	}
    }
#ifdef W_DEBUG
    /* This isn't an error, but something may be mixed up if the
       thread is expected to be in the ready Q */
    W_FORM2(cerr, ("change_priority can't find thread %#lx(%s)\n",
	      target, target->name()));
#endif
}

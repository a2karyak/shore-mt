/*<std-header orig-src='shore' incl-file-exclusion='READY_Q_H'>

 $Id: ready_q.h,v 1.5 1999/06/07 19:06:01 kupsch Exp $

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

#ifndef READY_Q_H
#define READY_Q_H

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
 *  The ready queue.
 *  The queue is really made up of multiple queues, each serving
 *  a priority level. A get() operation dequeues threads from the
 *  highest priority queue that is not empty.
 */

class sthread_t;

class ready_q_t : public sthread_base_t {
public:
	ready_q_t();
	~ready_q_t()   {}

	sthread_t 	*get();
	void		put(sthread_t* t);
	void		change_priority(sthread_t*, sthread_t::priority_t p);

	bool		is_empty() const { return count == 0; }
	bool		contains_only(sthread_t *t) const;

private:
	enum	{ nq = sthread_t::max_priority + 1 };

	unsigned	count; 
	sthread_t	*head[nq];
	sthread_t	*tail[nq];
};

/*<std-footer incl-file-exclusion='READY_Q_H'>  -- do not edit anything below this line -- */

#endif          /*</std-footer>*/

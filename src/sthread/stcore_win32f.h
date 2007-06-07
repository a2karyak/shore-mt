/*<std-header orig-src=SHORE incl-file-exclusion=STCORE_WIN32_H>

 $Id: stcore_win32f.h,v 1.1 2004/01/21 04:13:06 bolo Exp $

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

#ifndef STCORE_WIN32_H
#define STCORE_WIN32_H


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

typedef struct sthread_core_t {
	int	is_virgin;	/* TRUE if just started */
	void	(*start_proc)(void *);		/* thread start function */
	void	*start_arg;	/* argument for start_proc */
	int	stack_size;	/* stack size */
	void	*thread;	/* sthread which uses this core */
	void	*sp0;

	void	*fiber;
} sthread_core_t;


extern int  sthread_core_init(sthread_core_t* t,
			      void (*proc)(void *), void *arg,
			      unsigned stack_size);

extern void sthread_core_exit(sthread_core_t *t);

extern void sthread_core_set_use_float(sthread_core_t *core, int flag);

extern void sthread_core_switch(sthread_core_t *from, sthread_core_t *to);

extern int sthread_core_stack_ok(const sthread_core_t *core, int onstack);

extern ostream &operator<<(ostream &o, const sthread_core_t &c);

extern int stack_grows_up;
extern int minframe;

/*<std-footer incl-file-exclusion=STCORE_WIN32_H>  -- do not edit anything below this line -- */

#endif          /*</std-footer>*/

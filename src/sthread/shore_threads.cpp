/*<std-header orig-src='shore'>

 $Id: shore_threads.cpp,v 1.10 2006/01/29 23:27:17 bolo Exp $

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

/* shore_threads.cpp */
/*
 * Implementation of the Pure thread interface for
 * the shore threads package.
 *
 * The information contained in this file is proprietary to Pure
 * Software and may not be distributed.
 */

#include "pure_threads.h"

#include "sthread.h"
#include <new>

#ifdef __cplusplus
/* Make the external linkage as "C" for purify */
extern "C" {
#endif

/*
 * Locking interface.
 *
 * Use smutex_t's which are allocated by purify.
 * If they are un-named, this removes memory leak problems.
 * If they are named, bzzt problems.
 */


/* The cooperative threads implementation doesn't force
   purify to use locking */
unsigned int pure_use_locking = 0;

unsigned int
pure_lock_size()
{
	return (unsigned int) sizeof(smutex_t);
}

void
pure_initialize_lock(void *lock)
{
	smutex_t	*slock;
	static const char * lock_names[] = {
		"Pure runtime lock 0",
		"Pure runtime lock 1",
		"Pure runtime lock 2",
		"Pure runtime lock 3",
		"Pure runtime lock 4",
		"Pure runtime lock 5",
		"Pure runtime lock 6",
		"Pure runtime lock 7",
		"Pure runtime lock 8",
		"Pure runtime lock 9"
	};
	static int next_lock_name_number = 0;

	const char *name = lock_names[next_lock_name_number++];

#ifdef PURE_THREAD_DEBUG
	cout << "pure_init_lock(" << name << ")" << endl;
#endif
#if 1
	/* cure memory leak in with sthread_name_t */
	name = 0;
#endif	

	slock = new (lock) smutex_t(name);
	
	return;
}

int
pure_get_lock(void *lock, int wait_for_lock)
{
	smutex_t	*slock = (smutex_t *) lock;
	w_rc_t		e;
	long		timeout;

	timeout = wait_for_lock ? sthread_base_t::WAIT_FOREVER
		: sthread_base_t::WAIT_IMMEDIATE;
	
#ifdef PURE_THREAD_DEBUG	
	cout << "pure_get_lock(" << slock->name() << ")" << endl;
#endif

	e = slock->acquire(timeout);
	if (e == RCOK)
		return 1;

	if (e && e.err_num() == sthread_base_t::stTIMEOUT && !wait_for_lock)
		return 0;

	/* XXX generate an error */
	return 1;
}

void
pure_release_lock(void *lock)
{
	smutex_t	*slock = (smutex_t *) lock;

#ifdef PURE_THREAD_DEBUG
	cout << "pure_release_lock(" << slock->name() << ")" << endl;
#endif

	slock->release();
}


/*
 * For thread id's, use the thread's unique id
 * sthread_t *'s may be reused, but the id lasts forever, or
 * until the heat death of the 32bit universe :-)
 */

#if 1

typedef sthread_t *pure_thread_id_t;

unsigned int
pure_thread_id_size()
{
	return sizeof(pure_thread_id_t);
}

void
pure_thread_id(void *id_p)
{
#if defined(PURE_THREAD_DEBUG)
	cout << "pure_thread_id(" << sthread_t::me()->name() << ")" << endl;
#endif
	*(pure_thread_id_t *)id_p = sthread_t::me();
}

int
pure_thread_id_equal(void *id1_p, void *id2_p)
{
	if (!(id1_p && id2_p))
		return 0;
	return (*(pure_thread_id_t *)id1_p == *(pure_thread_id_t *)id2_p) ? 1 : 0;
}

#else

typedef sthread_t::id_t pure_thread_id_t;

unsigned int
pure_thread_id_size()
{
	return sizeof(pure_thread_id_t);
}

void
pure_thread_id(void *id_p)
{
	static pure_thread_id_t bogus = 0xffffffff;

#if defined(PURE_THREAD_DEBUG)
	cout << "pure_thread_id(" << sthread_t::me()->name() << ")" << endl;
#endif
	sthread_t *t = sthread_t::me();
	*(pure_thread_id_t *)id_p = t ? t->id : bogus;
}

int
pure_thread_id_equal(void *id1_p, void *id2_p)
{
	if (!(id1_p && id2_p))
		return 0;
	return (*(pure_thread_id_t *)id1_p == *(pure_thread_id_t *)id2_p) ? 1 : 0;
}
#endif

/*
 * NOTICE_STACK_CHANGE in combination with INIT_EXPLICIT gives
 * the best behavior for Pure software with native sthreads.
 * Purify doesn't attempt to do anything with threads until
 * it is explicitly initialized.    Other combinations just
 * don't work well, for purify starts prodding the threads package
 * at random ... even before it is constructed, and before
 * purify is explicitly initialized.   It can be made to work,
 * but it is hacky, and it doesn't work well.
 */

int pure_thread_switch_protocol = 
#if 1 || defined(PURE_THREAD_OLD)
PURE_THREAD_PROTOCOL_NOTICE_STACK_CHANGE
#else
PURE_THREAD_PROTOCOL_EXPLICIT_CONTEXT_SWITCH
#endif
;

#if 0 || defined(PURE_THREAD_OLD)
int pure_thread_init_protocol = PURE_THREAD_INIT_IMPLICIT;
#else
int pure_thread_init_protocol = PURE_THREAD_INIT_EXPLICIT;
#endif

#if 0
int pure_native_solaris_threads = 0;
int pure_shared_frame_stack = 0;
#endif

#ifdef __cplusplus
} /* extern "C" */
#endif


/*<std-header orig-src='shore' incl-file-exclusion='PURE_THREADS_H'>

 $Id: pure_threads.h,v 1.6 1999/06/07 19:06:01 kupsch Exp $

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

#ifndef PURE_THREADS_H
#define PURE_THREADS_H

#include "w_defines.h"

/*  -- do not edit anything above this line --   </std-header>*/

/* pure_threads.h */
/*
 * Interface for Pure thread support.
 *
 * Copyright (c) 1994 Pure Software.
 *
 * This file contains proprietary and confidential information and
 * remains the unpublished property of Pure Software. Use, disclosure,
 * or reproduction is prohibited except as permitted by express written
 * license agreement with Pure Software.
 */

/***************
 *
 * This file defines the protocol for thread library support.
 * The protocol is subject to change.
 * If you would like to add support for a new thread library,
 * please contact Pure Software at support@pure.com.
 *
 ***************/


#ifdef __cplusplus
extern "C" {
#endif

/* Prototype macro for non ANSI compilers. */
#ifndef P
#  if defined(__STDC__)
#    define P(args) args
#  else
#    define P(args) ()
#  endif
#endif

/***************
 *
 * The following functions need to be defined by the
 * thread library interface file.
 *
 ***************/

/*
 * Zero iff locking is a no-op.
 */
extern unsigned int pure_use_locking;

/*
 * Return the size of a lock object.
 */
unsigned int pure_lock_size P((void));

/*
 * Initialize a (new) lock object, whose size was determined
 * by pure_lock_size().
 */
void pure_initialize_lock P((void * lock));

/*
 * Obtain possesion of the lock by the current thread.
 * If wait_for_lock is 0 and the lock is already held in
 * in some thread (potentially even this thread), return
 * a value of 0 immediately, without getting the lock.
 * Otherwise wait (in an efficient manner) until the lock
 * is available, and return a value of 1 with the lock
 * owned by the current thread.
 */
int pure_get_lock P((void * lock, int wait_for_lock));

/*
 * Release the lock, which was previously locked by a
 * call to pure_get_lock().
 */
void pure_release_lock P((void * lock));

/*
 * Return the size of an object that can hold
 * a unique thread id marker.
 */
unsigned int pure_thread_id_size P((void));

/*
 * Fill in a thread id marker with the id
 * of the current thread.
 */
void pure_thread_id P((void * id_p));

/*
 * Compare two thread id values.
 * Return 1 if they represent the same thread,
 * otherwise return 0.
 */
int pure_thread_id_equal P((void * id1_p, void * id2_p));


/***************
 *
 * The following are provided in rtlib.o
 * for use by the thread library interface.
 *
 ***************/

#define PURE_THREAD_PROTOCOL_UNKNOWN			0
#define PURE_THREAD_PROTOCOL_NOTICE_STACK_CHANGE	1
#define PURE_THREAD_PROTOCOL_EXPLICIT_CONTEXT_SWITCH	2
extern int pure_thread_switch_protocol;
void pure_notice_context_switch P ((void * id_p));


#define PURE_THREAD_INIT_IMPLICIT			0
#define PURE_THREAD_INIT_EXPLICIT			1
extern int pure_thread_init_protocol;
void pure_init_threads P(());


#ifdef __cplusplus
}
#endif

/* For older version of purify */
#ifdef SUNOS41
#define PURE_THREAD_OLD
#endif

/*<std-footer incl-file-exclusion='PURE_THREADS_H'>  -- do not edit anything below this line -- */

#endif          /*</std-footer>*/

/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

/*
 *  $Id: w.h,v 1.13 1995/04/24 19:31:45 zwilling Exp $
 */
#ifndef W_H
#define W_H

#include <w_base.h>
#include <w_minmax.h>
#include <w_list.h>
#include <w_hash.h>

#ifdef W_INCL_SHMEM
#include <w_shmem.h>
#endif

#ifdef W_INCL_CIRCULAR_QUEUE
#include <w_cirqueue.h>
#endif

#ifdef W_INCL_TIMER
#include <w_timer.h>
#endif

#ifdef W_INCL_BITMAP
#include <w_bitmap.h>
#endif

#endif /*W_H*/

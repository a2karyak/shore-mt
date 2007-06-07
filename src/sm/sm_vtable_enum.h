/*<std-header orig-src='shore' incl-file-exclusion='VTABLE_ENUM_H'>

 $Id: sm_vtable_enum.h,v 1.1 2007/05/18 21:43:29 nhall Exp $

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

#ifndef SM_VTABLE_ENUM_H
#define SM_VTABLE_ENUM_H

#include "w_defines.h"

/*  -- do not edit anything above this line --   </std-header>*/

/*
 * Enum definition used for converting per-thread info
 * to a virtual table.  The enum assigns a distinct number to each
 * "attribute" of a thread, and takes into account a bunch of
 * derived thread types (for Paradise)
 * This acts as an index into a vtable_info_t;
 */

#include "sthread_vtable_enum.h"

enum {
	/* for xcts */
	xct_nthreads_attr,
	xct_gtid_attr,
	xct_tid_attr,
	xct_state_attr,
	xct_coordinator_attr,
	xct_forced_readonly_attr,

	/* last number! */
	xct_last
};

enum {
	/* for locks */
	lock_name_attr,
	lock_mode_attr,
	lock_duration_attr,
	lock_children_attr,
	lock_tid_attr,
	lock_status_attr,

	/* last number! */
	lock_last 
};
 
enum {
        /* per-smthread stats */
	junk = thread_last-1,
#include "smthread_stats_t_collect_enum_gen.h"
	smthread_last 
};

enum {
        /* per-sm stats */
#include "sm_stats_t_collect_enum_gen.h"
#include "sm2pc_stats_t_collect_enum_gen.h"
	sm_last 
};

enum {
	/* for buffer pool */
	bp_pid_attr,
	bp_pin_cnt_attr,
	bp_mode_attr,
	bp_dirty_attr,
	bp_hot_attr,

	/* last number! */
	bp_last 
};

enum {
    /* for per volume io */
    vol_id_attr,
    vol_writes_attr,
    vol_blks_written_attr,
    vol_reads_attr,
    vol_blks_read_attr,

    /* last number */
    vol_last
};

/*<std-footer incl-file-exclusion='VTABLE_ENUM_H'>  -- do not edit anything below this line -- */

#endif          /*</std-footer>*/

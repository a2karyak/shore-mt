/*<std-header orig-src='shore' incl-file-exclusion='VTABLE_ENUM_H'>

 $Id: vtable_enum.h,v 1.9 1999/06/07 19:02:49 kupsch Exp $

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

#ifndef VTABLE_ENUM_H
#define VTABLE_ENUM_H

#include "w_defines.h"

/*  -- do not edit anything above this line --   </std-header>*/

/*
 * Enum definition used for converting per-thread info
 * to a virtual table.  The enum assigns a distinct number to each
 * "attribute" of a thread, and takes into account a bunch of
 * derived thread types (for Paradise)
 * This acts as an index into a vtable_info_t;
 */

/*
 * TODO: remove the #defines below after all uses of the
 *  tokens are removed
 */

enum {
	/* for heaps (class factories)  */
	factory_class_name_attr,
#define factory_class_name factory_class_name_attr
	factory_class_size_attr,
#define factory_class_size factory_class_size_attr
	factory_preamble_overhead_attr, // per-object overhead
#define factory_preamble_overhead factory_preamble_overhead_attr
	factory_in_use_attr, 	// objects allocated from this factory 
				// including array allocations
				// and those allocated from global heap
				// (factory_global_in_use_attr)
#define factory_in_use factory_in_use_attr
	factory_high_water_attr, // highest value of factory_in_use_attr
#define factory_high_water factory_high_water
	factory_global_in_use_attr, // objects allocated from ::malloc
				// instead of from this factory heap
#define factory_global_in_use factory_global_in_use_attr
	factory_news_attr,	// objs allocated in calls to single new
#define factory_news factory_news_attr
	factory_dels_attr,	// objs deleted in calls to single delete
#define factory_dels factory_dels_attr
	factory_array_news_attr,// objs allocated in calls to new[]
#define factory_array_news factory_array_news_attr
	factory_array_dels_attr,// objs deleted in calls to delete[]
#define factory_array_dels factory_array_dels_attr

	/* set by each subclass: */
	factory_kind_attr,
#define factory_kind factory_kind_attr
	factory_static_overhead_attr, // per-factory overhead
#define factory_static_overhead factory_static_overhead_attr
	/* for fastnew factories: */
	factory_unused_attr,	// unused objs awaiting allocation 
#define factory_unused factory_unused_attr
				// sitting in the factory warehouse
	factory_chunks_attr,    // chunks allocated by factory
#define factory_chunks factory_chunks_attr
	factory_chunk_size_attr,// #objects/chunk 
#define factory_chunk_size factory_chunk_size_attr

	/* for threadnew factories: */
	factory_bytes_in_use_attr, // total #bytes of objects presently
				// in use, allocated by hook or by crook; 
				// does  NOT include per-object overhead or
				// per-factory overhead.  Does not include
				// objects sitting around unused in the
				// factory warehouse.

	factory_last
};

enum {
	/* for per-factory per-thread stuff */
	factory_histo_class_name_attr, 
#define factory_histo_class_name factory_histo_class_name_attr
	factory_histo_threadid_attr, // -1 means "no thread"
#define factory_histo_threadid factory_histo_threadid_attr
	factory_histo_alloced_attr, // objects allocated
#define factory_histo_alloced factory_histo_alloced_attr
	factory_histo_last
};

enum {
	/* for sthreads */
	sthread_id_attr,
	sthread_name_attr,
	sthread_status_attr,
	sthread_bytes_alloc_attr, // bytes in use, does not include overhead
	sthread_bytes_high_water_attr, // highest of sthread_bytes_alloc_attr
	sthread_inuse_attr,	// number allocated blobs in use (== news  
				// + new[]s - deletes - delete[]s)
#define sthread_news_attr sthread_inuse_attr
	sthread_bytes_overhead_attr, // sthread_inuse_attr * overhead
				// unfortunately, this is bogus at the 
				// moment, since not all factories have
				// the same overhead (TODO: fix)

	/* for smthreads */
	smthread_tid_attr,
#include "smthread_stats_t_collect_enum_gen.h"
	
	/* for twopc_threads */
	twopc_thread_purpose_attr,

	/* for subord_threads */
	subord_thread_coord_alive_attr,

	/* for coord_threads */
	coord_thread_state_attr, // might have to wait to get this
		// in which case, it will be "unnkown" rather
		// than block
	coord_thread_gtid_attr,
	coord_thread_awaiting_attr,
	coord_thread_died_attr,

	coord_thread_entry_numthreads_attr,

	/* for bpthread */
	bpthread_gtid_attr,

	/* for Paradise */
	pthread_auth_id_attr,
	pthread_client_id_attr,
	pthread_socket_number_attr,

	/* last number! */
	thread_last 
};

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

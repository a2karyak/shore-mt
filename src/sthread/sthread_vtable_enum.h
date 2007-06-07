/*<std-header orig-src='shore' incl-file-exclusion='VTABLE_ENUM_H'>

 $Id: sthread_vtable_enum.h,v 1.1 2007/05/18 21:53:44 nhall Exp $

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

#ifndef STHREAD_VTABLE_ENUM_H
#define STHREAD_VTABLE_ENUM_H

#include "w_defines.h"

/*  -- do not edit anything above this line --   </std-header>*/

/*
 * Enum definition used for converting per-thread info
 * to a virtual table.  The enum assigns a distinct number to each
 * "attribute" of a thread, and takes into account a bunch of
 * derived thread types (for Paradise)
 * This acts as an index into a vtable_info_t;
 */
#include "w_factory_vtable_enum.h"

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
#include "sthread_stats_collect_enum_gen.h"
	

	/* last number! */
	thread_last 
};

/*<std-footer incl-file-exclusion='VTABLE_ENUM_H'>  -- do not edit anything below this line -- */

#endif          /*</std-footer>*/

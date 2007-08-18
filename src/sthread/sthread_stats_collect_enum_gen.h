#ifndef STHREAD_STATS_COLLECT_ENUM_GEN_H
#define STHREAD_STATS_COLLECT_ENUM_GEN_H

/* DO NOT EDIT --- GENERATED from sthread_stats.dat by stats.pl
		   on Sat Aug 18 14:25:50 2007

<std-header orig-src='shore' genfile='true'>

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


	VT_idle_time,
	VT_io_time,
	VT_ctxsw,
	VT_spins,
	VT_fastpath,
	VT_nofastwrite,
	VT_local_io,
	VT_diskrw_io,
	VT_iowaits,
	VT_io,
	VT_ccio,
	VT_ccio2,
	VT_ccio3,
	VT_ccio4,
	VT_read,
	VT_write,
	VT_sync,
	VT_truncate,
	VT_writev,
	VT_writev_coalesce,
	VT_selects,
	VT_selfound,
	VT_eintrs,
	VT_idle,
	VT_idle_yield_return,
	VT_latch_wait,
	VT_latch_time,
	VT_mutex_wait,
	VT_scond_wait,
	VT_sevsem_wait,
	VT_zero,
	VT_one,
	VT_two,
	VT_three,
	VT_more,
	VT_wrapped,
	VT_selw0,
	VT_selw1,
	VT_selw2,
	VT_selw3,
	VT_selw4,
	VT_selwX,
	VT_self1,
	VT_self2,
	VT_self3,
	VT_self4,
	VT_selfX,

#endif /* STHREAD_STATS_COLLECT_ENUM_GEN_H */

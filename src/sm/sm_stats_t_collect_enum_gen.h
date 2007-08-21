#ifndef SM_STATS_T_COLLECT_ENUM_GEN_H
#define SM_STATS_T_COLLECT_ENUM_GEN_H

/* DO NOT EDIT --- GENERATED from sm_stats.dat by stats.pl
		   on Tue Aug 21 15:08:33 2007

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


	VT_bf_one_page_write,
	VT_bf_two_page_write,
	VT_bf_three_page_write,
	VT_bf_four_page_write,
	VT_bf_five_page_write,
	VT_bf_six_page_write,
	VT_bf_seven_page_write,
	VT_bf_eight_page_write,
	VT_bf_cleaner_sweeps,
	VT_bf_cleaner_signalled,
	VT_bf_kick_full,
	VT_bf_kick_replacement,
	VT_bf_kick_threshhold,
	VT_bf_sweep_page_hot,
	VT_bf_log_flush_all,
	VT_bf_log_flush_lsn,
	VT_bf_write_out,
	VT_bf_replace_out,
	VT_bf_replaced_dirty,
	VT_bf_replaced_clean,
	VT_bf_await_clean,
	VT_bf_prefetch_requests,
	VT_bf_prefetches,
	VT_vol_reads,
	VT_vol_writes,
	VT_vol_blks_written,
	VT_log_dup_sync_cnt,
	VT_log_sync_cnt,
	VT_log_fsync_cnt,
	VT_log_chkpt_cnt,
	VT_log_fetches,
	VT_log_inserts,
	VT_log_bytes_active,
	VT_log_records_generated,
	VT_log_bytes_generated,
	VT_io_extent_lsearch,
	VT_io_extent_lsearch_hop,

#endif /* SM_STATS_T_COLLECT_ENUM_GEN_H */

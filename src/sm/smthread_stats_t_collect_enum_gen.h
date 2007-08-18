#ifndef SMTHREAD_STATS_T_COLLECT_ENUM_GEN_H
#define SMTHREAD_STATS_T_COLLECT_ENUM_GEN_H

/* DO NOT EDIT --- GENERATED from smthread_stats.dat by stats.pl
		   on Sat Aug 18 14:26:29 2007

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


	VT_rec_pin_cnt,
	VT_rec_unpin_cnt,
	VT_rec_repin_cvt,
	VT_fm_pagecache_hit,
	VT_fm_nolatch,
	VT_fm_page_moved,
	VT_fm_page_full,
	VT_fm_histogram_hit,
	VT_fm_search_pages,
	VT_fm_search_failed,
	VT_fm_search_hit,
	VT_fm_lastpid_cached,
	VT_fm_lastpid_hit,
	VT_fm_alloc_pg,
	VT_fm_ext_touch,
	VT_fm_ext_touch_nop,
	VT_fm_nospace,
	VT_fm_cache,
	VT_fm_compact,
	VT_fm_append,
	VT_fm_appendonly,
	VT_bt_find_cnt,
	VT_bt_insert_cnt,
	VT_bt_remove_cnt,
	VT_bt_traverse_cnt,
	VT_bt_partial_traverse_cnt,
	VT_bt_restart_traverse_cnt,
	VT_bt_posc,
	VT_bt_scan_cnt,
	VT_bt_splits,
	VT_bt_cuts,
	VT_bt_grows,
	VT_bt_shrinks,
	VT_bt_links,
	VT_bt_upgrade_fail_retry,
	VT_bt_clr_smo_traverse,
	VT_bt_pcompress,
	VT_bt_plmax,
	VT_sort_keycmp_cnt,
	VT_sort_lexindx_cnt,
	VT_sort_getinfo_cnt,
	VT_sort_mof_cnt,
	VT_sort_umof_cnt,
	VT_sort_memcpy_cnt,
	VT_sort_memcpy_bytes,
	VT_sort_keycpy_cnt,
	VT_sort_mallocs,
	VT_sort_malloc_bytes,
	VT_sort_malloc_hiwat,
	VT_sort_malloc_max,
	VT_sort_malloc_curr,
	VT_sort_tmpfile_cnt,
	VT_sort_tmpfile_bytes,
	VT_sort_duplicates,
	VT_sort_page_fixes,
	VT_sort_page_fixes_2,
	VT_sort_lg_page_fixes,
	VT_sort_rec_pins,
	VT_sort_files_created,
	VT_sort_recs_created,
	VT_sort_rec_bytes,
	VT_sort_runs,
	VT_sort_run_size,
	VT_sort_phases,
	VT_sort_ntapes,
	VT_lid_lookups,
	VT_lid_remote_lookups,
	VT_lid_inserts,
	VT_lid_removes,
	VT_lid_cache_hits,
	VT_page_fix_cnt,
	VT_page_refix_cnt,
	VT_page_unfix_cnt,
	VT_page_alloc_cnt,
	VT_page_dealloc_cnt,
	VT_ext_lookup_hits,
	VT_ext_lookup_misses,
	VT_alloc_page_in_ext,
	VT_extent_lsearch,
	VT_extent_lsearch_hop,
	VT_begin_xct_cnt,
	VT_commit_xct_cnt,
	VT_abort_xct_cnt,
	VT_prepare_xct_cnt,
	VT_rollback_savept_cnt,
	VT_mpl_attach_cnt,
	VT_anchors,
	VT_compensate_in_log,
	VT_compensate_in_xct,
	VT_compensate_records,
	VT_compensate_skipped,
	VT_log_switches,
	VT_await_1thread_log,
	VT_acquire_1thread_log,
	VT_get_logbuf,
	VT_await_1thread_xct,
	VT_await_io_monitor,
	VT_await_vol_monitor,
	VT_s_prepared,
	VT_c_replies_dropped,
	VT_lock_query_cnt,
	VT_unlock_request_cnt,
	VT_lock_request_cnt,
	VT_lock_head_t_cnt,
	VT_lock_acquire_cnt,
	VT_lk_vol_acq,
	VT_lk_store_acq,
	VT_lk_page_acq,
	VT_lk_kvl_acq,
	VT_lk_rec_acq,
	VT_lk_ext_acq,
	VT_lk_user1_acq,
	VT_lk_user2_acq,
	VT_lk_user3_acq,
	VT_lk_user4_acq,
	VT_lock_cache_hit_cnt,
	VT_lock_request_t_cnt,
	VT_lock_extraneous_req_cnt,
	VT_lock_conversion_cnt,
	VT_lock_deadlock_cnt,
	VT_lock_esc_to_page,
	VT_lock_esc_to_store,
	VT_lock_esc_to_volume,
	VT_await_log_monitor,
	VT_await_log_monitor_var,
	VT_xlog_records_generated,
	VT_xlog_bytes_generated,

#endif /* SMTHREAD_STATS_T_COLLECT_ENUM_GEN_H */

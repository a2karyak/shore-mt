#ifndef SMTHREAD_STATS_T_MSG_GEN_H
#define SMTHREAD_STATS_T_MSG_GEN_H

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


/* SMT_rec_pin_cnt        */ "Times records were pinned in the buffer pool",
/* SMT_rec_unpin_cnt      */ "Times records were unpinned",
/* SMT_rec_repin_cvt      */ "Converted latch-lock to lock-lock deadlock",
/* SMT_fm_pagecache_hit   */ "Found recently-used page",
/* SMT_fm_nolatch         */ "Couldn't latch recently-used page ",
/* SMT_fm_page_moved      */ "Recently-used page moved to new store",
/* SMT_fm_page_full       */ "Recently-used page was full",
/* SMT_fm_histogram_hit   */ "Histogram says file search worthwhile",
/* SMT_fm_search_pages    */ "Pages inspected in file search",
/* SMT_fm_search_failed   */ "File search unsuccessful",
/* SMT_fm_search_hit      */ "File search successful",
/* SMT_fm_lastpid_cached  */ "Have last pid cached",
/* SMT_fm_lastpid_hit     */ "Found slot on page lastpid ",
/* SMT_fm_alloc_pg        */ "Allocated a new page",
/* SMT_fm_ext_touch       */ "Updates to extent info",
/* SMT_fm_ext_touch_nop   */ "No-op updates to extent info",
/* SMT_fm_nospace         */ "Could not create rec",
/* SMT_fm_cache           */ "Policy permitted looking in cache",
/* SMT_fm_compact         */ "Policy permitted searching file",
/* SMT_fm_append          */ "Policy permitted appending to file",
/* SMT_fm_appendonly      */ "Policy required strict append",
/* SMT_bt_find_cnt        */ "Btree lookups (find_assoc())",
/* SMT_bt_insert_cnt      */ "Btree inserts (create_assoc())",
/* SMT_bt_remove_cnt      */ "Btree removes (destroy_assoc())",
/* SMT_bt_traverse_cnt    */ "Btree traversals",
/* SMT_bt_partial_traverse_cnt */ "Btree traversals starting below root",
/* SMT_bt_restart_traverse_cnt */ "Restarted traversals",
/* SMT_bt_posc            */ "POSCs established",
/* SMT_bt_scan_cnt        */ "Btree scans started",
/* SMT_bt_splits          */ "Btree pages split (interior and leaf)",
/* SMT_bt_cuts            */ "Btree pages removed (interior and leaf)",
/* SMT_bt_grows           */ "Btree grew a level",
/* SMT_bt_shrinks         */ "Btree shrunk a level",
/* SMT_bt_links           */ "Btree links followed",
/* SMT_bt_upgrade_fail_retry */ "Failure to upgrade a latch forced a retry",
/* SMT_bt_clr_smo_traverse */ "Cleared SMO bits on traverse",
/* SMT_bt_pcompress       */ "Prefixes compressed",
/* SMT_bt_plmax           */ "Maximum prefix levels encountered",
/* SMT_sort_keycmp_cnt    */ "Key-comparison callbacks",
/* SMT_sort_lexindx_cnt   */ "Lexify index key callbacks",
/* SMT_sort_getinfo_cnt   */ "Create-sort-key callbacks",
/* SMT_sort_mof_cnt       */ "Marshal-object callbacks",
/* SMT_sort_umof_cnt      */ "Unmarshal-object callbacks",
/* SMT_sort_memcpy_cnt    */ "Memcpys",
/* SMT_sort_memcpy_bytes  */ "Bytes copied in memcpy",
/* SMT_sort_keycpy_cnt    */ "Keycopies (part of memcpy_cnt)",
/* SMT_sort_mallocs       */ "Allocations",
/* SMT_sort_malloc_bytes  */ "Bytes allocated total",
/* SMT_sort_malloc_hiwat  */ "Max allocated at any one time",
/* SMT_sort_malloc_max    */ "Largest chunk allocated",
/* SMT_sort_malloc_curr   */ "Amt presently allocated",
/* SMT_sort_tmpfile_cnt   */ "Records written to temp files",
/* SMT_sort_tmpfile_bytes */ "Bytes written to temp files",
/* SMT_sort_duplicates    */ "Duplicate records eliminated",
/* SMT_sort_page_fixes    */ "Orig slotted pages fixed by sort for read",
/* SMT_sort_page_fixes_2  */ "Tmp file slotted pages fixed by sort for read",
/* SMT_sort_lg_page_fixes */ "Large obj pages explicitly fixed by sort",
/* SMT_sort_rec_pins      */ "Recs explicitly pinned by sort",
/* SMT_sort_files_created */ "Files created by sort",
/* SMT_sort_recs_created  */ "Final records created by sort",
/* SMT_sort_rec_bytes     */ "Bytes in final records",
/* SMT_sort_runs          */ "Runs merged",
/* SMT_sort_run_size      */ "Pages of input recs per run",
/* SMT_sort_phases        */ "Polyphase phases",
/* SMT_sort_ntapes        */ "Number of pseudo-tapes used by sort",
/* SMT_lid_lookups        */ "Logical ID look-ups",
/* SMT_lid_remote_lookups */ "Extra index lookups for remote refs",
/* SMT_lid_inserts        */ "Logical IDs inserted",
/* SMT_lid_removes        */ "Logical IDs removed",
/* SMT_lid_cache_hits     */ "Hits in cache of recent lookups",
/* SMT_page_fix_cnt       */ "Times pages were fixed in the buffer pool",
/* SMT_page_refix_cnt     */ "Times pages were refixed (cheaper than fix)",
/* SMT_page_unfix_cnt     */ "Times pages were unfixed",
/* SMT_page_alloc_cnt     */ "Pages allocated",
/* SMT_page_dealloc_cnt   */ "Pages deallocated",
/* SMT_ext_lookup_hits    */ "Hits in extent lookups in cache ",
/* SMT_ext_lookup_misses  */ "Misses in extent lookups in cache ",
/* SMT_alloc_page_in_ext  */ "Extent searches for free pages",
/* SMT_extent_lsearch     */ "Linear searches through file looking for space",
/* SMT_extent_lsearch_hop */ "Linear search hops ",
/* SMT_begin_xct_cnt      */ "Transactions started",
/* SMT_commit_xct_cnt     */ "Transactions committed",
/* SMT_abort_xct_cnt      */ "Transactions aborted",
/* SMT_prepare_xct_cnt    */ "Transactions prepared",
/* SMT_rollback_savept_cnt */ "Rollbacks to savepoints (not incl aborts)",
/* SMT_mpl_attach_cnt     */ "Times a thread was not the only one attaching to a transaction",
/* SMT_anchors            */ "Log Anchors grabbed",
/* SMT_compensate_in_log  */ "Compensations written in log buffer",
/* SMT_compensate_in_xct  */ "Compensations written in xct log buffer",
/* SMT_compensate_records */ "Compensations written as own log record ",
/* SMT_compensate_skipped */ "Compensations would be a no-op",
/* SMT_log_switches       */ "Times log turned off",
/* SMT_await_1thread_log  */ "Times blocked on 1thread mutex for xct-log",
/* SMT_acquire_1thread_log */ "Times acquired 1thread mutex for xct-log",
/* SMT_get_logbuf         */ "Times acquired log buf for xct",
/* SMT_await_1thread_xct  */ "Times blocked on 1thread mutex for xct",
/* SMT_await_io_monitor   */ "Times couldn't enter I/O monitor immediately",
/* SMT_await_vol_monitor  */ "Times couldn't grab volume mutex immediately",
/* SMT_s_prepared         */ "Externally coordinated prepares",
/* SMT_c_replies_dropped  */ "Replies dropped by coordinator",
/* SMT_lock_query_cnt     */ "High-level query for lock information",
/* SMT_unlock_request_cnt */ "High-level unlock requests",
/* SMT_lock_request_cnt   */ "High-level lock requests",
/* SMT_lock_head_t_cnt    */ "Locks heads put in table for chains of requests",
/* SMT_lock_acquire_cnt   */ "Acquires to satisfy high-level requests",
/* SMT_lk_vol_acq         */ "Volume locks acquired",
/* SMT_lk_store_acq       */ "Store locks acquired",
/* SMT_lk_page_acq        */ "Page locks acquired",
/* SMT_lk_kvl_acq         */ "Key-value locks acquired",
/* SMT_lk_rec_acq         */ "Record locks acquired",
/* SMT_lk_ext_acq         */ "Extent locks acquired",
/* SMT_lk_user1_acq       */ "User1 locks acquired",
/* SMT_lk_user2_acq       */ "User2 locks acquired",
/* SMT_lk_user3_acq       */ "User3 locks acquired",
/* SMT_lk_user4_acq       */ "User4 locks acquired",
/* SMT_lock_cache_hit_cnt */ "Hits on lock cache (avoid acquires)",
/* SMT_lock_request_t_cnt */ "Lock request structures chained off lock heads",
/* SMT_lock_extraneous_req_cnt */ "Extraneous requests (already granted)",
/* SMT_lock_conversion_cnt */ "Requests requiring conversion",
/* SMT_lock_deadlock_cnt  */ "Deadlocks detected",
/* SMT_lock_esc_to_page   */ "Number of escalations to page level",
/* SMT_lock_esc_to_store  */ "Number of escalations to store level",
/* SMT_lock_esc_to_volume */ "Number of escalations to volume level",
/* SMT_await_log_monitor  */ "Possible long wait for log monitor ",
/* SMT_await_log_monitor_var */ "Short wait for log monitor ",
/* SMT_xlog_records_generated */ "Log records written for xct",
/* SMT_xlog_bytes_generated */ "Bytes written to the log for xct",
	"dummy stat code"

#endif /* SMTHREAD_STATS_T_MSG_GEN_H */

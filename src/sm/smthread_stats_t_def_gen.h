#ifndef SMTHREAD_STATS_T_DEF_GEN_H
#define SMTHREAD_STATS_T_DEF_GEN_H

/* DO NOT EDIT --- GENERATED from smthread_stats.dat by stats.pl
		   on Fri May 30 23:57:48 2008

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


#define SMT_rec_pin_cnt              0x00070000,0
#define SMT_rec_unpin_cnt              0x00070000,1
#define SMT_rec_repin_cvt              0x00070000,2
#define SMT_fm_pagecache_hit              0x00070000,3
#define SMT_fm_nolatch              0x00070000,4
#define SMT_fm_page_moved              0x00070000,5
#define SMT_fm_page_full              0x00070000,6
#define SMT_fm_histogram_hit              0x00070000,7
#define SMT_fm_search_pages              0x00070000,8
#define SMT_fm_search_failed              0x00070000,9
#define SMT_fm_search_hit              0x00070000,10
#define SMT_fm_lastpid_cached              0x00070000,11
#define SMT_fm_lastpid_hit              0x00070000,12
#define SMT_fm_alloc_pg              0x00070000,13
#define SMT_fm_ext_touch              0x00070000,14
#define SMT_fm_ext_touch_nop              0x00070000,15
#define SMT_fm_nospace              0x00070000,16
#define SMT_fm_cache              0x00070000,17
#define SMT_fm_compact              0x00070000,18
#define SMT_fm_append              0x00070000,19
#define SMT_fm_appendonly              0x00070000,20
#define SMT_bt_find_cnt              0x00070000,21
#define SMT_bt_insert_cnt              0x00070000,22
#define SMT_bt_remove_cnt              0x00070000,23
#define SMT_bt_traverse_cnt              0x00070000,24
#define SMT_bt_partial_traverse_cnt              0x00070000,25
#define SMT_bt_restart_traverse_cnt              0x00070000,26
#define SMT_bt_posc              0x00070000,27
#define SMT_bt_scan_cnt              0x00070000,28
#define SMT_bt_splits              0x00070000,29
#define SMT_bt_cuts              0x00070000,30
#define SMT_bt_grows              0x00070000,31
#define SMT_bt_shrinks              0x00070000,32
#define SMT_bt_links              0x00070000,33
#define SMT_bt_upgrade_fail_retry              0x00070000,34
#define SMT_bt_clr_smo_traverse              0x00070000,35
#define SMT_bt_pcompress              0x00070000,36
#define SMT_bt_plmax              0x00070000,37
#define SMT_sort_keycmp_cnt              0x00070000,38
#define SMT_sort_lexindx_cnt              0x00070000,39
#define SMT_sort_getinfo_cnt              0x00070000,40
#define SMT_sort_mof_cnt              0x00070000,41
#define SMT_sort_umof_cnt              0x00070000,42
#define SMT_sort_memcpy_cnt              0x00070000,43
#define SMT_sort_memcpy_bytes              0x00070000,44
#define SMT_sort_keycpy_cnt              0x00070000,45
#define SMT_sort_mallocs              0x00070000,46
#define SMT_sort_malloc_bytes              0x00070000,47
#define SMT_sort_malloc_hiwat              0x00070000,48
#define SMT_sort_malloc_max              0x00070000,49
#define SMT_sort_malloc_curr              0x00070000,50
#define SMT_sort_tmpfile_cnt              0x00070000,51
#define SMT_sort_tmpfile_bytes              0x00070000,52
#define SMT_sort_duplicates              0x00070000,53
#define SMT_sort_page_fixes              0x00070000,54
#define SMT_sort_page_fixes_2              0x00070000,55
#define SMT_sort_lg_page_fixes              0x00070000,56
#define SMT_sort_rec_pins              0x00070000,57
#define SMT_sort_files_created              0x00070000,58
#define SMT_sort_recs_created              0x00070000,59
#define SMT_sort_rec_bytes              0x00070000,60
#define SMT_sort_runs              0x00070000,61
#define SMT_sort_run_size              0x00070000,62
#define SMT_sort_phases              0x00070000,63
#define SMT_sort_ntapes              0x00070000,64
#define SMT_page_fix_cnt              0x00070000,65
#define SMT_page_refix_cnt              0x00070000,66
#define SMT_page_unfix_cnt              0x00070000,67
#define SMT_page_alloc_cnt              0x00070000,68
#define SMT_page_dealloc_cnt              0x00070000,69
#define SMT_ext_lookup_hits              0x00070000,70
#define SMT_ext_lookup_misses              0x00070000,71
#define SMT_alloc_page_in_ext              0x00070000,72
#define SMT_extent_lsearch              0x00070000,73
#define SMT_extent_lsearch_hop              0x00070000,74
#define SMT_begin_xct_cnt              0x00070000,75
#define SMT_commit_xct_cnt              0x00070000,76
#define SMT_abort_xct_cnt              0x00070000,77
#define SMT_prepare_xct_cnt              0x00070000,78
#define SMT_rollback_savept_cnt              0x00070000,79
#define SMT_mpl_attach_cnt              0x00070000,80
#define SMT_anchors              0x00070000,81
#define SMT_compensate_in_log              0x00070000,82
#define SMT_compensate_in_xct              0x00070000,83
#define SMT_compensate_records              0x00070000,84
#define SMT_compensate_skipped              0x00070000,85
#define SMT_log_switches              0x00070000,86
#define SMT_await_1thread_log              0x00070000,87
#define SMT_acquire_1thread_log              0x00070000,88
#define SMT_get_logbuf              0x00070000,89
#define SMT_await_1thread_xct              0x00070000,90
#define SMT_await_io_monitor              0x00070000,91
#define SMT_await_vol_monitor              0x00070000,92
#define SMT_s_prepared              0x00070000,93
#define SMT_c_replies_dropped              0x00070000,94
#define SMT_lock_query_cnt              0x00070000,95
#define SMT_unlock_request_cnt              0x00070000,96
#define SMT_lock_request_cnt              0x00070000,97
#define SMT_lock_head_t_cnt              0x00070000,98
#define SMT_lock_acquire_cnt              0x00070000,99
#define SMT_lk_vol_acq              0x00070000,100
#define SMT_lk_store_acq              0x00070000,101
#define SMT_lk_page_acq              0x00070000,102
#define SMT_lk_kvl_acq              0x00070000,103
#define SMT_lk_rec_acq              0x00070000,104
#define SMT_lk_ext_acq              0x00070000,105
#define SMT_lk_user1_acq              0x00070000,106
#define SMT_lk_user2_acq              0x00070000,107
#define SMT_lk_user3_acq              0x00070000,108
#define SMT_lk_user4_acq              0x00070000,109
#define SMT_lock_cache_hit_cnt              0x00070000,110
#define SMT_lock_request_t_cnt              0x00070000,111
#define SMT_lock_extraneous_req_cnt              0x00070000,112
#define SMT_lock_conversion_cnt              0x00070000,113
#define SMT_lock_deadlock_cnt              0x00070000,114
#define SMT_lock_esc_to_page              0x00070000,115
#define SMT_lock_esc_to_store              0x00070000,116
#define SMT_lock_esc_to_volume              0x00070000,117
#define SMT_await_log_monitor              0x00070000,118
#define SMT_await_log_monitor_var              0x00070000,119
#define SMT_xlog_records_generated              0x00070000,120
#define SMT_xlog_bytes_generated              0x00070000,121
#define SMT_STATMIN             0x70000
#define SMT_STATMAX             0x70079


#endif /* SMTHREAD_STATS_T_DEF_GEN_H */

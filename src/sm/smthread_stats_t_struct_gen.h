#ifndef SMTHREAD_STATS_T_STRUCT_GEN_H
#define SMTHREAD_STATS_T_STRUCT_GEN_H

/* DO NOT EDIT --- GENERATED from smthread_stats.dat by stats.pl
		   on Fri May 18 19:32:29 2007

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


 w_stat_t _dummy_before_stats;
 unsigned long rec_pin_cnt;
 unsigned long rec_unpin_cnt;
 unsigned long rec_repin_cvt;
 unsigned long fm_pagecache_hit;
 unsigned long fm_nolatch;
 unsigned long fm_page_moved;
 unsigned long fm_page_full;
 unsigned long fm_histogram_hit;
 unsigned long fm_search_pages;
 unsigned long fm_search_failed;
 unsigned long fm_search_hit;
 unsigned long fm_lastpid_cached;
 unsigned long fm_lastpid_hit;
 unsigned long fm_alloc_pg;
 unsigned long fm_ext_touch;
 unsigned long fm_ext_touch_nop;
 unsigned long fm_nospace;
 unsigned long fm_cache;
 unsigned long fm_compact;
 unsigned long fm_append;
 unsigned long fm_appendonly;
 unsigned long bt_find_cnt;
 unsigned long bt_insert_cnt;
 unsigned long bt_remove_cnt;
 unsigned long bt_traverse_cnt;
 unsigned long bt_partial_traverse_cnt;
 unsigned long bt_restart_traverse_cnt;
 unsigned long bt_posc;
 unsigned long bt_scan_cnt;
 unsigned long bt_splits;
 unsigned long bt_cuts;
 unsigned long bt_grows;
 unsigned long bt_shrinks;
 unsigned long bt_links;
 unsigned long bt_upgrade_fail_retry;
 unsigned long bt_clr_smo_traverse;
 unsigned long bt_pcompress;
 unsigned long bt_plmax;
 unsigned long sort_keycmp_cnt;
 unsigned long sort_lexindx_cnt;
 unsigned long sort_getinfo_cnt;
 unsigned long sort_mof_cnt;
 unsigned long sort_umof_cnt;
 unsigned long sort_memcpy_cnt;
 unsigned long sort_memcpy_bytes;
 unsigned long sort_keycpy_cnt;
 unsigned long sort_mallocs;
 unsigned long sort_malloc_bytes;
 unsigned long sort_malloc_hiwat;
 unsigned long sort_malloc_max;
 unsigned long sort_malloc_curr;
 unsigned long sort_tmpfile_cnt;
 unsigned long sort_tmpfile_bytes;
 unsigned long sort_duplicates;
 unsigned long sort_page_fixes;
 unsigned long sort_page_fixes_2;
 unsigned long sort_lg_page_fixes;
 unsigned long sort_rec_pins;
 unsigned long sort_files_created;
 unsigned long sort_recs_created;
 unsigned long sort_rec_bytes;
 unsigned long sort_runs;
 unsigned long sort_run_size;
 unsigned long sort_phases;
 unsigned long sort_ntapes;
 unsigned long lid_lookups;
 unsigned long lid_remote_lookups;
 unsigned long lid_inserts;
 unsigned long lid_removes;
 unsigned long lid_cache_hits;
 unsigned long page_fix_cnt;
 unsigned long page_refix_cnt;
 unsigned long page_unfix_cnt;
 unsigned long page_alloc_cnt;
 unsigned long page_dealloc_cnt;
 unsigned long ext_lookup_hits;
 unsigned long ext_lookup_misses;
 unsigned long alloc_page_in_ext;
 unsigned long extent_lsearch;
 unsigned long extent_lsearch_hop;
 unsigned long begin_xct_cnt;
 unsigned long commit_xct_cnt;
 unsigned long abort_xct_cnt;
 unsigned long prepare_xct_cnt;
 unsigned long rollback_savept_cnt;
 unsigned long mpl_attach_cnt;
 unsigned long anchors;
 unsigned long compensate_in_log;
 unsigned long compensate_in_xct;
 unsigned long compensate_records;
 unsigned long compensate_skipped;
 unsigned long log_switches;
 unsigned long await_1thread_log;
 unsigned long acquire_1thread_log;
 unsigned long get_logbuf;
 unsigned long await_1thread_xct;
 unsigned long await_io_monitor;
 unsigned long await_vol_monitor;
 unsigned long s_prepared;
 unsigned long c_replies_dropped;
 unsigned long lock_query_cnt;
 unsigned long unlock_request_cnt;
 unsigned long lock_request_cnt;
 unsigned long lock_head_t_cnt;
 unsigned long lock_acquire_cnt;
 unsigned long lk_vol_acq;
 unsigned long lk_store_acq;
 unsigned long lk_page_acq;
 unsigned long lk_kvl_acq;
 unsigned long lk_rec_acq;
 unsigned long lk_ext_acq;
 unsigned long lk_user1_acq;
 unsigned long lk_user2_acq;
 unsigned long lk_user3_acq;
 unsigned long lk_user4_acq;
 unsigned long lock_cache_hit_cnt;
 unsigned long lock_request_t_cnt;
 unsigned long lock_extraneous_req_cnt;
 unsigned long lock_conversion_cnt;
 unsigned long lock_deadlock_cnt;
 unsigned long lock_esc_to_page;
 unsigned long lock_esc_to_store;
 unsigned long lock_esc_to_volume;
 unsigned long await_log_monitor;
 unsigned long await_log_monitor_var;
 unsigned long xlog_records_generated;
 unsigned long xlog_bytes_generated;
public: 
friend ostream &
    operator<<(ostream &o,const smthread_stats_t &t);
public: 
friend w_statistics_t &
    operator<<(w_statistics_t &s,const smthread_stats_t &t);
public: 
friend smthread_stats_t &
    operator+=(smthread_stats_t &s,const smthread_stats_t &t);
public: 
friend smthread_stats_t &
    operator-=(smthread_stats_t &s,const smthread_stats_t &t);
    bool vacuous() const{
             const int *p=(int *)&this->_dummy_before_stats;
             const int *q=p+sizeof(*this);
             while(p<q)if(*p++) return false;
             return true;
         }
static const char	*stat_names[];
static const char	*stat_types;
#define W_smthread_stats_t  23 + 2

#endif /* SMTHREAD_STATS_T_STRUCT_GEN_H */

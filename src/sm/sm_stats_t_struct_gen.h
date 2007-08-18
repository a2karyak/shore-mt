#ifndef SM_STATS_T_STRUCT_GEN_H
#define SM_STATS_T_STRUCT_GEN_H

/* DO NOT EDIT --- GENERATED from sm_stats.dat by stats.pl
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


 w_stat_t _dummy_before_stats;
 unsigned long bf_one_page_write;
 unsigned long bf_two_page_write;
 unsigned long bf_three_page_write;
 unsigned long bf_four_page_write;
 unsigned long bf_five_page_write;
 unsigned long bf_six_page_write;
 unsigned long bf_seven_page_write;
 unsigned long bf_eight_page_write;
 unsigned long bf_cleaner_sweeps;
 unsigned long bf_cleaner_signalled;
 unsigned long bf_kick_full;
 unsigned long bf_kick_replacement;
 unsigned long bf_kick_threshhold;
 unsigned long bf_sweep_page_hot;
 unsigned long bf_log_flush_all;
 unsigned long bf_log_flush_lsn;
 unsigned long bf_write_out;
 unsigned long bf_replace_out;
 unsigned long bf_replaced_dirty;
 unsigned long bf_replaced_clean;
 unsigned long bf_await_clean;
 unsigned long bf_prefetch_requests;
 unsigned long bf_prefetches;
 unsigned long vol_reads;
 unsigned long vol_writes;
 unsigned long vol_blks_written;
 unsigned long log_dup_sync_cnt;
 unsigned long log_sync_cnt;
 unsigned long log_fsync_cnt;
 unsigned long log_chkpt_cnt;
 unsigned long log_fetches;
 unsigned long log_inserts;
 unsigned long log_bytes_active;
 unsigned long log_records_generated;
 unsigned long log_bytes_generated;
 unsigned long io_extent_lsearch;
 unsigned long io_extent_lsearch_hop;
public: 
friend ostream &
    operator<<(ostream &o,const sm_stats_t &t);
public: 
friend w_statistics_t &
    operator<<(w_statistics_t &s,const sm_stats_t &t);
public: 
friend sm_stats_t &
    operator+=(sm_stats_t &s,const sm_stats_t &t);
public: 
friend sm_stats_t &
    operator-=(sm_stats_t &s,const sm_stats_t &t);
    bool vacuous() const{
             const int *p=(int *)&this->_dummy_before_stats;
             const int *q=p+sizeof(*this);
             while(p<q)if(*p++) return false;
             return true;
         }
static const char	*stat_names[];
static const char	*stat_types;
#define W_sm_stats_t  21 + 2

#endif /* SM_STATS_T_STRUCT_GEN_H */

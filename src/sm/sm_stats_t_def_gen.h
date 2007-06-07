#ifndef SM_STATS_T_DEF_GEN_H
#define SM_STATS_T_DEF_GEN_H

/* DO NOT EDIT --- GENERATED from sm_stats.dat by stats.pl
		   on Fri May 18 19:32:28 2007

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


#define SM_bf_one_page_write              0x00060000,0
#define SM_bf_two_page_write              0x00060000,1
#define SM_bf_three_page_write              0x00060000,2
#define SM_bf_four_page_write              0x00060000,3
#define SM_bf_five_page_write              0x00060000,4
#define SM_bf_six_page_write              0x00060000,5
#define SM_bf_seven_page_write              0x00060000,6
#define SM_bf_eight_page_write              0x00060000,7
#define SM_bf_cleaner_sweeps              0x00060000,8
#define SM_bf_cleaner_signalled              0x00060000,9
#define SM_bf_kick_full              0x00060000,10
#define SM_bf_kick_replacement              0x00060000,11
#define SM_bf_kick_threshhold              0x00060000,12
#define SM_bf_sweep_page_hot              0x00060000,13
#define SM_bf_log_flush_all              0x00060000,14
#define SM_bf_log_flush_lsn              0x00060000,15
#define SM_bf_write_out              0x00060000,16
#define SM_bf_replace_out              0x00060000,17
#define SM_bf_replaced_dirty              0x00060000,18
#define SM_bf_replaced_clean              0x00060000,19
#define SM_bf_await_clean              0x00060000,20
#define SM_bf_prefetch_requests              0x00060000,21
#define SM_bf_prefetches              0x00060000,22
#define SM_vol_reads              0x00060000,23
#define SM_vol_writes              0x00060000,24
#define SM_vol_blks_written              0x00060000,25
#define SM_log_dup_sync_cnt              0x00060000,26
#define SM_log_sync_cnt              0x00060000,27
#define SM_log_fsync_cnt              0x00060000,28
#define SM_log_chkpt_cnt              0x00060000,29
#define SM_log_fetches              0x00060000,30
#define SM_log_inserts              0x00060000,31
#define SM_log_bytes_active              0x00060000,32
#define SM_log_records_generated              0x00060000,33
#define SM_log_bytes_generated              0x00060000,34
#define SM_io_extent_lsearch              0x00060000,35
#define SM_io_extent_lsearch_hop              0x00060000,36
#define SM_STATMIN             0x60000
#define SM_STATMAX             0x60024


#endif /* SM_STATS_T_DEF_GEN_H */

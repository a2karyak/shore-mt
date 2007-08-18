#ifndef SM_STATS_T_MSG_GEN_H
#define SM_STATS_T_MSG_GEN_H

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


/* SM_bf_one_page_write  */ "Single page written to volume",
/* SM_bf_two_page_write  */ "Two-page writes to volume",
/* SM_bf_three_page_write */ "Three-page writes to volume",
/* SM_bf_four_page_write */ "Four-page writes to volume",
/* SM_bf_five_page_write */ "Five-page writes to volume",
/* SM_bf_six_page_write  */ "Six-page writes to volume",
/* SM_bf_seven_page_write */ "Seven-page writes to volume",
/* SM_bf_eight_page_write */ "Eight-page writes to volume",
/* SM_bf_cleaner_sweeps  */ "Number of sweeps of the bf_cleaner thread",
/* SM_bf_cleaner_signalled */ "Number of sweeps initiated by a kick",
/* SM_bf_kick_full       */ "Kicks because pool is full of dirty pages",
/* SM_bf_kick_replacement */ "Kicks because doing page replacement",
/* SM_bf_kick_threshhold */ "Kicks because dirty page threshold met",
/* SM_bf_sweep_page_hot  */ "Page swept was not flushed because it was hot ",
/* SM_bf_log_flush_all   */ "Number of whole-log flushes by bf_cleaner",
/* SM_bf_log_flush_lsn   */ "Number of partial log flushes by bf_cleaner",
/* SM_bf_write_out       */ "Pages written out in background or forced",
/* SM_bf_replace_out     */ "Pages written out to free a frame for fixing",
/* SM_bf_replaced_dirty  */ "Victim for page replacement is dirty",
/* SM_bf_replaced_clean  */ "Victim for page replacement is clean",
/* SM_bf_await_clean     */ "Times awaited a clean page for fix()",
/* SM_bf_prefetch_requests */ "Requests to prefetch a page ",
/* SM_bf_prefetches      */ "Prefetches performed",
/* SM_vol_reads          */ "Data volume read requests (from disk)",
/* SM_vol_writes         */ "Data volume write requests (to disk)",
/* SM_vol_blks_written   */ "Data volume pages written (to disk)",
/* SM_log_dup_sync_cnt   */ "Times the log was flushed superfluously",
/* SM_log_sync_cnt       */ "Times the log was flushed (and was needed)",
/* SM_log_fsync_cnt      */ "Times the fsync system call was used",
/* SM_log_chkpt_cnt      */ "Checkpoints taken",
/* SM_log_fetches        */ "Log records fetched from log (read)",
/* SM_log_inserts        */ "Log records inserted into log (written)",
/* SM_log_bytes_active   */ "Log bytes written by active xcts",
/* SM_log_records_generated */ "Non-xct log records written",
/* SM_log_bytes_generated */ "Non-xct bytes written to the log",
/* SM_io_extent_lsearch  */ "Linear searches through store looking for space",
/* SM_io_extent_lsearch_hop */ "Linear search hops ",
	"dummy stat code"

#endif /* SM_STATS_T_MSG_GEN_H */

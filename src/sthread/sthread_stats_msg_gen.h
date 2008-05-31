#ifndef STHREAD_STATS_MSG_GEN_H
#define STHREAD_STATS_MSG_GEN_H

/* DO NOT EDIT --- GENERATED from sthread_stats.dat by stats.pl
		   on Fri May 30 23:57:12 2008

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


/* STHREAD_idle_time          */ "total time the sm process remains idle",
/* STHREAD_io_time            */ "total time threads wait for io.",
/* STHREAD_ctxsw              */ "Thread context switches",
/* STHREAD_spins              */ "Busy waits on spin lock",
/* STHREAD_fastpath           */ "Times diskrw was bypassed",
/* STHREAD_nofastwrite        */ "Slow Fastpath writes",
/* STHREAD_local_io           */ "Local I/O",
/* STHREAD_diskrw_io          */ "Disk R/W I/O",
/* STHREAD_iowaits            */ "Times waited on diskrw process",
/* STHREAD_io                 */ "Number of I/Os",
/* STHREAD_ccio               */ "Concurrent I/O calls",
/* STHREAD_ccio2              */ "Two concurrent I/Os",
/* STHREAD_ccio3              */ "Three concurrent I/Os",
/* STHREAD_ccio4              */ "Four concurrent I/Os",
/* STHREAD_read               */ "Number of reads",
/* STHREAD_write              */ "Number of writes",
/* STHREAD_sync               */ "Number of syncs",
/* STHREAD_truncate           */ "Number of truncates",
/* STHREAD_writev             */ "Number of writevs",
/* STHREAD_writev_coalesce    */ "Number of writevs coalesced",
/* STHREAD_selects            */ "Total # select() calls",
/* STHREAD_selfound           */ "select() calls that returned before timeout",
/* STHREAD_eintrs             */ "Times select() was interrupted by EINTR",
/* STHREAD_idle               */ "Times select() timed out",
/* STHREAD_idle_yield_return  */ "Times idle thread returned from yield()",
/* STHREAD_latch_wait         */ "Times a thread awaited a latch",
/* STHREAD_latch_time         */ "Time waiting for latches",
/* STHREAD_mutex_wait         */ "Times a thread awaited a mutex",
/* STHREAD_scond_wait         */ "Times a thread awaited a condition",
/* STHREAD_sevsem_wait        */ "Times a thread awaited an event semaphore ",
/* STHREAD_zero               */ "Times I/O done before next select()",
/* STHREAD_one                */ "Times I/O done after one select()",
/* STHREAD_two                */ "Times I/O finished after 2 select()s",
/* STHREAD_three              */ "Times I/O finished after 3 select()s",
/* STHREAD_more               */ "Times I/O finished after 4 or more select()s",
/* STHREAD_wrapped            */ "Times select counter wrapped ",
/* STHREAD_selw0              */ "Number of event waits for 0 events",
/* STHREAD_selw1              */ "Number of event waits for 1 events",
/* STHREAD_selw2              */ "Number of event waits for 2 events",
/* STHREAD_selw3              */ "Number of event waits for 3 events",
/* STHREAD_selw4              */ "Number of event waits for 4 events",
/* STHREAD_selwX              */ "Number of event waits for >4 events",
/* STHREAD_self1              */ "Number of event waits finding 1 event",
/* STHREAD_self2              */ "Number of event waits finding 2 events",
/* STHREAD_self3              */ "Number of event waits finding 3 events",
/* STHREAD_self4              */ "Number of event waits finding 4 events",
/* STHREAD_selfX              */ "Number of event waits finding >4 events",
	"dummy stat code"

#endif /* STHREAD_STATS_MSG_GEN_H */

#ifndef STHREAD_STATS_DEF_GEN_H
#define STHREAD_STATS_DEF_GEN_H

/* DO NOT EDIT --- GENERATED from sthread_stats.dat by stats.pl
		   on Tue Aug 21 15:07:55 2007

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


#define STHREAD_idle_time              0x00050000,0
#define STHREAD_io_time              0x00050000,1
#define STHREAD_ctxsw              0x00050000,2
#define STHREAD_spins              0x00050000,3
#define STHREAD_fastpath              0x00050000,4
#define STHREAD_nofastwrite              0x00050000,5
#define STHREAD_local_io              0x00050000,6
#define STHREAD_diskrw_io              0x00050000,7
#define STHREAD_iowaits              0x00050000,8
#define STHREAD_io              0x00050000,9
#define STHREAD_ccio              0x00050000,10
#define STHREAD_ccio2              0x00050000,11
#define STHREAD_ccio3              0x00050000,12
#define STHREAD_ccio4              0x00050000,13
#define STHREAD_read              0x00050000,14
#define STHREAD_write              0x00050000,15
#define STHREAD_sync              0x00050000,16
#define STHREAD_truncate              0x00050000,17
#define STHREAD_writev              0x00050000,18
#define STHREAD_writev_coalesce              0x00050000,19
#define STHREAD_selects              0x00050000,20
#define STHREAD_selfound              0x00050000,21
#define STHREAD_eintrs              0x00050000,22
#define STHREAD_idle              0x00050000,23
#define STHREAD_idle_yield_return              0x00050000,24
#define STHREAD_latch_wait              0x00050000,25
#define STHREAD_latch_time              0x00050000,26
#define STHREAD_mutex_wait              0x00050000,27
#define STHREAD_scond_wait              0x00050000,28
#define STHREAD_sevsem_wait              0x00050000,29
#define STHREAD_zero              0x00050000,30
#define STHREAD_one              0x00050000,31
#define STHREAD_two              0x00050000,32
#define STHREAD_three              0x00050000,33
#define STHREAD_more              0x00050000,34
#define STHREAD_wrapped              0x00050000,35
#define STHREAD_selw0              0x00050000,36
#define STHREAD_selw1              0x00050000,37
#define STHREAD_selw2              0x00050000,38
#define STHREAD_selw3              0x00050000,39
#define STHREAD_selw4              0x00050000,40
#define STHREAD_selwX              0x00050000,41
#define STHREAD_self1              0x00050000,42
#define STHREAD_self2              0x00050000,43
#define STHREAD_self3              0x00050000,44
#define STHREAD_self4              0x00050000,45
#define STHREAD_selfX              0x00050000,46
#define STHREAD_STATMIN             0x50000
#define STHREAD_STATMAX             0x5002e


#endif /* STHREAD_STATS_DEF_GEN_H */

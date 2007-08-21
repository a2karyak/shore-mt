#ifndef SHMC_STATS_MSG_GEN_H
#define SHMC_STATS_MSG_GEN_H

/* DO NOT EDIT --- GENERATED from shmc_stats.dat by stats.pl
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


/* DISKRW_alarm              */ "ALRM signals received",
/* DISKRW_notify             */ "Notified by server (SIGUSR1 or PIPE read)",
/* DISKRW_kicks              */ "Kicked server (SIGUSR2 or PIPE write)",
/* DISKRW_falarm             */ "Found msg because of ALRM",
/* DISKRW_fnotify            */ "Found msg because notified",
/* DISKRW_found              */ "Found msg by looking at queue",
/* DISKRW_lastsig            */ "Last signal received",
/* DISKRW_spins              */ "Busy waits on spin lock",
	"dummy stat code"

#endif /* SHMC_STATS_MSG_GEN_H */

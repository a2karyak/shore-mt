#ifndef SM2PC_STATS_T_MSG_GEN_H
#define SM2PC_STATS_T_MSG_GEN_H

/* DO NOT EDIT --- GENERATED from sm2pc_stats.dat by stats.pl
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


/* SM2PC_s_committed        */ "Externally coordinated commits",
/* SM2PC_s_aborted          */ "Externally coordinated aborts",
/* SM2PC_s_no_such          */ "Messages for unknown gtid",
/* SM2PC_s_prepare_recd     */ "Total prepare msgs received",
/* SM2PC_s_commit_recd      */ "Total commit msgs received",
/* SM2PC_s_abort_recd       */ "Total abort msgs received",
/* SM2PC_s_errors_recd      */ "Total messages received with error indication ",
/* SM2PC_s_acks_sent        */ "Total acks sent for commit/abort",
/* SM2PC_s_votes_sent       */ "Total votes sent for prepare",
/* SM2PC_s_status_sent      */ "Total recovery status messages sent",
/* SM2PC_s_errors_sent      */ "Total messages sent with error indication ",
/* SM2PC_c_coordinated      */ "Distrib tx initially requesting 2P commit ",
/* SM2PC_c_resolved         */ "Distrib tx resolved (includes recovery)",
/* SM2PC_c_resolved_commit  */ "Resolved transactions that committed",
/* SM2PC_c_resolved_abort   */ "Resolved transactions that aborted",
/* SM2PC_c_retrans          */ "Retransmitted messages",
/* SM2PC_c_acks_recd        */ "Total acks received for commit/abort",
/* SM2PC_c_votes_recd       */ "Total votes received ",
/* SM2PC_c_status_recd      */ "Total status requests received",
/* SM2PC_c_errors_recd      */ "Total messages received with error indication ",
/* SM2PC_c_prepare_sent     */ "Total prepare messages sent",
/* SM2PC_c_commit_sent      */ "Total commit messages sent",
/* SM2PC_c_abort_sent       */ "Total abort messages sent",
/* SM2PC_c_errors_sent      */ "Total messages sent with error indication ",
	"dummy stat code"

#endif /* SM2PC_STATS_T_MSG_GEN_H */

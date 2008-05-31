#ifndef SM2PC_STATS_T_COLLECT_ENUM_GEN_H
#define SM2PC_STATS_T_COLLECT_ENUM_GEN_H

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


	VT_s_committed,
	VT_s_aborted,
	VT_s_no_such,
	VT_s_prepare_recd,
	VT_s_commit_recd,
	VT_s_abort_recd,
	VT_s_errors_recd,
	VT_s_acks_sent,
	VT_s_votes_sent,
	VT_s_status_sent,
	VT_s_errors_sent,
	VT_c_coordinated,
	VT_c_resolved,
	VT_c_resolved_commit,
	VT_c_resolved_abort,
	VT_c_retrans,
	VT_c_acks_recd,
	VT_c_votes_recd,
	VT_c_status_recd,
	VT_c_errors_recd,
	VT_c_prepare_sent,
	VT_c_commit_sent,
	VT_c_abort_sent,
	VT_c_errors_sent,

#endif /* SM2PC_STATS_T_COLLECT_ENUM_GEN_H */

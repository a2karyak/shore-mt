#ifndef SM2PC_STATS_T_DEF_GEN_H
#define SM2PC_STATS_T_DEF_GEN_H

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


#define SM2PC_s_committed              0x00080000,0
#define SM2PC_s_aborted              0x00080000,1
#define SM2PC_s_no_such              0x00080000,2
#define SM2PC_s_prepare_recd              0x00080000,3
#define SM2PC_s_commit_recd              0x00080000,4
#define SM2PC_s_abort_recd              0x00080000,5
#define SM2PC_s_errors_recd              0x00080000,6
#define SM2PC_s_acks_sent              0x00080000,7
#define SM2PC_s_votes_sent              0x00080000,8
#define SM2PC_s_status_sent              0x00080000,9
#define SM2PC_s_errors_sent              0x00080000,10
#define SM2PC_c_coordinated              0x00080000,11
#define SM2PC_c_resolved              0x00080000,12
#define SM2PC_c_resolved_commit              0x00080000,13
#define SM2PC_c_resolved_abort              0x00080000,14
#define SM2PC_c_retrans              0x00080000,15
#define SM2PC_c_acks_recd              0x00080000,16
#define SM2PC_c_votes_recd              0x00080000,17
#define SM2PC_c_status_recd              0x00080000,18
#define SM2PC_c_errors_recd              0x00080000,19
#define SM2PC_c_prepare_sent              0x00080000,20
#define SM2PC_c_commit_sent              0x00080000,21
#define SM2PC_c_abort_sent              0x00080000,22
#define SM2PC_c_errors_sent              0x00080000,23
#define SM2PC_STATMIN             0x80000
#define SM2PC_STATMAX             0x80017


#endif /* SM2PC_STATS_T_DEF_GEN_H */

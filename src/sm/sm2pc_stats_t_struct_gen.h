#ifndef SM2PC_STATS_T_STRUCT_GEN_H
#define SM2PC_STATS_T_STRUCT_GEN_H

/* DO NOT EDIT --- GENERATED from sm2pc_stats.dat by stats.pl
		   on Wed May  7 19:54:01 2008

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
 unsigned long s_committed;
 unsigned long s_aborted;
 unsigned long s_no_such;
 unsigned long s_prepare_recd;
 unsigned long s_commit_recd;
 unsigned long s_abort_recd;
 unsigned long s_errors_recd;
 unsigned long s_acks_sent;
 unsigned long s_votes_sent;
 unsigned long s_status_sent;
 unsigned long s_errors_sent;
 unsigned long c_coordinated;
 unsigned long c_resolved;
 unsigned long c_resolved_commit;
 unsigned long c_resolved_abort;
 unsigned long c_retrans;
 unsigned long c_acks_recd;
 unsigned long c_votes_recd;
 unsigned long c_status_recd;
 unsigned long c_errors_recd;
 unsigned long c_prepare_sent;
 unsigned long c_commit_sent;
 unsigned long c_abort_sent;
 unsigned long c_errors_sent;
public: 
friend ostream &
    operator<<(ostream &o,const sm2pc_stats_t &t);
public: 
friend w_statistics_t &
    operator<<(w_statistics_t &s,const sm2pc_stats_t &t);
public: 
friend sm2pc_stats_t &
    operator+=(sm2pc_stats_t &s,const sm2pc_stats_t &t);
public: 
friend sm2pc_stats_t &
    operator-=(sm2pc_stats_t &s,const sm2pc_stats_t &t);
    bool vacuous() const{
             const int *p=(int *)&this->_dummy_before_stats;
             const int *q=p+sizeof(*this);
             while(p<q)if(*p++) return false;
             return true;
         }
static const char	*stat_names[];
static const char	*stat_types;
#define W_sm2pc_stats_t  17 + 2

#endif /* SM2PC_STATS_T_STRUCT_GEN_H */

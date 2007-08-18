#ifndef STHREAD_STATS_STRUCT_GEN_H
#define STHREAD_STATS_STRUCT_GEN_H

/* DO NOT EDIT --- GENERATED from sthread_stats.dat by stats.pl
		   on Sat Aug 18 14:25:50 2007

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
 float idle_time;
 float io_time;
 int ctxsw;
 int spins;
 int fastpath;
 int nofastwrite;
 int local_io;
 int diskrw_io;
 int iowaits;
 int io;
 int ccio;
 int ccio2;
 int ccio3;
 int ccio4;
 int read;
 int write;
 int sync;
 int truncate;
 int writev;
 int writev_coalesce;
 int selects;
 int selfound;
 int eintrs;
 int idle;
 int idle_yield_return;
 int latch_wait;
 int latch_time;
 int mutex_wait;
 int scond_wait;
 int sevsem_wait;
 int zero;
 int one;
 int two;
 int three;
 int more;
 int wrapped;
 int selw0;
 int selw1;
 int selw2;
 int selw3;
 int selw4;
 int selwX;
 int self1;
 int self2;
 int self3;
 int self4;
 int selfX;
public: 
friend ostream &
    operator<<(ostream &o,const sthread_stats &t);
public: 
friend w_statistics_t &
    operator<<(w_statistics_t &s,const sthread_stats &t);
public: 
friend sthread_stats &
    operator+=(sthread_stats &s,const sthread_stats &t);
public: 
friend sthread_stats &
    operator-=(sthread_stats &s,const sthread_stats &t);
    bool vacuous() const{
             const int *p=(int *)&this->_dummy_before_stats;
             const int *q=p+sizeof(*this);
             while(p<q)if(*p++) return false;
             return true;
         }
static const char	*stat_names[];
static const char	*stat_types;
#define W_sthread_stats  17 + 2

#endif /* STHREAD_STATS_STRUCT_GEN_H */

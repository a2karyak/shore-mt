#ifndef SHMC_STATS_STRUCT_GEN_H
#define SHMC_STATS_STRUCT_GEN_H

/* DO NOT EDIT --- GENERATED from shmc_stats.dat by stats.pl
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


 w_stat_t _dummy_before_stats;
 int alarm;
 int notify;
 int kicks;
 int falarm;
 int fnotify;
 int found;
 int lastsig;
 int spins;
public: 
friend ostream &
    operator<<(ostream &o,const shmc_stats &t);
public: 
friend w_statistics_t &
    operator<<(w_statistics_t &s,const shmc_stats &t);
public: 
friend shmc_stats &
    operator+=(shmc_stats &s,const shmc_stats &t);
public: 
friend shmc_stats &
    operator-=(shmc_stats &s,const shmc_stats &t);
    bool vacuous() const{
             const int *p=(int *)&this->_dummy_before_stats;
             const int *q=p+sizeof(*this);
             while(p<q)if(*p++) return false;
             return true;
         }
static const char	*stat_names[];
static const char	*stat_types;
#define W_shmc_stats  7 + 2

#endif /* SHMC_STATS_STRUCT_GEN_H */

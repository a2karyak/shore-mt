/*<std-header orig-src='shore' incl-file-exclusion='STHREAD_STATS_H'>

 $Id: sthread_stats.h,v 1.18 1999/06/07 19:06:15 kupsch Exp $

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

#ifndef STHREAD_STATS_H
#define STHREAD_STATS_H

#include "w_defines.h"

/*  -- do not edit anything above this line --   </std-header>*/

class sthread_stats {
public:
#include "sthread_stats_struct_gen.h"

    sthread_stats() {
	    // easier than remembering to add the inits 
	    // since we're changing the stats a lot
	    // during development
	    memset(this,'\0', sizeof(*this));
	}
    ~sthread_stats(){ }

    void clear() {
	memset((void *)this, '\0', sizeof(*this));
    }
};

extern ostream &operator<<(ostream &, const sthread_stats &stats);

extern class sthread_stats SthreadStats;

#define STATS SthreadStats

# 	define STH_STATS(x)  	SthreadStats.x
#	define INC_STH_STATS(x) SthreadStats.x++;
#	define SET_STH_STATS(x) SthreadStats.x = (y);

/*<std-footer incl-file-exclusion='STHREAD_STATS_H'>  -- do not edit anything below this line -- */

#endif          /*</std-footer>*/

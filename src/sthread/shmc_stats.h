/*<std-header orig-src='shore' incl-file-exclusion='SHMC_STATS_H'>

 $Id: shmc_stats.h,v 1.14 1999/06/07 19:06:07 kupsch Exp $

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

#ifndef SHMC_STATS_H
#define SHMC_STATS_H

#include "w_defines.h"

/*  -- do not edit anything above this line --   </std-header>*/


#ifdef EXCLUDE_SHMCSTATS
# 	define SHMCSTATS(x)  
# 	define INC_SHMCSTATS(x) 
# 	define SET_SHMCSTATS(x, y) 
#else
# 	define SHMCSTATS(x)  	ShmcStats.x
#	define INC_SHMCSTATS(x) ShmcStats.x++;
#	define SET_SHMCSTATS(x,y) ShmcStats.x = (y);

class shmc_stats {
public:
#include "shmc_stats_struct_gen.h"

    shmc_stats(): alarm(0), notify(0), kicks(0), falarm(0), fnotify(0),
				  found(0), lastsig(0), spins(0) {}

    ~shmc_stats(){ }

    void clear() {
	    memset((void *)this, '\0', sizeof(*this));
    }
};

extern class shmc_stats ShmcStats;

#undef DECL_SHMCSTATS

#if defined(IO_C)
#define DECL_SHMCSTATS 1
#endif

#ifndef _WINDOWS
#if defined(DISKRW_C)
#define DECL_SHMCSTATS 1
#endif
#endif

#ifdef DECL_SHMCSTATS

class shmc_stats ShmcStats; 

#endif  /* EXCLUDE_SHMCSTATS */

#endif /* STHREAD_C or DISKRW_C*/

/*<std-footer incl-file-exclusion='SHMC_STATS_H'>  -- do not edit anything below this line -- */

#endif          /*</std-footer>*/

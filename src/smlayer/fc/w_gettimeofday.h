/*<std-header orig-src='shore' incl-file-exclusion='W_GETTIMEOFDAY_H'>

 $Id: w_gettimeofday.h,v 1.7 1999/06/07 19:02:53 kupsch Exp $

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

#ifndef W_GETTIMEOFDAY_H
#define W_GETTIMEOFDAY_H

#include "w_defines.h"

/*  -- do not edit anything above this line --   </std-header>*/

#ifdef _WINDOWS
#include <time.h>
#include <sys/timeb.h>
#else
#include <sys/time.h>
#endif

#ifdef _WINDOWS
struct timezone {
	int	tz_minuteswest;	/* minutes west of Greenwich */
	int	tz_dsttime;	/* type of DST correction */
};
#endif /* _WINDOWS */

#if !defined(SOLARIS2) && !defined(Irix) && !defined(AIX41) 
#if defined(__GNUG__) || defined(_WINDOWS)
extern "C" {
    int gettimeofday(timeval*, struct timezone*);
} 
#endif
#endif

/*<std-footer incl-file-exclusion='W_GETTIMEOFDAY_H'>  -- do not edit anything below this line -- */

#endif          /*</std-footer>*/

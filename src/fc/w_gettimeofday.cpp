/*<std-header orig-src='shore'>

 $Id: w_gettimeofday.cpp,v 1.10 1999/06/30 19:23:59 bolo Exp $

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

#ifdef _WINDOWS

/*
 * This is a set of functions needed on NT to mimic
 * unix functionality: rusage, gettimeofday, getrlimit
 */


#include "w.h"
#include <io.h>
#include <w_rusage.h>
#include <w_gettimeofday.h>


void
filetime2timeval(FILETIME &f, timeval &tv)
{
	LARGE_INTEGER li;
	w_assert3(sizeof(li) == sizeof(f));
        memcpy(&li, &f, sizeof(f));
	__int64 i = li.QuadPart;
	i /= 10;	// 100 nano-seconds -> micro-seconds
#define MILLION 1000000
	tv.tv_usec = (int)(i % MILLION);
	tv.tv_sec = (int) (i / MILLION);
}

int getrusage(int x , struct rusage* use)
{
    FILETIME start, stop, kernel, user;
    if( GetProcessTimes(GetCurrentProcess(),
	&start, &stop, &kernel, &user)) {
	/* convert */
        memset(use, '\0', sizeof(struct rusage));
	filetime2timeval(user, use->ru_utime);
	filetime2timeval(kernel, use->ru_stime);

    } else {
	/* failed */
	w_rc_t e = RC(fcWIN32);
	cerr << "GetProcessTimes:" << e << endl;
	return -1;
    }
    return 0;
}

int gettimeofday(struct timeval* tval, struct timezone* tzone) 
{
	struct _timeb tb;
	_ftime(&tb);
	if (tval) {
		tval->tv_sec = tb.time;
		tval->tv_usec = tb.millitm * 1000;
	}
	if (tzone) {
		tzone->tz_minuteswest = 0;
		tzone->tz_dsttime = 0;
	}
	return 0;
}



#endif /* _WINDOWS */


/*<std-header orig-src='shore' incl-file-exclusion='SOLARIS_STATS_H'>

 $Id: solaris_stats.h,v 1.13 2004/10/05 23:19:54 bolo Exp $

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

#ifndef SOLARIS_STATS_H
#define SOLARIS_STATS_H

#include "w_defines.h"

/*  -- do not edit anything above this line --   </std-header>*/

/*
 * stats.h
 *
 * Class definition for the stats class.
 * Member functions are defined in stats.c
 */
#ifdef __GNUG__
#pragma interface
#endif

#include <sys/time.h>
#include <os_types.h>
#include <sys/procfs.h>

#include <iosfwd>

/*
 * unix_stats objects are meant to be used as follows:
 *
 *        s.start();
 *        section of code for which stats are being gathered
 *        s.Stop();
 *        examine gathered unix_stats
 */

#ifdef __GNUC__
#pragma interface
#endif


/*
 * Stats for Unix processes.
 */
class unix_stats {
protected:
    struct timeval	time1;		/* gettimeofday() buffer */
    struct timeval	time2;		/* gettimeofday() buffer */
    struct prusage	usage1;		/* getrusage() buffer */
    struct prusage	usage2;		/* getrusage() buffer */
	int		iterations;

public:
	unix_stats(){};
    float compute_time() const;

    void   start();			/* start gathering stats  */
	// you can start and then stop multiple times-- don't need to continue
    void   stop(int iter=1);			/* stop gathering stats  */
    int    clocktime() const;		/* elapsed real time in micro-seconds */
    int    usertime() const;		/* elapsed user time in micro-seconds */
    int    systime() const;	/* elapsed system time in micro-seconds */
    /* variants */
    int    s_clocktime() const;		/* diff of seconds only */
    int    s_usertime() const;		
    int    s_systime() const;	
    int    us_clocktime() const;	/* diff of microseconds only */
    int    us_usertime() const;		
    int    us_systime() const;	

    int    page_reclaims() const;	/* page reclaims */
    int    page_faults() const;	/* page faults */
    int    swaps() const;			/* swaps */
    int    inblock() const;		/* page-ins */
    int    oublock() const;		/* page-outs */
    int    syscalls() const;		/* system calls */
    int    iochars() const;		/* characters read and written */
    int    vcsw() const;			/* voluntary context swtch */
    int    invcsw() const;			/* involuntary context swtch */
    int    msgsent() const;			/* socket messages sent */
    int    msgrecv() const;			/* socket messages recvd */
    int    signals() const;			/* signals dispatched */

    ostream &print(ostream &o) const;
};

extern ostream& operator<<(ostream&, const unix_stats &s);

float compute_time(const struct timeval *start_time, const struct timeval *end_time);

/*<std-footer incl-file-exclusion='SOLARIS_STATS_H'>  -- do not edit anything below this line -- */

#endif          /*</std-footer>*/

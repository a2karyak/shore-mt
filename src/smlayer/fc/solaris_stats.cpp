/*<std-header orig-src='shore'>

 $Id: solaris_stats.cpp,v 1.12 1999/06/07 19:02:46 kupsch Exp $

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

#ifdef __GNUC__
#pragma implementation "solaris_stats.h"
#endif

#include <stddef.h>
#include <stdlib.h>
#include <unistd.h>

#include <errno.h>
#include <fcntl.h>
#include <sys/ioctl.h>

#include <stream.h>
#include <strstream.h>

#include <w_workaround.h>
#include "solaris_stats.h"

#define MILLION 1000000

#ifndef MAXPATHLEN	/* XXX */
#define	MAXPATHLEN	1024
#endif

static int read_prusage(int pid, prusage_t &usage)
{
	int	procfs;
	int	n;
	char	pathname[MAXPATHLEN+1];
	ostrstream	s(pathname, sizeof(pathname));

	W_FORM(s)("/proc/%05d", pid);
	s << ends;

	procfs = open(s.str(), O_RDONLY, 0);
	if (procfs == -1)
		return errno;

	n = ioctl(procfs, PIOCUSAGE, (char *)&usage);
	if (n == -1) {
		n = errno;
		close(procfs);
		return n;
	}

	close(procfs);

	return 0;
}

void
unix_stats::start() 
{
	iterations = 0; 
	/*
	 * Save the current stats in buffer area 1.
	 */
	read_prusage(getpid(), usage1);
	gettimeofday(&time1, NULL);
	return;
}

void
unix_stats::stop(int iter) 
{
	/*
	* Save the final stats in buffer area 2.
	*/
	gettimeofday(&time2, NULL);
	read_prusage(getpid(), usage2);

	iterations = iter < 1 ? 0 : iter;

	return;
}

int 
unix_stats::clocktime()  const  // in microseconds
{
    return MILLION * (time2.tv_sec - time1.tv_sec) + 
           (time2.tv_usec - time1.tv_usec);
}

int 
unix_stats::usertime()  const // in microseconds
{
    return MILLION * (usage2.pr_utime.tv_sec - usage1.pr_utime.tv_sec) +
           (usage2.pr_utime.tv_nsec - usage1.pr_utime.tv_nsec) / 1000;
}

int 
unix_stats::systime()  const // in microseconds
{
    return MILLION * (usage2.pr_stime.tv_sec - usage1.pr_stime.tv_sec) +
           (usage2.pr_stime.tv_nsec - usage1.pr_stime.tv_nsec) / 1000;
}

int
unix_stats::page_reclaims()  const
{
    return usage2.pr_minf - usage1.pr_minf;
}

int
unix_stats::page_faults()  const
{
    return usage2.pr_majf - usage1.pr_majf;
}

int
unix_stats::swaps()  const
{
    return usage2.pr_nswap - usage1.pr_nswap;
}

int
unix_stats::vcsw()  const
{
    return usage2.pr_vctx - usage1.pr_vctx;
}

int
unix_stats::invcsw()  const
{
    return usage2.pr_ictx - usage1.pr_ictx;
}
int
unix_stats::inblock()  const
{
    return usage2.pr_inblk - usage1.pr_inblk;
}

int
unix_stats::oublock()  const
{
    return usage2.pr_oublk - usage1.pr_oublk;
}

int
unix_stats::syscalls() const
{
	return usage2.pr_sysc - usage1.pr_sysc;
}

int
unix_stats::iochars() const
{
	return usage2.pr_ioch - usage1.pr_ioch;
}

int
unix_stats::msgsent()  const
{
    return usage2.pr_msnd - usage1.pr_msnd;
}
int
unix_stats::msgrecv()  const
{
    return usage2.pr_mrcv - usage1.pr_mrcv;
}

ostream& unix_stats::print(ostream& o) const
{
	int c,u,sys;

	if(iterations== 0) {
		o << "error: unix_stats was never properly \"stop()\"-ed. ";
		return o;
	}
	c=s_clocktime();
	u=s_usertime();
	sys=s_systime();

	o << "RUSAGE STATS: " << endl << endl;

	o << "seconds/iter: ";
	W_FORM(o)("  clk: %6d",(int)(c/iterations)) << " ";
	W_FORM(o)("  usr: %6d",(int)(u/iterations)) << " ";
	W_FORM(o)("  sys: %6d",(int)(sys/iterations)) << " ";
	o << endl;

	W_FORM(o)("minfaults %6d", page_reclaims()) << endl;
	W_FORM(o)("majfaults %6d", page_faults()) << endl;
	W_FORM(o)("swaps     %6d", swaps()) << endl;
	W_FORM(o)("csw       %6d", vcsw()) << endl;
	W_FORM(o)("inv       %6d", invcsw()) << endl;
	W_FORM(o)("inbloc    %6d", inblock()) << endl;
	W_FORM(o)("outblock  %6d", oublock()) << endl;
	W_FORM(o)("iochars   %6d", iochars()) << endl;
	W_FORM(o)("syscalls  %6d", syscalls()) << endl;
	W_FORM(o)("ssignals  %6d", signals()) << endl;
	W_FORM(o)("msgsnd    %6d", msgsent()) << endl;
	W_FORM(o)("msgrcv    %6d", msgrecv()) << endl;
	return o;
}

ostream& operator<<( ostream& o, const unix_stats &s)
{
	return s.print(o);
}

int 
unix_stats::s_clocktime()  const  
{
    return time2.tv_sec - time1.tv_sec;
}

int 
unix_stats::s_usertime()  const 
{
    return usage2.pr_utime.tv_sec - usage1.pr_utime.tv_sec;
}

int 
unix_stats::s_systime()  const 
{
    return usage2.pr_stime.tv_sec - usage1.pr_stime.tv_sec;
}

int 
unix_stats::us_clocktime()  const  // in microseconds
{
    return (time2.tv_usec - time1.tv_usec);
}

int 
unix_stats::us_usertime()  const // in microseconds
{
    return (usage2.pr_utime.tv_nsec - usage1.pr_utime.tv_nsec);
}

int 
unix_stats::us_systime()  const // in microseconds
{
    return (usage2.pr_stime.tv_nsec - usage1.pr_stime.tv_nsec);
}

int 
unix_stats::signals()  const 
{
    return (usage2.pr_sigs- usage1.pr_sigs);
}


float compute_time(const struct timeval* start_time, const struct timeval* end_time)
{
    float seconds, useconds;

    seconds = (float)(end_time->tv_sec - start_time->tv_sec);
    useconds = (float)(end_time->tv_usec - start_time->tv_usec);

    if (useconds < 0.0) {
        useconds = 1000000.0 + useconds;
        seconds--;
    }
    return (seconds + useconds/1000000.0);
}

float 
unix_stats::compute_time() const
{
    return ::compute_time(&time1, &time2);
}


/*<std-header orig-src='shore' incl-file-exclusion='W_RUSAGE_H'>

 $Id: w_rusage.h,v 1.10 2000/01/14 19:34:39 bolo Exp $

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

#ifndef W_RUSAGE_H
#define W_RUSAGE_H

#include "w_defines.h"

/*  -- do not edit anything above this line --   </std-header>*/

#ifdef _WINDOWS
#include <w_winsock.h>
#include <time.h>
#include <sys/timeb.h>
#else
#include <sys/time.h>

#ifdef Linux
    extern "C" {
#endif
#include <sys/resource.h>
#ifdef Linux
    }
#endif
#endif

/* XXX
 * 
 * This needs to be fixed.  The system should not be dependent
 * upon unix rusage facilities.  It should adapt to whatever is 
 * available (if anything)  on the host system.
 */

/* _SYS_RESOURCE_H_ for *BSD unix boxes */
#if !defined(_SYS_RESOURCE_H) && !defined(_SYS_RESOURCE_H_)
#define	_SYS_RESOURCE_H

/*
 * Process priority specifications
 */

#define	PRIO_PROCESS	0
#define	PRIO_PGRP	1
#define	PRIO_USER	2


/*
 * Resource limits
 */

#define	RLIMIT_CPU	0		/* cpu time in milliseconds */
#define	RLIMIT_FSIZE	1		/* maximum file size */
#define	RLIMIT_DATA	2		/* data size */
#define	RLIMIT_STACK	3		/* stack size */
#define	RLIMIT_CORE	4		/* core file size */
#define	RLIMIT_NOFILE	5		/* file descriptors */
#define	RLIMIT_VMEM	6		/* maximum mapped memory */
#define	RLIMIT_AS	RLIMIT_VMEM

#define	RLIM_NLIMITS	7		/* number of resource limits */

#define	RLIM_INFINITY	0x7fffffff

typedef unsigned long rlim_t;

struct rlimit {
	rlim_t	rlim_cur;		/* current limit */
	rlim_t	rlim_max;		/* maximum value for rlim_cur */
};

#define	RUSAGE_SELF	0
#define	RUSAGE_CHILDREN	-1

struct	rusage {
	struct timeval ru_utime;	/* user time used */
	struct timeval ru_stime;	/* system time used */
	long	ru_maxrss;		/* XXX: 0 */
	long	ru_ixrss;		/* XXX: 0 */
	long	ru_idrss;		/* XXX: sum of rm_asrss */
	long	ru_isrss;		/* XXX: 0 */
	long	ru_minflt;		/* any page faults not requiring I/O */
	long	ru_majflt;		/* any page faults requiring I/O */
	long	ru_nswap;		/* swaps */
	long	ru_inblock;		/* block input operations */
	long	ru_oublock;		/* block output operations */
	long	ru_msgsnd;		/* messages sent */
	long	ru_msgrcv;		/* messages received */
	long	ru_nsignals;		/* signals received */
	long	ru_nvcsw;		/* voluntary context switches */
	long	ru_nivcsw;		/* involuntary " */
};



#endif	/* _SYS_RESOURCE_H */

#ifdef HPUX8
#	include <sys/syscall.h>
#	define getrusage(a, b)  syscall(SYS_GETRUSAGE, a, b)

	extern "C" syscall(int, int, void*);
#endif /* HPUX8 */

#ifndef Linux
#ifndef getrusage
	extern "C" int getrusage(int x , struct rusage* use);
#endif /* getrusage */
#endif /* Linux  */

/*<std-footer incl-file-exclusion='W_RUSAGE_H'>  -- do not edit anything below this line -- */

#endif          /*</std-footer>*/

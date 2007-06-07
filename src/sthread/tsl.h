/*<std-header orig-src='shore' incl-file-exclusion='TSL_H'>

 $Id: tsl.h,v 1.23 2002/01/04 06:31:41 bolo Exp $

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

#ifndef TSL_H
#define TSL_H

#include "w_defines.h"

/*  -- do not edit anything above this line --   </std-header>*/

/*
 * The Testandset package is Copyright 1993, 1994, 1995, 1997, 
 * 1998, 1999, 2000, 2001 by:
 *
 *	Josef Burger	<bolo@cs.wisc.edu>
 *
 * All rights reserved.
 *
 * The Testandset package may be freely used as long as credit is given
 * to the author and the above copyright is maintained. 
 */

/*
 * lock definition file for testandset() package.
 */

#ifdef STHREAD_TSL_PTHREAD
#include <pthread.h>
#endif

typedef struct tslcb_t {
#if defined(STHREAD_TSL_PTHREAD)
	pthread_spinlock_t	lock;	
#else
#if defined(hp800) || defined(hppa)
    /*
     * gads, what a kludge
     * 
     * HPPA needs a 16 byte length aligned at a 16 byte boundary.
     * The tsl() package will automatically align the address
     * given to it to the NEXT 16 byte boundary.
     * 
     * If you use malloc, to allocate locks, it will return a
     * 8-byte aligned pointer, and you can use the lock[6] definition
     * for alignment to work correctly.
     *
     * If you use statically-allocated locks, either verify that 
     * the compiler will guarantee a 8-byte alignment for the structure,
     * or use a length of 32 (28 if space is tight) to guarantee
     * enough space for alignment.
     */
#if GUARANTEE_EIGHT
	int	lock[6];	/* MUST be 8-byte aligned */
#else
	int	lock[8];	/* if 4-aligned */
#endif
#endif /* hp800 */

#if defined(ibm032)
	short	lock;
#endif /* ibm032 */

#if defined(hp300) || defined(sparc) || defined(i386) || defined(vax)
	char	lock;
#endif	/* lots of stuff */

#if defined(mips)
	int	lock;
#endif

#if defined(i860) || defined(__i860)
	int	lock;
#endif

#if defined(Rs6000) || defined(PowerPC)
	int	lock;
#endif /* Rs6000 */

#if defined(alpha) || defined(__alpha)
	int	lock;
#endif

#if defined(ia64) || defined(__ia64)
	int	lock;
#endif
#endif
} tslcb_t;

extern "C" {
extern void tsl_init(tslcb_t *it);
extern unsigned tsl(tslcb_t *, int);
extern void tsl_release(tslcb_t *);
extern unsigned tsl_examine(tslcb_t *);
}

/*<std-footer incl-file-exclusion='TSL_H'>  -- do not edit anything below this line -- */

#endif          /*</std-footer>*/

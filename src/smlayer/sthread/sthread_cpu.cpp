/*<std-header orig-src='shore'>

 $Id: sthread_cpu.cpp,v 1.7 2000/01/11 21:44:02 bolo Exp $

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


/*
 *   NewThreads is Copyright 1992, 1993, 1994, 1995, 1996, 1997, 1998 by:
 *
 *	Josef Burger	<bolo@cs.wisc.edu>
 *	Dylan McNamee	<dylan@cse.ogi.edu>
 *      Ed Felten       <felten@cs.princeton.edu>
 *
 *   All Rights Reserved.
 *
 *   NewThreads may be freely used as long as credit is given
 *   to the above authors and the above copyright is maintained.
 */

#include <sthread.h>

#if !defined(STHREAD_CORE_PTHREAD) && !defined(STHREAD_CORE_WIN32)
#define	STHREAD_CORE_STD
#endif

#if defined(STHREAD_CORE_PTHREAD)
#include <pthread.h> 
#include "stcore_pthread.h"
#elif defined(STHREAD_CORE_WIN32)
#include "stcore_win32.h" 
#elif defined(STHREAD_CORE_STD)
#include "stcore.h"
#else
#error "thread core not selected"
#endif


#if defined(_WIN32) && defined(I386)
#include <float.h>
#endif


void	sthread_t::_reset_cpu()
{
#ifdef STHREAD_CORE_STD
	if (!_core->use_float)
		return;
#endif

#if defined(_WIN32) && defined(I386)
	/* Adjust precision to 53 bit mantissa (64 bit floats) */
	_controlfp(_MCW_PC, _PC_53);

#elif defined(I386) && defined(Linux)
	/* changing FPU mods breaks the C library */

#elif defined(I386) && defined(__GNUG__)
	/* XXX If this breaks your C runtime, it will need to be disabled */
	
	/* Form a 80387 floating point control word to have the
	 * following modes:
	 *
	 * RC=0		: Round to nearest or even
	 * PC=2		: 53 bit mantissa (64 bit fp, DOUBLE PRECISION)
	 * Mask all FP exceptions
	 */

	const	int	cw = 0x0027f;
	asm("fldcw %0" : : "m" (cw));
#elif defined(I386) && defined(W_DEBUG)
	cerr << "Warning: can't set 80387 control word" << endl;
#endif
}

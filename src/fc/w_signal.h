/*<std-header orig-src='shore' incl-file-exclusion='W_SIGNAL_H'>

 $Id: w_signal.h,v 1.21 2006/01/29 21:34:45 bolo Exp $

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

#ifndef W_SIGNAL_H
#define W_SIGNAL_H

#include "w_defines.h"

/*  -- do not edit anything above this line --   </std-header>*/

/* 
 * workarounds for 
 * various systems' signal definitions, some of which
 * are wrong (not ANSI-C, even though they claim to be)
 */
#ifndef W_WORKAROUND_H
#include <w_workaround.h>
#endif

#ifdef GNUG_BUG_8
#define signal __gnubug_signal__
#endif 

#ifndef _WINDOWS
#define	POSIX_SIGNALS
#endif

#include <cstdlib>
#include <csignal>

#ifdef GNUG_BUG_8
#undef signal
extern "C" void (*signal(int sig, void (*handler)(int)))(int);
#endif 

/* ANSI standard C defines signal() and handler: */
typedef void (*_W_ANSI_C_HANDLER)( int );
#define W_ANSI_C_HANDLER (_W_ANSI_C_HANDLER) /* type-cast */

/* POSIX defines sigaction() and  its handler differently from standard C */
/* if you want to use the full 3 argument handler and your posix
   system doesn't have the sa_sigaction union member, use this cast
   to coerce it into sa_handler. */
typedef void (*_W_POSIX_HANDLER)(int);
#define W_POSIX_HANDLER (_W_POSIX_HANDLER) /* type-cast */

/* prevent use of bsd functions -- use posix ones instead */
#if 0
#ifdef sigblock
#undef sigblock
#endif
#define sigblock do not use BSD signal functions

#ifdef sigsetmask
#undef sigsetmask
#endif
#define sigsetmask do not use BSD signal functions

#ifdef sigmask
#undef sigmask
#endif
#define sigmask do not use BSD signal functions
#endif /*0*/

/*<std-footer incl-file-exclusion='W_SIGNAL_H'>  -- do not edit anything below this line -- */

#endif          /*</std-footer>*/

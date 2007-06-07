/*<std-header orig-src='shore' incl-file-exclusion='W_WINDOWS_H'>

 $Id: w_windows.h,v 1.14 2000/01/14 19:34:40 bolo Exp $

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

#ifndef W_WINDOWS_H
#define W_WINDOWS_H

#include "w_defines.h"

/*  -- do not edit anything above this line --   </std-header>*/

#ifdef _WINDOWS

#ifndef  __ANSI_CPP__
#define __ANSI_CPP__
#endif /* __ANSI_CPP__ */
/*
 * Here is where you put all your _WINDOW-specific
 * configuration stuff, since (at least, to begin with)
 * making under NT doesn't use the imake, so it doesn't
 * get all the configuration stuff given there
 */
#define ON 1
#define OFF 0

#if defined(DEBUGCODE) && DEBUGCODE == ON
#ifndef DEBUG
#	define DEBUG
#	define W_DEBUG
#endif
#	undef NDEBUG
/* DEBUGCODE ON */
#else
#	undef DEBUG
#	undef W_DEBUG
#ifndef NDEBUG
#	define NDEBUG
#endif
/* DEBUGCODE OFF */
#endif /* DEBUGCODE */

#undef ON 
#undef OFF 

#ifndef W_CONFIG_ONLY
#include <w_winsock.h>
#include <os_types.h>

#include <string.h>
#include <memory.h>

typedef long mode_t;
typedef void* caddr_t;
typedef long uid_t;
typedef long gid_t;
typedef unsigned pid_t;
typedef unsigned int uint;
typedef unsigned short u_short;

#ifdef max
#undef max
#undef min
#endif /* max */

#define DEV_BSIZE 512 


#ifndef NBBY
#define NBBY 8
#endif /*NBBY*/


#define strcasecmp stricmp
#define strncasecmp strnicmp

#endif /* !W_CONFIG_ONLY */

#if defined(_M_IX86)  && (!defined(I386)  || !defined(i386))
#define i386
#endif
#endif  /* _WINDOWS */

/*<std-footer incl-file-exclusion='W_WINDOWS_H'>  -- do not edit anything below this line -- */

#endif          /*</std-footer>*/

/*<std-header orig-src='shore' incl-file-exclusion='CAT_H'>

 $Id: cat.h,v 1.6 1999/06/07 19:02:41 kupsch Exp $

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

#ifndef CAT_H
#define CAT_H

#include "w_defines.h"

/*  -- do not edit anything above this line --   </std-header>*/

/*
 * NB -- THIS FILE MUST BE LEGITIMATE INPUT TO cc and RPCGEN !!!!
 * Please do as follows:
 * a) keep all comments in traditional C style.
 * b) If you put something c++-specific make sure it's 
 * 	  got ifdefs around it
 */

#if defined(__STRICT_ANSI__)||defined(__GNUC__)||defined(__STDC__)||defined(__ANSI_CPP__)
#	define __cat(a,b) a##b
#	define _cat(a,b) __cat(a,b)
#	define _string(a) #a
#else

#ifdef COMMENT
/*  For compilers that don't understand # and ##, try one of these: 
//# error	preprocessor does not understand ANSI catenate and string.
*/

#		define _cat(a,b) a\
b
#		define _cat(a,b) a/**/b
#		define _string(a) "a"

#endif

#ifdef sparc
#		define _cat(a,b) a\
b
#		define _string(a) "a"
#endif

#endif

/*<std-footer incl-file-exclusion='CAT_H'>  -- do not edit anything below this line -- */

#endif          /*</std-footer>*/

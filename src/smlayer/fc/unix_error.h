/*<std-header orig-src='shore' incl-file-exclusion='UNIX_ERROR_H'>

 $Id: unix_error.h,v 1.16 1999/06/07 19:02:47 kupsch Exp $

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

#ifndef UNIX_ERROR_H
#define UNIX_ERROR_H

#include "w_defines.h"

/*  -- do not edit anything above this line --   </std-header>*/

#include <errno.h>


#ifndef _WINDOWS
/* Some boxes need errno declared, others have it in errno.h */

#if !defined(SOLARIS2)
extern int errno;
#endif

#if defined(Linux)
extern const char *const sys_errlist[];
#elif !defined(__NetBSD__)
extern char *sys_errlist[];
#endif

extern int sys_nerr;
#endif

#ifndef _WINDOWS 
#ifdef __cplusplus
extern "C" {
#endif /*__cplusplus*/


    /* gcc 2.6.0 include files contain perror */
#   if (!defined(__GNUC__)) || (__GNUC_MINOR__ < 6)
    void perror(const char *s);
#   endif

char *strerror(int errnum);

#ifdef __cplusplus
}
#endif /*__cplusplus*/
#endif /* !_WINDOWS */

extern int last_unix_error();
extern void format_unix_error(int, char *, int);

/*<std-footer incl-file-exclusion='UNIX_ERROR_H'>  -- do not edit anything below this line -- */

#endif          /*</std-footer>*/

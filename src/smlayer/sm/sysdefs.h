/*<std-header orig-src='shore' incl-file-exclusion='SYSDEFS_H'>

 $Id: sysdefs.h,v 1.33 1999/06/07 19:04:45 kupsch Exp $

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

#ifndef SYSDEFS_H
#define SYSDEFS_H

#include "w_defines.h"

/*  -- do not edit anything above this line --   </std-header>*/

#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#ifndef _MSC_VER
#include <unistd.h>
#endif

#if defined(_MSC_VER) && defined small
/* XXX namespace corruption in MSC header files */
#undef small
#endif

#include <limits.h>
#ifdef Decstation
#include <sysent.h>
#endif
#include <w_rusage.h>
#include <os_types.h>

#ifdef _WINDOWS
/* This stuff is picked up from winsock.h which is included globally
   at the moment.  That will change soon, but global visibility
   of networking stuff is not something to have. */
#else
#include <sys/socket.h>
#include <sys/signal.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <netdb.h>
#endif

#include <setjmp.h>
#include <new.h>
#include <w_stream.h>
//#include <rpc/rpc.h>

#define W_INCL_LIST
#include <w.h>
#include <sthread.h>

/*<std-footer incl-file-exclusion='SYSDEFS_H'>  -- do not edit anything below this line -- */

#endif          /*</std-footer>*/

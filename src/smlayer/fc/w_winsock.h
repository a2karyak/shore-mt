/*<std-header orig-src='shore' incl-file-exclusion='W_WINSOCK_H'>

 $Id: w_winsock.h,v 1.1 2000/01/13 23:48:02 bolo Exp $

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

#ifndef W_WINSOCK_H
#define W_WINSOCK_H

#include "w_defines.h"

/*  -- do not edit anything above this line --   </std-header>*/

/* XXX fix a namespace collision between paradise and winsock */
#define	int32	winsock_int32

#ifdef OLD_WINSOCK
#include <winsock.h>
#else
#include <winsock2.h>
#endif

#undef	int32

/*<std-footer incl-file-exclusion='W_WINSOCK_H'>  -- do not edit anything below this line -- */

#endif          /*</std-footer>*/

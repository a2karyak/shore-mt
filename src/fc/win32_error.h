/*<std-header orig-src='shore' incl-file-exclusion='WIN32_ERROR_H'>

 $Id: win32_error.h,v 1.5 1999/06/07 19:03:00 kupsch Exp $

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

#ifndef WIN32_ERROR_H
#define WIN32_ERROR_H

#include "w_defines.h"

/*  -- do not edit anything above this line --   </std-header>*/

extern int last_win32_error();
extern void format_win32_error(int error, char *, int);


/*<std-footer incl-file-exclusion='WIN32_ERROR_H'>  -- do not edit anything below this line -- */

#endif          /*</std-footer>*/

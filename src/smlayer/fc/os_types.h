/*<std-header orig-src='shore' incl-file-exclusion='OS_TYPES_H'>

 $Id: os_types.h,v 1.4 1999/06/07 19:02:46 kupsch Exp $

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

#ifndef OS_TYPES_H
#define OS_TYPES_H

#include "w_defines.h"

/*  -- do not edit anything above this line --   </std-header>*/

#include <sys/types.h>

#if defined(LARGEFILE_AWARE)


#if defined(SOLARIS2)
/* Everything is built with -D_FILE_OFFSET_BITS=64 and
 * -D_LARGEFILE64_SOURCE.  This causes off_t to be redefined
 */
#endif

#endif /* LARGEFILE_AWARE */


/*<std-footer incl-file-exclusion='OS_TYPES_H'>  -- do not edit anything below this line -- */

#endif          /*</std-footer>*/

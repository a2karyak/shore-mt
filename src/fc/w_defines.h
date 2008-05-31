/*<std-header orig-src='shore' incl-file-exclusion='W_DEFINES_H' no-defines='true'>

 $Id: w_defines.h,v 1.3 2008/05/31 05:03:26 nhall Exp $

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

#ifndef W_DEFINES_H
#define W_DEFINES_H

/*  -- do not edit anything above this line --   </std-header>*/

#include "shore.def"
/* shore-config.h does not have duplicate-include protection, but
   this file does, and we don't include shore-config.h anywhere else*/
#include "shore-config.h"

// OK, shore.def might or might not define SM_PAGESIZE
// If not, then shore-config.h defines it to 8K
// shore-config.h can define LARGE_PAGE based on config option --enable-largepages
// Now we need to force largepages if SM_PAGESIZE is too big
#if SM_PAGESIZE > 32768
#define LARGE_PAGE
#endif

/* Adapt shore to namespaces */
#if defined(__cplusplus) && !defined(USE_CPP_NAMESPACE_NO)
#ifdef _MSC_VER
namespace std {};
#endif
using namespace std;
#endif

/*<std-footer incl-file-exclusion='W_DEFINES_H'>  -- do not edit anything below this line -- */

#endif          /*</std-footer>*/

/*<std-header orig-src='shore' incl-file-exclusion='SM_VAS_H'>

 $Id: sm_vas.h,v 1.28 1999/06/07 19:02:31 kupsch Exp $

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

#ifndef SM_VAS_H
#define SM_VAS_H

#include "w_defines.h"

/*  -- do not edit anything above this line --   </std-header>*/

/*
 * sm_vas.h is the include file that all value added servers should
 * to get access to the Shore Server storage manager interface.
 */
#include <stddef.h>
#include <w_stream.h>

#include "w.h"
#include "option.h"
#include "basics.h"
#include "lid_t.h"
#include "vec_t.h"
#include "zvec_t.h"
#include "tid_t.h"
#include "stid_t.h"

#undef SM_SOURCE
#undef SM_LEVEL
#include "sm.h"
#include "pin.h"
#include "scan.h"
#include "kvl_t.h" // define kvl_t for lock_base_t
#include "lock_s.h" // define lock_base_t

#include "sort.h" // define sort_stream_i
#include "sort_s.h" // key_info_t

/*<std-footer incl-file-exclusion='SM_VAS_H'>  -- do not edit anything below this line -- */

#endif          /*</std-footer>*/

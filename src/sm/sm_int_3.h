/*<std-header orig-src='shore' incl-file-exclusion='SM_INT_3_H'>

 $Id: sm_int_3.h,v 1.9 1999/06/07 19:04:37 kupsch Exp $

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

#ifndef SM_INT_3_H
#define SM_INT_3_H

#include "w_defines.h"

/*  -- do not edit anything above this line --   </std-header>*/

#if defined(SM_SOURCE) && !defined(SM_LEVEL)
#    define SM_LEVEL 3
#endif

#ifndef SM_INT_2_H
#include "sm_int_2.h"
#endif

class dir_m;

class smlevel_3 : public smlevel_2 {
public:
    enum sm_store_property_t {
	t_regular 	= 0x1,
	t_temporary	= 0x2,
	t_load_file	= 0x4,	// allowed only in create, these files start out
				// as temp and are converted to regular on commit
	t_insert_file = 0x08,	// current pages are fully logged, new pages
				// are not logged.  EX lock is acquired on file.
				// only valid with a normal file, not indices.
	t_bad_storeproperty = 0x80// no bits in common with good properties
    };
    static dir_m*	dir;
};

ostream&
operator<<(ostream& o, smlevel_3::sm_store_property_t p);

#if (SM_LEVEL >= 3)
#    include <dir.h>
#endif

/*<std-footer incl-file-exclusion='SM_INT_3_H'>  -- do not edit anything below this line -- */

#endif          /*</std-footer>*/

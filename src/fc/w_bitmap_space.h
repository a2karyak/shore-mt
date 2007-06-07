/*<std-header orig-src='shore' incl-file-exclusion='W_BITMAP_SPACE_H'>

 $Id: w_bitmap_space.h,v 1.8 1999/06/07 19:02:50 kupsch Exp $

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

#ifndef W_BITMAP_SPACE_H
#define W_BITMAP_SPACE_H

#include "w_defines.h"

/*  -- do not edit anything above this line --   </std-header>*/

#include <w_base.h>

/*
 * The w_bitmap_space_t template provides space for a bitmap
 * and uses w_bitmap_t to implement the operations on the map.
 */

template <unsigned NBITS>
class w_bitmap_space_t : public w_bitmap_t {
public:
    NORET			w_bitmap_space_t() 
	: w_bitmap_t(_space, NBITS)	{};
    NORET			~w_bitmap_space_t()	{};
    NORET			w_bitmap_space_t(const w_bitmap_space_t& bm) 
	: w_bitmap_t(_space, NBITS)  {
	memcpy(_space, bm._space, (NBITS - 1)/8 + 1);
    }
    w_bitmap_space_t&		operator=(const w_bitmap_space_t& bm) {
	memcpy(_space, bm._space, (NBITS - 1)/8 + 1);
	return *this;
    }
private:
    uint1_t			_space[(NBITS - 1)/ 8 + 1];
};

/*<std-footer incl-file-exclusion='W_BITMAP_SPACE_H'>  -- do not edit anything below this line -- */

#endif          /*</std-footer>*/

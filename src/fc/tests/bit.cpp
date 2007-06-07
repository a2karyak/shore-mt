/*<std-header orig-src='shore'>

 $Id: bit.cpp,v 1.20 2004/10/11 21:59:35 bolo Exp $

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

#include "w_defines.h"

/*  -- do not edit anything above this line --   </std-header>*/

#define W_INCL_BITMAP
#include "w.h"

#include "w_bitmap_space.h"

#include <iostream>

const unsigned nbits = 71;
const unsigned nbytes = (nbits - 1) / 8 + 1;

#ifdef EXPLICIT_TEMPLATE
template class w_bitmap_space_t<nbits>;
#endif

int main()
{
    w_bitmap_space_t<nbits> bm;

    // test zero()
    bm.zero();
    unsigned i;
    for (i = 0; i < nbits; i++)  {
	w_assert1(bm.is_clr(i));
    }

    // test fill()
    bm.fill();
    for (i = 0; i < nbits; i++)  {
	w_assert1(bm.is_set(i));
    }

    // test random set
    bm.zero();
    for (i = 0; i < 30; i++)  {
	int bit = rand() % nbits;
	bm.set(bit);
	w_assert1(bm.is_set(bit));
    }

    // test random clear
    bm.fill();
    for (i = 0; i < 30; i++)  {
	int bit = rand() % nbits;
	bm.clr(bit);
	w_assert1(bm.is_clr(bit));
    }

    // test set first/last bit
    bm.zero();
    bm.set(0);
    w_assert1(bm.is_set(0));
    bm.set(nbits - 1);
    w_assert1(bm.is_set(nbits - 1));

    // test clear first/last bit
    w_assert1(! bm.is_clr(0) );
    bm.clr(0);
    w_assert1(bm.is_clr(0));
    
    w_assert1(! bm.is_clr(nbits - 1));
    bm.clr(nbits - 1);
    w_assert1(bm.is_clr(nbits - 1));
	      
    
    // test first set
    for (i = 0; i < 100; i++)  {
	bm.zero();
	int bit = rand() % nbits;
	bm.set(bit);
	
	w_assert1(bm.first_set(0) == bit);
	w_assert1(bm.first_set(bit) == bit);
    }
    bm.zero();
    bm.set(0);
    w_assert1(bm.first_set(0) == 0);
    w_assert1(bm.first_set(1) == -1);

    bm.zero();
    bm.set(nbits - 1);
    w_assert1(bm.first_set(0) == nbits - 1);
    w_assert1(bm.first_set(nbits - 1) == nbits - 1);

    // test first clr
    for (i = 0; i < 100; i++)  {
	bm.fill();
	int bit = rand() % nbits;
	bm.clr(bit);
	
	w_assert1(bm.first_clr(0) == bit);
	w_assert1(bm.first_clr(bit) == bit);
    }
    bm.fill();
    bm.clr(0);
    w_assert1(bm.first_clr(0) == 0);
    w_assert1(bm.first_clr(1) == -1);

    bm.fill();
    bm.clr(nbits - 1);
    w_assert1(bm.first_clr(0) == nbits - 1);
    w_assert1(bm.first_clr(nbits - 1) == nbits - 1);

    cout << "bitmap tests OK." << endl;
    return 0;
}


/*<std-header orig-src='shore'>

 $Id: serial_t.cpp,v 1.34 2001/06/23 06:06:18 bolo Exp $

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

#define SERIAL_T_C

#ifdef __GNUG__
#pragma implementation "serial_t.h"
#pragma implementation "serial_t_data.h"
#endif

#include <stdlib.h>
#include <w_stream.h>
#include "basics.h"
#include "serial_t.h"
#include "dual_assert.h"

#ifdef HP_CC_BUG_1
const uint4_t serial_t::mask_remote = 0x80000000;
const uint4_t serial_t::max_inc = ((1<<overflow_shift_bits)-1);
const uint4_t serial_t::max_any = serial_t::max_inc;
const uint4_t serial_t::other_bits = ~max_inc;
#endif /* HP_CC_BUG_1 */

/*
 * Set up serial number constants
 */
const serial_t serial_t::max_local(serial_t::max_any, false);  // all bits but high
const serial_t serial_t::max_remote(serial_t::max_any, true);  // all bits set
const serial_t serial_t::null;  		  // only low bit  set

const char* serial_t::key_descr =
#   ifdef BITS64
					"u4u4";
#   else
					"u4";
#   endif /*BITS64*/


ostream& operator<<(ostream& o, const serial_t_data& g)
{
    return o 
#ifdef BITS64
	<< g._high << "."
#endif
	<< g._low;
}

istream& operator>>(istream& i, serial_t_data& g)
{
#ifdef BITS64
    char c;
#endif

    return i 
#ifdef BITS64
	>> g._high  >> c 
#endif
	>> g._low;
}

ostream& operator<<(ostream& o, const serial_t& s)
{
    return o << s.data;
}

istream& operator>>(istream& i, serial_t& s)
{
    return i >> s.data;
}

bool
serial_t::_incr(
	uint4_t		*what,
	uint4_t		amt,
	uint4_t		*overflow
)
{
	uint4_t	temp = *what;
	temp += amt;
	if( ((*overflow) = (temp & ~max_any)) ) {
		// overflow occurred

		// compiler apparently doesn't do what we would like
		// it to do with (*overflow) >>= bits
		(*overflow) = (*overflow) >> overflow_shift_bits;

		temp -= max_any;

		*what = temp;
		return 1;
	}
	*what = temp;
	return 0;
}

bool 
serial_t::increment(uint4_t amount) 
{ 
	// higher layer has to enforce this:
	dual_assert3(amount < max_inc); 

#ifdef BITS64
	bool	 was_remote = ((data._high & mask_remote)==mask_remote);
#else
	bool	 was_remote = ((data._low & mask_remote)==mask_remote);
#endif
	bool	 was_ondisk = ((data._low & mask_disk)==mask_disk);

	// don't change this if overflow occurs; use temp variables
	uint4_t	l, h, overflow;

	// turn off remote
#ifdef BITS64
	h = data._high;
	data._high &= ~mask_remote;
#else
	h = data._low;
	data._low &= ~mask_remote;
#endif

	// get low half, shifted
	l = data._low>>1;

	// increment the low half
	if( _incr(&l, amount, &overflow)) {
		// lower half overflowed

#ifndef BITS64
		goto _overflow;
#else
		// don't compile this if we only have a low half
		// h &= ~mask_remote;  this had to be done earlier
		// to make the code work with 32 bits

		if(_incr(&h, overflow, &overflow) ) {
			// the whole thing overflowed
			goto _overflow;
		}
		data._high = h;
#endif
	}

// nooverflow:
	data._low = ((l<<1)&~mask_disk);
	if(was_ondisk) {
		data._low |= mask_disk;
	}
	if(was_remote) {
#ifdef BITS64
		data._high |= mask_remote;
#else
		data._low |= mask_remote;
#endif
	}
	dual_assert3(was_remote == is_remote());
	dual_assert3(was_ondisk == is_on_disk());
	return 0;

_overflow:
	// clean up even though there was an error
	if(was_ondisk) {
		data._low |= mask_disk;
	}
	if(was_remote) {
#ifdef BITS64
		data._high |= mask_remote;
#else
		data._low |= mask_remote;
#endif
	}
	dual_assert3(was_remote == is_remote());
	dual_assert3(was_ondisk == is_on_disk());
	return 1; // caller has to handle this error
}


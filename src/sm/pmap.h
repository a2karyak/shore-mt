/*<std-header orig-src='shore' incl-file-exclusion='PMAP_H'>

 $Id: pmap.h,v 1.10 2008/05/28 01:28:02 nhall Exp $

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

#ifndef PMAP_H
#define PMAP_H

#include "w_defines.h"

/*  -- do not edit anything above this line --   </std-header>*/

#ifdef __GNUG__
#pragma interface
#endif

#include <bitmap.h>

struct Pmap {
	/* number of bits */
	enum	{ _count = smlevel_0::ext_sz };
	/* number of bytes */
	enum	{ _size = smlevel_0::ext_map_sz_in_bytes };

	u_char bits[_size];

	inline	Pmap() {
		clear_all();
	}

	inline	void	set(int bit) { bm_set(bits, bit); }
	inline	void	clear(int bit) { bm_clr(bits, bit); }

	inline	bool	is_set(int bit) const { return bm_is_set(bits, bit); }
	inline	bool	is_clear(int bit) const { return bm_is_clr(bits, bit);}

	inline	int	num_set() const { return bm_num_set(bits, _count); }
	inline	int	num_clear() const { return bm_num_clr(bits, _count); }

	inline	int	first_set(int start) const {
		return bm_first_set(bits, _count, start);
	}
	inline	int	first_clear(int start) const {
		return bm_first_clr(bits, _count, start);
	}
	inline	int	last_set(int start) const {
		return bm_last_set(bits, _count, start);
	}
	inline	int	last_clear(int start) const {
		return bm_last_clr(bits, _count, start);
	}

	inline	unsigned	size() const { return _size; }
	inline	unsigned	count() const { return _count; }

#ifdef notyet
	inline	bool	is_empty() const { return (num_set() == 0); } 
#else
	/* bm_num_set is too expensive for this use.
	 XXX doesn't work if #bits != #bytes * 8 */
	inline	bool	is_empty() const {
		unsigned	i;
		for (i = 0; i < _size; i++)
			if (bits[i])
				break;
		return (i == _size);
	}
#endif
	inline	void	clear_all() { bm_zero(bits, _count); }
	inline	void	set_all() { bm_fill(bits, _count); }

	ostream	&print(ostream &s) const;
};

extern	ostream &operator<<(ostream &, const Pmap &pmap);

/* Aligned Pmaps -- actually, align the END of a Pmap so that
 * the other members of an extlink_t are properly aligned
 */

#define PMAP_SIZE_IN_BYTES ((SM_EXTENTSIZE + 7)/8)

#if ((PMAP_SIZE_IN_BYTES & 0x7) == 0x0)
typedef	Pmap	Pmap_Align;
#elif ((PMAP_SIZE_IN_BYTES & 0x7) == 0x1)
class Pmap_Align : public Pmap {
public:
	inline	Pmap_Align	&operator=(const Pmap &from) {
		*(Pmap *)this = from;	// don't copy the filler
		return *this;
	}
private:
	fill1	filler;		// keep purify happy
	fill2   filler2;	// ditto, as well as assert 
				// in extlink_t::extlink_t()
	fill4   filler3;	// ditto, as well as assert 
				// in extlink_t::extlink_t()
};
#elif ((PMAP_SIZE_IN_BYTES & 0x7) == 0x2)
class Pmap_Align : public Pmap {
public:
	inline	Pmap_Align	&operator=(const Pmap &from) {
		*(Pmap *)this = from;	// don't copy the filler
		return *this;
	}
private:
	// fill1	filler;		// keep purify happy
	fill2   filler2;	// ditto, as well as assert 
				// in extlink_t::extlink_t()
	fill4   filler3;	// ditto, as well as assert 
				// in extlink_t::extlink_t()
};
#elif ((PMAP_SIZE_IN_BYTES & 0x7) == 0x3)
class Pmap_Align : public Pmap {
public:
	inline	Pmap_Align	&operator=(const Pmap &from) {
		*(Pmap *)this = from;	// don't copy the filler
		return *this;
	}
private:
	fill1	filler;		// keep purify happy
	// fill2   filler2;	// ditto, as well as assert 
				// in extlink_t::extlink_t()
	fill4   filler3;	// ditto, as well as assert 
				// in extlink_t::extlink_t()
};
#elif ((PMAP_SIZE_IN_BYTES & 0x7) == 0x4)
class Pmap_Align : public Pmap {
public:
	inline	Pmap_Align	&operator=(const Pmap &from) {
		*(Pmap *)this = from;	// don't copy the filler
		return *this;
	}
private:
	// fill1	filler;		// keep purify happy
	// fill2   filler2;	// ditto, as well as assert 
				// in extlink_t::extlink_t()
	fill4   filler3;	// ditto, as well as assert 
				// in extlink_t::extlink_t()
};
#elif ((PMAP_SIZE_IN_BYTES & 0x7) == 0x5)
class Pmap_Align : public Pmap {
public:
	inline	Pmap_Align	&operator=(const Pmap &from) {
		*(Pmap *)this = from;	// don't copy the filler
		return *this;
	}
private:
	fill1	filler;		// keep purify happy
	fill2   filler2;	// ditto, as well as assert 
				// in extlink_t::extlink_t()
	// fill4   filler3;	// ditto, as well as assert 
				// in extlink_t::extlink_t()
};
#elif ((PMAP_SIZE_IN_BYTES & 0x7) == 0x6)
class Pmap_Align : public Pmap {
public:
	inline	Pmap_Align	&operator=(const Pmap &from) {
		*(Pmap *)this = from;	// don't copy the filler
		return *this;
	}
private:
	// fill1	filler;		// keep purify happy
	fill2   filler2;	// ditto, as well as assert 
				// in extlink_t::extlink_t()
	// fill4   filler3;	// ditto, as well as assert 
				// in extlink_t::extlink_t()
};
#elif ((PMAP_SIZE_IN_BYTES & 0x7) == 0x7)
class Pmap_Align : public Pmap {
public:
	inline	Pmap_Align	&operator=(const Pmap &from) {
		*(Pmap *)this = from;	// don't copy the filler
		return *this;
	}
private:
	fill1	filler;		// keep purify happy
	// fill2   filler2;	// ditto, as well as assert 
				// in extlink_t::extlink_t()
	// fill4   filler3;	// ditto, as well as assert 
				// in extlink_t::extlink_t()
};
#endif

/*<std-footer incl-file-exclusion='PMAP_H'>  -- do not edit anything below this line -- */

#endif          /*</std-footer>*/

/*<std-header orig-src='shore' incl-file-exclusion='W_SPTR_H'>

 $Id: w_sptr.h,v 1.7 1999/06/07 19:02:57 kupsch Exp $

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

#ifndef W_SPTR_H
#define W_SPTR_H

#include "w_defines.h"

/*  -- do not edit anything above this line --   </std-header>*/

/*********************************************************************
 *
 *  class w_sptr_t<T>
 *
 *  Internal use.
 *
 *********************************************************************/
template <class T>
class w_sptr_t {
public:
    NORET			w_sptr_t() : _value(0)  {};
    NORET			w_sptr_t(T* t) 
	: _value((w_base_t::u_long)t) {};
    NORET			w_sptr_t(const w_sptr_t& p)
	: _value(p._value & ~1)  {};
    NORET			~w_sptr_t()  {};

    w_sptr_t&			operator=(const w_sptr_t& p)  {
	_value = p._value & ~1;
	return *this;
    }
    T*				ptr() const  {
	return (T*) (_value & ~1);
    }
    T*				operator->() const {
	return ptr();
    }
    T&				operator*() const  {
	return *ptr();
    }

    void			set_val(T* t)  {
	_value = (w_base_t::u_long)t;
    }
    void			set_val(const w_sptr_t& p) {
	_value = p._value & ~1;
    }

    void			set_flag() {
	_value |= 1;
    }
    void 			clr_flag()  {
	_value &= ~1;
    }

    bool		is_flag() const {
	return _value & 1;
    }
private:
    w_base_t::u_long		_value;
};

/*<std-footer incl-file-exclusion='W_SPTR_H'>  -- do not edit anything below this line -- */

#endif          /*</std-footer>*/

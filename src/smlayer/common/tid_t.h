/*<std-header orig-src='shore' incl-file-exclusion='TID_T_H'>

 $Id: tid_t.h,v 1.57 1999/06/07 19:02:34 kupsch Exp $

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

#ifndef TID_T_H
#define TID_T_H

#include "w_defines.h"

/*  -- do not edit anything above this line --   </std-header>*/

#ifdef PURIFY_ZERO
#include <memory.h>
#endif

#include <ctype.h>
#include <string.h>

#ifndef W_BASE_H
#include <w_base.h>
#endif

/* Include netinet stuff for ntoh() and hton() */
#ifdef _WINDOWS
#ifdef OLD_WINSOCK
#include <winsock.h>
#else
#include <winsock2.h>
#endif
#else
#include <netinet/in.h>
#endif /* !_WINDOWS */

#if defined(AIX41)
/* giant kludge for aix namespace problems */
#include "aix_tid_t.h"
#else

#ifdef __GNUG__
#pragma interface
#endif

class tid_t {
public:
    enum { hwm = max_uint4 };

    tid_t(uint4_t l = 0, uint4_t h = 0) : hi(h), lo(l)             {};
    tid_t(const tid_t& t) : hi(t.hi), lo(t.lo)  {};

    uint4_t get_hi() const { return hi; }    // smuf-sc3-ip5
    uint4_t get_lo() const { return lo; }    // smuf-sc3-ip5

    tid_t& operator=(const tid_t& t)    {
        lo = t.lo, hi = t.hi;
        return *this;
    }

    operator const bool() const  { 
	return (hi == 0 && lo == 0); 
    }

    tid_t& incr()       {
	if (++lo > uint4_t(hwm)) ++hi, lo = 0;
        return *this;
    }

    friend inline ostream& operator<<(ostream&, const tid_t&);
    friend inline istream& operator>>(istream& i, tid_t& t);
    inline bool operator==(const tid_t& tid) const  {
	return this->hi == tid.hi && this->lo == tid.lo;
    }
    inline bool operator!=(const tid_t& tid) const  {
	return !(*this == tid);
    }
    inline bool operator<(const tid_t& tid) const  {
	return (this->hi < tid.hi) || (this->hi == tid.hi && this->lo < tid.lo);
    }
    inline bool operator<=(const tid_t& tid) const  {
	return !(tid < *this);
    }
    inline bool operator>(const tid_t& tid) const  {
	return (tid < *this);
    }
    inline bool operator>=(const tid_t& tid) const  {
	return !(*this < tid);
    }

    static const tid_t Max;
    static const tid_t null;

#define max_tid  (tid_t::Max)
#define null_tid (tid_t::null)

    
private:

    uint4_t       hi;
    uint4_t       lo;
};


#define max_gtid_len  256
#define max_server_handle_len  100

template <int LEN> class opaque_quantity;
template <int LEN> 
class opaque_quantity {

private:

	uint4_t         _length;
	unsigned char _opaque[LEN];

    public:
	opaque_quantity() {
	    (void) set_length(0);
#ifdef PURIFY_ZERO
	    memset(_opaque, '\0', LEN);
#endif
	}
	opaque_quantity(const char* s)
	{
#ifdef PURIFY_ZERO
	    memset(_opaque, '\0', LEN);
#endif
		*this = s;
	}

	friend bool
	operator== BIND_FRIEND_OPERATOR_PART_2(LEN) (
		const opaque_quantity<LEN>	&l,
		const opaque_quantity<LEN>	&r); 

	friend ostream & 
	operator<< BIND_FRIEND_OPERATOR_PART_2(LEN) (
		ostream &o, 
		const opaque_quantity<LEN>	&b);

	opaque_quantity<LEN>	&
	operator=(const opaque_quantity<LEN>	&r) 
	{
		(void) set_length(r.length());
		memcpy(_opaque,r._opaque,length());
		return *this;
	}
	opaque_quantity<LEN>	&
	operator=(const char* s)
	{
		w_assert3(strlen(s) <= LEN);
		(void) set_length(0);
		while ((_opaque[length()] = *s++))
		    (void) set_length(length() + 1);
		return *this;
	}
	opaque_quantity<LEN>	&
	operator+=(const char* s)
	{
		w_assert3(strlen(s) + length() <= LEN);
		while ((_opaque[set_length(length() + 1)] = *s++))
		    ;
		return *this;
	}
	opaque_quantity<LEN>	&
	operator-=(uint4_t len)
	{
		w_assert3(len <= length());
		(void) set_length(length() - len);
		return *this;
	}
	opaque_quantity<LEN>	&
	append(const void* data, uint4_t len)
	{
		w_assert3(len + length() <= LEN);
		memcpy((void*)&_opaque[length()], data, len);
		(void) set_length(length() + len);
		return *this;
	}
	opaque_quantity<LEN>    &
	zero()
	{
		(void) set_length(0);
		memset(_opaque, 0, LEN);
		return *this;
	}
	opaque_quantity<LEN>    &
	clear()
	{
		(void) set_length(0);
		return *this;
	}
	void *
	data_at_offset(uint i)  const
	{
		w_assert3(i < length());
		return (void*)&_opaque[i];
	}
	uint4_t	      wholelength() const {
		return (sizeof(_length) + length());
	}
	uint4_t	      set_length(uint4_t l) {
		if(is_aligned()) { 
		    _length = l; 
		} else {
		    char *m = (char *)&_length;
		    memcpy(m, &l, sizeof(_length));
		}
		return l;
	}
	uint4_t	      length() const {
		if(is_aligned()) return _length;
		else {
		    uint4_t l;
		    char *m = (char *)&_length;
		    memcpy(&l, m, sizeof(_length));
		    return l;
		}
	}
	void	      ntoh()  {
	    if(is_aligned()) {
		_length = ntohl(_length);
	    } else {
		uint4_t         l = ntohl(length());
		char *m = (char *)&l;
		memcpy(&_length, m, sizeof(_length));
	    }

	}
	void	      hton()  {
	    if(is_aligned()) {
		_length = htonl(_length);
	    } else {
		uint4_t         l = htonl(length());
		char *m = (char *)&l;
		memcpy(&_length, m, sizeof(_length));
	    }
	}
	bool	      is_aligned() const  {
	    return (((int)(&_length) & (sizeof(_length) - 1)) == 0);
	}

	ostream		&print(ostream & o) const {
		o << "opaque[" << _length << "]" ;

		uint4_t print_length = _length;
		if (print_length > LEN) {
			o << "[TRUNC TO LEN=" << LEN << "!!]";
			print_length = LEN;
		}
		o << '"';
		const unsigned char *cp = &_opaque[0];
		for (uint4_t i = 0; i < print_length; i++, cp++) {
			if (isprint(*cp))
			    o << *cp;
			else {
			    W_FORM(o)("\\x%02X", *cp);
			}
		}

		return o << '"';
	}
};


template <int LEN>
bool operator==(const opaque_quantity<LEN> &a,
	const opaque_quantity<LEN>	&b) 
{
	return ((a.length()==b.length()) &&
		(memcmp(a._opaque,b._opaque,a.length())==0));
}

template <int LEN>
ostream & 
operator<<(ostream &o, const opaque_quantity<LEN>	&b) 
{
	return b.print(o);
}

#include <w_stream.h>

inline ostream& operator<<(ostream& o, const tid_t& t)
{
    return o << t.hi << '.' << t.lo;
}

inline istream& operator>>(istream& i, tid_t& t)
{
    char ch;
    return i >> t.hi >> ch >> t.lo;
}


typedef opaque_quantity<max_gtid_len> gtid_t;
typedef opaque_quantity<max_server_handle_len> server_handle_t;

#endif/* AIX kludge */

/*<std-footer incl-file-exclusion='TID_T_H'>  -- do not edit anything below this line -- */

#endif          /*</std-footer>*/

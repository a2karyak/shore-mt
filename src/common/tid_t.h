/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

/*
 *  $Header: /p/shore/shore_cvs/src/common/tid_t.h,v 1.27 1996/07/17 22:42:36 nhall Exp $
 */
#ifndef TID_T_H
#define TID_T_H

#ifdef PURIFY
#ifndef RPCGEN
#include <memory.h>
#endif  /* RPCGEN */
#endif /*PURIFY*/

#if defined(AIX41)
/* giant kludge for aix namespace problems */
#include "aix_tid_t.h"
#else

#ifdef __GNUG__
#pragma interface
#endif

/*
 * NB -- THIS FILE MUST BE LEGITIMATE INPUT TO cc and RPCGEN !!!!
 * Please do as follows:
 * a) keep all comments in traditional C style.
 * b) If you put something c++-specific make sure it's 
 * 	  got ifdefs around it
 */
struct tid_t {
#ifdef	__cplusplus
public:
    enum { hwm = max_uint4 };

    tid_t(uint4 l = 0, uint4 h = 0) : hi(h), lo(l)             {};
    tid_t(const tid_t& t) : hi(t.hi), lo(t.lo)  {};
    tid_t& operator=(const tid_t& t)    {
        lo = t.lo, hi = t.hi;
        return *this;
    }

    operator const void*() const  { 
	return (void*) (hi == 0 && lo == 0); 
    }

    tid_t& incr()       {
	if (++lo > (uint4)hwm) ++hi, lo = 0;
        return *this;
    }

    friend inline ostream& operator<<(ostream&, const tid_t&);
    friend inline istream& operator>>(istream&, tid_t&);
    friend inline operator==(const tid_t& t1, const tid_t& t2)  {
	return t1.hi == t2.hi && t1.lo == t2.lo;
    }
    friend inline operator!=(const tid_t& t1, const tid_t& t2)  {
	return ! (t1 == t2);
    }
    friend inline operator<(const tid_t& t1, const tid_t& t2) {
	return (t1.hi < t2.hi) || (t1.hi == t2.hi && t1.lo < t2.lo);
    }
    friend inline operator<=(const tid_t& t1, const tid_t& t2) {
	return (t1.hi <= t2.hi) || (t1.hi == t2.hi && t1.lo <= t2.lo);
    }
    friend inline operator>(const tid_t& t1, const tid_t& t2) {
	return (t1.hi > t2.hi) || (t1.hi == t2.hi && t1.lo > t2.lo);
    }
    friend inline operator>=(const tid_t& t1, const tid_t& t2) {
	return (t1.hi >= t2.hi) || (t1.hi == t2.hi && t1.lo >= t2.lo);
    }

    static const tid_t max;
    static const tid_t null;

#define max_tid  (tid_t::max)
#define null_tid (tid_t::null)

    
private:

#endif  /* __cplusplus */
    uint4       hi;
    uint4       lo;
};

#define max_gtid_len  256
struct gtid_t {

	uint4         _length;
	unsigned char _opaque[max_gtid_len];

#ifdef __cplusplus
	gtid_t() {
	    _length = 0;
#ifdef PURIFY
	    memset(_opaque, '\0', max_gtid_len);
#endif
	}
	gtid_t	&
	operator=(const gtid_t	&r) 
	{
		_length=r._length;
		memcpy(_opaque,r._opaque,_length);
		return *this;
	}
#endif
};

#ifdef __cplusplus
inline 
bool operator==(const gtid_t &a,
	const gtid_t	&b) 
{
	return ((a._length==b._length) &&
		(memcmp(a._opaque,b._opaque,a._length)==0));
}

#ifdef DEBUG

#include <iostream.h>

// Only for debugging purposes; for debugging,
// we assume that this is a null-terminated character
// string that fits
inline ostream &
operator<<(ostream &o, const gtid_t &t)
{
    return o << (char *)(t._opaque);
}
inline istream &
operator>>(istream &i, gtid_t &t)
{
    i.width(sizeof(t._opaque));
    i >> t._opaque;
    t._length = strlen((const char *)t._opaque);
    // assert(t._opaque[t._length]=0);
    return i;
}
#endif DEBUG

inline ostream& operator<<(ostream& o, const tid_t& t)
{
    return o << t.hi << '.' << t.lo;
}

inline istream& operator>>(istream& i, tid_t& t)
{
    char ch;
    return i >> t.hi >> ch >> t.lo;
}

#endif /*__cplusplus*/

#endif/* AIX kludge */
#endif /*TID_T_H*/

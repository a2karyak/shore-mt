/*<std-header orig-src='shore' incl-file-exclusion='AIX_TID_T_H'>

 $Id: aix_tid_t.h,v 1.11 1999/06/07 19:02:22 kupsch Exp $

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

#ifndef AIX_TID_T_H
#define AIX_TID_T_H

#include "w_defines.h"

/*  -- do not edit anything above this line --   </std-header>*/

#ifdef __GNUG__
#pragma interface
#endif

/* XXX disgusting hack -- a conflict with aix4.1 thread id */
#define	tid_t	w_tid_t

/*
 * NB -- THIS FILE MUST BE LEGITIMATE INPUT TO cc and RPCGEN !!!!
 * Please do as follows:
 * a) keep all comments in traditional C style.
 * b) If you put something c++-specific make sure it's 
 * 	  got ifdefs around it
 */
struct w_tid_t {
#ifdef	__cplusplus
public:
    enum { hwm = max_uint4 };

    w_tid_t(uint4_t l = 0, uint4_t h = 0) : hi(h), lo(l)             {};
    w_tid_t(const w_tid_t& t) : hi(t.hi), lo(t.lo)  {};
    w_tid_t& operator=(const w_tid_t& t)    {
        lo = t.lo, hi = t.hi;
        return *this;
    }

    operator const void*() const  { 
	return (void*) (hi == 0 && lo == 0); 
    }

    w_tid_t& incr()       {
	if (++lo > (uint4_t)hwm) ++hi, lo = 0;
        return *this;
    }

    friend inline ostream& operator<<(ostream&, const w_tid_t&);
    friend inline istream& operator>>(istream&, w_tid_t&);
    friend inline operator==(const w_tid_t& t1, const w_tid_t& t2)  {
	return t1.hi == t2.hi && t1.lo == t2.lo;
    }
    friend inline operator!=(const w_tid_t& t1, const w_tid_t& t2)  {
	return ! (t1 == t2);
    }
    friend inline operator<(const w_tid_t& t1, const w_tid_t& t2) {
	return (t1.hi < t2.hi) || (t1.hi == t2.hi && t1.lo < t2.lo);
    }
    friend inline operator<=(const w_tid_t& t1, const w_tid_t& t2) {
	return (t1.hi <= t2.hi) || (t1.hi == t2.hi && t1.lo <= t2.lo);
    }
    friend inline operator>(const w_tid_t& t1, const w_tid_t& t2) {
	return (t1.hi > t2.hi) || (t1.hi == t2.hi && t1.lo > t2.lo);
    }
    friend inline operator>=(const w_tid_t& t1, const w_tid_t& t2) {
	return (t1.hi >= t2.hi) || (t1.hi == t2.hi && t1.lo >= t2.lo);
    }

    static const w_tid_t max;
    static const w_tid_t null;

#define max_tid  (w_tid_t::max)
#define null_tid (w_tid_t::null)

    
private:

#endif  /* __cplusplus */
    uint4_t       hi;
    uint4_t       lo;
};

#ifdef __cplusplus

inline ostream& operator<<(ostream& o, const w_tid_t& t)
{
    return o << t.hi << '.' << t.lo;
}

inline istream& operator>>(istream& i, w_tid_t& t)
{
    char ch;
    return i >> t.hi >> ch >> t.lo;
}

#endif /*__cplusplus*/

/*<std-footer incl-file-exclusion='AIX_TID_T_H'>  -- do not edit anything below this line -- */

#endif          /*</std-footer>*/

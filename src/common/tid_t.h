/*<std-header orig-src='shore' incl-file-exclusion='TID_T_H'>

 $Id: tid_t.h,v 1.66 2007/05/18 21:33:42 nhall Exp $

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

#if defined(AIX41) || defined(HPUX8)
/* XXX this is a giant hack because our transaction id, tid_t conflicts
   with the 'tid_t' thread ID that AIX and later HPUX added to the system
   header files.   This is a giant kludge, and it replaces the previous
   giant aix_tid_t.h kludge ... since that source was horribly out of date.
   Note there is a hack or two concerning this in the SM.  */

#define	tid_t	w_tid_t
#endif

#ifdef __GNUG__
#pragma interface
#endif

class tid_t {
public:
    enum { hwm = max_uint4 };

    tid_t(uint4_t l = 0, uint4_t h = 0) : hi(h), lo(l)             {};
    tid_t(const tid_t& t) : hi(t.hi), lo(t.lo)  {};

    uint4_t get_hi() const { return hi; }
    uint4_t get_lo() const { return lo; }

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

private:

    uint4_t       hi;
    uint4_t       lo;
};


/* XXX yes, this is disgusting, but at least it allows it to
   be a shore.def option.  In reality, this specification should
   be revisited.    These fixed length objects have caused a 
   fair amount of problems, and it might be time to rethink the
   issue a bit. */
#ifdef COMMON_GTID_LENGTH
#define max_gtid_len	COMMON_GTID_LENGTH
#else
#define max_gtid_len  40
#endif

#ifdef COMMON_SERVER_HANDLE_LENGTH
#define max_server_handle_len  COMMON_SERVER_HANDLE_LENGTH
#else
#define max_server_handle_len  100
#endif


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


#include "w_opaque.h"

typedef opaque_quantity<max_gtid_len> gtid_t;
typedef opaque_quantity<max_server_handle_len> server_handle_t;

/*<std-footer incl-file-exclusion='TID_T_H'>  -- do not edit anything below this line -- */

#endif          /*</std-footer>*/

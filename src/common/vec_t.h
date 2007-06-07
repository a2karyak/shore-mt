/*<std-header orig-src='shore' incl-file-exclusion='VEC_T_H'>

 $Id: vec_t.h,v 1.64 1999/06/07 19:02:35 kupsch Exp $

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

#ifndef VEC_T_H
#define VEC_T_H

#include "w_defines.h"

/*  -- do not edit anything above this line --   </std-header>*/

/* NB: you must already have defined the type size_t,
 * (which is defined include "basics.h") before you include this.
 * For RPCGEN-related reasons, you CANNOT just include basics.h here!
 */

#ifdef __GNUG__
#pragma interface
#endif

#ifdef RPCGEN
#define CADDR_T caddr_t
#else
#define CADDR_T const unsigned char *
#endif

#define MAX_SMALL_VEC_SIZE 8

struct vec_pair_t {
    CADDR_T	ptr;
    size_t	len;
};

struct VEC_t {
    int		_cnt;
    size_t	_size;
    vec_pair_t*	_base;	// pointer to beginning of _pair or malloced
			// space
    vec_pair_t	_pair[MAX_SMALL_VEC_SIZE];
};

#ifndef __cplusplus
typedef VEC_t vec_t;
#else
// cvec_t stands for "constant" vec (meaning things pointed to
// cannot be changed)
class cvec_t : protected VEC_t {
    friend class vec_t; // so vec_t can look at VEC_t
protected:
    static	CADDR_T  zero_location; // see zvec_t, which is supposed
				    // to be for the server-side only
public:
    enum dummy_enumid { max_small = MAX_SMALL_VEC_SIZE };
public:
    cvec_t() {
	_cnt = 0;
	_size = 0;
	_base = &_pair[0];
    }
    cvec_t(const cvec_t& v1, const cvec_t& v2) {
	_base= &_pair[0];
	set(v1, v2);
    }
    cvec_t(const void* p, size_t l) {
	_base = &_pair[0];
	set(p, l);
    }
    cvec_t(const cvec_t& v, size_t offset, size_t limit) {
	_base = &_pair[0];
	set(v, offset, limit);
    }
    ~cvec_t();

    void split(size_t l1, cvec_t& v1, cvec_t& v2) const;
    cvec_t& put(const cvec_t& v, size_t offset, size_t nbytes);
    cvec_t& put(const void* p, size_t l);
    cvec_t& put(const cvec_t& v);
    cvec_t& reset()  {
	_cnt = _size = 0;
	return *this;
    }
    cvec_t& set(const cvec_t& v1, const cvec_t& v2)  {
	return reset().put(v1).put(v2);
    }
    cvec_t& set(const cvec_t& v) {
	return reset().put(v);
    }

    cvec_t& set(const void* p, size_t l)  {
	return reset().put(p, l);
    }
    cvec_t& set(const cvec_t& v, size_t offset, size_t limit)  {
	return reset().put(v, offset, limit);
    }


    size_t size() const	{
#ifdef W_DEBUG
	/* BUGBUG:- removed when vec_t stabilized */
	check_size();
#endif /*W_DEBUG*/
	return _size;
    }

    size_t copy_to(void* p, size_t limit = 0x7fffffff) const;
    
    int cmp(const cvec_t& v, size_t* common_size = 0) const;
    int cmp(const void* s, size_t len) const;

    static int cmp(const cvec_t& v1,
	       const cvec_t& v2, size_t* common_size = 0)  {
	return v1.cmp(v2, common_size);
    }

    int count() const {return _cnt;}

    int checksum() const;
    void calc_kvl(uint4_t& h) const;
    void init() 	{ _cnt = _size = 0; }  // re-initialize the vector

    bool is_pos_inf() const	{ return this == &pos_inf; }
    bool is_neg_inf() const	{ return this == &neg_inf; }
    bool is_null() const	{ return size() == 0; }

    friend inline bool operator<(const cvec_t& v1, const cvec_t& v2);
    friend inline bool operator<=(const cvec_t& v1, const cvec_t& v2);
    friend inline bool operator>=(const cvec_t& v1, const cvec_t& v2);
    friend inline bool operator>(const cvec_t& v1, const cvec_t& v2);
    friend inline bool operator==(const cvec_t& v1, const cvec_t& v2);
    friend inline bool operator!=(const cvec_t& v1, const cvec_t& v2);

    friend ostream& operator<<(ostream&, const cvec_t& v);
    friend istream& operator>>(istream&, cvec_t& v);

    static cvec_t pos_inf;
    static cvec_t neg_inf;

private:
    // determine if this is a large vector (one where extra space
    // had to be malloc'd 
    bool _is_large() const {return _base != &_pair[0];}

    // determine max number of elements in the vector
    int  _max_cnt() const {
	return (int)(_is_large() ? _pair[0].len : (int)max_small);
    }
    // grow vector to have total_cnt elements
    void _grow(int total_cnt);

    // disabled
    cvec_t(const cvec_t& v);
    cvec_t& operator=(cvec_t);

    size_t recalc_size() const;
    bool check_size() const;

public:
    bool is_zvec() const { 
#ifdef W_DEBUG
	if(count()>0) {
	    if(_pair[0].ptr == zero_location) {
		assert(count() == 1);
	    }
	}
#endif
	return (count()==0)
		||
		(count() == 1 && _pair[0].ptr == zero_location);
    }
};

class vec_t : public cvec_t {
public:
    vec_t() : cvec_t()	{};
    vec_t(const cvec_t& v1, const cvec_t& v2) : cvec_t(v1, v2)  {};
    vec_t(const void* p, size_t l) : cvec_t(p, l)	{};
    vec_t(const vec_t& v, size_t offset, size_t limit)
    : cvec_t(v, offset, limit)	{};


    /*
     *  copy_from() does not change vec_t itself, but overwrites
     *  the data area to which the vec points
     *  (temporarily made const for VAS compatibility)
     */
    const vec_t& copy_from(
	const void* p,
	size_t limit,
	size_t offset = 0) const;	// offset tells where
				//in the vec to begin to copy
    
    vec_t& copy_from(const cvec_t& v);
    vec_t& copy_from(
	const cvec_t& v,
	size_t offset,		// offset in v
	size_t limit,		// # bytes
	size_t myoffset = 0);	// offset in this

    CADDR_T	ptr(int index) const { return (index >= 0 && index < _cnt) ? 
					_base[index].ptr : (CADDR_T) NULL; }
    size_t	len(int index) const { return (index >= 0 && index < _cnt) ? 
					_base[index].len : 0; }
    void mkchunk( int maxsize, // max size of result vec
		int skip,    	     // # skipped in *this
		vec_t	&result      // provided by the caller
    ) const;
    static vec_t& pos_inf;
    static vec_t& neg_inf;
private:
    // disabled
    vec_t(const vec_t&) : cvec_t()  {}
    vec_t& operator=(vec_t);
};

inline bool operator<(const cvec_t& v1, const cvec_t& v2)
{
    return v1.cmp(v2) < 0;
}

inline bool operator<=(const cvec_t& v1, const cvec_t& v2)
{
    return v1.cmp(v2) <= 0;
}

inline bool operator>=(const cvec_t& v1, const cvec_t& v2)
{
    return v1.cmp(v2) >= 0;
}

inline bool operator>(const cvec_t& v1, const cvec_t& v2)
{
    return v1.cmp(v2) > 0;
}

inline bool operator==(const cvec_t& v1, const cvec_t& v2)
{
    return (&v1==&v2) || v1.cmp(v2) == 0;
}

inline bool operator!=(const cvec_t& v1, const cvec_t& v2)
{
    return ! (v1 == v2);
}

#endif /*__cplusplus*/

/*<std-footer incl-file-exclusion='VEC_T_H'>  -- do not edit anything below this line -- */

#endif          /*</std-footer>*/

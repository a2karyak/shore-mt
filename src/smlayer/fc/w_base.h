/*<std-header orig-src='shore' incl-file-exclusion='W_BASE_H'>

 $Id: w_base.h,v 1.60 1999/10/25 05:53:18 bolo Exp $

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

#ifndef W_BASE_H
#define W_BASE_H

#include "w_defines.h"

/*  -- do not edit anything above this line --   </std-header>*/

/*******************************************************/
/* get configuration definitions from config/shore.def */
/* (in w_defines.h above).                             */
#include <w_windows.h>
/*
 * WARNING: we turned on ON and OFF in shore.def
 * but we MUST turn them off as soon as configuration
 * stuff has been read, because ON and OFF are re-defined
 * elsewhere as enums
 */
#ifdef ON
#undef ON
#endif

#ifdef OFF
#undef OFF
#endif
/* end configuration definitions                       */
/*******************************************************/


#if !defined(W_OS2) && !defined(_WINDOWS)
#define W_UNIX
#endif

#ifdef __GNUG__
#pragma interface
#endif

#include <stddef.h>
#include <stdlib.h>
#include <w_stream.h>

#include <limits.h>

#if defined(W_UNIX) && !defined(_WIN32)
#include <unistd.h>
#endif

#if defined(_MSC_VER) && defined(small)
/* XXX namespace corruption in MSC header files */
#undef small
#endif

#ifndef W_WORKAROUND_H
#include "w_workaround.h"
#endif

#if defined(DEBUG) && !defined(W_DEBUG)
#define W_DEBUG
#endif

#define NORET		/**/
#define CAST(t,o)	((t)(o))
#define SIZEOF(t)	(sizeof(t))

#define	W_UNUSED(x)	/**/

#ifdef W_DEBUG
#define W_IFDEBUG(x)	x
#define W_IFNDEBUG(x)	/**/
#else
#define W_IFDEBUG(x)	/**/
#define W_IFNDEBUG(x)	x
#endif

#ifdef W_TRACE
#define	W_IFTRACE(x)	x
#define	W_IFNTRACE(x)	/**/
#else
#define	W_IFTRACE(x)	/**/
#define	W_IFNTRACE(x)	x
#endif

#if defined(W_DEBUG_SPACE)
void 	w_space(int line, const char *file);
#define W_SPACE w_space(__LINE__,__FILE__)
#else
#define W_SPACE
#endif


#define w_assert1(x)    do {						\
	if (!(x)) w_base_t::assert_failed(#x, __FILE__, __LINE__);	\
	W_SPACE;							\
} while(0)

#ifdef W_DEBUG
#define w_assert3(x)	w_assert1(x)
#else
#define w_assert3(x)	/**/
#endif

/*
 * Cast to treat an enum as in integer value.  This is used when
 * a operator<< doesn't exist for the enum.  The use of the macro
 * indicates that this enum would be printed if it had a printer,
 * rather than wanting the integer value of the enum
 */
#define	W_ENUM(x)	((int)x)

class w_rc_t;

/*--------------------------------------------------------------*
 *  w_base_t							*
 *--------------------------------------------------------------*/
class w_base_t {
public:
    /*
     *  shorthands
     */
    typedef unsigned char	u_char;
    typedef unsigned short	u_short;
    typedef unsigned long	u_long;
    // typedef w_rc_t		rc_t;

    /*
     *  basic types
     */
    typedef char		int1_t;
    typedef u_char		uint1_t;
    typedef short		int2_t;
    typedef u_short		uint2_t;
    typedef long		int4_t;
    typedef u_long		uint4_t;

#ifdef _MSC_VER
#    if defined(_INTEGRAL_MAX_BITS) && (_INTEGRAL_MAX_BITS >= 64)
    	typedef __int64 		int8_t;
    	typedef unsigned __int64 	uint8_t;
#    else
#	error Compiler does not support int8_t (__int64).
#    endif

#elif defined(__GNUG__)
    	typedef long long		int8_t;
    	typedef unsigned long long	uint8_t;
#else
#error int8_t Not supported for this compiler.
#endif

    /* 
     * For statistics that are always 64-bit numbers
     */
    typedef uint8_t 		large_stat_t; 

    /* 
     * For statistics that are 64-bit numbers 
     * only when #defined LARGEFILE_AWARE
     */
#ifdef LARGEFILE_AWARE
    typedef uint8_t     	base_stat_t;
#else
    typedef uint4_t     	base_stat_t;
#endif

    typedef float		f4_t;
    typedef double		f8_t;

    static const int1_t		int1_max, int1_min;
    static const int2_t		int2_max, int2_min;
    static const int4_t		int4_max, int4_min;
    static const int8_t		int8_max, int8_min;

    static const uint1_t	uint1_max, uint1_min;
    static const uint2_t	uint2_max, uint2_min;
    static const uint4_t	uint4_max, uint4_min;
    static const uint8_t	uint8_max, uint8_min;

    /*
     *  miscellaneous
     */
    enum { align_on = 0x8, align_mod = align_on - 1 };

	/*
	// NEH: turned into a macro for the purpose of folding
    // static uint4_t		align(uint4_t sz);
	*/
#ifndef align
#define alignonarg(a) (((uint4_t)a)-1)
#define alignon(p,a) (((uint4_t)((char *)p + alignonarg(a))) & ~alignonarg(a))

#define ALIGNON 0x4
#define ALIGNON1 (ALIGNON-1)
#define align(sz) ((uint4_t)((sz + ALIGNON1) & ~ALIGNON1))
#endif /* align */

    static bool		is_aligned(uint4_t sz);
    static bool		is_aligned(const void* s);

    static bool		is_big_endian();
    static bool		is_little_endian();

    /*  strtoi8 and strtou8 act like strto[u]ll with the following
     *  two exceptions: the only bases supported are 0, 8, 10, 16;
     *  ::errno is not set
     */
    static int8_t	strtoi8(const char *, char ** end=0 , int base=0);
    static uint8_t	strtou8(const char *, char ** end=0, int base=0);

    static istream&	_scan_uint8(istream& i, uint8_t &, 
				bool chew_white,
				bool is_signed,
				bool& rangerr);

    static bool		is_finite(const f8_t x);
    static bool		is_infinite(const f8_t x);
    static bool		is_nan(const f8_t x);
    static bool		is_infinite_or_nan(const f8_t x);

    /*
     *  standard streams
     */
    friend ostream&		operator<<(
	ostream&		    o,
	const w_base_t&		    obj);

    static void			assert_failed(
    	const char*		    desc,
	const char*		    file,
	uint4_t 		    line);

    static	void		abort();
};

/* XXX compilers+environment that need this operator defined */
#if defined(_MSC_VER)  
ostream &operator<<(ostream &o, const w_base_t::int8_t &t);
ostream &operator<<(ostream &o, const w_base_t::uint8_t &t);
istream &operator>>(istream &i, w_base_t::int8_t &t);
istream &operator>>(istream &i, w_base_t::uint8_t &t);
#endif

/*--------------------------------------------------------------*
 *  w_base_t::align()						*
 *--------------------------------------------------------------*/
/*
 * NEH: turned into a macro
inline w_base_t::uint4_t
w_base_t::align(uint4_t sz)
{
    return (sz + ((sz & align_mod) ? 
		  (align_on - (sz & align_mod)) : 0));
}
*/

/*--------------------------------------------------------------*
 *  w_base_t::is_aligned()					*
 *--------------------------------------------------------------*/
inline bool
w_base_t::is_aligned(uint4_t sz)
{
    return (align(sz) == sz);
}

inline bool
w_base_t::is_aligned(const void* s)
{
    return is_aligned(CAST(uint4_t, s));
}

/*--------------------------------------------------------------*
 *  w_base_t::is_big_endian()					*
 *--------------------------------------------------------------*/
inline bool w_base_t::is_big_endian()
{
	/* Provide fast results on known architectures, slower
	   but correct results on unknown architectures.
	 */
#if defined(Sparc)
	return true;
#elif defined(I386)
	return false;
#else
	int i = 1;
	return ((char*)&i)[3] == i;
#endif
}

/*--------------------------------------------------------------*
 *  w_base_t::is_little_endian()				*
 *--------------------------------------------------------------*/
inline bool
w_base_t::is_little_endian()
{
    return ! is_big_endian();
}

/*--------------------------------------------------------------*
 *  w_vbase_t							*
 *--------------------------------------------------------------*/
class w_vbase_t : public w_base_t {
public:
    NORET			w_vbase_t()	{};
    virtual NORET		~w_vbase_t()	{};
};

class w_msec_t {
public:
    enum special_t { 
	t_infinity = -1,
	t_forever = t_infinity,
	t_immediate = 0
    };

    NORET			w_msec_t();
    NORET			w_msec_t(w_base_t::uint4_t ms);
    NORET			w_msec_t(special_t s);
    
    bool			is_forever();
    w_base_t::uint4_t		value();
private:
    w_base_t::int4_t		ms;
};

inline w_base_t::uint4_t
w_msec_t::value()
{
    return CAST(w_base_t::uint4_t, ms);
}

inline bool
w_msec_t::is_forever()
{
    return ms == t_forever;
}

/*
 * These types are auto initialized filler space for alignment
 * in structures.  The auto init helps with purify.
 *
 * XXX Some of the users of these structures DEPEND on zero
 * fill (sm keys, etc). Eventually this will be seperated into
 * zero fill and plain fill (zeroed with PURIFY_ZERO).  Until then
 * these must always be initialized to 0.
 */
struct fill1 {
	w_base_t::uint1_t u1;
	fill1() : u1(0) {}
};

struct fill2 {
	w_base_t::uint2_t u2;
	fill2() : u2(0) {}
};

struct fill3 {
	w_base_t::uint1_t	u1[3];
	fill3() { u1[0] = u1[1] = u1[2] = 0; }
};

struct fill4 {
	w_base_t::uint4_t u4;
	fill4() : u4(0) {}
};


#include <w_autodel.h>
#include <w_factory.h>
#include <w_factory_fast.h>
#include <w_factory_thread.h>
#include <w_error.h>
#include <w_rc.h>

/*<std-footer incl-file-exclusion='W_BASE_H'>  -- do not edit anything below this line -- */

#endif          /*</std-footer>*/

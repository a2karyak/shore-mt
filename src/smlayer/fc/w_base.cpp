/*<std-header orig-src='shore'>

 $Id: w_base.cpp,v 1.45 2000/02/01 23:20:49 bolo Exp $

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

#define W_SOURCE

#ifdef __GNUG__
#pragma implementation "w_base.h"
#endif

#ifdef _MSC_VER
/* Undefined for our purecall definition */
#define	_purecall	libc_purecall
#endif

#include <w_base.h>
#include <unix_error.h>


/*--------------------------------------------------------------*
 *  constants for w_base_t                                      *
 *--------------------------------------------------------------*/
const w_base_t::int1_t    w_base_t::int1_max  = 0x7f;
const w_base_t::int1_t    w_base_t::int1_min  = (const w_base_t::int1_t) 0x80u;
const w_base_t::uint1_t   w_base_t::uint1_max = 0xff;
const w_base_t::uint1_t   w_base_t::uint1_min = 0x0;
const w_base_t::int2_t    w_base_t::int2_max  = 0x7fff;
const w_base_t::int2_t    w_base_t::int2_min  = (const w_base_t::int2_t) 0x8000u;
const w_base_t::uint2_t   w_base_t::uint2_max = 0xffff;
const w_base_t::uint2_t   w_base_t::uint2_min = 0x0;
const w_base_t::int4_t    w_base_t::int4_max  = 0x7fffffff;
const w_base_t::int4_t    w_base_t::int4_min  = 0x80000000;
const w_base_t::uint4_t   w_base_t::uint4_max = 0xffffffff;
const w_base_t::uint4_t   w_base_t::uint4_min = 0x0;

#ifdef _MSC_VER
#	define LONGLONGCONSTANT(i) i
#	define ULONGLONGCONSTANT(i) i
#else
#	define LONGLONGCONSTANT(i) i##LL
#	define ULONGLONGCONSTANT(i) i##ULL
#endif /* _MSC_VER */

const w_base_t::uint8_t   w_base_t::uint8_max =
				ULONGLONGCONSTANT(0xffffffffffffffff);
const w_base_t::uint8_t   w_base_t::uint8_min =
				ULONGLONGCONSTANT(0x0);
const w_base_t::int8_t   w_base_t::int8_max =
				LONGLONGCONSTANT(0x7fffffffffffffff);
const w_base_t::int8_t   w_base_t::int8_min =
				LONGLONGCONSTANT(0x8000000000000000);

ostream&
operator<<(ostream& o, const w_base_t&)
{
    w_base_t::assert_failed("w_base::operator<<() called", __FILE__, __LINE__);
    return o;
}

void
w_base_t::assert_failed(
    const char*		desc,
    const char*		file,
    uint4_t		line)
{
#ifdef notyet
	/* generate a normal RC that makes assertion failure look just 
	   like any other fatal error.  On-stack hack is used
	   to prevent fastnew code from being used. */
	w_error_t	er(file, line, fcASSERT, 0);
	w_rc_t		e(&er);
	W_COERCE(e);
	e.reset();
#else
	/* make the error look something like an RC in the meantime. */
	cerr << "assertion failure: " << desc << endl
		<< "1. error in "
		<< file << ':' << line
		<< " Assertion failed" << endl
		<< "\tcalled from:" << endl
		<< "\t0) " << file << ':' << line
		<< endl << flush;
	abort();
#endif
}


/* XXX compilers and/or environments which need these */
#if defined(_MSC_VER) 

/* XXX These output operators only follow base and showbase requests
   on win32 and visual c++.  Which is better than before, so 
   I'm working on it. */

ostream &operator<<(ostream &o, const w_base_t::uint8_t &t)
{
	char buf[24];

#ifdef _MSC_VER
	char	*bufp = buf;
	int	base = 10;	/* the default */
	if (o.flags() & ios::hex) {
		base = 16;
		if (o.flags() & ios::showbase) {
			*bufp++ = '0';
			*bufp++ = 'x';
		}
	}
	else if (o.flags() & ios::oct) {
		base = 8;
		if (t && o.flags() & ios::showbase)
			*bufp++ = '0';
	}

	_ui64toa(t, bufp, base);

	bufp = buf;
#else
	char *bufp = &buf[sizeof(buf)-1];
	*bufp = '\0';
	bufp =  ulltostr(t, bufp);
#endif

	o << bufp;
	return o;
}

ostream &operator<<(ostream &o, const w_base_t::int8_t &t)
{
	char buf[24];

#ifdef _MSC_VER
	char	*bufp = buf;
	int	base = 10;	/* the default */
	if (o.flags() & ios::hex) {
		base = 16;
		if (o.flags() & ios::showbase) {
			*bufp++ = '0';
			*bufp++ = 'x';
		}
	}
	else if (o.flags() & ios::oct) {
		base = 8;
		if (t && o.flags() & ios::showbase)
			*bufp++ = '0';
	}
	
	
	/* XXX workaround for formatting error with i64toa, the
	   fix will work correctly if i64toa is fixed. */
	if (base == 10 && t < 0)
		*bufp++ = '-';

	_ui64toa((t < 0 && base == 10) ? -t : t, bufp, base);

	bufp = buf;
#else
	/* XXX workaround for formatting error with lltostr, the
	   fix will work correctly if lltostr is fixed. */
	char *bufp = &buf[sizeof(buf)-1];
	*bufp = '\0';
	bufp =  ulltostr(t < 0 ? -t : t, bufp);
	if (t < 0 && bufp)
		*--bufp = '-';
#endif
	o << bufp;
	return o;
}

istream &
operator>>(istream& i, w_base_t::uint8_t &t)
{
	bool	rangerr=false;
	return w_base_t::_scan_uint8(i, t, true, false, rangerr);
}

istream &
operator>>(istream& i, w_base_t::int8_t &t)
{
	bool	rangerr=false;
	w_base_t::uint8_t u;
	istream& res = w_base_t::_scan_uint8(i, u, true, true, rangerr);
	t = w_base_t::int8_t(u);
	return res;
}
#endif

#ifdef _MSC_VER

#include <string.h>
#include <ios.h>
#include <strstrea.h>
typedef long fmtflags;

#else

#include <strstream.h>
typedef ios::fmtflags  fmtflags;

#endif


/* USE_OUR_IMPLEMENTATION lets us test the
 * code in w_input.cpp
 * on Solaris or Linux or NetBSD 
 */
#undef USE_OUR_IMPLEMENTATION

#ifdef _MSC_VER
#define USE_OUR_IMPLEMENTATION
#endif


/* name is local to this file */
static w_base_t::uint8_t	
__strtou8(		
    const char *str,
    char **endptr,
    int 	base,
    bool  is_signed
)
{

#if defined(_MSC_VER) || defined(USE_OUR_IMPLEMENTATION)

    istrstream 		tmp(VCPP_BUG_1 str, strlen(str));
    streampos 		b =  tmp.tellg();

    fmtflags f = tmp.flags();
    f = f & ~(ios::basefield);
    switch(base) {
       case 0:
	    break;
       case 10:
	    f |= ios::dec;
	    break;
       case 8:
	    f |= ios::oct;
	    break;
       case 16:
	    f |= ios::hex;
	    break;
       default:
	    /*
	    cerr << "w_base_t::__strtou8 : base "
		    << base << " not supported" << endl;
	    W_FATAL(fcNOTIMPLEMENTED);
	    */
	    errno = EINVAL;
	    return 0;
	    break;
    }
    // strto*() permit leading white space
    f |= ios::skipws;
    tmp.flags(f);

#ifdef W_DEBUG
    if(tmp.flags() != f) {
	w_assert3(0);
    }
#endif

    w_base_t::uint8_t	u8;
    bool			rangerr=false;
    w_base_t::_scan_uint8(tmp, u8, false, is_signed, rangerr);

    if(rangerr) {
	errno = ERANGE;
    } 
    if(endptr) {
	streampos 		e =  tmp.tellg();
	*endptr = (char *)&str[e - b];
    }
    return u8;

#elif defined(__NetBSD__)
    return is_signed? ::strtoq(str, endptr, base): ::strtouq(str, endptr, base);
#elif defined(Alpha)
    return is_signed? strtol(str, endptr, base): strtoul(str, endptr, base);
#else
    return is_signed? strtoll(str, endptr, base): strtoull(str, endptr, base);
#endif
}

w_base_t::int8_t	
w_base_t::strtoi8(
    const char *str,
    char **endptr,
    int 	base
)
{
    w_base_t::int8_t	i8;
    w_base_t::int8_t	u8 =
	    __strtou8(str, endptr, base, true);
    i8 = w_base_t::int8_t(u8);
    return i8;
}

w_base_t::uint8_t	
w_base_t::strtou8(
    const char *str,
    char **endptr,
    int 	base
)
{
    return __strtou8(str, endptr, base, false);
}

#ifdef _MSC_VER
#include <float.h>
#elif defined(SOLARIS2)
#include <ieeefp.h>
#else
#include <math.h>
#endif

bool
w_base_t::is_finite(const f8_t x)
{
    bool value = false;
#ifdef _MSC_VER
    value = _finite(x);
#else
    value = finite(x);
#endif
    return value;
}

bool
w_base_t::is_infinite(const f8_t x)
{
    bool value = false;
#ifdef _MSC_VER
    value = !_finite(x) && !_isnan(x);
#elif defined(SOLARIS2)
    value = !finite(x) && !isnand(x);
#else
    value = !finite(x) && !isnan(x);
#endif
    return value;
}

bool
w_base_t::is_nan(const f8_t x)
{
    bool value = false;
#ifdef _MSC_VER
    value = _isnan(x);
#elif defined(SOLARIS2)
    value = isnand(x);
#else
    value = isnan(x);
#endif
    return value;
}

bool
w_base_t::is_infinite_or_nan(const f8_t x)
{
    bool value = false;
#ifdef _MSC_VER
    value = !_finite(x);
#else
    value = !finite(x);
#endif
    return value;
}


void	w_base_t::abort()
{
	cout.flush();
	cerr.flush();
#ifdef _WIN32
	/* It would be great if there was a USER ERROR exception,
	   or something like that.  Lacking that, I'll steal 
	   illegal instruction and make it non-continuable.
	   If someone thinks of a better exception to throw, 
	   tell me about it, I'm all ears. Bolo */

	RaiseException(EXCEPTION_ILLEGAL_INSTRUCTION,
		       EXCEPTION_NONCONTINUABLE,
		       0, 0);
	/* Fall through to abort in case excpetion handling goes west */
#endif
	::abort();
}


/* Provide our own pure_virtual() so we can generate abort's if
   a virtual function is abused.  Name choice is somewhat
   hacky, since it is system and compiler dependent. */

#ifdef __GNUG__
#define	PURE_VIRTUAL	extern "C" void __pure_virtual()
#elif defined(_MSC_VER)
#undef _purecall
#define	PURE_VIRTUAL	_cdecl _purecall(void)
#else
#define	PURE_VIRTUAL	void pure_virtual()
#endif

PURE_VIRTUAL
{
	/* Just in case the iostreams generate another error ... */
	static	bool	called = false;
	if (!called)
		cerr << "** Pure virtual function called" << endl;
	called = true;

	w_base_t::abort();
	/*NOTREACHED*/

#ifdef _MSC_VER
	/* A hack  for visual c++ + inbred header files */
	return 0;
#endif
}

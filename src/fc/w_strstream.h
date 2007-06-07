/*<std-header orig-src='shore' incl-file-exclusion='W_STRSTREAM_H'>

 $Id: w_strstream.h,v 1.17 2006/04/04 04:40:39 bolo Exp $

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

#ifndef W_STRSTREAM_H
#define W_STRSTREAM_H

#include "w_defines.h"

/*  -- do not edit anything above this line --   </std-header>*/

#include <w_workaround.h>

/*
 * Shore only supports "shore strstreams", which are a compatability
 * layer which
 *
 *   1) works with either strstream (or eventually) stringstream.
 *   2) provides automatic string deallocation, the most error-prone
 *      use of strstreams with allocated memory.
 *   3) Provides for static buffer non-dynamic streams to eliminate
 *      a large amount of duplicate shore code (forthcoming).
 *
 * The c_str() method _guarantees_ that the returned c-string WILL
 * be nul-terminated.  This prevents unterminated string bugs.  This
 * occurs at the expense of something else.  That cost is increasing
 * the buffer size on a dynamically allocated string.  Or, with a
 * fixed buffer, over-writing the last valid character (at the end
 * of the buffer) with a nul.   The implementation is extremely low
 * overhead in the expected case that the string is nul-terminated
 * by 'ends'.
 *
 * To access the contents without any funky appending behavior, the
 * proper way to do that is with address+length.  That is done with
 * the data() method and then pcount() for the length of the buffer.
 *
 * The auto-deallocation mimics the behavior of newer stringstreams,
 * 
 */

#if defined(W_USE_SSTREAM)

/* For a fixed write buffer, use the character array constructor,
   but add a "no expansion" allocator so the string words don't
   try to expand the fixed buffer any. */

#include <sstream>
#include <cstring>

class w_istrstream : public istringstream {
public:
	w_istrstream(const char *s)
	: istringstream(string(s, strlen(s)))
	{
	}
	w_istrstream(const char *s, size_t l)
	: istringstream(string(s, l))
	{
	}
};

class w_ostrstream : public ostringstream {
public:
	/* add a 'max length' constructor? */
	w_ostrstream()
	: ostringstream()
	{
	}

#ifdef notyet
	/* This can't work at present, the underlying memory buffer
	   is never updated.  Leaving this undef will cause the 
	   compiler to err at compile time. */

	w_ostrstream(char *s, size_t l)
	: ostringstream(string(s, l))
	{
		W_FATAL(fcINTERNAL);
	}
#endif

	/* XXX think about this, and semantics */
	const char *c_str()
	{
		return ostringstream::str().c_str();
	}

	void freeze(bool = true)
	{
	}

	const char *new_c_str()
	{
		const char *s = c_str();
		char *t = new char[strlen(s) + 1];
		if (!t)
			return 0;
		strcpy(t, s);
		return t;
	}

	const char *take_c_str()
	{
		return new_c_str();
	}

	size_t	pcount() const
	{
		return size();
	}

#if 0
	/* not all c++ have strings, and shore doesn't use them */
	const string &cxx_str()
	{
		return ostringstream::str();
	}
#endif
};

#else

#ifdef W_USE_COMPAT_STRSTREAM
#include "w_compat_strstream.h"
#else
#include <strstream>
#endif
#include <cstring>

#if defined(W_USE_COMPAT_STRSTREAM)
/* #define instead of typedef so everything is hidden, and not available
   or conflicting with other users. */
#define	istrstream	shore_compat::istrstream
#define	ostrstream	shore_compat::ostrstream
#define	strstreambuf	shore_compat::strstreambuf
#endif

class w_istrstream : public istrstream {
public:
	w_istrstream(const char *s)
	: istrstream(VCPP_BUG_1 s, strlen(s))
	{
	}

	w_istrstream(const char *s, size_t l)
	: istrstream(VCPP_BUG_1 s, l)
	{
	}

};

class w_ostrstream : public ostrstream {
	/* this only exists for the brain-damaged take_c_str(), eliminating
	   it and the functionality is planned. */
	   
	bool	taken;

public:
	w_ostrstream()
	: ostrstream(),
	  taken(false)
	{
	}

	w_ostrstream(char *s, size_t l)
	: ostrstream(s, l),
	  taken(false)
	{
	}

	~w_ostrstream()
	{
		/* Take the string back, avoid memory leaks */
		if (!taken)
			freeze(0);
	}

	void freeze(bool freeze_it = true)
	{
#ifdef _MSC_VER
		rdbuf()->freeze(freeze_it);
#else
		ostrstream::freeze(freeze_it);
#endif
		taken = freeze_it;
	}

	/* Guarantee that the buffer terminates with a NUL */
	const char *c_str()
	{
		const char	*s = str();
		int		l = pcount();
		int		last = (l == 0 ? 0 : l-1);

		// ensure it is nul-terminated
		if (s[last] != '\0') {
			*this << ends;

			// it could move with the addition
			s = str();
			int	l2 = pcount();
			last = (l2 == 0 ? 0 : l2-1);

			// no growth ... the end string didn't fit
			// over-write the last valid char.
			// a throw would be a possibility too.
			if (l == l2 || s[last] != '\0')
				((char *)s)[last] = '\0';
		}
		return s;
	}

	/* Data() + size() allows access to buffer with nulls */
	const char *data()
	{
		return str();
	}

	/* A snapshot of the buffer as it currently is .. but still frozen */
	const char *new_c_str()
	{
		const char *s = c_str();
		char *t = new char[strlen(s) + 1]; 
		if (t)
			strcpy(t, s);
		return t;
	}

	/* take the string ownership away from the stream, and that can
	   never be touched again. */
	/* XXX This is bad functionality, it exists for only 1 thing
	   in shore, w_rc info messages, and it will need to be revisited
	   when stringstreams with memory are eventually used.

	   IN OTHER WORDS, DO NOT EVER USE THIS!
	 */
	const char *take_c_str()
	{
		taken = true;
		return str();
	}

#if 0
	/* not all c++ have strings, and shore doesn't use them */
	const string &cxx_str()
	{
		return string(str(), pcount));
	}
#endif
};


/* Older strstreams have different buffer semantics than newer ones */

#if defined(__GNUG__)
#if W_GCC_THIS_VER < W_GCC_VER(3,0)
#define W_STRSTREAM_NEED_SETB
#endif
#endif
#if defined(_MSC_VER)
#if _MSC_VER < 1300
#define W_STRSTREAM_NEED_SETB
#endif
#endif


/* XXX this would be easy as a template, but too much effort to maintain,
   and the stack issue would still be there. */

class w_ostrstream_buf : public w_ostrstream {
	// maximum stack frame impingement
	enum { default_buf_size = 128 };

	char	*_buf;
	size_t	_buf_size;

	char	_default_buf[default_buf_size];

	// access underlying functions ... disgusting, but scoped funky
	class w_ostrstreambuf : public strstreambuf {
	public:
		void public_setbuf(char *start, size_t len)
		{
			// nothing to read
			setg(start, start, start);

			// just the buf to write
			setp(start, start + len);

#ifdef W_STRSTREAM_NEED_SETB
			// and the underlying buffer too
			setb(start, start+len);
#undef W_STRSTREAM_NEED_SETB
#endif
		}
	};

public:
	w_ostrstream_buf(size_t len)
	: w_ostrstream(_default_buf, default_buf_size),
	  _buf(_default_buf),
	  _buf_size(len <= default_buf_size ? len : (size_t)default_buf_size) // min
	{
		if (len > _buf_size)
			_buf = new char[len];

		if (len != _buf_size) {
			_buf_size = len;
			((w_ostrstreambuf*) rdbuf())->public_setbuf(_buf, _buf_size);
		}
	}

	~w_ostrstream_buf()
	{
		if (_buf != _default_buf)
			delete [] _buf;
		_buf = 0;
		_buf_size = 0;
	}
};

#ifdef W_USE_COMPAT_STRSTREAM
#undef istrstream
#undef ostrstream
#undef strstreambuf
#endif

#endif /* !W_USE_SSTREAM */


/*<std-footer incl-file-exclusion='W_STRSTREAM_H'>  -- do not edit anything below this line -- */

#endif          /*</std-footer>*/

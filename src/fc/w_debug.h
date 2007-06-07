/*<std-header orig-src='shore' incl-file-exclusion='W_DEBUG_H'>

 $Id: w_debug.h,v 1.18 2006/01/29 22:27:32 bolo Exp $

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

#ifndef W_DEBUG_H
#define W_DEBUG_H

#include "w_defines.h"

/*  -- do not edit anything above this line --   </std-header>*/

#ifdef __GNUG__
#pragma interface
#endif

#ifndef W_BASE_H
/* NB: DO NOT make this include w.h -- not yet */
#include <w_base.h>
#endif /* W_BASE_H */

#include <w_stream.h>

#ifdef __cplusplus
#ifndef ERRLOG_H
#include <errlog.h>
#endif /* ERRLOG_H */
#endif /* __cplusplus */

/* ************************************************************************
*  This is a set of macros for use with C or C++. They give various
*  levels of debugging printing when compiled with -DDEBUG, and 
*  generate no code under -UDEBUG.
*  With -DDEBUG, message printing is under the control of an environment
*  variable DEBUG_FLAGS (see debug.cpp).  If that variable is set, its value must 
*  be  a string.  The string is searched for __FILE__ and the function name 
*  in which the debugging message occurs.  If either one appears in the
*  string (value of the env variable), or if the string contains the
*  word "all", the message is printed.  
*
*
**** DUMP(x)  prints x (along with line & file)  
*             if "x" is found in debug environment variable
*
**** FUNC(fname)  makes a local variable "_w_fname_debug__", whose value is
*             the string "fname", then DUMPs the function name.
*
**** RETURN   prints that the function named by _w_fname_debug__ is returning
*             (if _w_fname_debug__ appears in the debug env variable).
*             This macro  MUST appear within braces if used after "if",
*    	      "else", "while", etc.
*
**** DBG(arg) prints line & file and the message arg if _w_fname_debug__
*    		  appears in the debug environment variable.
*             The argument must be the innermost part of legit C++
*             print statement, and it works ONLY in C++ sources.
*
**** _TODO_     prints line & file and "TODO", then fails on assert.
*
*  Example :
*
*    returntype
*    proc(args)
*    {
*    	FUNC(proc);
*       ....body...
*
*       DBG(
*          << "message" << value
*          << "more message";
*          if(test) {
*             cerr << "xyz";
*          }
*          cerr
*       )
*
*       ....more body...
*       if(predicate) {
*           RETURN value;
*    	}
*    }
*
*/
#include <cassert>
#include <unix_error.h>
#ifndef CAT_H
#include "cat.h"
#endif

#undef USE_REGEX

#ifdef USE_REGEX
#include "regex_posix.h"
#endif /* USE_REGEX */

/* XXX missing type in vc++, hack around it here too, don't pollute
   global namespace too badly. */
#ifdef _WIN32
typedef	long	w_dbg_fmtflags;
#else
typedef	ios::fmtflags	w_dbg_fmtflags;
#endif

/* ************************************************************************ 
 * 
 * DUMP, FUNC, and RETURN macros:
 *
 */
#ifdef _WINDOWS
#define _strip_filename(f) (strrchr(f, '\\')?strrchr(f, '\\'):f)
#else
#define _strip_filename(f) f
#endif

#ifdef W_TRACE

#    	define DUMP(str)\
	if(_w_debug.flag_on(_w_fname_debug__,__FILE__)) {\
	_w_debug.clog << __LINE__ << " " << _strip_filename(__FILE__) << ": " << _string(str)\
	<< flushl; }

#    define FUNC(fn)\
    	_w_fname_debug__ = _string(fn); DUMP(_string(fn));

#    define RETURN \
    		if(_w_debug.flag_on(_w_fname_debug__,__FILE__)) {\
		    w_dbg_fmtflags old = _w_debug.clog.setf(ios::dec, ios::basefield); \
		    _w_debug.clog  << __LINE__ << " " << _strip_filename(__FILE__) << ":" ; \
		    _w_debug.clog.setf(old, ios::basefield); \
		    _w_debug.clog << "return from " << _w_fname_debug__ << flushl; }\
    		return 

#else /* -UDEBUG */
#    define DUMP(str)
#    define FUNC(fn)
#    define RETURN return
#endif /*else of: ifdef W_DEBUG*/

/* ************************************************************************  */

/* ************************************************************************  
 * 
 * Class w_debug, macros DBG, DBG_NONL, DBG1, DBG1_NONL:
 */

#ifdef W_TRACE
/*
 * Alternative #2: Make _w_fname_debug static so that it doesn't rely
 * on an extern variable, and when the entire mess is compiled
 * with -UDEBUG, there's nothing.
 */
static const char    *_w_fname_debug__=0;
inline const char    *_w_fname_debug_make_gcc_silent__() {
	/* this unused inline function seems to be enough to keep
	 * gcc silent about the unused fname_debug__ variable.
	 */
	return _w_fname_debug__;
}
#endif

#if defined(__cplusplus)

	class w_debug : public ErrLog {
	private:
#if defined(_WIN32) && defined(FC_DEBUG_WIN32_LOCK)
		CRITICAL_SECTION _crit;
#endif
		char *_flags;
		enum { _all = 0x1, _none = 0x2 };
		unsigned int		mask;
		int			_trace_level;

#ifdef USE_REGEX
		static regex_t		re_posix_re;
		static bool		re_ready;
		static char*		re_error_str;
		static char*		re_comp_debug(const char* pattern);
		static int		re_exec_debug(const char* string);
#endif /* USE_REGEX */

		int			all(void) { return (mask & _all) ? 1 : 0; }
		int			none(void) { return (mask & _none) ? 1 : 0; }

	public:
		w_debug(const char *n, const char *f);
		~w_debug();
		int flag_on(const char *fn, const char *file);
		const char *flags() { return _flags; }
		void setflags(const char *newflags);
		void memdump(void *p, int len); // hex dump of memory
		int trace_level() { return _trace_level; }
	};

	extern w_debug _w_debug;
#endif  /*defined(__cplusplus)*/

#if defined(W_TRACE)&&defined(__cplusplus)


#	define DBG1(a) if(_w_debug.flag_on((_w_fname_debug__),__FILE__)) {\
	    w_dbg_fmtflags old = _w_debug.clog.setf(ios::dec, ios::basefield); \
	    _w_debug.clog  << __LINE__ << " " << _strip_filename(__FILE__) << ":" ; \
	    _w_debug.clog.setf(old, ios::basefield); \
	    _w_debug.clog  a	<< flushl; \
    }

#	define DBG1_NONL(a) if(_w_debug.flag_on((_w_fname_debug__),__FILE__)) {\
	    w_dbg_fmtflags old = _w_debug.clog.setf(ios::dec, ios::basefield); \
	    _w_debug.clog  << __LINE__ << " " << _strip_filename(__FILE__) << ":" ; \
	    _w_debug.clog.setf(old, ios::basefield); \
	    _w_debug.clog  a; \
    }

#	define DBG(a) DBG1(a)
#	define DBG_NONL(a) DBG1_NONL(a)  /* No new-line */
#	define DBG_ONLY(x) x

#else
#	define DBG(a) 
#	define DBG_NONL(a)
#	define DBG_ONLY(x)
#endif  /* defined(W_TRACE)&&defined(__cplusplus) */
/* ************************************************************************  */

/* ******************************************************
 * Use "dassert" to provide another level of debugging:
 * "dasserts" go away if -UDEBUG.
 * whereas
 * "asserts" go away only if -DNDEBUG and  -UDEBUG
 */
#ifdef W_DEBUG
#	define dassert(a) assert(a)
#else
#	define dassert(a) 
#endif /* else of: ifdef W_DEBUG*/
/* ****************************************************** */

/*
 * 
 * Macro _TODO_
 */
#define _TODO_ { _debug.clog << "TODO *****" << flushl; assert(0);  }

/* ****************************************************** */

#ifdef W_TRACE
extern class w_debug _w_debug;
#endif

#if defined(_WINDOWS) && 0
#define DBGTHRD(arg) DBG(<< "th."<< GetCurrentThreadId() << "." <<sthread_t::me()->id << " " arg)
#else 

#define DBGTHRD(arg) DBG(<<" th."<<sthread_t::me()->id << " " arg)
#endif /* !_WINDOWS */

/*<std-footer incl-file-exclusion='W_DEBUG_H'>  -- do not edit anything below this line -- */

#endif          /*</std-footer>*/

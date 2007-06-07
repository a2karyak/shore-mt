/*<std-header orig-src='shore' incl-file-exclusion='W_RC_H'>

 $Id: w_rc.h,v 1.64 1999/08/25 01:25:06 kupsch Exp $

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

#ifndef W_RC_H
#define W_RC_H

#include "w_defines.h"

/*  -- do not edit anything above this line --   </std-header>*/

/* w_sptr_t is visible from w_rc for historical reasons.  Users of
   w_sptr_t should include it themselves */
#include "w_sptr.h"

#ifdef __GNUG__
#pragma interface
#endif


/*********************************************************************
 *
 *  class w_rc_t
 *
 *  Return code.
 *
 *********************************************************************/
class w_rc_i; // forward
class w_rc_t : private w_sptr_t<w_error_t> {
    friend class w_rc_i;
public:
    NORET			w_rc_t();
    NORET			w_rc_t(w_error_t* e);
    NORET			w_rc_t(
	const char* const 	    filename,
	w_base_t::uint4_t	    line_num,
	w_base_t::uint4_t	    err_num);
    NORET			w_rc_t(
	const char* const 	    filename,
	w_base_t::uint4_t	    line_num,
	w_base_t::uint4_t	    err_num,
	w_base_t::int4_t	    sys_err);
    NORET			w_rc_t(const w_rc_t&);
    w_rc_t&			operator=(const w_rc_t&);
    NORET			~w_rc_t();

    w_error_t&			operator*() const;
    w_error_t*			operator->() const;
    NORET			operator bool() const;
    bool			is_error() const;
    w_base_t::uint4_t		err_num() const;
    w_base_t::int4_t		sys_err_num() const;
    w_rc_t&			reset();

    w_rc_t&			add_trace_info(
	const char* const 	    filename,
	w_base_t::uint4_t	    line_num);

    w_rc_t&			push(
	const char* const 	    filename,
	w_base_t::uint4_t	    line_num,
	w_base_t::uint4_t	    err_num);

    void			verify();
    void			error_not_checked();

    w_error_t*			delegate();

    void			fatal();

    /*
     *  streams
     */
    friend ostream&             operator<<(
        ostream&                    o,
        const w_rc_t&	            obj);

    static void			return_check(bool on_off);
private:
    static bool		do_check;
};

/*********************************************************************
 *
 *  class w_rc_i
 *
 *  Iterator for w_rc_t -- allows you to iterate
 *  over the w_error_t structures hanging off a
 *  w_rc_t
 *
 *********************************************************************/
class w_rc_i {
	w_rc_t 		&_rc;
	w_error_t  	*_next;
public:
	w_rc_i(w_rc_t &x) : _rc(x), _next(x.ptr()) {};
	w_base_t::int4_t	next_errnum() {
	    w_base_t::int4_t temp = 0;
	    if(_next) {
		temp = _next->err_num;
		_next = _next->next();
	    }
	    return temp;
	}
	w_error_t 	*next() {
	    w_error_t *temp = _next;
	    if(_next) {
		_next = _next->next();
	    }
	    return temp;
	}
private:
	// disabled
	w_rc_i(const w_rc_i &x);
//	: _rc( w_rc_t(w_error_t::no_error)),
//		_next(w_error_t::no_error) {};
};



/*********************************************************************
 *
 *  w_rc_t::w_rc_t()
 *
 *  Create an rc with no error. Mark as checked.
 *
 *********************************************************************/
inline NORET
w_rc_t::w_rc_t()
    : w_sptr_t<w_error_t>(w_error_t::no_error)
{
    set_flag();
}


/*********************************************************************
 *
 *  w_rc_t::w_rc_t(e)
 *
 *  Create an rc for error e. Rc is not checked.
 *
 *********************************************************************/
inline NORET
w_rc_t::w_rc_t(w_error_t* e)
    : w_sptr_t<w_error_t>(e)
{
}


/*********************************************************************
 *
 *  w_rc_t::reset()
 *
 *  Mark rc as not checked.
 *
 *********************************************************************/
inline w_rc_t&
w_rc_t::reset()
{
    clr_flag();
    return *this;
}


/*********************************************************************
 *
 *  w_rc_t::w_rc_t(rc)
 *
 *  Create an rc from another rc. Mark other rc as checked. Self
 *  is not checked.
 *
 *********************************************************************/
inline NORET
w_rc_t::w_rc_t(
    const w_rc_t& rc)
    : w_sptr_t<w_error_t>(rc)
{
    ((w_rc_t&)rc).set_flag();
    ptr()->_incr_ref();
}


/*********************************************************************
 *
 *  w_rc_t::verify()
 *
 *  Verify that rc has been checked. If not, call error_not_checked().
 *
 *********************************************************************/
inline void
w_rc_t::verify()
{
#if defined(W_DEBUG) || defined(W_DEBUG_RC)
	if (do_check && !is_flag())
		error_not_checked();
#endif
}


/*********************************************************************
 *
 *  w_rc_t::operator=(rc)
 *
 *  Copy rc to self. First verify that self is checked. Set rc 
 *  as checked, set self as unchecked.
 *
 *********************************************************************/
inline w_rc_t&
w_rc_t::operator=(
    const w_rc_t& rc)
{
    if (&rc == this)
    	return *this;

    verify();

    ptr()->_decr_ref();
    ((w_rc_t&)rc).set_flag();
    set_val(rc);
    ptr()->_incr_ref();
    return *this;
}


/*********************************************************************
 *
 *  w_rc_t::delegate()
 *
 *  Give up my error code. Set self as checked.
 *
 *********************************************************************/
inline w_error_t*
w_rc_t::delegate()
{
    w_error_t* t = ptr();
    set_val(w_error_t::no_error);
    set_flag();
    return t;
}


/*********************************************************************
 *
 *  w_rc_t::~w_rc_t()
 *
 *  Destructor. Verify status.
 *
 *********************************************************************/
inline NORET
w_rc_t::~w_rc_t()
{
    verify();
    ptr()->_decr_ref();
}


/*********************************************************************
 *
 *  w_rc_t::operator->()
 *  w_rc_t::operator*()
 *
 *  Return pointer (reference) to content. Set self as checked.
 *
 *********************************************************************/
inline w_error_t*
w_rc_t::operator->() const
{
    ((w_rc_t*)this)->set_flag();
    return ptr();
}
    

inline w_error_t&
w_rc_t::operator*() const
{
    return *(operator->());
}


/*********************************************************************
 *
 *  w_rc_t::is_error()
 *
 *  Return true if pointing to an error. Set self as checked.
 *
 *********************************************************************/
inline bool
w_rc_t::is_error() const
{
    ((w_rc_t*)this)->set_flag();
    return ptr()->err_num != 0;
}


/*********************************************************************
 *
 *  w_rc_t::err_num()
 *
 *  Return the error code in rc.
 *
 *********************************************************************/
inline w_base_t::uint4_t
w_rc_t::err_num() const
{
    // consider this to constitite a check.
    ((w_rc_t*)this)->set_flag();
    return ptr()->err_num;
}


/*********************************************************************
 *
 *  w_rc_t::sys_err_num()
 *
 *  Return the system error code in rc.
 *
 *********************************************************************/
inline w_base_t::int4_t
w_rc_t::sys_err_num() const
{
    return ptr()->sys_err_num;
}


/*********************************************************************
 *
 *  w_rc_t::operator bool()
 *
 *  Return non-zero if rc contains an error.
 *
 *********************************************************************/
inline NORET
w_rc_t::operator bool() const
{
    return is_error();
}



/*********************************************************************
 *
 *  Basic macros for using rc.
 *
 *  RC(x)   : create an rc for error code x.
 *  RCOK    : create an rc for no error.
 *  MAKERC(bool, x):	create an rc if true, else RCOK
 *
 *  e.g.  if (eof) 
 *            return RC(eENDOFFILE);
 *        else
 *            return RCOK;
 *  With MAKERC, this can be converted to
 *       return MAKERC(eof, eENDOFFILE);
 *
 *********************************************************************/
#define RC(x)		w_rc_t(__FILE__, __LINE__, x)
#define	RC2(x,y)	w_rc_t(__FILE__, __LINE__, x, y)
#define RCOK		w_rc_t(w_error_t::no_error)
#define	MAKERC(condition,err)	((condition) ? RC(err) : RCOK)



/********************************************************************
 *
 *  More Macros for using rc.
 *
 *  RC_AUGMENT(rc)   : add file and line number to the rc
 *  RC_PUSH(rc, e)   : add a new error code to rc
 *
 *  e.g. 
 *	w_rc_t rc = create_file(f);
 *      if (rc)  return RC_AUGMENT(rc);
 *	rc = close_file(f);
 *	if (rc)  return RC_PUSH(rc, eCANNOTCLOSE)
 *
 *********************************************************************/

#define RC_AUGMENT(rc)					\
    rc.add_trace_info(__FILE__, __LINE__)

#define RC_PUSH(rc, e)					\
    rc.push(__FILE__, __LINE__, e)

#define RC_SET_MSG(rc, m)				\
do {							\
    ostrstream os;					\
    os m << ends;					\
    rc->set_more_info_msg(os.str());			\
} while (0)

#define RC_APPEND_MSG(rc, m)				\
do {							\
    ostrstream os;					\
    os m << ends;					\
    rc->append_more_info_msg(os.str());			\
} while (0)

#define W_RETURN_RC(x)					\
do {							\
    return RC(x);					\
}  while (0)

#define W_RETURN_RC_MSG(x, m)				\
do {							\
    w_rc_t __e = RC(x);					\
    RC_SET_MSG(__e, m);					\
    return __e;						\
} while (0)


#define W_DO(x)  					\
do {							\
    w_rc_t __e = (x);					\
    if (__e) return RC_AUGMENT(__e);			\
} while (0)

#define W_DO_MSG(x, m)					\
do {							\
    w_rc_t __e = (x);					\
    if (__e) {						\
        RC_AUGMENT(__e);				\
	RC_APPEND_MSG(__e, m);				\
	return __e;					\
    }							\
} while (0)

// W_DO_GOTO must use an w_error_t* parameter (err) since
// HP_CC does not support labels in blocks where an object
// has a destructor.  Since w_rc_t has a destructor this
// is a serious limitation.
#define W_DO_GOTO(err/*w_error_t**/, x)  		\
do {							\
    (err) = (x).delegate();				\
    if (err != w_error_t::no_error) goto failure;	\
} while (0)

#define W_DO_PUSH(x, e)					\
do {							\
    w_rc_t __e = (x);					\
    if (__e)  { return RC_PUSH(__e, e); }		\
} while (0)

#define W_DO_PUSH_MSG(x, e, m)				\
do {							\
    w_rc_t __e = (x);					\
    if (__e)  {						\
    	RC_PUSH(__e, e);				\
	RC_SET_MSG(__e, m);				\
	return __e;					\
    }							\
} while (0)

#define W_COERCE(x)  					\
do {							\
    w_rc_t __e = (x);					\
    if (__e)  {						\
	RC_AUGMENT(__e);				\
	__e.fatal();					\
    }							\
} while (0)

#define W_COERCE_MSG(x, m)				\
do {							\
    w_rc_t __e = (x);					\
    if (__e)  {						\
	RC_APPEND_MSG(__e, m);				\
	W_COERCE(__e);					\
    }							\
} while (0)

#define W_FATAL(e)		W_COERCE(RC(e))
#define W_FATAL_MSG(e, m)	W_COERCE_MSG(RC(e), m)

#define W_IGNORE(x)	((void) x.is_error())

#define W_PRINT_ASSERT(m)  do {cerr m << flush;} while(0)

/* redefine W_COERCE and W_PRINT_ASSERT if USE_EXTERNAL_TRACE_EVENTS */
#ifdef USE_EXTERNAL_TRACE_EVENTS
#ifdef EXTTRACEEVENTS_H
#error extTraceEvents.h included before w_rc.h
#endif
#include "extTraceEvents.h"
#endif

/*<std-footer incl-file-exclusion='W_RC_H'>  -- do not edit anything below this line -- */

#endif          /*</std-footer>*/

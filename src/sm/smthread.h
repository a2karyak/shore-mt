/*<std-header orig-src='shore' incl-file-exclusion='SMTHREAD_H'>

 $Id: smthread.h,v 1.97 2007/05/18 21:43:29 nhall Exp $

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

#ifndef SMTHREAD_H
#define SMTHREAD_H

#include "w_defines.h"

/*  -- do not edit anything above this line --   </std-header>*/

#ifndef W_H
#include <w.h>
#endif
#ifndef SM_BASE_H
#include <sm_base.h>
#endif
#ifndef STHREAD_H
#include <sthread.h>
#endif

enum {
    WAIT_FOREVER = sthread_t::WAIT_FOREVER,
    WAIT_IMMEDIATE = sthread_t::WAIT_IMMEDIATE,
    WAIT_SPECIFIED_BY_XCT = sthread_t::WAIT_SPECIFIED_BY_XCT,
    WAIT_SPECIFIED_BY_THREAD = sthread_t::WAIT_SPECIFIED_BY_THREAD
};
typedef sthread_t::timeout_in_ms timeout_in_ms;

class xct_t;
class xct_log_t;
class sdesc_cache_t;
class lockid_t;

#ifdef __GNUG__
#pragma interface
#endif

class smthread_t;

class SmthreadFunc {
public:
	virtual ~SmthreadFunc();
	
	virtual void operator()(const smthread_t& smthread) = 0;
};

class SelectSmthreadsFunc : public ThreadFunc
{
    public:
	SelectSmthreadsFunc(SmthreadFunc& func) : f(func) {};
	void operator()(const sthread_t& thread);
    private:
	SmthreadFunc&	f;
};

class PrintSmthreadsOfXct : public SmthreadFunc
{
    public:
	PrintSmthreadsOfXct(ostream& out, const xct_t* x) : o(out), xct(x) {};
	void operator()(const smthread_t& smthread);
    private:
	ostream&	o;
	const xct_t*	xct;
};



typedef void st_proc_t(void*);

class smthread_stats_t;

class smthread_t : public sthread_t {
    friend class smthread_init_t;
    struct tcb_t {
	void*   user;
	xct_t*	xct;
	int	pin_count;  	// number of rsrc_m pins
	int	prev_pin_count; // previous # of rsrc_m pins
	bool	_in_sm;  	// thread is in sm ss_m:: function
	timeout_in_ms lock_timeout;	// timeout to use for lock acquisitions
#ifdef ARCH_LP64
	/* XXX Really want kc_buf aligned to the alignment of the most
	   restrictive type. It would be except sizeof above bool == 8,
	   and timeout_in_ms is 4 bytes. */
#ifdef notyet
	/* This would do it but it wastes space.   Perhaps the thing to do
	   is to make a class for the kc_stuff -- with big pages it
	   causes offsets of commonly used things in the TCB to become
	   large -- which causes 2 instructions to be used for the offset
	   and blows caching out of the water.  Something that dynamically
	   resizes might be just the ticket. */
	most_restrictive_aligned_type	_fill;
#else
	fill4	_fill;		
#endif
#endif
#if 1
        union {
		/* XXX see above "most restrictive aligned type" approx. */
		double	kc_buf_align;
		char	kc_buf[smlevel_0::page_sz];
	};
#else		
	char	kc_buf[smlevel_0::page_sz];
#endif
	int	kc_len;
	cvec_t	kc_vec;
	sdesc_cache_t	*_sdesc_cache;
	lockid_t	*_lock_hierarchy;
	xct_log_t* _xct_log;
	smthread_stats_t*	_stats;

	void 	attach_stats();
	void    detach_stats();
        inline smthread_stats_t& thread_stats() { return *_stats; }

	tcb_t() : user(0), xct(0), 
	    pin_count(0), prev_pin_count(0),
	    _in_sm(false), lock_timeout(WAIT_FOREVER), // default for a thread
	    kc_len(0), _sdesc_cache(0), _lock_hierarchy(0), 
	    _xct_log(0), _stats(0)
	{ 
#ifdef PURIFY_ZERO
	    kc_vec.reset();
	    memset(kc_buf, '\0', sizeof(kc_buf));
#endif
#ifdef BOLO_DEBUG
	    if ((ptrdiff_t)kc_buf & 0x7)
		    cerr << "tcb_t " << this << " kc_buf misaligned "
			 << (void*) kc_buf << endl;
#endif	    
	    attach_stats();
	}
	~tcb_t() { detach_stats(); }
    };

    tcb_t		_tcb;
    st_proc_t* const  	_proc;
    void* const		_arg;

public:

    NORET			smthread_t(
	st_proc_t* 		    f, 
	void* 			    arg,
	priority_t 		    priority = t_regular,
	const char* 		    name = 0, 
	timeout_in_ms		    lockto = WAIT_FOREVER,
	unsigned		    stack_size = default_stack);
    NORET			smthread_t(
	priority_t 		    priority = t_regular,
	const char* 		    name = 0,
	timeout_in_ms 		    lockto = WAIT_FOREVER,
	unsigned		    stack_size = default_stack);

    NORET			~smthread_t();

    virtual void 		run() = 0;
    virtual smthread_t*		dynamic_cast_to_smthread();
    virtual const smthread_t*	dynamic_cast_to_const_smthread() const;
    enum SmThreadTypes		{smThreadType = 1, smLastThreadType};
    virtual int			thread_type() { return smThreadType; }

    static void			for_each_smthread(SmthreadFunc& f);
    


    void 			attach_xct(xct_t* x);
    void 			detach_xct(xct_t* x);

    // set and get lock_timeout value
    inline
    timeout_in_ms		lock_timeout() { 
				    return tcb().lock_timeout; 
				}
    inline 
    void 			lock_timeout(timeout_in_ms i) { 
					tcb().lock_timeout = i;
				}

    // xct this thread is running
    inline
    xct_t* 			xct() { return tcb().xct; }

    inline
    xct_t* 			xct() const { return tcb().xct; }

    // XXX assumes all threads are smthreads
    static smthread_t* 		me() { return (smthread_t*) sthread_t::me(); }

    inline smthread_stats_t& thread_stats() { return tcb().thread_stats(); }
#define GET_TSTAT(x) me()->thread_stats().x
#define INC_TSTAT(x) me()->thread_stats().x++
#define ADD_TSTAT(x,y) me()->thread_stats().x += (y)
#define SET_TSTAT(x,y) me()->thread_stats().x = (y)

    /*
     *  These functions are used to verify than nothing is
     *  left pinned accidentally.  Call mark_pin_count before an
     *  operation and check_pin_count after it with the expected
     *  number of pins that should not have been realeased.
     */
    void 			mark_pin_count();
    void 			check_pin_count(int change);
    void 			check_actual_pin_count(int actual) ;
    void 			incr_pin_count(int amount) ;
    int	 			pin_count() ;
   
    /*
     *  These functions are used to verify that a thread
     *  is only in one ss_m::, scan::, or pin:: function at a time.
     */
    inline
    void 			in_sm(bool in)	{ tcb()._in_sm = in; }

    inline 
    bool 			is_in_sm() const { return tcb()._in_sm; }

    inline
    void*& 			user_p()  	{ return tcb().user; }

    inline
    char*			kc_buf() {
				    return tcb().kc_buf;
				}
    inline
    int				kc_len() {
				    return tcb().kc_len;
				}

    inline
    cvec_t&			kc_vec() { 
				    return tcb().kc_vec;
				}

    inline
    void			set_kc_len(int len) {
				    tcb().kc_len = len;
				}
    void			new_xct(xct_t *);
    void			no_xct(xct_t *);

    inline
    xct_log_t*  		xct_log() {
				    return tcb()._xct_log;
				}


    inline
    lockid_t * 			lock_hierarchy() {
				    return tcb()._lock_hierarchy;
				}

    inline
    sdesc_cache_t * 		sdesc_cache() {
				    return tcb()._sdesc_cache;
				}

    virtual void		_dump(ostream &) const; // to be over-ridden
    virtual void 		vtable_collect(vtable_info_t& t);


    /* thread-level block() and unblock aren't public or protected
       accessible.  Control sm thread-level blocking with ordinary
       synchronization tools at the sm level */
    w_rc_t			block(timeout_in_ms WAIT_FOREVER,
				      const char * const caller = 0,
				      const void * id = 0);
    w_rc_t			unblock(const w_rc_t &rc = *(w_rc_t*)0);

    /* block/unblock is used as an adhoc sync. method instead of
       using "guaranteed" synchronization.  Some places in the code
       which block/unblock may already have a mutex that locks
       the synchronization area.  This interface allows those locations
       to block(area_mutex) and get rid of the overhead associated
       with locking another mutex for the sm-level block */
    w_rc_t			block(smutex_t &on,
				      timeout_in_ms WAIT_FOREVER,
				      const char * const why =0);
    w_rc_t			unblock(smutex_t &on,
					const w_rc_t &rc = *(w_rc_t*)0);
    void			prepare_to_block();

    /* functions to get/set whether this thread should generate log warnings */
    bool			generate_log_warnings() const;
    void			generate_log_warnings(bool b);

private:
    void			user(); /* disabled sthread_t::user */

    /* sm-specif block / unblock implementation */
    smutex_t			_block;
    scond_t			_awaken;
    bool			_unblocked;
    bool			_waiting;
    w_rc_t			_sm_rc;
    bool			generateLogWarnings;

    inline
    tcb_t			&tcb() { return _tcb; }

    inline
    const tcb_t			&tcb() const { return _tcb; }
};

class smthread_init_t {
public:
    NORET			smthread_init_t();
    NORET			~smthread_init_t();
private:
    static int 			count;
};




inline smthread_t* 
me() 
{ 
    return smthread_t::me(); 
}


inline xct_t* 
xct() 
{ 
    return me()->xct(); 
}


inline void 
smthread_t::mark_pin_count()
{	
    tcb().prev_pin_count = tcb().pin_count;
}

inline void 
smthread_t::check_pin_count(int W_IFDEBUG(change)) 
{
#ifdef W_DEBUG
    int diff = tcb().pin_count - tcb().prev_pin_count;
    if (change >= 0) {
	w_assert3(diff <= change);
    } else {
	w_assert3(diff >= change);
    }
#endif /* W_DEBUG */
}

inline void 
smthread_t::check_actual_pin_count(int W_IFDEBUG(actual)) 
{
    w_assert3(tcb().pin_count == actual);
}


inline void 
smthread_t::incr_pin_count(int amount) 
{
    tcb().pin_count += amount; // w_assert3(tcb().pin_count >= 0);
}

inline int 
smthread_t::pin_count() 
{
    return tcb().pin_count;
}

inline bool smthread_t::generate_log_warnings() const
{
    return generateLogWarnings;
}

inline void smthread_t::generate_log_warnings(bool b)
{
    generateLogWarnings = b;
}

void
DumpBlockedThreads(ostream& o);

/*
 * redefine DBGTHRD to use our threads
 */
#ifdef DBGTHRD
#undef DBGTHRD
#endif
#define DBGTHRD(arg) DBG(<< " th." << smthread_t::me()->id << " " arg)
#ifdef W_TRACE
/* 
 * redefine FUNC to print the thread id
 */
#undef FUNC
#define FUNC(fn)\
	_w_fname_debug__ = _string(fn); DBGTHRD(<< _string(fn));
#endif /* W_TRACE */


/*
 * class to set the generate log warnings for the thread and to restore when
 * the object is destroyed.
 */
class DisableGenLogWarnings
{
    public:
        DisableGenLogWarnings(bool newValue = false);
	~DisableGenLogWarnings();
    private:
        bool	oldValue;
};

inline DisableGenLogWarnings::DisableGenLogWarnings(bool newValue)
{
    oldValue = me()->generate_log_warnings();
    me()->generate_log_warnings(newValue);
}

inline DisableGenLogWarnings::~DisableGenLogWarnings()
{
    me()->generate_log_warnings(oldValue);
}



/*<std-footer incl-file-exclusion='SMTHREAD_H'>  -- do not edit anything below this line -- */

#endif          /*</std-footer>*/

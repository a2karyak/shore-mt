/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

/*
 *  $Id: smthread.h,v 1.51 1996/05/06 20:11:59 nhall Exp $
 */
#ifndef SMTHREAD_H
#define SMTHREAD_H

#define W_INCL_LIST
#include <w.h>

#include <sm_base.h>
#include <sthread.h>

const WAIT_FOREVER = sthread_t::WAIT_FOREVER;
const WAIT_IMMEDIATE = sthread_t::WAIT_IMMEDIATE;
const WAIT_SPECIFIED_BY_XCT = sthread_t::WAIT_SPECIFIED_BY_XCT;
const WAIT_SPECIFIED_BY_THREAD = sthread_t::WAIT_SPECIFIED_BY_THREAD;

class xct_t;
class sdesc_cache_t;
class lockid_t;

#ifdef __GNUG__
#pragma interface
#endif

typedef void st_proc_t(void*);

class smthread_t : public sthread_t {
    friend class smthread_init_t;
    struct tcb_t {
	void*   user;
	xct_t*	xct;
	int	pin_count;  	// number of rsrc_m pins
	int	prev_pin_count; // previous # of rsrc_m pins
	bool	_in_sm;  	// thread is in sm ss_m:: function
	long 	lock_timeout;	// timeout to use for lock acquisitions
	char	kc_buf[smlevel_0::page_sz];
	int	kc_len;
	cvec_t	kc_vec;
	sdesc_cache_t	*_sdesc_cache;
	lockid_t	*_lock_hierarchy;

	tcb_t() : user(0), xct(0), pin_count(0), prev_pin_count(0),
	    _in_sm(false), lock_timeout(WAIT_FOREVER), // default for a thread
	    kc_len(0), _sdesc_cache(0), _lock_hierarchy(0)
	{ 
#ifdef PURIFY
	    kc_vec.reset();
	    for(int i=0; i< smlevel_0::page_sz; i++) kc_buf[i]=0;
#endif
	}
	~tcb_t() { }
    };

    tcb_t		_tcb;
    st_proc_t* const  	_proc;
    void* const		_arg;
public:
    NORET			smthread_t(
	st_proc_t* 		    f, 
	void* 			    arg,
	priority_t 		    priority = t_regular,
	bool 			    block_immediate = false,
	bool 			    auto_delete = false,
	const char* 		    name = 0, 
	long 			    lockto = WAIT_FOREVER);
    NORET			smthread_t(
	priority_t 		    priority = t_regular,
	bool 			    block_immediate = false, 
	bool 			    auto_delete = false,
	const char* 		    name = 0,
	long 			    lockto = WAIT_FOREVER);

    NORET			~smthread_t();

    virtual void 		run() = 0;
    
    void 			attach_xct(xct_t* x);
    void 			detach_xct(xct_t* x);

    // set and get lock_timeout value
    inline
    long 			lock_timeout() { 
				    return tcb().lock_timeout; 
				}
    inline 
    void 			lock_timeout(long i) { 
					tcb().lock_timeout = i;
				}

    // xct this thread is running
    inline
    xct_t* 			xct() { return tcb().xct; }

    inline
    const xct_t* 		const_xct() const { return const_tcb().xct; }

    static smthread_t* 		me() { return (smthread_t*) sthread_t::me(); }

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
   
    /*
     *  These functions are used to verify that a thread
     *  is only in one ss_m::, scan::, or pin:: function at a time.
     */
    inline
    void 			in_sm(bool in)	{ tcb()._in_sm = in; }

    inline 
    bool 			is_in_sm()	{ return tcb()._in_sm; }

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
    void			new_xct();
    void			no_xct();

    inline
    lockid_t * 			lock_hierarchy() {
				    return tcb()._lock_hierarchy;
				}

    inline
    sdesc_cache_t * 		sdesc_cache() {
				    return tcb()._sdesc_cache;
				}

    virtual void		_dump(ostream &); // to be over-ridden

private:
    void			user(); /* disabled sthread_t::user */

    inline
    tcb_t			&tcb() { return _tcb; }

    inline
    const tcb_t			&const_tcb() const { return _tcb; }
};

class smthread_init_t {
public:
    NORET			smthread_init_t();
    NORET			~smthread_init_t();
private:
    static int 			count;
};

static smthread_init_t smthread_init;



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

#ifndef DEBUG
#define change /*change not used*/
#endif
inline void 
smthread_t::check_pin_count(int change) 
#undef change
{
#ifdef DEBUG
    int diff = tcb().pin_count - tcb().prev_pin_count;
    if (change >= 0) {
	w_assert3(diff <= change);
    } else {
	w_assert3(diff >= change);
    }
#endif
}

#ifndef W_DEBUG
#define actual /*actual not used*/
#endif
inline void 
smthread_t::check_actual_pin_count(int actual) 
#undef actual
{
    w_assert3(tcb().pin_count == actual);
}


inline void 
smthread_t::incr_pin_count(int amount) 
{
    tcb().pin_count += amount; // w_assert3(tcb().pin_count >= 0);
}

#if !defined(SMTHREAD_C) && !defined(TEMPLATE_C)
#define sthread_t	error____use_smthread_t_instead
#endif /*SMTHREAD_C*/

#endif /*SMTHREAD_H*/


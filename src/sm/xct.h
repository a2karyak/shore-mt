/*<std-header orig-src='shore' incl-file-exclusion='XCT_H'>

 $Id: xct.h,v 1.143 2000/01/25 23:12:14 bolo Exp $

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

#ifndef XCT_H
#define XCT_H

#include "w_defines.h"

/*  -- do not edit anything above this line --   </std-header>*/

#ifdef __GNUG__
#pragma interface
#endif

class xct_dependent_t;

class xct_log_t : public smlevel_1 {
private:
    //per-thread-per-xct info
    bool 	_xct_log_off;
public:
    NORET	xct_log_t(): _xct_log_off(false) {};
    bool &	xct_log_off() {
	return _xct_log_off;
    }
};

class lockid_t; // forward
class sdesc_cache_t; // forward
class xct_log_t; // forward
class xct_impl; // forward
class xct_i; // forward
class restart_m; // forward
class lock_m; // forward
class lock_core_m; // forward
class lock_request_t; // forward
class xct_log_switch_t; // forward
class xct_lock_info_t; // forward
class xct_prepare_alk_log; // forward
class xct_prepare_fi_log; // forward
class xct_prepare_lk_log; // forward
class sm_quark_t; // forward
class CentralizedGlobalDeadlockClient; // forward
class smthread_t; // forward

class xct_t : public smlevel_1 {
    friend class xct_i;
    friend class xct_impl; 
    friend class smthread_t;
    friend class restart_m;
    friend class lock_m;
    friend class lock_core_m;
    friend class lock_request_t;
    friend class xct_log_switch_t;
    friend class xct_prepare_alk_log;
    friend class xct_prepare_fi_log; 
    friend class xct_prepare_lk_log; 
    friend class sm_quark_t; 
    friend class CentralizedGlobalDeadlockClient; 

protected:
    enum commit_t { t_normal = 0, t_lazy = 1, t_chain = 2 };
public:
    typedef xct_state_t 	state_t;

public:
    NORET			xct_t(
	sm_stats_info_t* 	    stats = 0,  // allocated by caller
	timeout_in_ms		    timeout = WAIT_SPECIFIED_BY_THREAD);
    NORET			xct_t(
	const tid_t& 		    tid, 
	state_t 		    s, 
	const lsn_t&		    last_lsn,
	const lsn_t& 		    undo_nxt,
	timeout_in_ms		    timeout = WAIT_SPECIFIED_BY_THREAD);
    // NORET			xct_t(const logrec_t& r);
    NORET			~xct_t();

   friend ostream& operator<<(ostream&, const xct_t&);

   static int   		collect(vtable_info_array_t&);
   void	  			vtable_collect(vtable_info_t &);

    NORET			operator bool() const {
				    return state() != xct_stale && this != 0;
				}

    state_t			state() const;
    void 			set_timeout(timeout_in_ms t) ;
    inline
    timeout_in_ms		timeout_c() const { 
				    return  _timeout; 
				}

    /*  
     * for 2pc: internal, external
     */
public:
    bool			is_local() const;
    void 			force_readonly();

    vote_t			vote() const;
    bool			is_extern2pc() const;
    rc_t			enter2pc(const gtid_t &g);
    const gtid_t*   		gtid() const;
    const server_handle_t& 	get_coordinator()const; 
    void 			set_coordinator(const server_handle_t &); 
    static rc_t			recover2pc(const gtid_t &g,
					bool mayblock, xct_t *&);  
    static rc_t			query_prepared(int &numtids);
    static rc_t			query_prepared(int numtids, gtid_t l[]);
    static xct_t *		find_coordinated_by(const server_handle_t &t);

    rc_t			prepare();
    rc_t			log_prepared(bool in_chkpt=false);
    bool			is_distributed() const;

    /*
     * basic tx commands:
     */
    static void 		dump(ostream &o); 
    static int 			cleanup(bool dispose_prepared=false); 
				 // returns # prepared txs not disposed-of

    tid_t 			tid() const { return _tid; }

    bool	 	    	is_instrumented() {
					return (__stats != 0);
				}
    void			give_stats(sm_stats_info_t* s) {
				    w_assert1(__stats == 0);
				    __stats = s;
				}
    void	 	    	clear_stats() {

				    // save & restore xlog_bytes_generated, 
				    // since the SM uses it
				    unsigned long save = 
					__stats->summary_thread.xlog_bytes_generated;
				    memset(__stats,0, sizeof(*__stats)); 
				    __stats->summary_thread.xlog_bytes_generated =  save;
				}
    sm_stats_info_t* 	    	steal_stats() {
				    sm_stats_info_t*s = __stats; 
				    __stats = 0;
				    return	 s;
				}
    const sm_stats_info_t&	const_stats_ref() { return *__stats; }
    rc_t			commit(bool lazy = false);
    rc_t			rollback(lsn_t save_pt);
    rc_t			save_point(lsn_t& lsn);
    rc_t			chain(bool lazy = false);
    rc_t			abort(bool save_stats = false);

    // used by restart.cpp, some logrecs
protected:
    sm_stats_info_t&		stats_ref() { return *__stats; }
    rc_t			dispose();
    void                        change_state(state_t new_state);
    void			set_first_lsn(const lsn_t &) ;
    void			set_last_lsn(const lsn_t &) ;
    void			set_undo_nxt(const lsn_t &) ;

public:

    // used by checkpoint, restart:
    const lsn_t&		last_lsn() const;
    const lsn_t&		first_lsn() const;
    const lsn_t&		undo_nxt() const;
    const logrec_t*		last_log() const;

    // used by restart, chkpt among others
    static xct_t*		look_up(const tid_t& tid);
    static tid_t 		oldest_tid();	// with min tid value
    static tid_t 		youngest_tid();	// with max tid value
    static void 		update_youngest_tid(const tid_t &);

    // used by sm.cpp:
    static uint4_t		num_active_xcts();

    // used for compensating (top-level actions)
    const lsn_t& 		anchor(bool grabit = true);
    void  			start_crit() { (void) anchor(false); }
    void 			release_anchor(bool compensate=true);
    void  			stop_crit() { (void) release_anchor(false); }
    void 			compensate(const lsn_t&, bool undoable = false);
    // for recovery:
    void 			compensate_undo(const lsn_t&);


public:
    // used in sm.cpp
    rc_t			add_dependent(xct_dependent_t* dependent);
    bool			find_dependent(xct_dependent_t* dependent);

    //
    //	logging functions -- used in logstub_gen.cpp only, so it's inlined here:
    //
    bool			is_log_on() const {
				    return (!me()->xct_log()->xct_log_off());
				}
    rc_t			get_logbuf(logrec_t*&);
    void 			give_logbuf(logrec_t*, const page_p *p = 0);
    void			flush_logbuf();

    //
    //	Used by I/O layer
    //
    void			AddStoreToFree(const stid_t& stid);
    void			AddLoadStore(const stid_t& stid);
    //	Used by vol.cpp
    void                        set_alloced();
    void                        set_freed();

    void 			num_extents_marked_for_deletion(
					base_stat_t &num) const;
public:

    //	For SM interface:
    void			GetEscalationThresholds(int4_t &toPage, 
					int4_t &toStore, int4_t &toVolume);
    void			SetEscalationThresholds(int4_t toPage, 
					int4_t toStore, int4_t toVolume);
    bool 			set_lock_cache_enable(bool enable);
    bool 			lock_cache_enabled();

protected:
    /////////////////////////////////////////////////////////////////
    // the following is put here because smthread 
    // doesn't know about the structures
    // and we have changed these to be a per-thread structures.
    static lockid_t*  		new_lock_hierarchy();
    static sdesc_cache_t*  	new_sdesc_cache_t();
    static xct_log_t*  		new_xct_log_t();
    void			steal(lockid_t*&, sdesc_cache_t*&, xct_log_t*&);
    void			stash(lockid_t*&, sdesc_cache_t*&, xct_log_t*&);

protected:
    int				attach_thread(); // returns # attached 
    int				detach_thread(); // returns # attached


    // stored per-thread, used by lock.cpp
    lockid_t*  			lock_info_hierarchy() const {
				    return me()->lock_hierarchy();
				}
public:
    // stored per-thread, used by dir.cpp
    sdesc_cache_t*    		sdesc_cache() const;

protected:
    // for xct_log_switch_t:
    switch_t 			set_log_state(switch_t s, bool &nested);
    void 			restore_log_state(switch_t s, bool nested);


public:
    ///////////////////////////////////////////////////////////
    // used for OBJECT_CC: TODO: remove from sm.cpp also
    // if not used elsewhere
    ///////////////////////////////////////////////////////////
    concurrency_t		get_lock_level(); // non-const: acquires mutex 
    void		   	lock_level(concurrency_t l);

    int				num_threads(); 	 

protected:
    // For use by lock manager:
    w_rc_t			lockblock(timeout_in_ms timeout);// await other thread
    void			lockunblock(); 	 // inform other waiters
    const int4_t*			GetEscalationThresholdsArray();

    rc_t			check_lock_totals(int nex, 
					int nix, int nsix, int ) const;
    rc_t			obtain_locks(lock_mode_t mode, 
					int nlks, const lockid_t *l); 
    rc_t			obtain_one_lock(lock_mode_t mode, 
					const lockid_t &l); 

    xct_lock_info_t*  		lock_info() const { return _lock_info; }
    static void			clear_deadlock_check_ids();

public:
    // use faster new/delete
    NORET			W_FASTNEW_CLASS_DECL(xct_t);

    // XXX this is only for chkpt::take().  This problem needs to
    // be fixed correctly.  DO NOT USE THIS.  Really want a
    // friend that is just a friend on some methods, not the entire class.
    static w_rc_t		acquire_xlist_mutex();
    static void			release_xlist_mutex();


/////////////////////////////////////////////////////////////////
// DATA
/////////////////////////////////////////////////////////////////
protected:
    // list of all transactions instances
    w_link_t			_xlink;
    static smutex_t		_xlist_mutex;
    static w_descend_list_t<xct_t, tid_t> _xlist;

    void 			put_in_order();

private:
    sm_stats_info_t* 	    	__stats; // allocated by user
    base_stat_t	 	    	__log_bytes_generated; // must be kept
				// even if not instrumented
    lockid_t*			__saved_lockid_t;
    sdesc_cache_t*		__saved_sdesc_cache_t;
    xct_log_t*			__saved_xct_log_t;

    static tid_t 		_nxt_tid;// Not safe for pre-emptive threads

    const tid_t 		_tid;
    timeout_in_ms		_timeout; // default timeout value for lock reqs
    xct_impl *			i_this;
    xct_lock_info_t*		_lock_info;

    /* 
     * _lock_cache_enable is protected by its own mutex, because
     * it is used from the lock manager, and the lock mgr is used
     * by the volume mgr, which necessarily holds the xct's 1thread_log
     * mutex.  Thus, in order to avoid mutex-mutex deadlocks,
     * we have a mutex to cover _lock_cache_enable that is used
     * for NOTHING but reading and writing this datum.
     */
    bool  			_lock_cache_enable;
    smutex_t			_lock_cache_enable_mutex;


    // the 1thread_xct mutex is used to ensure that only one thread
    // is using the xct structure on behalf of a transaction 
    smutex_t			_1thread_xct;
    static const char*		_1thread_xct_name; // name of xct mutex

private:
    void			acquire_1thread_xct_mutex();
    void			release_1thread_xct_mutex();

public:
    void			serialize_xct_threads() {
				    acquire_1thread_xct_mutex();
				}
    void			allow_xct_parallelism() {
				    release_1thread_xct_mutex();
				}
};

/*
 * Use X_DO inside compensated operations
 */
#define X_DO(x,anchor)             \
{                           \
    w_rc_t __e = (x);       \
    if (__e) {			\
	w_assert3(xct());	\
	W_COERCE(xct()->rollback(anchor));	\
	xct()->release_anchor(true);	\
	return RC_AUGMENT(__e); \
    } \
}

class xct_log_switch_t : public smlevel_0 {
    /*
     * NB: use sparingly!!!! EVERYTHING DONE UNDER
     * CONTROL OF ONE OF THESE IS A CRITICAL SECTION
     * This is necessary to support multi-threaded xcts,
     * to prevent one thread from turning off the log
     * while another needs it on, or vice versa.
     */
    switch_t old_state;
    bool     nested;
public:
    NORET
    xct_log_switch_t(switch_t s) 
    {
	if(smlevel_1::log) {
	    INC_TSTAT(log_switches);
	    nested = false;
	    if (xct()) {
		old_state = xct()->set_log_state(s, nested);
	    }
	}
    }

    NORET
    ~xct_log_switch_t()  {
	if(smlevel_1::log) {
	    if (xct()) {
		xct()->restore_log_state(old_state, nested);
	    }
	}
    }
};

/* XXXX This is somewhat hacky becuase I am working on cleaning
   up the xct_i xct iterator to provide various levels of consistency.
   Until then, the "locking option" provides enough variance so
   code need not be duplicated or have deep call graphs. */

class xct_i  {
public:
    // NB: still not safe, since this does not
    // lock down the list for the entire iteration.

    xct_t* 
    curr()  {
	    xct_t *x;
	    if (locked)
		    W_COERCE(xct_t::_xlist_mutex.acquire());
	    x = unsafe_iterator->curr();
	    if (locked)
		xct_t::_xlist_mutex.release();
	    return x;
    }
    xct_t* 
    next()  {
	xct_t *x;
	if (locked)
		W_COERCE(xct_t::_xlist_mutex.acquire());
	x = unsafe_iterator->next();
	if (locked)
		xct_t::_xlist_mutex.release();
	return x;
    }

    NORET xct_i(bool locked_accesses = true)
    : unsafe_iterator(0),
      locked(locked_accesses)
    {
	unsafe_iterator = new
	    w_list_i<xct_t>(xct_t::_xlist);
    }

    NORET ~xct_i() { delete unsafe_iterator; unsafe_iterator = 0; }

private:
    w_list_i<xct_t> *unsafe_iterator;
    const bool	    locked;

    // disabled
    xct_i(const xct_i&);
    xct_i& operator=(const xct_i&);
};
    

// For use in sm functions that don't allow
// active xct when entered.  These are functions that
// apply to local volumes only.
class xct_auto_abort_t : public smlevel_1 {
public:
    xct_auto_abort_t(xct_t* xct) : _xct(xct) {}
    ~xct_auto_abort_t() {
	switch(_xct->state()) {
	case smlevel_1::xct_ended:
	    // do nothing
	    break;
	case smlevel_1::xct_active:
	    W_COERCE(_xct->abort());
	    break;
	default:
	    W_FATAL(eINTERNAL);
	}
    }
    rc_t commit() {
	// These are only for local txs
	// W_DO(_xct->prepare());
	w_assert3(!_xct->is_distributed());
	W_DO(_xct->commit());
	return RCOK;
    }
    rc_t abort() {W_DO(_xct->abort()); return RCOK;}

private:
    xct_t*	_xct;
};

/*<std-footer incl-file-exclusion='XCT_H'>  -- do not edit anything below this line -- */

#endif          /*</std-footer>*/

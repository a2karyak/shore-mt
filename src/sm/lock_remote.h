/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

/*
 *  $Id: lock_remote.h,v 1.1
 */

#ifndef LOCK_REMOTE_H
#define LOCK_REMOTE_H

#ifdef MULTI_SERVER

#include <lock_s.h>

#ifdef __GNUG__
#pragma interface
#endif


class remote_lock_m : public lock_base_t {
public:

#ifdef GNUG_BUG_4
    typedef lock_base_t::mode_t mode_t;
    typedef lock_base_t::duration_t duration_t;
    typedef lock_base_t::status_t status_t;
#endif

#if defined(LOCK_REMOTE_C) || defined(BF_C) || defined(CALLBACK_C)
    static bool		purged_flag(const lpid_t& pid);
#endif

#ifndef BF_C 

    NORET		remote_lock_m() { _core = llm->_core; }

    //
    // The "optimistic" parameter should be used when (a) we don't want the lock
    // request to block (due to lock conflicts) AND (b) we want to get the lock
    // "fast". If "optimistic" is TRUE, then the lock mgr will first check
    // whether any remote servers need to get involved. Since going over the
    // network does not satisfy (b), lock() will, in this case, return with a
    // MAYBLOCK error code, without acquiring any lock. If no remote servers
    // need to be contacted, then lock() will try to get the local lock with
    // 0 timeout. If a conflict is detected, lock() will again return the
    // MAYBLOCK error code, without acquiring any lock.
    //
    // Example when (a) and (b) are true:
    // Asking for a lock while holding a latch. Normally the latch should be
    // released before asking for the lock, but in order to optimize this case
    // we first ask the lock in "non-blocking mode". If there are no conflicts
    // the lock is granted and we are done. Otherwise, we release the latch
    // and re-try for the lock, this time willing to wait for it.
    // In the single-server case, asking a lock in "non-blocking mode" was done
    // by calling lock() with 0 timeout. This is still possible in the server-
    // -to-server case, however a "lock-miss" (i.e. an unsuccessful non-blocking
    // lock req) can be very expensive now. The "optimistic" parameter should be    // used in this case to keep the "lock-miss" case cheap, so that the
    // optimization described above is still valid.
    // 
    // NOTE: if "optimistic" is TRUE, then the value of "timeout" is irrelevant.
    //
    static rc_t		lock(
	const lockid_t&	    n,
	mode_t		    m,
	duration_t	    d = t_long,
	long		    timeout = WAIT_SPECIFIED_BY_XCT,
	bool		    optimistic = FALSE,
	int*		    old_value = 0);

    static rc_t		lock_force(
    	const lockid_t&	    n,
	mode_t		    m,
	duration_t	    d = t_long,
	long		    timeout = WAIT_SPECIFIED_BY_XCT,
	bool		    optimistic = FALSE);

    static rc_t		unlock(const lockid_t& n);

    static rc_t		unlock_duration(
	duration_t	    duration,
	bool		    all_less_than);

    static rc_t		query(
	const lockid_t&	    n,
	mode_t&		    m,
	const tid_t&	    tid = tid_t::null,
	bool		    implicit = FALSE);

    static rc_t		open_quark();
    static rc_t		close_quark(bool release_locks);

#if defined(OBJECT_CC) && defined (MULTI_SERVER)
    static void		set_lock_pending(const lockid_t& name);
    static void         reset_lock_pending(const lockid_t& name);
    static void		reset_req_pending(
	const lpid_t&	    pid,
	bool		    assert_lock = TRUE);
    static void		set_req_pending(const lpid_t& pid);
    static bool		is_req_pending(const lpid_t& pid);
    static bool		req_nonpending_lookup(const lpid_t& pid);
    static bool		test_and_clear_cb_repeat(const lockid_t& name);
    static void		clear_cb_repeat(const lockid_t& name);
    static bool		is_adaptive(const lpid_t& pid);
    static void		set_adaptive(const lpid_t& pid);
    static bool		register_adaptive(
	const lockid_t&	    n,
	bool		    optimistic = FALSE);
    static bool		try_adaptive(
	const lpid_t&	    pid,
	const srvid_t&	    srv,
	bool		    check_only = FALSE);
#endif /* OBJECT_CC && MULTI_SERVER */

    void		dump() { llm->dump(); }

private:
    static rc_t		_lock(
	const lockid_t&	    n,
	mode_t		    m,
	duration_t	    d,
	long		    timeout,
	bool		    force,
	bool		    optimistic,
	int*		    old_value = 0);

    lock_core_m* _core;

#endif /* ifndef(BF_C) */
};

#endif /*LOCK_REMOTE_H*/

#endif /* MULTI_SERVER */

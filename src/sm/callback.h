/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

/*
 *  $Id: callback.h,v 1.1
 */

#ifndef CALLBACK_H
#define CALLBACK_H

#ifdef __GNUG__
#pragma interface
#endif

#ifndef MULTI_SERVER

class callback_m : public smlevel_1 {
public:
    NORET callback_m() {};
    NORET ~callback_m() {};
};

#else /* MULTI_SERVER */

class callback_op_t;
class callback_xct_proxy_t;
struct callback_t;
struct lock_head_t;
struct lock_request_t;


class adaptive_cb_t;


class callback_m : public smlevel_1 {
    friend class lock_head_t;
public:
    NORET callback_m();
    NORET ~callback_m() { delete adaptive_cb_tab; adaptive_cb_tab = 0; };

#if defined(BF_C) || defined(CALLBACK_C)
    static rc_t         purge_page(const lpid_t& pid, bool is_cached = TRUE);
#endif

#if defined(LOCK_CORE_C) || defined(CALLBACK_C)
    static rc_t         purge_page(lock_head_t& lock);
    static rc_t		adaptive_callback(lock_head_t* lock);
#endif

#if defined(REMOTE_C) || defined(LOCK_REMOTE_C) || defined(CALLBACK_C)
    static rc_t		callback(
	const lockid_t&		name,
	lock_mode_t		mode,
	const srvid_t&		req_srv,
	lock_mode_t		prev_pgmode,
	long			timeout,
	bool*			adaptive = 0);
#endif

#if defined(REMOTE_C) || defined(CALLBACK_C)
    typedef callback_op_t::cb_outcome_t cb_outcome_t;

    static rc_t		handle_cb_reply(
	callback_op_t&		cbop,
	callback_t*&		cb,
	cb_outcome_t		outcome,
	bool&			done,
	int			numblockers = 0,
	uint2			block_level = lockid_t::t_bad,
	blocker_t*		blockers = 0);
    static void         handle_cb_req(callback_xct_proxy_t* cbxct);
#endif

private:
    static rc_t		_block_cb(
	xct_t*			xd,
	callback_op_t&		cbop,
	uint2			block_level,
	lock_mode_t		block_mode);
    static void		_block_cb_upper_levels(
	xct_t*			xd,
	uint2			level,
	lockid_t*		path,
	bool&			blocker_aborted);
    static rc_t		_block_cb_at_cb_level(
	xct_t*			xd,
	callback_op_t&		cbop,
	lock_mode_t		block_mode);

#ifdef OBJECT_CC
    static rc_t		_block_rec_cb_at_page_level(
	xct_t*			xd,
	callback_op_t&		cbop);
    static void		_block_cb_handle_obj_lock(
	lock_request_t*		req,
	bool&			calling_back);

    static rc_t		_page_blocker_done(callback_op_t& cbop);
    static rc_t		_re_acquire_rec_lock(const lockid_t& name);
#endif

    static rc_t		_check_deadlock(lock_head_t* lock);
    static void		_check_lock(
	const lockid_t&		name,
	bool			check_req_pending = TRUE);

    static void		_handle_cb_req(
	callback_xct_proxy_t*	cbxct,
	lock_mode_t		mode,
	bool&			pinv,
	bool&			deadlock);
    static void		_invalidate(callback_xct_proxy_t* cbxct, uint2 level);
    static void		_finish_callback(callback_xct_proxy_t* cbxct);

#if !defined(PRREMPTIVE) && defined(OBJECT_CC)
    /*
     * Used to serialize callbacks of adaptive page locks, if threads are
     * non-preemptive. If threads are preemptive, then the mutex of the 
     * lock_head is held during the callback.
     */
    static w_hash_t<adaptive_cb_t, lpid_t>* adaptive_cb_tab;
#endif

    static long		_cbcount;	// for debugging
};


class adaptive_cb_t {
public:
    NORET       adaptive_cb_t(const lpid_t& p) : pid(p), count(0), 
							mutex("adapt_mtx") {}
    NORET	~adaptive_cb_t() { link.detach(); }

    w_link_t    link;
    lpid_t      pid;
    int         count;
    smutex_t    mutex;
};


#endif /* MULTI_SERVER */

#endif /* CALLBACK_H */


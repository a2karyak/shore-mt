/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

/*
 *  $Id: remote_client.h,v 1.18 1996/02/28 22:13:29 markos Exp $
 */
#ifndef REMOTE_CLIENT_H
#define REMOTE_CLIENT_H

// Class remote_client_m is used by low level subsystems to
// access remote server (ie. the server is playing the role
// of a client of a remote server).

class remote_m;

#ifdef __GNUG__
#pragma interface
#endif


class remote_client_m : public smlevel_0 {
public:
    remote_client_m() {}

#if defined(REMOTE_CLIENT_C) || defined(BF_C)
    rc_t                read_page(
	const lockid_t&		name,
        bfcb_t*                 b,
	uint2			ptag,
        bool                  detect_race,
	bool			parallel_read,
        bool                  is_new=FALSE);
    rc_t		flush_log(const lpid_t& pid, const lsn_t& lsn);
    rc_t send_page(const lpid_t& pid, page_s& buf);    
#endif

#if defined(REMOTE_CLIENT_C) || defined(CALLBACK_C)
#ifdef MULTI_SERVER
    typedef callback_op_t::cb_outcome_t cb_outcome_t;

    static rc_t		send_callbacks(const callback_op_t& cbop);
    static rc_t		send_callback_aborts(callback_op_t& cbop);
    static rc_t		get_cb_replies( callback_op_t& cbop);
    static rc_t		send_cb_reply(
	const callback_xct_proxy_t&	cbxct,
	cb_outcome_t			outcome,
	int				numblockers = 0,
	blocker_t*			blockers = 0,
	uint2				block_level = 0);
    static rc_t         adaptive_callback(
	const lpid_t&	    pid,
        const srvid_t&      srv,
        int                 n,
        adaptive_xct_t*     xacts,
	long		    cbid);
    static rc_t		register_purged_page(const lpid_t& pid);
#endif /* MULTI_SERVER */
#endif

#if defined(REMOTE_CLIENT_C) || defined(LID_C)
    static rc_t         mount_volume(const lvid_t& lvid);
    static rc_t		lookup_lid(
	vid_t			vid,
	const lid_t&		lid,
	lid_m::lid_entry_t&	entry); 
#endif

#if defined(REMOTE_CLIENT_C) || defined(LOCK_CORE_C)
    static rc_t         spread_xct(const vid_t& vid);
    static rc_t		send_deadlock_notification(
	callback_op_t*		cbop,
	const tid_t&		sender,
	const tid_t&		victim);
#endif

#if defined(REMOTE_CLIENT_C) || defined(LOCK_REMOTE_C) || defined(BF_C)
    static rc_t         acquire_lock(
        const lockid_t&         n,
        lock_mode_t             m,
        lock_duration_t         d = t_long,
        long                    timeout = WAIT_SPECIFIED_BY_XCT,
        bool                    force = false,
	int*			value = 0);
#endif

#if defined(REMOTE_CLIENT_C) || defined(SM_C)
    static rc_t         register_volume(const lvid_t& lvid) ;
    static rc_t         commit_xct(xct_t& xct);
    static rc_t         abort_xct(xct_t& xct);
#endif

#if defined(REMOTE_CLIENT_C) || defined(XCT_C)
    static rc_t		log_insert(logrec_t& r, lsn_t& lsn);
#endif

#if defined(REMOTE_CLIENT_C) || defined(FILE_C)
    static rc_t         get_pids(
        stid_t              stid,
	bool		    lock,
        bool              last,
        shpid_t             first,
        int&                num,
        shpid_t*            pids);
#endif
};

#endif /* REMOTE_CLIENT_H */

/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

/*
 *  $Header: /p/shore/shore_cvs/src/sm/remote_client.c,v 1.16 1996/02/28 22:13:28 markos Exp $
 */

#define SM_SOURCE
#define REMOTE_CLIENT_C

#ifdef __GNUG__
#   pragma implementation
#endif

#include "sm_int.h"
#include <srvid_t.h>
#include "remote_s.h"
#include "remote.h"
#include "remote_client.h"

rc_t remote_client_m::register_volume(const lvid_t& lvid)
{
#ifndef MULTI_SERVER
    return RCOK;
#endif /* !MULTI_SERVER */
    return remote_m::register_volume(lvid);
}

rc_t remote_client_m::mount_volume(const lvid_t& lvid)
{
#ifndef MULTI_SERVER
    return RC(eBADVOL);
#endif /* !MULTI_SERVER */
    return remote_m::mount_volume(lvid);
}

rc_t remote_client_m::spread_xct(const vid_t& vid)
{
#ifndef MULTI_SERVER
    W_FATAL(eINTERNAL);
#endif /* !MULTI_SERVER */
    return remote_m::spread_xct(vid);
}

rc_t remote_client_m::commit_xct(xct_t& xct)
{
#ifndef MULTI_SERVER
    W_FATAL(eINTERNAL);
#endif /* !MULTI_SERVER */
    return remote_m::commit_xct(xct);
}

rc_t remote_client_m::abort_xct(xct_t& xct)
{
#ifndef MULTI_SERVER
    W_FATAL(eINTERNAL);
#endif /* !MULTI_SERVER */
    return remote_m::abort_xct(xct);
}

rc_t remote_client_m::lookup_lid(vid_t vid, const lid_t& lid,
				 lid_m::lid_entry_t& entry)
{
#ifndef MULTI_SERVER
    return RC(eBADVOL);
#endif /* !MULTI_SERVER */
    return remote_m::lookup_lid(vid, lid, entry);
}

rc_t remote_client_m::acquire_lock(const lockid_t& n, lock_mode_t m,
			lock_duration_t d, long timeout, bool force,
			int* value)
{
#ifndef MULTI_SERVER
    W_FATAL(eINTERNAL);
#endif /* !MULTI_SERVER */
    return remote_m::acquire_lock(n, m, d, timeout, force, value);
}

rc_t remote_client_m::read_page(const lockid_t& name, bfcb_t* b, uint2 ptag,
		bool detect_race, bool parallel_read, bool is_new)
{
#ifndef MULTI_SERVER
    return RC(eBADVOL);
#endif /* !MULTI_SERVER */
    return remote_m::read_page(name, b, ptag, detect_race, parallel_read, is_new);
}


#ifdef SHIV
rc_t remote_client_m::send_page(const lpid_t& pid, page_s& buf)
{
#ifndef MULTI_SERVER
    return RC(eBADVOL);
#endif /* !MULTI_SERVER */
    return remote_m::send_page(pid, buf);
}
#endif /* SHIV */

#ifdef MULTI_SERVER

rc_t remote_client_m::send_callbacks(const callback_op_t& cbop)
{
    return remote_m::send_callbacks(cbop);
}

rc_t remote_client_m::send_callback_aborts(callback_op_t& cbop)
{
    return remote_m::send_callback_aborts(cbop);
}

rc_t remote_client_m::get_cb_replies(callback_op_t& cbop)
{
    return remote_m::get_cb_replies(cbop);
}

rc_t remote_client_m::send_cb_reply(const callback_xct_proxy_t& cbxct,
		cb_outcome_t outcome,
		int numblockers, blocker_t* blockers, uint2 block_level)
{
    return remote_m::send_cb_reply(cbxct, outcome,
					numblockers, blockers, block_level);
}

rc_t remote_client_m::send_deadlock_notification(callback_op_t* cbop,
				const tid_t& sender, const tid_t& victim)
{
    return remote_m::send_deadlock_notification(cbop, sender, victim);
}

#ifdef OBJECT_CC
rc_t remote_client_m::adaptive_callback(const lpid_t& pid, const srvid_t& srv,
				int n, adaptive_xct_t* xacts, long cbid)
{
    return remote_m::adaptive_callback(pid, srv, n, xacts, cbid);
}
#endif

rc_t remote_client_m::register_purged_page(const lpid_t& pid)
{
    return remote_m::register_purged_page(pid);
}

rc_t remote_client_m::log_insert(logrec_t& r, lsn_t& lsn)
{
    return remote_m::_rlog->insert(r, lsn);
}

rc_t remote_client_m::flush_log(const lpid_t& pid, const lsn_t& lsn)
{
    int nlogs;
    return remote_m::_rlog->flush(lsn, nlogs, 0, pid);
}

rc_t remote_client_m::get_pids(stid_t stid, bool lock, bool last,
				shpid_t first, int& num, shpid_t* pids)
{
    return remote_m::get_pids(stid, lock, last, first, num, pids);
}

#endif /* MULTI_SERVER */

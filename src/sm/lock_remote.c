/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

/*
 *  $Id: lock_remote.c,v 1.20 1996/06/27 17:23:01 kupsch Exp $
 */

#ifdef MULTI_SERVER

#define SM_SOURCE
#define LOCK_REMOTE_C

#ifdef __GNUG__
#pragma implementation "lock_remote.h"
#endif

#include "sm_int.h"
#include "lock_x.h"
#include "lock_core.h"
#include "callback.h"
#include "remote_client.h"
#include "remote_s.h"
#include "cache.h"


//
// NOTE: this function will query the local lock mgr only. I should write
// another method (e.g. query_remote) which will take into account locks
// held by a remote server but not by the quering server.
//
rc_t remote_lock_m::query(const lockid_t& n, mode_t& m, const tid_t& tid,
							bool implicit)
{
    return llm->query(n, m, tid, implicit);
}


rc_t remote_lock_m::unlock(const lockid_t& n)
{
    if (n.vid().is_remote()) {
	W_FATAL(eNOTIMPLEMENTED);
    }
    return llm->unlock(n);
}


//
// NOTE: So far, the following method is used only during commit or abort time
// to release all the locks of the xact. For this use there are no remote
// operations that should be performed by unlock_duration. However, if we need
// to unlock all locks of a particular duration before commit or abort time,
// then unlock_duration must be extented to unlock any remote locks as well.
//
rc_t remote_lock_m::unlock_duration(duration_t duration, bool all_less_than)
{
    return llm->unlock_duration(duration, all_less_than);
}


rc_t
remote_lock_m::open_quark()
{
#ifdef MULTI_SERVER
    if( comm_m::use_comm())  {
	W_FATAL(fcINTERNAL);  // not yet supported
    } else
#endif
    {
	W_DO(rlm->_core->open_quark(xct()));
    } 
    return RCOK;
}


rc_t
remote_lock_m::close_quark(bool release_locks)
{
#ifdef MULTI_SERVER
    if( comm_m::use_comm())  {
	W_FATAL(fcINTERNAL);  // not yet supported
    } else
#endif
    {
	W_DO(rlm->_core->close_quark(xct(), release_locks));
    }
    return RCOK;
}

#if !defined(MULTI_SERVER)
#define old_value /*old_value not used*/
#endif
rc_t
remote_lock_m::lock(const lockid_t& n, mode_t m, duration_t d,
				long timeout, bool optimistic, int* old_value)
#undef old_value
{
#ifdef MULTI_SERVER
    if(comm_m::use_comm()) {
	if (n.lspace() == lockid_t::t_extent || n.lspace() == lockid_t::t_kvl) {
	    if (optimistic) w_assert3(timeout == 0);
	    mode_t _prev_mode;
	    mode_t _prev_pgmode;
	    rc_t rc = llm->_lock(n, m, _prev_mode, _prev_pgmode, d, timeout, FALSE);
	    if (rc) {
		if (rc.err_num() == eLOCKTIMEOUT && optimistic) return RC(eMAYBLOCK);
		return rc.reset();
	    }
	    return RCOK;
	}

	rc_t rc;
	do {
	    rc = _lock(n, m, d, timeout, FALSE, optimistic, old_value);
	    if (!rc || rc.err_num() != ePREEMPTED) break;
	} while(1);

	return rc.reset();
    } else 
#endif
    {
	mode_t _prev_mode;
	mode_t _prev_pgmode;
	if (optimistic) w_assert3(timeout == 0);
	rc_t rc = llm->_lock(n, m, _prev_mode, _prev_pgmode, d, timeout, FALSE);
	if (rc) {
	    if (rc.err_num() == eLOCKTIMEOUT && optimistic) return RC(eMAYBLOCK);
	    return rc.reset();
	}
	return RCOK;
    }
}


rc_t
remote_lock_m::lock_force(const lockid_t& n, mode_t m, duration_t d,
					long timeout, bool optimistic)
{
#ifdef MULTI_SERVER
    if(comm_m::use_comm()) {
	if (n.lspace() == lockid_t::t_extent || n.lspace() == lockid_t::t_kvl) {
	    if (optimistic) w_assert3(timeout == 0);
	    mode_t _prev_mode;
	    mode_t _prev_pgmode;
	    rc_t rc = llm->_lock(n, m, _prev_mode, _prev_pgmode, d, timeout, TRUE);
	    if (rc) {
		if (rc.err_num() == eLOCKTIMEOUT && optimistic) return RC(eMAYBLOCK);
		return rc.reset();
	    }
	    return RCOK;
	}

	rc_t rc;
	do {
	    rc = _lock(n, m, d, timeout, TRUE, optimistic);
	    if (!rc || rc.err_num() != ePREEMPTED) break;
	} while(1);

	return rc.reset();
    } else 
#endif
    {

	mode_t _prev_mode;
	mode_t _prev_pgmode;
	if (optimistic) w_assert3(timeout == 0);
	rc_t rc = llm->_lock(n, m, _prev_mode, _prev_pgmode, d, timeout, TRUE);
	if (rc) {
	    if (rc.err_num() == eLOCKTIMEOUT && optimistic) return RC(eMAYBLOCK);
	    return rc.reset();
	}
	return RCOK;
    }
}


#ifdef MULTI_SERVER


rc_t
remote_lock_m::_lock(const lockid_t& n, mode_t m, duration_t d,
		long timeout, bool force, bool optimistic, int* old_value)
{
    w_assert3(n.lspace() != lockid_t::t_extent && n.lspace() !=lockid_t::t_kvl);

    switch (timeout) {
    case WAIT_SPECIFIED_BY_THREAD:
        timeout = me()->lock_timeout();
        break;
    case WAIT_SPECIFIED_BY_XCT:
        timeout = xct()->timeout_c();
        break;
    default:
        break;
    }
    w_assert3(timeout >= 0 || timeout == WAIT_FOREVER);

    if (m == UD) m = EX; // UD mode is not supported for server-to-server

    mode_t prev_mode;
    mode_t prev_pgmode;
    lockid_t name = n;
    rc_t rc;

    int level = xct()->get_lock_level();

    switch (level) {
    case t_cc_file:
    case t_cc_page:
	if (level == t_cc_file && name.lspace() > lockid_t::t_store)
	    name.truncate(lockid_t::t_store);
	else if (level == t_cc_page && name.lspace() > lockid_t::t_page)
	    name.truncate(lockid_t::t_page);

	if (n.lspace() > name.lspace()) {
	    if (m == IS || m == SH)	m = SH;
	    else			m = EX;

	} else if (n.lspace() ==  name.lspace()) {
	    if (m == IX || m == SIX)	m = EX;
	    else if (m == IS)		m = SH;
	}
	break;

#ifdef OBJECT_CC
    case t_cc_record:
	w_assert3(cc_alg == t_cc_record);
	w_assert3(n.lspace() != lockid_t::t_record || m==NL || m==SH || m==EX);
	break;
#endif

    default:
	W_FATAL(eBADCCLEVEL);
    }

    if (optimistic) {

	// In the "optimistic" case, we must check whether any remote
	// servers need to get involved BEFORE the local lock is requested.
	// Consider what can happen otherwise: a req for an EX lock could
	// succeed locally and then decide not to do any callbacks, although
	// it should do callbacks if optimistic was false. When the caller
	// retries with infinite timeout, the callbacks will not be performed
	// again because an EX local lock will be held already ==> error

	W_DO(llm->query(name, prev_mode, xct()->tid(), TRUE));

        if (name.vid().is_remote()) {

	    if (name.lspace() == lockid_t::t_record) {
		if (m == EX && prev_mode != EX && !register_adaptive(name,TRUE))
		    return RC(eMAYBLOCK);

	    } else if (name.lspace() == lockid_t::t_page) {
		if ((m == EX && prev_mode != EX) ||
#ifdef OBJECT_CC
		  (m == SIX && prev_mode != SIX && prev_mode != EX) ||
		  (m==IX && (prev_mode==NL ||prev_mode==IS || prev_mode==SH)) ||
#endif
		  (m == UD && prev_mode != EX && prev_mode != UD))
		return RC(eMAYBLOCK);
	    } else {
		if (prev_mode != supr[m][prev_mode]) return  RC(eMAYBLOCK);
	    }

	    if (force)
		rc = llm->_lock(name, m, prev_mode, prev_pgmode, d, 0, TRUE);
	    else
		rc = llm->_lock(name, m, prev_mode, prev_pgmode, d, 0, FALSE);

	    if (rc) {
		if (rc.err_num() == eLOCKTIMEOUT)        return RC(eMAYBLOCK);
	        else                                    return rc.reset();
	    }

	    return RCOK;

	} else {
	    bool cm_mutex = FALSE;

	    if ((m == EX && prev_mode != EX) ||
		(name.lspace() == lockid_t::t_page && (m == SIX || m == IX) &&
		 (prev_mode == NL || prev_mode == IS || prev_mode == SH))) {

	        cm->mutex_acquire();
		cm_mutex = TRUE;

	        uint4 numsites;
	        cb_site_t sites[smlevel_0::max_servers];
	        cm->find_copies(name, sites, numsites, srvid_t::null);

	        if (numsites > 0) {
	            cm->mutex_release();
	            return RC(eMAYBLOCK);
	        }
	    }

	    if (force)
		rc = llm->_lock(name, m, prev_mode, prev_pgmode, d, 0, TRUE);
	    else
		rc = llm->_lock(name, m, prev_mode, prev_pgmode, d, 0, FALSE);

	    if (cm_mutex) cm->mutex_release();

#if defined(HIER_CB) && defined(MULTI_SERVER)
	    if (cc_alg == t_cc_record && name.lspace() >= lockid_t::t_page &&
		    (prev_pgmode==NL || prev_pgmode==IS || prev_pgmode==SH))
		reset_req_pending(*(lpid_t*)name.name(), FALSE);
#endif

	    if (rc) {
		if (rc.err_num() == eLOCKTIMEOUT)	return RC(eMAYBLOCK);
	        else					return rc.reset();
	    }
	    return RCOK;
	}
    }

    // optimistic is FALSE

    if (timeout != WAIT_FOREVER) { W_FATAL(eNOTIMPLEMENTED); }

    if (force) {
	W_DO(llm->_lock(name, m, prev_mode, prev_pgmode, d, timeout, TRUE));
    } else {
	W_DO(llm->_lock(name, m, prev_mode, prev_pgmode, d, timeout, FALSE));
    }

    if (name.lspace() == lockid_t::t_record) {

	if (name.vid().is_remote()) {
	    if (m == EX && prev_mode != EX && !register_adaptive(name))
		W_DO(rm->acquire_lock(name, m, d, timeout, force, old_value));
	} else {
	    if (m == EX && prev_mode != EX)
		W_DO(cbm->callback(name, EX, srvid_t::ME, prev_pgmode,timeout));
	}

    } else if (name.lspace() == lockid_t::t_page) {

	w_assert3(prev_mode == prev_pgmode);

	if (name.vid().is_remote()) {
	    if ((m == EX && prev_mode != EX)
#ifdef OBJECT_CC
		  || (m == SIX && prev_mode != SIX && prev_mode != EX)
		  || (m==IX && (prev_mode==NL ||prev_mode==IS || prev_mode==SH))
#endif
		  )
	        W_DO(rm->acquire_lock(name, m, d, timeout, force));

#ifdef HIER_CB
	    if ((cc_alg==t_cc_record) && (m==SH || m==SIX || m==EX) &&
			(prev_mode==NL || prev_mode==IS || prev_mode==IX)) {
		page_s* page;
		W_DO(bf->fix(page, *(lpid_t *)name.name(),
					page_p::t_file_p, LATCH_NL, FALSE));
		if (bf->is_partially_cached(page)) {
		    w_assert3(page->tag == page_p::t_file_p);
		    rc_t rc = bf->get_page(name, bf->get_cb(page),
			   page_p::t_file_p, FALSE, FALSE, FALSE, FALSE, FALSE);
		    bf->unfix(page);
		    if (rc) return rc.reset();
		} else {
		   bf->unfix(page);
		}
	    }
#endif
	} else {
	    if ((m == EX && prev_mode != EX)
#ifdef OBJECT_CC
		  ||(m==SIX &&(prev_mode==NL ||prev_mode==IS ||prev_mode==SH))||
		  (m==IX && (prev_mode==NL || prev_mode==IS || prev_mode==SH))
#endif
		  )
		W_DO(cbm->callback(name, m, srvid_t::ME,prev_mode,timeout));
	}

    } else {
	if (name.vid().is_remote()) {
	    if (prev_mode != supr[m][prev_mode]) {
		W_DO(rm->acquire_lock(name, m, d, timeout, force));
	    }
	} else {
	    if (m == EX && prev_mode != EX) {
		W_DO(cbm->callback(name, EX, srvid_t::ME, NL, timeout));
	    }
	}
    }

    return RCOK;
}


bool remote_lock_m::purged_flag(const lpid_t& pid)
{
    lock_head_t* lock = rlm->_core->find_lock(pid, FALSE);
    if (lock) {
       bool purged = lock->purged();
       MUTEX_RELEASE(lock->mutex);
       return purged;
    }
    return FALSE;
}

#if defined(OBJECT_CC) && defined (MULTI_SERVER)

void remote_lock_m::set_lock_pending(const lockid_t& name)
{
    lock_head_t* lock = llm->_core->find_lock(name, FALSE);
    w_assert3(lock && !lock->pending());
    lock->set_pending();
    MUTEX_RELEASE(lock->mutex);
}


void remote_lock_m::reset_lock_pending(const lockid_t& name)
{
    lock_head_t* lock = llm->_core->find_lock(name, FALSE);
    w_assert3(lock && lock->pending());
    lock->reset_pending();
    MUTEX_RELEASE(lock->mutex);
}


void remote_lock_m::set_req_pending(const lpid_t& pid)
{
    w_assert3(cc_alg == t_cc_record);
    lock_head_t* lock = llm->_core->find_lock(pid, FALSE);
    w_assert3(lock);
    lock_request_t* req = llm->_core->find_req(lock->queue, xct());
    w_assert3(req);
    w_assert3(!req->pending());
    req->set_pending();
    MUTEX_RELEASE(lock->mutex);
}


void remote_lock_m::reset_req_pending(const lpid_t& pid, bool assert_lock)
{
    w_assert3(cc_alg == t_cc_record);
    lock_head_t* lock = llm->_core->find_lock(pid, FALSE);
    w_assert3(!assert_lock || lock);
    if (lock) {
        lock_request_t* req = llm->_core->find_req(lock->queue, xct());
        w_assert3(req);
        w_assert3(req->pending());
        req->reset_pending();
        MUTEX_RELEASE(lock->mutex);
    }
}


bool remote_lock_m::is_req_pending(const lpid_t& pid)
{
    w_assert3(cc_alg == t_cc_record);
    lock_head_t* lock = llm->_core->find_lock(pid, FALSE);
    if (lock) {
        lock_request_t* req = llm->_core->find_req(lock->queue, xct());
        if (req) {
            MUTEX_RELEASE(lock->mutex);
            return req->pending();
	}
    }
    MUTEX_RELEASE(lock->mutex);
    return FALSE;
}


bool remote_lock_m::req_nonpending_lookup(const lpid_t& pid)
{
    w_assert3(cc_alg == t_cc_record);
    lock_head_t* lock = llm->_core->find_lock(pid, FALSE);
    w_assert3(lock);
    lock_request_t* req = 0;
    w_list_i<lock_request_t> iter(lock->queue);
    while (req = iter.next()) {
        if (req->status() == lock_m::t_waiting) break;
        if (req->mode == IX || req->mode == SIX || req->mode == EX) {
            if (!req->pending()) {
                MUTEX_RELEASE(lock->mutex);
                return TRUE;
            }
        } else {
            w_assert3(!req->pending());
        }
    }
    MUTEX_RELEASE(lock->mutex);
    return FALSE;
}


bool remote_lock_m::test_and_clear_cb_repeat(const lockid_t& name)
{
    w_assert3(cc_alg == t_cc_record && name.lspace() == lockid_t::t_record);
    lock_head_t* lock = llm->_core->find_lock(name, FALSE);
    w_assert3(lock && lock->granted_mode == EX);
    if (lock->repeat_cb()) {
	lock->reset_repeat_cb();
	return TRUE;
    }
    return FALSE;
}


void remote_lock_m::clear_cb_repeat(const lockid_t& name)
{
    w_assert3(cc_alg == t_cc_record && name.lspace() == lockid_t::t_record);
    lock_head_t* lock = llm->_core->find_lock(name, FALSE);
    if (lock)
	lock->reset_repeat_cb();
}


bool remote_lock_m::is_adaptive(const lpid_t& pid)
{
    if (!cc_adaptive) return FALSE;

    lock_head_t* lock = llm->_core->find_lock(pid, FALSE);
    if (lock && lock->adaptive()) {
	MUTEX_RELEASE(lock->mutex);
	return TRUE;
    }
    MUTEX_RELEASE(lock->mutex);
    return FALSE;
}


void remote_lock_m::set_adaptive(const lpid_t& pid)
{
    w_assert3(cc_adaptive);
    lock_head_t* lock = llm->_core->find_lock(pid, FALSE);
    w_assert3(lock && !lock->adaptive());
    lock->set_adaptive();
    MUTEX_RELEASE(lock->mutex);
}


bool remote_lock_m::register_adaptive(const lockid_t& n, bool optimistic)
{
    if (!cc_adaptive) return FALSE;

    lpid_t pid = n.pid();
    int slot = n.rid().slot;

    xct_lock_info_t* xlocks = xct()->lock_info();

    MUTEX_ACQUIRE(xlocks->mutex);

    adaptive_lock_t* adaptive_lock = xlocks->adaptive_locks->lookup(pid);

    if (adaptive_lock) {
	bm_set(adaptive_lock->recs, slot);
	MUTEX_RELEASE(xlocks->mutex);
        return TRUE;
    }

    if (!optimistic) {
        adaptive_lock = new adaptive_lock_t(pid);
        xlocks->adaptive_locks->push(adaptive_lock);
    }
    MUTEX_RELEASE(xlocks->mutex);
    return FALSE;
}


bool remote_lock_m::try_adaptive(const lpid_t& pid, const srvid_t& srv,
								bool check_only)
{
    w_assert3(cc_adaptive);

    lock_head_t* lock = llm->_core->find_lock(pid, FALSE);
    w_assert3(lock);
    w_assert3(lock->granted_mode == SIX || lock->granted_mode == IX);
    lock_request_t* req = 0;
    w_list_i<lock_request_t> iter(lock->queue);
    while (req = iter.next()) {
	if (req->status() == t_waiting) break;
	if (req->xd->is_master_proxy() ||
	    			req->xd->master_site()->server() != srv) {
	    w_assert3(!lock->adaptive());
	    MUTEX_RELEASE(lock->mutex);
	    return FALSE;
	}
    }
    if (!check_only) lock->set_adaptive();
    MUTEX_RELEASE(lock->mutex);
    return TRUE;
}


#endif /* OBJECT_CC */
#endif /* MULTI_SERVER */
#endif /* MULTI_SERVER */

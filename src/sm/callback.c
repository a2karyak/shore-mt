/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

/*
 *  $Id: callback.c,v 1.21 1996/06/27 17:22:53 kupsch Exp $
 */

#define SM_SOURCE
#define CALLBACK_C

#ifdef __GNUG__
#pragma implementation "callback.h"
#endif

#include <basics.h>

#ifdef MULTI_SERVER

#include <sm_int.h>
#include "srvid_t.h"
#include "lock_x.h"
#include "lock_core.h"
#include "remote_s.h"
#include "callback.h"
#include "remote_client.h"
#include "cache.h"
#include <auto_release.h>

#ifdef __GNUG__
template class w_list_t<adaptive_cb_t>;
template class w_list_i<adaptive_cb_t>;
template class w_hash_t<adaptive_cb_t, lpid_t>;
#endif

typedef callback_op_t cb_t;

#if defined(NOT_PREEMPTIVE) && defined(OBJECT_CC)
w_hash_t<adaptive_cb_t, lpid_t>* callback_m::adaptive_cb_tab = 0;
#endif

long callback_m::_cbcount = 0;


callback_m::callback_m()
{
#if defined(NOT_PREEMPTIVE) && defined(OBJECT_CC)
    if (cc_adaptive) {
        adaptive_cb_tab = new w_hash_t<adaptive_cb_t, lpid_t>(100,
					offsetof(adaptive_cb_t, pid),
					offsetof(adaptive_cb_t, link));
        if (!adaptive_cb_tab) W_FATAL(eOUTOFMEMORY);
    }
#endif
}


rc_t callback_m::callback(const lockid_t& name, lock_mode_t mode,
		const srvid_t& srv, lock_mode_t prev_pgmode,
		long timeout, bool* adaptive)
{
    xct_t* xd = xct();
    uint2 cb_outcome;
    lpid_t pid = (name.lspace() >=lockid_t::t_page ? name.pid() : lpid_t::null);

#ifdef OBJECT_CC
    if (adaptive) *adaptive = FALSE;
#endif

#ifdef DEBUG
    w_assert3(name.lspace() != lockid_t::t_record || cc_alg == t_cc_record);
    w_assert3(mode == EX || (cc_alg == t_cc_record &&
	name.lspace() == lockid_t::t_page && (mode == SIX || mode == IX)));

    lock_mode_t m = NL;
    W_COERCE(llm->query(name, m, xd->tid()));
    w_assert3(m == mode);
#endif

#ifdef HIER_CB
    // see comment 3.
    bool myreq_pending = FALSE;
    if (cc_alg == t_cc_record && name.lspace() >= lockid_t::t_page) {
	if (prev_pgmode==NL || prev_pgmode==IS || prev_pgmode==SH) {
	    myreq_pending = TRUE;
	    w_assert3(lm->is_req_pending(pid));
	}
    } else if (name.lspace() >= lockid_t::t_page) {
	w_assert3(!lm->is_req_pending(pid));
    }
#endif /* HIER_CB */

    uint4 numsites = 0;
    cb_site_t sites[smlevel_0::max_servers];
    callback_op_t* cbop = 0;

    cm->find_copies(name, sites, numsites, srv);

    if (numsites == 0) {
	TRACE(2, "no caching servers found" << flushl);
#ifdef HIER_CB
	if (myreq_pending)
	    // see comment 3.
	    lm->reset_req_pending(pid);
	else
	    w_assert3(name.lspace() < lockid_t::t_page ||
					!lm->is_req_pending(pid));
#endif /* HIER_CB */

#ifdef OBJECT_CC
	if (cc_adaptive && name.lspace() == lockid_t::t_record &&
			xd->is_remote_proxy() && lm->try_adaptive(pid, srv)) {
	    w_assert3(adaptive);
	    TRACE(31, "Adaptive lock acquired: pid: "<<pid <<" xct: "<<xd->tid());
	    *adaptive = TRUE;
	}
#endif /* OBJECT_CC */

	return RCOK;

    } else {

	TRACE(1, "Callback:" << " name: " << name << " mode: " << mode
	    << " req_srv: " << srv << " req_xct: " << xd->tid() << flushl <<
            "master_site: " << (xd->is_remote_proxy() ?
		xd->master_site()->server().id : 0) <<
            " master_xct: " << (xd->is_remote_proxy() ?
		xd->master_site()->master_tid() : xd->tid())<<
             " CB ID = " << _cbcount << flushl
        );

	if (timeout != WAIT_FOREVER) { W_FATAL(eNOTIMPLEMENTED); } // for now

	cbop = xd->callback();

#ifdef HIER_CB
	// see comment 3.
	cbop->myreq_pending = myreq_pending;
	cbop->page_dirty = (myreq_pending ?
				lm->req_nonpending_lookup(pid) : TRUE);

	w_assert3(cc_alg == t_cc_record || cbop->page_dirty == FALSE);

	if (cbop->page_dirty && mode != EX) {
	    if (myreq_pending)	lm->reset_req_pending(pid);
	    else		w_assert3(!lm->is_req_pending(pid));
	    return RCOK;
	}
#endif /* HIER_CB */

	for (int i = 0; i < numsites; i++) {
	    // See comment 1.
	    callback_t* cb = new callback_t(sites[i].srv, sites[i].races);
	    cbop->callbacks.push(cb);
	}

	cbop->cb_id = _cbcount++;
	cbop->mode = mode;
	cbop->num_no_replies = cbop->callbacks.num_members();
	cbop->name = name;

#ifdef HIER_CB
	if (mode != EX) {
	    cbop->name.lspace() = lockid_t::t_record;
	    cbop->name.rid().slot = max_recs - 1;
	}
#endif /* HIER_CB */

	smlevel_0::stats.callback_op_cnt++;

        rc_t rc = rm->send_callbacks(*cbop);
	if (rc) {
	    // lock_pending and cb_repeat cannot be set at this point, so no 
	    // need to reset them. If req_pending is set, it should remain so.
	    cbop->clear();
	    return rc.reset();
	}

        rc = rm->get_cb_replies(*cbop);
	if (rc) {
#ifdef OBJECT_CC
	    if (cbop->blocked && name.lspace() == lockid_t::t_record) {
		lm->reset_lock_pending(name);
		lm->clear_cb_repeat(name);
	    }
#endif
	    cbop->clear();
	    return rc.reset();
	}

#ifdef OBJECT_CC
	if (cbop->blocked && name.lspace() == lockid_t::t_record)
	    lm->reset_lock_pending(name);
#endif

#ifdef HIER_CB
	if (name.lspace() == lockid_t::t_record &&
				lm->test_and_clear_cb_repeat(name)) {
	    // see comment 8.
	    TRACE(27, "REPEAT CALLBACKS name: "<<name<<" CB ID= "<<cbop->cb_id);
	    w_assert3(cbop->blocked);

#ifdef DEBUG
            _check_lock(name, FALSE);
	    if (myreq_pending) w_assert3(lm->is_req_pending(pid));
#endif
	    cbop->clear();
	    W_DO(callback(name, mode, srv, prev_pgmode, timeout));

	} else {
	    if (name.lspace() == lockid_t::t_record && myreq_pending)
		lm->reset_req_pending(pid);

	    cb_outcome = cbop->outcome;
	    cbop->clear();
	}
#else
	cb_outcome = cbop->outcome;
	cbop->clear();
#endif /* HIER_CB */

#ifdef DEBUG
	w_assert3(cbop->cb_id == -1);
        _check_lock(name);
        if (cc_alg != t_cc_record && cbop->outcome == cb_t::PINV) {
            cm->find_copies(name, sites, numsites, srv);
            // NOTE: The following assertion is wrong in the case of record
            //       level locking, even in the case that cbop->outcome == PINV
            //       The reason is that some other site, which does not cache
            //       the page at the time the callback started, may try to
            //       read the page (but a different obj than the one being
            //       called back by xct()) and it may succeed to get the page
            //       before the callback finishes.
            w_assert3(numsites == 0);
        }
#endif

#ifdef OBJECT_CC
	if (cc_adaptive && name.lspace() == lockid_t::t_record &&
			xd->is_remote_proxy() && cb_outcome == cb_t::PINV &&
			lm->try_adaptive(pid, srv)) {
            w_assert3(adaptive);
	    TRACE(31, "Adaptive lock acquired: pid: "<<pid <<" xct: "<<xd->tid());
            *adaptive = TRUE;
        }
#endif /* OBJECT_CC */

        return RCOK;
    }
}


rc_t callback_m::handle_cb_reply(callback_op_t& cbop, callback_t*& cb, 
	cb_outcome_t outcome,  bool& done, 
	int numblockers, uint2 block_level, blocker_t* blockers)
{
    bool first_round_done = FALSE;
    if (cb && cb->outcome == cb_t::NONE) {	// first round reply
	w_assert3(cbop.num_no_replies > 0);
	cbop.num_no_replies--;
	if (cbop.num_no_replies == 0) first_round_done = TRUE;
    }

#ifdef DEBUG
    if (cc_alg == t_cc_page) {
        w_assert3(outcome != cb_t::BLOCKED || cb->outcome == cb_t::NONE);
        w_assert3(outcome != cb_t::DEADLOCK || cb->outcome == cb_t::NONE);
    }
    if (outcome == cb_t::KILLED) {
        w_assert3(cbop.outcome == cb_t::DEADLOCK);
        w_assert3(cb->outcome == cb_t::BLOCKED || cb->outcome == cb_t::NONE);
    }
    w_assert3(cbop.outcome <= cb_t::DEADLOCK);
#endif

#ifdef HIER_CB
    bool page_blocked = FALSE;
#endif

    switch (outcome) {

    case cb_t::PINV:
	smlevel_0::stats.PINV_cb_replies_cnt++;

	if (cb->read_races > 0 || cbop.name.lspace() <= lockid_t::t_store) {
	    // see comment 1.
	    TRACE(20, "WARNING WARNING WARNING: deleting " << cb->read_races <<
		" read races after cb for: " << cbop.name << " is done" << endl
	    );
	    cm->delete_read_race(cbop.name, cb->server, cb->read_races);
	}

#ifdef OBJECT_CC

	if (cbop.name.lspace() == lockid_t::t_record) {
#ifdef DEBUG
	    bool registered =  cm->query(cbop.name, cb->server);
	    if (registered) {
		// This may happen if two callbacks for the same page are sent
		// to the same client together and the replies arrive at the
		// server in the revesre order than the send order.
		TRACE(137, "WARNING WARNING WARNING: " << endl <<
			"page registered in copy table after PINV: cbid: " <<
			cbop.cb_id << " name: " << cbop.name);
	   }
#ifdef HIER_CB
	   w_assert3(cbop.myreq_pending|| !lm->is_req_pending(cbop.name.pid()));
#endif /* HIER_CB */
#endif /* DEBUG */

#ifdef HIER_CB
	    page_blocked = cb->page_blocked;
	    if (page_blocked)
		TRACE(21, "Page Blocker Done: cb_id: " << cbop.cb_id
						<< " cb srv: " << cb->server);
#endif /* HIER_CB */

	    delete cb;
	    cb = 0;

	} else {
	    delete cb;
	    cb = 0;
	}
#else
	delete cb;
	cb = 0;
#endif /* OBJECT_CC */

	if (cbop.outcome != cb_t::DEADLOCK && !cbop.local_deadlock) {
	    if (cbop.outcome == cb_t::NONE) cbop.outcome = callback_op_t::PINV;

#ifdef HIER_CB
	    if (page_blocked) {
		rc_t rc = _page_blocker_done(cbop);
		if (rc) {
		    if (rc.err_num() == eDEADLOCK) {
			w_assert3(xct()->lock_info()->wait == 0);
		        done = TRUE;
		        TRACE(23, "CBOP DONE: DEADLOCK: cb_id: " << cbop.cb_id);
		    }
		    return rc.reset();
		}
	    }
#endif /* HIER_CB */

	    if (cbop.callbacks.num_members() == 0) {
		w_assert3(!cbop.local_deadlock);
		w_assert3(cbop.num_no_replies == 0);
		w_assert3(xct()->lock_info()->wait == 0);
		done = TRUE;
		TRACE(22, "CBOP DONE: SUCESS: cb_id: " << cbop.cb_id);
		return RCOK;
	    }
	} else {
	    if (cbop.callbacks.num_members() == 0) {
		w_assert3(cbop.num_no_replies == 0);
		w_assert3(xct()->lock_info()->wait == 0);
		done = TRUE;
		TRACE(23, "CBOP DONE: DEADLOCK: cb_id: " << cbop.cb_id);
		return RC(eDEADLOCK);
	    }
	}
	break;

#ifdef OBJECT_CC

    case cb_t::OINV:
	smlevel_0::stats.OINV_cb_reply_cnt++;

	w_assert3(cbop.name.lspace() == lockid_t::t_record);

#ifdef HIER_CB
	w_assert3(cbop.myreq_pending || !lm->is_req_pending(cbop.name.pid()));

	page_blocked = cb->page_blocked;
	if (page_blocked)
	    TRACE(21, "Page Blocker Done: cb_id: " << cbop.cb_id
						<< " cb srv: " << cb->server);
#endif /* HIER_CB */

	delete cb;
	cb = 0;

        if (cbop.outcome != cb_t::DEADLOCK && !cbop.local_deadlock) {
            cbop.outcome = callback_op_t::OINV;

#ifdef HIER_CB
	    if (page_blocked) {
		rc_t rc = _page_blocker_done(cbop);
		if (rc) {
		    if (rc.err_num() == eDEADLOCK) {
			w_assert3(xct()->lock_info()->wait == 0);
			done = TRUE;
			TRACE(23, "CBOP DONE: DEADLOCK: cb_id: " << cbop.cb_id);
		    }
		    return rc.reset();
		}
	    }
#endif /* HIER_CB */

            if (cbop.callbacks.num_members() == 0) {
		w_assert3(!cbop.local_deadlock);
                w_assert3(cbop.num_no_replies == 0);
		w_assert3(xct()->lock_info()->wait == 0);
                done = TRUE;
		TRACE(22, "CBOP DONE: SUCESS: cb_id: " << cbop.cb_id);
                return RCOK;
            }
        } else {
            if (cbop.callbacks.num_members() == 0) {
                w_assert3(cbop.num_no_replies == 0);
                w_assert3(xct()->lock_info()->wait == 0);
                done = TRUE;
		TRACE(23, "CBOP DONE: DEADLOCK: cb_id: " << cbop.cb_id);
                return RC(eDEADLOCK);
            }
        }
        break;
#endif /* OBJECT_CC */

    case cb_t::BLOCKED:
	smlevel_0::stats.BLOCKED_cb_reply_cnt++;

	uint2 level = cbop.name.lspace();

#ifdef DEBUG
	w_assert3(numblockers > 0);
	lock_mode_t block_mode = (lock_mode_t) blockers[0].mode;

	if (level != block_level) {
	    w_assert3(cc_alg == t_cc_record);
	    w_assert3(level == lockid_t::t_record);
	    w_assert3(block_level == lockid_t::t_page && block_mode == SH);
#ifdef HIER_CB
	    w_assert3(!cbop.page_dirty);
#endif /* HIER_CB */
	}
	if (cbop.mode == IX || cbop.mode == SIX) {
	    w_assert3(cc_alg == t_cc_record);
	    w_assert3(block_level != level);
	    w_assert3(block_mode == SH);
	} else {
	    w_assert3(cbop.mode == EX);
	}
	w_assert3( (level <= lockid_t::t_store && block_mode == IS) ||
		   (level == lockid_t::t_record && block_mode == SH) ||
		   (level == lockid_t::t_page && (block_mode == SH ||
						  block_mode == IS)) );
	if (level == lockid_t::t_page && cc_alg == t_cc_page) {
		w_assert3(block_mode == SH);
	}
#endif /* DEBUG */

#ifdef OBJECT_CC
	if (level == lockid_t::t_record && !cbop.blocked &&
				cbop.name.rid().slot != max_recs-1)
	    lm->set_lock_pending(cbop.name);
#endif
	bool was_blocked = cbop.blocked;

	cb->outcome = callback_op_t::BLOCKED;
	cbop.blocked = TRUE;

#ifdef HIER_CB
	if (cbop.outcome != cb_t::DEADLOCK && !cbop.local_deadlock) {
	    if (cb->page_blocked) {
	        w_assert3(level == lockid_t::t_record && block_level == level);
	        rc_t rc = _page_blocker_done(cbop);
		if (rc) {
		    w_assert3(rc.err_num() != eDEADLOCK);
		    TRACE(23, "CBOP DONE: DEADLOCK: cb_id: " << cbop.cb_id);
		    done = TRUE;
		    return rc.reset();
		}
	        cb->page_blocked = FALSE;
	    }
	}
#endif /* HIER_CB */

	if (cbop.outcome != cb_t::DEADLOCK && !cbop.local_deadlock) {

	    for (int i = 0; i < numblockers; i++) {
	        xct_i xct_iter;
	        xct_t* xd;
	        while(	(xd = xct_iter.next()) &&
			(!xd->is_remote_proxy() ||
			 xd->master_site()->server() != cb->server ||
			 xd->master_site()->master_tid() != blockers[i].tid)
		);

	        if (xd && xd->state() == xct_active) {

#ifdef HIER_CB
		    if (block_level != level && cb->page_blocked == FALSE) {
			cb->page_blocked = TRUE;
			cbop.page_blockers++;
		    }
#endif /* HIER_CB */

	            if (_block_cb(xd, cbop, block_level,
					(lock_mode_t) blockers[i].mode))
			break;
		} else {
		    TRACE(24, "cb blocker not found or not active: cb_id: " <<
			cbop.cb_id <<" blocker master tid: " <<blockers[i].tid);
		}

		if (!was_blocked) smlevel_0::stats.cb_conflict_cnt++;
    	    }
	}
	delete[] blockers;

	break;

    case cb_t::DEADLOCK:
	smlevel_0::stats.DEADLOCK_cb_reply_cnt++;

	delete cb;
	cb = 0;
	if (cbop.outcome != cb_t::DEADLOCK && !cbop.local_deadlock) {
	    cbop.outcome = callback_op_t::DEADLOCK;

	    TRACE(25,"DEADLOCK DEADLOCK! deadlock cb reply:cbid: "<<cbop.cb_id);

	    if (xct()->lock_info()->wait) {
		w_assert3(xct()->lock_info()->wait->status() == lock_m::t_converting);
		xct()->lock_info()->wait->status(lock_m::t_aborted);
		xct()->lock_info()->wait = 0;
	    }

	    if (cbop.callbacks.num_members() == 0) {
	        w_assert3(cbop.num_no_replies == 0);
	        done = TRUE;
		TRACE(23, "CBOP DONE: DEADLOCK: cb_id: " << cbop.cb_id);
	        return RC(eDEADLOCK);
	    } else {
	        W_DO(rm->send_callback_aborts(cbop));
	    }
	} else {
	    if (cbop.callbacks.num_members() == 0) {
		w_assert3(cbop.num_no_replies == 0);
		done = TRUE;
		TRACE(23, "CBOP DONE: DEADLOCK: cb_id: " << cbop.cb_id);
		return RC(eDEADLOCK);
	    }
	}
	break;

    case cb_t::KILLED:
	smlevel_0::stats.KILLED_cb_reply_cnt++;

	delete cb;
	cb = 0;
	w_assert3(cbop.outcome == cb_t::DEADLOCK);
	if (cbop.callbacks.num_members() == 0) {
	    w_assert3(cbop.num_no_replies == 0);
	    w_assert3(xct()->lock_info()->wait == 0);
	    done = TRUE;
	    TRACE(23, "CBOP DONE: DEADLOCK: cb_id: " << cbop.cb_id);
	    return RC(eDEADLOCK);
	}
	break;

    case cb_t::LOCAL_DEADLOCK:
	smlevel_0::stats.LOCAL_DEADLOCK_cb_reply_cnt++;

	w_assert3(cbop.callbacks.num_members() > 0);

	if (cbop.outcome != cb_t::DEADLOCK) {
	    cbop.outcome = callback_op_t::DEADLOCK;
	    w_assert3(xct()->lock_info()->wait == 0);
	    W_DO(rm->send_callback_aborts(cbop));
	}
	break;

    default:
	W_FATAL(eBADCBREPLY);
    }
    return RCOK;
}


#ifdef HIER_CB

rc_t callback_m::_page_blocker_done(callback_op_t& cbop)
{
    w_assert3(cc_alg == t_cc_record);
    w_assert3(cbop.name.lspace() == lockid_t::t_record);
    w_assert3(cbop.page_blockers > 0);
    cbop.page_blockers--;
    if (cbop.page_blockers == 0) {
        rc_t rc = _re_acquire_rec_lock(cbop.name);

        if (rc) {
            cbop.outcome = callback_op_t::DEADLOCK;
	    w_assert3(rc.err_num() == eDEADLOCK);
            w_assert3(xct()->lock_info()->wait == 0);
            w_assert3(xct()->lock_info()->cycle == 0);
	    if (cbop.callbacks.num_members() == 0) {
		// This can be the case if another xact (not one that has 
		// received a cb req from xct()) managed to read the obj 
		// while xct()'s EX lock on the obj was downgraded to an SH mode
		return rc.reset();
	    } else {
                W_DO(rm->send_callback_aborts(cbop));
	    }
        }
    }
    return RCOK;
}


rc_t callback_m::_re_acquire_rec_lock(const lockid_t& name)
{
    lock_head_t* plock = llm->_core->find_lock(*(lpid_t*)name.name(), FALSE);
    w_assert3(plock);

    lock_request_t* preq = llm->_core->find_req(plock->queue, xct());
    w_assert3(preq);

    if (preq->status() == lock_m::t_granted) {
        w_assert3(preq->mode == IX || preq->mode == SIX);
	w_assert3(xct()->lock_info()->wait == 0);
        MUTEX_RELEASE(plock->mutex);
    } else {
	// comment 7
	w_assert3(preq->status() == lock_m::t_converting);
	w_assert3(!lock_m::compat[preq->convert_mode][plock->granted_mode]);
	w_assert3(plock->waiting);
        w_assert3(xct()->lock_info()->wait == preq);

	preq->thread = me();
	MUTEX_RELEASE(plock->mutex);
	W_COERCE(me()->block());
	MUTEX_ACQUIRE(plock->mutex);

	w_assert3(xct()->lock_info()->wait == 0);
        rc_t rc = RCOK;

	if (preq->status() == lock_m::t_granted) {
	    ;
	} else if (preq->status() == lock_m::t_aborted) {
	    rc = RC(eDEADLOCK);
	} else {
	    W_FATAL(eINTERNAL);
	}

	llm->_core->wakeup_waiters(plock);
	w_assert3(plock);
	MUTEX_RELEASE(plock->mutex);

	if (rc) return rc.reset();
    }

    if (name.rid().slot != max_recs-1) {
        MUTEX_ACQUIRE(xct()->lock_info()->mutex);
        rc_t rc;
        do {
	    lock_mode_t prev_mode;
	    lock_mode_t mode;
            rc = llm->_core->acquire(xct(), name, 0, 0, EX, prev_mode, t_long,
							WAIT_FOREVER, mode);
	    w_assert3(prev_mode == SH);
	    if (rc && rc.err_num() != ePREEMPTED) {
	        MUTEX_RELEASE(xct()->lock_info()->mutex);
	        return rc.reset();
	    }
        } while (rc);
        MUTEX_RELEASE(xct()->lock_info()->mutex);
    }
    return RCOK;
}

#endif /* HIER_CB */


rc_t callback_m::_block_cb(xct_t* xd, callback_op_t& cbop,
				uint2 block_level, lock_mode_t block_mode)
{
    lockid_t path[lockid_t::NUMLEVELS];
    const lockid_t& name = cbop.name;
    uint2 level = name.lspace();
    int i;
    if (level <= lockid_t::t_page) {
        path[level] = name;
        i = level;
    } else {
	path[level-1] = name;
	i = level-1;
    }

    while (i > 0 && llm->get_parent(path[i], path[i-1])) i--;

    MUTEX_ACQUIRE(xct()->lock_info()->mutex);
    if (level <= lockid_t::t_page) {
	llm->_core->_update_cache(xct(), name, NL);
    } else if (block_level != level) {
	llm->_core->_update_cache(xct(), path[level-1], NL);
    }
    MUTEX_RELEASE(xct()->lock_info()->mutex);

#ifndef NOT_PREEMPTIVE
    W_COERCE(xd->lock_info()->mutex.acquire());
    auto_release_t<smutex_t> dummy(xd->lock_info()->mutex);
#endif

    if (block_level > lockid_t::t_vol) {
	bool blocker_aborted;
        _block_cb_upper_levels(xd, block_level, path, blocker_aborted);
	if (blocker_aborted) return RCOK;
    }

#ifdef HIER_CB
    if (block_level == level) {
	W_DO(_block_cb_at_cb_level(xd, cbop, block_mode));
    } else {
	w_assert3(block_mode == SH);
	W_DO(_block_rec_cb_at_page_level(xd, cbop));
    }
#else
    W_DO(_block_cb_at_cb_level(xd, cbop, block_mode));
#endif /* HIER_CB */

    return RCOK;
}


void callback_m::_block_cb_upper_levels(xct_t* xd, uint2 level, lockid_t* path,
						bool& blocker_aborted)
{
    blocker_aborted = FALSE;
    lock_head_t* lock = 0;
    lock_request_t* req = 0;

    if (level == lockid_t::t_record) level--;

    for (int i = 0; i < level; i++) {
        lock = llm->_core->find_lock(path[i], FALSE);
        w_assert3(lock);
        w_assert3(lock->granted_mode != EX && lock->granted_mode != NL);
        req = llm->_core->find_req(lock->queue, xd);

        if (req) {
            if (req->status() == lock_m::t_granted ||
                                req->status() == lock_m::t_converting) {
#ifdef HIER_CB
                w_assert3(req->mode == IS || req->mode == IX ||
				// see comment 10.
                          	(i == lockid_t::t_page && req->mode == SH));
#else
		w_assert3(req->mode == IS || req->mode == IX);
#endif

            } else if (req->status() == lock_m::t_waiting) {
#ifdef HIER_CB
		w_assert3(req->mode > IS || 
				// see comment 11.
				(i == lockid_t::t_page && req->mode == IS));
#else
		w_assert3(req->mode == IS || req->mode == IX);
#endif
		req->rlink.detach();
		lock->queue.push(req);

		if (req->mode > IS) {
                    req->convert_mode = req->mode;
		    req->status(lock_m::t_converting);
		    req->mode = IS;
		} else {
		    req->status(lock_m::t_granted);
		    xd->lock_info()->wait = 0;
		    W_COERCE(req->thread->unblock());
		}
            } else {
		w_assert3(req->status() == lock_m::t_aborted);
		blocker_aborted = TRUE;
	    }

        } else {
            req = new lock_request_t(xd, IS, t_long, FALSE);
            // TODO: carry the duration of the lock from the client
            req->status(lock_m::t_granted);
            lock->queue.push(req);
	    smlevel_0::stats.lock_acquire_cnt++;
        }

        MUTEX_RELEASE(lock->mutex);
    }
}


#ifdef HIER_CB

rc_t callback_m::_block_rec_cb_at_page_level(xct_t* xd, callback_op_t& cbop)
{
    lock_head_t* lock = 0;
    lock_request_t* my_req = 0;
    lock_request_t* req = 0;
    lock_request_t* creq = 0;
    bool lookup_deadlocks = FALSE;
    bool deadlock_exists = FALSE;

    lock = llm->_core->find_lock(cbop.name.pid(), FALSE);
    w_assert3(lock);
#ifndef NOT_PREEMPTIVE
    auto_release_t<smutex_t> dummy1(lock->mutex);
#endif

    w_list_i<lock_request_t> iter(lock->queue);

    while (req = iter.next()) {

	if (req->xd == xd) {
	    creq = req;
	    continue;
	}
        if (req->xd == xct()) my_req = req;

	if (req->status() == lock_m::t_waiting) break;

        switch (req->status()) {
        case lock_m::t_granted:
#ifdef DEBUG
	    w_assert3(req->mode != EX);
	    if (req->pending()) w_assert3(req->mode == IX || req->mode == SIX);
	    if (req->mode == IX || req->mode == SIX) w_assert3(req->pending());
#endif
	    if (req->mode == IX || req->mode == SIX) {
	        req->status(lock_m::t_converting);
	        req->convert_mode = req->mode;
	        req->mode = (req->mode == SIX ? SH : IS);

	        lock->waiting = TRUE;

	        bool calling_back = TRUE;
	        _block_cb_handle_obj_lock(req, calling_back);

	        if (calling_back) req->xd->lock_info()->wait = req;
	        w_assert3(req->thread == 0);
	    }
	    break;

        case lock_m::t_converting:
#ifdef DEBUG
	    w_assert3(lock->waiting == TRUE);
	    w_assert3(req->mode != EX);
	    w_assert3(req->mode == IS || req->mode == SH);
	    //if (req->pending()) w_assert3(req->mode == IS || req->mode == SH);
	    //if (req->mode == IX || req->mode == EX) w_assert3(req->pending());
#endif
	    break;

	case lock_m::t_aborted:
	    w_assert3(req->xd != xd);
	    w_assert3(req->mode == IS);
	    break;

        default:
	    W_FATAL(eINTERNAL);
	    break;
        }
    }

    w_assert3(my_req && my_req->status() == lock_m::t_converting);

    if (!creq && req) creq = llm->_core->find_req(lock->queue, xd);

    if (creq) {
	switch (creq->status()) {
	case lock_m::t_granted:
	    // An SIX mode for xd is possible: see comment 2
	    w_assert3(creq->mode==SH || creq->mode==IS || creq->mode==SIX);
	    if (creq->mode == IS) {
		creq->mode = SH;
		lock->granted_mode = SH;
		lookup_deadlocks = TRUE;
	    }

	    w_assert3(lock->granted_mode == lock->granted_mode_other(0));
	    break;

	case lock_m::t_converting:
	case lock_m::t_waiting:
	    // see comment 4
	    w_assert3(creq->status() == lock_m::t_waiting ||
			creq->mode==SH || creq->mode==IS || creq->mode==SIX);

	    lock_mode_t wait_mode = (creq->status() == lock_m::t_converting) ?
				     creq->convert_mode : creq->mode;
	    w_assert3(wait_mode != IS && wait_mode != IX);

	    /*
	     * Grant an SH page lock to xd.
	     */
	    creq->mode = (creq->status() == lock_m::t_waiting) ? 
					SH : lock_m::supr[SH][creq->mode];
	    if (creq->status() == lock_m::t_waiting) {
                creq->convert_mode = wait_mode;
                creq->rlink.detach();
                lock->queue.push(creq);
            }
	    lock->granted_mode = lock_m::supr[SH][creq->mode];
	    w_assert3(lock->granted_mode_other(0) == lock->granted_mode);

	    /*
	     * Check the lock mode that xd has been waiting for and grant
	     * that lock if possible.
	     */
	    switch (wait_mode) {
	    case SH:
	        creq->status(lock_m::t_granted);
	        xd->lock_info()->wait = 0;
	        W_COERCE(creq->thread->unblock());

	        // No deadlock should exist in this case because xd has just
		// been granted its pending lock req, so it cannot be waiting
		// for any other lock.
		break;
	    case EX:
		// there is a real deadlock between xct() and xd in this case,
		// and this deadlock should be detected.
		creq->status(lock_m::t_converting);
		lookup_deadlocks = TRUE;
		deadlock_exists = TRUE;
		break;
	    case SIX:
		lock_mode_t mode_other = lock->granted_mode_other(creq);
		w_assert3(lock->granted_mode == lock_m::supr[mode_other][creq->mode]);
		if (lock_m::compat[SIX][mode_other]) {
		    lock->granted_mode = SIX;
		    creq->status(lock_m::t_granted);
		    creq->mode = SIX;
		    xd->lock_info()->wait = 0;
		    W_COERCE(creq->thread->unblock());
		} else {
		    creq->status(lock_m::t_converting);
		    lookup_deadlocks = TRUE;
		}
		break;
	    default:
		W_FATAL(eINTERNAL);
	    }

	    break;

	case lock_m::t_aborted:
	    lock->granted_mode = my_req->convert_mode;
	    w_assert3(lock->granted_mode == lock->granted_mode_other(0));
	    break;

	default:
	    W_FATAL(eINTERNAL);
	    break;
	}

    } else {
	creq = new lock_request_t(xd, SH, t_long, FALSE);
	creq->status(lock_m::t_granted);
	lock->queue.push(creq);
	lock->granted_mode = SH;
	w_assert3(lock->granted_mode_other(0) == SH);
	lookup_deadlocks = TRUE;
    }

    llm->_core->wakeup_waiters(lock);
    w_assert3(lock);

#ifndef DEBUG
    if (lookup_deadlocks) {
#endif
	bool self_abort = FALSE;
	bool converter_not_aborted = FALSE;

	iter.reset(lock->queue);

        while(req = iter.next()) {
	    if (req->status() == lock_m::t_granted) continue;
	    if (req->status() == lock_m::t_aborted) continue;

	    if (req->status() == lock_m::t_waiting && converter_not_aborted)
		break;

	    bool deadlock_found;
            rc_t rc = llm->_core->_check_deadlock(req->xd, &deadlock_found);

	    if (!lookup_deadlocks) w_assert3(!rc && !deadlock_found);
	    if (deadlock_exists) w_assert3(deadlock_found);

	    if (req->status() == lock_m::t_waiting && !deadlock_found) break;

	    if (!rc) {
		if (req->status() == lock_m::t_converting)
		    converter_not_aborted = TRUE;

	    } else {
		w_assert3(req->status() == lock_m::t_aborted);
		w_assert3(req->xd->lock_info()->wait == 0);

		if (req->xd == xct()) {
		    cbop.outcome = callback_op_t::DEADLOCK;
		    w_assert3(xct()->lock_info()->cycle == 0);
		    W_DO(rm->send_callback_aborts(cbop));
		    self_abort = TRUE;

	        } else if (req->thread == 0) {
		    w_assert3(req->pending());
		    callback_op_t* cbop = req->xd->callback();
		    w_assert3(cbop->cb_id >= 0);
		    w_assert3(cbop->blocked);
		    w_assert3(cbop->name == req->get_lock_head()->name);
		    cbop->local_deadlock = TRUE;

		    W_DO(rm->send_deadlock_notification(cbop, xct()->tid(),
								req->xd->tid()));
	        } else {
		    W_COERCE(req->thread->unblock());
	        }
	    }
        }

	if (self_abort) return RC(eDEADLOCK);
#ifndef DEBUG
    }
#endif

    return RCOK;
}


void callback_m::_block_cb_handle_obj_lock(lock_request_t* req, 
							bool& calling_back)
{
    lock_head_t* obj_lock = 0;
    lock_request_t* obj_req = 0;

    callback_op_t* cbop = req->xd->callback();
    lockid_t name = cbop->name;

    if (name.lspace() != lockid_t::t_bad && name.lspace() != lockid_t::t_record)
	return;

    if (name.lspace() == lockid_t::t_bad) {
	w_assert3(cbop->cb_id == -1);

	calling_back = FALSE;

	obj_req = req->xd->lock_info()->wait;

	if (!obj_req) {
	    // This is possible if req->xd was victimized during deadlock
	    // detection while xct() was waiting for the cb reply and req->xd
	    // has not aborted yet at the time the cb reply arrives.
	    return;
	}

	obj_lock = obj_req->get_lock_head();
	w_assert3(obj_lock);

	name = obj_lock->name;

	w_assert3(name.lspace() == lockid_t::t_record);
	w_assert3(*(lpid_t*)name.name() == *(lpid_t*)req->get_lock_head()->name.name());
    } else {
	calling_back = TRUE;
    }

    obj_lock = llm->_core->find_lock(name, FALSE);
    w_assert3(obj_lock);
#ifndef NOT_PREEMPTIVE
    auto_release_t<smutex_t> dummy2(obj_lock->mutex);
#endif

    obj_req = llm->_core->find_req(obj_lock->queue, req->xd);
    w_assert3(obj_req);

    if (cbop->name.lspace() == lockid_t::t_bad) {
	w_assert3(obj_req->status() == lock_m::t_waiting ||
		  obj_req->status() == lock_m::t_converting);
	w_assert3(obj_req->mode == EX || obj_req->convert_mode == EX);

	llm->_core->_update_cache(req->xd, req->get_lock_head()->name, NL);
	req->status(lock_m::t_granted);
	req->reset_pending();
	req->xd->lock_info()->wait = 0;

        W_COERCE(obj_req->thread->unblock(RC(ePREEMPTED)));

    } else {
	w_assert3(cbop->cb_id >= 0);

        if (obj_req->mode == EX) {
            w_assert3(obj_req->status() == lock_m::t_granted);
            obj_req->mode = SH;
            obj_lock->granted_mode = SH;
            if (obj_lock->waiting)
                llm->_core->wakeup_waiters(obj_lock);
        } else {
            w_assert3(obj_req->mode == SH);
            w_assert3(obj_lock->granted_mode == SH);
            if (obj_req->status() == lock_m::t_converting) {
                obj_req->status(lock_m::t_granted);
                llm->_core->wakeup_waiters(obj_lock);
            }
        }
    }
}

#endif /* HIER_CB */


rc_t callback_m::_block_cb_at_cb_level(xct_t* xd, callback_op_t& cbop, 
							lock_mode_t block_mode)
{
    const lockid_t& name = cbop.name;
    lock_head_t* lock = 0;
    lock_request_t* req = 0;
    lock_request_t* my_req = 0;
    bool lookup_deadlocks = FALSE;

    lock = llm->_core->find_lock(name, FALSE);
    w_assert3(lock);
#ifndef NOT_PREEMPTIVE
    auto_release_t<smutex_t> dummy1(lock->mutex);
#endif

    my_req = llm->_core->find_req(lock->queue, xct());
    w_assert3(my_req);

    lock_mode_t mymode = (name.lspace() >= lockid_t::t_page ? SH : SIX);

#ifdef HIER_CB
    if (cc_alg == t_cc_record && name.lspace() == lockid_t::t_page &&
							!cbop.myreq_pending) {
	w_assert3(block_mode == IS);
	mymode = SIX;
    }
#endif /* HIER_CB */

    switch (my_req->status()) {
    case lock_m::t_granted:
	if (my_req->mode == EX) {
            w_assert3(lock->granted_mode == EX);
            my_req->status(lock_m::t_converting);
            my_req->convert_mode = EX;
            my_req->mode = mymode;

            lock->granted_mode = mymode;
            lock->waiting = TRUE;

            xct()->lock_info()->wait = my_req;
            w_assert3(my_req->thread == 0);
	} else {
#ifdef HIER_CB
	    // see scenario 1.1 at the end of this file.
	    w_assert3(cc_alg==t_cc_record && name.lspace()==lockid_t::t_record);
	    w_assert3(cbop.page_blockers > 0);
	    w_assert3(my_req->mode == SH);
#else
	    W_FATAL(eINTERNAL);
#endif /* HIER_CB */
	}
        break;

    case lock_m::t_converting:
        w_assert3(my_req->convert_mode == EX);
        w_assert3(lock->granted_mode == mymode && lock->waiting);
	w_assert3(xct()->lock_info()->wait == my_req);
	break;

    default:
	W_FATAL(eINTERNAL);
	break;
    }

    req = llm->_core->find_req(lock->queue, xd);

    w_assert3(lock_m::compat[lock->granted_mode_other(0)][block_mode]);

    if (req) {
	switch (req->status()) {

	case lock_m::t_waiting:
            // xd can be waiting for any mode. The reason is that xd may send 
	    // any lock req right after the xct() cb gets blocked at the client,
	    // and that lock req can reach the server before the cb reply.

	    if (lock_m::supr[req->mode][block_mode] == block_mode) {
		req->status(lock_m::t_granted);
	        req->mode = block_mode;
                req->rlink.detach();
		// keep my_req at the head of the lock queue to make some
		// assertions elsewhere easier.
                req->rlink.attach(&my_req->rlink);
                xd->lock_info()->wait = 0;
                W_COERCE(req->thread->unblock());

	    } else {
		req->status(lock_m::t_converting);
		req->convert_mode = lock_m::supr[req->mode][block_mode];
		req->mode = block_mode;
		req->rlink.detach();
		// keep my_req at the head of the lock queue to make some
		// assertions elsewhere easier.
		req->rlink.attach(&my_req->rlink);

		// Although this is a real deadlock situation, the deadlock may
		// be undetectable at this point because xct() may be trying to
		// callback a record and is now waiting at the page level due
		// to page lockers.
		lookup_deadlocks = TRUE;
	    }

	    break;

#ifdef HIER_CB
	case lock_m::t_granted:
	case lock_m::t_converting:
	    // for the granted case see scenario 1.1 at the end of this file.
	    // for the converting case see scenario 1.2 at the end of this file.
	    w_assert3(cc_alg==t_cc_record && name.lspace()==lockid_t::t_record);
	    w_assert3(my_req->mode == SH && req->mode == SH);
	    break;
#endif /* HIER_CB */

	case lock_m::t_aborted:
	    break;

	default:
	    W_FATAL(eINTERNAL);
	    break;
	}

    } else {
	w_assert3(lock_m::compat[block_mode][lock->granted_mode]);

        req = new lock_request_t(xd, block_mode, t_long, FALSE);
        req->status(lock_m::t_granted);
        // keep my_req at the head of the lock queue to make some
	// assertions elsewhere easier.
	req->rlink.attach(&my_req->rlink);
	smlevel_0::stats.lock_acquire_cnt++;

        if (my_req->status() == lock_m::t_converting) lookup_deadlocks = TRUE;
    }

#ifndef DEBUG
    if (lookup_deadlocks) {
#endif
	bool deadlock_found;
	rc_t rc = llm->_core->_check_deadlock(xct(), &deadlock_found);

	if (!lookup_deadlocks) w_assert3(!rc && !deadlock_found);

	if (rc) {
	    cbop.outcome = callback_op_t::DEADLOCK;
            W_DO(rm->send_callback_aborts(cbop));

	    _check_deadlock(lock);

	    return RC(eDEADLOCK);
	}
#ifndef DEBUG
    }
#endif

    return RCOK;
}


rc_t callback_m::_check_deadlock(lock_head_t* lock)
{
    bool found;
    lock_request_t* req = 0;
    w_list_i<lock_request_t> iter(lock->queue);

    while (req = iter.next()) {
        if (req->status() == lock_m::t_granted) continue;
        if (req->status() == lock_m::t_aborted) continue;

        rc_t rc = llm->_core->_check_deadlock(req->xd, &found);

        // if one of the waiting or converting xacts in the queue is not
        // in a cycle then none of the rest xacts can be in a cycle.
        if (!found) break;

        // If req->xd was the xact chosen as victim...
        if (rc) {
	    w_assert3(req->status() == lock_m::t_aborted);
	    w_assert3(req->xd->lock_info()->wait == 0);

	    if (req->thread == 0) {
		callback_op_t* cbop = req->xd->callback();
		w_assert3(cbop->cb_id > 0);
		w_assert3(cbop->name == lock->name);
		cbop->local_deadlock = TRUE;

		W_DO(rm->send_deadlock_notification(cbop, xct()->tid(),
								req->xd->tid()));
	    } else {
                W_COERCE(req->thread->unblock());
	    }
        }
    }
    return RCOK;
}


void callback_m::_check_lock(const lockid_t& name, bool check_req_pending)
{
    MUTEX_ACQUIRE(xct()->lock_info()->mutex);

#ifdef OBJECT_CC
    if (name.lspace() == lockid_t::t_record) {
	lock_head_t* plock = llm->_core->find_lock(*(lpid_t*)name.name(),FALSE);
        w_assert3(plock);

        lock_request_t* preq = llm->_core->find_req(plock->queue, xct());

        w_assert3(preq);
        w_assert3(preq->status() == lock_m::t_granted);
        w_assert3(preq->mode == IX || preq->mode == SIX);
        w_assert3(plock->granted_mode == preq->mode);
#ifndef HIER_CB
	w_assert3(!preq->pending());
#else
	if (check_req_pending) w_assert3(!preq->pending());
#endif
        MUTEX_RELEASE(plock->mutex);
    }
#endif

    lock_head_t* lock = llm->_core->find_lock(name, FALSE);
    w_assert3(lock);
    w_assert3(!lock->pending());

    lock_request_t* req = llm->_core->find_req(lock->queue, xct());

    w_assert3(req);
    w_assert3(req->status() == lock_m::t_granted && req->mode == EX);
    w_assert3(lock->granted_mode == req->mode);
    w_assert3(lock->granted_mode_other(req) == NL);
    w_assert3(!lock->pending());

    MUTEX_RELEASE(lock->mutex);
    MUTEX_RELEASE(xct()->lock_info()->mutex);
}


void callback_m::handle_cb_req(callback_xct_proxy_t* cbxct)
{
    lockid_t name = cbxct->name();

    TRACE(3, "handling cb req: id: " << cbxct->cb_id << " name: " << name);
    //
    // The next if condition can be true if a callback request and an 
    // abort-callback request arrive together (i.e. with the same "tcp" message)
    // at the client. In this case, the abort-callback req will be served
    // before the callback req, even though the callback req is received first!
    //
    if (cbxct->xct()->state() != xct_active) {
	w_assert3(cbxct->xct()->state() == xct_ended);
	TRACE(4, "cb xct has received abort-cb req already");
	W_IGNORE(rm->send_cb_reply(*cbxct, callback_op_t::KILLED));
	cbxct->set_thread(0);
	delete cbxct;
	return;
    }

    bool pinv;
    bool deadlock;

#ifdef HIER_CB
    w_assert3(cc_alg == t_cc_record || cbxct->page_dirty() == FALSE);
#endif

    if (name.lspace() != lockid_t::t_record) {
	_handle_cb_req(cbxct, EX, pinv, deadlock);

#ifdef HIER_CB
    } else if (name.lspace() == lockid_t::t_record && cbxct->page_dirty()) {
	w_assert3(name.rid().slot != max_recs-1);  // not an IX page cb
	_handle_cb_req(cbxct, EX, pinv, deadlock);
#endif

    } else {
	_handle_cb_req(cbxct, IX, pinv, deadlock);
	if (deadlock || pinv) return;
	_handle_cb_req(cbxct, EX, pinv, deadlock);
    }
}


void callback_m::_handle_cb_req(callback_xct_proxy_t* cbxct, lock_mode_t mode,
					bool& pinv, bool& deadlock)
{
    pinv = FALSE;
    deadlock = FALSE;

    lockid_t name = cbxct->name();
    uint2 level = name.lspace();

    if (mode != EX) {
	name.truncate(lockid_t::t_page);
	level = lockid_t::t_page;
    }

    if (!xct()) me()->attach_xct(cbxct->xct());
    w_assert3(xct() == cbxct->xct());

    MUTEX_ACQUIRE(xct()->lock_info()->mutex);
    lock_head_t* lock = llm->_core->find_lock(name, TRUE);

    lock_request_t* my_req = new lock_request_t(xct(), EX, t_short, TRUE);
    lock->queue.append(my_req);

    // Try the EX lock first, even if mode "mode" is IX.

    if (lock->granted_mode == NL) {
	my_req->status(lock_m::t_granted);
	lock->granted_mode = EX;

	MUTEX_RELEASE(lock->mutex);
        MUTEX_RELEASE(xct()->lock_info()->mutex);

        _invalidate(cbxct, level);
        _finish_callback(cbxct);
	pinv = TRUE;

	smlevel_0::stats.lock_acquire_cnt++;

	return;
    }

    my_req->mode = mode;
    if (mode != EX) {
	my_req->xlink.detach();
	xct()->lock_info()->list[t_short].push(my_req);
    }

    // Try the IX lock (applicable only if "mode" is IX)

    if (lock_m::compat[lock->granted_mode][mode]) {
        w_assert3(mode == IX);
        w_assert3(lock->waiting == FALSE);

        my_req->status(lock_m::t_granted);
        lock->granted_mode = lock_m::supr[lock->granted_mode][mode];

        MUTEX_RELEASE(lock->mutex);
        MUTEX_RELEASE(xct()->lock_info()->mutex);

	smlevel_0::stats.lock_acquire_cnt++;

        return;
    }

    // Callback has to wait

    blocker_t* blockers = new blocker_t[lock->queue.num_members()];
    w_assert1(blockers);
    lock_request_t* req = 0;
    w_list_i<lock_request_t> iter(lock->queue);

    int i = 0;
    while (req = iter.next()) {
	if (req == my_req) continue;

	if (req->xd->is_cb_proxy()) {
	    w_assert3(mode != EX);
	    w_assert3(lock_m::compat[req->mode][mode]);
	    continue;
	} else {
            w_assert3(req->xd->is_master_proxy());
	}

	if (mode == EX) {
	    blockers[i].tid = req->xd->tid();

	    if (name.lspace() < lockid_t::t_page) {
		blockers[i++].mode = IS;
	    } else if (name.lspace() == lockid_t::t_record) {
		blockers[i++].mode = SH;
	    } else {			// page callback
#ifdef OBJEC_CC_PLUS
		if (cbxct->page_dirty())	blockers[i++].mode = IS;
		else				blockers[i++].mode = SH;
#else
		blockers[i++].mode = SH;
#endif /* OBJEC_CC_PLUS */
	    }

	} else {
#ifdef OBJEC_CC_PLUS
	    // In the case of an IX mode it is possible to have a converter
            // or waiter waiting for SH mode.
	    lock_mode_t block_mode = NL;

	    switch (req->status()) {
	    case lock_m::t_granted:
	    case lock_m::t_waiting:
	    case lock_m::t_aborted:
		block_mode = req->mode;
		break;
	    case lock_m::t_converting:
		block_mode = req->convert_mode;
	    default:
		W_FATAL(eINTERNAL);
	    }

	    if (block_mode != IS && block_mode != IX) {
		blockers[i].tid = req->xd->tid();
		blockers[i++].mode = SH;
	    }
#else
	   W_FATAL(eINTERNAL);
#endif /* OBJEC_CC_PLUS */
	}
    }

    TRACE(6, "cb req needs to wait: id: " << cbxct->cb_id << " name: "
	    << name << " blockers are: ";
	    for (int j = 0; j < i; j++) { _debug.clog <<blockers[j].tid<<' '; }
	    _debug.clog);

    W_COERCE(rm->send_cb_reply(*cbxct, cb_t::BLOCKED, i, blockers, level));
    delete [] blockers;

    my_req->status(lock_m::t_waiting);
    my_req->thread = me();
    xct()->lock_info()->wait = my_req;
    lock->waiting = TRUE;

    rc_t rc;

    if (mode != EX || level != lockid_t::t_record) {
	// see comment 6.
    } else {
	rc = llm->_core->_check_deadlock(xct());
    }

    if (!rc) {
	MUTEX_RELEASE(lock->mutex);
	MUTEX_RELEASE(xct()->lock_info()->mutex);

	smlevel_0::stats.lock_conflict_cnt++;

	rc = me()->block();

	if (my_req->status() == lock_m::t_aborted)
	    smlevel_0::stats.lock_conflict_cnt--;

	w_assert3(!rc || rc.err_num() == eREMOTEDEADLOCK);
	MUTEX_ACQUIRE(xct()->lock_info()->mutex);
	MUTEX_ACQUIRE(lock->mutex);
    }
    my_req->thread = 0;
    w_assert3(xct()->lock_info()->wait == 0);

    switch (my_req->status()) {
    case lock_m::t_granted:
	w_assert3(xct()->state() == smlevel_1::xct_active);
	smlevel_0::stats.lock_acquire_cnt++;
	break;

    case lock_m::t_aborted:
	if (rc && rc.err_num() == eREMOTEDEADLOCK) {
	    w_assert3(xct()->state() == smlevel_1::xct_ended);
	    TRACE(7, "cb xct self-aborted while being blocked: id: " <<
					cbxct->cb_id << " name: " << name);
	} else {
	    w_assert3(!rc || rc.err_num() == eDEADLOCK);
	    w_assert3(xct()->state() == smlevel_1::xct_active);
	    TRACE(7, "cb xct victimized by local deadlock detection id: "
			<< cbxct->cb_id << " name: " << name);
	}

	delete my_req;
	llm->_core->wakeup_waiters(lock);
	if (lock) MUTEX_RELEASE(lock->mutex);
	MUTEX_RELEASE(xct()->lock_info()->mutex);

	if (rc && rc.err_num() == eREMOTEDEADLOCK) {
	    W_COERCE(rm->send_cb_reply(*cbxct, callback_op_t::KILLED));
	} else {
	    W_COERCE(rm->send_cb_reply(*cbxct, callback_op_t::DEADLOCK));
	}
	_finish_callback(cbxct);
	deadlock = TRUE;

	return;

    default:
	W_FATAL(eINTERNAL);
    }

    MUTEX_RELEASE(lock->mutex);
    MUTEX_RELEASE(xct()->lock_info()->mutex);

    if (mode == EX) {
        TRACE(8, "cb req got the lock; now about to do invalidation: id: "
	    << cbxct->cb_id << " name: " << name);

        _invalidate(cbxct, level);
        _finish_callback(cbxct);
    }
}


void callback_m::_invalidate(callback_xct_proxy_t* cbxct, uint2 level)
{
    lockid_t name = cbxct->name();
    rc_t rc;

    switch (level) {
    case lockid_t::t_record:
#ifdef OBJECT_CC
	rid_t rid = name.rid();

	rc = bf->discard_record(rid, FALSE);
	if (rc)  {
	    if (rc.err_num() != eNOTFOUND) W_FATAL(rc.err_num());
#ifndef HIER_CB
	    w_assert3(lm->purged_flag(rid.pid));
#endif
	    // assertion wrong: see comment 9.
	    // w_assert3(lm->purged_flag(rid.pid));

	    smlevel_0::stats.false_cb_cnt++;
	}
	W_COERCE(rm->send_cb_reply(*cbxct, callback_op_t::OINV));
	break;
#else
	W_FATAL(eINTERNAL);
#endif /* OBJECT_CC */

    case lockid_t::t_page:
        rc = bf->discard_page(name.pid());
        if (rc)  {
            if (rc.err_num() != eNOTFOUND) W_FATAL(rc.err_num());
            W_COERCE(purge_page(name.pid(), FALSE));

	    smlevel_0::stats.false_cb_cnt++;
        }
	W_COERCE(rm->send_cb_reply(*cbxct, callback_op_t::PINV));
        break;
    case lockid_t::t_store:
        W_COERCE(bf->discard_store(*(stid_t *)name.name()));
	W_COERCE(rm->send_cb_reply(*cbxct, callback_op_t::PINV));
        break;
    case lockid_t::t_vol:
        W_COERCE(bf->discard_volume(*(vid_t *)name.name()));
	W_COERCE(rm->send_cb_reply(*cbxct, callback_op_t::PINV));
        break;
    default:
        W_FATAL(eINTERNAL);
    }
}


void callback_m::_finish_callback(callback_xct_proxy_t* cbxct)
{
    W_COERCE(xct()->dispose());
    cbxct->set_thread(0);
    delete cbxct;
}


rc_t callback_m::purge_page(lock_head_t& lock)
{
    w_assert3(lock.name.lspace() == lockid_t::t_page);

    lpid_t& pid = *(lpid_t *)(lock.name.name());

    w_assert3(pid.is_remote());
    w_assert3(lock.queue.num_members() == 0 && lock.purged());

    if (! bf->has_frame(pid)) {
	    W_COERCE(rm->register_purged_page(pid));
    }
    return RCOK;
}


//
// This function is called from the buffer mgr whenever a remote page
// gets purged. In this case the page was indeed cached before being purged
//
// This function is also called from callback_m::do_callback_at_client when
// a page to be called back is not found into the cache. The discussion below
// explains why this is so.
//
// In general, when a page is being called back, there are 4 cases to consider:
//
// (a) purged flag is TRUE && page is cached
// (b) purged flag is TRUE && page is NOT cached
// (c) purged flag is FALSE && page is NOT cached
// (d) purged flag is FALSE && page is cached
//
// NOTE: the purged flag if FALSE by default when no lock exits on the page.
// NOTE: the purged flag is set when the page is purged while it is locked by
//	 one or more local xcts. The purged flag is NOT cleared if the page
//	 gets re-fetched while a local lock still exists on the page.
//
// (d) is the common case: a callback is sent for a page that is indeed cached
//     and either nobody is using it at the client or it has not been purged
//     recently while it was in use.
// (c) represents a race condition: the page has been purged recently but this
//     info had not reached to the server when the server decided to send the
//     callback. In this case, the page should not be put into the purged list
//     because this has already been done.
// In both (a) and (b) the page has not been put into the purged list, so we
//     must do it now. Further, we have to reset the purged flag so that the
//     page is not put twice into the purged list (i.e. now and when the cb
//     xct commits). Case (b) can occur if, for example, the page gets purged
//     while the callback request is blocked at the client, and it is not re-
//     fetched before the callback is granted.
//
rc_t callback_m::purge_page(const lpid_t& pid, bool is_cached)
{
    lock_head_t* lock = llm->_core->find_lock(pid, FALSE);
    w_assert3(is_cached || (lock && lock->granted_mode == EX));

    if (lock) {
	w_assert3(lock->granted_mode != NL);
	if (lock->granted_mode != EX) {
            lock->set_purged();
            MUTEX_RELEASE(lock->mutex);
	    return RCOK;
	} else {
	    lock_request_t* locker = lock->queue.top();
/*
	    //
	    // This is necessary only if there can be lockers with NL
	    // granted mode. Otherwise, there should be a single locker
	    // positioned at the top of the lock queue
	    //
	    w_list_i<lock_request_t> iter(lock->queue);
	    while (locker = iter.next()) {
		if (locker->status() == t_granted) {
		    w_assert3(locker->mode == EX);
		    break;
		}
	    }
*/
	    w_assert3(locker);
	    w_assert3(locker->status()==lock_m::t_granted && locker->mode==EX);
	    w_assert3(is_cached || locker->xd->is_cb_proxy());

	    if (! locker->xd->is_cb_proxy()) {
		lock->set_purged();
		MUTEX_RELEASE(lock->mutex);
		return RCOK;
	    } else {
		if (lock->purged()) {
		    lock->reset_purged();
		} else {
		    if (! is_cached) {
			MUTEX_RELEASE(lock->mutex);
			return RCOK;
		    }
		}
	    }
	}
    }
    W_COERCE(rm->register_purged_page(pid));
    if (lock) MUTEX_RELEASE(lock->mutex);
    return RCOK;
}


#ifdef OBJECT_CC

rc_t callback_m::adaptive_callback(lock_head_t* lock)
{
    w_assert3(cc_adaptive);
    w_assert3(lock->name.lspace() == lockid_t::t_page);
    w_assert3(lock->adaptive());

    lpid_t pid = lock->name.pid();

    long cbid = _cbcount++;

#ifdef NOT_PREEMPTIVE
    adaptive_cb_t* cb = adaptive_cb_tab->lookup(pid);
    if (!cb) {
	cb = new adaptive_cb_t(pid);
	adaptive_cb_tab->push(cb);
    } else {
	TRACE(30, "Adaptive cb waits: pid: " << pid << " xct: " <<xct()->tid());
    }

    cb->count++;

    W_COERCE(cb->mutex.acquire());

    if (lock->adaptive()) {
#endif
	adaptive_xct_t* xacts = new adaptive_xct_t[lock->queue.num_members()];
	w_assert1(xacts);

	lock_request_t* req = lock->queue.top();
	w_assert3(req->xd->is_remote_proxy());
	srvid_t srv = req->xd->master_site_id();
	int n = 0;
	w_list_i<lock_request_t> iter(lock->queue);

	while((req = iter.next())) {
	    if (req->xd == xct()) {
		w_assert3(req != lock->queue.top());
		w_assert3(iter.next() == 0);
		break;
	    }
	    w_assert3(req->xd->is_remote_proxy());
	    w_assert3(req->xd->master_site_id() == srv);
	    xacts[n].tid = req->xd->tid();
	    n++;
	}

	TRACE(28, "Adaptive cb: pid: " << pid << " xct: " << xct()->tid()
		<< " srv: " << srv << " xacts: ";
		for (int i=0; i < n; i++) { _debug.clog << xacts[i].tid<<' '; }
		_debug.clog << " CB ID = " << cbid << flushl);

	W_DO(rm->adaptive_callback(pid, srv, n, xacts, cbid));

	TRACE(28, "Adaptive cb done: id: " << cbid);

	for (int i = 0; i < n; i++) {
	    xct_t* xd = 0;
	    if (xacts[i].tid != tid_t::null) xd = xct_t::look_up(xacts[i].tid);

	    if (xd && xd->state() != smlevel_1::xct_ended) {
#ifdef DEBUG
		TRACE(29, "adaptive locks: cbid: " << cbid << " xd: "
			<< xd->tid() << " recs: ");
		for (int k = 0; k <= 20; k++) {
                    if (bm_is_set(xacts[i].recs, k)) {
                        TRACE_NONL(29, '1');
                    } else {
                        TRACE_NONL(29, '0');
                    }
                }
                TRACE(29, endl);
#endif
	        MUTEX_ACQUIRE(xd->lock_info()->mutex);
	        for (int j = 0; j < max_recs; j++) {
		    if (bm_is_set(xacts[i].recs, j)) {
			rid_t rid(pid, j);
			lock_head_t* rec_lock = llm->_core->find_lock(rid,TRUE);
			w_assert3(rec_lock->granted_mode != EX);
			w_assert3(rec_lock->queue.num_members() <= 1);
			lock_request_t* rec_req = 0;
			if (rec_lock->queue.num_members() == 0) {
			    rec_req = new lock_request_t(xd, EX, t_long, FALSE);
			    rec_lock->queue.append(rec_req);
			} else {
			    rec_req = rec_lock->queue.top();
			    w_assert3(rec_req->xd == xd);
			    w_assert3(rec_req->status() == lock_m::t_granted);
			    w_assert3(rec_req->mode == SH);
			    rec_req->mode = EX;
			}
			rec_req->status(lock_m::t_granted);
			rec_req->count++;
			rec_lock->granted_mode = EX;
			MUTEX_RELEASE(rec_lock->mutex);
		    }
	        }
		MUTEX_RELEASE(xd->lock_info()->mutex);
	    }
	}

	lock->reset_adaptive();

	delete [] xacts;

#ifdef NOT_PREEMPTIVE
    }

    cb->mutex.release();

    cb->count--;
    if (cb->count == 0) {
	adaptive_cb_tab->remove(cb);
	delete cb;
    }
#endif

    return RCOK;
}

#endif /* OBJECT_CC */

/*****************************************************************************/

/*
 * COMMENT 1:
 *
 * With obj-level locking a client may read a page while some other xact is
 * calling back an obj from that page. This is why we must remember the number
 * of read-races before sending the callbacks. Consider the following example:
 * The server has 2 read-races registered for page P by client C. T1, at the
 * server, is trying to callback P. C is not currently using the page. T1 sends
 * a cb to C which causes the invalidation of P from C. Right after this
 * invalidation, two xacts C1 and C2 at C try to read P. Assume that C2's read
 * req (the one marked as "parallel read") reaches the server first, then the
 * PINV cb reply arrives and much later C1's read req also reaches the server.
 * When C2's read req arrives it will register a third read-race for P and then
 * P will be shipped to C. When the cb reply arrives P will be dropped from the
 * copy table. Now, obviously, the server should not delete all (3) read-races
 * for P and C from the race table because another xact (T2) may try to update
 * P before C1's read req arrives at the server. T2 should send a cb to C since
 * P is cached there due to C2's read req.
 *
 * However when calling back a file or volume, then the page cannot be read by
 * any xact while the callback is in progress. So, in this case we don't have to
 * remember the number of read-races per page and client before sending the cbs.
 * When a cb completes successfully at a client, all read races for that client
 * and for pages of the called back file can be dropped from the race table.
 */

/*
 * COMMENT 2:
 *
 * A granted SIX mode is possible under the following scenario:
 * C1 and C2 have both locked page P in SH mode locally. In addition, C1 has an
 * IS page lock at the server. T1 is trying to update P. T1 sends cbs to C1 and
 * C2. Right after handling the cb req at C1, C1 tries to update P, so it sends
 * SIX req to the server. The messages arrive at the server in the following
 * order: first the cb reply from C2, then the SIX req from C1, last the cb
 * reply from C1. Between the arrival of the SIX req and C1's cb reply C2
 * terminates. Just before the termination of C2 both C1 and T1 appear as
 * converters at the server. When C2 releases its lock, either C1 or T1 will be
 * granted its convert mode.
 */

/*
 * comment 3:
 *
 * Before sending cb's an xact that tries to update a page for the first time,
 * marks its page lock as pending (this is done inside lock_core.c). This serves
 * the following purpose:
 *
 * A calling back xact which receives a "page blocked" cb reply should grant the
 * SH page lock to its blocker only if all IX and SIX locks on the page are
 * "pending". A "pending" IX or SIX lock means that its holder is also in the 
 * process of calling back an obj of the page and it hasn't updated any other 
 * obj in the page so far.
 *
 * This is a real race condition: 
 * xd has acquired a local page level SH lock and has propagated this lock req
 * to the server. Then the cb req from xct() arrives at the client, finds the
 * local SH lock. and the cb reply reaches the server before the SH lock req.
 *
 * It is hard to handle this case because doing deadlock detection at the server
 * may victize xd which is not blocked yet and it is not a calling back xact.
 * A possible solution is to abort xd. This must be done carefuly: xct() should
 * set an "aborted" flag in xd's server proxy. xd's server proxy should check
 * this flag before handling any req arriving from its master proxy.
 */

/*
 * COMMENT 4:
 *
 * xd can be waiting or converting for any mode except IX. The reason is that
 * xd may send any lock req right after the xct() cb gets blocked at the client,
 * and that lock req can reach the server before the cb reply. xd cannot be
 * waiting for IX because xd already holds an SH lock on the page, so acquiring
 * an IX mode (either explicitly or implicitly) will cause the propagation of
 * an SIX lock req to the server.
 */

/*
 * comment 5:
 *
 * If a converter or a waiter is waiting for an IS or SH mode, then there is a
 * granted IX, or SIX, or EX mode, so the cb reply should be "deadlock". If the
 * wait mode is IX, or SIX, or EX then again the cb reply should be "deadlock".
 */

/*
 * comment 6:
 *
 * No need to do deadlock detection for xct() because my_req is the first (and
 * possibly the only) lock that xct() tries to acquire; therefore nobody can
 * be waiting for xct().
 * However, if the lock requested by xct() has to wait, then it is possible
 * that xct() will get invloved in a deadlock later: assume xct() asks for EX
 * and it has to wait for C1, then C1 waits for C2 on some other data, and 
 * finally C2 requests an SH lock on the callback data.
 */

/*
 * comment 7:
 *
 * This is possible under the following scenario:
 * Xacts C1 and T both have read page P and hold IS locks on it at the server.
 * Xact C2 holds an SH page lock on P at some client.
 * C1 tries to update obj X in P. It sends a cb to C2 and gets a "page-blocked"
 * cb reply. C1 will then become a converter with IS granted and IX convert
 * modes. Next T tries to acquire an SH page lock on P and it succeeds!.
 * Next C2 commits, so C1 gets a OINV cb reply and it will try to re-acquire
 * its IX page lock on P. But this is not possible because of T's SH lock on P.
 */

/*
 * comment 8:
 *
 * Repeating the callback operation may be necessary under the following case:
 *
 * T tries to update obj O in page P. C1 holds an SH page lock on P at a client  * site. C2 is using the page but not obj O. T sends cb's to C1 and C2. C2 
 * replies with OINV. Next C1 replies with "page blocked" and T downgrades its
 * EX lock on O. Before C1 commits, C2 tries to read obj O. A read req is sent
 * to the server. C2 may or may not hold an IS on P at the server already.
 * If C2 does hold an IS on P then it will be able to get the SH lock on O and
 * receive the page. If C2 does not hold an IS on P, then it will block on P
 * because T is a converter on P. But even in this case C2 will get the SH lock
 * on O before T re-acquires its EX lock: when C1 commits both T's IX and C2's
 * IS locks on P will be granted. Then C2 will wake up and ask and get the SH 
 * lock on O. T has to wait for the cb reply from C1 before it can try to
 * re-acquire the EX lock on O. So, in both cases, C2 will get the page. T will
 * have to wait for C2 before it can require the EX lock on O, but after C2
 * commits, T will not send another cb to C2 which is an error.
 *
 * The solution is the following: 
 * If any transaction acquires an SH lock on an obj on which there is a pending
 * cb (this can be checked by checking if the lock head for the obj is pending),
 * then it flags the lock head of the obj as "repeat_cb". After the callback on
 * the obj is complete, the calling back xct check for the "repeat_cb" flag.
 * If it is on, the cb operation is repeated.
 */

/*
 * comment 9:
 *
 * This assertion will be wrong under the following scenario:
 * T1 updates rec X in page P. P has already been updated before by T1 or other
 * xact. T1 sends cb for X to C1. This cb blocks at the rec level and no lock is
 * acquired by the cb at the page level (since the page is already dirty). Now,
 * C1 tries to update the page with a page EX lock. The EX page lock is granted
 * locally but when C1 tries to get the EX lock at the server it aborts. C1 will
 * purge page P from the cache during the abort. When C1 releases its local
 * locks, P will be pushed into the purged list since C1 is the only one locking
 * P.
 */

/*
 * comment 10.
 *
 * The last condition in this assertion is possible if, for example:
 * (a) xd have had an IS on page P before xct() started the callback, and
 * (b) xct() loses its IX on P due to an SH lock on P by some other client, and
 * (c) xd asks for a SH lock on P right after xct()'s cb gets blocked by xd, and
 * (d) xd's SH lock reply reaches the server before the cb reply
 */

/*
 * comment 11.
 * 
 */

/*
 * SCENARIO 1 :
 *
 * Xacts involved: T1 running at the server, C1 at client 1, and C2 at client 2.
 *
 * C2 is caching page P and has SH page lock on it. This lock has not been 
 * propagated to the server. T1 wants to update rec R in P. At the same time,
 * C1 wants to read R. 
 *
 * T1 sends callbacks to C1 and C2. C1 sends a read req for R to the server.
 * Assume the read req arrives at the server before T1's cb to C1.
 * So the situation with the server lock table is as shown below:
 *
 *                      -----    ----------      ----------
 *                      | P |<---| T1, IX |<-----| C1, IS |
 *                      -----    ----------      ----------
 *			  |
 *      ----------      -----    ----------
 *      | C1, SH |----->| R |<---| T1, EX |
 *      ---------       -----    ----------
 *
 * At this state, T1 has not received any cb replies yet. 
 * T1 will receive the following cb replies:
 *      cb1: T1 blocked by C1 at record level (mode SH)
 *      cb2: T1 blocked by C2 at page level (mode SH)
 *
 * Assume that cb2 arrives first.
 *
 * Then the server's lock table will reach the state shown below:
 *
 *      ----------      -----    ----------      ----------      ----------
 *      | T1, IX |----->| P |<---| T1, IS |<-----| C1, IS |<-----| C2, SH |
 *      ----------      -----    ----------      ----------      ----------
 *			  |
 *                      -----    ----------     ----------
 *                      | R |<---| T1, SH |<----| C1, SH |
 *                      -----    ----------     ----------
 *
 * Now consider the following cases:
 * 
 * 1.1 The next thing to happen is the arrival of cb1.
 *     Then T1 will try to get an SH lock on behalf of C1 and it will find that
 *     this lock is already granted.
 *
 * 1.2 Assume there is one more xact like C1, i.e. C3 is caching P but not R
 *     and C3 is trying to read R at the same time as T1 tries to update R.
 *     Assume again that cb2 arrives first. After cb2's arrival and handling,
 *     both C1 and C3 will hold the SH lock on R. 
 *     The next thing to happen is that C1 tries to update R, and C1's write
 *     lock req arrives at the server before cb1. Then when cb1 arrives it
 *     will find C1 being a converter.
 *
 */

#endif /* MULTI_SERVER */

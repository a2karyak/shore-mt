/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

#undef TURN_OFF_LOCKING

/*
 *  $Id: lock.c,v 1.112 1996/06/27 17:23:00 kupsch Exp $
 */
#define SM_SOURCE
#define LOCK_C

#ifdef __GNUG__
#pragma implementation "lock.h"
#endif

#include <sm_int.h>
#include <lock_x.h>
#include <lock_core.h>

#define W_VOID(x) x

#ifdef __GNUG__
template class w_list_i<lock_request_t>;
template class w_list_t<lock_request_t>;
template class w_list_i<lock_head_t>;
template class w_list_t<lock_head_t>;
template class lock_cache_t<xct_lock_info_t::lock_cache_size>;
template class w_cirqueue_t<lock_cache_elem_t>;
template class w_cirqueue_reverse_i<lock_cache_elem_t>;
#endif


#define DBGTHRD(arg) DBG(<<" th."<<me()->id << " " arg)

const cvec_t 			kvl_t::eof("EOF", 3);
const cvec_t 			kvl_t::bof("BOF", 3);


lock_m::lock_m(int sz)
{
    _core = new lock_core_m(sz);
    w_assert1(_core);
}


lock_m::~lock_m()
{
    delete _core;
}


void lock_m::dump()
{
    _core->dump();
}

inline
bool 
lock_m::get_parent(const lockid_t& c, lockid_t& p)
{
    //
    // optimized for speed ... assumed byte order
    // 	0-1: volume id
    // 	0-4: store id
    //	0-8: page id 
    //
    p.zero();
    p.lspace() = lockid_t::t_bad;
    switch (c.lspace()) {
    case lockid_t::t_vol:
	// returns false since 
	// it doesn't have a parent
	break;
    case lockid_t::t_extent:
	// NEW: parent of extent is volume
	new (&p) lockid_t(*(vid_t*)c.name());
	break;
    case lockid_t::t_store:
	// parent of store is volume
	new (&p) lockid_t(*(vid_t*)c.name());
	break;
    case lockid_t::t_page:
	// parent of page is store
	new (&p) lockid_t(*(stid_t*)c.name());
	break;
    case lockid_t::t_kvl:
	// parent of kvl is store?
	new (&p) lockid_t(*(stid_t*)c.name());
	break;
    case lockid_t::t_record:
	// parent of record is page
	new (&p) lockid_t(*(lpid_t*)c.name());
	break;
    default:
	W_FATAL(eINTERNAL);
    }
    return p.lspace() != lockid_t::t_bad;
}


rc_t lock_m::query(
    const lockid_t&     n,
    mode_t&             m,
    const tid_t&        tid,
    bool		implicit)
{
#ifdef TURN_OFF_LOCKING
    if(n.lspace() == lockid_t::t_store) {
	m = EX;
	return RCOK;
    } else if(n.lspace() != lockid_t::t_extent
	 && n.lspace() != lockid_t::t_vol) 
    {
	m = NL;
	return RCOK;
    }
    /* do usual thing for extents and volumes */
#endif
    xct_t *	xd = xct();
    w_assert3(!implicit || tid != tid_t::null);

    smlevel_0::stats.lock_query_cnt++;
    if (!implicit) {
	m = NL;

	// search the cahe first, if you can

	if (n.lspace() <= lockid_t::t_page &&
			xd && tid == xd->tid() && xd->lock_cache_enabled())  {

	    mode_t* cm = 0;
	    W_COERCE(xd->lock_info()->mutex.acquire());

	    cm = xd->lock_info()->cache[n.lspace()].search(n);
	    if (cm) {
		m = *cm;
		W_VOID(xd->lock_info()->mutex.release());
		return RCOK;
	    }
	    W_VOID(xd->lock_info()->mutex.release());
	}

	// cache was not searched, or search failed

	if (tid == tid_t::null) {
	    lock_head_t* lock = _core->find_lock(n, FALSE);
            if (lock) {
                m = lock->granted_mode;
                MUTEX_RELEASE(lock->mutex);
            }
            return RCOK;
	}

	lock_request_t* req = 0;
	lock_head_t* lock = _core->find_lock(n, FALSE);
	if (lock) req = _core->find_req(lock->queue, tid);
	if (req) {
            m = req->mode;
	    MUTEX_RELEASE(lock->mutex);
	    return RCOK;
	}

        if (lock) MUTEX_RELEASE(lock->mutex);
	return RCOK;

    } else {
	return _query_implicit(n, m, tid);
    }
    return RCOK;
}


rc_t lock_m::_query_implicit(
    const lockid_t&     n,
    mode_t&		m,
    const tid_t&        tid)
{
    w_assert1(tid != tid_t::null);
    m = NL;
    xct_t *	xd = xct();
    lockid_t 	path[lockid_t::NUMLEVELS];
    bool 	SIX_found = FALSE;
    int 	pathlen = 1;

    path[0] = n;

    while (pathlen < lockid_t::NUMLEVELS &&
		get_parent(path[pathlen-1], path[pathlen])) pathlen++;

    // search the cahe first, if you can

    if (xd && tid == xd->tid() && xd->lock_cache_enabled())  {

	mode_t* cm = 0;
	W_COERCE(xd->lock_info()->mutex.acquire());

	for (int curr = pathlen-1; curr >= 0; curr--) {
	    if (path[curr].lspace() <= lockid_t::t_page) {
	        cm = xd->lock_info()->cache[path[curr].lspace()].search(path[curr]);
                if (cm) {
		    if (curr == 0) {
			w_assert3(path[curr] == n);
			if (SIX_found)	m = supr[SH][*cm];
			else		m = *cm;
			W_VOID(xd->lock_info()->mutex.release());
			return RCOK;
		    } else {
			if (*cm == SH || *cm == UD || *cm == EX) {
			    m = *cm;
			} else if (*cm == SIX) {
			    SIX_found = TRUE;
			    continue;
			} else {
			    continue;
			}
			W_VOID(xd->lock_info()->mutex.release());
			return RCOK;
		    }
		} else {
		    break;
		}
	    }
	}
	W_VOID(xd->lock_info()->mutex.release());
    }

    // cache was not searched, or search failed

    SIX_found = FALSE;

    for (int curr = pathlen-1; curr >= 0; curr--) {
	lock_request_t* req = 0;
	lock_head_t* lock = _core->find_lock(path[curr], FALSE);
	if (lock) req = _core->find_req(lock->queue, tid);
	if (req) {
	    if (curr == 0) {
		w_assert3(path[curr] == n);
		if (SIX_found)  m = supr[SH][req->mode];
		else		m = req->mode;
		MUTEX_RELEASE(lock->mutex);
		return RCOK;
	    } else {
	        if (req->mode == SH || req->mode == UD || req->mode == EX) {
		    m = req->mode;
		} else if (req->mode == SIX) {
		    SIX_found = TRUE;
		    m = SH;	// the mode we are looking for is at least SH;
				// we must look deeper in case there is higher
				// mode
		    MUTEX_RELEASE(lock->mutex);
		    continue;
	        } else {
		    MUTEX_RELEASE(lock->mutex);
		    continue;
		}
	        MUTEX_RELEASE(lock->mutex);
	        return RCOK;
	    }
	}
	if (lock) MUTEX_RELEASE(lock->mutex);
    }

    return RCOK;
}


rc_t
lock_m::query_lockers(
    const lockid_t&	n,
    int&		numlockers,
    locker_mode_t*&	lockers)
{
    numlockers = 0;
    lockers = 0;
    lock_head_t* lock = _core->find_lock(n, FALSE);

    if (lock && lock->queue.num_members() > 0) {
	lockers = new locker_mode_t [lock->queue.num_members()];
	w_list_i<lock_request_t> iter(lock->queue);
	lock_request_t* request = 0;
	while ((request = iter.next()) && request->status() != t_waiting) {
	    lockers[numlockers].tid = request->xd->tid();
	    lockers[numlockers].mode = request->mode;
	    numlockers++;
	    w_assert3(request->mode != NL);
	}
    }
    if (lock) MUTEX_RELEASE(lock->mutex);
    return RCOK;
}


rc_t
lock_m::open_quark()
{
    W_DO(lm->_core->open_quark(xct()));
    return RCOK;
}


rc_t
lock_m::close_quark(bool release_locks)
{
    W_DO(lm->_core->close_quark(xct(), release_locks));
    return RCOK;
}


rc_t
lock_m::_lock(
    const lockid_t& 	n,
    mode_t 		m,
    mode_t&		prev_mode,
    mode_t&		prev_pgmode,
    duration_t 		duration,
    long 		timeout, 
    bool 		force)
{
    FUNC(lock_m::_lock);
    w_rc_t  	rc; // == RCOK
    xct_t* 	xd = xct();
    bool	acquired=false;
    smlevel_0::stats.lock_request_cnt++;

    DBGTHRD(<< "lock " << n << " mode=" << m << " duration=" << duration 
	<< " timeout=" << timeout << " xct=" << *xd 
    );
    prev_mode = NL;
    prev_pgmode = NL;

    switch (timeout) {
    case WAIT_SPECIFIED_BY_XCT:
	// If there's no xct, use thread (default is WAIT_FOREVER)
	if(xd) {
	    timeout = xd->timeout_c();
	    break;
	}
	// DROP THROUGH to WAIT_SPECIFIED_BY_THREAD ...
	// (whose default is WAIT_FOREVER)

    case WAIT_SPECIFIED_BY_THREAD:
	timeout = me()->lock_timeout();
	break;

    default:
	break;
    }

    w_assert3(timeout >= 0 || timeout == WAIT_FOREVER);

    if (xd) {
	W_COERCE(xd->lock_info()->mutex.acquire());
	acquired=true;
	if (n.lspace() <= lockid_t::t_page && xd->lock_cache_enabled()) {
	    mode_t* cm = xd->lock_info()->cache[n.lspace()].search(n);
	    if (cm && *cm == supr[m][*cm]) {
		prev_mode = *cm;
		if (n.lspace() == lockid_t::t_page) prev_pgmode = *cm;
		smlevel_0::stats.lock_cache_hit_cnt++;
		goto done; // release mutex
	    }
	}

	lockid_t* h = xd->lock_info_hierarchy();
	w_assert3(h);
	h[0] = n;

	mode_t pmode = parent_mode[m];
	mode_t* hmode[lockid_t::NUMLEVELS];
	hmode[0] = 0;

	int i;
	for (i = 1; i < lockid_t::NUMLEVELS; i++)  {

	    if (get_parent(h[i-1], h[i]) == FALSE) break;

	    if (xd->lock_cache_enabled()) {
		hmode[i] = 0;
		lock_cache_t<xct_lock_info_t::lock_cache_size>* cache = 0;

		cache = &xd->lock_info()->cache[h[i].lspace()];

		if (cache)
		    hmode[i] = cache->search(h[i]);

		if (hmode[i] && supr[*hmode[i]][pmode] == *hmode[i])  {
		    // cache hit
		    smlevel_0::stats.lock_cache_hit_cnt++;

		    if (h[i].lspace() == lockid_t::t_page)
			prev_pgmode = *hmode[i];

		    if (! force)  {
			mode_t held = *hmode[i];
			if (held == SH || held == EX || held == UD ||
						(held == SIX && m == SH))  {
			    i = 0;
			    if (held == SIX && n.lspace() > h[i].lspace()) {
				_query_implicit(n, prev_mode, xd->tid());
				if (n.lspace() == lockid_t::t_record && h[i].lspace() < lockid_t::t_page)
				    _query_implicit(*(lpid_t*)n.name(), prev_pgmode, xd->tid());
				else
				    prev_pgmode = prev_mode;
			    } else {
			        prev_mode = held;
			        prev_pgmode = held;
			    }
			}
		    }
		    break;
		}
	    }
	}
	w_assert3(i < lockid_t::NUMLEVELS || (i == lockid_t::NUMLEVELS && 
		 h[i-1].lspace() == lockid_t::t_vol));
	
	// If the transaction is running during a quark, the
	// locks should be short duration so that the quark can
	// unlock them.  
	if (xd->lock_info()->in_quark_scope()) {
	    if (duration > t_short) {
		duration = t_short;
	    }
	}

	for (int j = i - 1; j >= 0; j--)  {
	    mode_t ret;
	    mode_t need = (j == 0 ? m : pmode);

	    do {
		rc = _core->acquire(xd, h[j], 0, 0, need, prev_mode, duration, 
								timeout, ret);
	    } while (rc && rc.err_num() == eRETRY);
	    if(rc) {
		RC_AUGMENT(rc);
		goto done;
	    }

	    if (h[j].lspace() == lockid_t::t_page)  prev_pgmode = prev_mode;

	    bool brake = (ret == SH || ret == EX || ret == UD); 

	    if (duration == t_instant) {
		W_COERCE( _core->release(xd, h[j], 0, 0, TRUE) );

	    } else if (duration >= t_long && xd->lock_cache_enabled())  {
		if (hmode[j])
		    *hmode[j] = ret;
		else if (h[j].lspace() <= lockid_t::t_page)
		    xd->lock_info()->cache[h[j].lspace()].put(h[j], ret);

		if (h[j].lspace() <= lockid_t::t_page && (brake || ret == SIX)){
		    int k;
		    for (k = h[j].lspace(); k < lockid_t::t_page;)
		        xd->lock_info()->cache[++k].reset();
		    for (k = j-1; k >= 0; k--)
			hmode[k] = NULL;
		}
	    } 
	    if (! force && (brake || (ret == SIX && m == SH))) {
		if (h[j].lspace() < lockid_t::t_page) prev_pgmode = prev_mode;
		break;
	    }
	}
    } else {
	// Tan thinks this might cause a problem during recovery (when
	// (de)allocating pages.  So far it hasn't. 
	// Tan: as predicted, this has shown up in btree undo with KVL.
	// W_FATAL(eNOTRANS); 
    }
done:
    if(acquired)  {
	w_assert3(xd!=0);
	W_VOID(xd->lock_info()->mutex.release());
    }
    return rc;
}


rc_t
lock_m::unlock(const lockid_t& n)
{
#ifdef TURN_OFF_LOCKING
    if(n.lspace() != lockid_t::t_extent
     && n.lspace() != lockid_t::t_vol) {
	return RCOK;
    }
    /* do usual thing for extents */
#endif
    FUNC(lock_m::unlock);
    DBGTHRD(<< "unlock " << n );
    xct_t* 	xd = xct();
    w_rc_t  	rc; // == RCOK
    if (xd)  {
	W_COERCE(xd->lock_info()->mutex.acquire());
	lockid_t h[2];
	int c = 0;
	h[c] = n;
	do {
	    c = 1 - c;
	    rc = _core->release(xd, h[1-c], 0, 0, FALSE);
	    if(rc) break;
	} while (get_parent(h[1-c], h[c]));
	W_VOID(xd->lock_info()->mutex.release());
    }

    smlevel_0::stats.unlock_request_cnt++;
    return rc;
}


rc_t lock_m::unlock_duration(
    duration_t 		duration,
    bool 		all_less_than)
{
#ifdef TURN_OFF_LOCKING
    /* hard to turn this off */
#endif
    FUNC(lock_m::unlock_duration);
    DBGTHRD(<< "lock_m::unlock_duration" 
	<< " duration=" << duration 
	<< " all_less_than=" << all_less_than 
    );
    xct_t* 	xd = xct();
    w_rc_t	rc;	// == RCOK
    if (xd)  {
	W_COERCE(xd->lock_info()->mutex.acquire());
	rc =  _core->release_duration(xd, duration, all_less_than);
	W_VOID(xd->lock_info()->mutex.release());
    }
    return rc;
}

void			
lock_m::stats(
    u_long & buckets_used,
    u_long & max_bucket_len, 
    u_long & min_bucket_len, 
    u_long & mode_bucket_len, 
    float & avg_bucket_len,
    float & var_bucket_len,
    float & std_bucket_len
) const 
{
	core()->stats(
		buckets_used, max_bucket_len,
		min_bucket_len, mode_bucket_len,
		avg_bucket_len, var_bucket_len, std_bucket_len
		);
}

rc_t
lock_m::lock(
    const lockid_t&	n, 
    mode_t 		m,
    duration_t 		duration,
    long 		timeout,
    mode_t*		prev_mode,
    mode_t*             prev_pgmode)
{
#ifdef TURN_OFF_LOCKING
    if(n.lspace() != lockid_t::t_extent
     && n.lspace() != lockid_t::t_vol) {
	if (prev_mode != 0) *prev_mode = EX;
	if (prev_pgmode != 0) *prev_pgmode = EX;
	return RCOK;
    }
    /* do usual thing for extents */
#endif

    mode_t _prev_mode;
    mode_t _prev_pgmode;
    rc_t rc = _lock(n, m, _prev_mode, _prev_pgmode, duration, timeout, FALSE);
    if (prev_mode != 0) *prev_mode = _prev_mode;
    if (prev_pgmode != 0) *prev_pgmode = _prev_pgmode;
    return rc;
}

rc_t
lock_m::lock_force(
    const lockid_t&	n, 
    mode_t 		m,
    duration_t 		duration,
    long 		timeout,
    mode_t*		prev_mode,
    mode_t*             prev_pgmode)
{
#ifdef TURN_OFF_LOCKING
    if(n.lspace() != lockid_t::t_extent &&
	n.lspace() != lockid_t::t_vol) {
	if (prev_mode != 0) *prev_mode = EX;
	if (prev_pgmode != 0) *prev_pgmode = EX;
	return RCOK;
    }
    /* do usual thing for extents */
#endif
    mode_t _prev_mode;
    mode_t _prev_pgmode;
    rc_t rc = _lock(n, m, _prev_mode, _prev_pgmode, duration, timeout, TRUE);
    if (prev_mode != 0) *prev_mode = _prev_mode;
    if (prev_pgmode != 0) *prev_pgmode = _prev_pgmode;
    return rc;
}

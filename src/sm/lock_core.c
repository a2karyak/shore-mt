/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

/*
 *  $Id: lock_core.c,v 1.43 1996/06/27 17:16:14 kupsch Exp $
 */
#define LOCK_CORE_C

#ifdef __GNUG__
#pragma implementation "lock_s.h"
#pragma implementation "lock_x.h"
#pragma implementation "lock_core.h"
#endif

#include <sm_int.h>
#include <lock_x.h>
#include <lock_core.h>

#ifdef MULTI_SERVER
#include "srvid_t.h"
#include "callback.h"
#include "remote_s.h"
#include "remote_client.h"
#endif

#ifdef __GNUG__
template class w_list_t<adaptive_lock_t>;
template class w_list_i<adaptive_lock_t>;
template class w_hash_t<adaptive_lock_t, lpid_t>;
template class w_hash_i<adaptive_lock_t, lpid_t>;
#endif

#define DBGTHRD(arg) DBG(<<" th."<<me()->id << " " arg)

W_FASTNEW_STATIC_DECL(lock_request_t, 2048);
W_FASTNEW_STATIC_DECL(lock_head_t, 256);


/*********************************************************************
 *
 *  parent_mode[i] is the lock mode of parent of i
 *	e.g. parent_mode[EX] is IX.
 *
 *********************************************************************/
const lock_base_t::mode_t lock_m::parent_mode[NUM_MODES] = {
    NL, IS, IX, IS, IX, IX, IX
};


/*********************************************************************
 *
 *   duration_str[i]: 	string describing duration i
 *   mode_str[i]:	string describing lock mode i
 *
 *********************************************************************/
const char* const lock_base_t::duration_str[NUM_DURATIONS] = {
    "INSTANT", "SHORT", "MEDIUM", "LONG", "VERY_LONG"
};

const char* const lock_base_t::mode_str[NUM_MODES] = {
    "NL", "IS", "IX", "SH", "SIX", "UD", "EX"
};


/*********************************************************************
 *
 *  Compatibility Table (diff xact)
 *	Page 408, Table 7.11, "Transaction Processing" by Gray & Reuter
 *
 *  compat[r][g] returns bool value if a requested mode r is compatible
 *	with a granted mode g.
 *
 *********************************************************************/
const bool lock_base_t::compat
[NUM_MODES] /* request mode */
[NUM_MODES] /* granted mode */
= {
    	  /* NL     IS     IX     SH     SIX    UD     EX */ 
/*NL*/    { TRUE,  TRUE,  TRUE,  TRUE,  TRUE,  TRUE,  TRUE  }, 
/*IS*/    { TRUE,  TRUE,  TRUE,  TRUE,  TRUE,  FALSE, FALSE },
/*IX*/    { TRUE,  TRUE,  TRUE,  FALSE, FALSE, FALSE, FALSE },
/*SH*/    { TRUE,  TRUE,  FALSE, TRUE,  FALSE, FALSE, FALSE },
/*SIX*/   { TRUE,  TRUE,  FALSE, FALSE, FALSE, FALSE, FALSE },
/*UD*/    { TRUE,  FALSE, FALSE, TRUE,  FALSE, FALSE, FALSE },
/*EX*/    { TRUE,  FALSE, FALSE, FALSE, FALSE, FALSE, FALSE }
};


/*********************************************************************
 *
 *  Supremum Table (Page 467, Figure 8.6)
 *
 *	supr[i][j] returns the supremum of two lock modes i, j.
 *
 *********************************************************************/
const lock_base_t::mode_t lock_base_t::supr[NUM_MODES][NUM_MODES] = {
    { NL,   IS,   IX,   SH,   SIX,  UD,   EX },
    { IS,   IS,   IX,   SH,   SIX,  UD,   EX },
    { IX,   IX,   IX,   SIX,  SIX,  EX,   EX },
    { SH,   SH,   SIX,  SH,   SIX,  UD,   EX },
    { SIX,  SIX,  SIX,  SIX,  SIX,  SIX,  EX },
    { UD,   UD,   EX,   UD,   SIX,  UD,   EX },
    { EX,   EX,   EX,   EX,   EX,   EX,   EX }
};


/*********************************************************************
 *
 *  xct_lock_info_t::xct_lock_info_t()
 *
 *********************************************************************/
xct_lock_info_t::xct_lock_info_t(uint2 type) :
	mutex("xct_lock_info"),
#ifdef MULTI_SERVER
	adaptive_locks(0),
#endif
	wait(0), cycle(0), quark_marker(0)
{
    for (int i = 0; i < NUM_DURATIONS; i++)  {
	list[i].set_offset(offsetof(lock_request_t, xlink));
#ifdef MULTI_SERVER
	EX_list[i].set_offset(offsetof(lock_request_t, xlink));
#endif
    }

#if defined(MULTI_SERVER) && defined(OBJECT_CC)
    if (cc_adaptive && type == xct_t::t_master) {
	adaptive_locks = new w_hash_t<adaptive_lock_t, lpid_t>(100,
				offsetof(adaptive_lock_t, pid),
				offsetof(adaptive_lock_t, link));
	if (!adaptive_locks) { W_FATAL(eOUTOFMEMORY); }
    }
#endif /* MULTI_SERVER && OBJECT_CC */
}


/*********************************************************************
 *
 *  xct_lock_info_t::~xct_lock_info_t()
 *
 *********************************************************************/
xct_lock_info_t::~xct_lock_info_t()
{
#if defined(MULTI_SERVER) && defined(OBJECT_CC)
    if (cc_adaptive && adaptive_locks) {
        adaptive_lock_t* lock;
        w_hash_i<adaptive_lock_t, lpid_t> iter(*adaptive_locks);
        while (lock = iter.next()) {
	    adaptive_locks->remove(lock);
	    delete lock;
        }
        delete adaptive_locks;
        adaptive_locks = 0;
    }
#endif /* MULTI_SERVER && OBJECT_CC */

#ifdef DEBUG
    for (int i = 0; i < NUM_DURATIONS; i++)  {
	if(! list[i].is_empty() ) {
	    DBGTHRD(<<"memory leak: non-empty list in xct_lock_info_t: " <<i);
	}
#ifdef MULTI_SERVER
	if( ! EX_list[i].is_empty() ) {
	    DBGTHRD(<<"memory leak: non-empty EX_list in xct_lock_info_t: "<<i);
	}
#endif /* MULTI_SERVER */
    }
#endif /* DEBUG */
}


/*********************************************************************
 *
 *  xct_lock_info_t output operator
 *
 *********************************************************************/
ostream &            
operator<<(ostream &o, const xct_lock_info_t &x)
{
	if(x.cycle) {
		o << " cycle: " << x.cycle->tid();
	}
	if(x.wait) {
		o << " wait: " << *x.wait;
	}
	return o << endl;
}



/*********************************************************************
 *
 *   lock_head_t::granted_mode_other(exclude)
 *
 *   Compute group mode of *other* members of the granted group.
 *   The lock request "exclude" is specifically neglected.
 *
 *********************************************************************/
inline lock_base_t::mode_t
lock_head_t::granted_mode_other(const lock_request_t* exclude)
{
    w_assert3(!exclude || exclude->status() == lock_m::t_granted ||
			  exclude->status() == lock_m::t_converting);

    lock_base_t::mode_t gmode = NL;
    w_list_i<lock_request_t> iter(queue);
    lock_request_t* f;
    while ((f = iter.next())) {
	if (f->status() == lock_m::t_waiting) break;
	if (f->status() == lock_m::t_aborted && f->convert_mode == NL) break;

	// f is granted -- make sure it's got a mode that really
	// should have been granted, i.e., it's compatible with all the other
	// granted modes.  UD cases aren't symmetric, so we do both checks here:
	w_assert3(lock_m::compat[f->mode][gmode] || lock_m::compat[gmode][f->mode]);

	if (f != exclude) gmode = lock_base_t::supr[f->mode][gmode];
    }

    return gmode;
}

#if defined(MULTI_SERVER) && defined(OBJECT_CC)

inline bool
lock_head_t::adaptive()
{
    w_assert3(smlevel_0::cc_adaptive)
    w_assert3(!name.vid().is_remote());
    w_assert3(!(flags & t_adaptive) || name.lspace() == lockid_t::t_page);

    return flags & t_adaptive;
}


inline void
lock_head_t::set_adaptive()
{
    w_assert3(smlevel_0::cc_adaptive)
    w_assert3(name.lspace() == lockid_t::t_page);
    w_assert3(!name.vid().is_remote());

    flags |= t_adaptive;
}


inline void
lock_head_t::reset_adaptive()
{
    w_assert3(smlevel_0::cc_adaptive)
    w_assert3(name.lspace() == lockid_t::t_page);
    w_assert3(!name.vid().is_remote());

    flags &= ~t_adaptive;
}


srvid_t
lock_head_t::adaptive_owner(lock_request_t* except)
{
    w_assert3(adaptive());

    lock_request_t* req = queue.top();
    w_assert3(req->xd->is_remote_proxy());
    srvid_t srv = req->xd->master_site_id();
#ifdef DEBUG
    adaptive_cb_t* cb = smlevel_1::cbm->adaptive_cb_tab->lookup(name.pid());
    if (!cb) {
        w_list_i<lock_request_t> iter(queue);
        while (req = iter.next()) {
            if (req->status() == lock_m::t_waiting) break;
            if (req == except) continue;
            w_assert3(req->xd->is_remote_proxy());
            w_assert3(req->xd->master_site_id() == srv);
        }
    }
#endif
    return srv;
}

#endif /* MULTI_SERVER && OBJECT_CC */


/*********************************************************************
 *
 *  lock_request_t::lock_request_t(xct, mode, duration)
 *
 *  Create a lock request for transaction "xct" with "mode" and on
 *  "duration".
 *
 *********************************************************************/
#if !defined(W_DEBUG)&&!defined(MULTI_SERVER)
#define remote /* remote not used */
#endif
inline NORET
lock_request_t::lock_request_t(xct_t* x, mode_t m, duration_t d, bool remote)
    : state(0), mode(m), convert_mode(NL), count(0), duration(d),
      thread(0), xd(x)
#undef remote
{
    smlevel_0::stats.lock_request_t_cnt++;

    // since d is unsigned, the >= comparison must be split to avoid
    // gcc warning.
    w_assert1((d == 0 || d > 0) && d < lock_base_t::NUM_DURATIONS);

#ifdef MULTI_SERVER
    if ((mode == EX) && remote)	x->lock_info()->EX_list[d].push(this);
    else			x->lock_info()->list[d].push(this);
#else
    w_assert3(!remote);
    x->lock_info()->list[d].push(this);
#endif
}

/*********************************************************************
 * 
 *  special constructor to make a marker for open quarks
 *
 *********************************************************************/
#ifndef W_DEBUG
#define quark_marker
#endif
lock_request_t::lock_request_t(xct_t* x, bool quark_marker)
    : mode(NL), convert_mode(NL),
      count(0), duration(t_short), thread(0), xd(x)
#undef quark_marker
{
    FUNC(lock_request_t::lock_request_t(make marker));
    // since the only purpose of this constructor is to make a quark
    // marker, is_quark_marker should be true
    w_assert3(quark_marker == true);

    // a quark marker simply has an empty rlink 

    x->lock_info()->list[duration].push(this);
}


bool
lock_request_t::is_quark_marker() const
{
    if (rlink.member_of() == NULL) {
	w_assert3(mode == NL);
	return true;
    }
    return false;  // not a marker
}

#if HASH_FUNC==3
// use this to compute highest prime # 
// less that requested hash table size. 
// Actually, it uses the highest prime less
// than the next power of 2 larger than the
// number requested.  Lowest allowable
// hash table option size is 64.

static const u_long primes[] = {
	/* 0x40, 64*/ 61,
	/* 0x80, 128 */ 127,
	/* 0x100, 256 */ 251,
	/* 0x200, 512 */ 509,
	/* 0x400, 1024 */ 1021,
	/* 0x800, 2048 */ 2039,
	/* 0x1000, 4096 */ 4093,
	/* 0x2000, 8192 */ 8191,
	/* 0x4000, 16384 */ 16381,
	/* 0x8000, 32768 */ 32749,
	/* 0x10000, 65536 */ 65521,
	/* 0x20000, 131072 */ 131071,
	/* 0x40000, 262144 */ 262139,
	/* 0x80000, 524288 */ 524287,
	/* 0x100000, 1048576 */ 1048573,
	/* 0x200000, 2097152 */ 2097143,
	/* 0x400000, 4194304 */ 4194301,
	/* 0x800000, 8388608   */ 8388593
};
#endif


lock_core_m::lock_core_m(uint sz) : 
	_htab(0), _htabsz(0), _hashmask(0), _hashshift(0)
	, lockHeadPool(offsetof(lock_head_t, chain))
{
    /* round up to the next power of 2 */
    int b=0; // count bits shifted
    for (_htabsz = 1; _htabsz < sz; _htabsz <<= 1) b++;

    w_assert1(!_htab); // just to check size

    w_assert1(_htabsz >= 0x40);
    w_assert1(b >= 6 && b <= 23);
    // if anyone wants a hash table bigger,
    // he's probably in trouble.

#if HASH_FUNC==3
	
    // get highest prime for that numer:
    b -= 6;

    // TODO: REMOVE -- NANCY
    b = 5;

    _htabsz = primes[b];
#endif

    _htab = new bucket_t[_htabsz];

    // compute reasonable mask and shift value
    // make sure the mask includes bits from
    // all parts of a lockid_t.  That means that
    // all bytes of the u_long should be included.
    // Distribute the bits throughout the word,
    // making sure to get at least the lowest-order
    // bit.  We can add the first 6 bits right away
    // because the minimum lock table size is 64
    // buckets, or 0x40 or a shift of 6 bits.

#if HASH_FUNC < 3

    // _hashshift determines how we do the hash:
    // 0 means xor all the BYTES together too
    // 1 xor all shorts together
    // 2 means no extra xor necessary

    b--;
    _hashshift = b / BPB;
    _hashmask = _htabsz-1;
#endif

    w_assert1(_htab);
}

lock_core_m::~lock_core_m()
{
    if (! shutdown_clean)  {
        for (uint i = 0; i < _htabsz; i++)  {
		
            ACQUIRE(i);
            lock_head_t* lock;
            while ((lock = _htab[i].chain.pop()))  {
                MUTEX_ACQUIRE(lock->mutex);
                lock_request_t* req;
                while ((req = lock->queue.pop()))  {
                    delete req;
                }
                MUTEX_RELEASE(lock->mutex);
                delete lock;
            }
	    RELEASE(i);
        }
    }

    // free the all the lock_head_t's in the pool
    LOCK_HEAD_POOL_ACQUIRE(lockHeadPoolMutex);
    while (lock_head_t* theLockHead = lockHeadPool.pop())  {
	delete theLockHead;
    }
    LOCK_HEAD_POOL_RELEASE(lockHeadPoolMutex);

    delete[] _htab;
}


//
// NOTE: if "lock" is not 0, then the lock_head pointed should be protected
//	 (via lock->mutex) before calling this function. Before the function
//	  returns, the mutex on the lock is released
//
rc_t
lock_core_m::acquire(
    xct_t* 		xd,
    const lockid_t&	name,
    lock_head_t* 	lock,
    lock_request_t*	req,
    mode_t 		mode,
    mode_t&		prev_mode,
    duration_t		duration,
    long 		timeout,
    mode_t&		ret)
{
#ifdef DEBUG
    DBGTHRD(<<"lock_core_m::acquire " <<" lockid " <<name<< " tid"<< xd->tid());
    if(req) { DBGTHRD(<< " request =" << *req); }

    w_assert3(xd == xct());
    w_assert3(MUTEX_IS_MINE(xd->lock_info()->mutex));

    if (lock) {
	w_assert3(MUTEX_IS_MINE(lock->mutex));
    }
    DBGTHRD(
	<< " xd=" << xd->tid() << " name=" << name << " mode=" << mode
	<< " duration=" << duration << " timeout=" << timeout );
#endif /* DEBUG */

    bool acquired_lock_mutex = false;
    ret = NL;

    if(!lock) {
	lock = find_lock(name, TRUE);
	acquired_lock_mutex = true;
    }

    if (!req) {
	w_list_i<lock_request_t> iter(lock->queue);
	while ((req = iter.next()) && req->xd != xd);
    }

    {

    /*
     *  See if this is a conversion request
     */
    if (req) {		// conversion
	w_assert3(req->status() == lock_m::t_granted);

#if defined(DEBUG) && defined(MULTI_SERVER) && defined(OBJECT_CC)
	if (cc_adaptive && !name.vid().is_remote() && lock->adaptive())
	    w_assert3(xd->is_remote_proxy() &&
		      xd->master_site_id() == lock->adaptive_owner());
#endif

#if defined(HIER_CB) && defined(MULTI_SERVER)
	// Even though the requested lock may be granted by this lock mgr,
	// the lock is not really granted until callbacks are sent to 
	// clients and return successfully. To indicate this, the request is
	// set in a "pending" state. This is needed during callbacks.
	if (cc_alg == t_cc_record && name.lspace() == lockid_t::t_page &&
		(mode == EX || mode == SIX || mode == IX) && 
		(req->mode==IS || req->mode==SH) && !name.vid().is_remote()) {
	    req->set_pending();
	} else {
	    w_assert3(!req->pending());
	}
#endif /* HIER_CB && MULTI_SERVER */

	prev_mode = req->mode;
	mode = supr[mode][req->mode];

	// optimization: case of re-request of an equivalent or lesser mode
	if (req->mode == mode)  { 
	    smlevel_0::stats.lock_extraneous_req_cnt++;
	    goto success; 
	}
	smlevel_0::stats.lock_conversion_cnt++;

	mode_t granted_mode_other = lock->granted_mode_other(req);
	w_assert3(lock->granted_mode == supr[granted_mode_other][req->mode]);

	if (compat[mode][granted_mode_other]) {
	    /* compatible --> no wait */
	    req->mode = mode;
	    lock->granted_mode = supr[mode][granted_mode_other];
	    goto success;
	}
	/*
	 * in the special case of multiple threads in a transaction,
	 * we will soon (below) return without awaiting this lock if
	 * another thread in this tx is awaiting a lock; in that case,
	 * we don't want to have changed these data members of the request. 
	 */
	if(! (timeout && xd->lock_info()->wait) ) {
	    // only if we won't be blocking below
	    req->status(lock_m::t_converting);
	    req->convert_mode = mode;
	    req->thread = me();
	}
#ifdef MULTI_SERVER
	w_assert1(!xd->lock_info()->wait);
#endif

    } else {		// it is a new request
	prev_mode = NL;

#ifdef MULTI_SERVER
	if (name.lspace() == lockid_t::t_vol) {
	    if (name.vid().is_remote() && xd->is_master_proxy()) {
		MUTEX_RELEASE(lock->mutex);
		W_DO(rm->spread_xct(*(vid_t *)name.name()));
		lock = find_lock(name, TRUE);
	    }
	}
#endif
	req = new lock_request_t(xd, mode, duration, name.vid().is_remote());
	lock->queue.append(req);

#if defined(HIER_CB) && defined(MULTI_SERVER)
	// same comment as in the case of lock convesrion (see above).
	if (cc_alg == t_cc_record && name.lspace() == lockid_t::t_page &&
		(mode==EX || mode==SIX || mode==IX) && !name.vid().is_remote())
	    req->set_pending();
#endif /* HIER_CB && MULTI_SERVER */            

	if ((!lock->waiting) && compat[mode][lock->granted_mode]) {
	    /* compatible ---> no wait */
	    req->status(lock_m::t_granted);
	    lock->granted_mode = supr[mode][lock->granted_mode];

#if defined(OBJECT_CC) && defined(MULTI_SERVER)
	    if (cc_adaptive && !name.vid().is_remote() && lock->adaptive() &&
			(xd->is_master_proxy() ||
			 xd->master_site_id() != lock->adaptive_owner(req))) {
		W_DO(cbm->adaptive_callback(lock));
	    }
#endif /* OBJECT_CC && MULTI_SERVER */

	    goto success;
	}

	req->status(lock_m::t_waiting); 
	req->thread = me();
    }

    /* need to wait */

#ifdef DEBUG
    w_assert3(ret == NL);
    DBGTHRD(<<" will wait");
    if(xd->lock_info()->wait) {
	// the multi-thread case 
	w_assert3(xd->num_threads()>1);
	// w_assert3(lock->waiting==TRUE); could be waiting on a different lock
    }
    // request should match
    w_assert3(req->xd == xd);
#endif

    if(timeout && xd->lock_info()->wait) {
	// Another thread in our xct is blocking. We're going to have to
	// wait on another resource, until our partner thread unblocks,
	// and then try again.

	DBGTHRD(<< "waiting for other thread in :"
	<< " xd=" << xd->tid() << " name=" << name << " mode=" << mode
	<< " duration=" << duration << " timeout=" << timeout );

	// if this was not a conversion, delete the (new) request
	if(prev_mode == NL) delete req;

	if(lock) MUTEX_RELEASE(lock->mutex);
	MUTEX_RELEASE(xd->lock_info()->mutex);

	w_rc_t rc;
	rc = xd->lockblock(timeout);
	MUTEX_ACQUIRE(xd->lock_info()->mutex);

	// if we're leaving, we should leave the 
	// mutex state as it was when we entered
	if( !acquired_lock_mutex) MUTEX_ACQUIRE(lock->mutex);

	if(!rc) rc = RC(eRETRY);
	return rc;
    }

    lock->waiting = TRUE;

    rc_t rc;

    if (timeout)  {
	DBGTHRD(<<" timeout " << timeout);
	// set up the possible deadlock so we can run the deadlock detector
	xd->lock_info()->wait = req;

	rc = _check_deadlock(xd);

	if (!rc) {
	    // either no deadlock or there is a deadlock but 
	    // some other xact was selected as victim
	    // BUGBUG: this code won't work with preemptive threads.

	    MUTEX_RELEASE(lock->mutex);
	    MUTEX_RELEASE(xd->lock_info()->mutex);
	    smlevel_0::stats.lock_conflict_cnt++;

	    DBGTHRD(<< "waiting (blocking) for:"
	          << " xd=" << xd->tid() << " name=" << name << " mode=" << mode
		  << " duration=" << duration << " timeout=" << timeout );

	    rc = me()->block(timeout, 0, "lock");

	    DBGTHRD(<< "acquired (unblocked):"
		    << " xd=" << xd->tid() << " name="<< name << " mode="<< mode
		    << " duration=" << duration << " timeout=" << timeout );

	    if (rc) {
		w_assert3(rc.err_num() == ePREEMPTED);
		w_assert3(name.lspace() == lockid_t::t_record);
	    }

	    // unblock any other thread waitiers
	    if(xd->num_threads()>1) {
		xd->lockunblock();
	    }

	    MUTEX_ACQUIRE(xd->lock_info()->mutex);
	    MUTEX_ACQUIRE(lock->mutex);
	    DBGTHRD(<<" LOCK HEAD==" << (long) lock
			<< " lock->name " <<  lock->name << " name " <<  name);
	    w_assert3(lock->name == name);
	}
    }

    req->thread = 0;

    // make sure that we have cleaned up properly after deadlock detection
    if (timeout) {
	w_assert3(xd->lock_info()->wait == 0);
    } else {
	xd->lock_info()->wait = 0;
    }
    w_assert3(xd->lock_info()->cycle == 0);
    DBGTHRD(<<" request->status()=" << req->status());

    if (! (rc))  {
	//
	// Not victimized during self-initiated deadlock detection.
	// Either a timeout, or we were waken up after some other xact
	// released its lock, or we were chosen as a victim
	// during deadlock detection initiated by some other xact.
	//
	switch (req->status()) {
	case lock_m::t_granted:
#if defined(OBJECT_CC) && defined(MULTI_SERVER)
            if (cc_adaptive && lock->adaptive() && (xd->is_master_proxy() ||
                         xd->master_site_id() != lock->adaptive_owner(req)))
                lock->reset_adaptive();
#endif /* OBJECT_CC && MULTI_SERVER */
	    goto success;
	case lock_m::t_waiting:
	case lock_m::t_converting:
	    rc = RC(eLOCKTIMEOUT);
	    break;
	case lock_m::t_aborted:
	    rc = RC(eDEADLOCK);
	    break;
	default:
	    W_FATAL(eINTERNAL);
	}
    }

    // count only block events followed by sucess.
    smlevel_0::stats.lock_conflict_cnt--;

    /*
     *  the lock request was unsuccessful due to either a deadlock or a 
     *  timeout; so cancel it.
     */
    DBGTHRD(<<" request->status()=" << req->status());
    switch (req->status())  {
    case lock_m::t_converting:
        req->status(lock_m::t_granted);        // revert to granted
        break;
    case lock_m::t_waiting:
        delete req;
        req = 0;
        w_assert3(lock->queue.num_members() > 0);
        break;
    case lock_m::t_aborted:
	if (req->convert_mode == NL) {	// aborted while waiting
	    delete req;
	    req = 0;
	} else {				// aborted while converting
	    ;
	}
	if (lock->queue.num_members() == 0) {
	    // This can happen if T1 is waiting for T2 and then T2 blocks
	    // and forms two cycles simultaneously. If both T1 and T2 are
	    // victimized, then lock will remain without requests.
	    lock->granted_mode = NL;
	    w_assert3(lock->waiting == FALSE);
	}
	break;
    default:
        W_FATAL(eINTERNAL);
    }

    DBGTHRD(<<" waking up waiters");
    wakeup_waiters(lock);

    w_assert3(ret == NL);
    ret = NL;
    if (lock) MUTEX_RELEASE(lock->mutex);
    return rc.reset();

    }

  success:
    w_assert3(req->status() == lock_m::t_granted);
    w_assert3(lock);

#ifdef MULTI_SERVER
    if (mode == EX && name.vid().is_remote() &&
	(name.lspace()==lockid_t::t_store || name.lspace()==lockid_t::t_vol) &&
        duration != t_instant) {
        xct()->EX_locked_files_insert(name);
    }

    if (req->mode == EX && prev_mode != EX && prev_mode != NL &&
						name.vid().is_remote()) {
	req->xlink.detach();
	xd->lock_info()->EX_list[req->duration].push(req);
    }
#endif

    if (req->duration < duration) {
	req->duration = duration;
	req->xlink.detach();
#ifdef MULTI_SERVER
	if (req->mode == EX && name.vid().is_remote())
	    xd->lock_info()->EX_list[duration].push(req);
	else
#endif
	    xd->lock_info()->list[duration].push(req);
    }

#if defined(MULTI_SERVER) && defined(HIER_CB)
    if (lock->pending() && mode == SH && prev_mode == NL) {
	w_assert3(name.lspace() == lockid_t::t_record);
	xct_t* locker = lock->queue.top()->xd;
	lock_request_t* locker_req = lock->queue.top();
	w_assert3(lock->granted_mode == SH);
	w_assert3(locker_req->mode == SH);
	w_assert3(locker_req->status() != lock_m::t_waiting);
	w_assert3(locker->callback()->cb_id >= 0);
	w_assert3(locker->callback()->name == name);
	w_assert3(locker != xct());

	if (locker_req->status() == lock_m::t_granted) lock->set_repeat_cb();
    }
#endif

    ret = mode;
    ++req->count;
    w_assert3(ret != NL);
    MUTEX_RELEASE(lock->mutex);
    smlevel_0::stats.lock_acquire_cnt++;

    switch(name.lspace()) {
    case lockid_t::t_bad:
	break;
    case lockid_t::t_vol:
	smlevel_0::stats.lk_vol_acq++; 
	break;
    case lockid_t::t_store:
	smlevel_0::stats.lk_store_acq++; 
	break;
    case lockid_t::t_page:
	smlevel_0::stats.lk_page_acq++; 
	break;
    case lockid_t::t_kvl:
	smlevel_0::stats.lk_kvl_acq++; 
	break;
    case lockid_t::t_record:
	smlevel_0::stats.lk_rec_acq++; 
	break;
    case lockid_t::t_extent:
	smlevel_0::stats.lk_ext_acq++; 
	break;
    }

    return RCOK;
}


rc_t
lock_core_m::release(
	xct_t*			xd,
	const lockid_t&		name,
	lock_head_t*		lock,
	lock_request_t*		request,
	bool			force)
{
    DBGTHRD(<<"lock_core_m::release "
		<< " lockid " <<name
		<< " tid" << xd->tid()  );

    w_assert3(xd == me()->xct());
    w_assert3(MUTEX_IS_MINE(xd->lock_info()->mutex));
    uint4 idx = _hash(hash(name));

    if(!lock) {
#ifdef MULTI_SERVER
        if (name.vid().is_remote()) bf->mutex_acquire();
#endif
	ACQUIRE(idx);
	lock = find_lock(_htab[idx].chain, name);
	if (lock) MUTEX_ACQUIRE(lock->mutex);
    } else {
	IS_MINE(idx);
	w_assert3(MUTEX_IS_MINE(lock->mutex));
	// TODO: w_assert3(! name.vid().is_remote() || bf->mutex_is_mine());
    }

    if (!lock) {
#ifdef MULTI_SERVER
	if (name.vid().is_remote()) bf->mutex_release();
#endif
	RELEASE(idx);
	return RCOK;
    }

    if (!request) request = find_req(lock->queue, xd);

    if (! request) {
	// lock does not belong to me --- no need to unlock
#ifdef MULTI_SERVER
	if (name.vid().is_remote()) bf->mutex_release();
#endif
	MUTEX_RELEASE(lock->mutex);
	RELEASE(idx);
	return RCOK;
    }

    w_assert3(lock == request->get_lock_head());
    w_assert3(request->status()==lock_m::t_granted || xd->state() == xct_ended);

    if (!force && (request->duration >= t_long || request->count > 1)) {
#ifdef MULTI_SERVER
        if (name.vid().is_remote()) bf->mutex_release();
#endif
        if (request->count > 1) --request->count;
        MUTEX_RELEASE(lock->mutex);
	RELEASE(idx);
        return RCOK;
    }

    delete request;
    request = 0;
    _update_cache(xd, name, NL);

    if (lock->queue.num_members() == 0) {

#ifdef MULTI_SERVER
	if (lock->purged()) {
	    w_assert3(name.vid().is_remote());
	    W_COERCE(cbm->purge_page(*lock));
	}
	if (name.vid().is_remote()) bf->mutex_release();
#endif
	MUTEX_RELEASE(lock->mutex);
	FreeLockHeadToPool(lock);
	RELEASE(idx);
        return RCOK;
    }

#ifdef MULTI_SERVER
    if (name.vid().is_remote()) bf->mutex_release();
#endif

    lock->granted_mode = lock->granted_mode_other(0);
    if (lock->waiting) wakeup_waiters(lock);
    MUTEX_RELEASE(lock->mutex);
    RELEASE(idx);

    return RCOK;
}


#ifdef NOTDEF
//
// NOTE: if "lock" is not 0, then the lock_head pointed should be protected
//       (via lock->mutex) before calling this function. Before the function
//        returns, the mutex on the lock is released.
// NOTE: an xact CAN downgrade the lock mode of another xact.
// NOTE: the request to be downgraded should be in the granted status.
//
rc_t
lock_core_m::downgrade(
        xct_t*                  xd,
        const lockid_t&         name,
        lock_head_t*            lock,
        lock_request_t*         request,
        mode_t                  mode,
        bool                    force)
{
    w_assert3(MUTEX_IS_MINE(xd->lock_info()->mutex));
    if (lock) w_assert3(MUTEX_IS_MINE(lock->mutex));

    if(!lock) lock = find_lock(name, FALSE);
    w_assert3(lock);

    if (!request) request = find_req(lock->queue, xd);
    w_assert3(request);
    w_assert3(lock == request->get_lock_head());

    if (!force && request->duration >= t_long) {
        MUTEX_RELEASE(lock->mutex);
        return RCOK;
    }

    w_assert3(request->mode == supr[request->mode][mode]);
    w_assert3(request->status() == lock_m::t_granted);

#ifdef MULTI_SERVER
    if (request->mode == EX && mode != EX && name.vid().is_remote) {
        w_assert3(request->xlink.member_of() ==
				&(xd->lock_info()->EX_list[request->duration]));
        request->xlink.detach();
	xd->lock_info()->list[request->duration].push(request);
    }
#endif

    request->mode = mode;
//  --request->count;           ???????????????
    _update_cache(xd, name, mode);


    lock->granted_mode = lock->granted_mode_other(0);

    if (lock->waiting) wakeup_waiters(lock);

    MUTEX_RELEASE(lock->mutex);

    return RCOK;
}
#endif


void
lock_core_m::wakeup_waiters(lock_head_t*& lock)
{
    if (lock->queue.num_members() == 0) {
	lockid_t name = lock->name;
	MUTEX_RELEASE(lock->mutex);

#ifdef MULTI_SERVER
	if (name.vid().is_remote()) bf->mutex_acquire();
#endif
	uint4 idx = _hash(hash(name));
	ACQUIRE(idx);
	lock = find_lock(_htab[idx].chain, name);

	if (lock) {
	    MUTEX_ACQUIRE(lock->mutex);

	    if (lock->queue.num_members() == 0) {
#ifdef MULTI_SERVER
	        if (lock->purged()) {
	            w_assert3(lock->name.vid().is_remote());
		    W_COERCE(cbm->purge_page(*lock));
	        }
		if (name.vid().is_remote()) bf->mutex_release();
#endif

		MUTEX_RELEASE(lock->mutex);
	        delete lock;
		lock = 0;
		RELEASE(idx);
		return;
	    }
	}

#ifdef MULTI_SERVER
	if (name.vid().is_remote()) bf->mutex_release();
#endif
	RELEASE(idx);

	if (!lock) return;
    }

    lock_request_t* request = 0;

    lock->waiting = FALSE;
    w_list_i<lock_request_t> iter(lock->queue);
    bool cvt_pending = FALSE;

    while (!lock->waiting && (request = iter.next())) {
        bool wake_up = FALSE;

        switch (request->status()) {
	case lock_m::t_converting: {
            mode_t gmode = lock->granted_mode_other(request);
	    w_assert3(lock->granted_mode == supr[gmode][request->mode]);
            wake_up = compat[request->convert_mode][gmode];
            if (wake_up)
		    request->mode = request->convert_mode;
            else
                cvt_pending = TRUE;
	    break;
	}
        case lock_m::t_waiting:
            if (cvt_pending)  {
                // do not wake up waiter because of pending convertions
                lock->waiting = TRUE;
                break;
            }
            wake_up = compat[request->mode][lock->granted_mode];
            lock->waiting = ! wake_up;
	    break;
	case lock_m::t_aborted:
	case lock_m::t_granted:
	    break;
        default:
            W_FATAL(eINTERNAL);
        }

        if (wake_up) {
            request->status(lock_m::t_granted);
	    request->xd->lock_info()->wait = 0;
            lock->granted_mode = supr[request->mode][lock->granted_mode];
            if (request->thread) { W_COERCE(request->thread->unblock()); }
        }
    }

    if (cvt_pending) lock->waiting = TRUE;
}


rc_t
lock_core_m::release_duration(
    xct_t*              xd,
    duration_t          duration,
    bool              all_less_than)
{
    lock_head_t* lock = 0;
    lock_request_t* request = 0;
    w_assert1((duration == 0 || duration > 0) && duration < NUM_DURATIONS);

    /*
     *  If all_less_than is set, then release from 0 to "duration",
     *          else release only those of "duration"
     */
    for (int i = (all_less_than ? t_instant : duration); i <= duration; i++) {

        while ((request = xd->lock_info()->list[i].pop()))  {
	    if (request->is_quark_marker()) {
		delete request;
		continue;
	    }
            lock = request->get_lock_head();
#ifdef MULTI_SERVER
	    w_assert3(request->mode!=EX || !(lock->name).vid().is_remote());

	    if ((lock->name).vid().is_remote()) bf->mutex_acquire();
#endif
	    ACQUIRE(_hash(hash(lock->name)));
	    MUTEX_ACQUIRE(lock->mutex);
            W_COERCE(release(xd, lock->name, lock, request, TRUE) );
        }

#ifdef MULTI_SERVER
	while (request = xd->lock_info()->EX_list[i].pop())  {
            w_assert3(!request->is_quark_marker());
	    w_assert3(request->mode == EX);

	    lock = request->get_lock_head();
	    w_assert3((lock->name).vid().is_remote());

            bf->mutex_acquire();
            uint4 idx = _hash(hash(lock->name));
            ACQUIRE(idx);
            MUTEX_ACQUIRE(lock->mutex);
            W_COERCE(release(xd, lock->name, lock, request, TRUE) );
        }
#endif
    }

    return RCOK;
}


// This method opens a new quark by using a special lock_request_t 
// as a marker for the beginning of the quark.
// (see lock_x.h for descr of quark).
rc_t
lock_core_m::open_quark(
    xct_t*              xd)
{
    FUNC(lock_core_m::open_quark);
    if (xd->lock_info()->in_quark_scope()) {
	// nested quarks not allowed
	W_FATAL(eINTERNAL);
    }

    xd->lock_info()->quark_marker = new lock_request_t(xd, true /*is marker*/);
    if (xd->lock_info()->quark_marker == NULL) return RC(fcOUTOFMEMORY);
    return RCOK;
}


// This method releases all short read locks acquired since
// the last quark was opened (see lock_x.h for descr of quark).
rc_t
lock_core_m::close_quark(
    xct_t*              xd,
    bool		release_locks)
{
    FUNC(lock_core_m::close_quark);
    if (!xd->lock_info()->in_quark_scope()) {
	W_FATAL(eINTERNAL);
    }

    if (!release_locks) {
	// locks should not be released, so just remove the marker
	xd->lock_info()->quark_marker->xlink.detach();
	delete xd->lock_info()->quark_marker;
	xd->lock_info()->quark_marker = NULL;
	return RCOK;
    }

    lock_head_t*	lock = 0;
    lock_request_t*	request = 0;
    bool		found_marker = false;

    // release all locks up to the marker for the beginning of the quark
    w_list_i<lock_request_t> iter(xd->lock_info()->list[t_short]);
    while ((request = iter.next()))  {
	w_assert3(request->duration == t_short);
	if (request->is_quark_marker()) {
	    w_assert3(request == xd->lock_info()->quark_marker);
	    request->xlink.detach();
	    xd->lock_info()->quark_marker = NULL;
	    delete request;
	    found_marker = true;
	    break;  // finished
	}
	if (request->mode == IS || request->mode == SH || request->mode == UD) {
	    // share lock, so we can release it early
	    request->xlink.detach();

	    lock = request->get_lock_head();
#ifdef MULTI_SERVER
	    if ((lock->name).vid().is_remote()) bf->mutex_acquire();
#endif
	    ACQUIRE(_hash(hash(lock->name)));
	    MUTEX_ACQUIRE(xd->lock_info()->mutex);
	    MUTEX_ACQUIRE(lock->mutex);

	    // Note that the release is done with force==TRUE.
	    // This is correct because if this lock was acquired
	    // before the quark, then we would not be looking at
	    // now.  Since it was acquire after, it is ok to
	    // release it, regardless of the request count.
	    W_COERCE(release(xd, lock->name, lock, request, TRUE) );
	    // releases all the mutexes it acquires
	    MUTEX_RELEASE(xd->lock_info()->mutex);
       } else {
	    // can't release IX, SIX, EX locks
       }
    }
    w_assert3(found_marker);
    return RCOK;
}




xct_t*		last_waiter = 0;

rc_t lock_core_m::_check_deadlock(xct_t* self, bool* deadlock_found)
{
    if (deadlock_found) *deadlock_found = FALSE;

    do {
        rc_t rc = _find_cycle(self);

        if (rc) {
    	    if (deadlock_found) *deadlock_found = TRUE;

	    xct_t* victim_xd = last_waiter;
	    tid_t victim_tid = last_waiter->tid();
	    xct_t* waiter = last_waiter;
	    xct_t* next_waiter = 0;

	    //
	    // Choose a victim
	    // The victim is the youngest transaction
	    // in order to avoid "livelocks".
	    //
	    while ((next_waiter = waiter->lock_info()->cycle)) {
	        waiter->lock_info()->cycle = 0;
	        if (next_waiter->tid() > victim_tid) {
		    victim_xd = next_waiter;
		    victim_tid = next_waiter->tid();
	        }
	        waiter = next_waiter;
	    }
	    w_assert3(waiter == last_waiter);

	    // With our server-to-server callback algorithm it is possible to
	    // find a cycle which does not contain "self" (i.e. while traversing
	    // a path in the wait-for graph, a cycle can be found which does not
	    // contain the first node where the traversal started from). This
	    // can happen only when a calling back xact blocks at clients and
	    // acquires server locks on behalf of xacts it is waiting for.
	    // For this case, remove any remaining arcs of the lock path
	    waiter = self;
	    while ((next_waiter = waiter->lock_info()->cycle)) {
	        waiter->lock_info()->cycle = 0;
	        waiter = next_waiter;
	    }
	    w_assert3(self->lock_info()->cycle == 0);

	    if (victim_xd == self) {
		w_assert1(self->lock_info()->wait->status() == lock_m::t_waiting || 
			  self->lock_info()->wait->status() == lock_m::t_converting);
	        self->lock_info()->wait->status(lock_m::t_aborted);
	        self->lock_info()->wait = 0;
	        return RC(eDEADLOCK);

	    } else {
		// For deadlock detection to work with preemptive threads
		// the whole lock table should be covered by a latch.
		// For any locking operation this latche should be acquired
		// in SH mode at the begining of the operation. To do deadlock
		// detection all lower level semaphores should be realesed and
		// the latch should be acquired in EX mode. 
		// Under this assumption, it is ok to access the lock_info()->wait
		// variable of the victim xact as done below.
		// BUGBUG: for now this works only with non-preemptive threads.

	        lock_request_t* req = victim_xd->lock_info()->wait;
		w_assert1(req->status() == lock_m::t_waiting ||
			  req->status() == lock_m::t_converting);
		req->status(lock_m::t_aborted);
		victim_xd->lock_info()->wait = 0;

	        if (req->thread) {
		    DBGTHRD(<<"about to unblock " << *req);
	            W_DO(req->thread->unblock());
	        } else {
#ifdef MULTI_SERVER
		    // The victimized xact is in the middle of a callback
		    // operation. Send an msg to its cb port notifying it
		    // that it should abort.
		    callback_op_t* cbop = victim_xd->callback();
		    w_assert3(cbop->cb_id >= 0);

		    // The next assertion is wrong because the deadlock can
		    // occur at a coarser level than the cbop->name level.
		    // w_assert3(cbop->name == req->get_lock_head()->name);

		    w_assert3(cbop->blocked);
		    cbop->local_deadlock = TRUE;

		    W_DO(rm->send_deadlock_notification(cbop, self->tid(),
								victim_tid));
#else
		    W_FATAL(eINTERNAL);
#endif /* MULTI_SERVER */
	        }
	    }

        } else {
	    break;
	}

    } while (1);

    w_assert3(self->lock_info()->cycle == 0);
    return RCOK;
}



/*********************************************************************
 *
 *  lock_core_m::_find_cycle(self)
 *
 *********************************************************************/
rc_t 
lock_core_m::_find_cycle(xct_t* self)
{
    xct_t* him;		// him is some xct self is waiting for

    DBGTHRD(<<"_find_cycle(tid " << self->tid()<< ")");

    w_assert3(self->lock_info()->cycle == 0);

    if (self->lock_info()->wait == 0) {
	DBGTHRD(<<"not waiting");
	return RCOK;	// not in deadlock if self is not waiting
    }

    w_assert3(self->lock_info()->wait->status() == lock_m::t_converting ||
		self->lock_info()->wait->status() == lock_m::t_waiting);

    w_list_i<lock_request_t> iter((self->lock_info()->wait)->get_lock_head()->queue);
    lock_request_t* them;	// them is a cursor on the lock queue of me
    
    if (self->lock_info()->wait->status() == lock_m::t_converting)  {
	DBGTHRD(<<"converting");
	/*
	 *  look at everyone that holds the lock
	 */
	mode_t my_mode = self->lock_info()->wait->convert_mode;
	while ((them = iter.next()))  {
	    if (them->xd == self) continue;
	    if (them->status() == lock_m::t_aborted) continue;
	    if (them->status() == lock_m::t_waiting)  {
		break;		// no more granted/converting req
	    }
	    if (!compat[them->mode][my_mode])  {

		him = them->xd;
		self->lock_info()->cycle = him;	// i am waiting for him  

		if (him->lock_info()->cycle)  {

		    TRACE(401, "DEADLOCK DEADLOCK!!!!!");
		    TRACE(401,
			"xct " << self->tid() << " waiting for xct " << him->tid()
			<< " on " << *(self->lock_info()->wait->get_lock_head())
		    );

		    last_waiter = him;
		    return RC(eDEADLOCK);	// he is in the cycle    
		}				                         
		rc_t rc = _find_cycle(him);	// look deeper
		
		if (rc)  {
		    TRACE(401,
                        "xct " << self->tid() << " waiting for xct " << him->tid()
			<< " on " << *(self->lock_info()->wait->get_lock_head())
                    );
		    return rc.reset();
		}
	    }
	}
    } else {
	/*
	 *  look at everyone ahead of me
	 */
	mode_t my_mode = self->lock_info()->wait->mode;
	while ((them = iter.next())) {
	    if (them->xd == self) break;
	    if (them->status() == lock_m::t_aborted) continue;
	    if (!compat[them->mode][my_mode] ||
			them->status() == lock_m::t_waiting ||
			them->status() == lock_m::t_converting)  {

		him = them->xd;
		self->lock_info()->cycle = him;	// i am waiting for him

		if (him->lock_info()->cycle)  {

		    DBGTHRD(<<"DEADLOCK with " << him->tid() );

		    TRACE(401, "DEADLOCK DEADLOCK!!!!!");
                    TRACE(401,
                        "xct " << self->tid() << " waiting for xct " << him->tid()
			<< " on " << *(self->lock_info()->wait->get_lock_head())
                    );

		    last_waiter = him;
		    return RC(eDEADLOCK); 	// he is in the cycle
		} 
		rc_t rc = _find_cycle(him);	// look deeper
		
		if (rc)  {
		     DBGTHRD(<<
                        "xct " << self->tid() << " waiting for xct " << him->tid()
			<< " on " << *(self->lock_info()->wait->get_lock_head())
                    );

		     TRACE(401,
                        "xct " << self->tid() << " waiting for xct " << him->tid()
			<< " on " << *(self->lock_info()->wait->get_lock_head())
                    );
		    return rc.reset();
		}
	    }
	}
    }

    self->lock_info()->cycle = 0;
    return RCOK;
}


/*********************************************************************
 *
 *  lock_core_m::_update_cache(xd, name, mode)
 *
 *********************************************************************/
void
lock_core_m::_update_cache(xct_t *xd, const lockid_t& name, mode_t m)
{
    if (xd->lock_cache_enabled()) {
        if (name.lspace() <= lockid_t::t_page) {
            lock_cache_t<5>* cache = &xd->lock_info()->cache[name.lspace()];
            mode_t* mode = cache->search(name);
            if (mode) *mode = m;
        }
    }
}


/*********************************************************************
 *
 *  lock_core_m::dump()
 *
 *  Dump the lock hash table (for debugging).
 *
 *********************************************************************/
void
lock_core_m::dump()
{
    for (uint h = 0; h < _htabsz; h++)  {
	ACQUIRE(h);
        w_list_i<lock_head_t> i(_htab[h].chain);
        lock_head_t* lock;
        lock = i.next();
	if(lock) {
            cout << h << ": ";
	}
        while (lock)  {
	    MUTEX_ACQUIRE(lock->mutex);
            cout << "\t " << *lock << endl;
            lock_request_t* request;
            w_list_i<lock_request_t> r(lock->queue);
            while ((request = r.next()))  {
                cout << "\t\t" << *request << endl;
            }
	    MUTEX_RELEASE(lock->mutex);
	    lock = i.next();
        }
	RELEASE(h);
    }
}



/*********************************************************************
 *
 *  operator<<(ostream, lock_request)
 *
 *  Pretty print a lock request to "ostream".
 *
 *********************************************************************/
ostream& 
operator<<(ostream& o, const lock_request_t& r)
{
    o << "xct:" << r.xd->tid()
      << " mode:" << lock_base_t::mode_str[r.mode]
      << " cnt:" << r.count
      << " dur:" << lock_base_t::duration_str[r.duration]
      << " stat:";


    switch (r.status()) {
    case lock_m::t_granted:
        o << 'G';
        break;
    case lock_m::t_converting:
        o << 'U' << lock_base_t::mode_str[r.convert_mode];
        break;
    case lock_m::t_waiting:
        o << 'W';
        break;
    case lock_m::t_denied:
        o << 'D';
        break;
	case lock_m::t_aborted:
        o << 'A';
		break;
    default:
        W_FATAL(eINTERNAL);
    }

    return o;
}



/*********************************************************************
 *
 *  operator<<(ostream, lock_head)
 *
 *  Pretty print a lock_head to "ostream".
 *
 *********************************************************************/
ostream& 
operator<<(ostream& o, const lock_head_t& l)
{
    o << l.name << ' ' << lock_base_t::mode_str[l.granted_mode];
    if (l.waiting) o << " W";
    return o;
}



/*********************************************************************
 *
 *  operator<<(ostream, kvl)
 *
 *  Pretty print "kvl" to "ostream".
 *
 *********************************************************************/
ostream& 
operator<<(ostream& o, const kvl_t& kvl)
{
    return o << "k(" << kvl.stid << '.' << kvl.h << '.' << kvl.g << ')';
}




/*********************************************************************
 *
 *  operator>>(istream, kvl)
 *
 *  Read a kvl from istream into "kvl". Format of kvl is a string
 *  of format "k(stid.h.g)".
 *
 *********************************************************************/
istream& 
operator>>(istream& i, kvl_t& kvl)
{
    char c[6];
    i >> c[0] >> c[1] >> kvl.stid >> c[2]
      >> kvl.h >> c[3]
      >> kvl.g >> c[4];
    c[5] = '\0';
    if (i) {
	if (strcmp(c, "k(..)"))  {
	    i.clear(ios::badbit|i.rdstate());  // error
	}
    }
    return i;
}



/*********************************************************************
 *
 *  operator<<(ostream, lockid)
 *
 *  Pretty print a lockid to "ostream".
 *
 *********************************************************************/
ostream& 
operator<<(ostream& o, const lockid_t& i)
{
    o << "L(";
    switch (i.lspace())  {
    case i.t_vol:
	o << * (vid_t*) i.name();
	break;
    case i.t_store:
	o << * (stid_t*) i.name();
	break;
    case i.t_extent:
	o << * (extid_t*) i.name();
	break;
    case i.t_page:
	o << * (lpid_t*) i.name();
	break;
    case i.t_kvl:
	o << * (kvl_t*) i.name();
	break;
    case i.t_record:
	o << * (rid_t*) i.name();
	break;
    default:
	W_FATAL(eINTERNAL);
    }
    return o << ')';
}


extern "C" double sqrt(double);
void		
lock_core_m::stats(
    u_long & buckets_used,
    u_long & max_bucket_len, 
    u_long & min_bucket_len, 
    u_long & mode_bucket_len, 
    float & avg_bucket_len,
    float & var_bucket_len,
    float & std_bucket_len
) const
{
    FUNC(lock_core_m::stats);
    register u_long 	used=0, mn=uint4_max, mx=0, t=0, md=0;
    float   		var = 0.0;
    float		abl = 0.0;
    double 		stddev=0.0;
#ifdef DEBUG_HASH
    int 		collisions=0;
    int			smallest_bucket_used=0, largest_bucket_used=0;
#endif

    bucket_t 	*bk;
    uint 	i,j=0;

    for(i=0; i<_htabsz; i++) {
	bk = &_htab[i];
	j = bk->chain.num_members();
	if(j) {
	    used++;
	    t += j;
	    if(j>mx) mx=j;
	    if(j<mn) mn=j;
	} else {
	    DBG(<<"bucket " << i << " empty");
	}
    }
    if(used>0) {
	// space for computing mode
	u_int	mode[mx+1];

	for(i=0; i<=mx; i++) mode[i]=0;

	// average bucket len
	abl = (float)t/(float)used;

#ifdef DEBUG_HASH
	if(mx==1) {
	    cerr << "NO COLLISIONS: " << endl;
	}
#endif
	// variance
	float f;
	for(i=0; i<_htabsz; i++) {
	    bk = &_htab[i];
	    j = bk->chain.num_members();
	    assert(j <= mx);

#ifdef DEBUG_HASH
	    if(j>0){
		if(i < smallest_bucket_used)
		    smallest_bucket_used=i;
		if (i > largest_bucket_used) 
		    largest_bucket_used=i;
	    } 
	    // if it's the max, let's actually look at the
	    // values that collide
	    if(mx > 1) {
		if(j ==  mx) {
		    w_list_i<lock_head_t> iter( bk->chain );
		    cerr << "COLLISIONS FOR MAX: " << endl;
		    u_long		h;
		    lock_head_t* 	lock;
		    while (lock = iter.next())  {
			h = hash(lock->name);
			cerr << "id " << lock->name 
			    << "->hash->" << ::form("0x%x",h)
			    << "->_hash->" << _hash(h)
			    << "->bucket " << i
			<< endl;
		    }
		}
	    }
#endif

	    // mode -- we aren't counting 0s
	    mode[j]++;
	    if(j) {
		f = (float)j - abl;
		if(f>0.0) {
		    var += f*f; 
		}
	    }
	}
	var = var/used;

	// mode -- we aren't counting 0s
	// only because for small samples,
	// it'll be dominated by 0 and won't
	// tell us much
#ifdef DEBUG_HASH
	cerr << _htabsz - used << " bkts len 0" << endl;
#endif
	uint mdl=0;
	for(i=1; i<=mx; i++) {
	    if(mode[i] > mdl) {
		mdl=mode[i];
		md = i;
	    }
#ifdef DEBUG_HASH
	    collisions += mode[i] * (i-1);
	    if(mode[i]>0) {
		cerr << mode[i] << " bkts len " << i << endl;
		// cerr << "\tcollisions so far=" << collisions << endl;
	    }
#endif
	}

	dassert(var >= 0.0); // could be no variance
		// if all hash to same bucket, or if
		// the distribution is perfectly flat

	// standard deviation
	errno = 0;
	stddev = sqrt((double)var);
	if(errno) {
	    cerr << "cannot compute std dev of " << var << endl;
	    w_assert3(0);
	}
    }
    mode_bucket_len = md;
    buckets_used = used;
    max_bucket_len =mx;
    if(mn==uint4_max) mn=0;
    min_bucket_len =mn;
    avg_bucket_len = abl;
    var_bucket_len = var;
    std_bucket_len = (float) stddev;
#ifdef DEBUG_HASH
    if(stddev > 2.0 ) {
	cerr << "DISMAL "  ;
    } else if(stddev > 1.0 ) {
	cerr << "BAD "  ;
    }else if(stddev > .5 ) {
	cerr << "POOR "  ;
    } else if(stddev > .1 ) {
	cerr << "FAIR " ;
    } else  {
	cerr << "GOOD " ;
    }
    cerr <<"DISTRIBUTION for of " << t << " values -> "
	<< buckets_used << "/" 
	<< _htabsz << " possible values " << endl;
    cerr <<" and " << collisions << " collisions " << endl;
    cerr <<" range of buckets is " << smallest_bucket_used
	<< " -> " << largest_bucket_used << endl;

    {
	// calculate perfection:
	int r,q, m,c;
	r = t % _htabsz;
	q = t / _htabsz;

	if(t<_htabsz) { 
	    q=0;
	} else {
	    c = (q-1)*_htabsz;
	    c += r;
	}

	m = q;
	if(r>0) { 
	    m++;
	}
	if(t<_htabsz) {
	    cerr <<"perfect would have max=" << 1
	    << ", and " << 0 << " collisions"  << endl;
	} else {
	    float pmean = t/((float)_htabsz);

	    cerr <<"perfect would have max=" << m
	    << ", mean= " << pmean
	    << ", stddev= " << ((float)m) - pmean
	    << ", and " << c << " collisions" 
	    << endl;

	}

	cerr << "Algorithm in use is " << HASH_FUNC << endl;
    }
#endif
}

#if HASH_FUNC==1
u_long	
lock_core_m::_hash(u_long id) const
{
    u_char 	c;
    u_long	res=id; 
    const u_char *cp = (const u_char *)&id;
    u_short	s;

    switch(_hashshift) {
	case 0:
	    c = *(cp++);
	    c ^= *(cp++);
	    c ^= *(cp++);
	    c ^= *cp;
	    res = (u_long)c;
	    break;

	case 1:
	    s = *(cp++);
	    s ^= *(cp++)<<BPB;
	    s ^= *(cp++);
	    s ^= *(cp++)<<BPB;
	    res = (u_long)s;
	    break;
	case 2:
	    s = id & 0xffffff00 >> 8;
	    res = (u_long) s;
	    break;
    }
    return	res & _hashmask;
}
#endif

#if HASH_FUNC==2
u_long	
lock_core_m::_hash(u_long id) const
{
    u_long 	res;

    id *= id;
    // pull out some middle bits
    res = id & 0xffffff00 >> 8;
    return	res & _hashmask;
}
#endif

#if HASH_FUNC==3
u_long	
lock_core_m::_hash(u_long id) const
{
    return id % _htabsz;
}
#endif

#if HASH_FUNC==4
u_long	
lock_core_m::_hash(u_long id) const
{
    u_long 	r = id & 0xffff0000 >> 16;
    u_long  res = r * (id & 0xffff);  
	// pull out some middle bits
    return	res & _hashmask;
}
#endif

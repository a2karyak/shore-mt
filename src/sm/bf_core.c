/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994 Computer Sciences Department,          -- */
/* -- University of Wisconsin -- Madison, subject to            -- */
/* -- the terms and conditions given in the file COPYRIGHT.     -- */
/* -- All Rights Reserved.                                      -- */
/* --------------------------------------------------------------- */

#define TRYTHIS
#define TRYTHIS6 0x1

/*
 * $Id: bf_core.c,v 1.18 1996/06/07 22:20:32 nhall Exp $
 */

#ifndef BF_CORE_C
#define BF_CORE_C

#ifdef __GNUG__
#pragma implementation "bf_core.h"
#endif

#include <stdlib.h>
#include "sm_int.h"
#include "bf_s.h"
#include "bf_core.h"
#include "page_s.h"


#ifdef __GNUC__
template class w_list_t<bfcb_t>;
template class w_list_i<bfcb_t>;
template class w_hash_t<bfcb_t, bfpid_t>;
#endif


/*********************************************************************
 *
 *  bf_core_m class static variables
 *
 *      _total_fix      : number of fixed frames
 *      _num_bufs       : number of frames
 *      _bufpool        : array of frames
 *      _buftab         : array of bf control blocks (one per frame)
 *
 *********************************************************************/
unsigned long			bf_core_m::ref_cnt = 0;// # calls to find/grab
unsigned long			bf_core_m::hit_cnt = 0;// # finds w/ find/grab

smutex_t			bf_core_m::_mutex("bf_mutex");

int                             bf_core_m::_total_fix = 0;
int                             bf_core_m::_num_bufs = 0;
page_s*                         bf_core_m::_bufpool = 0;
bfcb_t*                         bf_core_m::_buftab = 0;

w_hash_t<bfcb_t, bfpid_t>*	bf_core_m::_htab = 0;
w_list_t<bfcb_t>*		bf_core_m::_unused = 0;
w_list_t<bfcb_t>*		bf_core_m::_transit = 0;

int				bf_core_m::_hand = 0; // hand of clock
int				bf_core_m::_strategy = 0; 


/*********************************************************************
 *
 *  bf_core_m::bf_core_m(n, extra, desc)
 *
 *  Create the buffer manager data structures and the shared memory
 *  buffer pool. "n" is the size of the buffer_pool (number of frames).
 *  "Desc" is an optional parameter used for naming latches; it is used only
 *  for debugging purposes.
 *
 *********************************************************************/
NORET
bf_core_m::bf_core_m(uint4 n, char *bp, int stratgy, char* desc)
{
    _num_bufs = n;
    _strategy = stratgy;

    _bufpool = (page_s *)bp;
    w_assert1(_bufpool);
    w_assert1(is_aligned(_bufpool));

    _htab = new w_hash_t<bfcb_t, bfpid_t>(2 * n, offsetof(bfcb_t, pid),
						 offsetof(bfcb_t, link));
    if (!_htab) { W_FATAL(eOUTOFMEMORY); }

    _unused = new w_list_t<bfcb_t>(offsetof(bfcb_t, link));
    if (!_unused) { W_FATAL(eOUTOFMEMORY); }

    _transit = new w_list_t<bfcb_t>(offsetof(bfcb_t, link));
    if (!_transit) { W_FATAL(eOUTOFMEMORY); }

    /*
     *  Allocate and initialize array of control info 
     */
    _buftab = new bfcb_t [_num_bufs];
    if (!_buftab) { W_FATAL(eOUTOFMEMORY); }

    for (int i = 0; i < _num_bufs; i++)  {
	_buftab[i].frame = _bufpool + i;
	_buftab[i].dirty = false;
	_buftab[i].pid = lpid_t::null;
	_buftab[i].rec_lsn = lsn_t::null;

	_buftab[i].latch.setname(desc);
	_buftab[i].exit_transit.rename(desc);
	_buftab[i].pin_cnt = 0;

	_buftab[i].refbit = 0;
	_buftab[i].hot = 0;

#if defined(OBJECT_CC) && defined(MULTI_SERVER)
	_buftab[i].remote_io = 0;
	bm_zero(_buftab[i].recmap, max_recs);
	if (desc) _buftab[i].recmap_mutex.rename(desc, "recmap_mx");
#endif
	_unused->append(&_buftab[i]);
    }
}


/*********************************************************************
 *
 *  bf_core_m::~bf_core_m()
 *
 *  Destructor. There should not be any frames pinned when 
 *  bf_core_m is being destroyed.
 *
 *********************************************************************/
NORET
bf_core_m::~bf_core_m()
{
    MUTEX_ACQUIRE(_mutex);
    for (int i = 0; i < _num_bufs; i++) {
	w_assert3(! _in_htab(_buftab + i) );
    }
    while (_unused->pop());
    delete _unused;
    delete _transit;
    delete _htab;

    delete [] _buftab;

    MUTEX_RELEASE(_mutex);

}


/*********************************************************************
 *
 *  bf_core_m::_in_htab(e)
 *
 *  Return true if e is in the hash table.
 *  False otherwise (e is either in _unused list or _transit list).
 *
 *********************************************************************/
bool
bf_core_m::_in_htab(const bfcb_t* e) const
{
    return e->link.member_of() != _unused && e->link.member_of() != _transit;
}


/*********************************************************************
 *
 *  bf_core_m::_in_transit(e)
 *
 *********************************************************************/
bool
bf_core_m::_in_transit(const bfcb_t* e) const
{
    return e->link.member_of() == _transit;
}


/*********************************************************************
 *
 *  bf_core_m::has_frame(p)
 *
 *  Returns true if page "p" is cached or in-transit-in. False otherwise.
 *
 *********************************************************************/
bool
bf_core_m::has_frame(const bfpid_t& p, bfcb_t*& ret)
{
    w_assert3(MUTEX_IS_MINE(_mutex));
    ret = 0;

    bfcb_t* f = _htab->lookup(p);

    if (f)  {
	ret = f;
	return true;
    }

    w_list_i<bfcb_t> i(*_transit);
    while ((f = i.next()))  {
	if (f->pid == p) break;
    }
    if (f) {
	ret = f;
	return true;
    }
    return false;
}


/*********************************************************************
 *
 *  bf_core_m::grab(ret, pid, found, is_new, mode, timeout)
 *
 *  Obtain and latch a frame for the page "pid" to be read in.
 *  The frame is latched in "mode" mode and its control block is 
 *  returned in "ret".
 *
 *********************************************************************/
w_rc_t 
bf_core_m::grab(
    bfcb_t*& 		ret,
    const bfpid_t&	pid,
    bool& 		found,
    bool& 		is_new,
    latch_mode_t 	mode,
    int 		timeout)
{
    w_assert3(cc_alg == t_cc_record || mode > 0);
    // Can't use macros, because we are using functions that assume
    // that the mutex is held, such as
    // exit_transit.wait(), below ...
    //MUTEX_ACQUIRE(_mutex);		// enter monitor

    W_COERCE(_mutex.acquire());
    bfcb_t* p;
  again: 
    {
	ret = 0;
	is_new = false;
	ref_cnt++;
	p = _htab->lookup(pid);
	found = (p != 0);
    
	if (found) {
	    hit_cnt++;
	} else {
	    /*
	     *  need replacement resource ... 
	     *  first check if the resource is in transit. 
	     */
	    {
		w_list_i<bfcb_t> i(*_transit);
		while ((p = i.next()))  {
		    if (p->pid == pid) break;
		    if (p->old_pid_valid && p->old_pid == pid) break;
		}
	    }
	    if (p)  {
		if (! pid.is_remote()) {
		    /*
		     *  in-transit. Wait until it exits transit and retry
		     */
		    W_IGNORE(p->exit_transit.wait( _mutex ) );
		    goto again;
		} else {
#ifdef MULTI_SERVER
		    if (p->old_pid_valid) {
			/*
			 * If p->old_pid == pid then the page is in-transit-out.
			 * If p->old_pid != pid then the page is in-transit-in,
			 * but we have to wait until the victim page is written
			 * out.
			 */
			W_IGNORE(p->exit_transit.wait( _mutex ) );
			 goto again;
		    } else {
			p->pin_cnt++;
			if (mode != LATCH_NL)
			    // should be able to acquire the latch w/o blocking
			    W_COERCE(p->latch.acquire(mode, 0));

			// don't set it if it's already set -- it could be > 1
			// from one of the refbit hints on find() and unpin().
			if( ! p->refbit ) p->refbit = 1;

			ret = p;
		        // see comments at top of function MUTEX_RELEASE(_mutex);
			_mutex.release();

		        return RC(eINTRANSIT);
		    }
#else
		    W_FATAL(eINTERNAL);
#endif /* MULTI_SERVER */
		}
	    }
	    /*
	     *  not-in-transit ...
	     *  find an unused resource or a replacement
	     */
	    is_new = ((p = _unused->pop()) != 0);
	    if (! is_new)   p = _replacement();

	    /*
	     *  prepare the resource to enter in-transit state
	     */
	    p->old_pid = p->pid;
	    p->pid = pid;
	    p->old_pid_valid = !is_new;
#if defined(OBJECT_CC) && defined(MULTI_SERVER)
	    bm_zero(p->recmap, max_recs);
#endif
	    _transit->push(p);
	    w_assert3(p->link.member_of() == _transit);
	    w_assert1(p->pin_cnt == 0);

	} /* end_if (foud) ... */

	/*
	 * don't set refbit if it's already set -- it could
	 * be > 1 from one of the refbit hints on find() and unpin() .
	 */
	if(! p->refbit) p->refbit = 1;

	p->pin_cnt++;

	rc_t rc;
	if (mode != LATCH_NL) rc = p->latch.acquire(mode, 0);

	/* 
	 *  we should be able to acquire the latch if "pid" is not found 
	 */
	w_assert1(found || (!rc));

	/*
	 *  release monitor before we try a blocking latch acquire
	 */
	// see comments at top of function MUTEX_RELEASE(_mutex);
	_mutex.release();

	if (rc && timeout)  rc = p->latch.acquire(mode, timeout);
	if (rc) {
	    /*
	     *  Clean up and bail out.
	     */
	    w_assert1(found);

	    // see comments at top of function MUTEX_ACQUIRE(_mutex);
	    W_COERCE(_mutex.acquire());

	    p->pin_cnt--;
	    w_assert1(p->pin_cnt >= 0);

	    // see comments at top of function MUTEX_RELEASE(_mutex);
	    _mutex.release();

	    return RC_AUGMENT(rc);
	}
    }

    ret = p;
    return RCOK;
}


/*********************************************************************
 *
 *  bf_core_m::find(ret, pid, mode, ref_bit, timeout)
 *
 *  If page "pid" is cached, find() acquires a "mode" latch and returns,
 *  in "ret", a pointer to the associated bf control block; returns an
 *   error if the resource is not cached.
 *
 *********************************************************************/
w_rc_t 
bf_core_m::find(
    bfcb_t*& 		ret,
    const bfpid_t&	pid,
    latch_mode_t 	mode,
    uint4 		ref_bit,
    int 		timeout)
{
    // MUTEX_ACQUIRE(_mutex); can't use macro because
    // we acquire the mutex in all cases

    W_COERCE(_mutex.acquire());
    bfcb_t* p;
  again:
    {
	ret = 0;
	ref_cnt++;
	p = _htab->lookup(pid);
	if (! p) {
	    /* 
	     *  not found ...
	     *  check if the resource is in transit
	     */
	    {
		w_list_i<bfcb_t> i(*_transit);
		while ((p = i.next()))  {
		    if (p->pid == pid) break;
		    if (p->old_pid_valid && p->old_pid == pid) break;
		}
	    }
	    if (p)  {
		if (! pid.is_remote()) {
                    /*
                     *  in-transit. Wait until it exits transit and retry
                     */
                    W_IGNORE(p->exit_transit.wait(_mutex));
                    goto again;
                } else {
#ifdef MULTI_SERVER
                    if (p->old_pid_valid) {
                        /*
                         * see ::grab for the same case
                         */
                        W_IGNORE(p->exit_transit.wait(_mutex));
                        goto again;
                    } else {
                        p->pin_cnt++;
                        if (mode != LATCH_NL)
                            // should be able to acquire the latch w/o blocking
                            W_COERCE(p->latch.acquire(mode, 0));
			if (p->refbit < ref_bit)  p->refbit = ref_bit;
                        ret = p;

			// see comments at beginning
                        // MUTEX_RELEASE(_mutex);
			_mutex.release();

                        return RC(eINTRANSIT);
                    }
#else
		    W_FATAL(eINTERNAL);
#endif /* MULTI_SERVER */
                }
	    }
	    
	    /* give up */
	    // MUTEX_RELEASE(_mutex);
	    _mutex.release();
	    return RC(fcNOTFOUND);
	}

	hit_cnt++;

	if (p->refbit < ref_bit)  p->refbit = ref_bit;
	p->pin_cnt++;

	rc_t rc;
	if (mode != LATCH_NL) rc = p->latch.acquire(mode, 0);

	/*
	 *  release monitor before we try a blocking latch acquire
	 */
	// MUTEX_RELEASE(_mutex);
	_mutex.release();

	if (rc && timeout)  rc = p->latch.acquire(mode, timeout);
	if (rc)  {
	    /*
	     *  Clean up and bail out.
	     */
	    MUTEX_ACQUIRE(_mutex);
	    p->pin_cnt--;
	    w_assert1(p->pin_cnt >= 0);
	    MUTEX_RELEASE(_mutex);
	    return RC_AUGMENT(rc);
	}
    }

    ret = p;
    return RCOK;
}


/*********************************************************************
 *
 *  bf_core_m::publish(p, error_occured)
 *
 *  Publishes the frame "p" that was previously grab() with 
 *  a cache-miss. All threads waiting on the frame are awakened.
 *
 *********************************************************************/
void 
bf_core_m::publish( bfcb_t* p, bool error_occured)
{
    MUTEX_ACQUIRE(_mutex);

    /*
     *  Sanity checks
     */
    w_assert3(p - _buftab >= 0 && p - _buftab < _num_bufs);
    w_assert3(p->pin_cnt > 0);
#ifndef MULTI_SERVER
    w_assert3(p->link.member_of() == _transit);
#endif

    // The next assertion is not valid if pages can be pinned w/o being
    // latched, i.e. in the case of record-level locking
    w_assert3(cc_alg == t_cc_record || p->latch.is_locked());

    w_assert3(!p->old_pid_valid);

    if (p->link.member_of() == _transit) {

        /*
         *  If error, cancel request (i.e. release the latch).
	 *  If there exist other requestors, leave the frame in the transit
	 *  list, otherwise move it to the free list.
	 *  If no error, put the frame into the hash table.
         */
        if (error_occured)  {
	    p->pin_cnt--;
	    w_assert1(p->pin_cnt >= 0);
#if defined(OBJECT_CC) && defined(MULTI_SERVER)
	    if (p->latch.held_by(me())) p->latch.release();
	    w_assert3(p->pin_cnt == 0 || p->remote_io);
#else
	    w_assert3(p->latch.held_by(me()));
	    p->latch.release();
#endif
#ifndef MULTI_SERVER
	    w_assert1(p->pin_cnt == 0);
#endif
	    if (p->pin_cnt == 0) {
		p->link.detach();	// Detach from transit list
		p->clear();
		_unused->push(p);	// Push into the free list
		p->exit_transit.broadcast();
	   }
        } else {
	    p->link.detach();		// Detach from transit list
    	    _htab->push(p);
            /*
             *  Wake up all threads waiting for p to exit transit
             *  All threads waiting on p in grab or find will retry.
             *  Those originally waiting for new-key will now find it cached
             *  while those waiting for old-key will find no traces of it.
             */
            p->exit_transit.broadcast();
	}

    } else {
	w_assert3(_in_htab(p));
	if (error_occured) {
	    p->pin_cnt--;
#if defined(OBJECT_CC) && defined(MULTI_SERVER)
	    if (p->latch.held_by(me())) p->latch.release();
#else
	    w_assert3(p->latch.held_by(me()));
	    p->latch.release();
#endif
	}
    }

    MUTEX_RELEASE(_mutex);
}


/*********************************************************************
 *
 *  bf_core_m::publish_partial(p)
 *
 *  Partially publish the frame "p" that was previously grab() 
 *  with a cache-miss. All threads waiting on the frame are awakened.
 *
 *********************************************************************/
void 
bf_core_m::publish_partial(bfcb_t* p)
{
    MUTEX_ACQUIRE(_mutex);
    w_assert3(p - _buftab >= 0 && p - _buftab < _num_bufs);
    w_assert3(p->link.member_of() == _transit);
#ifndef OBJECT_CC
    // The next assertion is not valid if pages can be pinned w/o being
    // latched. For now, it is ok in the case of page-level locking only.
    w_assert3(p->latch.is_locked()); 
#endif
    w_assert3(p->old_pid_valid);
    /*
     *  invalidate old key
     */
    p->old_pid_valid = false;

    /*
     *  Wake up all threads waiting for p to exit transit.
     *  All threads waiting on p in grab or find will retry.
     *  Those originally waiting for new-key will block in transit
     *  again, while those waiting for old-key will find no traces of it.
     */
    p->exit_transit.broadcast();
    MUTEX_RELEASE(_mutex);
}


/*********************************************************************
 *
 *  bf_core_m::snapshot(npinned, nfree)
 *
 *  Return # frames pinned and # unused frames in "npinned" and
 *  "nfree" respectively.
 *
 *********************************************************************/
void 
bf_core_m::snapshot( u_int& npinned, u_int& nfree)
{
    /*
     *  No need to obtain mutex since this is only an estimate.
     */
    int count = 0;
    for (int i = _num_bufs - 1; i; i--)  {
	if (_in_htab(&_buftab[i]))  {
	    if (_buftab[i].latch.is_locked() || _buftab[i].pin_cnt > 0) ++count;
	} 
    }

    npinned = count;
    nfree = _unused->num_members();
}


/*********************************************************************
 *
 *  bf_core_m::is_mine(p)
 *
 *  Return true if p is latched exclussively by current thread.
 *  False otherwise.
 *
 *********************************************************************/
bool 
bf_core_m::is_mine(const bfcb_t* p)
{
    w_assert3(p - _buftab >= 0 && p - _buftab < _num_bufs);
    w_assert3(_in_htab(p));
    return p->latch.is_mine();
}


/*********************************************************************
 *
 *  bf_core_m::pin(p, mode)
 *
 *  Pin resource "p" in latch "mode".
 *
 *********************************************************************/
void 
bf_core_m::pin(bfcb_t* p, latch_mode_t mode)
{
    MUTEX_ACQUIRE(_mutex);
    w_assert3(p - _buftab >= 0 && p - _buftab < _num_bufs);
    w_assert3(_in_htab(p));
    p->pin_cnt++;
    MUTEX_RELEASE(_mutex);

    if (mode != LATCH_NL) W_COERCE( p->latch.acquire(mode) );
}


/*********************************************************************
 *
 *  bf_core_m::upgrade_latch_if_not_block(p, would_block)
 *
 *********************************************************************/
void 
bf_core_m::upgrade_latch_if_not_block(bfcb_t* p, bool& would_block)
{
    MUTEX_ACQUIRE(_mutex);
    w_assert3(p - _buftab >= 0 && p - _buftab < _num_bufs);
    w_assert3(_in_htab(p));
    // p->pin_cnt++;	// DO NOT Increment!!
    MUTEX_RELEASE(_mutex);

    W_COERCE( p->latch.upgrade_if_not_block(would_block) );
}


/*********************************************************************
 *
 *  bf_core_m::pin_cnt(p)
 *
 *  Returns the pin count of resource "p".
 *
 *********************************************************************/
int
bf_core_m::pin_cnt(const bfcb_t* p)
{
    w_assert3(p - _buftab >= 0 && p - _buftab < _num_bufs);
    w_assert3(_in_htab(p));
    return p->pin_cnt;
}


/*********************************************************************
 *
 *  bf_core_m::unpin(p, int ref_bit)
 *
 *  Unlatch the frame "p". 
 *
 *********************************************************************/
#ifndef W_DEBUG
#define in_htab /* in_htab not used */
#endif
void
bf_core_m::unpin(bfcb_t*& p, uint ref_bit, bool in_htab)
#undef in_htab
{
    MUTEX_ACQUIRE(_mutex);

    w_assert3(p - _buftab >= 0 && p - _buftab < _num_bufs);
    w_assert3(!in_htab || _in_htab(p));

    /*  
     * if we were given a hit about the page's
     * about-to-be-referenced-ness, apply it.
     * but o.w., don't make it referenced. (That
     * shouldn't happen in unfix().)
     */
    if (p->refbit < ref_bit)  p->refbit = ref_bit;


    // The following code used to be included to get the page reused
    // sooner.  However, performance tests show that that this can
    // cause recently referenced pages to be "swept" early
    // by the clock hand.
    /*
    if (ref_bit == 0) {
	_hand = p - _buftab;  // reset hand for MRU
    }
    */

    p->pin_cnt--;
    w_assert1(p->pin_cnt >= 0);

#if defined(OBJECT_CC) && defined(MULTI_SERVER)
    if (p->latch.held_by(me())) p->latch.release();
#else
    w_assert3(p->latch.held_by(me()));
    p->latch.release();
#endif

    w_assert3(p->pin_cnt > 0 || p->latch.lock_cnt() == 0);
    MUTEX_RELEASE(_mutex);

    // prevent future use of p
    p = 0;
}


/*********************************************************************
 *
 *  bf_core_m::_remove(p)
 *
 *  Remove frame "p" from hash table. Insert into unused list.
 *  Called from ::remove while _mutex is held.
 *
 *********************************************************************/
rc_t 
bf_core_m::_remove(bfcb_t*& p)
{
    w_assert3(p - _buftab >= 0 && p - _buftab < _num_bufs);
    w_assert3(_in_htab(p));
    if (p->pin_cnt != 1)  W_FATAL(fcINTERNAL);
    if (p->latch.is_hot())  W_FATAL(fcINTERNAL);
    w_assert3(p->latch.is_mine());
    w_assert3(p->latch.lock_cnt() == 1);
    p->pin_cnt = 0;
    _htab->remove(p);
    p->latch.release();
    p->clear();
    _unused->push(p);
    p = NULL;

    return RCOK;
}


/*********************************************************************
 *
 *  bf_core_m::_replacement()
 *
 *  Find a replacement resource.
 *  Called from ::grab while _mutex is held.
 *
 *********************************************************************/
bfcb_t* 
bf_core_m::_replacement()
{
    /*
     *  Use the clock algorithm to find replacement resource.
     */
    register bfcb_t* p;
    int start = _hand, rounds = 0;
    int i;
    for (i = start; ; i++)  {

	if (i == _num_bufs) {
	    i = 0;
	}
	if (i == start && ++rounds == 4)  {
	    cerr << "bf_core_m: cannot find free resource" << endl;
	    cerr << *this;
	    W_FATAL(fcFULL);
	}

	/*
	 *  p is current entry.
	 */
	p = _buftab + i;
	if (! _in_htab(p))  {
	    // p could be in transit
	    continue;
	}
	/*
	 * On the first partial-round, consider only clean pages.
	 * After that, dirty ones are ok.
	 */
	if (
#ifdef TRYTHIS
#ifdef NOTDEF
	    // After round 1 is done, dirty pages 
	    // are considered
	    (((_strategy & TRYTHIS6) && 
#endif
	    (rounds > 1 || !p->dirty)
#ifdef NOTDEF
	    )||
	    ((_strategy & TRYTHIS6)==0)) 
#endif
	    &&

#endif 
	   // don't want to replace a hot page
	   (!p->refbit && !p->hot && !p->pin_cnt && !p->latch.is_locked()) )  {
	    /*
	     *  Found one!
	     */
	    break;
	}
	
	/*
	 *  Unsuccessful. Decrement ref count. Try next entry.
	 */
	if (p->refbit>0) p->refbit--;
    }
    w_assert3( _in_htab(p) );

    /*
     *  Remove from hash table.
     */
    _htab->remove(p);

    /*
     *  Update clock hash.
     */
    _hand = (i+1 == _num_bufs) ? 0 : i+1;
    return p;
}


/*********************************************************************
 *
 *  bf_core_m::audit()
 *
 *  Check invarients for integrity.
 *
 *********************************************************************/
int 
bf_core_m::audit() const
{
#ifdef DEBUG
    int total_locks=0 ;

    for (int i = 0; i < _num_bufs; i++)  {
	bfcb_t* p = _buftab + i;
	if (_in_htab(p))  {
		
	    if(p->latch.is_locked()) { 
		 w_assert3(p->latch.lock_cnt()>0);
	    } else {
		 w_assert3(p->latch.lock_cnt()==0);
	    }
	    total_locks += p->latch.lock_cnt() ;
	}
    }
    DBG(<< "end of bf_core_m::audit");
    return total_locks;
#else 
	return 0;
#endif
}


/*********************************************************************
 *
 *  bf_core_m::dump(ostream, debugging)
 *
 *  Dump content to ostream. If "debugging" is true, print
 *  synchronization info as well.
 *
 *********************************************************************/
void
bf_core_m::dump(ostream &o, bool /*debugging*/)const
{
    int n = 0;

    o << "pid" << '\t' << '\t' << "dirty?" << '\t' << "rec_lsn" << '\t'
      << "pin_cnt" << '\t' << "l_mode" << '\t'
      << "l_cnt" << '\t' << "l_hot" << '\t' << "l_id" << '\t'
      << "remote_io" << endl << flush;

    for (int i = 0; i < _num_bufs; i++)  {
        bfcb_t* p = _buftab + i;
        if (_in_htab(p))  {
	    n++;
	    p->print_frame(o, p->frame->nslots, TRUE);
        } else if (_in_transit(p)) {
	   p->print_frame(o, p->frame->nslots, FALSE);
	}
    }
    o << "total number of frames in the hash table: " << n << endl;
    o << "number of buffers: " << _num_bufs << endl;
    o << "total_fix: " << _total_fix<< endl;
    o <<endl<<flush;
}

ostream &
operator<<(ostream& out, const bf_core_m& mgr)
{
    mgr.dump(out, 0);
    return out;
}


bf_core_i::~bf_core_i()
{
    if (_curr)  {
	bfcb_t* tmp = _curr;
	_r.unpin(tmp);  // unpin will null the pointer
    }
}


bfcb_t*
bf_core_i::next()
{
//    _r._mutex.acquire();

    if (_curr) { 
	bfcb_t* tmp = _curr;
	_r.unpin(tmp);  // unpin will null the pointer
	_curr = 0; 
    }
    while( _idx < _r._num_bufs ) {
	bfcb_t* p = &_r._buftab[_idx++];
	if (_r._in_htab(p)) {
	    _r.pin(p, _mode);
	    w_assert3(_r._in_htab(p));
	    _curr = p;
	    break;
	}
    }
//    _r._mutex.release();
    return _curr;
}


rc_t
bf_core_i::discard_curr()
{
    w_assert3(_curr);
    bfcb_t* tmp = _curr;
    W_DO( _r.remove(tmp) );
    w_assert3(_curr);
    _curr = 0;
    return RCOK;
}

#endif /* BF_CORE_C */

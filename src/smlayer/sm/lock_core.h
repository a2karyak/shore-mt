/*<std-header orig-src='shore' incl-file-exclusion='LOCK_CORE_H'>

 $Id: lock_core.h,v 1.39 2001/06/26 18:33:55 bolo Exp $

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

#ifndef LOCK_CORE_H
#define LOCK_CORE_H

#include "w_defines.h"

/*  -- do not edit anything above this line --   </std-header>*/

#ifdef __GNUG__
#pragma interface
#endif


class LockCoreFunc
{
    public:
	virtual void operator()(const xct_t* xct) = 0;
};


class bucket_t; // forward
class lock_core_m : public lock_base_t {
    enum { BPB=CHAR_BIT };

friend class callback_m;

public:
    typedef lock_base_t::mode_t mode_t;
    typedef lock_base_t::duration_t duration_t;

    NORET		lock_core_m(uint sz);
    NORET		~lock_core_m();

    int   		collect(vtable_info_array_t&);

#ifdef EXPENSIVE_LOCK_STATS
    void		stats(
			    u_long & buckets_used,
			    u_long & max_bucket_len, 
			    u_long & min_bucket_len, 
			    u_long & mode_bucket_len, 
			    float & avg_bucket_len,
			    float & var_bucket_len,
			    float & std_bucket_len
			    ) const;
#endif /* EXPENSIVE_LOCK_STATS */


    void		dump(ostream &o);
    void		_dump(ostream &o);
    void		ForEachLockGranteeXctWithoutMutexes(
				const lockid_t&			n,
				LockCoreFunc&			f);

    lock_head_t*	find_lock(
				const lockid_t&			n,
				bool				create);
    lock_head_t*	find_lock(
				w_list_t<lock_head_t>&		l,
				const lockid_t&			n);
    lock_request_t*	find_req(
				w_list_t<lock_request_t>&	l,
				const xct_t*			xd);
    lock_request_t*	find_req(
				w_list_t<lock_request_t>&	l,
				const tid_t&			tid);

    rc_t		acquire(
				xct_t*			xd,
				const lockid_t&		name,
				lock_head_t*		lock,
				lock_request_t**	request,
				mode_t			mode,
				mode_t&			prev_mode,
				duration_t		duration,
				timeout_in_ms		timeout,
				mode_t&			ret);

#ifdef NOTDEF
    rc_t		downgrade(
				xct_t*			xd,
				const lockid_t&		name,
				lock_head_t*		lock,
				lock_request_t*		request,
				mode_t			mode,
				bool			force);
#endif

    rc_t		release(
				xct_t*			xd,
				const lockid_t&		name,
				lock_head_t*		lock,
				lock_request_t*		request,
				bool			force);

    void		wakeup_waiters(lock_head_t*& lock);

    bool		upgrade_ext_req_to_EX_if_should_free(
				lock_request_t*		req);

    rc_t		release_duration(
				xct_t*			xd,
				duration_t		duration,
				bool			all_less_than,
				extid_t*		ext_to_free);

    rc_t		open_quark(xct_t*		xd);
    rc_t		close_quark(
				xct_t*			xd,
				bool			release_locks);

    lock_head_t*	GetNewLockHeadFromPool(
				const lockid_t&		name,
				mode_t			mode);
    
    void		FreeLockHeadToPool(lock_head_t* theLockHead);

private:
    uint4_t		deadlock_check_id;
    uint4_t		_hash(uint4_t) const;
    rc_t	_check_deadlock(xct_t* xd, bool* deadlock_found = 0);
    rc_t	_find_cycle(xct_t* self);
    void	_update_cache(xct_t *xd, const lockid_t& name, mode_t m);

#ifndef NOT_PREEMPTIVE
#define ONE_MUTEX
#ifdef ONE_MUTEX
    smutex_t 		    mutex;
#endif
#endif


    bucket_t* 			_htab;
    uint4_t			_htabsz;
    uint4_t			_hashmask;
    uint4_t			_hashshift;

    w_list_t<lock_head_t>	lockHeadPool;
#ifndef ONE_MUTEX
    smutex_t			lockHeadPoolMutex("lockpool");
#endif

    int				_requests_allocated;
};


#ifdef ONE_MUTEX
#define ACQUIRE(i) MUTEX_ACQUIRE(mutex);
#define RELEASE(i) MUTEX_RELEASE(mutex);
#define IS_MINE(i) w_assert3(MUTEX_IS_MINE(mutex));
#define LOCK_HEAD_POOL_ACQUIRE(mutex)  /* do nothing */
#define LOCK_HEAD_POOL_RELEASE(mutex)  /* do nothing */
#else
#define ACQUIRE(i) MUTEX_ACQUIRE(_htab[i].mutex);
#define RELEASE(i) MUTEX_RELEASE(_htab[i].mutex);
#define IS_MINE(i) w_assert3(MUTEX_IS_MINE(_htab[i].mutex));
#define LOCK_HEAD_POOL_ACQUIRE(mutex)  MUTEX_ACQUIRE(mutex)
#define LOCK_HEAD_POOL_RELEASE(mutex)  MUTEX_ACQUIRE(mutex)
#endif


inline lock_head_t*
lock_core_m::GetNewLockHeadFromPool(const lockid_t& name, mode_t mode)
{
    LOCK_HEAD_POOL_ACQUIRE(lockHeadPoolMutex);

    lock_head_t*	result;
    if ((result = lockHeadPool.pop()))  {
	result->name = name;
	result->granted_mode = mode;
    }  else  {
	result = new lock_head_t(name, mode);
    }

    LOCK_HEAD_POOL_RELEASE(lockHeadPoolMutex);

    return result;
}


inline void
lock_core_m::FreeLockHeadToPool(lock_head_t* theLockHead)
{
    LOCK_HEAD_POOL_ACQUIRE(lockHeadPoolMutex);

    theLockHead->chain.detach();
    lockHeadPool.push(theLockHead);

    LOCK_HEAD_POOL_RELEASE(lockHeadPoolMutex);
}


inline lock_head_t*
lock_core_m::find_lock(w_list_t<lock_head_t>& l, const lockid_t& n)
{
    lock_head_t* lock;
    w_list_i<lock_head_t> iter(l);
    while ((lock = iter.next()) && lock->name != n) ;
    return lock;
}


inline lock_request_t*
lock_core_m::find_req(w_list_t<lock_request_t>& l, const xct_t* xd)
{
    lock_request_t* request;
    w_list_i<lock_request_t> iter(l);
    while ((request = iter.next()) && request->xd != xd);
    return request;
}


inline lock_request_t*
lock_core_m::find_req(w_list_t<lock_request_t>& l, const tid_t& tid)
{
    lock_request_t* request;
    w_list_i<lock_request_t> iter(l);
    while ((request = iter.next()) && request->xd->tid() != tid);
    return request;
}

/*<std-footer incl-file-exclusion='LOCK_CORE_H'>  -- do not edit anything below this line -- */

#endif          /*</std-footer>*/

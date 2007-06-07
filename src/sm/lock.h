/*<std-header orig-src='shore' incl-file-exclusion='LOCK_H'>

 $Id: lock.h,v 1.62 1999/06/07 19:04:10 kupsch Exp $

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

#ifndef LOCK_H
#define LOCK_H

#include "w_defines.h"

/*  -- do not edit anything above this line --   </std-header>*/

#include <kvl_t.h>
#include <lock_s.h>

class xct_lock_info_t;
class lock_core_m;
class GatherThreadWaitFors;

#ifdef __GNUG__
#pragma interface
#endif

class lock_m : public lock_base_t {
friend class callback_m;
friend class remote_lock_m;
friend class GatherThreadWaitFors;
public:

    typedef lock_base_t::mode_t mode_t;
    typedef lock_base_t::duration_t duration_t;
    typedef lock_base_t::status_t status_t;

    NORET			lock_m(int sz);
    NORET			~lock_m();

    int   			collect(vtable_info_array_t&);
    void			dump(ostream &o);

    void			stats(
				    u_long & buckets_used,
				    u_long & max_bucket_len, 
				    u_long & min_bucket_len, 
				    u_long & mode_bucket_len, 
				    float & avg_bucket_len,
				    float & var_bucket_len,
				    float & std_bucket_len
				    ) const;

    static const mode_t 	parent_mode[NUM_MODES];

    bool                      	get_parent(const lockid_t& c, lockid_t& p);

    rc_t			lock(
	const lockid_t& 	    n, 
	mode_t 			    m,
	duration_t 		    duration = t_long,
	timeout_in_ms		    timeout = WAIT_SPECIFIED_BY_XCT,
	mode_t*			    prev_mode = 0,
	mode_t*			    prev_pgmode = 0,
	lockid_t**		    nameInLockHead = 0);
     
    rc_t			lock_force(
	const lockid_t& 	    n,
	mode_t 			    m,
	duration_t 		    duration = t_long,
	timeout_in_ms		    timeout = WAIT_SPECIFIED_BY_XCT,
	mode_t*			    prev_mode = 0,
	mode_t*			    prev_pgmode = 0,
	lockid_t**		    nameInLockHead = 0);

    rc_t			unlock(const lockid_t& n);

    rc_t			unlock_duration(
	duration_t 		    duration,
	bool 			    all_less_than,
	bool			    dont_clean_exts = false);
    
    rc_t			dont_escalate(
	const lockid_t&		    n,
	bool			    passOnToDescendants = true);

    rc_t			query(
	const lockid_t& 	    n, 
	mode_t& 		    m, 
	const tid_t& 		    tid = tid_t::null,
	bool			    implicit = false);
   
   rc_t				query_lockers(
	const lockid_t&		    n,
	int&			    numlockers,
	locker_mode_t*&		    lockers);

    lock_core_m*		core() const { return _core; }

    static void	  		lock_stats(
	u_long& 		    locks,
	u_long& 		    acquires,
	u_long& 		    cache_hits, 
	u_long& 		    unlocks,
	bool 			    reset);
    
    static rc_t			open_quark();
    static rc_t			close_quark(bool release_locks);


private:
    rc_t			_lock(
	const lockid_t& 	    n, 
	mode_t 			    m,
	mode_t&			    prev_mode,
	mode_t&			    prev_pgmode,
	duration_t 		    duration,
	timeout_in_ms		    timeout,
	bool 			    force,
	lockid_t**		    nameInLockHead);

    rc_t			_query_implicit(
	const lockid_t&		    n,
	mode_t&			    m,
	const tid_t&		    tid);

    lock_core_m* _core;
    friend class lock_query_i;
};


inline bool is_valid(lock_base_t::mode_t m)
{
    return ((int(m)==lock_base_t::MIN_MODE || 
	int(m) > lock_base_t::MIN_MODE) &&
	int(m) <= lock_base_t::MAX_MODE);
}

/*<std-footer incl-file-exclusion='LOCK_H'>  -- do not edit anything below this line -- */

#endif          /*</std-footer>*/

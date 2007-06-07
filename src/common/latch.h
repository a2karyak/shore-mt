/*<std-header orig-src='shore' incl-file-exclusion='LATCH_H'>

 $Id: latch.h,v 1.31 2004/10/05 23:19:52 bolo Exp $

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

#ifndef LATCH_H
#define LATCH_H

#include "w_defines.h"

/*  -- do not edit anything above this line --   </std-header>*/

#ifndef STHREAD_H
#include <sthread.h>
#endif

#ifdef __GNUG__
#pragma interface
#endif

enum latch_mode_t { LATCH_NL = 0, LATCH_SH = 1, LATCH_EX = 2 };

/*

   Class latch_t provides shared and exclusive latches.  The code is
   optimized for the case of a single thread acquiring an exclusive
   latch (especially one that is not held).  Up to 4 (max_sh) threads
   can hold a shared latch at one time.  If more try, they are made to
   wait.
*/

/*
   A latch may be acquire()d multiple times by a single thread.  The
   mode of subsequent acquire()s must be at or above the level of the
   currently held latch.  Each of these individual locks must be
   released.

   The upgrade_if_not_block() method allows a thread to convert a
   shared latch into an exlusive latch.  The number of times the
   thread holds the latch is NOT changed.

   The following two sequences illustrate the difference.
   acquire(share)		acquire(share)
     acquire(exclusive)		  upgrade_if_not_block()
     // two latches		  // one latch
    release(exclusive)		release()
   release(share)
*/   

#include <iosfwd>

class latch_t : public sthread_named_base_t {

public:
    NORET			latch_t(const char* const desc = 0);
    NORET			~latch_t();

    ostream			&print(ostream &) const;

    inline const void *		id() const { return &_waiters; } // id used for resource tracing
    inline void 		setname(const char *const desc);
    w_rc_t			acquire(
	latch_mode_t 		    m, 
	sthread_t::timeout_in_ms    timeout = sthread_base_t::WAIT_FOREVER);
    w_rc_t			upgrade_if_not_block(
	bool& 			    would_block);
    void   			release();
    bool 			is_locked() const;
    bool 			is_hot() const;
    int    			lock_cnt() const;
    int				num_holders() const;
    int				held_by(const sthread_t* t) const;
    bool 			is_mine() const;
    latch_mode_t		mode() const;

    const sthread_t * 		holder() const;

    /* Maximum number of threads that can hold a share latch */
#ifdef SM_LATCH_SHARE_MAX
    enum { max_sh = SM_LATCH_SHARE_MAX };
#else
    enum { max_sh = 4 };
#endif

    static const char* const    latch_mode_str[3];

private:
    smutex_t			_mutex;
    latch_mode_t		_mode;		    // highest mode held

    // slots for each holding thread
    uint2_t			_cnt[max_sh];	    // number of times held
    sthread_t*			_holder[max_sh];    // thread holding the latch

    scond_t			_waiters;
    
    // CONSTRAINTS:
    // Slots must be filled in order.  If slot i is empty then
    // the remaining slots must be empty.
    // This makes it's easy to see if the latch is held and
    // speeds counting the number of holders.

    uint2_t		 	_held_by(const sthread_t*, int& num_holders);
    uint2_t 			_first_free_slot();
    void			_fill_slot(uint2_t slot); // fill hole

    // disabled
    NORET			latch_t(const latch_t&);
    latch_t&   			operator=(const latch_t&);
};

extern ostream &operator<<(ostream &, const latch_t &);

inline void
latch_t::setname(const char* const desc)
{
    rename("l:", desc);
    _mutex.rename("l:m:", desc);
    _waiters.rename("l:c:", desc);
}

inline bool
latch_t::is_locked() const
{
    return _cnt[0] != 0; 
}

inline bool
latch_t::is_hot() const
{
    return _waiters.is_hot(); 
}

/* XXX this should be protected with a mutex.  It will
   work with non-preepmtive threads,  but ...  FIX IT. */

inline bool 
latch_t::is_mine() const
{
    return _mode == LATCH_EX && _holder[0] == sthread_t::me();
}

inline const sthread_t * 
latch_t::holder() const
{
    return _holder[0];
}

inline latch_mode_t
latch_t::mode() const
{
    return _mode;
}

/*<std-footer incl-file-exclusion='LATCH_H'>  -- do not edit anything below this line -- */

#endif          /*</std-footer>*/

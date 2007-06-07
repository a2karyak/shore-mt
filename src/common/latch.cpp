/*<std-header orig-src='shore'>

 $Id: latch.cpp,v 1.40 2002/11/14 02:07:11 bolo Exp $

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

#include "w_defines.h"

/*  -- do not edit anything above this line --   </std-header>*/

#ifdef __GNUC__
#pragma implementation
#endif

#include "sthread.h"
#include "latch.h"
#include "w_debug.h"

#include <memory.h>
#include <w_statistics.h>
#include <sthread_stats.h>



const char* const  latch_t::latch_mode_str[3] = { "NL", "SH", "EX" };


latch_t::latch_t(const char* const desc)
: _mode(LATCH_NL)
{
    setname(desc);

    unsigned i;
    for (i = 0; i < max_sh; i++) {
	    _cnt[i] = 0;
	    _holder[i] = 0;
    }
}


latch_t::~latch_t()
{
	unsigned	errors = 0;

	for (unsigned i = 0; i < max_sh; i++) {
		if (!_holder[i])
			continue;
		errors++;
		if (errors == 1)
			cerr << "Latch busy at destruction:" << endl
				<< *this << endl;

		cerr << "\tslot[" << i << "]  holds thread:" << endl
			<< *_holder[i] << endl;
	}
	if (errors)
		W_FATAL(fcINTERNAL);
}


w_rc_t
latch_t::acquire(latch_mode_t m, sthread_t::timeout_in_ms timeout)
{
    FUNC(latch_t::acquire);
    w_assert3(m != LATCH_NL);
    uint2_t h = 0;	// index into _holder and _cnt
    W_COERCE(_mutex.acquire());
    sthread_t* self = sthread_t::me();

    DBGTHRD(<< "want to acquire in mode " << W_ENUM(m) << *this);

    // note that if no thread holds the latch, then the while is skipped
    while (_cnt[0]) {  // while some thread holds a latch
	if (_mode == LATCH_SH) {
	    int	num_holders;
	    h = _held_by(self, num_holders);
	    if (h < max_sh) {
		// we hold the share latch already

		if ( (m == LATCH_SH) || (num_holders == 1)) {
		    // always allow reaquiring the share latch
		    // and allow acquiring the EX latch if this thread
		    // is the only holder
		    break; // finish getting the latch
		}
		// fall through an wait
	    } else {
		// we don't already hold the latch

		if (_waiters.is_hot() || num_holders == max_sh) {
		    // other waiters || max shared holders
		    // so fall through and wait
		} else {
		    if ( (m == LATCH_SH) || (num_holders == 0)) {
			// first free slot must not be zero
			// since already held by another thread	
			w_assert3(_first_free_slot() != 0);
			h = _first_free_slot();
			break; // finish getting the latch
		    } 
		    // fall through an wait
		}
	    }
	} else {
	    // the latch is already held in EX mode
	    w_assert3(_mode == LATCH_EX);
	    if (_holder[0] == self)  {
		// we hold it, so get it again
		m = LATCH_EX;  	// once EX is held, all reacquires
				// must be EX also
		h = 0;
		break;
	    }
	    // else we don't hold it, so fall through an wait
	}
	DBGTHRD(<<"about to await " << *this);

	w_assert3(num_holders() > 0);
	w_assert3(_cnt[0] > 0);
	SthreadStats.latch_wait++;

#if 0
	// temporary for debugging
	cerr << "thread " 
	     << self->name() 
	     << " awaits latch with mutex " << _mutex.name() << endl;
#endif

	w_rc_t rc = _waiters.wait(_mutex, timeout);
	if (rc)  {
	    w_assert1(rc.err_num() == sthread_t::stTIMEOUT);
	    _mutex.release();
	    if(timeout > 0) {
		SthreadStats.latch_time += timeout;
	    }
	    return RC_AUGMENT(rc);
	}
	h = 0;
    }

    w_assert1(h < max_sh);

    _mode = m;
    _holder[h] = self;
#if defined(W_DEBUG) || defined(SHORE_TRACE)
    // us the address of the condition variable so that it matches
    // the id that shows up in "blocked on..." in a thread dump
    // if some thread is blocked on the variable
    self->push_resource_alloc(_mutex.name(), id());
#endif
    _cnt[h]++;
    _mutex.release();

#if defined(W_DEBUG) || defined(SHORE_TRACE)
    self->push_resource_alloc(name(), (void *)this, true);
#endif

    DBGTHRD(<< "acquired " << *this);

    return RCOK;
}

w_rc_t
latch_t::upgrade_if_not_block(bool& would_block)
{
    FUNC(latch_t::upgrade_if_not_block);
    w_assert3(_mode >= LATCH_SH);
    uint2_t h = 0;	// index into _holder and _cnt
    W_COERCE(_mutex.acquire());
    sthread_t* self = sthread_t::me();

    DBGTHRD(<< "want to upgrade " << *this );

    w_assert3(_cnt[0]);  // someone (we) must hold the latch
    int	num_holders;
    h = _held_by(self, num_holders);
    if (h < max_sh && num_holders == 1) {
	DBGTHRD(<< "we hold the latch already and are the only holders");
        would_block = false;

	// establish the EX latch
	_mode = LATCH_EX;
	w_assert3(_holder[h] == self);
    } else {
	DBGTHRD(<< "would_block");
	would_block = true;
    } 

    _mutex.release();

    return RCOK;
}

void 
latch_t::release()
{
    FUNC(latch_t::release);
    W_COERCE(_mutex.acquire());
    sthread_t* self = sthread_t::me();

    DBGTHRD(<< "want to release " << *this );

    // check for the common case of being the only holder
    if (_holder[0] == self) {
	_cnt[0]--;
#if defined(W_DEBUG) || defined(SHORE_TRACE)
	self->pop_resource_alloc(id());
#endif
	if (_cnt[0] == 0) {
	    _holder[0] = 0;
	    if ( _cnt[1] != 0) {
		// we are releasing and we are not the only holder
		w_assert3(_mode == LATCH_SH);
		_fill_slot(0); 	// move other shared holder to slot 0
	    } else {
		_mode = LATCH_NL;
	    }
	    DBGTHRD(<< "releasing latch:" << this );
	    _waiters.broadcast();
	} else {
	    DBGTHRD(<< "Did not release latch:" << this );
	}
    } else {
	// handle the general case
	uint2_t h;	// index into _holder and _cnt
	int	  num_holders;
	h = _held_by(self, num_holders);
	if (h == max_sh) {
		cerr << "ERROR: attempting to release a latch not owned"
		     << " by thread " << self << endl
		     << *this << endl;
	}
	w_assert1(h < max_sh);
	w_assert3(_cnt[h] > 0);
	w_assert3(num_holders > 1);
	DBGTHRD( << "h=" << h << ", num_holders=" <<num_holders);
	_cnt[h]--;
#if defined(W_DEBUG) || defined(SHORE_TRACE)
	self->pop_resource_alloc(id());
#endif
	if (_cnt[h] == 0) {
	    _holder[h] = 0;
	    if (num_holders > 1 ) {
		w_assert3(_mode == LATCH_SH);
		_fill_slot(h); 	// move other shared holder to slot 0
				    // to maintain constraint in latch_t	
	    }
	    DBGTHRD(<< "releasing latch:" << this);
	    _waiters.broadcast();
	} else {
	    DBGTHRD(<< "Did not release " << *this );
	    DBGTHRD(<< " h=" << h << ", _cnt[h]=" << _cnt[h]);
	}
    }
    _mutex.release();
    DBGTHRD(<< "exiting latch::release " << *this);

#if defined(W_DEBUG) || defined(SHORE_TRACE)
    self->pop_resource_alloc((void *)this);
#endif
}

int latch_t::lock_cnt() const
{
    if (_mode == LATCH_EX) return _cnt[0];

    int total = _cnt[0];
    for (uint2_t i = 1; i < max_sh; i++) total += _cnt[i];
    return total;
}

int latch_t::num_holders() const
{
    if (_mode == LATCH_EX) return 1;

    int total = _cnt[0];
    for (uint2_t i = 1; i < max_sh; i++)
	if (_cnt[i] > 0) total++;
    return total;
}

// return the number of times the latch is held by the "t" thread,
// or 0 if "t" does not hold the latch
int
latch_t::held_by(const sthread_t* t) const
{
    for (uint2_t i = 0; i < max_sh; i++) {
	if (_holder[i] == t) {
	    w_assert3(_cnt[i] > 0);
	    return _cnt[i];
	}
    }
    return 0;
}

// return slot if held by t and the numbers of threads holding the latch
w_base_t::uint2_t 
latch_t::_held_by(const sthread_t* t, int& num_holders)
{
    uint2_t found = max_sh;			// t not found yet
    num_holders = 0;

    for (uint2_t i = 0; i < max_sh; i++) {
	if (_cnt[i] != 0) {
	    num_holders++;
	    if (_holder[i] == t) found = i;
	} else {
	    // _cnt[i] == 0 so there are no more holders
	    break;
	}
    }
    return found;
}

// return first slot with count == 0, else return max_sh 
w_base_t::uint2_t 
latch_t::_first_free_slot()
{
    uint2_t i;
    for (i = 0; i < max_sh; i++) {
	if (_cnt[i] == 0) break;
    }
    return i;
}

// move one slot (slot > hole) into hole to maintain constraints
void 
latch_t::_fill_slot(uint2_t hole)
{
    w_assert3(_cnt[hole] == 0 && _holder[hole] == 0);
    
    // check for common case where there is no other holder
    if (_cnt[hole+1] == 0) return;

    // find largest slot with holder and move it to hole
    for (uint2_t i = max_sh-1; i > hole; i--) {
	if (_cnt[i] > 0) {
	    _cnt[hole] = _cnt[i];
	    _holder[hole] = _holder[i];
	    _cnt[i] = 0;
	    _holder[i] = 0;
	    break;
	}
    }
}


ostream &latch_t::print(ostream &out) const
{
	register int i;
	sthread_t	*t;

	out <<	"latch(" << this << "): " << name();

	for(i=0; i< max_sh; i++) {
	    out << "\t_cnt[" << i << "]=" << _cnt[i];
	    if ((t = _holder[i])) {
		out << "\t_holder[" << i << "]=" << t->id << endl;
	    }
	}
	return out;
}


ostream& operator<<(ostream& out, const latch_t& l)
{
	return l.print(out);
}


/*<std-header orig-src='shore' incl-file-exclusion='PROLOGUE_H'>

 $Id: prologue.h,v 1.46 1999/08/20 18:29:02 nhall Exp $

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

#ifndef PROLOGUE_H
#define PROLOGUE_H

#include "w_defines.h"

/*  -- do not edit anything above this line --   </std-header>*/

#ifdef __GNUG__
#pragma interface
#endif

#define SM_PROLOGUE_RC(func_name, is_in_xct, pin_cnt_change)	\
	FUNC(func_name); 					\
	prologue_rc_t prologue(prologue_rc_t::is_in_xct, (pin_cnt_change)); \
	if (prologue.error_occurred()) return prologue.rc();

#define INIT_SCAN_PROLOGUE_RC(func_name, pin_cnt_change)		\
	FUNC(func_name);					\
	prologue_rc_t prologue(prologue_rc_t::in_xct, (pin_cnt_change)); \
	if (prologue.error_occurred()) { 			\
	    _error_occurred = prologue.rc();			\
	    return; 						\
	}

 

class prologue_rc_t {
public:
    enum xct_state_t {
	in_xct,  	// must be active and not prepared
	commitable_xct, // must be prepared if external, else must be active or prepared
	not_in_xct,     // may not have tx, regardless of state
	can_be_in_xct,  // in or not -- no test for active or prepared
	abortable_xct   // active or prepared 
    };
 
    prologue_rc_t(xct_state_t is_in_xct, int pin_cnt_change);
    ~prologue_rc_t();
    void no_longer_in_xct();
    bool error_occurred() const {return _rc.is_error();}
    rc_t   rc() const {return _rc;}

private:
    xct_state_t  _xct_state;
    int     _pin_cnt_change;
    rc_t    _rc;
    xct_log_switch_t*    _toggle;
    xct_t*	_victim;
};

/*
 * Install the code in sm.cpp
 */
#ifdef SM_C

prologue_rc_t::prologue_rc_t(xct_state_t is_in_xct, int pin_cnt_change) :
		_xct_state(is_in_xct), _pin_cnt_change(pin_cnt_change),
		_toggle(0), _victim(0)
{
    w_assert3(!me()->is_in_sm());
    xct_t *x = xct();
    bool	check_log = false;

    switch (_xct_state) {
    case in_xct:
	if ( (!x) || (x->state() != smlevel_1::xct_active)) {
	    _rc = rc_t(__FILE__, __LINE__, 
		    (x && x->state() == smlevel_1::xct_prepared)?
		    smlevel_0::eISPREPARED :
		    smlevel_0::eNOTRANS
		);
	    _xct_state = not_in_xct; // otherwise destructor will fail
	} 
	check_log = true;
	break;

    case commitable_xct: {
	// called from commit and chain
	// If this tx is participating in an external 2pc,
	// it MUST be prepared before commit.  If it's
	// an internally  "distributed"  transaction, it
	// must be prepared or active. (since this
	// prologue is called only on the "client" side.)
	int	error = 0;
	if ( ! x  ) {
	    error = smlevel_0::eNOTRANS;
	} else if (x->is_extern2pc() && (x->state() != smlevel_1::xct_prepared) ) {
	    error = smlevel_0::eNOTPREPARED;
	} else if( (x->state() != smlevel_1::xct_active) &&
		(x->state() != smlevel_1::xct_prepared) ) {
	    error = smlevel_0::eNOTRANS;
	}
	if(error) {
	    _rc = rc_t(__FILE__, __LINE__, error);
	    _xct_state = not_in_xct; // otherwise destructor will fail
	}

	break;
    }
    case abortable_xct:
	// do not special-case external2pc transactions -- they
	// can abort any time, since this is presumed-abort protocol
	if (! x || (x->state() != smlevel_1::xct_active && 
		x->state() != smlevel_1::xct_prepared)) {
	    _rc = rc_t(__FILE__, __LINE__, smlevel_0::eNOTRANS);
	    _xct_state = not_in_xct; // otherwise destructor will fail
	}
	break;

    case not_in_xct:
	if (x) _rc = rc_t(__FILE__, __LINE__, smlevel_0::eINTRANS);
	break;

    case can_be_in_xct:
	// do nothing
	check_log = true;
	break;

    default:
	W_FATAL(smlevel_0::eINTERNAL);
	break;
    }
#ifdef W_DEBUG
    me()->mark_pin_count();
    me()->in_sm(true);
#endif /* W_DEBUG */

    if(_xct_state != not_in_xct) {
	_toggle = new xct_log_switch_t(smlevel_0::ON);
    }

    if(check_log && !_rc) {
	if( ! smlevel_0::in_recovery() ) {
	    _rc = xct_log_warn_check_t::check(_victim);
	}
    }
}


inline
prologue_rc_t::~prologue_rc_t()
{
    if (_xct_state == in_xct || _xct_state == abortable_xct) {
	xct_t& x = *xct();
	x.flush_logbuf(); // NEEDED?
    }
#ifdef W_DEBUG
    me()->check_pin_count(_pin_cnt_change);
    me()->in_sm(false);
#endif /* W_DEBUG */
    if(_toggle) { delete _toggle; }

    if(_victim) {
	sm_stats_info_t * stats = _victim->is_instrumented() ? 
		_victim->steal_stats() : 0;
	W_COERCE(_victim->abort());
	delete _victim;
	delete stats; 
	_victim = 0;
    }
}

inline void
prologue_rc_t::no_longer_in_xct()
{
    _xct_state = not_in_xct;
}

#endif /* SM_C */

/*<std-footer incl-file-exclusion='PROLOGUE_H'>  -- do not edit anything below this line -- */

#endif          /*</std-footer>*/

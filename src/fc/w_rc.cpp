/*<std-header orig-src='shore'>

 $Id: w_rc.cpp,v 1.27 1999/06/07 19:02:55 kupsch Exp $

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
#pragma implementation "w_rc.h"
#endif

#define W_SOURCE
#include <w_base.h>

#ifdef EXPLICIT_TEMPLATE
template class w_sptr_t<w_error_t>;
#endif

bool w_rc_t::do_check = true;


void
w_rc_t::return_check(bool on_off)
{
    do_check = on_off;
}

NORET
w_rc_t::w_rc_t(
    const char* const	filename,
    w_base_t::uint4_t	line_num,
    w_base_t::uint4_t	err_num)
    : w_sptr_t<w_error_t>( w_error_t::make(filename, line_num, err_num) )
{
    ptr()->_incr_ref();
}

NORET
w_rc_t::w_rc_t(
    const char* const	filename,
    w_base_t::uint4_t	line_num,
    w_base_t::uint4_t	err_num,
    w_base_t::int4_t	sys_err)
: w_sptr_t<w_error_t>( w_error_t::make(filename, line_num, err_num, sys_err) )
{
    ptr()->_incr_ref();
}

w_rc_t&
w_rc_t::push(
    const char* const	filename,
    w_base_t::uint4_t	line_num,
    w_base_t::uint4_t	err_num)
{
    w_error_t* p = w_error_t::make(filename, line_num,
				   err_num, ptr());
    p->_incr_ref();
    set_val(p);
    return *this;
}

void
w_rc_t::fatal()
{
	cerr << "fatal error:" << endl << *this << endl;
	w_base_t::abort();
}

w_rc_t&
w_rc_t::add_trace_info(
    const char* const   filename,
    w_base_t::uint4_t   line_num)
{
    ptr()->add_trace_info(filename, line_num);
    return *this;
}

void 
w_rc_t::error_not_checked()
{
    cerr << "Error not checked: rc=" << (*this) << endl;
//    W_FATAL(fcINTERNAL);
}

ostream&
operator<<(
    ostream&            o,
    const w_rc_t&       obj)
{
    return o << *obj;
}


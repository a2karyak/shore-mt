/*<std-header orig-src='shore'>

 $Id: w_factory_thread.cpp,v 1.7 1999/06/07 19:02:52 kupsch Exp $

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

#define W_SOURCE

#include "w.h"
#include "w_shore_alloc.h"

#ifdef W_USE_OWN_HEAP_BASE
W_THREADNEW_STATIC_PTR_DECL(w_own_heap_base_t);
// w_threadnew_t*	w_own_heap_base_t::_w_threadnew = 0;
w_own_heap_base_t::w_own_heap_base_t() 
{
    if (w_own_heap_base_t::_w_threadnew == 0) {
	W_THREADNEW_STATIC_PTR_DECL_INIT(w_own_heap_base_t);
	if (w_own_heap_base_t::_w_threadnew == 0)
	W_FATAL(fcOUTOFMEMORY);
    }
}
void 
w_own_heap_base_t::_check( w_base_t::uint4_t sz,
			const char *n,
			int line,
			const char *file) 
{
    if (_w_threadnew == 0) {
	_w_threadnew = new w_threadnew_t(sz,n,line,file);
	if (_w_threadnew == 0) W_FATAL(fcOUTOFMEMORY);	
    }     					
}
#endif

/**************************************************************************
**
** w_threadnew_t
** allocate off per-thread heap (eventually -- for now, it just keeps
** statistics)
**
**************************************************************************/

NORET
w_threadnew_t::w_threadnew_t(
    const uint4_t	esize,
    const char *	typname,
    int 		line,
    const char *	file
    )
    : w_factory_t(esize, typname, line, file),
    _bytes_in_use(0)
{
}

void
w_threadnew_t::_check() const
{
    // for the time being, assume max allocatable is an int
    w_assert3((int) _bytes_in_use >= 0);
}


NORET
w_threadnew_t::~w_threadnew_t()
{
}

inline void*
w_threadnew_t::_array_alloc(size_t s, int)
{
    _bytes_in_use += s;
    return global_alloc(s);
}

void 
w_threadnew_t::_array_dealloc(void* p, size_t s, int)
{
    _bytes_in_use -= s;
    global_free(p);
}

void*
w_threadnew_t::_alloc(size_t s)
{
    _bytes_in_use += s;
    return global_alloc(s);
}

void 
w_threadnew_t::_dealloc(void* p, size_t s)
{
    _bytes_in_use -= s;
    global_free(p);
}

void		
w_threadnew_t::vtable_collect(vtable_info_t& t) 
{
    /* Convert factory info to strings: */

    w_factory_t::vtable_collect(t);

    t.set_string(factory_kind_attr,  "thread");
    t.set_uint(factory_static_overhead_attr,  sizeof(*this));

    t.set_uint(factory_bytes_in_use_attr,  _bytes_in_use);
    // NB: elem_sz() is always 1 for this
    w_assert3(_bytes_in_use >= (elem_sz() * elem_in_use()));
}


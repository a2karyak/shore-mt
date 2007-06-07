/*<std-header orig-src='shore'>

 $Id: vtable_sthread.cpp,v 1.15 2007/05/18 21:53:44 nhall Exp $

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

/*
 * Functions to gather info from the threads for
 * placing the info into a virtual table
 */

#define STHREAD_C

#include <w.h>
#include <w_debug.h>
#include <w_stream.h>
#include <cstdlib>
#include <cstring>
#include "sthread.h"

#include "vtable_info.h"
#include "sthread_vtable_enum.h"


#ifdef EXPLICIT_TEMPLATE
template class vtable_func<sthread_t>;
#endif /* __GNUG__ */

int
sthread_t::collect(vtable_info_array_t &v)
{
    int nt=0;
    // TODO: lock list
    {
	w_list_i<sthread_t> i(*_class_list);
	while (i.next())  { nt++; }
    }

#ifdef W_DEBUG
    int n = 0;
/*
    // to test realloc:
    if(nt > 1) {
	nt /= 2;
    }
*/
#endif

    // Account for the executable's largest thread stats enum;
    // It's not known until run time.


    if(v.init(nt, global_vtable_last)) return -1;

    vtable_func<sthread_t> f(v);
    {
	w_list_i<sthread_t> i(*_class_list);
	while (i.next())  {
#ifdef W_DEBUG
	    // NB: if we lock the threads list, 
	    // this test won't be necessary
	    n++;
	    if(n > v.size()) {
		f.back_out(0);
		f.realloc();
	    }
#endif
	    f(*i.curr());
	}
    }
    // TODO: unlock the list

    return 0; // no error
}

#include <w_strstream.h>

void		
sthread_t::vtable_collect(vtable_info_t& t) 
{
    /* Convert sthread info to strings: */

    t.set_int(sthread_id_attr, id);
    t.set_string(sthread_name_attr, name() );
    t.set_string(sthread_status_attr,  sthread_t::status_strings[status()]);
    t.set_int(sthread_bytes_alloc_attr,  _bytes_allocated);
    t.set_int(sthread_bytes_high_water_attr,  _high_water_mark);
    t.set_int(sthread_inuse_attr,  _allocations);
    t.set_int(sthread_bytes_overhead_attr,  _allocations*w_fastnew_t::overhead());
}


/*<std-header orig-src='shore'>

 $Id: w_factory_fast.cpp,v 1.11 1999/08/04 21:39:51 bolo Exp $

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

#ifdef __GNUG__
#pragma implementation "w_factory_fast.h"
#endif

#include "w_base.h"
#include "w_shore_alloc.h"

#ifdef PURIFY
#include <purify.h>
#endif

/* Memory allocation alignment is different from whatever align() does.
   Actually, it would  be much better if align took an "aligned to"
   argument, instead of doing 4 implicitly.  That has caused no 
   end of problems.  Anyway, some system require different memory
   alignment in allocations.   That's what this does */
#if defined(Sparc)
#define	fastnew_align(x)	alignon(x,8)
#define	is_fastnew_aligned(x)	((((int)(x)) & 0x7) == 0)
#else
#define	fastnew_align(x)	align(x)
#define	is_fastnew_aligned(x)	is_aligned(x)
#endif

#ifdef EXPLICIT_TEMPLATE
template class vtable_func<w_fastnew_t>;
#endif
/****************************************************************************
**
** w_fastnew_t: per-class factory for allocation from single heap of
** fixed-sized objects.
** NB: ultimately, this should be moved to sthread/ -- when
** we use a per-thread heap
**
****************************************************************************/

NORET
w_fastnew_t::w_fastnew_t(
    uint4_t		elem_sz,
    uint4_t		elem_per_chunk,
    const char *	typname,
    int 		line,
    const char *	file
    )
    : w_factory_t(elem_sz, typname, line, file) ,
      _chunk_list(0),
      _elem_per_chunk(elem_per_chunk), 
      _chunks(0),
      _next_free(0)
{

}


void
w_fastnew_t::_check() const
{
    // TODO: must deal with integer overflow in counters
    // count chunks
    int 		count;
    chunk_t 		*chk;
    for(count=0, chk = _chunk_list; chk!=0; chk=chk->next) {
	count++;
    }
    w_assert3(array_dels() <= array_news());
    w_assert3((int)array_news() >= 0);

    w_assert3(dels() <= news());
    w_assert3((int)news() >= 0);

    w_assert3(elem_in_use() >= global_elem_in_use());
}



NORET
w_fastnew_t::~w_fastnew_t()
{
    bool choke = false;

    if (elem_in_use() != 0) choke = true;

#if defined(W_DEBUG) || defined(W_FASTNEW_PANIC) || defined(PURIFY) || 1
    if (choke)
	    W_FORM2(cerr,("w_fastnew: %s:%d: Memory leak ... %d element(s) in use!\n",
			 file(), line(), elem_in_use()));
#endif

#ifdef W_FASTNEW_PANIC
    if (choke) {
#define	WFN_ERROR_MSG	"w_fastnew: panic on memory leak\n"
        ::write(2, WFN_ERROR_MSG, sizeof(WFN_ERROR_MSG);
#   	if __GNUC_MINOR__ == 6
	// sometimes this helps
	// but sometimes cerr is destroyed
	// before we get here (?)
	//cerr <<  
        //  "The following assert failure is a result of a gcc 2.6.0 bug." 
	// << endl << flush;
	// w_assert1(0);
#	else
	w_assert1(0);
#    	endif
    }
#endif /* FASTNEW PANICS */


    chunk_t* p = _chunk_list;
    chunk_t* next;
    while (p)  {
	next = p->next;
#ifdef  INSTRUMENT_MEM_ALLOC
	// NB: should already have been detached
	// p -= preamble_overhead;
	// preamble* l = (preamble *)p;
	// l->_link.detach();
#endif
	::free(p);
	p = next;
    }
    _chunk_list = 0;
}


void*
w_fastnew_t::_alloc(size_t s)
{

#if defined(PURIFY)
    //  We have to do this when purify
    //  is running if we want to find the
    //  real sources of memory leaks
    if (purify_is_running())  {
	return global_alloc(s);
    }
#endif /*PURIFY*/

    void* ret = _next_free;
    if (s == elem_sz() && ret)  {
	_next_free = * (void**) _next_free;
	return ret;
    } else if (s != elem_sz())  {
	// allocating an array of elements
	return global_alloc(s);
    }
    
    if (! _next_free)  {
	// allocate enough space to hold all of the elements
	// plus a chunk_t at the beginning.  Note that
	// the sizeof chunk_t must be aligned so that
	// elements are aligned.
	size_t new_sz = size_t(
		(
#ifdef  INSTRUMENT_MEM_ALLOC
		preamble_overhead +
#endif
		elem_sz()) * _elem_per_chunk + fastnew_align(sizeof(chunk_t)));

	_chunks++;
	chunk_t* cp = (chunk_t*) ::malloc(new_sz);
	if (! cp)  return 0;

	cp->next = _chunk_list;
	_chunk_list = cp;
	
	register void** curr = 0;

	// first element starts after the aligned sizeof chunk_t
	void* first = ((char*)cp) + fastnew_align(sizeof(chunk_t));
	char* next = (char*) first;
	w_assert3(is_fastnew_aligned(next));

	for (unsigned i = 0; i < _elem_per_chunk; i++)  {
	    curr = (void**) next;
	    next = next + elem_sz() 
#ifdef  INSTRUMENT_MEM_ALLOC
		+ preamble_overhead
#endif
	    ;
	    /*
	     * NB: this should really read: curr->next = next
	     * where curr should point to a free_preamble struct
	     */
	    *curr = next;
	}
	
	*curr = 0;
	_next_free = first;

    }

    ret = _next_free;
    _next_free = * (void**) _next_free;
    return ret;
}

void 
w_fastnew_t::_dealloc(void* p, size_t s)
{
#ifdef PURIFY
    //  We have to do this when purify
    //  is running if we want to find the
    //  real sources of memory leaks
    if (purify_is_running())  {
	global_free(p);
	return;
    }
#endif

    if (s == elem_sz())  {
	*(void**)p = _next_free;
	_next_free = p;
	return;
    }

    if (s != elem_sz())  {
	// deallocating an array
	global_free(p);
	return;
    }
    *(void**)p = _next_free;
    _next_free = p;
}


void		
w_fastnew_t::vtable_collect(vtable_info_t& t) 
{
    /* Convert factory info to strings: */

    w_factory_t::vtable_collect(t);

    t.set_uint(factory_bytes_in_use_attr,  (elem_in_use() * elem_sz()));

    t.set_string(factory_kind_attr,  "fast");
    t.set_uint(factory_static_overhead_attr,  sizeof(*this));

    t.set_uint(factory_chunks_attr,  _chunks);
    t.set_uint(factory_chunk_size_attr,  
	_elem_per_chunk * (elem_sz()+ preamble_overhead)); 

    w_base_t::uint4_t u = _chunks * _elem_per_chunk;
    u -= news() - dels(); 
    u += array_news() - array_dels(); // array news / dels are global
    u -= global_elem_in_use();
    t.set_uint(factory_unused_attr,  u);

}


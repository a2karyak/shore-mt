/*<std-header orig-src='shore'>

 $Id: w_factory.cpp,v 1.15 1999/06/07 19:02:52 kupsch Exp $

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

#undef W_DEBUG_SPACE

#ifdef __GNUG__
#pragma implementation "w_factory.h"
#endif

#include <w_stream.h>

#include "w_base.h"
#include <stddef.h>
#include <new.h>
#include <memory.h>
#undef W_FASTNEW_H
#include "w_factory.h"



#ifdef EXPLICIT_TEMPLATE
template class w_list_t<w_factory_t>;
template class w_list_i<w_factory_t>;
template class w_list_t<w_factory_t::preamble>;
template class w_list_i<w_factory_t::preamble>;
template class vtable_func<w_factory_t>;
template class vtable_func<w_factory_t::histogram>;
#endif 

w_factory_list_t*    	      		   w_factory_t::_factory_list = 0;

// 1 for global pseudo-factory
int			 		   w_factory_t::_factory_list_count=1;

// w_factory_t::w_factory_preamble_list_t*    w_factory_t::_alloced_list = 0;

#ifdef INSTRUMENT_MEM_ALLOC
w_base_t::uint4_t    w_factory_t::preamble_overhead = sizeof(w_factory_t::preamble);
#else
w_base_t::uint4_t    w_factory_t::preamble_overhead = 1; // size of empty struct
// NB: the reason we're doing this is to avoid re-compiling the world
// if we turn on/off INSTRUMENT_MEM_ALLOC, although as long as it's
// in shore.def, it will still re-build the world
#endif

NORET
w_factory_t::w_factory_t(
    const uint4_t	esize,
    const char *	typname,
    int 		line,
    const char *	file
    )
    : 
      _array_news(0), _news(0), 
      _array_dels(0), _dels(0), 
      _elem_sz(esize),
      _global_elem_in_use(0),
      _elem_in_use(0),
      _hiwat(0),
      __line (line), __file(file), __typename(typname),
      _alloced_list(0)
{
    if(_factory_list == 0) {
	_factory_list = new w_factory_list_t(offsetof(w_factory_t, _link));
	if (_factory_list == 0) W_FATAL(fcOUTOFMEMORY);
    }
    _factory_list->append(this);
    _factory_list_count++;

	_alloced_list = new w_factory_preamble_list_t(
		offsetof(w_factory_t::preamble, _link));
	if (_alloced_list == 0)
		W_FATAL(fcOUTOFMEMORY);
}

NORET
w_factory_t::~w_factory_t()
{
    w_assert3(_global_elem_in_use == 0);
    w_assert3(_news == _dels);
    w_assert3(_array_news == _array_dels);

    // remove from list of factories
    _link.detach();
    _factory_list_count--;
    if(_factory_list_count==0) {
	delete _factory_list;
	_factory_list = 0;
    }
    delete _alloced_list;
    _alloced_list = 0;
}

void
w_factory_t::_check() const
{
    w_assert3(_news >= _dels);
    w_assert3(_array_news >= _array_dels);
}

void *
w_factory_t::link_alloced(
	void *p,
	size_t 
#ifdef  INSTRUMENT_MEM_ALLOC
	s
#endif

) const 
{
#ifdef  INSTRUMENT_MEM_ALLOC
    w_heapid_t	h = w_shore_thread_alloc(s, false);

    preamble* l = new (p) preamble;
    l->_heap_id = h;
#ifdef VCPP_BUG_7
    l->_size = s;
#endif
    _alloced_list->append(l);

/*
    cerr << "Alloced " << __typename
	<< " for thread " << heapid 
	<< endl;
*/

    p = (char *)p + preamble_overhead;
#endif /* INSTRUMENT_MEM_ALLOC */
    return p;
}

void *
w_factory_t::unlink_dealloced(
	void *p,
#ifdef  INSTRUMENT_MEM_ALLOC
	size_t& s
#else
	size_t& 
#endif
) const 
{
#ifdef  INSTRUMENT_MEM_ALLOC
    w_heapid_t	h = w_shore_thread_alloc(0, true);

    p = (char *)p - preamble_overhead;
    preamble* l = (preamble *)p;

#ifdef VCPP_BUG_7
    // On NT: s != l->_size.  Use the latter.
    s = l->_size;
#endif

    // charge the dealloc to the thread that allocated it
    if(l->_heap_id != h) {
	w_shore_thread_dealloc(s, l->_heap_id);
    } else {
    	(void) w_shore_thread_alloc(s, true);
    }

/*
    cerr << "unlinked " << __typename
	<< " from thread " << l->_heap_id 
	<< endl;
*/
    l->_link.detach();
#else
#ifdef VCPP_BUG_7
#error cannot configure with VCPP_BUG_7 and without INSTRUMENT_MEM_ALLOC
#endif

    // don't change s.  Trust it.
#endif /* INSTRUMENT_MEM_ALLOC */
    return p;
}

void*
w_factory_t::global_alloc(
    size_t 		s
) 
{
    ++_global_elem_in_use;
    void *result;
#ifdef  INSTRUMENT_MEM_ALLOC
    s += preamble_overhead;
#endif
    result = ::malloc(s);
    return result;
}

void
w_factory_t::global_free(
    void *		p
) 
{
    --_global_elem_in_use;
    ::free(p);
}

void		
w_factory_t::global_vtable_collect(vtable_info_t& t) 
{
    /* Convert factory info to strings: */
    size_t amt, allocations, hiwat;
    w_shore_alloc_stats(amt, allocations, hiwat);

    t.set_string(factory_class_name_attr,  "global");
    t.set_int(factory_class_size_attr,  0);
    t.set_uint(factory_preamble_overhead_attr,  0);
    t.set_uint(factory_in_use_attr,  allocations); 
    t.set_uint(factory_high_water_attr, hiwat); 
    t.set_uint(factory_global_in_use_attr,  allocations);

    t.set_uint(factory_news_attr,  0); // not kept
    t.set_uint(factory_array_news_attr,  0); // not kept
    t.set_uint(factory_dels_attr,  0); // not kept
    t.set_uint(factory_array_dels_attr,  0); // not kept

    t.set_string(factory_kind_attr,  "global");
    t.set_uint(factory_static_overhead_attr,  0);
    t.set_uint(factory_bytes_in_use_attr,  amt);
}

int
w_factory_t::collect(vtable_info_array_t &v)
{
    if(v.init(_factory_list_count, factory_last)) return -1;

    vtable_func<w_factory_t> f(v);

    f.call(w_threadnew_t::global_vtable_collect);

    w_list_i<w_factory_t> i(*_factory_list);

    while (i.next())  {
	f(*i.curr());
    }
    return 0; // no error
}

void		
w_factory_t::vtable_collect(vtable_info_t& t) 
{
    /* Convert factory info to strings: */

    t.set_string(factory_class_name_attr,  __typename);
    t.set_int(factory_class_size_attr,  _elem_sz);
    t.set_uint(factory_preamble_overhead_attr,  preamble_overhead);
    t.set_uint(factory_in_use_attr,  elem_in_use());
    t.set_uint(factory_high_water_attr, high_water()); // not kept
    t.set_uint(factory_global_in_use_attr,  global_elem_in_use());

    t.set_uint(factory_news_attr,  news());
    t.set_uint(factory_array_news_attr,  array_news());
    t.set_uint(factory_dels_attr,  dels());
    t.set_uint(factory_array_dels_attr,  array_dels());

}

void 
w_factory_t::histogram::_realloc() 
{
    int newsize = _size *2;
    int * tmp = new int[ newsize ];
    int j=0;
    for(j=0; j<_size; j++)  {
	if(_array[j]) tmp[j] = _array[j];
	else tmp[j]=0;
    }
    for(j=_size; j<newsize; j++)  { tmp[j]=0; }
    _size = newsize;
    delete[] _array;
    _array = tmp;
}

void 
w_factory_t::histogram::inc(int j) 
{
    while(j >= _size) {
	_realloc();
    }
    _elements++;
    _array[j]++;
}

void		
w_factory_t::histogram::vtable_collect(
    w_factory_t::histogram &h, 
    int i, 
    vtable_info_t& t) 
{
    h.vtable_collect(i, t);
}

void		
w_factory_t::histogram::vtable_collect(
    int i, vtable_info_t& t) 
{
    /*
    cerr << "w_factory_t::histogram::vtable_collect "
	<<  __typename
	<< endl << "\t"
	<< " i=" << i 
	<< " this->size()=" << size() 
	<< " this->array(i)=" << array(i) 
	<< endl;
    */
    w_assert3(i < size() );
    w_assert3(array(i) > 0);

    t.set_string(factory_histo_class_name_attr,  __typename);
    t.set_int(factory_histo_threadid_attr, i-1);
    t.set_int(factory_histo_alloced_attr, array(i));
}

int
w_factory_t::collect_histogram(vtable_info_array_t &v)
{
    if(v.init(_factory_list_count, factory_last)) return -1;

    /*
     * Declare a special case of a vtable func that allows us to
     * bump the vtable_info_array_t cursor for each ENTRY in the
     * histogram
     */
    class histo_vtable_func: public vtable_func<w_factory_t::histogram> {
    private:
		w_factory_t::histogram* _h;
    public: 
	NORET histo_vtable_func(vtable_info_array_t &v) :
		vtable_func<w_factory_t::histogram>(v) {}

	void 	init_histo(w_factory_t::histogram* h) { _h = h; }
	typedef void vtable_collect_histo_func(
		w_factory_t::histogram& _h,
		int i,
		vtable_info_t& t);
	void call(vtable_collect_histo_func f) // arbitrary gather func
	{
	    int i;
	    for(i=0; i < _h->size(); i++) {
		if(_h->array(i) > 0) {
		    if(_array.quant() >= _array.size()) {
			// have to expand _array
			(void) realloc();
		    }
		    f(*_h, i, _array[_curr]);
		    _array.filled();
		    // bump its counter
		    w_assert3(_curr < _array.size());
		    _curr++;
		}
	    }
	}
    } f(v);

    w_list_i<w_factory_t> i(*_factory_list);
    w_factory_t::histogram h;
    f.init_histo(&h);

    /* 
     * for each factory... scan the factory's list of allocated
     * objects, and create a histogram (by thread id) of the number
     * of such objects allocated to each thread
     */

    while (i.next())  {
	w_factory_t *fact = i.curr();
	w_list_i<w_factory_t::preamble> j(*(fact->_alloced_list));
	/*
	 * re-use the histogram structure for each factory.
	 */
	h.reinit(fact->__typename,100);
	while (j.next())  {
	    preamble *x = j.curr();

	    h.inc(w_id2int(x->_heap_id));
	}
	/*
	 * Now stash an entry in the vtable_info_array_t for
	 * each non-zero bar in the histogram 
	 */
	if(h.nelements()>0) {
	    f.call(w_factory_t::histogram::vtable_collect);
	}
    }
    return 0; // no error
}



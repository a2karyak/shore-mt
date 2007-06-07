/*<std-header orig-src='shore' incl-file-exclusion='W_FACTORY_H'>

 $Id: w_factory.h,v 1.16 2007/05/18 21:38:24 nhall Exp $

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

#ifndef W_FACTORY_H
#define W_FACTORY_H

#include "w_defines.h"

/*  -- do not edit anything above this line --   </std-header>*/

/* avoid inclusion of these from w_base.h: */
/* NB: undef them at the end of this file */
#define W_FACTORY_FAST_H
#define W_FACTORY_THREAD_H


#include <w_list.h>
#include <vtable_info.h>
#include <w_factory_vtable_enum.h>
#include <w_shore_alloc.h>

#ifdef __GNUG__
#pragma interface
#endif

class w_factory_t;
typedef w_list_t<w_factory_t>		w_factory_list_t;


class w_factory_t : public w_base_t {
public:
    class histogram {
    public:
	void reinit(const char *n, int i) {
	    __typename = n;
	    _elements=0;
	    if(_size < i) {
		delete[] _array;
		_array = new int[i];
		_size = i;
	    }
	    memset(_array, '\0', sizeof(int) *_size);
	}
	NORET histogram() : __typename(0), _size(0), _elements(0), _array(0) { }
	NORET histogram(const char *n, int i) : __typename(n),
		_size(0), _elements(0), _array(0) { reinit(n, i); }
	NORET ~histogram() {
	    delete[] _array;
	}
    	void inc(int j);
    	int  nelements() const { return _elements; }
    	int  size() const { return _size; }
    	int  array(int i) const { return _array[i]; }
        void vtable_collect(int i, vtable_info_t& t) ;
        static void vtable_collect(histogram &h, int i, vtable_info_t& t) ;
        void vtable_collect(vtable_info_t& ) { w_assert1(0); }
    private:
    	void _realloc();

    private:
	const char *__typename;
	int _size;
	int _elements;
	int *_array;
    };

    NORET w_factory_t(
	const uint4_t	esize,
	const char *	typname,
	int 		line,
	const char *	file
    );
    virtual NORET ~w_factory_t(); 

    void*			alloc(size_t s);
    void			dealloc(void* p, size_t s);

				// For array_[de]alloc():
				// s == total size of block
				// n == sizeof(T)
    void* 			array_alloc(size_t s, int n);
    void 			array_dealloc(void* p, size_t s, int n);

    static w_base_t::uint4_t	overhead(){
				    return  preamble_overhead;
				}
    virtual void		vtable_collect(vtable_info_t& t) ;
    static int 			collect(vtable_info_array_t &v);
    static int 			collect_histogram(vtable_info_array_t &v);
    static void			global_vtable_collect(vtable_info_t& t);

protected:
    virtual void*		_alloc(size_t s)=0;
    virtual void		_dealloc(void* p, size_t s)=0;
    virtual void* 		_array_alloc(size_t s, int n)=0;
    virtual void 		_array_dealloc(void* p, size_t s, int n)=0;

    void*			unlink_dealloced(
				    void *		p,
				    size_t&		s
				) const;
    void*			link_alloced(
				    void *		p,
				    size_t		s
				) const ;
    inline void			alloced(size_t ) { 
				     _news++; 
				     _elem_in_use++;
				     if(_elem_in_use > _hiwat) 
					_hiwat = _elem_in_use;
				 }

    inline void			alloced(size_t s, int n) { 
				    w_assert3(_array_dels <= _array_news);
				    // s/n == # elements allocated in this 
				    // single array allocation
				    int q = (int)s/n;
				    w_assert3(q>=0);
				    _array_news += q;
				    _elem_in_use += q;
				     if(_elem_in_use > _hiwat) 
					_hiwat = _elem_in_use;
				}

    inline void			freed(size_t ) { 
				     _dels++; 
				     _elem_in_use--;
				}
    inline void			freed(size_t s, int n) { 
				    w_assert3((int)_array_dels >= 0);
				    w_assert3(_array_dels < _array_news);
				    w_assert3(_array_news > 0);
				    // s/n == # elements allocated in this 
				    // single array allocation
				    int q = (int)s/n;
				    w_assert3(q>=0);
				    _array_dels += q;

				    w_assert1((int)_elem_in_use >= q);
				    _elem_in_use -= q;
				}
    virtual void		_check()const;

    void* 			global_alloc(size_t s);
    void 			global_free(void *p);

private:

    uint4_t			_array_news; // # allocated in calls to  new[]
					// so after 10 calls of new T[25] 
					// _array_news == 250
    uint4_t			_news;	// # allocated in calls to new
					// so after 10 calls of new T
					// _array_news == 10
    uint4_t			_array_dels;// # deleted in calls to  delete[]
					// so after 10 calls of delete x
					// where x was allocated w/  new T[25]
					// _array_dels == 250
    uint4_t			_dels;  // # deleted with calls to delete
    const uint4_t		_elem_sz;  // sizeof(T)
    uint4_t			_global_elem_in_use; // # object allocations
					// that were satisfied by ::malloc()
					// rather than with this factory's 
					// own heap. 
    uint4_t			_elem_in_use; // # objects presently allocated
					// by new and new[] (and global_alloc)
					// (decreases on delete and delete[])
    uint4_t			_hiwat; // highest value of _elem_in_use

    int 			__line;
    const char *		__file;
    const char *		__typename;
    w_link_t			_link;	

    struct preamble {
	w_heapid_t		_heap_id; // thread id
				// returned by w_shore_thread_alloc
	w_link_t		_link;

#ifdef VCPP_BUG_7
	size_t			_size;
	/* NO filler needed, visual c++ 4-byte aligns everything. */
/*
 * NB: array deallocation does not work on NT without this,
 * due to a VC++ bug.  
 * The fix is kind of convoluted, requiring that the
 * size of the array be stashed in the preamble.  This is unfortunate,
 * not only because of the cost in space, but also because of the
 * way this was layered -- the freed() and alloced() functions have
 * to be adjusted also -- the NT-fix can't be buried only in the
 * link_allocated() and unlink_deallocated() functions.
 */
#endif

	preamble() : _heap_id(0)
#ifdef VCPP_BUG_7
		, _size(0)
#endif
	{
	}
    }; 

protected:
    typedef w_list_t<preamble>	w_factory_preamble_list_t;
    static w_base_t::uint4_t	preamble_overhead;
    w_factory_preamble_list_t*  _alloced_list;

    inline uint4_t		array_news() const { return _array_news; }
    inline uint4_t		news() const { return _news; }
    inline uint4_t		array_dels() const { return _array_dels; }
    inline uint4_t		dels() const { return _dels; }
    inline const uint4_t	elem_sz() const { return _elem_sz; }
    inline uint4_t		global_elem_in_use() const { return
				_global_elem_in_use; }
    inline uint4_t		elem_in_use() const { return _elem_in_use; }
    inline uint4_t		high_water() const { return _hiwat; }
    inline const char *		typname() const { return __typename; }
    inline int 			line() const { return __line; }
    inline const char *		file() const { return __file; }

private:
    // TODO: lock the factory list
    static w_factory_list_t*    _factory_list;
    static int			 _factory_list_count;

    NORET			w_factory_t(const w_factory_t&);
    w_factory_t&		operator=(const w_factory_t&);
};

inline void*
w_factory_t::alloc(size_t s)
{
    void* ret;
    alloced(s); //update stats


    ret = _alloc(s);
    ret = link_alloced(ret, s);
#ifdef W_DEBUG
    _check();
#endif
    return ret;
}

inline void
w_factory_t::dealloc(void* p, size_t s)
{
    if(p) {
	p = unlink_dealloced(p, s);
	freed(s); // update stats
	_dealloc(p, s);
    }
#ifdef W_DEBUG
    _check();
#endif
}

inline void*
w_factory_t::array_alloc(size_t s, int n)
{
    // allocating an array (size s) of elements of size n
    alloced(s,n);
    void *v = _array_alloc(s, n);
    v = link_alloced(v, s);

#ifdef W_DEBUG
    _check();
#endif
    return v;
}

inline void 
w_factory_t::array_dealloc(void* p, size_t s, int n)
{

    if(p) {
	p = unlink_dealloced(p, s);
	freed(s,n);
	_array_dealloc(p, s, n);
    }
#ifdef W_DEBUG
    _check();
#endif
}

#undef W_FACTORY_FAST_H
#undef W_FACTORY_THREAD_H

/*<std-footer incl-file-exclusion='W_FACTORY_H'>  -- do not edit anything below this line -- */

#endif          /*</std-footer>*/

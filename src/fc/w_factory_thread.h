/*<std-header orig-src='shore' incl-file-exclusion='W_FACTORY_THREAD_H'>

 $Id: w_factory_thread.h,v 1.7 1999/06/07 19:02:53 kupsch Exp $

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

#ifndef W_FACTORY_THREAD_H
#define W_FACTORY_THREAD_H

#include "w_defines.h"

/*  -- do not edit anything above this line --   </std-header>*/

/***************** w_threadnew_t ****************************************/


class w_threadnew_t : public w_factory_t {
public:
    NORET			w_threadnew_t(
	const uint4_t		    esize,
	const char *		    typname,
	int 		            line,
	const char *                file
	);
    NORET			~w_threadnew_t();


    void			stats(int &alloc,
					int &use,
					unsigned int &sz
					)const;
    void			vtable_collect(vtable_info_t& t) ;
private:
    w_base_t::uint4_t		_bytes_in_use;
    void			_check()const;
    void*			_alloc(size_t s);
    void			_dealloc(void* p, size_t s);
    void* 			_array_alloc(size_t s, int n);
    void 			_array_dealloc(void* p, size_t s, int n);

    // disabled
    NORET			w_threadnew_t(const w_threadnew_t&);
    w_threadnew_t&		operator=(const w_threadnew_t&);
};


/*
 * Include this macro in a classes private area to override
 * new and delete.  Note that it creates a static member called
 * _w_threadnew.  
 */
#define W_THREADNEW_CLASS_DECL(T)	_W_THREADNEW_CLASS_DECL(_w_threadnew,T)
#define W_THREADNEW_CLASS_PTR_DECL(T) _W_THREADNEW_CLASS_DECL(*_w_threadnew,T)
#define _W_THREADNEW_CLASS_DECL(_fn,T)		\
static w_threadnew_t _fn;			\
void* operator new(size_t s)  			\
{						\
    _check(sizeof(T), #T, __LINE__, __FILE__);	\
    return (_fn).alloc(s); 	                \
}						\
void* operator new(size_t , void *p)		\
{						\
    return p; 	                		\
}						\
void operator delete(void* p, size_t s)		\
{						\
    (_fn).dealloc(p, s);			\
}						\
void* operator new[](size_t s)  		\
{						\
    _check(sizeof(T), #T, __LINE__, __FILE__);	\
    return (_fn).array_alloc(s, sizeof(T));	\
}						\
void operator delete[](void* p, size_t s)	\
{						\
    (_fn).array_dealloc(p, s, sizeof(T));	\
}

/*
 * Call this macro outside of the class in order to construct the
 * the _mem_allocator.
 *
 * PARAMETERS:
 *  T: put the class name here
 */

#define W_THREADNEW_STATIC_DECL(T)	\
w_threadnew_t	T::_w_threadnew(#T,__LINE__,__FILE__);
#define W_THREADNEW_STATIC_PTR_DECL(T)	\
w_threadnew_t*	T::_w_threadnew = 0;
#define W_THREADNEW_STATIC_PTR_DECL_INIT(T )	\
T::_w_threadnew = new w_threadnew_t(sizeof(T), #T,__LINE__,__FILE__);

#if defined(INSTRUMENT_MEM_ALLOC) && defined(NOTDEF)
#define W_USE_OWN_HEAP_BASE
#endif

#ifdef W_USE_OWN_HEAP_BASE
class w_own_heap_base_t {
public:
    NORET			w_own_heap_base_t();
    NORET			~w_own_heap_base_t() {}
    static void			_check(w_base_t::uint4_t sz,
					const char *n,
					int line,
					const char *file);
    W_THREADNEW_CLASS_PTR_DECL(w_own_heap_base_t);

};
#else
class w_own_heap_base_t {
    //  intentionally empty
};
#endif /* W_USE_OWN_HEAP_BASE */

/*<std-footer incl-file-exclusion='W_FACTORY_THREAD_H'>  -- do not edit anything below this line -- */

#endif          /*</std-footer>*/

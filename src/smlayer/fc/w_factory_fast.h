/*<std-header orig-src='shore' incl-file-exclusion='W_FACTORY_FAST_H'>

 $Id: w_factory_fast.h,v 1.9 2000/02/01 23:22:42 bolo Exp $

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

#ifndef W_FACTORY_FAST_H
#define W_FACTORY_FAST_H

#include "w_defines.h"

/*  -- do not edit anything above this line --   </std-header>*/

/***************** w_fastnew_t ****************************************/

struct w_fastnew_chunk_t {
    w_fastnew_chunk_t*		next;
};

class w_fastnew_t;

class w_fastnew_t : public w_factory_t {
public:
    NORET			w_fastnew_t(
	uint4_t			    elem_sz,
	uint4_t			    elem_per_chunk,
	const char *		    typname,
	int 		            line,
	const char *                file
	);
    NORET			~w_fastnew_t();


    void			vtable_collect(vtable_info_t& t) ;

    void* 			_array_alloc(size_t s, int n);
    void 			_array_dealloc(void* p, size_t s, int n);

    void			stats(int &alloc,
					int &use,
					unsigned int &sz
					)const;

private:
    typedef w_fastnew_chunk_t chunk_t;

    void			_check()const;
    void*			_alloc(size_t s);
    void			_dealloc(void* p, size_t s);

    chunk_t*			_chunk_list;
    const uint4_t		_elem_per_chunk;
    uint4_t			_chunks;
    void*			_next_free;

    // disabled
    NORET			w_fastnew_t(const w_fastnew_t&);
    w_fastnew_t&		operator=(const w_fastnew_t&);
};

inline void*
w_fastnew_t::_array_alloc(size_t s, int)
{
    return global_alloc(s);
}

inline void 
w_fastnew_t::_array_dealloc(void* p, size_t, int)
{
    global_free(p);
}

	

#if defined(NO_FASTNEW)
#define	W_FASTNEW_CLASS_DECL(T)
#define	W_FASTNEW_CLASS_PTR_DECL(T)
#define	_W_FASTNEW_CLASS_DECL(_fn,T)

#define W_FASTNEW_STATIC_DECL(T, el_per_chunk)
#define W_FASTNEW_STATIC_PTR_DECL(T)
#define W_FASTNEW_STATIC_PTR_DECL_INIT(T, el_per_chunk)
#else
/*
 * Include this macro in a classes private area to override
 * new and delete.  Note that it creates a static member called
 * _w_fastnew.  
 */
#define W_FASTNEW_CLASS_DECL(T)	_W_FASTNEW_CLASS_DECL(_w_fastnew,T)
#define W_FASTNEW_CLASS_PTR_DECL(T) _W_FASTNEW_CLASS_DECL(*_w_fastnew,T)
#define _W_FASTNEW_CLASS_DECL(_fn,T)		\
static w_fastnew_t _fn;				\
void* operator new(size_t s)  			\
{						\
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
 *  el_per_chunk: number of objects per chunk
 */

#define W_FASTNEW_STATIC_DECL(T, el_per_chunk)	\
w_fastnew_t	T::_w_fastnew(sizeof(T), el_per_chunk,#T,__LINE__,__FILE__);
#define W_FASTNEW_STATIC_PTR_DECL(T)	\
w_fastnew_t*	T::_w_fastnew = 0;
#define W_FASTNEW_STATIC_PTR_DECL_INIT(T, el_per_chunk)	\
T::_w_fastnew = new w_fastnew_t(sizeof(T), el_per_chunk,#T,__LINE__,__FILE__);

#endif

/*<std-footer incl-file-exclusion='W_FACTORY_FAST_H'>  -- do not edit anything below this line -- */

#endif          /*</std-footer>*/

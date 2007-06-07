/*<std-header orig-src='shore' incl-file-exclusion='W_LIST_H'>

 $Id: w_list.h,v 1.52 2007/05/18 21:38:25 nhall Exp $

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

#ifndef W_LIST_H
#define W_LIST_H

#include "w_defines.h"

/*  -- do not edit anything above this line --   </std-header>*/

#ifndef W_BASE_H
#include <w_base.h>
#endif

#include <iostream>

class w_list_base_t;
template <class T> class w_list_t;
template <class T> class w_list_i;

/*--------------------------------------------------------------*
 *  w_link_t							*
 *--------------------------------------------------------------*/
class w_link_t {
public:
    NORET			w_link_t();
    NORET			~w_link_t();
    NORET			w_link_t(const w_link_t&);
    w_link_t& 			operator=(const w_link_t&);

    void			attach(w_link_t* prev_link);
    void			check() {
				    w_assert3(_prev == this && _next == this); // not in a list
				}
    w_link_t* 			detach();
    w_list_base_t* 		member_of() const;

    w_link_t*			next() const;
    w_link_t*			prev() const;

private:
    w_list_base_t*		_list;
    w_link_t*			_next;
    w_link_t*			_prev;

    friend class w_list_base_t;
};

/*--------------------------------------------------------------*
 *  w_list_base_t						*
 *--------------------------------------------------------------*/
class w_list_base_t : public w_vbase_t {
    
public:
    bool 			is_empty() const;
    uint4_t			num_members() const;

    void			dump();
protected:
    NORET			w_list_base_t();
    NORET			w_list_base_t(uint4_t offset);
    NORET			~w_list_base_t();

    void			set_offset(uint4_t offset);
    
    w_link_t			_tail;
    uint4_t			_cnt;
    uint4_t			_adj;

private:
    NORET			w_list_base_t(w_list_base_t&); // disabled
    w_list_base_t&		operator=(w_list_base_t&);
    
    friend class w_link_t;
};

template <class T> class w_list_i;
template <class T> class w_list_t;

inline NORET
w_link_t::w_link_t()
: _list(0) 
{
	_next = this;
	_prev = this;
	/* empty body */
}

inline NORET
w_link_t::~w_link_t()
{
    w_assert3(_next == this && _prev == this);
}

inline NORET
w_link_t::w_link_t(const w_link_t&)
: _list(0)
{
    _next = _prev = this;
}

inline w_link_t&
w_link_t::operator=(const w_link_t&)
{
    _list = 0;
    return *(_next = _prev = this);
}

inline w_list_base_t*
w_link_t::member_of() const
{
    return _list;
}

inline w_link_t*
w_link_t::prev() const
{
    return _prev;
}

inline w_link_t*
w_link_t::next() const
{
    return _next;
}

inline NORET
w_list_base_t::w_list_base_t()
    : _cnt(0), _adj(uint4_max)  // _adj must be set by a later call
				// to set_offset().  We init _adj
				// with a large number to detect
				// errors
{
    _tail._list = 0;
    w_assert3(_tail._next == &_tail && _tail._prev == &_tail);
}

inline NORET
w_list_base_t::w_list_base_t(uint4_t offset)
    : _cnt(0), _adj(offset)
{
    _tail._list = this;
    w_assert3(_tail._next == &_tail && _tail._prev == &_tail);
}

inline void 
w_list_base_t::set_offset(uint4_t offset)
{
    w_assert3(_cnt == 0 && _adj == uint4_max && _tail._list == 0);
    _tail._list = this;
    _adj = offset;
}

inline NORET
w_list_base_t::~w_list_base_t()
{
    w_assert3(_cnt == 0);
}

inline bool
w_list_base_t::is_empty() const
{
    return _cnt == 0;
}

inline w_base_t::uint4_t
w_list_base_t::num_members() const
{
    return _cnt;
}

template <class T> class w_list_t;


    
/*--------------------------------------------------------------*
 *  w_list_t							*
 *--------------------------------------------------------------*/
BIND_FRIEND_OPERATOR_PART_1(T,w_list_t<T>)

template <class T>
class w_list_t : public w_list_base_t {
protected:
    w_link_t* 			link_of(T* t) {

        w_assert3(t);
        return CAST(w_link_t*, CAST(char*,t)+_adj);
    }
    const w_link_t* 		link_of(const T* t) const {

        w_assert3(t);
        return CAST(w_link_t*, CAST(char*,t)+_adj);
    }

    T* 				base_of(w_link_t* p) {

        w_assert3(p);
        return CAST(T*, CAST(char*, p) - _adj);
    }
    const T* 			base_of(const w_link_t* p) const {

        w_assert3(p);
        return CAST(T*, CAST(char*, p) - _adj);
    }

public:
    NORET			w_list_t() {}
    NORET			w_list_t(uint4_t link_offset)
	: w_list_base_t(link_offset)
    {
#ifdef __GNUC__
#else
	w_assert3(link_offset + sizeof(w_link_t) <= sizeof(T));
#endif
    }

    NORET			~w_list_t() {}

    void			set_offset(uint4_t link_offset) {
	w_list_base_t::set_offset(link_offset);
    }

    virtual void		put_in_order(T* t)  {
	
	w_list_t<T>::push(t);
    }

    w_list_t<T>&		push(T* t)   {

        link_of(t)->attach(&_tail);
        return *this;
    }

    w_list_t<T>&        	append(T* t) {

        link_of(t)->attach(_tail.prev());
        return *this;
    }

    T*                  	pop()   {
        return _cnt ? base_of(_tail.next()->detach()) : 0;
    }

    T*                  	chop()  {
        return _cnt ? base_of(_tail.prev()->detach()) : 0;
    }

    T*                  	top()   {
        return _cnt ? base_of(_tail.next()) : 0;
    }

    T*                  	bottom(){
        return _cnt ? base_of(_tail.prev()) : 0;
    }

    friend ostream&		operator<< BIND_FRIEND_OPERATOR_PART_2(T) (
	ostream& 		    o,
	const w_list_t<T>& 	    l);

private:
    // disabled
    NORET			w_list_t(const w_list_t<T>&x)
#ifdef _MSC_VER
      {w_assert1(0);}
#endif
    ;

    // disabled
    w_list_t<T>&		operator=(const w_list_t<T>&)
#ifdef _MSC_VER
      {w_assert1(0); return *this;}
#endif
    ;
    
    friend class w_list_i<T>;
};

#define	W_LIST_ARG(class,member)	w_offsetof(class,member)


/*--------------------------------------------------------------*
 *  w_list_i							*
 *--------------------------------------------------------------*/
template <class T>
class w_list_i : public w_base_t {
public:
    NORET			w_list_i()
	: _list(0), _next(0), _curr(0)		{};

    NORET			w_list_i(w_list_t<T>& l, bool backwards = false)
	: _list(&l), _curr(0), _backwards(backwards) {
	_next = (backwards ? l._tail.prev() : l._tail.next());
    }

    NORET			~w_list_i()	{};

    void			reset(w_list_t<T>& l, bool backwards = false)  {
	_list = &l;
	_curr = 0;
	_backwards = backwards;
	_next = (_backwards ? l._tail.prev() : l._tail.next());
    }
    
    T*				next()	 {
	if (_next)  {
	    _curr = (_next == &(_list->_tail)) ? 0 : _list->base_of(_next);
	    _next = (_backwards ? _next->prev() : _next->next());
	}
	return _curr;
    }

    T*				curr() const  {
	return _curr;
    }
    
private:

    w_list_t<T>*		_list;
    w_link_t*			_next;
    T*				_curr;
    bool			_backwards;

    // disabled
    NORET			w_list_i(w_list_i<T>&)
#ifdef _MSC_VER
      {w_assert1(0);}
#endif
    ;
    w_list_i<T>&		operator=(w_list_i<T>&)
#ifdef _MSC_VER
      {w_assert1(0); return *this;}
#endif
    ;
};

template <class T>
class w_list_const_i : public w_list_i<T> {
public:
    NORET			w_list_const_i()  {};
    NORET			w_list_const_i(const w_list_t<T>& l)
	: w_list_i<T>(*CAST(w_list_t<T>*, &l))	{};
    NORET			~w_list_const_i() {};
    
    void			reset(const w_list_t<T>& l) {
	w_list_i<T>::reset(*CAST(w_list_t<T>*, &l));
    }

    const T*			next() { return w_list_i<T>::next(); }
    const T*			curr() const { 
	return w_list_i<T>::curr(); 
    }
private:
    // disabled
    NORET			w_list_const_i(w_list_const_i<T>&);
    w_list_const_i<T>&		operator=(w_list_const_i<T>&);
};

template <class T, class K>
class w_keyed_list_t : public w_list_t<T> {
public:
    T*				first() { 
	return w_list_t<T>::top();
    }
    T*				last()  { 
	return w_list_t<T>::bottom();
    }
    virtual T*			search(const K& k);

    NORET			w_keyed_list_t();
    NORET			w_keyed_list_t(
	w_base_t::uint4_t	    key_offset,
	w_base_t::uint4_t 	    link_offset);

    NORET			~w_keyed_list_t()	{};

    void			set_offset(
	w_base_t::uint4_t	    key_offset,
	w_base_t::uint4_t 	    link_offset);

protected:
    const K&			key_of(const T& t)  {
	return * (K*) (((const char*)&t) + _key_offset);
    }

    using w_list_t<T>::_tail;
    using w_list_t<T>::base_of;

private:
    // disabled
    NORET			w_keyed_list_t(
	const w_keyed_list_t<T,K>&);
    w_base_t::uint4_t		_key_offset;

    // disabled
    w_list_t<T>&		push(T* t);
    w_list_t<T>&        	append(T* t) ;
    T*                  	chop();
    T*                  	top();
    T*                  	bottom();
};

#define	W_KEYED_ARG(class,key,link)	\
	W_LIST_ARG(class,key), W_LIST_ARG(class,link) 

template <class T, class K>
class w_ascend_list_t : public w_keyed_list_t<T, K>  {
    using w_keyed_list_t<T, K>::_tail;
    using w_keyed_list_t<T, K>::base_of;

public:
    NORET			w_ascend_list_t(
	w_base_t::uint4_t 	    key_offset,
	w_base_t::uint4_t	    link_offset)
	: w_keyed_list_t<T, K>(key_offset, link_offset)   {
    };
    NORET			~w_ascend_list_t()	{};

    virtual T*			search(const K& k);
    virtual void		put_in_order(T* t);

private:
    NORET			w_ascend_list_t(
				const w_ascend_list_t<T,K>&); // disabled
};

template <class T, class K>
class w_descend_list_t : public w_keyed_list_t<T, K> {
    using w_keyed_list_t<T, K>::_tail;
    using w_keyed_list_t<T, K>::base_of;

public:
    NORET			w_descend_list_t(
	w_base_t::uint4_t 	    key_offset,
	w_base_t::uint4_t	    link_offset)
	: w_keyed_list_t<T, K>(key_offset, link_offset)   {
    };
    NORET			~w_descend_list_t()	{};

    virtual T*			search(const K& k);
    virtual void		put_in_order(T* t);

private:
    NORET			w_descend_list_t(
				const w_descend_list_t<T,K>&); // disabled
};



template <class T>
ostream&
operator<<(
    ostream&			o,
    const w_list_t<T>&		l)
{
    const w_link_t* p = l._tail.next();

    cout << "cnt = " << l.num_members();

    while (p != &l._tail)  {
	const T* t = l.base_of(p);
	if (! (o << endl << '\t' << *t))  break;
	p = p->next();
    }
    return o;
}


template <class T, class K>
NORET
w_keyed_list_t<T, K>::w_keyed_list_t(
    w_base_t::uint4_t	    key_offset,
    w_base_t::uint4_t 	    link_offset)
    : w_list_t<T>(link_offset), _key_offset(key_offset)    
{
#ifdef __GNUC__
#else
    w_assert3(key_offset + sizeof(K) <= sizeof(T));
#endif
}

template <class T, class K>
NORET
w_keyed_list_t<T, K>::w_keyed_list_t()
    : _key_offset(0)
{
}

template <class T, class K>
void
w_keyed_list_t<T, K>::set_offset(
    w_base_t::uint4_t		key_offset,
    w_base_t::uint4_t		link_offset)
{
    w_assert3(_key_offset == 0);
    w_list_t<T>::set_offset(link_offset);
    _key_offset = key_offset;
}

template <class T, class K>
T*
w_keyed_list_t<T, K>::search(const K& k)
{
    w_link_t	*p;
    for (p = _tail.next();
	 p != &_tail && (key_of(*base_of(p)) != k);
	 p = p->next());
    return (p && (p!=&_tail)) ? base_of(p) : 0;
}

template <class T, class K>
T*
w_ascend_list_t<T, K>::search(const K& k)
{
    w_link_t	*p;
    for (p = _tail.next();
	 p != &_tail && (key_of(*base_of(p)) < k);
	 p = p->next());

    return p ? base_of(p) : 0;
}

template <class T, class K>
void
w_ascend_list_t<T, K>::put_in_order(T* t)
{
    w_link_t	*p;
    for (p = _tail.next();
	 p != &_tail && (key_of(*base_of(p)) <= key_of(*t));
	 p = p->next());

    if (p)  {
	link_of(t)->attach(p->prev());
    } else {
        link_of(t)->attach(_tail.prev());
    }
}

template <class T, class K>
T*
w_descend_list_t<T, K>::search(const K& k)
{
    w_link_t	*p;
    for (p = _tail.next();
	 p != &_tail && (key_of(*base_of(p)) > k);
	 p = p->next());

    return p ? base_of(p) : 0;
}

template <class T, class K>
void
w_descend_list_t<T, K>::put_in_order(T* t)
{
    w_link_t	*p;
    for (p = _tail.next();
	 p != &_tail && (key_of(*base_of(p)) >= key_of(*t));
	 p = p->next());

    if (p)  {
	link_of(t)->attach(p->prev());
    } else {
        link_of(t)->attach(_tail.prev());
    }
}

/*<std-footer incl-file-exclusion='W_LIST_H'>  -- do not edit anything below this line -- */

#endif          /*</std-footer>*/

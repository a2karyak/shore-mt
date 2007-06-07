/*<std-header orig-src='shore' incl-file-exclusion='W_HASH_H'>

 $Id: w_hash.h,v 1.34 2007/05/18 21:38:25 nhall Exp $

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

#ifndef W_HASH_H
#define W_HASH_H

#include "w_defines.h"

/*  -- do not edit anything above this line --   </std-header>*/

#include <w_base.h>
#include <w_list.h>


template <class T, class K> class w_hash_t;
template <class T, class K> class w_hash_i;

inline w_base_t::uint4_t w_hash(long l)
{
	return (w_base_t::uint4_t) l;
}

inline w_base_t::uint4_t w_hash(unsigned long l)
{
	return (w_base_t::uint4_t) l;
}

inline w_base_t::uint4_t w_hash(w_base_t::uint4_t i)  {
    return i;
}

inline w_base_t::uint4_t w_hash(w_base_t::int4_t i)   {
    return CAST(w_base_t::uint4_t,i);
}

inline w_base_t::uint4_t w_hash(w_base_t::uint2_t i)  {
    return i;
}

inline w_base_t::uint4_t w_hash(w_base_t::int2_t i)   {
    return CAST(w_base_t::int2_t, i);
}

BIND_FRIEND_OPERATOR_PART_1B(T,K,w_hash_t<T,K>)


template <class T, class K>
class w_hash_t : public w_base_t {
public:
    NORET			w_hash_t(
	uint4_t 		    sz,
	uint4_t			    key_offset,
	uint4_t 		    link_offset);
    NORET			~w_hash_t();

    w_hash_t&			push(T* t);
    w_hash_t&			append(T* t);
    T*				lookup(const K& k) const;
    T*				remove(const K& k);
    void			remove(T* t);
    uint4_t			num_members() { return _cnt; }

    friend ostream&		operator<< BIND_FRIEND_OPERATOR_PART_2B(T,K) (
	ostream& 		    o,
	const w_hash_t<T, K>& 	    obj);
    

private:
    friend class w_hash_i<T, K>;
    uint4_t			_top;
    uint4_t			_mask;
    uint4_t			_cnt;
    uint4_t			_key_offset;
    uint4_t			_link_offset;
    w_list_t<T>*		_tab;

    const K&			_keyof(const T& t) const  {
	return * (K*) (((const char*)&t) + _key_offset);
    }
    w_link_t&			_linkof(T& t) const  {
	return * (w_link_t*) (((char*)&t) + _link_offset);
    }

    // disabled
    NORET			w_hash_t(const w_hash_t&)
#ifdef _MSC_VER
	{w_assert1(0);}
#endif
    ;
    w_hash_t&			operator=(const w_hash_t&)
#ifdef _MSC_VER
	{w_assert1(0); return *this;}
#endif
    ;
};

// XXX They are the same for now, avoids offsetof duplication
#define	W_HASH_ARG(class,key,link)	W_KEYED_ARG(class, key, link)


template <class T, class K>
class w_hash_i : public w_base_t {
public:
    NORET		w_hash_i(const w_hash_t<T, K>& t) : _bkt(uint4_max), _htab(t) {};
	
    NORET		~w_hash_i()	{};
    
    T*			next();
    T*			curr()		{ return _iter.curr(); }

private:
    uint4_t		_bkt;
    w_list_i<T>		_iter;
    const w_hash_t<T, K>&	_htab;
    
#ifdef _MSC_VER
    NORET		w_hash_i(w_hash_i& h)
	: _htab(h._htab)
	{ w_assert1(0); };
#else
    NORET		w_hash_i(w_hash_i&);
#endif

    w_hash_i&		operator=(w_hash_i&)
#ifdef _MSC_VER
	{w_assert1(0); return *this;}
#endif
    ;
};


template <class T, class K>
ostream& operator<<(
    ostream&			o,
    const w_hash_t<T, K>&	h)
{
    for (int i = 0; i < h._top; i++)  {
	o << '[' << i << "] ";
	w_list_i<T> iter(h._tab[i]);
	while (iter.next())  {
	    o << h._keyof(*iter.curr()) << " ";
	}
	o << endl;
    }
    return o;
}

template <class T, class K>
NORET
w_hash_t<T, K>::w_hash_t(
    w_base_t::uint4_t 	sz,
    w_base_t::uint4_t 	key_offset,
    w_base_t::uint4_t	link_offset)
: _top(0), _cnt(0), _key_offset(key_offset),
  _link_offset(link_offset), _tab(0)
{
    for (_top = 1; _top < sz; _top <<= 1);
    _mask = _top - 1;
    
    w_assert1(!_tab); // just to check space
    _tab = new w_list_t<T>[_top];
    w_assert1(_tab);
    for (unsigned i = 0; i < _top; i++)  {
	_tab[i].set_offset(_link_offset);
    }
}

template <class T, class K>
NORET
w_hash_t<T, K>::~w_hash_t()
{
    w_assert1(_cnt == 0);
    delete[] _tab;
}

template <class T, class K>
w_hash_t<T, K>&
w_hash_t<T, K>::push(T* t)
{
    _tab[w_hash(_keyof(*t)) & _mask].push(t);
    ++_cnt;
    return *this;
}

template <class T, class K>
w_hash_t<T, K>& w_hash_t<T, K>::append(T* t)
{
    _tab[w_hash(_keyof(*t)) & _mask].append(t);
    ++_cnt;
    return *this;
}

template <class T, class K>
T*
w_hash_t<T, K>::lookup(const K& k) const
{
    w_list_t<T>& list = _tab[w_hash(k) & _mask];
    w_list_i<T> i( list );
    register T* t;
    int4_t count;
    for (count = 0; (t = i.next()) && ! (_keyof(*t) == k); ++count);
    if (t && count) {
	w_link_t& link = _linkof(*t);
	link.detach();
	list.push(t);
    }
	
    return t;
}

template <class T, class K>
T*
w_hash_t<T, K>::remove(const K& k)
{
    w_list_i<T> i(_tab[w_hash(k) & _mask ]);
    while (i.next() && ! (_keyof(*i.curr()) == k));

    if (i.curr()) {
	--_cnt;
	_linkof(*i.curr()).detach();
    }
    return i.curr();
}

template <class T, class K>
void
w_hash_t<T, K>::remove(T* t)
{
    w_assert3(_linkof(*t).member_of() ==
	      &_tab[w_hash(_keyof(*t)) & _mask ]);
    _linkof(*t).detach();
    --_cnt;
}

template <class T, class K>
T* w_hash_i<T, K>::next()
{
    if (_bkt == uint4_max)  {
	_bkt = 0;
	_iter.reset(_htab._tab[_bkt++]);
    }

    if (! _iter.next())  {
	while (_bkt < _htab._top)  {
	    
	    _iter.reset( _htab._tab[ _bkt++ ] );
	    if (_iter.next())  break;
	}
    }
    return _iter.curr();
}

/*<std-footer incl-file-exclusion='W_HASH_H'>  -- do not edit anything below this line -- */

#endif          /*</std-footer>*/

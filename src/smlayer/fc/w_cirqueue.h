/*<std-header orig-src='shore' incl-file-exclusion='W_CIRQUEUE_H'>

 $Id: w_cirqueue.h,v 1.21 1999/06/07 19:02:51 kupsch Exp $

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

#ifndef W_CIRQUEUE_H
#define W_CIRQUEUE_H

#include "w_defines.h"

/*  -- do not edit anything above this line --   </std-header>*/

template <class T> class w_cirqueue_t;
template <class T> class w_cirqueue_i;
template <class T> class w_cirqueue_reverse_i;

template <class T>
class w_cirqueue_t : public w_base_t {
    enum {end_of_q = 0xffffffff};
public:
    NORET			w_cirqueue_t(T* arr, uint4_t size)
	: sz(size), cnt(0), front(end_of_q), rear(end_of_q), array(arr) {};
    NORET			~w_cirqueue_t()		{};
    
    bool			is_empty() {
	return cnt == 0;
    }

    bool			is_full() {
	return cnt == sz;
    }

    void 			reset()   {
	cnt = 0, front = rear = end_of_q;
    }

    w_rc_t			put(const T& t)	{
        w_rc_t rc;
        if (cnt >= sz)
            rc = RC(fcFULL);
        else {
            ++cnt;
            (++rear >= sz ? rear = 0 : 0);
            array[rear] = t;
            (front == end_of_q ? front = 0 : 0);
        }

        return rc;
    }
    w_rc_t			get(T& t)  {
	w_rc_t rc;
	if (! cnt)  {
	    rc = RC(fcEMPTY);
	} else {
	    t = array[front];
	    (--cnt == 0 ? front = rear = end_of_q :
		(++front >= sz ? front =  0 : 0));
	}
	return rc;
    }
    w_rc_t			get()  {
	w_rc_t rc;
	if (! cnt)  {
	    rc = RC(fcEMPTY);
	} else {
	    (--cnt == 0 ? front = rear = end_of_q :
		(++front >= sz ? front =  0 : 0));
	}
	return rc;
    }

    uint4_t			cardinality() {
	return cnt;
    }

protected:
    friend class w_cirqueue_i<T>;
    friend class w_cirqueue_reverse_i<T>;
    uint4_t			sz, cnt;
    uint4_t			front, rear;
    T*				array;
};

template <class T>
class w_cirqueue_i : public w_base_t {
public:
    NORET			w_cirqueue_i(w_cirqueue_t<T>& q) 
	: _q(q), _idx(q.front)  {};

    T* 				next()  {
	T* ret = 0;
	if (_idx != w_cirqueue_t<T>::end_of_q)  {
	    ret = &_q.array[_idx];
	    if ((_idx == _q.rear ? _idx = w_cirqueue_t<T>::end_of_q : ++_idx) == _q.sz)
		_idx = 0;
	}
	return ret;
    }
private:
    w_cirqueue_t<T>& 		_q;
    uint4_t 			_idx;
};

template <class T>
class w_cirqueue_reverse_i : public w_base_t {
public:
    NORET			w_cirqueue_reverse_i(w_cirqueue_t<T>& q) 
	: _q(q), _idx(q.rear)  {};

    T* 				next()  {
	T* ret = 0;
	if (_idx != w_cirqueue_t<T>::end_of_q)  {
	    ret = &_q.array[_idx];
#ifdef __GNUC__
	    if (_idx == _q.front) 
		_idx = w_cirqueue_t<T>::end_of_q;
	    else if (_idx-- == 0)  
		_idx = _q.sz - 1;
#else
	    if ((_idx == _q.front ? _idx = w_cirqueue_t<T>::end_of_q : _idx--) == 0)
		_idx = _q.sz - 1;
#endif
	}
	return ret;
    }
private:
    w_cirqueue_t<T>& 		_q;
    uint4_t 			_idx;
};

/*<std-footer incl-file-exclusion='W_CIRQUEUE_H'>  -- do not edit anything below this line -- */

#endif          /*</std-footer>*/

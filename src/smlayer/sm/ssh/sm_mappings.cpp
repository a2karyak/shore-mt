/*<std-header orig-src='shore'>

 $Id: sm_mappings.cpp,v 1.35 2003/06/19 22:39:36 bolo Exp $

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
 * Code for testing the functions of the 2PC coordinator 
 * and subordinate coode
 * Part of ssh
 */

#ifdef __GNUG__
#pragma implementation
#endif

#include <sm_vas.h>
#include <sm_mappings.h>
#include <coord.h>
#include <w_debug.h>

/*
 **************************************************************************
 * mapping for coordinator
 **************************************************************************
 */

NORET 
ep_map::ep_map() :
    _slist(0),
    _slist_size(0), // # entries in the _slist
    _ns(0)
{
}
void
ep_map::expand(int num) 
{
    // essentially realloc
    DBG(<<"expand map to size " << num);

    server_handle_t*	old_slist = _slist;
    server_handle_t*	new_slist = new server_handle_t[num];

    if(!new_slist) {
	W_FATAL(ss_m::eOUTOFMEMORY);
    }
    if(old_slist) {
	memcpy(new_slist, old_slist, _slist_size * sizeof(server_handle_t));
	delete[] old_slist;
    }
    _slist = new_slist;
    _slist_size = num;
}

NORET 
ep_map::~ep_map()
{
    clear();
}

void
ep_map::add_ns(NameService *ns) { _ns = ns; }

rc_t  
ep_map::name2endpoint(const server_handle_t &s, Endpoint &ep)
{

    if(_slist == 0) {
	return RC(nsNOTFOUND);
    }
    int j;

    if(find(s, j)) {
	W_DO(_box.get(j, ep));
	DBG(<<" about to .acquire() on ep " << ep);

	ep.acquire();
	DBG( << " returning " << s << " <--> " << ep);
	return RCOK;
    }

    DBG( << s << ": not found ");

    return RC(nsNOTFOUND);
}

rc_t  
ep_map::endpoint2name(const Endpoint &ep, server_handle_t &s)
{
    w_assert3(ep.is_valid());

    if(_slist == 0) {
	return RC(nsNOTFOUND);
    }
    int j;

    if(find(ep, j)) {
	s = _slist[j];
#ifdef W_TRACE
	/*
	DBG(<<"ep_map::endpoint2name ep=" );
	_debug.clog <<"endpoint2name: " 
		<< ep <<" <--> " << s << flushl;
	*/
#endif
	return RCOK;
    }
    return RC(nsNOTFOUND);
}

/*
 * If we find the endpoint, we return its index
 */
bool
ep_map::find(const Endpoint &ep, int &i)
{
    w_assert3(_slist_size <= (int)_box.size());
    Endpoint    x;
    rc_t        rc;
    for(i=0; i< _slist_size; i++) {
        rc = _box.get(i, x);
        if(rc) { 
	    DBG(<<"ep_map::find: _box.get returns " << rc);
	    continue; 
	}
        if(x == ep) {
	    DBG(<<"find: returns index " << i);
            return true;
        }
    }
    DBG(<<"find: not found " << i);
    return false;
}

void
ep_map::clear()
{
    int i;
    if(_slist) {
        w_assert3(_slist_size <= (int)_box.size());
        Endpoint        x;
        rc_t    rc;
        for(i=0; i< _slist_size; i++) {
            rc = _box.get(i, x);
            if(rc) { 
		DBGTHRD(
		    <<"ep_map::clear couldn't release ep "
		    << i );
		continue; 
	    }

	    DBGTHRD(<<" releasing " 
		<< i
		<< " name " << _slist[i]
		<< " " << x
		);
	    if(x.refs() > 3) {
		cerr << "WARNING: clearing map with #refs>3: " << x << endl;
	    }
            x.release();
        }
    }
    _slist_size = 0;
    delete[] _slist;
    _slist = 0;
}

/*
 * If we find the server_handle, we return its index
 */
bool
ep_map::find(const server_handle_t &s, int &i)const
{
    DBG(<<"ep_map::find2: looking for name "
	<< s );
    for(i=0; i< _slist_size; i++) {
        if(_slist[i] == s) {
	    DBG(<<"find: returns index " << i
		<< " name " << _slist[i]
		);
            return true;
        }
    }
    DBG(<<"find: not found ");
    return false;
}

rc_t
ep_map::add(int num, const char **list)
{
    rc_t        	rc;
    Endpoint    	ep;
    const char *	c;
    server_handle_t 	s;

    w_assert3(_ns!=0);

    int i;
    for(i=0; i< num; i++) {
	c = list[i];
	s = c;

        // See if we've already stashed this endpoint/spec pair
        int j;
        if(find(s, j)) {
	    DBG(<<"already in table");
	    return RC(fcFOUND);
	    // TODO: should we remove it and re-insert?
            continue;
        } else {
            // get the equiv Endpoint
	    // NB: nameserver wants a null-terminated
	    // string for the name
	    ostrstream ss;
	    ss << s << ends;
            w_rc_t e = _ns->lookup(ss.str(), ep);
	    delete [] ss.str();
	    W_DO(e);
#ifdef W_TRACE
	    _debug.clog << _slist_size << ": NS: " 
		    << s << " <-->  " <<  ep << flushl;
#endif 
	    w_assert3(ep);
	    w_assert3(ep.is_valid());

	    /*
	    w_assert3(ep.refs() > 0 
		    && ep.refs() <= 3
		);
	    */

	    expand(_slist_size+1); // changes _slist_size

            // stash the server list in the given order
            _slist[_slist_size-1] = s; // make a copy

            // stash it in the box
            W_DO(_box.set(_slist_size-1, ep));
	    DBGTHRD(<<" added as entry @ " 
		<< _slist_size-1 
		<<  " " << c << " <--> " << ep);
        }
    }
    return RCOK;
}

rc_t
ep_map::refresh(const char *c, Endpoint& given, bool force_lookup)
{
    rc_t        	rc;
    Endpoint		held;
    Endpoint		ep;
    server_handle_t     s(c);

   DBG(<<"refresh: given= " << given);
#if defined(W_TRACE) || defined(NOTDEF)
    // TODO: remove this check
    unsigned int refs = given.is_valid()? given.refs() : 0;
    unsigned int refsh = 0;
#endif

    int j;
    bool replace = false;
    if(find(s, j)) {
	DBG(<<"already in table");
	W_DO(_box.get(j, held));
	if(held == given && !force_lookup) {
	    DBG(<<"returning early - found && no force");
	    return RCOK;
	} 
#if defined(W_TRACE) || defined(NOTDEF)
	refsh = held.is_valid()? held.refs() : 0;
#endif
	held.release();
	// If held==given, given is down one 
	// If given is still valid, it'll be re-inserted
	// and its ref count re-increased, below.
	replace = true;
    }
    // replace it or insert it (not in the table)
    // NB: nameserver wants a null-terminated
    // string for the name

    ostrstream	ss;
    ss << s << ends;
    w_rc_t e = _ns->lookup(ss.str(), given);
    delete [] ss.str();
    W_DO(e);

    // Now given might be a *new* endpoint altogether.

    // if held==given, given is even, even if given
    //      is not what it was on entry.
    // else given is up one ref count
    DBG(<<"given " << given);

    // refs should not change unless the entry is being
    // put in and the given ep wasn't there before 

    DBG(<<"refs=" << refs
	<< " refsh=" << refsh
	<< " held=" << held
	<< " given=" << given
    );
#ifdef NOTDEF
    // TODO:  get this cleaned up
    w_assert3( 
	((held == given) && 
		(refs == given.refs()) ||
		(refsh == given.refs()))
    ||
	(refs == 0) 
    ||
	(refs + 1 <= given.refs()) 
	);
#endif

    if(given.isDead()) {
	rc = RC(scDEAD);
    } else {
	if(replace) {
	    // being put in

	    rc = _box.set(j, given);
	} else {
	    expand(_slist_size+1);
	    // stash the server list in the given order
	    _slist[_slist_size-1] = s; // make a copy
	    // stash it in the box
	    j = _slist_size - 1;
	    rc = _box.set(_slist_size-1, given);
	    if(!rc) _slist_size++;
	}
	DBGTHRD(<<" added as entry @ " << j
		<<  " " << c << " <--> " << given);
    }
    if(rc) {
	// leave the ref count the same
	// if we're returning in error
	if(held != given) given.release();
	return rc;
    }
    return RCOK;
}


void
ep_map::dump(ostream &o) const
{
    ep_map_i  	i(this);
    bool	eof=false;
    Endpoint 	ep;
    server_handle_t	s;

    while(i.next(eof), !eof) {
	i.ep(ep);
	i.name(s);
        o << "    " << s << " <--> " << ep << endl;
    }
}


/*
 **************************************************************************
 * mapping for subordinates
 **************************************************************************
 */

#ifdef EXPLICIT_TEMPLATE
template class w_list_t<tid_map::_map>; 
template class w_list_i<tid_map::_map>;
template class w_hash_t<tid_map::_map, const gtid_t>;
template class w_hash_i<tid_map::_map, const gtid_t>;
#endif
w_hash_t<tid_map::_map, const gtid_t> tid_map::__table(
	16, W_HASH_ARG(tid_map::_map, _gtid, _link));

w_base_t::uint4_t 
w_hash (const gtid_t &g) 
{
    u_char 	 c = 0;
    const u_char *cp = (const u_char *)g.data_at_offset(0);
    unsigned int 	 j;

    // chances are that all the lengths will be
    // the same, so we'll only use the value portion
    for(j=0; j < g.length(); j++) {
	c ^= *(cp++);
    }
    // We're using a small hash table so if we return
    // something mod 256 that's just fine.
    return (w_base_t::uint4_t) c;
}

NORET 
tid_map::_map::_map(const gtid_t &g, const tid_t &t): 
	_gtid(g), _tid(t) 
{}
NORET 
tid_map::_map::~_map()
{}

NORET 
tid_map::tid_map() 
{
   _table = &__table;
#if 0
   _table = new w_hash_t<tid_map::_map, const gtid_t>(16,
   	    W_HASH_ARG(tid_map::_map, _gtid, _link));
    if(!_table) {
	W_FATAL(fcOUTOFMEMORY);
    }
#endif
}

NORET 
tid_map::~tid_map()
{
    if(_table) 
    {
	{
	    w_hash_i<tid_map::_map, const gtid_t> iter(*_table);
	    _map* p;
	    bool  bad=false;
	    while ((p = iter.next()))  {
		bad = true;
		cerr << " unexpected remaining entry " 
		    << p->_gtid << endl;
		_table->remove(p);
		delete p;
	    }
	}
	// _table is not malloced anymore:
	// delete _table;
	_table = 0;
    }
}

rc_t
tid_map::remove(const gtid_t &g, const tid_t &t)
{

    W_COERCE(_mutex.acquire());
    _map	*m = _table->remove(g);
    _mutex.release();
    if(m) {
	w_assert1(m->_tid == t);
	delete m;
    }
    return RCOK;
}

rc_t  
tid_map::gtid2tid(const gtid_t &g, tid_t &t)
{
    W_COERCE(_mutex.acquire());
    _map	*m = _table->lookup(g);
    if(m) {
	t = m->_tid;
    }
    _mutex.release();
    if(m) {
	return RCOK;
    } else {
	return RC(fcNOTFOUND);
    }
}

void
tid_map::dump(ostream &o) const
{
    if(_table) {
	w_hash_i<tid_map::_map, const gtid_t> iter(*_table);
	_map* p;
	while ((p = iter.next()))  {
	    o << "    " << p->_gtid
		<< " <--> " << p->_tid <<endl;
	}
    }
}

rc_t
tid_map::add(const gtid_t &g, const tid_t &t)
{
    W_COERCE(_mutex.acquire());
    _map	*m = _table->lookup(g);
    if(!m) {
	m = new _map(g,t);
	_table->push(m);
    }
    _mutex.release();
    return RCOK;
}

NORET
ep_map_i::ep_map_i(const ep_map *m)
	: 
	_index(-1),
	_map(m)
{
}

void
ep_map_i::next(bool &eof) 
{
    if(++_index >= _map->_slist_size) {
	eof = true;
    } else eof = false;
}

rc_t
ep_map_i::name(server_handle_t &out) 
{
    if(_index >= _map->_slist_size) {
	return RC(ss_m::eBADSCAN);
    }
    out = _map->_slist[_index];
    return RCOK;
}

rc_t
ep_map_i::ep(Endpoint &out) 
{
    if(_index > _map->_slist_size) {
	return RC(ss_m::eBADSCAN);
    }
    // hack to deal with the fact that EndpointBox::get is
    // not const:
    ep_map *x = (ep_map *)_map;
    return x->_box.get(_index, out);
}


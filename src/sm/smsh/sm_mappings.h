/*<std-header orig-src='shore' incl-file-exclusion='SM_MAPPINGS_H'>

 $Id: sm_mappings.h,v 1.15 2005/08/05 04:06:25 bolo Exp $

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

#ifndef SM_MAPPINGS_H
#define SM_MAPPINGS_H

#include "w_defines.h"

/*  -- do not edit anything above this line --   </std-header>*/

#ifdef __GNUG__
#pragma interface
#endif


#include <sm_vas.h>
#include <mappings.h>

class log_entry;
class ep_map_i;
class ep_map : public name_ep_map {
    /* an abstract class that defines mappings needed by
     * the 2PC coordinator and subordinates
     */
    friend class ep_map_i;
public:
    NORET  	ep_map();
    NORET  	~ep_map();
    rc_t  	name2endpoint(const server_handle_t &, Endpoint &);
    rc_t  	endpoint2name(const Endpoint &, server_handle_t &);

    void  	add_ns(NameService *_ns);

    // For use by sm_coordinator:
    void  	expand(int num);
    rc_t  	add(int argc, const char **argv);
    rc_t        refresh(const char *c, Endpoint& ep, bool force=false);
    void  	clear();
    void  	dump(ostream&) const;

private:
    EndpointBox		_box;
    server_handle_t*	_slist;
    int			_slist_size;
    NameService*	_ns;

    /* No mutex on this because it'll be set up once at the beginning */
    bool  	find(const Endpoint &ep, int &i);
    bool  	find(const server_handle_t &s, int &i) const;
};

class ep_map_i {

public:
     NORET 		ep_map_i(const ep_map *e);
     void 		next(bool &eof);
     rc_t 		name(server_handle_t &);
     rc_t 		ep(Endpoint &);

private:
    int 	_index;
    const ep_map*	_map;
};


class tid_map : public tid_gtid_map {
    /* an abstract class that defines mappings needed by
     * the 2PC coordinator and subordinates
     */

private:
    struct _map {
	gtid_t 		_gtid;
	tid_t  		_tid;
	w_link_t 	_link;
	NORET _map(const gtid_t &g, const tid_t &t);
	NORET ~_map();
    };
    friend ostream &operator<<(ostream &o, const _map &);
#ifdef _MSC_VER
    static w_hash_t<_map, const gtid_t> __table;
    w_hash_t<_map, const gtid_t> *_table;
#else
    static w_hash_t<tid_map::_map, const gtid_t> __table;
    w_hash_t<tid_map::_map, const gtid_t> *_table;
#endif
    smutex_t	_mutex; /* will be expanded and shrunk on the fly */


public:
    NORET tid_map();
    NORET ~tid_map();
    rc_t  add(const gtid_t &, const tid_t &);
    rc_t  remove(const gtid_t &, const tid_t &);
    rc_t  gtid2tid(const gtid_t &, tid_t &);

    void  dump(ostream&) const;
};

/*<std-footer incl-file-exclusion='SM_MAPPINGS_H'>  -- do not edit anything below this line -- */

#endif          /*</std-footer>*/

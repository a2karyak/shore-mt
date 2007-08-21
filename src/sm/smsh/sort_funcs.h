/*<std-header orig-src='shore' incl-file-exclusion='SORT_FUNCS_H'>

 $Id: sort_funcs.h,v 1.11 2007/08/21 19:46:14 nhall Exp $

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

#ifndef SORT_FUNCS_H
#define SORT_FUNCS_H

#include "w_defines.h"

/*  -- do not edit anything above this line --   </std-header>*/


#include <cctype>
#include <umemcmp.h>

#define MAXBV 0x7f
struct metadata {
    typed_btree_test t;
    smsize_t  	offset;
    smsize_t  	length;
    bool	aligned;
    bool 	fixed;
    bool 	lexico;
    bool 	nullable;
    bool 	derived;
    NORET metadata() : t(test_nosuch), 
	offset(0), length(0), aligned(false), fixed(false),
		lexico(false), nullable(false), derived(false) {}
};

/*
 * In sort_funcs3.cpp :
 */

extern nbox_t universe;

w_rc_t 
originalboxCSKF(
    const rid_t& rid,
    const object_t&	in_obj,
    key_cookie_t cookie,
    factory_t&	internal,
    skey_t*	out
) ;

w_rc_t 
get_key_info(
    const rid_t&        ,  // record id
    const object_t&	,
    key_cookie_t    	,  // type info
    factory_t&		,
    skey_t*	
);

w_rc_t 
mhilbert (
    const rid_t&        ,  // record id
    const object_t&	,
    key_cookie_t    	,  // type info
    factory_t&		,
    skey_t*	
); 
#ifdef USE_LID
w_rc_t 
lmhilbert (
    const lrid_t&        ,  // record id
    const object_t&	,
    key_cookie_t    	,  // type info
    factory_t&		,
    skey_t*	
); 
/* end USE_LID*/
#endif
w_rc_t 
onehilbert (
    const rid_t&        ,  // record id
    const object_t&	,
    key_cookie_t    	,  // type info
    factory_t&		,
    skey_t*	
); 
w_rc_t 
lonehilbert (
    const rid_t&        ,  // record id
    const object_t&	,
    key_cookie_t    	,  // type info
    factory_t&		,
    skey_t*	
); 

void
make_random_alpha(char *object, w_base_t::uint4_t length);

void
make_random_box(char *object, w_base_t::uint4_t length);

int
check_file( stid_t  fid, sort_keys_t&kl, bool do_compare);

class deleter {
private:
    stid_t 	fid;
#ifdef USE_LID
    lfid_t	lfid;
/* end USE_LID*/
#endif
    scan_index_i*	scanp;
    scan_rt_i*	scanr;
public:
    deleter() : fid(stid_t::null), 
#ifdef USE_LID
	lfid(lfid_t::null.lvid, lfid_t::null.serial),
/* end USE_LID*/
#endif
	scanp(0),
	scanr(0)
	{}
    deleter(stid_t& s) : fid(s), 
#ifdef USE_LID
	lfid(lfid_t::null.lvid, lfid_t::null.serial),
/* end USE_LID*/
#endif
	scanp(0),
	scanr(0)
	{}
#ifdef USE_LID
    deleter(lfid_t& s) : fid(stid_t::null), 
	lfid(s.lvid, s.serial),
	scanp(0),
	scanr(0)
	{}
/* end USE_LID*/
#endif
    void set(stid_t&s) { fid = s; }
#ifdef USE_LID
    void set(lfid_t&s) { lfid.lvid = s.lvid; lfid.serial = s.serial; }
/* end USE_LID*/
#endif
    void set(scan_index_i* s) { scanp = s; }
    void set(scan_rt_i* s) { scanr = s; }
    ~deleter();
    void disarm(); 
};

CF getcmpfunc(typed_btree_test t, 
	CSKF&  lexify_func, key_cookie_t lfunc_cookie) ;
void 
convert(typed_value &k, int& kk, typed_btree_test t, char *stringbuffer ) ;
void 
convert(int kk, typed_value &k, typed_btree_test t, char *stringbuffer ) ;

w_rc_t 
ltestCSKF(
    const rid_t&          rid,  // record id
    const object_t&	in_obj,
    key_cookie_t    k,  // type info
    factory_t&	internal,
    skey_t*	out
);
w_rc_t 
testCSKF(
    const rid_t&          rid,  // record id
    const object_t&	in_obj,
    key_cookie_t    k,  // type info
    factory_t&	internal,
    skey_t*	out
);



/*<std-footer incl-file-exclusion='SORT_FUNCS_H'>  -- do not edit anything below this line -- */

#endif          /*</std-footer>*/

/*<std-header orig-src='shore'>

 $Id: sort_funcs.cpp,v 1.18 2003/01/31 22:47:40 bolo Exp $

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

#define SM_SOURCE

/*
 * A set of applications functions -- to be moved into the
 * sm library -- to handle new sorting API
 */


#include "shell.h"
#include "sort_funcs.h"



/*
 * Helper function for next test function below
 * Scans index and file. Index is supposed to be key=oid
 * and file contains objects whose last few bytes are oid that
 * matches corresponding index entry.  Index and file should be
 * in same order vis-a-vis the oids.
 */
w_rc_t
compare_index_file(
    stid_t idx,
    stid_t fid
)
{
    char	stringbuffer[MAXBV];
    scan_index_i scani(idx, scan_index_i::gt, cvec_t::neg_inf, 
			      scan_index_i::lt, cvec_t::pos_inf,
			      true // include nulls
			      );
    scan_file_i  scanf(fid, ss_m::t_cc_file);

    bool 	feof;
    bool 	ieof;
    w_rc_t	rc;
    pin_i*	pin;
    vec_t 	key;
    smsize_t 	klen, elen;
    rid_t	rid;
    int 	i=0;
    while ( !(rc=scanf.next(pin, 0, feof)) && !feof ) {
	i++;
	rc = scani.next(ieof);
	if(rc) {
	    DBG(<<"rc=" << rc);
	    return RC_AUGMENT(rc);
	}
	w_assert3(!ieof);
	klen = MAXBV;

	key.reset().put(stringbuffer, MAXBV);

	vec_t el;
	elen = sizeof(rid);
	el.put(&rid, elen);
	rc = scani.curr(&key, klen, &el, elen);
	if(rc) {
	    DBG(<<"rc=" << rc);
	    return RC_AUGMENT(rc);
	}

        w_assert3(elen == sizeof(rid));

	/* Compare rid with the oid we find in the file record */
	smsize_t offset = pin->body_size() - sizeof(rid_t);
	DBG(<<"start: " << pin->start_byte()
		<< " length:" << pin->length()
		<< " offset " << offset);
	while(pin->start_byte()+pin->length() <= offset){ 
	    rc = pin->next_bytes(feof); 
	    if(rc) {
		DBG(<<"rc=" << rc);
		return RC_AUGMENT(rc);
	    }
	    w_assert3(!feof);
	}
	offset -=  pin->start_byte();
	// not handling logical case...
	rid_t myrid;
	if(offset + sizeof(rid_t) > pin->length()) {
	    smsize_t amt = pin->length() - offset;
	    memcpy(&myrid, pin->body() + offset, amt);
	    rc = pin->next_bytes(feof); 
	    if(rc) {
		DBG(<<"rc=" << rc);
		return RC_AUGMENT(rc);
	    }
	    w_assert3(!feof);
	    memcpy((char *)(&myrid)+amt, pin->body(), sizeof(rid_t) - amt);
	} else {
	    DBG(<<"copy out rid starting from offset " << offset);
	    memcpy(&myrid, pin->body() + offset, sizeof(rid_t));
	}
	if(myrid != rid) {
	    cerr << "Mismatch !!! File item # " << i
		<< " contains rid " << myrid
		<< " while index item contains rid " << rid
		<< endl;
	} else {
	    DBG(<<"OK for rid " << rid 
		<< " key length " << klen);
	}
    }
    rc = scani.next(ieof);
    w_assert3(ieof && feof);
    if(rc) {
	DBG(<<"rc=" << rc);
	return RC_AUGMENT(rc);
    }
    return RCOK;
}

/*
 * Helper function for test function below.
 * Scans index, counts # nulls and # non-nulls in the
 * entire file.
 */
w_rc_t
count_nulls(
    stid_t idx
)
{
    bool 	ieof=false;
    w_rc_t	rc;
    vec_t 	key, el;
    smsize_t 	klen, elen;

    int 	nonnulls=0, nulls=0, total=0;
    char	stringbuffer[MAXBV];
    char	stringbuffer2[MAXBV];
    {
	int 	i=0;
	/* Scan for non-nulls */
	scan_index_i scani(idx, 
		scan_index_i::gt, cvec_t::neg_inf, 
		scan_index_i::lt, cvec_t::pos_inf,
		false // do not include nulls
	      );
	while ( !( rc = scani.next(ieof)) && !ieof ) {
	    i++;
	    elen = (klen = MAXBV);
	    key.reset().put(stringbuffer, MAXBV);
	    el.reset().put(stringbuffer2, MAXBV);

	    rc = scani.curr(&key, klen, &el, elen);
	    if(rc) {
		DBG(<<"rc=" << rc);
		return RC_AUGMENT(rc);
	    }
	    w_assert3(key.size() != 0);
	} 
	if(rc) {
	    DBG(<<"rc=" << rc);
	    return RC_AUGMENT(rc);
	}
	w_assert3(ieof);
	nonnulls = i;
    }
    {
    	int i=0;
	/* Scan for nulls only */
	vec_t	nullvec;
	scan_index_i scani(idx, 
		scan_index_i::eq, nullvec,
	        scan_index_i::eq, nullvec,
		true // include nulls
        );
	while ( !( rc = scani.next(ieof)) && !ieof ) {
	    i++;
	    elen = (klen = MAXBV);
	    key.reset().put(stringbuffer, MAXBV);
	    el.reset().put(stringbuffer2, MAXBV);

	    rc = scani.curr(&key, klen, &el, elen);
	    if(rc) {
		DBG(<<"rc=" << rc);
		return RC_AUGMENT(rc);
	    }
	    w_assert3(key.size() != 0);
	} 
	if(rc) {
	    DBG(<<"rc=" << rc);
	    return RC_AUGMENT(rc);
	}
	w_assert3(ieof);
      	nulls=i;
    }
    {
    	int i=0;
	/* Scan whole file */
	vec_t	nullvec;
	scan_index_i scani(idx, 
		scan_index_i::gt, cvec_t::neg_inf, 
		scan_index_i::lt, cvec_t::pos_inf,
		true // include nulls
        );
	while ( !( rc = scani.next(ieof)) && !ieof ) {
	    i++;
	    elen = (klen = MAXBV);
	    key.reset().put(stringbuffer, MAXBV);
	    el.reset().put(stringbuffer2, MAXBV);
	    rc = scani.curr(&key, klen, &el, elen);
	    if(rc) {
		DBG(<<"rc=" << rc);
		return RC_AUGMENT(rc);
	    }
	    w_assert3(key.size() != 0);
	} 
	if(rc) {
	    DBG(<<"rc=" << rc);
	    return RC_AUGMENT(rc);
	}
	w_assert3(ieof);
      	total=i;
    }
    w_assert1(nulls + nonnulls == total);
    return RCOK;
}

/*
 * Scan the file, deleting corresponding entries from
 * the index.  Probe, Delete the key/elem pr, re-probe,
 * re-insert, re-probe, re-delete, re-probe.
 * This tests insert/remove of null entries, for one thing.
 * The file given should be the original file if it still
 * exists, so that we can avoid deleting in sorted order.
 */
w_rc_t
delete_index_entries(
    stid_t idx,
    stid_t fid,
    smsize_t keyoffset
)
{
    char	stringbuffer[MAXBV];
    scan_file_i  scanf(fid, ss_m::t_cc_file);

    bool 	feof;
    w_rc_t	rc;
    pin_i*	pin;
    vec_t 	key, elem;
    smsize_t 	klen, elen;
    rid_t	rid;
    int 	i=0;
    while ( !(rc=scanf.next(pin, 0, feof)) && !feof ) {
	i++;

	smsize_t ridoffset = pin->body_size() - sizeof(rid_t);
	klen = ridoffset - keyoffset;

	smsize_t offset = keyoffset;

	/* Get key from file record */
	while(pin->start_byte()+pin->length() <= offset){ 
	    rc = pin->next_bytes(feof); 
	    if(rc) {
		DBG(<<"rc=" << rc);
		return RC_AUGMENT(rc);
	    }
	    w_assert3(!feof);
	}
	offset -=  pin->start_byte();
	// not handling logical case...
        smsize_t amt = pin->length() - offset;
        DBG(<<"offset=" <<offset << " amt=" << amt);
        memcpy(&stringbuffer, pin->body() + offset, amt);

	if(offset + klen > pin->length()) {
	    rc = pin->next_bytes(feof); 
	    if(rc) {
		DBG(<<"rc=" << rc);
		return RC_AUGMENT(rc);
	    }
	    w_assert3(!feof);
	    offset = 0;
	    DBG(<<"offset=" <<offset << " amt=" << amt);
	    memcpy(stringbuffer+amt, pin->body(), klen - amt);
	}
	key.reset().put(stringbuffer, klen);

	offset = ridoffset;

	/* Get oid from file record */
	while(pin->start_byte()+pin->length() <= offset){ 
	    rc = pin->next_bytes(feof); 
	    if(rc) {
		DBG(<<"rc=" << rc);
		return RC_AUGMENT(rc);
	    }
	    w_assert3(!feof);
	}
	offset -=  pin->start_byte();

	if(offset + sizeof(rid_t) > pin->length()) {
	    smsize_t amt = pin->length() - offset;
	    DBG(<<"offset=" <<offset << " amt=" << amt);
	    memcpy(&rid, pin->body() + offset, amt);
	    rc = pin->next_bytes(feof); 
	    if(rc) {
		DBG(<<"rc=" << rc);
		return RC_AUGMENT(rc);
	    }
	    w_assert3(!feof);
	    offset = 0;
	    DBG(<<"offset=" <<offset << " amt=" << amt);
	    memcpy((char *)(&rid)+amt, pin->body(), sizeof(rid_t) - amt);
	} else {
	    DBG(<<"copy out rid starting from offset " << offset);
	    memcpy(&rid, pin->body() + offset, sizeof(rid_t));
	}
	w_assert3(rid == pin->rid());

#ifdef W_TRACE
// _debug.setflags("sort_funcs.cpp btree.cpp btree_impl.cpp btree_p.cpp");
#endif
	/*
	 * For key,elem pair, 
	 * Probe, delete, probe, insert, probe, delete, probe
	 */
	char *el = stringbuffer+klen;
	elen = MAXBV;
	bool found;
	DBG(<<" START DEBUGGING NEW KEY " << key 
		<< " for rid " << rid);

	DBG(<< " find_assoc " << key );
	// gets *** FIRST *** elem for this key 
	// -- might not match this oid
        rc = sm->find_assoc(idx, key, el, elen, found);
	DBG(<<rc);

	elem.reset().put(el, elen);
	DBG(<<"found=" << found << " elem = " << elem);

	if(!found) {
  	    DBG(<<"ERROR: NOT FOUND");
	    cerr << "ERROR: Cannot find index entry for " << 
		key << " " << rid <<endl;
	}
	W_DO(rc);

	if(elen != sizeof(rid_t)) {
	    cerr << __LINE__ << " ERROR: wrong elem length - expected " <<
		sizeof(rid_t) << " got " <<elen <<endl;
	}
	if(umemcmp(&rid, el, elen)) {
	    /*
	    cout << "UNSTABLE SORT: rids don't match: expected " 
		<< rid << " found for key " << key
		<<endl;
	    */
	}

	elem.reset().put(&rid, sizeof(rid_t));

	DBG(<<"**** KEY LEN = " << klen << " for rid " <<rid);

	DBG(<< "destroy " << key << ","<<elem);
	rc = sm->destroy_assoc(idx, key, elem);
	DBG(<< rc);
	W_DO(rc);

	/*
        cout << "PRINT INDEX AFTER REMOVAL of key,elem " 
		<< key 
		<< "," << elem
		<< endl;
        W_DO( sm->print_index(idx) );
	*/

	DBG(<< "find " << key);
	elen = MAXBV;
        rc = sm->find_assoc(idx, key, el, elen, found);
	DBG(<< " after-delete find check returns " << rc
		<< " and found=" << found);
	if(found) {
	   // duplicate key - elems had better not match
	    if(elen != sizeof(rid_t)) {
		cerr <<  __LINE__ << " ERROR: wrong elem length - expected " <<
		    sizeof(rid_t) << " got " <<elen <<endl;
	    }
	    if(umemcmp(&rid, el, elen)) {
	       DBG(<<" found duplicate, but elems don't match:  OK");
	    } else {
		cerr << "ERROR: found deleted key,elem pr: "
		    << key << " " << rid <<endl;
	    }
	}
	W_DO(rc);

        elem.reset().put(&rid, sizeof(rid));
	DBG(<< "create " << key << " elem= " << elem << " rid=" << rid);
        W_DO( sm->create_assoc(idx, key, elem) );

	DBG(<<" DONE DEBUGGING THIS KEY " );
#ifdef W_TRACE
// _debug.setflags("sort_funcs.cpp");
#endif

	DBG(<< "find " << key);
	elen = MAXBV;
        W_DO( sm->find_assoc(idx, key, el, elen, found) );
	if(!found) {
	    cerr << "ERROR: can't find inserted key,elem pr: "
		    << key << " " << rid <<endl;
	}
	DBG(<< "destroy " << key);
	W_DO( sm->destroy_assoc(idx, key, elem) );
	DBG(<< "find " << key);
	elen = MAXBV;
        W_DO( sm->find_assoc(idx, key, el, elen, found) );
	if(found) {
	   // duplicate key - elems had better not match
	    if(elen != sizeof(rid_t)) {
		cerr <<  __LINE__ << " ERROR: wrong elem length - expected " <<
		    sizeof(rid_t) << " got " <<elen <<endl;
	    }
	    if(umemcmp(&rid, el, elen)) {
	       DBG(<<" found duplicate, but elems don't match:  OK");
	    } else {
		cerr << "ERROR: found deleted key,elem pr: "
		    << key << " " << rid <<endl;
	    }
	}
	// leave it out of the index
	/*
        cout << "PRINT INDEX AFTER REMOVAL of key " << key << endl;
        W_DO( sm->print_index(idx) );
	*/
#ifdef W_TRACE
// _debug.setflags("none");
#endif
    }

    {
	// Index had better be empty now
	scan_index_i*	scanp = new scan_index_i(idx,
			      scan_index_i::gt, cvec_t::neg_inf, 
			      scan_index_i::lt, cvec_t::pos_inf,
			      true /* nullsok */
			      );
        deleter		d4; 	// auto_delete for scan_index_i
        d4.set(scanp);
	bool eof=false;
	for (i = 0; (!(rc = scanp->next(eof)) && !eof) ; i++);
	if(i > 0) {
	    cerr << " ERROR: INDEX IS NOT EMPTY!  contains " << i 
			<< " entries " << endl;
	    w_assert3(0);
	}
    }
    if(rc) {
	DBG(<<"rc=" << rc);
	return RC_AUGMENT(rc);
    }
    return RCOK;
}

/*
 * t_test_bulkload_int_btree : a misnomer -- now it tests several key types
 * 				
 * Does several things: 1) creates file of typed large records with key
 * 	that crosses page boundary (unless key is of length 1)
 *      and contains its own OID at the end.
 * 2) sorts file, requesting index output
 * 3) uses that to bulk-load an index
 * 4) checks the index - scans it and compares the oid given with
 * 	the orig object's key
 * 5) sorts the orig file, requesting new file output, preserve orig file
 * 6) scans index and new file, comparing keys and trailing OIDs
 */
int
t_test_bulkload_int_btree(Tcl_Interp* ip, int ac, TCL_AV char* av[])
{
    bool    unique_case=false; // NB: if you turn this to true,
	// the test will fail because the TEST can't handle the
	// case where duplicate nulls are eliminated, partly because
	// it doesn't account for *which* null is left after elimination.
	// (ditto for any other dup that gets eliminated)

    bool    use_logical = false;
    if (use_logical_id(ip))  {
	use_logical = true;
    }
    if (check(ip, "vid nkeys keytype nullok|notnull", ac, 5))
	return TCL_ERROR;

    int nkeys_arg = 2;
    int vid_arg = 1;
    int type_arg = 3;
    int null_arg = 4;

    bool 	nullsok=false;
    if(strcmp(av[null_arg], "nullok")==0) {
	nullsok = true;
    } else if(strcmp(av[null_arg], "notnull")==0) {
	nullsok = false;
    } else {
	cerr << "Bad argument #" << null_arg
		<< " to test_bulkload_int_btree: expected nullok | notnull " 
		<< " got " << av[null_arg]
		<<endl;
	return TCL_ERROR;
    }

    int runsize = 3; // minimum
    // Let's puff up the objects a bit - make sure 
    // the key crosses a page boundary

    int data_sz = global_sm_config_info.lg_rec_page_space;

    char *garbage = new char[data_sz-1];
    memset(garbage, '0', data_sz -1);
    w_auto_delete_array_t<char> delgarbage(garbage);
    vec_t	zeroes(garbage, data_sz-1);

    const 	int n = atoi(av[nkeys_arg]);
    /* make the values range run from -n/2 to n/2 */
    const 	int h = n/2;
    const bool 	reverse = false; /* Put them into the file in reverse order */
    char	stringbuffer[MAXBV];
    typed_value k;
    vec_t 	data;
    vec_t 	hdr; // keep it null

    const char *test=av[type_arg];
    typed_btree_test t = cvt2type(test);
    if(t == test_bv && n > MAXBV-1) {
	cerr << "Can't do b*1000 test with more than "
		<< MAXBV-1 << " records - sorry" << endl;
	return TCL_ERROR;
    }
    const char*	kd = check_compress_flag(test); 
    CSKF	lfunc;
    generic_CSKF_cookie lfunc_cookie;
    CF		cmpfunc = getcmpfunc(t, lfunc, 
				key_cookie_t(&lfunc_cookie));

    lfunc_cookie.in_hdr = false; // for this test, keys in body
    lfunc_cookie.offset = zeroes.size();
    k._length =  lfunc_cookie.length;

    smsize_t 	len_hint = k._length + zeroes.size(); 

    sort_keys_t kl(1); // one key
    {
	// Set attibutes of sort as a whole
	int bad=0;
	// if(kl.set_keep_orig()) { DBG(<<""); bad++; }
	if(kl.set_unique(unique_case)) { DBG(<<""); bad++; }
	if(kl.set_null_unique(unique_case)) { DBG(<<""); bad++; }
	if(kl.set_ascending()) { DBG(<<""); bad++; }

	// Can't use stable for btree bulk-load because
	// we must use rid order for that.
	kl.set_stable(false);

	// no key cookie
	if(kl.set_for_index(lfunc, key_cookie_t(&lfunc_cookie))) { 
		DBG(<<""); bad++; }
	if(bad>0) {
	    tclout.seekp(ios::beg); 
	    tclout << ssh_err_name(ss_m::eBADARGUMENT) << ends;
	    Tcl_AppendResult(ip, tclout.str(), 0);
	    return TCL_ERROR;
	}
    }

    if ((t == test_b1)||(t == test_bv)||(t == test_b23)||(t == test_blarge)) {
	    kl.set_sortkey_derived(0, 
		use_logical? ltestCSKF : testCSKF, 
		(key_cookie_t)zeroes.size(),
		false, // not in hdr: in body
		true, // aligned
		true, // lexico
		cmpfunc
	    );
    } else if(nullsok) {
	    kl.set_sortkey_derived(0, 
		use_logical? ltestCSKF : testCSKF, 
		(key_cookie_t)zeroes.size(),
		false, // not in hdr: in body
		false, // aligned
		false, // lexico
		cmpfunc
	    );
    } else {
	    kl.set_sortkey_fixed(0,
		// offset, length:
		zeroes.size(), k._length, 
		false, // not in hdr: in body
		false, // aligned
		false, // lexico
		cmpfunc
	    );
    }
    {
	deleter	    d1; // auto-delete
	deleter	    d2; // auto-delete

	lfid_t  	    lfid; // logical case
	lstid_t 	    lidx; // logical case
	stid_t 	    stid; // phys case
	stid_t 	    fid;  // phys case
	vid_t  	    vid;  // phys case
	if (use_logical)  {
	    istrstream anon(VCPP_BUG_1 av[vid_arg], strlen(av[vid_arg])); 
		    anon >> lidx.lvid;
	    lfid.lvid = lidx.lvid;

	    DO( sm->create_index(lidx.lvid, 
			    (unique_case? sm->t_uni_btree : sm->t_btree),  
			    ss_m::t_load_file, kd, ss_m::t_cc_kvl, 
			    0, lidx.serial
			    ) );
	    DBG(<<"d1.set " << lidx); 
	    d1.set(lidx);

	    DO( sm->create_file(lfid.lvid, lfid.serial, ss_m::t_load_file) );
	    DBG(<<"d2.set " << lfid); 
	    d2.set(lfid);
	} else {
	    DO( sm->create_index(atoi(av[vid_arg]), 
		// sm->t_uni_btree, 
		sm->t_btree,
		 ss_m::t_load_file, kd, ss_m::t_cc_kvl, stid,
		 serial_t::null
		 ) );
	    DBG(<<"d1.set " << stid); 
	    d1.set(stid);
	    DO( sm->create_file(atoi(av[vid_arg]), fid, ss_m::t_load_file) );
	    DBG(<<"d2.set " << fid); 
	    d2.set(fid);
	    vid = stid.vol;
	}
	{
	    /* Create the records in the original file */
	    int 	i;	
	    lrid_t  lrid;
	    rid_t   rid;
	    for (i = 0; i < n; i++)  {
		int kk = i - h;
		if(reverse) kk = 0 - kk;
		convert(kk, k, t, stringbuffer);
		if(t == test_bv || t == test_b23 || t == test_blarge) {
		    w_assert3(k._u.bv == stringbuffer);
		}

		data.reset().put(zeroes);

		DBG(<<"Data contains zvec len " << data.size());
		if(nullsok && ((i & 0x1)== 0x1)) {
		    // every other one becomes null key
		    DBG(<<"NULL key");
		} else if(t == test_bv || t == test_b23 || t == test_blarge) {
		    DBG(<<" .. adding string of length " << k._length
			<<":" <<  k._u.bv
		    );
		    data.put(k._u.bv, k._length);
		} else {
		    DBG(<<" .. adding key of length " << k._length);
		    data.put(&k._u, k._length);
		}
		if (use_logical)  {
		    vec_t oid(&lrid, sizeof(lrid));
		    DO( sm->create_rec( lfid.lvid, lfid.serial, 
				    hdr,
				    len_hint,
				    data,
				    lrid.serial) );
		    DBG(<<"created rec: value=" << kk 
			<<" object is " << lrid 
			<<" size is " << data.size() 
			<< "(before append oid)"
			);

		    DO( sm->append_rec(lrid.lvid, lrid.serial, oid) );
		    /* APPEND OID */
		} else {
		    vec_t oid(&rid, sizeof(rid));
		    DO( sm->create_rec( fid, 
				    hdr,
				    len_hint,
				    data,
				    rid) );
		    DBG(<<"created rec " << i+1 << " : value=" << kk 
			<<" object is " << rid 
			<<" size is " << data.size() 
				<< "+" << oid.size()
			);
		    /* APPEND OID */
		    DO( sm->append_rec(rid, oid, false) );
		}
	    }
	}

	{   // Sort the file, preparing a file of key/oid 
	    // pairs for bulk-ld
	    lfid_t  olfid; // logical case
	    stid_t  ofid;  // physical case
	    deleter d3;    // auto-delete

	    if (use_logical)  {
		DO( sm->create_file(olfid.lvid, olfid.serial, ss_m::t_load_file) );
		DBG(<<"d3.set " << olfid);
		d3.set(olfid); // auto-delete
	    } else {
		DO( sm->create_file(vid, ofid, ss_m::t_load_file, serial_t::null) );
		DBG(<<"d3.set " << fid);
		d3.set(ofid); // auto-delete
	    }

	    if (use_logical)  {
		DO( sm->sort_file(lfid.lvid, lfid.serial,
				olfid.lvid,
				olfid.serial,
				1, &lfid.lvid,
				kl,
				len_hint,
				runsize,
				runsize*ss_m::page_sz));
	    } else {
		DO( sm->sort_file(fid, ofid,
				1, &vid,
				kl,
				len_hint,
				runsize,
				runsize*ss_m::page_sz));
	        if((t == test_bv) && !nullsok) {
		    /* 
		     * check file won't work where
		     * keys have to be scrambled
		     */
		    int ck = check_file(ofid, kl, ((t == test_bv) && !nullsok));
		    if(ck) {
			cerr << "check_file failed, reason=" << ck <<endl;
			return TCL_ERROR;
			DBG(<<"Check output file done");
		    }
		}
	    }
	    DBG(<<"Sort file done");


	    sm_du_stats_t stats;
	    if (use_logical)  {
		DO( sm->bulkld_index(lidx.lvid, lidx.serial,
			     1, &lfid.lvid, &lfid.serial,
			     stats, false, false) );
	    } else {
		DO( sm->bulkld_index(stid, 1, &ofid, stats, false, false) );
	    }
	    DBG(<<"Bulk load done");

	    /*
	    cout << "PRINT INDEX AFTER BULK LOAD" << endl;
	    DO( sm->print_index(stid) );
	    */

	    // the temp output file (d3) is deleted here
	}

	{
	    /* verify */
	    scan_index_i* scanp = 0;
	    deleter	d4; // auto_delete for scan_index_i
	    if (use_logical)  {
		scanp = new scan_index_i(lidx.lvid, lidx.serial, 
				  scan_index_i::gt, cvec_t::neg_inf, 
				  scan_index_i::lt, cvec_t::pos_inf,
				  true /* nullsok */
				  );
	    } else {
		scanp = new scan_index_i(stid,
			      scan_index_i::gt, cvec_t::neg_inf, 
			      scan_index_i::lt, cvec_t::pos_inf,
			      true /* nullsok */
			      );
	    }
	    DBG(<<"d4.set scanp");
	    d4.set(scanp);

	    DBG(<<"Starting index scan" );
	    bool 	eof;
	    w_rc_t 	rc;
	    int 	i;
	    pin_i	handle;
	    vec_t 	key;
	    smsize_t klen, elen;
	    lrid_t	lrid;
	    rid_t	rid;
	    k._u.i8_num = 0; // just for kicks
	    for (i = 0; (!(rc = scanp->next(eof)) && !eof) ; i++)  {
		klen = k._length;
		if(t == test_bv || t == test_b23 || t == test_blarge) {
		    key.reset().put(stringbuffer, MAXBV);
		    k._u.bv = stringbuffer;
		} else {
		    key.reset().put(&k._u, klen);
		}

		vec_t el;
		if(use_logical) {
		    elen = sizeof(lrid);
		    el.put(&lrid, elen);
		} else {
		    elen = sizeof(rid);
		    el.put(&rid, elen);
		}
		DO( scanp->curr(&key, klen, &el, elen));

		DBG(<<" klen = " << klen);

		if(t == test_bv || t == test_b23 || t == test_blarge) {
		    w_assert3(k._u.bv == stringbuffer);
		    if(nullsok) {
			w_assert1(klen < MAXBV+1);
		    } else {
			w_assert1(klen >= 1 && klen < MAXBV+1);
		    }
		} else {
		    if(nullsok) {
			w_assert1(klen == smsize_t(k._length) ||
				klen == 0);
		    } else {
			w_assert1(klen == smsize_t(k._length));
		    }
		}
		if(use_logical) {
		    w_assert1(elen == smsize_t(sizeof(lrid_t)));
		} else {
		    w_assert1(elen == smsize_t(sizeof(rid_t)));
		}

		/*
		 * Verify key 
		 */
	        int kk = 0;
		if(klen > 0) {
		    convert(k, kk, t, stringbuffer);
		    DBG(<<"index scan found key " << kk); 
		    DBG(<<"object is " << rid );
		    if(reverse) {
			w_assert1(0-kk == i - h);
		    } else {
			switch(t) {
			case test_i1:
			case test_i2:
			case test_i4:
			case test_i8:
			case test_f4:
			case test_f8:
			    // XXX
			    //w_assert1(kk == i - h);
			    break;

			case test_b1:
			case test_b23:
			case test_blarge:
			case test_bv:
			case test_u1:
			case test_u2:
			case test_u4:
			case test_u8:
			    // because of unsigned-ness,
			    // things get reorded
			    break;
			default:
			    w_assert1(0);
			    break;
			}
		    }
		}

		/* 
		 * Now verify that this is really the key of that object
		 */
		smsize_t tail=0;
		if(use_logical) {
		    tail = sizeof(lrid_t);
		    DO(handle.pin(lrid.lvid, lrid.serial, zeroes.size()));
		} else {
		    tail = sizeof(rid_t);
		    DO(handle.pin(rid, zeroes.size()));
		}
		smsize_t offset = zeroes.size() - handle.start_byte();
		smsize_t amt = handle.length() - offset;
		smsize_t l = klen;
		if(klen == 0) {
		    // we'll end up copying oid into stringbuf, 
		    // because for null keys, the oid gets spread across
		    // the two pages
		    l = tail;
		}

		typed_value bodykey;
		if(klen == 0) {
		    memcpy(stringbuffer, handle.body()+offset, amt);
		} else if(t == test_bv || t == test_b23 || t == test_blarge) {
		    memcpy(stringbuffer, handle.body()+offset, amt);
		    bodykey._u.bv = stringbuffer;
		} else {
		    memcpy(&bodykey._u, handle.body()+offset, amt);
		}
		l -= amt;
		bool eof;
		DO(handle.next_bytes(eof));

		// b1, u1, i1 all fit on one page
		// except for tail (oid)

		w_assert1(!eof); // at least 2 pgs/object in this test
		if(klen != 0) {
		    w_assert1(handle.length() == l + tail);
		}
		if(klen == 0) {
		    memcpy(stringbuffer+amt, handle.body(), l);
		} else if(t == test_bv || t == test_b23 || t == test_blarge) {
		    DBG(<<"memcpy amt=" << l << " at offset " << 0);
		    memcpy(stringbuffer+amt, handle.body(), l);
		    bodykey._u.bv = stringbuffer;
		} else {
		    DBG(<<"memcpy amt=" << l << " at offset " << 0);
		    memcpy( (char *)(&bodykey._u)+amt, handle.body(), l);
		}

		if(use_logical) {
		    lrid_t	oid;
		    if(klen == 0) {
			memcpy( &oid, stringbuffer, tail);
		    } else {
			memcpy( &oid, handle.body()+l, tail);
		    }
		    DBG(<<"rid is " <<lrid);
		    if(oid != lrid) {
			cerr << "Mismatched OIDs " 
				<< " index has " << lrid
				<< " body has " << oid 
				<<endl;
		    }
		} else {
		    rid_t	oid;
		    if(klen == 0) {
			memcpy( &oid, stringbuffer, tail);
		    } else {
			memcpy( &oid, handle.body()+l, tail);
		    }
		    DBG(<<"rid is " <<rid);
		    if(oid != rid) {
			cerr << "Mismatched OIDs " 
				<< " index has " << rid
				<< " body has " << oid 
				<<endl;
		    }
		}
		DO(handle.next_bytes(eof));
		w_assert1(eof); // no more than 2 pgs/object in this test

		if(klen == 0) {
		    DBG(<<"object contains NULL key "); 
		} else if(klen>0) {
		    int bodyk; 
		    convert(bodykey, bodyk, t, stringbuffer);
		    DBG(<<"object contains key " << bodyk); 

		    if(bodyk != kk) {
			cerr << "Mismatched key, object" <<endl;
			cerr << "...index key is " << kk <<endl; 
			if(use_logical) {
			    cerr << "...object id is " << lrid <<endl;
			} else {
			    cerr << "...object id is " << rid << endl;
			}
			cerr << "...key in body is " << bodyk <<endl;
		    }
		}
	    }
	    DBG(<<"scan .next yields " << rc << " eof=" << eof);
	    DO( rc );
	    if(!nullsok) w_assert1(i == n);
	    DBG(<<"scan is finished");
	    // d4 will delete the scan ptr
	}
	{   // Re-sort the file, preparing a deep copy of the orig file 
	    lfid_t  olfid; // logical case
	    stid_t  ofid;  // physical case
	    deleter d3;    // auto-delete


	    if (use_logical)  {
		DO( sm->create_file(olfid.lvid, olfid.serial, ss_m::t_load_file) );
		DBG(<<"d3.set " << olfid);
		d3.set(olfid); // auto-delete
	    } else {
		DO( sm->create_file(vid, ofid, ss_m::t_load_file, serial_t::null) );
		DBG(<<"d3.set " << fid);
		d3.set(ofid); // auto-delete
	    }

	    if(kl.set_for_file(true/*deep*/, true/*keep*/, 
		false /*carry_obj*/)){ DBG(<<""); }
		// TODO: test with carry_obj
		// TODO: test set_object_marshal

	    if (use_logical)  {
		DO( sm->sort_file(lfid.lvid, lfid.serial,
				olfid.lvid,
				olfid.serial,
				1, &lfid.lvid,
				kl,
				len_hint,
				runsize,
				runsize*ss_m::page_sz));
	    } else {
		DO( sm->sort_file(fid, ofid,
				1, &vid,
				kl,
				len_hint,
				runsize,
				runsize*ss_m::page_sz));
	    }
	    DBG(<<"Re-sort file done");

	    { /* verify deep copy file */
		if(!use_logical) {
		    // not implemented for logical
		    w_rc_t rc = compare_index_file(stid, ofid);
		    DBG(<<"rc=" << rc);
		    DO(rc);

		    rc = count_nulls(stid);
		    DBG(<<"rc=" << rc);
		    DO(rc);
		}
	    }
	    { /* test insert/remove/probe for nulls */
		if(!use_logical) {
		    // not implemented for logical
		    w_rc_t rc = delete_index_entries(stid, 
			fid, // orig file
			zeroes.size());
		    DBG(<<"rc=" << rc);
		    DO(rc);
		}
	    }
	}// the temp output file (d3) is deleted here
	DBG(<<"Leaving scope of d1, d2");
	// d1, d2 will delete the index and original file
    }
    DBG(<<"LEFT scope of d1, d2");
    return TCL_OK;
}

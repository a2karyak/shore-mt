/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

/*
 *  $Id: shell.c,v 1.183 1996/07/09 20:42:50 nhall Exp $
 */
#define SHELL_C

#include <limits.h>
#include "w.h"
#include "sm_vas.h"
#include "debug.h"
#include "ssh.h"
#include <tcl.h>
#include <string.h>
#include "ssh_error.h"
#ifdef SOLARIS2
#include <solaris_stats.h>
#else
#include <unix_stats.h>
#endif


extern ss_m* sm;
extern bool start_client_support; // from ssh.c

typedef int smproc_t(Tcl_Interp* ip, int ac, char* av[]);
const MAXRECLEN = 1000;

char   outbuf[ss_m::page_sz * 2];
static ostrstream tclout(outbuf, sizeof(outbuf));

#undef DO
#define DO(x)							\
{								\
    w_rc_t ___rc = x;						\
    if (___rc)  {						\
	tclout.seekp(ios::beg);                                 \
	tclout << ssh_err_name(___rc.err_num()) << ends;	\
	Tcl_AppendResult(ip, tclout.str(), 0);			\
	return TCL_ERROR;					\
    }								\
}

#undef DO_GOTO
#define DO_GOTO(x)  						\
{								\
    w_rc_t rc = x;						\
    if (rc)  {							\
	tclout.seekp(ios::beg);                                 \
	tclout << ssh_err_name(rc.err_num()) << ends;		\
	Tcl_AppendResult(ip, tclout.str(), 0);			\
	goto failure;						\
    }								\
}
    
void   dump_pin_hdr(ostream &out, pin_i &handle); // later in this file
void   dump_pin_body(ostrstream &out, pin_i &handle,
	smsize_t start, smsize_t length, Tcl_Interp *ip); // later in this file

w_rc_t dump_scan(scan_file_i &scan, ostream &out); // later in this file

inline int
streq(char* s1, char* s2)
{
    return !strcmp(s1, s2);
}

vec_t &
parse_vec(const char *c, int len) 
{
    DBG(<<"parse_vec("<<c<<")");
    static vec_t junk;
    int 	i;

    junk.reset();
    if(::strncmp(c, "zvec", 4)==0) {
	c+=4;
	i = atoi(c);
	if(i>=0) {
	    junk.set(zvec_t(i));
	    DBG(<<"returns zvec_t("<<i<<")");
	    return junk;
	}
    }
    DBG(<<"returns normal vec_t()");
    junk.put(c, len);
    return junk;
}

vid_t 
make_vid_from_lvid(const char* lv)
{
    // see if the lvid string represents a long lvid)
    if (!strchr(lv, '.')) {
	vid_t vid;
	GNUG_BUG_12(istrstream(lv, strlen(lv)) ) >> vid;
	return vid;
    }
    lvid_t lvid;
    GNUG_BUG_12(istrstream(lv, strlen(lv)) ) >> lvid;
    return vid_t(lvid.low);
}

//ss_m::ndx_t cvt2ndx_t(char* s)
static smlevel_0::ndx_t 
cvt2ndx_t(char* s)
{
    static struct nda_t {
	char* 			name;
	smlevel_0::ndx_t 	type;
    } nda[] = {
	{ "btree",	ss_m::t_btree },
	{ "uni_btree", 	ss_m::t_uni_btree },
	{ "rtree",	ss_m::t_rtree },
	{ "rdtree",	ss_m::t_rdtree },
	{ 0,		ss_m::t_bad_ndx_t }
    };

    for (nda_t* p = nda; p->name; p++)  {
	if (streq(s, p->name))  return p->type;
    }
    return sm->t_bad_ndx_t;
}

static const char *
cvt2string(scan_index_i::cmp_t i)
{
    switch(i) {
    case scan_index_i::gt: return ">";
    case scan_index_i::ge: return ">=";
    case scan_index_i::lt: return "<";
    case scan_index_i::le: return "<=";
    case scan_index_i::eq: return "==";
    default: return "BAD";
    }
    return "BAD";
}
static scan_index_i::cmp_t 
cvt2cmp_t(char* s)
{
    if (streq(s, ">"))  return scan_index_i::gt;
    if (streq(s, ">=")) return scan_index_i::ge;
    if (streq(s, "<"))  return scan_index_i::lt;
    if (streq(s, "<=")) return scan_index_i::le;
    if (streq(s, "==")) return scan_index_i::eq;
    return scan_index_i::bad_cmp_t;
}

static lockid_t 
cvt2lockid_t(char* str)
{
    stid_t stid;
    lpid_t pid;
    rid_t rid;
    kvl_t kvl;
    vid_t vid;
    int len = strlen(str);

    istrstream ss(str, len);
    lockid_t n;

    /* This switch conversion is used, because the previous,
       "try everything" was causing problems with I/O streams.
       It's better anyway, because it is deterministic instead
       of priority ordered. */
    switch (str[0]) {
    case 's':
	    ss >> stid;
	    break;
    case 'r':
	    ss >> rid;
	    break;
    case 'p':
	    ss >> pid;
	    break;
    case 'k':
	    ss >> kvl;
	    break;
    default:
	    /* volume id's are just XXX.YYY.  They should be changed  */
	    ss >> vid;
	    break;
    }

    if (!ss) {
	    cerr << "cvt2lockid_t: bad lockid -- " << str << endl;
	    abort();
	    return n;
    }

    switch (str[0]) {
    case 's':
	    n = lockid_t(stid);
	    break;
    case 'r':
	    n = lockid_t(rid);
	    break;
    case 'p':
	    n = lockid_t(pid);
	    break;
    case 'k':
	    n = lockid_t(kvl);
	    break;
    default:
	    n = lockid_t(vid);
	    break;
    }

    return n;
}

static lock_mode_t
cvt2lock_mode(char* s)
{
    for (int i = lock_base_t::MIN_MODE; i <= lock_base_t::MAX_MODE; i++)  {
	if (strcmp(s, lock_base_t::mode_str[i]) == 0)
	    return (lock_mode_t) i;
    }
    cerr << "cvt2lock_mode: bad lock mode" << endl;
    W_FATAL(eINTERNAL);
    return IS;
}

static lock_duration_t 
cvt2lock_duration(char* s)
{
    for (int i = 0; i < lock_base_t::NUM_DURATIONS; i++) {
	if (strcmp(s, lock_base_t::duration_str[i]) == 0)
	    return (lock_duration_t) i;
    }

    cerr << "cvt2lock_duration: bad lock duration" << endl;
    W_FATAL(eINTERNAL);
    return t_long;
}

static sm_store_property_t
cvt2store_property(char* s)
{
    sm_store_property_t prop = t_regular;
    if (strcmp(s, "tmp") == 0)  {
	prop = t_temporary;
    } else if (strcmp(s, "regular") == 0)  {
	prop = t_regular;
    } else if (strcmp(s, "no_log") == 0) {
	prop = t_no_log;
    } else {
	W_FATAL(eINTERNAL);
    }
    return prop;
}

static sob_cmp_t 
cvt2sob_cmp_t(char* s)
{
    if (streq(s, "||"))  return t_overlap;
    if (streq(s, "/")) return t_cover;
    if (streq(s, "=="))  return t_exact;
    if (streq(s, "<<")) return t_inside;
    return t_bad;
}

static ss_m::concurrency_t 
cvt2concurrency_t(char* s)
{
    if (streq(s, "t_cc_none"))  return ss_m::t_cc_none;
    if (streq(s, "t_cc_record"))  return ss_m::t_cc_record;
    if (streq(s, "t_cc_page"))  return ss_m::t_cc_page;
    if (streq(s, "t_cc_file"))  return ss_m::t_cc_file;
    if (streq(s, "t_cc_kvl"))  return ss_m::t_cc_kvl;
    if (streq(s, "t_cc_im"))  return ss_m::t_cc_im;
    if (streq(s, "t_cc_append"))  return ss_m::t_cc_append;
    return ss_m::t_cc_bad;
}

static int
check(Tcl_Interp* ip, char* s, int ac, int n1, int n2 = 0, int n3 = 0,
      int n4 = 0)
{
    if (ac != n1 && ac != n2 && ac != n3 && ac != n4) {
	if (s[0])  {
	    Tcl_AppendResult(ip, "wrong # args; should be \"", s,
			     "\"", 0);
	} else {
	    Tcl_AppendResult(ip, "wrong # args, none expected", 0);
	}
	return -1;
    }
    return 0;
}

inline bool use_logical_id(Tcl_Interp* ip)
{
    extern char* Logical_id_flag_tcl; // from ssh.c

    char* value = Tcl_GetVar(ip, Logical_id_flag_tcl, TCL_GLOBAL_ONLY); 
    w_assert1(value != NULL);
    char result = value[0];
    bool ret = FALSE;

    switch (result) {
    case '1':
	ret = TRUE;
	break;
    case '0':
	ret = FALSE;
	break;
    default:
	W_FATAL(eINTERNAL);
    }
    return ret;
}


static int
t_test_bulkload_int_btree(Tcl_Interp* ip, int ac, char* av[])
{
    if (check(ip, "vid nkeys", ac, 3))
	return TCL_ERROR;

    const n = atoi(av[2]);
    const h = n/2;
    if (use_logical_id(ip))  {
	lfid_t lfid;
	lstid_t lstid;
	GNUG_BUG_12(istrstream(av[1], strlen(av[1]))) >> lstid.lvid;
	lfid.lvid = lstid.lvid;

	DO( sm->create_index(lstid.lvid, sm->t_uni_btree, 
			     t_regular, "i4", 0, lstid.serial) );

	DO( sm->create_file(lfid.lvid, lfid.serial, t_temporary) );
	
	int i;	
	for (i = 0; i < n; i++)  {
	    long k = i - h;
	    lrid_t rid;
	    DO( sm->create_rec( lfid.lvid, lfid.serial, 
				vec_t(&k, sizeof(k)),
				sizeof(k),
				vec_t(&k, sizeof(k)),
				rid.serial) );
	}

	sm_du_stats_t stats;
	DO( sm->bulkld_index(lstid.lvid, lstid.serial,
			     lfid.lvid, lfid.serial,
			     stats) );

	/* verify */
	scan_index_i scan(lstid.lvid, lstid.serial, 
			  scan_index_i::gt, cvec_t::neg_inf, 
			  scan_index_i::lt, cvec_t::pos_inf);
	
	bool eof;
	w_rc_t rc;
	for (i = 0; (!(rc = scan.next(eof)) && !eof) ; i++)  {
	    long k, e;
	    vec_t key, el;
	    key.put(&k, sizeof(k));
	    el.put(&e, sizeof(e));
	    smsize_t klen, elen;
	    klen = elen = sizeof(k);
	    DO( scan.curr(&key, klen, &el, elen));
	    w_assert1(klen == elen && klen == sizeof(k));
	    w_assert1(k == i - h);
	    w_assert1(e == k);
	    cout << "retrieved " << k << endl;
	}
	DO( rc );
	w_assert1(i == n);

	DO( sm->destroy_index(lstid.lvid, lstid.serial) );
	DO( sm->destroy_file(lfid.lvid, lfid.serial) );
    } else {
	stid_t stid;
	stid_t fid;
	DO( sm->create_index(atoi(av[1]), sm->t_uni_btree, 
			     t_regular, "i4", stid) );
	DO( sm->create_file(atoi(av[1]), fid, t_temporary) );

	int i;
	for (i = 0; i < n; i++)  {
	    long k = i - h;
	    rid_t rid;
	    DO( sm->create_rec( fid, 
				vec_t(&k, sizeof(k)), 
				sizeof(k),
				vec_t(&k, sizeof(k)),
				rid) );
	}
	
	sm_du_stats_t stats;
	DO( sm->bulkld_index(stid, fid, stats) );

	scan_index_i scan(stid,
			  scan_index_i::gt, cvec_t::neg_inf, 
			  scan_index_i::lt, cvec_t::pos_inf);
	
	bool eof;
	w_rc_t rc;
	for (i = 0; (!(rc = scan.next(eof)) && !eof) ; i++)  {
	    long k, e;
	    vec_t key, el;
	    key.put(&k, sizeof(k));
	    el.put(&e, sizeof(e));
	    smsize_t klen, elen;
	    klen = elen = sizeof(k);
	    DO( scan.curr(&key, klen, &el, elen));
	    w_assert1(klen == elen && klen == sizeof(k));
	    w_assert1(k == i - h);
	    w_assert1(e == k);
	}
	DO( rc );
	w_assert1(i == n);

	DO( sm->destroy_index(stid) );
	DO( sm->destroy_file(fid) );
    }
	
    return TCL_OK;
}

static int
t_test_int_btree(Tcl_Interp* ip, int ac, char* av[])
{
    if (check(ip, "vid nkeys", ac, 3))
	return TCL_ERROR;

    const n = atoi(av[2]);
    assert(n > 0);
    const h = n/2;
    const low = 0 - h;
    const high = (n-1) - h;
    const zero = 0;
    int highm1 = (high - 1)>low?(high-1): low;
    int highm2 = (high - 2)>low?(high-2): low;
    int lowp1 = (low + 1)>high?(low+1): high;
    int lowp2 = (low + 2)>high?(low+2): high;

    lstid_t lstid;
    stid_t stid;
    int i;

    /*
     * Create the index
     */
    if (use_logical_id(ip))  {
	GNUG_BUG_12(istrstream(av[1], strlen(av[1]))) >> lstid.lvid;
	DO( sm->create_index(lstid.lvid, sm->t_uni_btree, 
			     t_regular, "i4", 0, lstid.serial) );

        vec_t key(&i, sizeof(i)); 
	for (i = low; i <= high; i++)  {
	    DO( sm->create_assoc( lstid.lvid, lstid.serial, key, key));
	    cout << "inserting " << i << endl;
	}
    } else {
	DO( sm->create_index(atoi(av[1]), sm->t_uni_btree, 
			     t_regular, "i4", stid) );
        vec_t key(&i, sizeof(i)); 
	for (i = low; i <= high; i++)  {
	    DO( sm->create_assoc( stid, key, key));
	    cout << "inserting " << i << endl;
	}
    }

    /*
     * Test many scans
     */
    int first, last;
    scan_index_i::cmp_t op1, op2;
    int last_test=15;
    vec_t first_vec(&first, sizeof(first));
    vec_t last_vec(&last, sizeof(last));

    for (int bounds_set = 0; bounds_set <= last_test; bounds_set++) {
	switch(bounds_set) {
	case 0:
	    first = low;
	    last = high;
	    break;
	case 1:
	    first = low;
	    last = low/2;
	    break;
	case 2:
	    first = low;
	    last = lowp1;
	    break;
	case 3:
	    first = low;
	    last = lowp2;
	    break;
	case 4:
	    first = lowp1;
	    last = lowp2;
	    break;
	case 5:
	    first = low;
	    last = zero;
	    break;
	case 6:
	    // not used
	    break;
	case 7:
	    first = low;
	    last = low;
	    break;

	case 8:
	    first = high/2;
	    last = high;
	    break;
	case 9:
	    first = highm1;
	    last = high;
	    break;
	case 10:
	    first = highm2;
	    last = high;
	    break;
	case 11:
	    first = high;
	    last = high;
	    break;
	case 12:
	    first = zero;
	    last = high;
	    break;
	case 13:
	    first = zero;
	    last = zero;
	    break;
	case 14:
	    first = low-1;
	    last = high;
	    break;
	case 15:
	    first = low;
	    last = high+1;
	    break;
	case 16:
	    first = low-10;
	    last = high+100;
	    break;
	default:
	    abort();
	}
	cout << "SCAN# " <<bounds_set ;
	for (int scan_type = 0; scan_type <= 8; scan_type++) {
	    int expected = 0;
	    {
		// things get a bit difficult if first<low or
		// last>high.

		switch(scan_type) {
		case 0:
		    op1 = scan_file_i::ge;
		    op2 = scan_file_i::le;
		    expected = (last - first) + 1;
		    if(expected < 0) expected = 0;
		    break;
		case 1:
		    op1 = scan_file_i::gt;
		    op2 = scan_file_i::le;
		    expected = last - first;
		    if(expected < 0) expected = 0;
		    break;
		case 2:
		    op1 = scan_file_i::ge;
		    op2 = scan_file_i::lt;
		    expected = last - first;
		    if(expected < 0) expected = 0;
		    break;
		case 3:
		    op1 = scan_file_i::gt;
		    op2 = scan_file_i::lt;
		    expected = (last - first) - 1;
		    if(expected < 0) expected = 0;
		    break;
		case 4:
		    op1 = scan_file_i::ge;
		    op2 = scan_file_i::eq;
		    expected = (last >= first) ? 1 : 0;
		    break;
		case 5:
		    op1 = scan_file_i::gt;
		    op2 = scan_file_i::eq;
		    expected = (last > first) ? 1 : 0;
		    break;
		case 6:
		    op1 = scan_file_i::eq;
		    op2 = scan_file_i::le;
		    expected = (last >= first) ? 1 : 0;
		    break;
		case 7:
		    op1 = scan_file_i::eq;
		    op2 = scan_file_i::lt;
		    expected = (last > first) ? 1 : 0;
		    break;
		case 8:
		    op1 = scan_file_i::eq;
		    op2 = scan_file_i::eq;
		    expected = (last == first) ? 1 : -1;
		    break;
		default:
		    abort();
		}
		if(expected > 0) {
		    switch (op1) {
			case scan_file_i::ge:
			    if((first<low) && op2 != scan_file_i::eq) 
				expected -= (low-first);
			    break;
			case scan_file_i::gt:
			    break;
			case scan_file_i::eq:
			    if(first<low) expected = 0;
			    break;
			case scan_file_i::le:
			case scan_file_i::lt:
			default:
			    break;
		    }
		    switch(op2) {
			case scan_file_i::le:
			    if((last>high) && op1 != scan_file_i::eq) 
				    expected -= (last-high);
			    break;
			case scan_file_i::lt:
			    break;
			case scan_file_i::eq:
			    if(last>high) expected = 0;
			    break;
			case scan_file_i::ge:
			case scan_file_i::gt:
			default:
			    break;
		    }
		}
	    }
	    cout << " scanning " 
		 << cvt2string(op1) <<" " << first 
		 << " " 
		 << cvt2string(op2) <<" " << last <<endl;

	    /*
	     * Test scan of entire index
	     */
	    char scan_space[sizeof(scan_index_i)];
	    scan_index_i* scan = 0;
	    if (use_logical_id(ip))  {
		scan = new (scan_space) scan_index_i(lstid.lvid, lstid.serial, 
				  op1, first_vec, 
				  op2, last_vec);
	    } else {
		scan = new (scan_space) scan_index_i(stid,
				  op1, first_vec, 
				  op2, last_vec);
	    }
	    if(scan && scan->error_code()) {
		if(expected <0) {
			// expected this error
			scan->~scan_index_i();
			continue;
		} else {
		    cerr << " unexpected error: " << scan->error_code() 
			<< endl;
		}
	    }

	    bool eof;
	    w_rc_t rc;

	    int num_scanned = 0;
	    int tmp=first;
	    {
		// compute starting place...
		if(first<low) {
		    tmp += low-first;
		}
	    	if(op1==scan_file_i::gt) tmp++;
	    }
	    for (i = tmp;
		(!(rc = scan->next(eof)) && !eof) ; 
		i++)  {

		long k, e;
		vec_t key, el;
		key.put(&k, sizeof(k));
		el.put(&e, sizeof(e));
		smsize_t klen, elen;
		klen = elen = sizeof(k);
		DO( scan->curr(&key, klen, &el, elen));
	    cout << "retrieved " << k << endl;
		num_scanned++;
		w_assert1(klen == elen && klen == sizeof(k));
		if(op2==scan_file_i::eq) {
		    w_assert1(k == last);
		} else {
		    // too hard to get the right thing to compare
		    // w_assert1(k == i);
		}
		w_assert1(e == k);
	    }
	    scan->finish();
	    DO( rc );

	    if(expected>0) {
		int boundary ;
		if(op2==scan_file_i::eq) {
		    // boundary is a test on i, not the key
		    boundary = (op1 == scan_file_i::gt ? first+1 : first);
		} else if(op2==scan_file_i::lt) {
		    boundary = last-1;
		} else {
		    boundary = last;
		}
		boundary++; // i is incremented...
		if(op1==scan_file_i::eq) {
		    boundary = first;
		}
		if( i < boundary) {
		    cerr << "PREMATURE EOF : eof at " << i
		    << " expected eof at " << boundary << endl;
		}
		// w_assert1(i == boundary);
	    }

	    if( num_scanned != expected ) {
		cerr << "WRONG NUMBER : expected " << expected
		<< " got " << num_scanned << endl;
	    }
	    w_assert1(num_scanned == expected);

	    scan->~scan_index_i();

	}
    }

    if (use_logical_id(ip))  {
	DO( sm->destroy_index(lstid.lvid, lstid.serial) );
    } else {
	DO( sm->destroy_index(stid) );
    }
    
    return TCL_OK;
}

static int
t_checkpoint(Tcl_Interp* ip, int ac, char* /*av*/[])
{
    if (check(ip, "", ac, 1))  return TCL_ERROR;
    sm->checkpoint();
    return TCL_OK;
}

static int
t_sleep(Tcl_Interp* ip, int ac, char* av[])
{
    if (check(ip, "milliseconds", ac, 2))  return TCL_ERROR;
    int timeout = atoi(av[1]);
    me()->sleep(timeout);
    return TCL_OK;
}


static int
t_begin_xct(Tcl_Interp* ip, int ac, char*[])
{
    if (check(ip, "", ac, 1)) return TCL_ERROR;
    DO( sm->begin_xct() );
    return TCL_OK;
}

static int
t_commit_xct(Tcl_Interp* ip, int ac, char* av[])
{
    if (check(ip, "[lazy]", ac, 1, 2)) return TCL_ERROR;
    bool lazy = false;
    if (ac == 2) lazy = (strcmp(av[1], "lazy") == 0);
    DO( sm->commit_xct(lazy) );
    return TCL_OK;
}

static  int
t_chain_xct(Tcl_Interp* ip, int ac, char* av[])
{
    if (check(ip, "[lazy]", ac, 1, 2))  return TCL_ERROR;
    bool lazy = false;
    if (ac == 2) lazy = (strcmp(av[1], "lazy") == 0);
    DO( sm->chain_xct(lazy) );
    return TCL_OK;
}

#ifndef DEBUG
/* defined in common in the debugging case, but needed here */
inline istream &
operator>>(istream &i, gtid_t &t)
{
    // for purify:
    t._length = 0;
    memset(t._opaque, '\0', sizeof(t._opaque));

    i.width(sizeof(t._opaque));
    i >> t._opaque;
    t._length = strlen((const char *)t._opaque);
    // assert(t._opaque[t._length]=0);
    return i;
}
#endif

static int
t_enter2pc(Tcl_Interp* ip, int ac, char* av[])
{
    gtid_t	g;

    if (check(ip, "gtid_t", ac, 2)) return TCL_ERROR;
    GNUG_BUG_12(istrstream(av[1], strlen(av[1]))) >> g;
    DO( sm->enter_2pc(g));
    return TCL_OK;
}

static int
t_recover2pc(Tcl_Interp* ip, int ac, char* av[])
{
    if (check(ip, "gtid_t", ac, 2)) return TCL_ERROR;

    gtid_t	g;
    tid_t 		t;
    GNUG_BUG_12(istrstream(av[1], strlen(av[1]))) >> g;
    DO( sm->recover_2pc(g, true, t));
    tclout.seekp(ios::beg);
    tclout << t << ends;
    Tcl_AppendResult(ip, tclout.str(), 0);
    return TCL_OK;
}

static int
t_prepare_xct(Tcl_Interp* ip, int ac, char* /*av*/[])
{
    if (check(ip, "", ac, 1)) return TCL_ERROR;
    vote_t	v;
    DO( sm->prepare_xct(v));
    tclout.seekp(ios::beg);
    tclout << v << ends;
    Tcl_AppendResult(ip, tclout.str(), 0);
    return TCL_OK;
}

static int
t_abort_xct(Tcl_Interp* ip, int ac, char* /*av*/[])
{
    if (check(ip, "", ac, 1)) return TCL_ERROR;
    DO( sm->abort_xct());
    return TCL_OK;
}

static int
t_save_work(Tcl_Interp* ip, int ac, char* /*av*/[])
{
    if (check(ip, "", ac, 1))  return TCL_ERROR;
    sm_save_point_t sp;
    DO( sm->save_work(sp) );
    tclout.seekp(ios::beg);
    tclout << sp << ends;
    Tcl_AppendResult(ip, tclout.str(), 0);
    return TCL_OK;
}

static int
t_rollback_work(Tcl_Interp* ip, int ac, char* av[])
{
    if (check(ip, "savepoint", ac, 2))  return TCL_ERROR;
    sm_save_point_t sp;
    GNUG_BUG_12(istrstream(av[1], strlen(av[1]))) >> sp;
    DO( sm->rollback_work(sp));
    // keep compiler quiet about unused parameters
    if (av) {}

    return TCL_OK;
}

static int
t_open_quark(Tcl_Interp* ip, int ac, char* av[])
{
    if (check(ip, "", ac, 1))  return TCL_ERROR;
    // keep compiler quiet about unused parameters
    if (av) {}

    sm_quark_t quark;
    DO(quark.open());
    tclout.seekp(ios::beg);
    tclout << quark << ends;
    Tcl_AppendResult(ip, tclout.str(), 0);
    return TCL_OK;
}

static int
t_close_quark(Tcl_Interp* ip, int ac, char* av[])
{
    if (check(ip, "quark [release]", ac, 2, 3))  return TCL_ERROR;
    sm_quark_t quark;
    GNUG_BUG_12(istrstream(av[1], strlen(av[1]))) >> quark;
    bool release_locks = false;
    if (ac == 3) {
	if (strcmp("release", av[2]) == 0) {
	    release_locks = true;
	} else {
	    return TCL_ERROR;
	}
    }
    DO( quark.close(release_locks));
    return TCL_OK;
}

static int
t_xct(Tcl_Interp* ip, int ac, char* /*av*/[])
{
    if (check(ip, "", ac, 1))  return TCL_ERROR;

    tclout.seekp(ios::beg);
    tclout << xct() <<  ends;
    Tcl_AppendResult(ip, tclout.str(), 0);
    return TCL_OK;
}

static int
t_attach_xct(Tcl_Interp* ip, int ac, char* av[])
{
    if (check(ip, "xct", ac, 2))
	return TCL_ERROR;
    
    xct_t* x = (xct_t*) strtol(av[1], 0, 0);
    me()->attach_xct(x);
    return TCL_OK;
}

static int
t_detach_xct(Tcl_Interp* ip, int ac, char* av[])
{
    if (check(ip, "xct", ac, 2))
	return TCL_ERROR;
    
    xct_t* x = (xct_t*) strtol(av[1], 0, 0);
    me()->detach_xct(x);
    return TCL_OK;
}

static int
t_state_xct(Tcl_Interp* ip, int ac, char* av[])
{
    if (check(ip, "xct", ac, 2))
	return TCL_ERROR;
    
    xct_t* x = (xct_t*) strtol(av[1], 0, 0);
	ss_m::xct_state_t t = sm->state_xct(x);
    tclout.seekp(ios::beg);
    tclout << t << ends;
    Tcl_AppendResult(ip, tclout.str(), 0);
    return TCL_OK;
}

static int
t_tid_to_xct(Tcl_Interp* ip, int ac, char* av[])
{
    if (check(ip, "xct", ac, 2))
	return TCL_ERROR;

	tid_t t;
	GNUG_BUG_12(istrstream(av[1], strlen(av[1])) ) >> t;
    
    xct_t* x = sm->tid_to_xct(t);
    tclout.seekp(ios::beg);
    tclout << x << ends;
    Tcl_AppendResult(ip, tclout.str(), 0);
    return TCL_OK;
}
static int
t_xct_to_tid(Tcl_Interp* ip, int ac, char* av[])
{
    if (check(ip, "xct", ac, 2))
	return TCL_ERROR;
    
    xct_t* x = (xct_t*) strtol(av[1], 0, 0);
    tid_t t = sm->xct_to_tid(x);
    tclout.seekp(ios::beg);
    tclout << t << ends;
    Tcl_AppendResult(ip, tclout.str(), 0);
    return TCL_OK;
}

static int
t_force_buffers(Tcl_Interp* ip, int ac, char*[])
{
    if (check(ip, "[flush]", ac, 1,2)) return TCL_ERROR;
    bool flush = false;
    if (ac == 2) {
	flush = true;
    }
    DO( sm->force_buffers(flush) );
    return TCL_OK;
}
 

static int
t_force_vol_hdr_buffers(Tcl_Interp* ip, int ac, char* av[])
{
    if (check(ip, "lvid", ac, 2)) return TCL_ERROR;
    // keep compiler quiet about unused parameters
    if (av) {}

    lvid_t lvid;
    GNUG_BUG_12(istrstream(av[1], strlen(av[1])) ) >> lvid;
    DO( sm->force_vol_hdr_buffers(lvid) );
    return TCL_OK;
}
 

static int
t_snapshot_buffers(Tcl_Interp* ip, int ac, char* av[])
{
    if (check(ip, "", ac, 1))
	return TCL_ERROR;
    // keep compiler quiet about unused parameters
    if (av) {}

    u_int ndirty, nclean, nfree, nfixed;
    DO( sm->snapshot_buffers(ndirty, nclean, nfree, nfixed) );

    tclout.seekp(ios::beg);
    tclout << "ndirty " << ndirty 
      << "  nclean " << nclean
      << "  nfree " << nfree
      << "  nfixed " << nfixed
      << ends;
    
    Tcl_AppendResult(ip, tclout.str(), 0);
    return TCL_OK;
}

    
static int
t_gather_stats(Tcl_Interp* ip, int ac, char* av[])
{
    if (check(ip, "[reset]", ac, 1,2))
	return TCL_ERROR;

    bool reset = FALSE;
    if (ac == 2) {
	if (strcmp(av[1], "reset") == 0) {
	    reset = TRUE;
	}
    }

    sm_stats_info_t stats;

    DO( sm->gather_stats(stats, reset));

    tclout.seekp(ios::beg);
    tclout << stats << ends;
    Tcl_AppendResult(ip, tclout.str(), 0);
    return TCL_OK;
}

static int
t_config_info(Tcl_Interp* ip, int ac, char*[])
{
    if (check(ip, "", ac, 1))
	return TCL_ERROR;

    sm_config_info_t info;

    DO( sm->config_info(info));

    tclout.seekp(ios::beg);
    tclout << info << ends;
    Tcl_AppendResult(ip, tclout.str(), 0);
    return TCL_OK;
}

static int
t_set_disk_delay(Tcl_Interp* ip, int ac, char* av[])
{
    if (check(ip, "milli_sec", ac, 2))
	return TCL_ERROR;

    uint4 msec = atoi(av[1]);
    DO( sm->set_disk_delay(msec));

    return TCL_OK;
}

static int
t_start_log_corruption(Tcl_Interp* ip, int ac, char* /*av*/[])
{
    if (check(ip, "", ac, 1))
	return TCL_ERROR;

    DO( sm->start_log_corruption());

    return TCL_OK;
}

static int
t_sync_log(Tcl_Interp* ip, int ac, char* /*av*/[])
{
    if (check(ip, "", ac, 1))
	return TCL_ERROR;

    DO( sm->sync_log());

    return TCL_OK;
}

static int
t_vol_root_index(Tcl_Interp* ip, int ac, char* av[])
{
    if (check(ip, "vid", ac, 2))
	return TCL_ERROR;


    if (use_logical_id(ip)) {
	lstid_t	 iid;

	GNUG_BUG_12(istrstream(av[1], strlen(av[1])) ) >> iid.lvid;
	DO( sm->vol_root_index(iid.lvid, iid.serial));
	tclout.seekp(ios::beg);
	tclout << iid << ends;

    } else {
	vid_t   vid;
	stid_t iid;

	GNUG_BUG_12(istrstream(av[1], strlen(av[1])) ) >> vid;
	DO( sm->vol_root_index(vid, iid));
	tclout.seekp(ios::beg);
	tclout << iid << ends;

    }
    Tcl_AppendResult(ip, tclout.str(), 0);
    return TCL_OK;
}

static int
t_dump_buffers(Tcl_Interp* ip, int ac, char*[])
{
    if (check(ip, "", ac, 1)) return TCL_ERROR;
    sm->dump_buffers();
    return TCL_OK;
}

static int
t_get_volume_quota(Tcl_Interp* ip, int ac, char* av[])
{
    if (check(ip, "vid", ac, 2))  return TCL_ERROR;
    smksize_t capacity, used;
    lvid_t lvid;
    GNUG_BUG_12(istrstream(av[1], strlen(av[1])) ) >> lvid;
    DO( sm->get_volume_quota(lvid, capacity, used) );

    tclout.seekp(ios::beg);
    tclout << capacity << " " << used << ends;
    Tcl_AppendResult(ip, tclout.str(), 0);
    return TCL_OK;
}

static int
t_get_device_quota(Tcl_Interp* ip, int ac, char* av[])
{
    if (check(ip, "device", ac, 2))  return TCL_ERROR;
    smksize_t capacity, used;
    DO( sm->get_device_quota(av[1], capacity, used) );
    tclout.seekp(ios::beg);
    tclout << capacity << " " << used << ends;
    Tcl_AppendResult(ip, tclout.str(), 0);
    return TCL_OK;
}

static int
t_format_dev(Tcl_Interp* ip, int ac, char* av[])
{
    if (check(ip, "device size_in_KB 'force/noforce'", ac, 4)) 
	return TCL_ERROR;
    bool force = FALSE;
    if (av[3] && streq(av[3], "force")) force = TRUE;
    DO( sm->format_dev(av[1], atoi(av[2]),force));
    return TCL_OK;
}

static int
t_mount_dev(Tcl_Interp* ip, int ac, char* av[])
{
    // return volume count
    if (check(ip, "device [local_lvid_for_vid]", ac, 2, 3))
	return TCL_ERROR;

    u_int volume_count=0;
    devid_t devid;

    if (use_logical_id(ip)) {
	if (ac != 2) return TCL_ERROR;
	DO( sm->mount_dev(av[1], volume_count, devid));
	tclout.seekp(ios::beg);
	tclout << volume_count << ends;
    } else {
	if (ac == 3) {
	    DO( sm->mount_dev(av[1], volume_count, devid, make_vid_from_lvid(av[2])));
	} else {
	    DO( sm->mount_dev(av[1], volume_count, devid));
	}
	tclout.seekp(ios::beg);
	tclout << volume_count << ends;
    }
    Tcl_AppendResult(ip, tclout.str(), 0);
    return TCL_OK;
}

static int
t_dismount_dev(Tcl_Interp* ip, int ac, char* av[])
{
    if (check(ip, "device", ac, 2))
	return TCL_ERROR;
    // keep compiler quiet about unused parameters
    if (av) {}
    
    DO( sm->dismount_dev(av[1]));
    return TCL_OK;
}

static int
t_dismount_all(Tcl_Interp* ip, int ac, char* av[])
{
    if (check(ip, "", ac, 1))
	return TCL_ERROR;
    // keep compiler quiet about unused parameters
    if (av) {}

    DO( sm->dismount_all());
    return TCL_OK;
}

static int
t_list_devices(Tcl_Interp* ip, int ac, char* av[])
{
    if (check(ip, "", ac, 1))
	return TCL_ERROR;
    // keep compiler quiet about unused parameters
    if (av) {}

    u_int count;
    const char** dev_list; 
    devid_t* devid_list; 
    DO( sm->list_devices(dev_list, devid_list, count));

    // return list of volumes
    for (u_int i = 0; i < count; i++) {
	tclout.seekp(ios::beg);
	tclout << dev_list[i] << " " << devid_list[i] << ends;
	Tcl_AppendElement(ip, tclout.str());
    }
    if (count > 0) {
	delete [] dev_list;
	delete [] devid_list;
    }
    return TCL_OK;
}

static int
t_list_volumes(Tcl_Interp* ip, int ac, char* av[])
{
    if (check(ip, "device", ac, 2))
	return TCL_ERROR;
    u_int count;
    lvid_t* lvid_list; 
    DO( sm->list_volumes(av[1], lvid_list, count));

    // return list of volumes
    for (u_int i = 0; i < count; i++) {
	tclout.seekp(ios::beg);
	tclout << lvid_list[i] << ends;
	Tcl_AppendElement(ip, tclout.str());
    }
    if (count > 0) delete [] lvid_list;
    return TCL_OK;
}

static int
t_generate_new_lvid(Tcl_Interp* ip, int ac, char* av[])
{
    if (check(ip, "", ac, 1))
	return TCL_ERROR;
    // keep compiler quiet about unused parameters
    if (av) {}

    lvid_t lvid;
    DO( sm->generate_new_lvid(lvid));

    // return list of volumes
    tclout.seekp(ios::beg);
    tclout << lvid << ends;
    Tcl_AppendResult(ip, tclout.str(), 0);
    return TCL_OK;
}

static int
t_create_vol(Tcl_Interp* ip, int ac, char* av[])
{
    if (check(ip, "device lvid quota_in_KB ['skip_raw_init']", ac, 4, 5)) 
	return TCL_ERROR;

    lvid_t lvid;
    GNUG_BUG_12(istrstream(av[2], strlen(av[2])) ) >> lvid;
    uint4 quota = atoi(av[3]);
    bool skip_raw_init = FALSE;
    if (av[4] && streq(av[4], "skip_raw_init")) skip_raw_init = TRUE;

    if (use_logical_id(ip)) {
	DO( sm->create_vol(av[1], lvid, quota, skip_raw_init));
    } else {
	DO( sm->create_vol(av[1], lvid, quota, skip_raw_init, make_vid_from_lvid(av[2])));
    }
    return TCL_OK;
}

static int
t_destroy_vol(Tcl_Interp* ip, int ac, char* av[])
{
    if (check(ip, "lvid", ac, 2)) 
	return TCL_ERROR;

    lvid_t lvid;
    GNUG_BUG_12(istrstream(av[1], strlen(av[1])) ) >> lvid;
    DO( sm->destroy_vol(lvid));
    return TCL_OK;
}

static int
t_add_logical_id_index(Tcl_Interp* ip, int ac, char* av[])
{
    if (check(ip, "lvid reserved", ac, 3)) return TCL_ERROR;
   
    lvid_t lvid;
    GNUG_BUG_12(istrstream(av[1], strlen(av[1])) ) >> lvid;
    uint4 reserved = atoi(av[2]);

    DO(sm->add_logical_id_index(lvid, reserved, reserved));
    return TCL_OK;
}

static int
t_has_logical_id_index(Tcl_Interp* ip, int ac, char* av[])
{
    if (check(ip, "lvid", ac, 2)) return TCL_ERROR;
   
    lvid_t lvid;
    GNUG_BUG_12(istrstream(av[1], strlen(av[1])) ) >> lvid;
    
    bool has_index;
    DO(sm->has_logical_id_index(lvid, has_index));
    if (has_index) {
	Tcl_AppendResult(ip, "TRUE", 0);
    } else {
	Tcl_AppendResult(ip, "FALSE", 0);
    }
    return TCL_OK;
}

static int
t_get_du_statistics(Tcl_Interp* ip, int ac, char* av[])
{
    // borthakur : June, 1994
    if (check(ip, "vid|stid [pretty] [noaudit]", ac, 2, 3, 4))  return TCL_ERROR;
    sm_du_stats_t stats;

    bool pretty = FALSE;
    bool audit = TRUE;
    if (ac == 4) {
	if (strcmp(av[3], "noaudit") == 0) {
	    audit = FALSE;
	}
	if (strcmp(av[3], "pretty") == 0) {
	    pretty = TRUE;
	}
    }
    if (ac >= 3) {
	if (strcmp(av[2], "pretty") == 0) {
	    pretty = TRUE;
	}
	if (strcmp(av[2], "noaudit") == 0) {
	    audit = FALSE;
	}
    }
    if( ac < 2) {
	Tcl_AppendResult(ip, "need storage id or volume id", 0);
	return TCL_ERROR;
    }

    char* str = av[1];
    int len = strlen(av[1]);

    if (use_logical_id(ip)) {
	lstid_t stid;
	lvid_t vid;

	istrstream s(str, len); s  >> stid;
	istrstream v(str, len); v  >> vid;

	if (s) {
	    DO( sm->get_du_statistics(stid.lvid, stid.serial, stats, audit) );
	} else {
	    // assume v is correct
	    DO( sm->get_du_statistics(vid, stats, audit) );
	}
    } else {
	stid_t stid;
	vid_t vid;

	istrstream s(str, len); s  >> stid;
	istrstream v(str, len); v  >> vid;

	if (s) {
	    DO( sm->get_du_statistics(stid, stats, audit) );
	} else if (v) {
	    DO( sm->get_du_statistics(vid, stats, audit) );
	} else {
	    cerr << "bad ID for " << av[0] << " -- " << str << endl;
	    return TCL_ERROR;
	}
    }
    if (pretty) stats.print(cout);

    tclout.seekp(ios::beg);
    tclout <<  stats << ends; 
    Tcl_AppendResult(ip, tclout.str(), 0);

    return TCL_OK;
}

static int
t_set_debug(Tcl_Interp* ip, int ac, char* av[])
{
    if (check(ip, "[flag_string]", ac, 1, 2)) return TCL_ERROR;
  
    //
    // If "" is used, debug prints are turned off
    // Always returns the previous setting
    //
    Tcl_AppendResult(ip,  _debug.flags(), 0);
    if(ac>1) {
	 _debug.setflags(av[1]);
    }
    return TCL_OK;
}

static int
t_set_store_property(Tcl_Interp* ip, int ac, char* av[])
{
    if (check(ip, "fid property", ac, 3))  
	return TCL_ERROR;

    sm_store_property_t property = cvt2store_property(av[2]);

    if (use_logical_id(ip))  {
	lstid_t fid;
	GNUG_BUG_12(istrstream(av[1], strlen(av[1])) ) >> fid;
	DO( sm->set_store_property(fid.lvid, fid.serial, property) );
    } else {
	stid_t fid;
	GNUG_BUG_12(istrstream(av[1], strlen(av[1])) ) >> fid;
	DO( sm->set_store_property(fid, property) );
    }

    return TCL_OK;
}

static int
t_get_store_property(Tcl_Interp* ip, int ac, char* av[])
{
    if (check(ip, "fid", ac, 3))  
	return TCL_ERROR;

    sm_store_property_t property;

    if (use_logical_id(ip))  {
	lstid_t fid;
	GNUG_BUG_12(istrstream(av[1], strlen(av[1])) ) >> fid;
	DO( sm->get_store_property(fid.lvid, fid.serial, property) );
    } else {
	stid_t fid;
	GNUG_BUG_12(istrstream(av[1], strlen(av[1])) ) >> fid;
	DO( sm->get_store_property(fid, property) );
    }

    tclout.seekp(ios::beg);
    tclout << property << ends;

    Tcl_AppendResult(ip, tclout.str(), 0);
    return TCL_OK;
}

static int
t_create_index(Tcl_Interp* ip, int ac, char* av[])
{
    if (check(ip, "vid ndxtype [\"tmp|regular\"] [keydescr] [small]", ac, 3, 6))
	return TCL_ERROR;

    const char *keydescr="b*1000";

    bool small = false;
    sm_store_property_t property = t_regular;
    if (ac > 3) {
	property = cvt2store_property(av[3]);
    }
    if (ac > 4) {
	keydescr = (const char *)av[4];

	// KLUDGE -- restriction for now
	// only because it's better than no check at all
	if(keydescr[0] != 'b' 
		&& keydescr[0] != 'i'
		&& keydescr[0] != 'u'
	) {
	    Tcl_AppendResult(ip, 
	       "create_index: 4th argument must start with b,i or u", 0);
	    return TCL_ERROR;
		
	}
    }
    if (ac > 5) {
	if(::strcmp(av[5],"small")!=0) {
	    Tcl_AppendResult(ip, 
	       "create_index: 5th argument must be \"small\" or empty", 0);
	    return TCL_ERROR;
	}
	small = true;
	if (! use_logical_id(ip)) {
	   cerr << "ssh shell.c: not using logical ids; 5th argument ignored"
		<< endl;
	}
    }

    if (use_logical_id(ip)) {
	lstid_t lstid;
	GNUG_BUG_12(istrstream(av[1], strlen(av[1])) ) >> lstid.lvid;
	DO( sm->create_index(lstid.lvid, cvt2ndx_t(av[2]),
			     property, keydescr, small?1:10, 
			     lstid.serial) );
	tclout.seekp(ios::beg);
	tclout << lstid << ends;
    } else {
	stid_t stid;
	DO( sm->create_index(atoi(av[1]),
			     cvt2ndx_t(av[2]), property, keydescr,
			     stid));
	tclout.seekp(ios::beg);
	tclout << stid << ends;
    }

    Tcl_AppendResult(ip, tclout.str(), 0);
    return TCL_OK;
}

static int
t_destroy_index(Tcl_Interp* ip, int ac, char* av[])
{
    if (check(ip, "fid", ac, 2))
        return TCL_ERROR;
    
    if (use_logical_id(ip)) {
	lstid_t fid;
	GNUG_BUG_12(istrstream(av[1], strlen(av[1])) ) >> fid;
	DO( sm->destroy_index(fid.lvid, fid.serial) );
    } else {
	stid_t fid;
	GNUG_BUG_12(istrstream(av[1], strlen(av[1])) ) >> fid;
	DO( sm->destroy_index(fid) );
    }

    return TCL_OK;
}

//
// prepare for bulk load:
//  read the input file and put recs into a sort stream
//
static rc_t prepare_for_blkld(sort_stream_i& s_stream, 
			Tcl_Interp* ip, char* fid,
			char* type, char* universe=NULL)
{
    key_info_t info;
    sort_parm_t sp;

    info.where = key_info_t::t_hdr;

    if (!strcmp(type,"string")) {
        info.type = key_info_t::t_string;
    	info.len = 0;
    } else if (!strcmp(type,"char")) {
        info.type = key_info_t::t_char;
    	info.len = 1;
    } else if (!strcmp(type,"int")) {
        info.type = key_info_t::t_int;
    	info.len = 4;
    } else if (!strcmp(type,"float")) {
    	info.len = 4;
        info.type = key_info_t::t_float;
    } else {
        info.type = key_info_t::t_spatial;
	if (universe==NULL) {
            if (check(ip, "stid src type [universe]", 1, 0))
	        return RC(ss_m::eASSERT);
	}
	nbox_t univ(universe);
	info.universe = univ;
    	info.len = univ.klen();
    }
    
    sp.run_size = 3;
    sp.destructive = TRUE;
    scan_file_i* f_scan = 0;

    if (use_logical_id(ip)) {
	lstid_t l_stid;
	GNUG_BUG_12(istrstream(fid, strlen(fid)) ) >> l_stid;
	vid_t tmp_vid;
	W_DO( sm->lvid_to_vid(l_stid.lvid, tmp_vid));
        sp.vol = tmp_vid;
	f_scan = new scan_file_i(l_stid.lvid, l_stid.serial, ss_m::t_cc_file);
    } else {
	stid_t stid;
	GNUG_BUG_12(istrstream(fid, strlen(fid)) ) >> stid;
        sp.vol = stid.vol;
	f_scan = new scan_file_i(stid, ss_m::t_cc_file);
    }

    /*
     * scan the input file, put into a sort stream
     */
    s_stream.init(info, sp);
    pin_i* pin;
    bool eof = FALSE;
    while (!eof) {
	W_DO(f_scan->next(pin, 0, eof));
	if (!eof && pin->pinned()) {
	    vec_t key(pin->hdr(), pin->hdr_size()),
		  el(pin->body(), pin->body_size());
	    W_DO(s_stream.put(key, el));
	}
    }

    delete f_scan;
    
    return RCOK;
}

static int
t_blkld_ndx(Tcl_Interp* ip, int ac, char* av[])
{
    if (check(ip, "stid src [type [universe]]", ac, 3, 4, 5))
	return TCL_ERROR;

    sm_du_stats_t stats;

    if (ac > 3) {
	// using sorted stream
    	sort_stream_i s_stream;

    	if (ac==4) {
            DO( prepare_for_blkld(s_stream, ip, av[2], av[3]) );
    	} else {
            DO( prepare_for_blkld(s_stream, ip, av[2], av[3], av[4]) );
	}

	if (use_logical_id(ip)) {
	    lstid_t l_stid;
	    GNUG_BUG_12(istrstream(av[1], strlen(av[1])) ) >> l_stid;
	    DO( sm->bulkld_index(l_stid.lvid, l_stid.serial,
				s_stream,  stats) );
	} else {
	    stid_t stid;
	    GNUG_BUG_12(istrstream(av[1], strlen(av[1])) ) >> stid;
	    DO( sm->bulkld_index(stid, s_stream, stats) ); 
	}
    } else {
	// input file already sorted
	if (use_logical_id(ip)) {
	    lstid_t l_stid, l_src;
	    GNUG_BUG_12(istrstream(av[1], strlen(av[1])) ) >> l_stid;
	    GNUG_BUG_12(istrstream(av[2], strlen(av[2])) ) >> l_src;
	    DO( sm->bulkld_index(l_stid.lvid, l_stid.serial,
				 l_src.lvid, l_src.serial, stats) );
	} else {
	    stid_t stid, src;
	    GNUG_BUG_12(istrstream(av[1], strlen(av[1])) ) >> stid;
	    GNUG_BUG_12(istrstream(av[2], strlen(av[2])) ) >> src;
	    DO( sm->bulkld_index(stid, src, stats) ); 
	}
    } 

    {
	tclout.seekp(ios::beg);
	tclout << stats.btree << ends;
	Tcl_AppendResult(ip, tclout.str(), 0);
    }
    
    return TCL_OK;

}

static int
t_blkld_md_ndx(Tcl_Interp* ip, int ac, char* av[])
{
    if (check(ip, "stid src hf he universe", ac, 6)) 
	return TCL_ERROR;

    nbox_t univ(av[5]);
    
    sm_du_stats_t stats;

    sort_stream_i s_stream;

    char* type = "spatial";

    DO( prepare_for_blkld(s_stream, ip, av[2], type, av[5]) );

    if (use_logical_id(ip)) {
	lstid_t l_stid;
	GNUG_BUG_12(istrstream(av[1], strlen(av[1])) ) >> l_stid;
	DO( sm->bulkld_md_index(l_stid.lvid, l_stid.serial,
				  s_stream, stats,
				  atoi(av[3]), atoi(av[4]), &univ) );
    } else {
	stid_t stid;
	GNUG_BUG_12(istrstream(av[1], strlen(av[1])) ) >> stid;
	DO( sm->bulkld_md_index(stid, s_stream, stats,
				  atoi(av[3]), atoi(av[4]), &univ) );
    }

    {
	tclout.seekp(ios::beg);
	tclout << stats.rtree << ends;
	Tcl_AppendResult(ip, tclout.str(), 0);
    }
    
    return TCL_OK;
}

static int
t_print_index(Tcl_Interp* ip, int ac, char* av[])
{
    if (check(ip, "stid", ac, 2))
	return TCL_ERROR;

    if (use_logical_id(ip)) {
	lstid_t stid;
	GNUG_BUG_12(istrstream(av[1], strlen(av[1])) ) >> stid;
	DO( sm->print_index(stid.lvid, stid.serial) );
    } else {
	stid_t stid;
	GNUG_BUG_12(istrstream(av[1], strlen(av[1])) ) >> stid;
	DO( sm->print_index(stid) );
    }

    return TCL_OK;
}

static int
t_print_md_index(Tcl_Interp* ip, int ac, char* av[])
{
    if (check(ip, "stid", ac, 2))
	return TCL_ERROR;

    if (use_logical_id(ip)) {
	lstid_t stid;
	GNUG_BUG_12(istrstream(av[1], strlen(av[1])) ) >> stid;
	DO( sm->print_md_index(stid.lvid, stid.serial) );
    } else {
	stid_t stid;
	GNUG_BUG_12(istrstream(av[1], strlen(av[1])) ) >> stid;
	DO( sm->print_md_index(stid) );
    }

    return TCL_OK;
}

#ifdef USE_RDTREE
static int
t_print_set_index(Tcl_Interp* ip, int ac, char* av[])
{
    if (check(ip, "stid", ac, 2))
      return TCL_ERROR;
 
    if (use_logical_id(ip)) {
 	lstid_t stid;
 	GNUG_BUG_12(istrstream(av[1], strlen(av[1])) ) >> stid;
 	DO( sm->print_set_index(stid.lvid, stid.serial) );
    } else {
 	stid_t stid;
 	GNUG_BUG_12(istrstream(av[1], strlen(av[1])) ) >> stid;
 	DO( sm->print_set_index(stid) );
    }
 
    return TCL_OK;
}
#endif /* USE_RDTREE */

#include <crash.h>


#ifndef DEBUG
#define av /* av not used */
#endif
static int
t_crash(Tcl_Interp* ip, int ac, char* av[])
#undef av
{

    if (check(ip, "str cmd", ac, 3))
	return TCL_ERROR;

    DBG(<<"crash " << av[1] << " " << av[2] );

    _exit(1);
    // no need --
    return TCL_OK;
}

static int
t_restart(Tcl_Interp* ip, int ac, char* av[])
{
    if (check(ip, "clean:bool", ac, 1, 2))
	return TCL_ERROR;

    if (start_client_support)  {
	DO(stop_comm());  // stop ssh connection listen thread
    }
    bool clean = false;
    if(ac==2) {
	if(strcmp(av[1],"true")==0) {
	    clean = true;
	}
    }
    sm->set_shutdown_flag(clean);
    delete sm;


    sm = new ss_m();
    w_assert1(sm);
    
    if (start_client_support) {
	DO(start_comm());  // restart connection listen thread
    }

    return TCL_OK;
}

static int
t_create_file(Tcl_Interp* ip, int ac, char* av[])
{
    if (check(ip, "vid  [\"tmp/regular\"]",
	      ac, 2, 3)) 
	return TCL_ERROR;
    
    sm_store_property_t property = t_regular;
  
    if (ac > 2) {
	property = cvt2store_property(av[2]);
    }

    if (use_logical_id(ip)) {
	lfid_t lfid;
	GNUG_BUG_12(istrstream(av[1], strlen(av[1])) ) >> lfid.lvid;
	DO( sm->create_file(lfid.lvid, lfid.serial, property) );
	tclout.seekp(ios::beg);
	tclout << lfid << ends;
    } else {
	stid_t stid;
	DO( sm->create_file(atoi(av[1]), stid, property) );
	tclout.seekp(ios::beg);
	tclout << stid << ends;
    }
    
    Tcl_AppendResult(ip, tclout.str(), 0);
    return TCL_OK;
}

static int
t_destroy_file(Tcl_Interp* ip, int ac, char* av[])
{
    if (check(ip, "fid", ac, 2))
        return TCL_ERROR;
    
    if (use_logical_id(ip)) {
	lstid_t fid;
	GNUG_BUG_12(istrstream(av[1], strlen(av[1])) )>> fid;
	DO( sm->destroy_file(fid.lvid, fid.serial) );
    } else {
	stid_t fid;
	GNUG_BUG_12(istrstream(av[1], strlen(av[1])) ) >> fid;
	DO( sm->destroy_file(fid) );
    }

    return TCL_OK;
}

static int
t_create_scan(Tcl_Interp* ip, int ac, char* av[])
{
    FUNC(t_create_scan);
    if (check(ip, "stid cmp1 bound1 cmp2 bound2 [concurrency_t] [keydescr]", ac, 6, 8))
	return TCL_ERROR;
    
    vec_t t1, t2;
    vec_t* bound1 = 0;
    vec_t* bound2 = 0;
    scan_index_i::cmp_t c1, c2;
    

    bool convert=false;
    int b1, b2; // used if keytype is int or unsigned

    c1 = cvt2cmp_t(av[2]);
    c2 = cvt2cmp_t(av[4]);
    if (streq(av[3], "pos_inf"))  {
	bound1 = &vec_t::pos_inf;
    } else if (streq(av[3], "neg_inf")) {
	bound1 = &vec_t::neg_inf;
    } else {
	convert = true;
	(bound1 = &t1)->put(av[3], strlen(av[3]));
    }
    
    if (streq(av[5], "pos_inf")) {
	bound2 = &vec_t::pos_inf;
    } else if (streq(av[5], "neg_inf")) {
	bound2 = &vec_t::neg_inf;
    } else {
	(bound2 = &t2)->put(av[5], strlen(av[5]));
	convert = true;
    }

    if(convert && (ac > 7)) {
       const char *keydescr = av[7];

	// KLUDGE -- restriction for now
	// only because it's better than no check at all
	if(keydescr[0] != 'b' 
		&& keydescr[0] != 'i'
		&& keydescr[0] != 'u'
	) {
	    Tcl_AppendResult(ip, 
	       "create_index: 7th argument must start with b,i or u", 0);
	    return TCL_ERROR;
		
	}
	if( (keydescr[0] == 'i'
		||
	    keydescr[0] == 'u')
	) {
	    b1 = atoi(av[3]);
	    t1.reset();
	    t1.put(&b1, sizeof(int));
	    DBG(<<"bound1 = " << b1);

	    b2 = atoi(av[5]);
	    t2.reset();
	    t2.put(&b2, sizeof(int));
	    DBG(<<"bound2 = " << b2);
	}
    }

    ss_m::concurrency_t cc = ss_m::t_cc_kvl;
    if (ac == 7) {
	cc = cvt2concurrency_t(av[6]);
    }
    DBG(<<"c1 = " << c1);
    DBG(<<"c2 = " << c2);
    DBG(<<"cc = " << cc);

    scan_index_i* s;
    if (use_logical_id(ip)) {
	lstid_t stid;
	GNUG_BUG_12(istrstream(av[1], strlen(av[1])) ) >> stid;
	s = new scan_index_i(stid.lvid, stid.serial, 
			     c1, *bound1, c2, *bound2, cc);
    } else {
	stid_t stid;
	GNUG_BUG_12(istrstream(av[1], strlen(av[1])) ) >> stid;
	s = new scan_index_i(stid, c1, *bound1, c2, *bound2, cc);
    }
    if (!s) {
	cerr << "Out of memory: file " << __FILE__ 
	     << " line " << __LINE__ << endl;
	W_FATAL(fcOUTOFMEMORY);
    }
    
    tclout.seekp(ios::beg);
    tclout << s << ends;
    Tcl_AppendResult(ip, tclout.str(), 0);
    return TCL_OK;
}

static int
t_scan_next(Tcl_Interp* ip, int ac, char* av[])
{
    if (check(ip, "scanid [keydescr]", ac, 2, 3))
	return TCL_ERROR;
    
    scan_index_i* s = (scan_index_i*) strtol(av[1], 0, 0);
    smsize_t klen, elen;
    klen = elen = 0;
    bool eof;
    DO( s->next(eof) );
    if (eof) {
	Tcl_AppendElement(ip, "eof");
    } else {

	DO( s->curr(0, klen, 0, elen) );
	w_assert1(klen + elen + 2 <= ss_m::page_sz);

	vec_t key(outbuf, klen);
	vec_t el(outbuf + klen + 1, elen);
	DO( s->curr(&key, klen, &el, elen) );

	bool convert = false;
	if (ac > 3) {
	    const char *keydescr = (const char *)av[3];

	    // KLUDGE -- restriction for now
	    // only because it's better than no check at all
	    if(keydescr[0] != 'b' 
		    && keydescr[0] != 'i'
		    && keydescr[0] != 'u'
	    ) {
		Tcl_AppendResult(ip, 
		   "create_index: 3th argument must start with b,i or u", 0);
		return TCL_ERROR;
		    
	    }
	    if( keydescr[0] == 'i'
		    ||
		keydescr[0] == 'u'
	    ) {
		convert = true;

		// only i4 and u4 are supported here...
		if(klen != 4) {
		    cerr << "integer/unsigned key lengths other than 4 ("
			<< klen << ") are not supported." <<endl;
		    
		    w_assert1(klen == 4);
		}
	    }
	} 
	if(convert) {
	    int i;
	    memcpy(&i, outbuf, sizeof(int));
	    Tcl_AppendResult(ip, ::form("%d",i), " ", outbuf + klen + 1, 0);
	} else {
	    outbuf[klen] = outbuf[klen + 1 + elen] = '\0';
	    Tcl_AppendResult(ip, outbuf, " ", outbuf + klen + 1, 0);
	}
    }
    
    return TCL_OK;
}

static int
t_destroy_scan(Tcl_Interp* ip, int ac, char* av[])
{
    if (check(ip, "scanid", ac, 2))
	return TCL_ERROR;
    
    scan_index_i* s = (scan_index_i*) strtol(av[1], 0, 0);
    delete s;
    
    return TCL_OK;
}

static int
t_create_multi_recs(Tcl_Interp* ip, int ac, char* av[])
{
    if (check(ip, "fid hdr len_hint data count", ac, 6))
        return TCL_ERROR;

    vec_t hdr, data;
    hdr.set(parse_vec(av[2], strlen(av[2])));
    data.set(parse_vec(av[4], strlen(av[4])));

    tclout.seekp(ios::beg);

    register int i;
    int count = atoi(av[5]);

    if (use_logical_id(ip)) {
        lstid_t  stid;
        lrid_t   rid;

        GNUG_BUG_12(istrstream(av[1], strlen(av[1])) ) >> stid;
        for (i=0; i<count; i++)
                DO( sm->create_rec(stid.lvid, stid.serial, hdr,
                           atoi(av[3]), data, rid.serial) );
//      rid.lvid = stid.lvid;
//      tclout << rid << ends;

    } else {
        stid_t  stid;
        rid_t   rid;

        GNUG_BUG_12(istrstream(av[1], strlen(av[1])) ) >> stid;

        unix_stats timer;
	timer.start();
        for (i=0; i<count; i++)
                DO( sm->create_rec(stid, hdr, atoi(av[3]), data, rid) );
        timer.stop(1);
        cerr << "t_create_multi_recs: Write clock time = " 
	<< timer.clocktime() << endl;
    }
    tclout << "success" << ends;
    Tcl_AppendResult(ip, tclout.str(), 0);
    return TCL_OK;
}

static int
t_multi_file_append(Tcl_Interp* ip, int ac, char* av[])
{
    if (check(ip, "scan hdr len_hint data count", ac, 6))
        return TCL_ERROR;

    append_file_i *s = (append_file_i *)strtol(av[1], 0, 0);

    int len_hint = atoi(av[3]);

    vec_t hdr, data;
    hdr.set(parse_vec(av[2], strlen(av[2])));
    data.set(parse_vec(av[4], strlen(av[4])));

    tclout.seekp(ios::beg);

    register int i;
    int count = atoi(av[5]);

    if (use_logical_id(ip)) {
        lrid_t   lrid;

        for (i=0; i<count; i++)
                DO( s->create_rec(hdr, len_hint,  data, lrid) );
    } else {
        rid_t   rid;
        unix_stats timer;
	timer.start();
        for (i=0; i<count; i++)
                DO( s->create_rec(hdr, len_hint, data, rid) );
        timer.stop(1);
        cerr << "t_multi_file_append: Write clock time = " 
		<< timer.clocktime() << endl;
    }
    tclout << "success" << ends;
    Tcl_AppendResult(ip, tclout.str(), 0);
    return TCL_OK;
}

static int
t_create_rec(Tcl_Interp* ip, int ac, char* av[])
{
    if (check(ip, "fid hdr len_hint data", ac, 5)) 
	return TCL_ERROR;
   
    vec_t hdr, data;
    hdr.set(parse_vec(av[2], strlen(av[2])));
    data.set(parse_vec(av[4], strlen(av[4])));

    if (use_logical_id(ip)) {
	lstid_t  stid;
	lrid_t   rid;

	//cout << "using logical ID interface for CREATE_REC" << endl;

	GNUG_BUG_12(istrstream(av[1], strlen(av[1])) ) >> stid;
	DO( sm->create_rec(stid.lvid, stid.serial, hdr, 
			   atoi(av[3]), data, rid.serial) );
	rid.lvid = stid.lvid;

	tclout.seekp(ios::beg);
	tclout << rid << ends;

    } else {
	stid_t  stid;
	rid_t   rid;
	
	GNUG_BUG_12(istrstream(av[1], strlen(av[1])) ) >> stid;
	DO( sm->create_rec(stid, hdr, atoi(av[3]), data, rid) );
	tclout.seekp(ios::beg);
	tclout << rid << ends;
    }
    Tcl_AppendResult(ip, tclout.str(), 0);
    return TCL_OK;
}

static int
t_destroy_rec(Tcl_Interp* ip, int ac, char* av[])
{
    if (check(ip, "rid", ac, 2)) 
	return TCL_ERROR;
    

    if (use_logical_id(ip)) {
	lrid_t   rid;

	GNUG_BUG_12(istrstream(av[1], strlen(av[1])) ) >> rid;
	DO( sm->destroy_rec(rid.lvid, rid.serial) );

    } else {
	rid_t   rid;

	GNUG_BUG_12(istrstream(av[1], strlen(av[1])) ) >> rid;
	DO( sm->destroy_rec(rid) );
    }

    Tcl_AppendElement(ip, "update success");
    return TCL_OK;
}

static int
t_read_rec(Tcl_Interp* ip, int ac, char* av[])
{
    if (check(ip, "rid start length [num_pins]", ac, 4, 5)) 
	return TCL_ERROR;
    
    smsize_t   start  = atoi(av[2]);
    smsize_t   length = atoi(av[3]);
    int	    num_pins;
    if (ac == 5) {
	num_pins = atoi(av[4]);		
    } else {
	num_pins = 1;
    }
    pin_i   handle;

    if (use_logical_id(ip)) {
	lrid_t   rid;
	GNUG_BUG_12(istrstream(av[1], strlen(av[1])) ) >> rid;

	for (int i=1; i<num_pins; i++) {
	    W_IGNORE(handle.pin(rid.lvid, rid.serial, start));
	    handle.unpin();
	}
	DO(handle.pin(rid.lvid, rid.serial, start));
	tclout.seekp(ios::beg);
	tclout << "rid=" << rid << ends;

    } else {
	rid_t   rid;
	GNUG_BUG_12(istrstream(av[1], strlen(av[1])) ) >> rid;

	for (int i=1; i<num_pins; i++) {
	    W_IGNORE(handle.pin(rid, start));
	    handle.unpin();
	}
	DO(handle.pin(rid, start)); 
	tclout.seekp(ios::beg);
	tclout << "rid=" << rid << " " << ends;
    }
    /* result: 
       rid=<rid> size(h.b)=<sizes> hdr=<hdr>
       \nBytes:x-y: <body>
    */

    if (length == 0) {
	length = handle.body_size();
    }
    tclout << ends;
    Tcl_AppendResult(ip, tclout.str(), 0);

    tclout.seekp(ios::beg);
    tclout << " size(h.b)=" << handle.hdr_size() << "." 
			  << handle.body_size() << " " << ends;
    Tcl_AppendResult(ip, tclout.str(), 0);

    dump_pin_hdr(tclout, handle);
    tclout << " " << ends;
    Tcl_AppendResult(ip, tclout.str(), 0);


    tclout.seekp(ios::beg);
    dump_pin_body(tclout, handle, start, length, ip);

    return TCL_OK;
}

static int
t_print_rec(Tcl_Interp* ip, int ac, char* av[])
{
    if (check(ip, "rid start length", ac, 4)) 
	return TCL_ERROR;
    
    smsize_t   start  = atoi(av[2]);
    smsize_t   length = atoi(av[3]);
    pin_i   handle;

    if (use_logical_id(ip)) {
	lrid_t   rid;
	GNUG_BUG_12(istrstream(av[1], strlen(av[1])) ) >> rid;

	DO(handle.pin(rid.lvid, rid.serial, start));
	cout << "rid=" << rid << " "; 

    } else {
	rid_t   rid;
	GNUG_BUG_12(istrstream(av[1], strlen(av[1])) ) >> rid;

	DO(handle.pin(rid, start)); 
	cout << "rid=" << rid << " "; 
    }

    if (length == 0) {
	length = handle.body_size();
    }

    dump_pin_hdr(cout, handle);
    tclout.seekp(ios::beg);
    dump_pin_body(tclout, handle, start, length, 0);
    tclout << ends;
    cout << tclout.str();

    return TCL_OK;
}

static int
t_read_rec_body(Tcl_Interp* ip, int ac, char* av[])
{
    if (check(ip, "rid [start [length]]", ac, 2, 3, 4)) 
	return TCL_ERROR;

    smsize_t start = (ac >= 3) ? atoi(av[2]) : 0;
    smsize_t length = (ac >= 4) ? atoi(av[3]) : 0;
    pin_i handle;

    if (use_logical_id(ip)) {
        lrid_t   rid;
        GNUG_BUG_12(istrstream(av[1], strlen(av[1])) ) >> rid;

        DO(handle.pin(rid.lvid, rid.serial, start));
    } else {
        rid_t   rid;
        GNUG_BUG_12(istrstream(av[1], strlen(av[1])) ) >> rid;

        DO(handle.pin(rid, start));
    }

    if (length == 0) {
        length = handle.body_size();
    }

    smsize_t offset = (start - handle.start_byte());
    smsize_t start_pos = start;
    smsize_t amount;

    Tcl_ResetResult(ip);

    bool eor = FALSE;  // check end of record
    while (length > 0)
    {
        amount = MIN(length, handle.length() - offset);

        sprintf(outbuf, "%.*s", (int)amount, handle.body() + offset);
	Tcl_AppendResult(ip, outbuf, 0);

        length -= amount;
        start_pos += amount;
        offset = 0;

        DO(handle.next_bytes(eor));
	if (eor) {
	    break;		// from while loop
        }
    }  // end while
    return TCL_OK;
}

static int
t_update_rec(Tcl_Interp* ip, int ac, char* av[])
{
    if (check(ip, "rid start data", ac, 4)) 
	return TCL_ERROR;
    
    smsize_t   start;
    vec_t   data;

    data.set(parse_vec(av[3], strlen(av[3])));
    start = atoi(av[2]);    

    if (use_logical_id(ip)) {
	lrid_t   rid;

	GNUG_BUG_12(istrstream(av[1], strlen(av[1])) ) >> rid;
	DO( sm->update_rec(rid.lvid, rid.serial, start, data) );

    } else {
	rid_t   rid;

	GNUG_BUG_12(istrstream(av[1], strlen(av[1])) ) >> rid;
	DO( sm->update_rec(rid, start, data) );
    }

    Tcl_AppendElement(ip, "update success");
    return TCL_OK;
}

static int
t_update_rec_hdr(Tcl_Interp* ip, int ac, char* av[])
{
    if (check(ip, "rid start hdr", ac, 4)) 
	return TCL_ERROR;
    
    smsize_t   start;
    vec_t   hdr;

    hdr.set(parse_vec(av[3], strlen(av[3])));
    start = atoi(av[2]);    

    if (use_logical_id(ip)) {
	lrid_t   rid;

	GNUG_BUG_12(istrstream(av[1], strlen(av[1])) ) >> rid;
	DO( sm->update_rec_hdr(rid.lvid, rid.serial, start, hdr) );

    } else {
	rid_t   rid;

	GNUG_BUG_12(istrstream(av[1], strlen(av[1])) ) >> rid;
	DO( sm->update_rec_hdr(rid, start, hdr) );
    }

    Tcl_AppendElement(ip, "update hdr success");
    return TCL_OK;
}

static int
t_append_rec(Tcl_Interp* ip, int ac, char* av[])
{
    if (check(ip, "rid data", ac, 3)) 
	return TCL_ERROR;
    
    vec_t   data;
    data.set(parse_vec(av[2], strlen(av[2])));

    if (use_logical_id(ip)) {
	lrid_t   rid;
	GNUG_BUG_12(istrstream(av[1], strlen(av[1])) ) >> rid;
	DO( sm->append_rec(rid.lvid, rid.serial, data) );

    } else {
	rid_t   rid;
	GNUG_BUG_12(istrstream(av[1], strlen(av[1])) ) >> rid;
	DO( sm->append_rec(rid, data, FALSE) );
    }

    Tcl_AppendElement(ip, "append success");
    return TCL_OK;
}

static int
t_truncate_rec(Tcl_Interp* ip, int ac, char* av[])
{
    if (check(ip, "rid amount", ac, 3)) 
	return TCL_ERROR;
 
    smsize_t amount = atoi(av[2]);
     
    if (use_logical_id(ip)) {
	lrid_t   rid;
	GNUG_BUG_12(istrstream(av[1], strlen(av[1])) ) >> rid;
	DO( sm->truncate_rec(rid.lvid, rid.serial, amount) );
    } else {
	rid_t   rid;
	GNUG_BUG_12(istrstream(av[1], strlen(av[1])) ) >> rid;
	DO( sm->truncate_rec(rid, amount) );
    }

    Tcl_AppendElement(ip, "append success");
    return TCL_OK;
}

void
dump_pin_hdr( 
	ostream &out,
	pin_i &handle
) 
{
    int j;

    // DETECT ZVECS
    // Assume the header is reasonably small

    w_assert1(handle.hdr_size() < sizeof(outbuf));
    memcpy(outbuf, handle.hdr(), handle.hdr_size());
    outbuf[handle.hdr_size()] = 0;
    
    for (j=handle.hdr_size()-1; j>=0; j--) {
	    if(outbuf[j]!= '\0') break;
    }
    if(j== -1) {
	    out << " hdr=zvec" << handle.hdr_size() ;
    } else {
	    out << " hdr=" << outbuf;
    }
}

void
dump_pin_body( 
    ostrstream &out, 
    pin_i &handle, 
    smsize_t start,
    smsize_t length,
    Tcl_Interp * ip
)
{
    const	int buf_len = ss_m::page_sz + 1;

    char *buf = (char *) malloc(buf_len);
    if (!buf)
	    W_FATAL(fcOUTOFMEMORY);
	    

    // BODY is not going to be small
    // write to ostream, and if
    // ip is defined, flush to ip every
    // so - often
    if(start == (smsize_t)-1) {
	start = 0;
    }
    if(length == 0) {
	length = handle.body_size();
    }

    smsize_t i = start;
    smsize_t offset = start-handle.start_byte();
    bool eor = false;  // check end of record
    while (i < length && !eor) {

	smsize_t handle_len = MIN(handle.length(), length);
	//s << "Bytes:" << i << "-" << i+handle.length()-offset 
	//  << ": " << handle.body()[0] << "..." << ends;
	int width = handle_len-offset;

	// out.seekp(ios::beg);
	out << "\nBytes:" << i << "-" << i+handle_len-offset << ": " ;
	if(ip) {
	    out << ends;
	    Tcl_AppendResult(ip, out.str(), 0);
	    out.seekp(ios::beg);
	}

	// DETECT ZVECS
	{
	    int j;

	    /* Zero the buffer beforehand.  The sprintf will output *at
	       most* 'width' characters, and if the printed string is
	       shorter, the loop examines unset memory.

	       XXX this seems overly complex, how about checking width
	       for non-zero bytes instead of the run-around with
	       sprintf?
	     */

	    w_assert1(width+1 < buf_len);
	    memset(buf, '\0', width+1);
	    sprintf(buf, "%.*s", width, handle.body()+offset);
	    for (j=width-1; j>=0; j--) {
		if(buf[j]!= '\0')  {
		    DBG(<<"byte # " << j << 
		    " is non-zero: " << (unsigned char) buf[j] );
		    break;
		}
	    }
	    if(j== -1) {
		out << "zvec" << width;
	    } else {
		sprintf(buf, "%.*s", width, handle.body()+offset);
		out << buf;
		if(ip) {
		    out << ends;
		    Tcl_AppendResult(ip, buf, 0);
		    out.seekp(ios::beg);
		}
	    }
	}
	i += handle_len-offset;
	offset = 0;
	if (i < length) {
	    if(handle.next_bytes(eor)) {
		cerr << "error in next_bytes" << endl;
		free(buf);
		return;
	    }
	}
    }
    free(buf);
}

w_rc_t
dump_scan( scan_file_i &scan, ostream &out) 
{
    w_rc_t rc;
    bool eof=false;
    pin_i*  pin = 0;
    const	int buf_unit = 1024;	/* must be a power of 2 */
    char	*buf = 0;
    int		buf_len = 0;

    while ( !(rc=scan.next(pin, 0, eof)) && !eof) {
	out << pin->rid();
	out << " body: " ;
	vec_t w;
	bool eof = false;

	for(unsigned int j = 0; j < pin->body_size();) {
	    if(pin->length() > 0) {
		w.reset();

		/* Lazy buffer allocation, in buf_unit chunks.
		   There must be space for the nul string terminator */
		if ((int)pin->length() >= buf_len) {
			/* allocate the buffer in buf_unit increments */
			w_assert3(buf_len <= 8192);
			if (buf)
				free(buf);
			/* need a generic alignment macro (alignto) */
			buf_len = (pin->length() + 1 + buf_unit-1) &
				~(buf_unit - 1);
			buf = (char *)malloc(buf_len);
			if (!buf)
				W_FATAL(fcOUTOFMEMORY);
		}

		w.put(pin->body(), pin->length());
		w.copy_to(buf, buf_len);
		buf[pin->length()] = '\0';
		out << buf 
			    << "(length=" << pin->length() << ")" 
			    << "(size=" << pin->body_size() << ")" 
			    << endl;
		rc = pin->next_bytes(eof);
		if(rc) break;
		j += pin->length();
	    }
	    if(eof || rc) break;
	}
	out << endl;
    }
    if (buf)
	    free(buf);
    return rc;
}

static int
t_scan_recs(Tcl_Interp* ip, int ac, char* av[])
{
    if (check(ip, "fid [start_rid]", ac, 2, 3)) 
	return TCL_ERROR;
    
    rc_t    rc;
    bool  use_logical = use_logical_id(ip);
    scan_file_i *scan;

    if (use_logical) {
	lrid_t   rid;
	lstid_t  fid;

	GNUG_BUG_12(istrstream(av[1], strlen(av[1])) ) >> fid;
	if (ac == 3) {
	    GNUG_BUG_12(istrstream(av[2], strlen(av[2])) ) >> rid;
	    scan = new scan_file_i(fid.lvid, fid.serial, rid.serial);
	    w_assert1(scan->error_code() ||
		    fid.lvid == scan->lvid() && fid.serial == scan->lfid());

	} else {
	    scan = new scan_file_i(fid.lvid, fid.serial);
	    w_assert1(scan->error_code() || 
		    fid.lvid == scan->lvid() && fid.serial == scan->lfid());
	}
    } else {
	rid_t   rid;
	stid_t  fid;

	GNUG_BUG_12(istrstream(av[1], strlen(av[1])) ) >> fid;
	if (ac == 3) {
	    GNUG_BUG_12(istrstream(av[2], strlen(av[2])) ) >> rid;
	    scan = new scan_file_i(fid, rid);
	} else {
	    scan = new scan_file_i(fid);
	}
    }
    rc = dump_scan(*scan, cout);
    delete scan;

    DO( rc );

    Tcl_AppendElement(ip, "scan success");
    return TCL_OK;
}

static int
t_scan_rids(Tcl_Interp* ip, int ac, char* av[])
{
    if (check(ip, "fid [start_rid]", ac, 2, 3))
        return TCL_ERROR;
  
    pin_i*      pin;
    bool      eof;
    rc_t	rc;
    bool      use_logical = use_logical_id(ip);
    scan_file_i *scan = NULL;

    Tcl_ResetResult(ip);
    if (use_logical) {
        lrid_t   rid;
        lstid_t  fid;
        lrid_t   curr;

        GNUG_BUG_12(istrstream(av[1], strlen(av[1])) ) >> fid;

        if (ac == 3) {
            GNUG_BUG_12(istrstream(av[2], strlen(av[2])) ) >> rid;
            scan = new scan_file_i(fid.lvid, fid.serial, rid.serial, ss_m::t_cc_page);
        } else {
            scan = new scan_file_i(fid.lvid, fid.serial, ss_m::t_cc_page);
	}

        w_assert1(scan->error_code() || 
		fid.lvid == scan->lvid() && fid.serial == scan->lfid());

        while ( !(rc = scan->next(pin, 0, eof)) && !eof ) {
            curr.lvid = pin->lvid();
            curr.serial = pin->serial_no();
            tclout.seekp(ios::beg);
            tclout << curr << ends;
	    Tcl_AppendResult(ip, tclout.str(), " ", 0);
        }
    } else {
        rid_t   rid;
        stid_t  fid;

        GNUG_BUG_12(istrstream(av[1], strlen(av[1])) ) >> fid;

        if (ac == 3) {
            GNUG_BUG_12(istrstream(av[2], strlen(av[2])) ) >> rid;
            scan = new scan_file_i(fid, rid);
        } else {
            scan = new scan_file_i(fid);
	}

        while ( !(rc = scan->next(pin, 0, eof)) && !eof ) {
            tclout.seekp(ios::beg);
            tclout << pin->rid() << ends;
	    Tcl_AppendResult(ip, tclout.str(), " ", 0);
        }
    }

    if (scan) {
	delete scan;
	scan = NULL;
    }

    DO( rc);

    return TCL_OK;
}

static int
t_pin_create(Tcl_Interp* ip, int ac, char*[])
{
    if (check(ip, "", ac, 1))
	return TCL_ERROR;

    pin_i* p = new pin_i;;
    if (!p) {
	cerr << "Out of memory: file " << __FILE__ 
	     << " line " << __LINE__ << endl;
	W_FATAL(fcOUTOFMEMORY);
    }
    
    tclout.seekp(ios::beg);
    tclout << p << ends;
    Tcl_AppendResult(ip, tclout.str(), 0);
    return TCL_OK;
}

static int
t_pin_destroy(Tcl_Interp* ip, int ac, char* av[])
{
    if (check(ip, "pin", ac, 2))
	return TCL_ERROR;
    
    pin_i* p = (pin_i*) strtol(av[1], 0, 0);
    if (p) delete p;

    return TCL_OK;
}

static int
t_pin_pin(Tcl_Interp* ip, int ac, char* av[])
{
    if (check(ip, "pin rec_id start [SH/EX]", ac, 4,5))
	return TCL_ERROR;
    
    pin_i* p = (pin_i*) strtol(av[1], 0, 0);
    smsize_t start = atoi(av[3]);
    lock_mode_t lmode = SH;
 
    if (ac == 5) {
	if (strcmp(av[4], "EX") == 0) {
	    lmode = EX;
	}
    }

    if (use_logical_id(ip)) {
	lrid_t   rid;
	GNUG_BUG_12(istrstream(av[2], strlen(av[2])) ) >> rid;

	DO(p->pin(rid.lvid, rid.serial, start, lmode));

    } else {
	rid_t   rid;
	GNUG_BUG_12(istrstream(av[2], strlen(av[2])) ) >> rid;

	DO(p->pin(rid, start, lmode)); 
    }
    return TCL_OK;
}

static int
t_pin_unpin(Tcl_Interp* ip, int ac, char* av[])
{
    if (check(ip, "pin", ac, 2))
	return TCL_ERROR;
    
    pin_i* p = (pin_i*) strtol(av[1], 0, 0);
    p->unpin();

    return TCL_OK;
}

static int
t_pin_repin(Tcl_Interp* ip, int ac, char* av[])
{
    if (check(ip, "pin [SH/EX]", ac, 2,3))
	return TCL_ERROR;
    
    lock_mode_t lmode = SH;
    if (ac == 3) {
	if (strcmp(av[2], "EX") == 0) {
	    lmode = EX;
	}
    }

    pin_i* p = (pin_i*) strtol(av[1], 0, 0);
    DO(p->repin(lmode));

    return TCL_OK;
}

static int
t_pin_body(Tcl_Interp* ip, int ac, char* av[])
{
    if (check(ip, "pin", ac, 2))
	return TCL_ERROR;
    
    pin_i* p = (pin_i*) strtol(av[1], 0, 0);

    memcpy(outbuf, p->body(), p->length());
    outbuf[p->length()] = '\0';

    Tcl_AppendResult(ip, outbuf, 0);
    return TCL_OK;
}

static int
t_pin_body_cond(Tcl_Interp* ip, int ac, char* av[])
{
    if (check(ip, "pin", ac, 2))
	return TCL_ERROR;
    
    pin_i* p = (pin_i*) strtol(av[1], 0, 0);
    const char* body = p->body_cond();
    if (body) {
	memcpy(outbuf, body, p->length());
	outbuf[p->length()] = '\0';
    } else {
	outbuf[0] = '\0';
    }
    Tcl_AppendResult(ip, outbuf, 0);

    return TCL_OK;
}

static int
t_pin_next_bytes(Tcl_Interp* ip, int ac, char* av[])
{
    if (check(ip, "pin", ac, 2))
	return TCL_ERROR;
    
    pin_i* p = (pin_i*) strtol(av[1], 0, 0);
    bool eof;
    DO(p->next_bytes(eof));

    if (eof) {
	Tcl_AppendResult(ip, "1", 0);
    } else {
	Tcl_AppendResult(ip, "0", 0);
    }

    return TCL_OK;
}

static int
t_pin_hdr(Tcl_Interp* ip, int ac, char* av[])
{
    if (check(ip, "pin", ac, 2))
	return TCL_ERROR;
    
    pin_i* p = (pin_i*) strtol(av[1], 0, 0);
    memcpy(outbuf, p->hdr(), p->hdr_size());
    outbuf[p->hdr_size()] = '\0';

    Tcl_AppendResult(ip, outbuf, 0);

    return TCL_OK;
}

static int
t_pin_pinned(Tcl_Interp* ip, int ac, char* av[])
{
    if (check(ip, "pin", ac, 2))
	return TCL_ERROR;
    
    pin_i* p = (pin_i*) strtol(av[1], 0, 0);

    if (p->pinned()) {
	Tcl_AppendResult(ip, "1", 0);
    } else {
	Tcl_AppendResult(ip, "0", 0);
    }

    return TCL_OK;
}

static int
t_pin_is_small(Tcl_Interp* ip, int ac, char* av[])
{
    if (check(ip, "pin", ac, 2))
	return TCL_ERROR;
    
    pin_i* p = (pin_i*) strtol(av[1], 0, 0);

    if (p->is_small()) {
	Tcl_AppendResult(ip, "1", 0);
    } else {
	Tcl_AppendResult(ip, "0", 0);
    }

    return TCL_OK;
}

static int
t_pin_rid(Tcl_Interp* ip, int ac, char* av[])
{
    if (check(ip, "pin", ac, 2))
	return TCL_ERROR;
    
    pin_i* p = (pin_i*) strtol(av[1], 0, 0);

    tclout.seekp(ios::beg);
    if (use_logical_id(ip)) {
	lrid_t rid(p->lvid(), p->serial_no());
	tclout << rid << ends;
    } else {
	tclout << p->rid() << ends;
    }
    Tcl_AppendResult(ip, tclout.str(), 0);

    return TCL_OK;
}

static int
t_pin_append_rec(Tcl_Interp* ip, int ac, char* av[])
{
    if (check(ip, "pin data", ac, 3))
	return TCL_ERROR;
    
    vec_t   data;
    data.set(parse_vec(av[2], strlen(av[2])));

    pin_i* p = (pin_i*) strtol(av[1], 0, 0);
    DO( p->append_rec(data) );

    return TCL_OK;
}

static int
t_pin_update_rec(Tcl_Interp* ip, int ac, char* av[])
{
    if (check(ip, "pin start data", ac, 4))
	return TCL_ERROR;
   
    smsize_t start = atoi(av[2]); 
    vec_t   data;
    data.set(parse_vec(av[3], strlen(av[3])));

    pin_i* p = (pin_i*) strtol(av[1], 0, 0);
    DO( p->update_rec(start, data) );

    return TCL_OK;
}

static int
t_pin_update_rec_hdr(Tcl_Interp* ip, int ac, char* av[])
{
    if (check(ip, "pin start data", ac, 4))
	return TCL_ERROR;
   
    smsize_t start = atoi(av[2]); 
    vec_t   data;
    data.set(parse_vec(av[3], strlen(av[3])));

    pin_i* p = (pin_i*) strtol(av[1], 0, 0);
    DO( p->update_rec_hdr(start, data) );

    return TCL_OK;
}

static int
t_pin_truncate_rec(Tcl_Interp* ip, int ac, char* av[])
{
    if (check(ip, "pin amount", ac, 3))
	return TCL_ERROR;
   
    smsize_t amount = atoi(av[2]); 

    pin_i* p = (pin_i*) strtol(av[1], 0, 0);
    DO( p->truncate_rec(amount) );

    return TCL_OK;
}

static int
t_scan_file_create(Tcl_Interp* ip, int ac, char* av[])
{
    if (check(ip, "fileid concurrency_t [start_rid]", ac, 3,4))
	return TCL_ERROR;
    
    ss_m::concurrency_t cc;
    cc = cvt2concurrency_t(av[2]);

    scan_file_i* s = NULL;

    if (use_logical_id(ip)) {
	lrid_t   lfid;
	GNUG_BUG_12(istrstream(av[1], strlen(av[1])) ) >> lfid;

	if (ac == 4) {
	    lrid_t   lrid;
	    GNUG_BUG_12(istrstream(av[2], strlen(av[3])) ) >> lrid;
	    s = new scan_file_i(lfid.lvid, lfid.serial, lrid.serial, cc);
	} else {
	    if(cc ==  ss_m::t_cc_append) {
		s = new append_file_i(lfid.lvid, lfid.serial);
	    } else {
		s = new scan_file_i(lfid.lvid, lfid.serial, cc);
	    }
	}
	if (!s) {
	    cerr << "Out of memory: file " << __FILE__ 
		 << " line " << __LINE__ << endl;
	    W_FATAL(fcOUTOFMEMORY);
	}
    } else {
	stid_t   fid;
	GNUG_BUG_12(istrstream(av[1], strlen(av[1])) ) >> fid;

	if (ac == 4) {
	    rid_t   rid;
	    GNUG_BUG_12(istrstream(av[2], strlen(av[3])) ) >> rid;
	    s = new scan_file_i(fid, rid, cc);
	} else {
	    if(cc ==  ss_m::t_cc_append) {
		s = new append_file_i(fid);
	    } else {
		s = new scan_file_i(fid, cc);
	    }
	}
	if (!s) {
	    cerr << "Out of memory: file " << __FILE__ 
		 << " line " << __LINE__ << endl;
	    W_FATAL(fcOUTOFMEMORY);
	}
    }
    
    tclout.seekp(ios::beg);
    tclout << (unsigned)s << ends;
    Tcl_AppendResult(ip, tclout.str(), 0);
    return TCL_OK;
}

static int
t_scan_file_destroy(Tcl_Interp* ip, int ac, char* av[])
{
    if (check(ip, "scan", ac, 2))
	return TCL_ERROR;
    
    scan_file_i* s = (scan_file_i*) strtol(av[1], 0, 0);
    if (s) delete s;

    return TCL_OK;
}


static int
t_scan_file_cursor(Tcl_Interp* ip, int ac, char* av[])
{
    if (check(ip, "scan ", ac, 2))
	return TCL_ERROR;
   
    scan_file_i* s = (scan_file_i*) strtol(av[1], 0, 0);

    pin_i* p;
    bool eof=false;
    s->cursor(p,  eof); // void func
    if (eof) p = NULL;

    tclout.seekp(ios::beg);
    if (p == NULL) {
	tclout << "NULL" << ends;
    } else {
	tclout << p << ends;
    }
    Tcl_AppendResult(ip, tclout.str(), 0);

    return TCL_OK;
}

static int
t_scan_file_next(Tcl_Interp* ip, int ac, char* av[])
{
    if (check(ip, "scan start", ac, 3))
	return TCL_ERROR;
   
    smsize_t start = atoi(av[2]);
    scan_file_i* s = (scan_file_i*) strtol(av[1], 0, 0);

    pin_i* p;
    bool eof=false;
    DO(s->next(p, start, eof));
    if (eof) p = NULL;

    tclout.seekp(ios::beg);
    if (p == NULL) {
	tclout << "NULL" << ends;
    } else {
	tclout << p << ends;
    }
    Tcl_AppendResult(ip, tclout.str(), 0);

    return TCL_OK;
}


static int
t_scan_file_next_page(Tcl_Interp* ip, int ac, char* av[])
{
    if (check(ip, "scan start", ac, 3))
	return TCL_ERROR;
   
    smsize_t start = atoi(av[2]);
    scan_file_i* s = (scan_file_i*) strtol(av[1], 0, 0);

    pin_i* p;
    bool eof;
    DO(s->next_page(p, start, eof));
    if (eof) p = NULL;

    tclout.seekp(ios::beg);
    if (p == NULL) {
	tclout << "NULL" << ends;
    } else {
	tclout << p << ends;
    }
    Tcl_AppendResult(ip, tclout.str(), 0);

    return TCL_OK;
}


static int
t_scan_file_finish(Tcl_Interp* ip, int ac, char* av[])
{
    if (check(ip, "scan", ac, 2))
	return TCL_ERROR;
   
    scan_file_i* s = (scan_file_i*) strtol(av[1], 0, 0);
    s->finish();
    return TCL_OK;
}

static int
t_scan_file_append(Tcl_Interp* ip, int ac, char* av[])
{
    if (check(ip, "scan hdr len_hint data", ac, 5)) 
	return TCL_ERROR;

    append_file_i* s = (append_file_i*) strtol(av[1], 0, 0);
   
    vec_t hdr, data;
    hdr.set(parse_vec(av[2], strlen(av[2])));
    data.set(parse_vec(av[4], strlen(av[4])));

    lrid_t   lrid;
    rid_t    rid;

    tclout.seekp(ios::beg);

    if(s->is_logical()) {
	DO( s->create_rec(hdr, atoi(av[3]), data, lrid) );
	tclout << lrid << ends;
    } else {
	DO( s->create_rec(hdr, atoi(av[3]), data, rid) );
	tclout << rid << ends;
    }
    Tcl_AppendResult(ip, tclout.str(), 0);
    return TCL_OK;
}

static int
t_create_assoc(Tcl_Interp* ip, int ac, char* av[])
{
    if (check(ip, "stid key el [keydescr]", ac, 4, 5)) 
		 // 1    2  3   4
	return TCL_ERROR;
    
    vec_t key, el;
    int i; // used iff keytype is int/unsigned
    
    key.put(av[2], strlen(av[2]));
    el.put(av[3], strlen(av[3]));

    if (ac > 4) {
	const char *keydescr = (const char *)av[4];

	// KLUDGE -- restriction for now
	// only because it's better than no check at all
	if(keydescr[0] != 'b' 
		&& keydescr[0] != 'i'
		&& keydescr[0] != 'u'
	) {
	    Tcl_AppendResult(ip, 
	       "create_index: 4th argument must start with b,i or u", 0);
	    return TCL_ERROR;
		
	}
	if( keydescr[0] == 'i'
		||
	    keydescr[0] == 'u'
	) {
	    i = atoi(av[2]);
	    key.reset();
	    key.put(&i, sizeof(int));
	}
    }

    if (use_logical_id(ip)) {
	lstid_t stid;
	GNUG_BUG_12(istrstream(av[1], strlen(av[1])) ) >> stid;
	DO( sm->create_assoc(stid.lvid, stid.serial, key, el) );
    } else {
	stid_t stid;
	GNUG_BUG_12(istrstream(av[1], strlen(av[1])) ) >> stid;
	DO( sm->create_assoc(stid, key, el) );
    }	
    return TCL_OK;
}

static int
t_destroy_assoc(Tcl_Interp* ip, int ac, char* av[])
{
    if (check(ip, "stid key el", ac, 4)) 
	return TCL_ERROR;
    
    vec_t key, el;
    
    key.put(av[2], strlen(av[2]));
    el.put(av[3], strlen(av[3]));

    if (use_logical_id(ip)) {
	lstid_t stid;
	GNUG_BUG_12(istrstream(av[1], strlen(av[1])) ) >> stid;
	DO( sm->destroy_assoc(stid.lvid, stid.serial, key, el) );
    } else {
	stid_t stid;
	GNUG_BUG_12(istrstream(av[1], strlen(av[1])) ) >> stid;
	DO( sm->destroy_assoc(stid, key, el) );
    }
    
    return TCL_OK;
}
static int
t_destroy_all_assoc(Tcl_Interp* ip, int ac, char* av[])
{
    if (check(ip, "stid key", ac, 3)) 
	return TCL_ERROR;
    
    vec_t key;
    
    key.put(av[2], strlen(av[2]));

    int num_removed;
    if (use_logical_id(ip)) {
	lstid_t stid;
	GNUG_BUG_12(istrstream(av[1], strlen(av[1])) ) >> stid;
	DO( sm->destroy_all_assoc(stid.lvid, stid.serial, key, num_removed) );
    } else {
	stid_t stid;
	GNUG_BUG_12(istrstream(av[1], strlen(av[1])) ) >> stid;
	DO( sm->destroy_all_assoc(stid, key, num_removed) );
    }
    
    tclout.seekp(ios::beg);
    tclout << num_removed << ends;
    Tcl_AppendResult(ip, tclout.str(), 0);
    return TCL_OK;
}

static int
t_find_assoc(Tcl_Interp* ip, int ac, char* av[])
{
    if (check(ip, "stid key", ac, 3))
	return TCL_ERROR;
    
    vec_t key;
    bool found;
    key.put(av[2], strlen(av[2]));
    
#define ELSIZE 4044
    char *el = new char[ELSIZE];
    smsize_t elen = ELSIZE-1;

    w_rc_t ___rc;

    if (use_logical_id(ip)) {
	lstid_t stid;
	GNUG_BUG_12(istrstream(av[1], strlen(av[1])) ) >> stid;
	___rc = sm->find_assoc(stid.lvid, stid.serial, key, el, elen, found);
    } else {
	stid_t stid;
	GNUG_BUG_12(istrstream(av[1], strlen(av[1])) ) >> stid;
	___rc = sm->find_assoc(stid, key, el, elen, found);
    }
    if (___rc)  {
	tclout.seekp(ios::beg);
	tclout << ___rc << ends;
	Tcl_AppendResult(ip, tclout.str(), 0);
	delete[] el;
	return TCL_ERROR;
    }

    el[elen] = '\0';
    if (found) {
	Tcl_AppendElement(ip, el);
	delete[] el;
	return TCL_OK;
    } 

    Tcl_AppendElement(ip, "not found");
    delete[] el;
    return TCL_ERROR;
}

static int
t_create_md_index(Tcl_Interp* ip, int ac, char* av[])
{
    if (check(ip, "vid dim ndxtype", ac, 4))
	return TCL_ERROR;

    int2 dim = atoi(av[2]);

    if (use_logical_id(ip)) {
	lstid_t stid;
	GNUG_BUG_12(istrstream(av[1], strlen(av[1])) ) >> stid.lvid;
	DO( sm->create_md_index(stid.lvid, cvt2ndx_t(av[3]),
				t_regular, stid.serial, dim) );
	tclout.seekp(ios::beg);
	tclout << stid << ends;
    } else {
	stid_t stid;
	DO( sm->create_md_index(atoi(av[1]), cvt2ndx_t(av[3]),
				t_regular, stid, dim) );
	tclout.seekp(ios::beg);
	tclout << stid << ends;
    }
    Tcl_AppendResult(ip, tclout.str(), 0);
    return TCL_OK;
}

#ifdef USE_RDTREE
static int
t_create_set_index(Tcl_Interp* ip, int ac, char* av[])
{
    if (check(ip, "vid elemsz ndxtype", ac, 4))
      return TCL_ERROR;
 
    int2 dim = atoi(av[2]);
 
    tclout.seekp(ios::beg);
    if (use_logical_id(ip)) {
 	lstid_t stid;
 	GNUG_BUG_12(istrstream(av[1], strlen(av[1])) ) >> stid.lvid;
 	DO( sm->create_set_index(stid.lvid, cvt2ndx_t(av[3]), 
				 t_regular, stid.serial, dim) );
 	tclout << stid << ends;
    } else {
 	stid_t stid;
 	DO( sm->create_set_index(atoi(av[1]), cvt2ndx_t(av[3]), 
				 t_regular, stid, dim) );
 	tclout << stid << ends;
    }
    w_assert1(s);
    Tcl_AppendElement(ip, tmp);
    return TCL_OK;
}
#endif /* USE_RDTREE */

static int
t_create_md_assoc(Tcl_Interp* ip, int ac, char* av[])
{
    if (check(ip, "stid box el", ac, 4))
	return TCL_ERROR;

    vec_t el;
    nbox_t key(av[2]);

    el.put(av[3], strlen(av[3]) + 1);
    if (use_logical_id(ip)) {
	lstid_t stid;
	GNUG_BUG_12(istrstream(av[1], strlen(av[1])) ) >> stid;
	DO( sm->create_md_assoc(stid.lvid, stid.serial, key, el) );
    } else {
	stid_t stid;
	GNUG_BUG_12(istrstream(av[1], strlen(av[1])) ) >> stid;
	DO( sm->create_md_assoc(stid, key, el) );
    }

    return TCL_OK;
}

#ifdef USE_RDTREE
static int
t_create_set_assoc(Tcl_Interp* ip, int ac, char* av[])
{
    if (check(ip, "stid box el", ac, 4))
      return TCL_ERROR;
 
    vec_t el;
    set_t key(strchr(av[2],'{'));
    rangeset_t rkey(key);
 
    el.put(av[3], strlen(av[3]) + 1);
    if (use_logical_id(ip)) {
 	lstid_t stid;
 	GNUG_BUG_12(istrstream(av[1], strlen(av[1])) ) >> stid;
 	DO( sm->create_set_assoc(stid.lvid, stid.serial, rkey, el) );
    } else {
 	stid_t stid;
 	GNUG_BUG_12(istrstream(av[1], strlen(av[1])) ) >> stid;
 	DO( sm->create_set_assoc(stid, rkey, el) );
    }
 
    return TCL_OK;
}
#endif /* USE_RDTREE */

static int
t_find_md_assoc(Tcl_Interp* ip, int ac, char* av[])
{
    if (check(ip, "stid box", ac, 3))
	return TCL_ERROR;

    nbox_t key(av[2]);
    bool found;

    char el[80];
    smsize_t elen = sizeof(el);

    if (use_logical_id(ip)) {
	lstid_t stid;
	GNUG_BUG_12(istrstream(av[1], strlen(av[1])) ) >> stid;
	DO( sm->find_md_assoc(stid.lvid, stid.serial, key, el, elen, found) );
    } else {
	stid_t stid;
	GNUG_BUG_12(istrstream(av[1], strlen(av[1])) ) >> stid;
	DO( sm->find_md_assoc(stid, key, el, elen, found) );
    }
    if (found) {
      Tcl_AppendElement(ip, el);
    }
    return TCL_OK;
}

#ifdef USE_RDTREE
static int
t_find_set_assoc(Tcl_Interp* ip, int ac, char* av[])
{
    if (check(ip, "stid box", ac, 3))
      return TCL_ERROR;
 
    set_t key(strchr(av[2],'{'));
    rangeset_t rkey(key);
    bool found;
 
    char el[80];
    smsize_t elen = sizeof(el);
 
    if (use_logical_id(ip)) {
 	lstid_t stid;
 	GNUG_BUG_12(istrstream(av[1], strlen(av[1])) ) >> stid;
 	DO( sm->find_set_assoc(stid.lvid, stid.serial,
			       rkey, el, elen, found) );
    } else {
 	stid_t stid;
 	GNUG_BUG_12(istrstream(av[1], strlen(av[1])) ) >> stid;
 	DO( sm->find_set_assoc(stid, rkey, el, elen, found) );
    }
    if (found) {
	Tcl_AppendElement(ip, el);
    }
    return TCL_OK;
}
#endif /* USE_RDTREE */

static int
t_destroy_md_assoc(Tcl_Interp* ip, int ac, char* av[])
{
    if (check(ip, "stid box el", ac, 4))
	return TCL_ERROR;

    vec_t el;
    nbox_t key(av[2]);

    el.put(av[3], strlen(av[3]) + 1);
    if (use_logical_id(ip)) {
	lstid_t stid;
	GNUG_BUG_12(istrstream(av[1], strlen(av[1])) ) >> stid;
	DO( sm->destroy_md_assoc(stid.lvid, stid.serial, key, el) );
    } else {
	stid_t stid;
	GNUG_BUG_12(istrstream(av[1], strlen(av[1])) ) >> stid;
	DO( sm->destroy_md_assoc(stid, key, el) );
    }

    return TCL_OK;
}

//
// following code is borrowed from EXODUS
//
static long seed = 682937845;
/* produces uniformly distributed double real numbers by the same algorithm as
   r0random except for a final real division near the end of the procedure */
static double d_random ()
{
#define   K  16807
#define   P  2147483647 /* 2 to the 31 - 1 */
#define   Q  32768      /* 2 to the 15     */
#define   R  65536      /* 2 to the 16     */

    long x1, x2, y1, y2 ;
    double random;
    x1 = seed % Q * K;
    x2 = seed / Q * K;
    y1 = x2 % R * Q;
    y2 = x2 / R;

    if (x1 <= P - y1) y1 = x1 + y1;
    else {
        y1 = x1 - P + y1 - 1;
        y2 = y2 + 1;
    }
    if (y1 < P - y2) seed = y1 + y2;
    else seed = y1 - P + y2;
    if (seed < 0) seed = seed * -1;
    random = (double)seed / (double) P;
    return(random);
} /* d_random */

static void boxGen(nbox_t* rect[], int number, const nbox_t& universe)
{
    register int i;
    int box[4];
    int length, width;

    for (i = 0; i < number; i++) {
        // generate the bounding box
//    length = (int) (d_random() * universe.side(0) / 10);
//    width = (int) (d_random() * universe.side(1) / 10);
        length = width = 20;
        if (length > 10*width || width > 10*length)
             width = length = (width + length)/2;
        box[0] = ((int) (d_random() * (universe.side(0) - length)) 
		  + universe.side(0)) % universe.side(0) 
                + universe.bound(0);
        box[1] = ((int) (d_random() * (universe.side(1) - width)) 
		  + universe.side(1)) % universe.side(1)
                + universe.bound(1);
        box[2] = box[0] + length;
        box[3] = box[1] + width;
        rect[i] = new nbox_t(2, box);
        }
}

#ifdef USE_RDTREE
static void setGen(set_t* sets[], int number, int numvals, int bound)
{
    register int i, j;
    int curvals;
    int *array;
 
    for (i = 0; i < number; i++) {
	// generate the set
 	curvals = 0;
 	// we can't handle the empty set yet!!
	//	while (curvals == 0)
 	curvals = random()%numvals;
 	array = new int[curvals];
 	for (j = 0; j < curvals; j++)
 	  array[j] = random()%bound;
 	// build the set_t
 	sets[i] = new set_t(sizeof(int), curvals, (char *)array);
    }
} 
#endif /* USE_RDTREE */

/*
 XXX why are 'flag' and 'i' static?  It breaks multiple threads.
 It's that way so jiebing can have repeatable box generation.
 In the future, the generator should be fixed.  There is also
 a problem with deleteing  possibly in-use boxes.
 */
static int
t_next_box(Tcl_Interp* ip, int ac, char* av[])
{
    if (check(ip, "universe", ac, 2))
        return TCL_ERROR;

    register j;
    static flag = FALSE;
    static i = -1;
    static nbox_t* box[50];
    nbox_t universe(av[1]);

    if (++i >= 50) {
        for (j=0; j<50; j++) delete box[j];
        flag = FALSE;
        }

    if (!flag) {
        boxGen(box, 50, universe);
        i = 0; flag = TRUE;
        }

    Tcl_AppendElement(ip, (char *) (*box[i]));

    return TCL_OK;
}

#ifdef USE_RDTREE
static int
t_next_set(Tcl_Interp* ip, int ac, char* av[])
{
    if (check(ip, "numvals/bound/seed", ac, 4))
      return TCL_ERROR;
 
    register j;
    static flag = FALSE;
    static i = -1;
    static set_t* sets[300];
    char *buf;
    int numvals = atoi(av[1]);
    int bound = atoi(av[2]);
    int seed = atoi(av[3]);
    srandom(seed);
 
    if (++i >= 300) {
	for (j=0; j<300; j++) delete sets[j];
	flag = FALSE;
    }
 
    if (!flag) {
	setGen(sets, 300, numvals, bound);
	i = 0; flag = TRUE;
    }
 
    buf = (char *) (*sets[i]);
    Tcl_AppendElement(ip, buf);
    delete [] buf;
 
    return TCL_OK;
}
#endif /* USE_RDTREE */

static int
t_draw_rtree(Tcl_Interp* ip, int ac, char* av[])
{
    if (check(ip, "stid", ac, 2))
        return TCL_ERROR;

    if (use_logical_id(ip)) {
	lstid_t stid;
	GNUG_BUG_12(istrstream(av[1], strlen(av[1])) ) >> stid;
	DO( sm->draw_rtree(stid.lvid, stid.serial) );
    } else {
	stid_t stid;
	GNUG_BUG_12(istrstream(av[1], strlen(av[1])) ) >> stid;
	DO( sm->draw_rtree(stid) );
    }
    return TCL_OK;
}

static int
t_rtree_stats(Tcl_Interp* ip, int ac, char* av[])
{
    if (check(ip, "stid", ac, 2))
        return TCL_ERROR;
    uint2 level = 5;
    uint2 ovp[5];
    rtree_stats_t stats;

    if (use_logical_id(ip)) {
	lstid_t stid;
	GNUG_BUG_12(istrstream(av[1], strlen(av[1])) ) >> stid;
	DO( sm->rtree_stats(stid.lvid, stid.serial, stats, level, ovp) );
    } else {
	stid_t stid;
	GNUG_BUG_12(istrstream(av[1], strlen(av[1])) ) >> stid;
	DO( sm->rtree_stats(stid, stats, level, ovp) );
    }

    cout << "rtree stat: " << endl;
    cout << stats << endl;

    cout << "overlapping stats: ";
    for (uint i=0; i<stats.level_cnt; i++) {
	cout << " (" << i << ") " << ovp[i];
    }
    cout << endl;
 
    return TCL_OK;
}

static int
t_rtree_scan(Tcl_Interp* ip, int ac, char* av[])
{
    if (check(ip, "stid cond box [concurrency_t]", ac, 4, 5))
        return TCL_ERROR;

    nbox_t box(av[3]);
    sob_cmp_t cond = cvt2sob_cmp_t(av[2]);
    scan_rt_i* s = NULL;

    ss_m::concurrency_t cc = ss_m::t_cc_page;
    if (ac == 5) {
	cc = cvt2concurrency_t(av[4]);
    }

    if (use_logical_id(ip)) {
	lstid_t stid;
	GNUG_BUG_12(istrstream(av[1], strlen(av[1])) ) >> stid;

	s = new scan_rt_i(stid.lvid, stid.serial, cond, box, cc);
    } else {
	stid_t stid;
	GNUG_BUG_12(istrstream(av[1], strlen(av[1])) ) >> stid;

	s = new scan_rt_i(stid, cond, box, cc);
    }
    if (!s) {
	cerr << "Out of memory: file " << __FILE__
	     << " line " << __LINE__ << endl;
	W_FATAL(fcOUTOFMEMORY);
    }

    bool eof = FALSE;
    char tmp[100];
    smsize_t elen = 100;
    nbox_t ret(box);

    cout << "------- start of query ----------\n";
    cout << "matching rects of " << av[2] 
	 << " query for rect (" << av[3] << ") are: \n";

    if ( s->next(ret, tmp, elen, eof) ) {
        delete s; return TCL_ERROR;
        }
    while (!eof) {
        ret.print(4);
	cout << "\telem " << tmp << endl;
        elen = 100;
        if ( s->next(ret, tmp, elen, eof) ) {
            delete s; return TCL_ERROR;
            }
        }

    cout << "-------- end of query -----------\n";
    delete s;
    return TCL_OK;
}

#ifdef USE_RDTREE
static int
t_rdtree_scan(Tcl_Interp* ip, int ac, char* av[])
{
    if (check(ip, "stid cond box [concurrency_t]", ac, 4, 5))
      return TCL_ERROR;
 
    set_t set(av[3]);
    sob_cmp_t cond = cvt2sob_cmp_t(av[2]);
    scan_rdt_i* s = NULL;
 
    ss_m::concurrency_t cc = ss_m::t_cc_page;
    if (ac == 5) {
	cc = cvt2concurrency_t(av[4]);
    }

    if (use_logical_id(ip)) {
 	lstid_t stid;
 	GNUG_BUG_12(istrstream(av[1], strlen(av[1])) ) >> stid;
 
 	s = new scan_rdt_i(stid.lvid, stid.serial, cond, set, cc);
    } else {
 	stid_t stid;
 	GNUG_BUG_12(istrstream(av[1], strlen(av[1])) ) >> stid;
 
 	s = new scan_rdt_i(stid, cond, set, cc);
    }
    if (!s) {
 	cerr << "Out of memory: file " << __FILE__
	  << " line " << __LINE__ << endl;
	W_FATAL(fcOUTOFMEMORY);
    }
 
    bool eof = FALSE;
    char tmp[100];
    smsize_t elen = 100;
    rangeset_t ret(set);
 
 
    cout << "------- start of query ----------\n";
    cout << "matching sets of " << av[2] 
      << " query for set (" << av[3] << ") are: \n";
 
    if ( s->next(ret, tmp, elen, eof) ) {
	delete s; return TCL_ERROR;
    }
    while (!eof) {
	ret.print(4);
 	cout << "\telem " << tmp << endl;
	elen = 100;
	if ( s->next(ret, tmp, elen, eof) ) {
	    delete s; return TCL_ERROR;
	}
    }
 
    cout << "-------- end of query -----------\n";
    delete s;
    return TCL_OK;
}
#endif /* USE_RDTREE */

static sort_stream_i* sort_container;

static int
t_begin_sort_stream(Tcl_Interp* ip, int ac, char* av[])
{
    if (check(ip, "vid runsize type klen rlen \"distinct/duplicate\" \"up/down\" [universe [derived] ]", ac, 8, 9, 10))
	return TCL_ERROR;

    key_info_t info;
    sort_parm_t sp;
    
    info.offset = 0;
    info.len = atoi(av[4]);
    info.where = key_info_t::t_hdr;

    if (!strcmp(av[3],"string")) {
        info.type = key_info_t::t_string;
    } else if (!strcmp(av[3],"char")) {
        info.type = key_info_t::t_char;
    } else if (!strcmp(av[3],"int")) {
        info.type = key_info_t::t_int;
    } else if (!strcmp(av[3],"float")) {
        info.type = key_info_t::t_float;
    } else {
        info.type = key_info_t::t_spatial;
    }

    sp.run_size = atoi(av[2]);
    sp.vol = atoi(av[1]);

    if (strcmp("normal", av[6]))  {
	sp.unique = !strcmp("distinct", av[6]);
	if (!sp.unique) {
    	    if (check(ip, "vid runsize type klen rlen \"distinct/duplicate\" \"up/down\" [universe [derived] ]", ac, 0))
            return TCL_ERROR;
        }
    }

    if (strcmp("up", av[7]))  {
	sp.ascending = strcmp("down", av[7]);
	if (sp.ascending) {
    	    if (check(ip, "vid runsize type klen rlen \"distinct/duplicate\" \"up/down\" [universe [derived] ]", ac, 0))
            return TCL_ERROR;
        }
    }

    if (ac>8) {
	nbox_t univ(av[8]);
	info.universe = univ;
    }

    if (ac>9) {
	info.derived = atoi(av[9]);
    }

    if (use_logical_id(ip)) {
        lvid_t lvid;
        GNUG_BUG_12(istrstream(av[1], strlen(av[1])) ) >> lvid;
	vid_t tmp_vid;
	DO(sm->lvid_to_vid(lvid, tmp_vid));
	sp.vol = tmp_vid;
    }
    
    if (sort_container) delete sort_container;
    sort_container = new sort_stream_i(info, sp, atoi(av[5]));
    w_assert1(sort_container);
    return TCL_OK;
}
    
static int
t_sort_stream_put(Tcl_Interp* ip, int ac, char* av[])
{
    if (check(ip, "type key data", ac, 4))
	return TCL_ERROR;

    if (!sort_container)
	return TCL_ERROR;

    vec_t key, data;
    nbox_t box(2);
    int int_num;
    float float_num;

    if (!strcmp(av[1],"string")) {
    	key.put(av[2], strlen(av[2]));
    } else if (!strcmp(av[1],"char")) {
    	key.put(av[2], sizeof(char));
    } else if (!strcmp(av[1],"int")) {
	int_num = atoi(av[2]);
    	key.put((void*)&int_num, sizeof(int));
    } else if (!strcmp(av[1],"float")) {
	float_num = atof(av[2]);
    	key.put((void*)&float_num, sizeof(float));
    } else if (!strcmp(av[1],"spatial")) {
	box.put(av[2]);
    	key.put(box.kval(), box.klen());
    } else {
	int_num = atoi(av[2]);
	key.put((void*)&int_num, sizeof(int));
    }
    
    data.put(av[3], strlen(av[3]) + 1);

    DO( sort_container->put(key, data) );
    return TCL_OK;
}

static int
t_sort_stream_get(Tcl_Interp* ip, int ac, char* av[])
{
    if (check(ip, "next", ac, 2))
	return TCL_ERROR;

    if (strcmp(av[1],"next")) {
      if (check(ip, "next", ac, 0))
	return TCL_ERROR;
    }

    char tmp[100];

    vec_t key, data;
    bool eof = true;
    if (sort_container) {
    	DO( sort_container->get_next(key, data, eof) );
    }

    if (eof) {
	Tcl_AppendElement(ip, "eof");
	if (sort_container) {
	    delete sort_container;
	    sort_container = 0;
	}
    } else {
	int buf, size;
	tmp[0] = '\0';
	key.copy_to((void*)&buf, key.size());
	sprintf(tmp, "%d", buf);
	size = strlen(tmp);
	data.copy_to(tmp+size+1, data.size());
    	tmp[size] = tmp[size + 1 + data.size() ] = '\0';
    	Tcl_AppendResult(ip, tmp, " ", tmp + size + 1, 0);
    }

    return TCL_OK;
}

static int
t_sort_stream_end(Tcl_Interp* ip, int ac, char* av[])
{
    if (sort_container) delete sort_container;
    sort_container = 0;

    // keep compiler quiet about unused parameters
    if (ip || ac || av) {}
    return TCL_OK;
}

static int
t_create_sort_rec(Tcl_Interp* ip, int ac, char* av[])
{
    if (check(ip, "fid hdr len_hint data type [universe]", ac, 6, 7))
	return TCL_ERROR;
    
    vec_t hdr, data;
    nbox_t box(2);
    int int_num;
    float float_num;
    
    if (!strcmp(av[5],"string")) {
    	hdr.put(av[2], strlen(av[2]));
    } else if (!strcmp(av[5],"char")) {
    	hdr.put(av[2], sizeof(char));
    } else if (!strcmp(av[5],"int")) {
	int_num = atoi(av[2]);
    	hdr.put((void*)&int_num, sizeof(int));
    } else if (!strcmp(av[5],"float")) {
	float_num = atof(av[2]);
    	hdr.put((void*)&float_num, sizeof(float));
    } else if (!strcmp(av[5],"spatial")) {
	box.put(av[2]);
	if (ac == 7) {
	    nbox_t u(av[6]);
	    int hval = box.hvalue(u);
    	    hdr.put(&hval, sizeof(hval));
	} else {
    	    hdr.put(box.kval(), box.klen());
        }
    } else {
	int_num = atoi(av[2]);
	hdr.put((void*)&int_num, sizeof(int));
    }
    
    
    data.put(av[4], strlen(av[4]) + 1);


    if (use_logical_id(ip)) {
	lstid_t  stid;
	lrid_t   rid;

	GNUG_BUG_12(istrstream(av[1], strlen(av[1])) ) >> stid;
	DO( sm->create_rec(stid.lvid, stid.serial, hdr, atoi(av[3]),
			   data, rid.serial) );
	rid.lvid = stid.lvid;
	tclout.seekp(ios::beg);
	tclout << rid << ends;
    } else {
	stid_t  stid;
	rid_t   rid;

	GNUG_BUG_12(istrstream(av[1], strlen(av[1])) ) >> stid;
	DO( sm->create_rec(stid, hdr, atoi(av[3]), data, rid) );
	tclout.seekp(ios::beg);
	tclout << rid << ends;
    }
    Tcl_AppendResult(ip, tclout.str(), 0);
    return TCL_OK;
}

static int
t_sort_file(Tcl_Interp* ip, int ac, char* av[])
{
    if (check(ip, "fid vid runsize type klen \"distinct/normal\" \"destruct/keep\" [\"tmp/regular\" universe [derived]]", ac, 8, 10, 11)) 
	return TCL_ERROR;
    
    key_info_t info;

    info.offset = 0;
    info.len = atoi(av[5]);
    info.where = key_info_t::t_hdr;
    info.universe = NULL;

    bool unique = FALSE;
    if (strcmp("normal", av[6]))  {
	if (strcmp("distinct", av[6]))  {
            check(ip, "fid vid runsize type klen \"distinct/normal\" [\"tmp/notmp\" universe [derived]]", ac, 0);
            return TCL_ERROR;
        } else {
            unique = TRUE;
        }
    }
    
    bool destruct = TRUE; 
    if (strcmp("destruct", av[7]))  {
	if (strcmp("keep", av[7]))  {
            check(ip, "fid vid runsize type klen \"distinct/normal\" \"destruct/keep\" [\"tmp/notmp/load_once\" universe [derived]]", ac, 0);
            return TCL_ERROR;
        } else {
            destruct = FALSE;
        }
    }
    
    if (ac > 10) {
	info.derived = atoi(av[10]);
    } else {
	info.derived = FALSE;
    }

    sm_store_property_t property = t_regular;
    if (ac > 8) {
	property = cvt2store_property(av[8]);
    }

    if (info.derived)
        info.where = key_info_t::t_hdr;

    if (!strcmp(av[4],"string")) {
        info.type = key_info_t::t_string;
    } else if (!strcmp(av[4],"char")) {
        info.type = key_info_t::t_char;
    } else if (!strcmp(av[4],"int")) {
        info.type = key_info_t::t_int;
    } else if (!strcmp(av[4],"float")) {
        info.type = key_info_t::t_float;
    } else if (!strcmp(av[4],"spatial")) {
        info.type = key_info_t::t_spatial;
	w_assert3(ac > 6);
	nbox_t bbox(av[9]);
	info.universe = bbox;
    } else {
        info.type = key_info_t::t_int;
    }

    if (use_logical_id(ip)) {
	lstid_t in_fid;
	GNUG_BUG_12(istrstream(av[1], strlen(av[1])) ) >> in_fid;
	lstid_t out_fid;
	GNUG_BUG_12(istrstream(av[2], strlen(av[2])) ) >> out_fid.lvid;

	DO( sm->sort_file(in_fid.lvid, in_fid.serial, out_fid.lvid,
			  out_fid.serial, property, info, 
			  atoi(av[3]), unique, destruct) );
	tclout.seekp(ios::beg);
	tclout << out_fid << ends;
    } else {
	stid_t in_fid;
	GNUG_BUG_12(istrstream(av[1], strlen(av[1])) ) >> in_fid;
	stid_t out_fid;
	DO( sm->sort_file(in_fid, atoi(av[2]), out_fid, property,
			  info, atoi(av[3]), unique, destruct) );
	tclout.seekp(ios::beg);
	tclout << out_fid << ends;
    }
    Tcl_AppendResult(ip, tclout.str(), 0);
    return TCL_OK;
}

static int
t_scan_sorted_recs(Tcl_Interp* ip, int ac, char* av[])
{
    if (check(ip, "fid type [universe]", ac, 3, 4))
	return TCL_ERROR;

    pin_i*  pin;
    bool  eof;
    rc_t    rc;
    scan_file_i* scan;

    if (use_logical_id(ip)) {
	lstid_t  fid;
	GNUG_BUG_12(istrstream(av[1], strlen(av[1])) ) >> fid;
	scan = new scan_file_i(fid.lvid, fid.serial, ss_m::t_cc_file);
    } else {
	stid_t  fid;
	GNUG_BUG_12(istrstream(av[1], strlen(av[1])) ) >> fid;
	scan = new scan_file_i(fid, ss_m::t_cc_file);
    }

    while ( !(rc=scan->next(pin, 0, eof)) && !eof) {

	if (!strcmp(av[2],"string")) {
	    cout << pin->hdr();
	    }
	else if (!strcmp(av[2],"char")) {
	    cout << "(" << *(char*)pin->hdr() << ")";
	    }
	else if (!strcmp(av[2],"int")) {
	    cout << "(" << *(int*)pin->hdr() << ")";
	    }
	else if (!strcmp(av[2],"float")) {
	    cout << "(" << *(float*)pin->hdr() << ")";
	    }
	else if (!strcmp(av[2],"spatial")) {
	    if (ac==4) {
		nbox_t space(av[3]);
		nbox_t key((char*)pin->hdr(), space.klen());
		cout << "(" << key.hvalue(space) << ")";
		}
	    }
	else {
	    cout << *(int*)pin->hdr();
	}

	char strbuf[100];
	if (pin->is_small()) {
	    memcpy(strbuf, pin->body(), pin->body_size());
            strbuf[pin->body_size()] = '\0';
	} else {
	    memcpy(strbuf, pin->body(), 96);
	    memcpy(&strbuf[96], "...", 3);
	    strbuf[99] = '\0';
	}
	cout << " " << strbuf << endl;
    }

    delete scan;

    DO(rc);

    Tcl_AppendElement(ip, "scan success");
    return TCL_OK;
}

static int
t_link_to_remote_id(Tcl_Interp* ip, int ac, char* av[])
{
    if (check(ip, "local_volume remote_id", ac, 3))
	return TCL_ERROR;

    if (use_logical_id(ip)) {
	lid_t remote_id;
	lid_t new_id;
	GNUG_BUG_12(istrstream(av[1], strlen(av[1])) ) >> new_id.lvid;
	GNUG_BUG_12(istrstream(av[2], strlen(av[2])) ) >> remote_id;
	DO( sm->link_to_remote_id(new_id.lvid, new_id.serial,
				  remote_id.lvid, remote_id.serial));
	tclout.seekp(ios::beg);
	tclout << new_id << ends;
    } else {
	cout << "WARNING: link_to_remote_id used without logical IDs" << endl;
	tclout.seekp(ios::beg);
	tclout << av[2] << ends;
    }
    Tcl_AppendResult(ip, tclout.str(), 0);
    return TCL_OK;
}

static int
t_convert_to_local_id(Tcl_Interp* ip, int ac, char* av[])
{
    if (check(ip, "remote_id", ac, 2))
	return TCL_ERROR;

    if (use_logical_id(ip)) {
	lid_t remote_id;
	lid_t local_id;
	GNUG_BUG_12(istrstream(av[1], strlen(av[1])) ) >> remote_id;
	DO( sm->convert_to_local_id(remote_id.lvid, remote_id.serial,
	                             local_id.lvid, local_id.serial));
	tclout.seekp(ios::beg);
	tclout << local_id << ends;
    } else {
	cout << "WARNING: convert_to_local_id used without logical IDs"
	     << endl;
	tclout.seekp(ios::beg);
	tclout << av[1] << ends;
    }
    Tcl_AppendResult(ip, tclout.str(), 0);
    return TCL_OK;
}

static int
t_lfid_of_lrid(Tcl_Interp* ip, int ac, char* av[])
{
    if (check(ip, "lrid", ac, 2))
	return TCL_ERROR;

    if (use_logical_id(ip)) {
	lid_t lrid;
	lid_t lfid;
	GNUG_BUG_12(istrstream(av[1], strlen(av[1])) ) >> lrid;
	DO( sm->lfid_of_lrid(lrid.lvid, lrid.serial, lfid.serial));
	lfid.lvid = lrid.lvid;
	tclout.seekp(ios::beg);
	tclout << lfid << ends;
    } else {
	cout << "WARNING: t_lfid_of_lrid used without logical IDs" << endl;
	tclout.seekp(ios::beg);
	tclout << av[1] << ends;
    }
    Tcl_AppendResult(ip, tclout.str(), 0);
    return TCL_OK;
}

static int
t_serial_to_rid(Tcl_Interp* ip, int ac, char* av[])
{
    if (check(ip, "lrid", ac, 2))
	return TCL_ERROR;

    if (use_logical_id(ip)) {
	rid_t rid;
	lid_t lrid;
	GNUG_BUG_12(istrstream(av[1], strlen(av[1])) ) >> lrid;
	DO( sm->serial_to_rid(lrid.lvid, lrid.serial, rid));
	tclout.seekp(ios::beg);
	tclout << rid << ends;
    } else {
	cout << "WARNING: t_serial_to_rid used without logical IDs" << endl;
	tclout.seekp(ios::beg);
	tclout << av[1] << ends;
    }
    Tcl_AppendResult(ip, tclout.str(), 0);
    return TCL_OK;
}

static int
t_serial_to_stid(Tcl_Interp* ip, int ac, char* av[])
{
    if (check(ip, "lstid", ac, 2))
	return TCL_ERROR;

    if (use_logical_id(ip)) {
	stid_t stid;
	lid_t lstid;
	GNUG_BUG_12(istrstream(av[1], strlen(av[1])) ) >> lstid;
	DO( sm->serial_to_stid(lstid.lvid, lstid.serial, stid));
	tclout.seekp(ios::beg);
	tclout << stid << ends;
    } else {
	cout << "WARNING: t_serial_to_stid used without logical IDs" << endl;
	tclout.seekp(ios::beg);
	tclout << av[1] << ends;
    }
    Tcl_AppendResult(ip, tclout.str(), 0);
    return TCL_OK;
}

static int
t_set_lid_cache_enable(Tcl_Interp* ip, int ac, char* av[])
{
    if (check(ip, "enable/disable)", ac, 2) )
	return TCL_ERROR;

    bool enable;
    if (strcmp(av[1], "enable") == 0) {
	enable = TRUE;
    } else if (strcmp(av[1], "disable") == 0) {
	enable = FALSE;
    } else {
	Tcl_AppendResult(ip, "wrong first arg, should be enable/disable", 0);
	return TCL_ERROR;
    }

    DO(sm->set_lid_cache_enable(enable));
    return TCL_OK;
}

static int
t_lid_cache_enabled(Tcl_Interp* ip, int ac, char* av[])
{
    if (check(ip, "", ac, 1) )
	return TCL_ERROR;

    // keep compiler quiet about unused parameters
    if (av) {}

    bool enabled;
    DO(sm->lid_cache_enabled(enabled));
    if (enabled) {
	Tcl_AppendResult(ip, "TRUE", 0);
    } else {
	Tcl_AppendResult(ip, "FALSE", 0);
    }

    return TCL_OK;
}

static int
t_test_lid_cache(Tcl_Interp* ip, int ac, char* av[])
{
    if (check(ip, "lvid num_add", ac, 3) )
	return TCL_ERROR;

    int num_add = atoi(av[2]);
    if (use_logical_id(ip)) {
	lvid_t lvid;
	GNUG_BUG_12(istrstream(av[1], strlen(av[1])) ) >> lvid;
	DO(sm->test_lid_cache(lvid, num_add));
    } else {
    }

    return TCL_OK;
}

static int
t_lock(Tcl_Interp* ip, int ac, char* av[])
{
    if (check(ip, "entity mode [duration [timeout]]", ac, 3, 4, 5) )
	return TCL_ERROR;

    rc_t rc; // return code
    bool use_phys = TRUE;
    lock_mode_t mode = cvt2lock_mode(av[2]);

    if (use_logical_id(ip)) {
	lid_t   id;
	lvid_t	lvid;
	int 	len = strlen(av[1]);
	istrstream l(av[1], len); l  >> id;
	istrstream v(av[1], len); v  >> lvid;

	if (l) {
	    // this was a logical id (volume ID and serial #)
	    if (ac == 3)  {
		rc = sm->lock(id.lvid, id.serial, mode);
	    } else {
		lock_duration_t duration = cvt2lock_duration(av[3]);
		if (ac == 4) {
		    rc = sm->lock(id.lvid, id.serial, mode, duration);
		} else {
		    long timeout = atoi(av[4]);
		    rc = sm->lock(id.lvid, id.serial, mode, duration, timeout);
		}
	    }
	} else {
	    // this was only a logical volume id (volume ID and serial #)
	    if (ac == 3)  {
		rc = sm->lock(lvid, mode);
	    } else {
		lock_duration_t duration = cvt2lock_duration(av[3]);
		if (ac == 4) {
		    rc = sm->lock(lvid, mode, duration);
		} else {
		    long timeout = atoi(av[4]);
		    rc = sm->lock(lvid, mode, duration, timeout);
		}
	    }
	}
	if (rc)  {
	    if (rc.err_num() != ss_m::eBADLOGICALID && rc.err_num() != ss_m::eBADVOL) { 
		return TCL_ERROR;
	    }
        } else {
	    use_phys = FALSE;
	}
    }
   
    if (use_phys) {
	lockid_t n = cvt2lockid_t(av[1]);
	
	if (ac == 3)  {
	    DO( sm->lock(n, mode) );
	} else {
	    lock_duration_t duration = cvt2lock_duration(av[3]);
	    if (ac == 4) {
		DO( sm->lock(n, mode, duration) );
	    } else {
		long timeout = atoi(av[4]);
		DO( sm->lock(n, mode, duration, timeout) );
	    }
	}
    }
    return TCL_OK;
}

static int
t_lock_many(Tcl_Interp* ip, int ac, char* av[])
{
    if (check(ip, "num_requests entity mode [duration [timeout]]", 
	      ac, 4, 5, 6) )
	return TCL_ERROR;

    rc_t rc; // return code
    bool use_phys = TRUE;
    int num_requests = atoi(av[1]);
    int i;
    lock_mode_t mode = cvt2lock_mode(av[3]);

    if (use_logical_id(ip)) {
	lid_t   id;
	GNUG_BUG_12(istrstream(av[2], strlen(av[2])) ) >> id;
	if (ac == 4)  {
	    for (i = 0; i < num_requests; i++) {
		rc = sm->lock(id.lvid, id.serial, mode);
		if (rc) break;
	    }
	} else {
	    lock_duration_t duration = cvt2lock_duration(av[4]);
	    if (ac == 5) {
		for (i = 0; i < num_requests; i++) {
		    rc = sm->lock(id.lvid, id.serial, mode, duration);
		    if (rc) break;
		}
	    } else {
		long timeout = atoi(av[5]);
		for (i = 0; i < num_requests; i++) {
		    rc = sm->lock(id.lvid, id.serial, mode, duration, timeout);
		    if (rc) break;
		}
	    }
	}
	if (rc)  {
	    if (rc.err_num() != ss_m::eBADLOGICALID && rc.err_num() != ss_m::eBADVOL) { 
		return TCL_ERROR;
	    }
        } else {
	    use_phys = FALSE;
	}
    }
   
    if (use_phys) {
	lockid_t n = cvt2lockid_t(av[2]);
	
	if (ac == 4)  {
	    for (i = 0; i < num_requests; i++)
		DO( sm->lock(n, mode) );
	} else {
	    lock_duration_t duration = cvt2lock_duration(av[4]);
	    if (ac == 5) {
		for (i = 0; i < num_requests; i++)
		    DO( sm->lock(n, mode, duration) );
	    } else {
		long timeout = atoi(av[5]);
		for (i = 0; i < num_requests; i++)
		    DO( sm->lock(n, mode, duration, timeout) );
	    }
	}
    }
    return TCL_OK;
}

static int
t_unlock(Tcl_Interp* ip, int ac, char* av[])
{
    if (check(ip, "entity", ac, 2) )
	return TCL_ERROR;

    rc_t rc; // return code
    bool use_phys = TRUE;

    if (use_logical_id(ip)) {
	lid_t   id;
	GNUG_BUG_12(istrstream(av[1], strlen(av[1])) ) >> id;
	rc = sm->unlock(id.lvid, id.serial);
	if (rc)  {
	    if (rc.err_num() != ss_m::eBADLOGICALID && rc.err_num() != ss_m::eBADVOL) { 
		return TCL_ERROR;
	    }
        } else {
	    use_phys = FALSE;
	}
    }
    if (use_phys) {
	lockid_t n = cvt2lockid_t(av[1]);
	DO( sm->unlock(n) );
    }
    return TCL_OK;
}

extern "C" void dumpthreads();

static int
t_dump_threads(Tcl_Interp* ip, int ac, char*[])
{
    if (check(ip, "", ac, 1))
	return TCL_ERROR;

    dumpthreads();
    return TCL_OK;
}
static int
t_dump_locks(Tcl_Interp* ip, int ac, char*[])
{
    if (check(ip, "", ac, 1))
	return TCL_ERROR;

    DO( sm->dump_locks() );
    return TCL_OK;
}

static int
t_query_lock(Tcl_Interp* ip, int ac, char* av[])
{
    if (check(ip, "entity", ac, 2) )
	return TCL_ERROR;

    rc_t rc; // return code
    bool use_phys = TRUE;
    lock_mode_t m = NL;

    if (use_logical_id(ip)) {
	lid_t   id;
	GNUG_BUG_12(istrstream(av[1], strlen(av[1])) ) >> id;
	rc = sm->query_lock(id.lvid, id.serial, m);
	if (rc)  {
	    if (rc.err_num() != ss_m::eBADLOGICALID && rc.err_num() != ss_m::eBADVOL) { 
		return TCL_ERROR;
	    }
        } else {
	    use_phys = FALSE;
	}
    }
    if (use_phys) {
	lockid_t n = cvt2lockid_t(av[1]);
	DO( sm->query_lock(n, m) );
    }

    Tcl_AppendResult(ip, lock_base_t::mode_str[m], 0);
    return TCL_OK;
}

static int
t_set_lock_cache_enable(Tcl_Interp* ip, int ac, char* av[])
{
    if (check(ip, "enable/disable)", ac, 2) )
	return TCL_ERROR;

    bool enable;
    if (strcmp(av[1], "enable") == 0) {
	enable = TRUE;
    } else if (strcmp(av[1], "disable") == 0) {
	enable = FALSE;
    } else {
	Tcl_AppendResult(ip, "wrong first arg, should be enable/disable", 0);
	return TCL_ERROR;
    }

    DO(sm->set_lock_cache_enable(enable));
    return TCL_OK;
}

static int
t_lock_cache_enabled(Tcl_Interp* ip, int ac, char* av[])
{
    if (check(ip, "", ac, 1) )
	return TCL_ERROR;
    // keep compiler quiet about unused parameters
    if (av) {}

    bool enabled;
    DO(sm->lock_cache_enabled(enabled));
    if (enabled) {
	Tcl_AppendResult(ip, "TRUE", 0);
    } else {
	Tcl_AppendResult(ip, "FALSE", 0);
    }

    return TCL_OK;
}

static int
t_print_lid_index(Tcl_Interp* ip, int ac, char* av[])
{
    if (check(ip, "vid", ac, 2) )
	return TCL_ERROR;

    if (use_logical_id(ip))  {
	lvid_t lvid;
	GNUG_BUG_12(istrstream (av[1], strlen(av[1])) ) >> lvid;
	DO( sm->print_lid_index(lvid) );
	return TCL_OK;
    }
    
    Tcl_AppendResult(ip, "logical id not used", 0);
    return TCL_ERROR;
}

static int
t_create_many_rec(Tcl_Interp* ip, int ac, char* av[])
{
    if (check(ip, "num_recs fid hdr len_hint chunkdata chunkcount", ac, 7)) 
	return TCL_ERROR;
   
    unsigned num_recs = atoi(av[1]);
    unsigned chunk_count = atoi(av[6]);
    unsigned len_hint = atoi(av[4]);
    vec_t hdr, data;
    hdr.put(av[3], strlen(av[3]));
    data.put(av[5], strlen(av[5]));

    
    if (use_logical_id(ip)) {
	lstid_t  stid;
	lrid_t   rid;

	GNUG_BUG_12(istrstream(av[2], strlen(av[2])) ) >> stid;
	cout << "creating " << num_recs << " records in " << stid
	     << " with hdr_len= " << hdr.size() << " chunk_len= "
	     << data.size() << " in " << chunk_count << " chunks."
	     << endl;

	for (uint i = 0; i < num_recs; i++) {
	    DO( sm->create_rec(stid.lvid, stid.serial, hdr, 
			       len_hint, data, rid.serial) );
	    for (uint j = 1; j < chunk_count; j++) {
		DO( sm->append_rec(stid.lvid, rid.serial, data));
	    }
	}
	rid.lvid = stid.lvid;

	tclout.seekp(ios::beg);
	tclout << rid << ends;

    } else {
	stid_t  stid;
	rid_t   rid;
	
	GNUG_BUG_12(istrstream(av[2], strlen(av[2])) ) >> stid;
	cout << "creating " << num_recs << " records in " << stid
	     << " with hdr_len= " << hdr.size() << " chunk_len= "
	     << data.size() << " in " << chunk_count << " chunks."
	     << endl;

	for (uint i = 0; i < num_recs; i++) {
	    DO( sm->create_rec(stid, hdr, len_hint, data, rid) );
	    for (uint j = 1; j < chunk_count; j++) {
		DO( sm->append_rec(rid, data, FALSE));
	    }
	}
	tclout.seekp(ios::beg);
	tclout << rid << ends;
    }
    Tcl_AppendResult(ip, tclout.str(), 0);
    return TCL_OK;
}

static int
t_update_rec_many(Tcl_Interp* ip, int ac, char* av[])
{
    if (check(ip, "num_updates rid start data", ac, 5)) 
	return TCL_ERROR;
    
    uint4   start;
    vec_t   data;
    int	    num_updates = atoi(av[1]);

    data.set(parse_vec(av[4], strlen(av[4])));
    start = atoi(av[3]);    

    if (use_logical_id(ip)) {
	lrid_t   rid;

	GNUG_BUG_12(istrstream(av[2], strlen(av[2])) ) >> rid;
	for (int i = 0; i < num_updates; i++) {
	    DO( sm->update_rec(rid.lvid, rid.serial, start, data) );
	}

    } else {
	rid_t   rid;

	GNUG_BUG_12(istrstream(av[2], strlen(av[2])) ) >> rid;
	for (int i = 0; i < num_updates; i++) {
	    DO( sm->update_rec(rid, start, data) );
	}
    }

    Tcl_AppendElement(ip, "update success");
    return TCL_OK;
}


static int
t_lock_record_blind(Tcl_Interp* ip, int ac, char* av[])
{
    if (check(ip, "num", ac, 2))
	return TCL_ERROR;

    const page_capacity = 20;
    rid_t rid;
    rid.pid._stid.vol = 999;
    uint4 n;
    GNUG_BUG_12(istrstream(av[1], strlen(av[1]))) >> n;

    unix_stats t;
    t.start();
    
    for (uint i = 0; i < n; i += page_capacity)  {
	rid.pid.page = i+1;
	uint j = page_capacity;
	if (i + j >= n) j = n - i; 
	while (j--) {
	    rid.slot = j;
	    DO( sm->lock(rid, EX) );
	}
    }

    t.stop();
    cerr << "Time to get " << n << " record locks: " << endl;
    cerr << t << endl;

    return TCL_OK;
}


static int
t_testing(Tcl_Interp* ip, int ac, char* av[])
{
    if (check(ip, "stid key times", ac, 4))
	return TCL_ERROR;

    stid_t stid;
    GNUG_BUG_12(istrstream(av[1], strlen(av[1])) ) >> stid;

    //align the key
    char* key_align = strdup(av[2]);
    w_assert1(key_align);

    vec_t key;
    key.set(key_align, strlen(key_align));

    int times = atoi(av[3]);

    char el[80];
    smsize_t elen = sizeof(el) - 1;
    
    bool found;
    for (int i = 0; i < times; i++)  {
	DO( sm->find_assoc(stid, key, el, elen, found) );
    }

    free(key_align);
    el[elen] = '\0';

    if (found)  {
	Tcl_AppendElement(ip, el);
	return TCL_OK;
    }

    Tcl_AppendElement(ip, "not found");
    return TCL_ERROR;
}

static int
t_janet_setup(Tcl_Interp* ip, int ac, char* av[])
{
    if (check(ip, "stid nkeys", ac, 3))
	return TCL_ERROR;

    stid_t stid;
    GNUG_BUG_12(istrstream(av[1], strlen(av[1])) ) >> stid;

    int nkeys = atoi(av[2]);

    int buf[3];

    int i;
    vec_t k(&i, sizeof(i));
    vec_t e(buf, sizeof(buf));

    for (i = 0; i < nkeys; i++)  {
	buf[0] = buf[1] = buf[2] = i;
	DO( sm->create_assoc(stid, k, e));
    }

    return TCL_OK;
}

static int
t_janet_lookup(Tcl_Interp* ip, int ac, char* av[])
{
    if (check(ip, "stid nkeys nlookups", ac, 4))
	return TCL_ERROR;

    stid_t stid;
    GNUG_BUG_12(istrstream(av[1], strlen(av[1])) ) >> stid;

    int nkeys = atoi(av[2]);
    int nlookups = atoi(av[3]);

    int buf[3];

    int key;
    smsize_t elen = sizeof(buf);

    vec_t kv(&key, sizeof(key));

    for (int i = 0; i < nlookups; i++)  {
	key = rand() % nkeys;
	bool found;
	DO( sm->find_assoc(stid, kv, buf, elen, found) );
	if (buf[0] != key || buf[1] != key || buf[2] != key || 
		elen != sizeof(buf)) {
	    cerr << "janet_lookup: btree corrupted" << endl
		 << "key = " << key << endl
		 << "buf[0] = " << buf[0] << endl
		 << "buf[1] = " << buf[1] << endl
		 << "buf[2] = " << buf[2] << endl;
	}
    }

    return TCL_OK;
}


struct cmd_t {
    char* name;
    smproc_t* func;
};

static cmd_t cmd[] = {
    { "testing", t_testing },
    { "janet_setup", t_janet_setup },
    { "janet_lookup", t_janet_lookup },
    { "sleep", t_sleep },
    { "test_int_btree", t_test_int_btree },
    { "test_bulkload_int_btree", t_test_bulkload_int_btree },

    { "checkpoint", t_checkpoint },

    // thread/xct related
    { "begin_xct", t_begin_xct },
    { "abort_xct", t_abort_xct },
    { "commit_xct", t_commit_xct },
    { "chain_xct", t_chain_xct },
    { "save_work", t_save_work },
    { "rollback_work", t_rollback_work },
    { "open_quark", t_open_quark },
    { "close_quark", t_close_quark },
    { "xct", t_xct },  // get current transaction
    { "attach_xct", t_attach_xct},
    { "detach_xct", t_detach_xct},
    { "state_xct", t_state_xct},
    { "xct_to_tid", t_xct_to_tid},
    { "tid_to_xct", t_tid_to_xct},
    { "prepare_xct", t_prepare_xct},
    { "enter2pc", t_enter2pc},
    { "recover2pc", t_recover2pc},

    //
    // basic sm stuff
    //
    { "force_buffers", t_force_buffers },
    { "force_vol_hdr_buffers", t_force_vol_hdr_buffers },
    { "gather_stats", t_gather_stats },
    { "snapshot_buffers", t_snapshot_buffers },
    { "print_lid_index", t_print_lid_index },
    { "config_info", t_config_info },
    { "set_disk_delay", t_set_disk_delay },
    { "start_log_corruption", t_start_log_corruption },
    { "sync_log", t_sync_log },
    { "dump_buffers", t_dump_buffers },
    { "set_store_property", t_set_store_property },
    { "get_store_property", t_get_store_property },

    // Device and Volume
    { "format_dev", t_format_dev },
    { "mount_dev", t_mount_dev },
    { "dismount_dev", t_dismount_dev },
    { "dismount_all", t_dismount_all },
    { "list_devices", t_list_devices },
    { "list_volumes", t_list_volumes },
    { "generate_new_lvid", t_generate_new_lvid },
    { "create_vol", t_create_vol },
    { "destroy_vol", t_destroy_vol },
    { "add_logical_id_index", t_add_logical_id_index },
    { "has_logical_id_index", t_has_logical_id_index },
    { "get_volume_quota", t_get_volume_quota },
    { "get_device_quota", t_get_device_quota },
    { "vol_root_index", t_vol_root_index },

    { "get_du_statistics", t_get_du_statistics },   // DU DF
    //
    // Debugging
    //
    { "set_debug", t_set_debug },
    //
    //	Indices
    //
    { "create_index", t_create_index },
    { "destroy_index", t_destroy_index },
    { "blkld_ndx", t_blkld_ndx },
    { "create_assoc", t_create_assoc },
    { "destroy_all_assoc", t_destroy_all_assoc },
    { "destroy_assoc", t_destroy_assoc },
    { "find_assoc", t_find_assoc },
    { "print_index", t_print_index },

    // R-Trees
    { "create_md_index", t_create_md_index },
    { "create_md_assoc", t_create_md_assoc },
    { "find_md_assoc", t_find_md_assoc },
    { "destroy_md_assoc", t_destroy_md_assoc },
    { "next_box", t_next_box },
    { "draw_rtree", t_draw_rtree },
    { "rtree_stats", t_rtree_stats },
    { "rtree_query", t_rtree_scan },
    { "blkld_md_ndx", t_blkld_md_ndx },
    { "sort_file", t_sort_file },
    { "create_sort_rec", t_create_sort_rec },
    { "scan_sorted_recs", t_scan_sorted_recs },
    { "print_md_index", t_print_md_index },

    { "begin_sort_stream", t_begin_sort_stream },
    { "sort_stream_put", t_sort_stream_put },
    { "sort_stream_get", t_sort_stream_get },
    { "sort_stream_end", t_sort_stream_end },

   // RD-Trees
#ifdef USE_RDTREE
   { "create_set_index", t_create_set_index },
   { "create_set_assoc", t_create_set_assoc },
   { "find_set_assoc", t_find_set_assoc },
   { "next_set", t_next_set },
   { "rdtree_query", t_rdtree_scan },
   { "print_set_index", t_print_set_index },
#endif /* USE_RDTREE */
    
    //
    //  Scans
    //
    { "create_scan", t_create_scan },
    { "scan_next", t_scan_next },
    { "destroy_scan", t_destroy_scan },
    //
    // Files, records
    //
    { "create_file", t_create_file },
    { "destroy_file", t_destroy_file },
    { "create_rec", t_create_rec },
    { "create_multi_recs", t_create_multi_recs },
    { "multi_file_append", t_multi_file_append },
    { "destroy_rec", t_destroy_rec },
    { "read_rec", t_read_rec },
    { "print_rec", t_print_rec },
    { "read_rec_body", t_read_rec_body },
    { "update_rec", t_update_rec },
    { "update_rec_hdr", t_update_rec_hdr },
    { "append_rec", t_append_rec },
    { "truncate_rec", t_truncate_rec },
    { "scan_recs", t_scan_recs },
    { "scan_rids", t_scan_rids },

    // Pinning
    { "pin_create", t_pin_create },
    { "pin_destroy", t_pin_destroy },
    { "pin_pin", t_pin_pin },
    { "pin_unpin", t_pin_unpin },
    { "pin_repin", t_pin_repin },
    { "pin_body", t_pin_body },
    { "pin_body_cond", t_pin_body_cond },
    { "pin_next_bytes", t_pin_next_bytes },
    { "pin_hdr", t_pin_hdr },
    { "pin_pinned", t_pin_pinned },
    { "pin_is_small", t_pin_is_small },
    { "pin_rid", t_pin_rid },
    { "pin_append_rec", t_pin_append_rec },
    { "pin_update_rec", t_pin_update_rec },
    { "pin_update_rec_hdr", t_pin_update_rec_hdr },
    { "pin_truncate_rec", t_pin_truncate_rec },

    // Scanning files
    { "scan_file_create", t_scan_file_create },
    { "scan_file_destroy", t_scan_file_destroy },
    { "scan_file_next", t_scan_file_next },
    { "scan_file_cursor", t_scan_file_cursor },
    { "scan_file_next_page", t_scan_file_next_page },
    { "scan_file_finish", t_scan_file_finish },
    { "scan_file_append", t_scan_file_append },

    // Logical ID related
    { "link_to_remote_id", t_link_to_remote_id },
    { "convert_to_local_id", t_convert_to_local_id },
    { "lfid_of_lrid", t_lfid_of_lrid },
    { "serial_to_rid", t_serial_to_rid },
    { "serial_to_stid", t_serial_to_stid },
    { "set_lid_cache_enable", t_set_lid_cache_enable },
    { "lid_cache_enabled", t_lid_cache_enabled },
    { "test_lid_cache", t_test_lid_cache },

    // Lock
    { "lock", t_lock },
    { "lock_many", t_lock_many },
    { "unlock", t_unlock },
    { "dump_locks", t_dump_locks },
    { "dump_threads", t_dump_threads },
    { "query_lock", t_query_lock },
    { "set_lock_cache_enable", t_set_lock_cache_enable },
    { "lock_cache_enabled", t_lock_cache_enabled },

    // Restart
    { "restart", t_restart },
    // crash name "command"
    { "crash", t_crash },

    //
    // Performance tests
    //   Name format:
    //     OP_many_rec = perform operation on many records
    //     OP_rec_many = perform operation on 1 record many times
    //
    { "create_many_rec", t_create_many_rec },
    { "update_rec_many", t_update_rec_many },
    { "lock_record_blind", t_lock_record_blind },

    //{ 0, 0}
};

static int cmd_cmp(const void* c1, const void* c2) 
{
    cmd_t& cmd1 = *(cmd_t*) c1;
    cmd_t& cmd2 = *(cmd_t*) c2;
    return strcmp(cmd1.name, cmd2.name);
}

void sm_dispatch_init()
{
    qsort( (char*)cmd, sizeof(cmd)/sizeof(cmd_t), sizeof(cmd_t), cmd_cmp);
}

int
sm_dispatch(ClientData, Tcl_Interp* ip, int ac, char* av[])
{
    int ret = TCL_OK;
    cmd_t key;

    w_assert3(ac >= 2);	
    key.name = av[1];
    cmd_t* found = (cmd_t*) bsearch((char*)&key, 
				    (char*)cmd, 
				    sizeof(cmd)/sizeof(cmd_t), 
				    sizeof(cmd_t), cmd_cmp);
    if (found) {
	ret = found->func(ip, ac - 1, av + 1);
	return ret;
    }

    tclout.seekp(ios::beg);
    tclout << __FILE__ << ':' << __LINE__  << ends;
    Tcl_AppendResult(ip, tclout.str(), " unknown sm routine \"",
		     av[1], "\"", 0);
    return TCL_ERROR;
}



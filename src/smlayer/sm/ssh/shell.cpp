/*<std-header orig-src='shore'>

 $Id: shell.cpp,v 1.306 1999/08/16 19:44:51 nhall Exp $

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

#define SHELL_C


#include "shell.h"

#include <stdio.h>


#ifdef __GNUG__
#define	IOS_FAIL(stream)	stream.setstate(ios::failbit)
#elif defined(_MSC_VER)
#define	IOS_FAIL(stream)	stream.setf(ios::failbit)
#else
#define	IOS_FAIL(stream)	stream.setstate(ios_base::failbit)
#endif


#ifdef W_DEBUG
// Set to true if you want every occurence of rc_t error
// to get output to stderr
bool dumprc = false; // see shell.h
#endif /* W_DEBUG */

#ifdef USE_COORD
CentralizedGlobalDeadlockServer *globalDeadlockServer = 0;
sm_coordinator* co = 0;
#endif
const char*
cvt_store_t(ss_m::store_t n)
{
    switch(n) {
    case ss_m::t_bad_store_t:
	return "t_bad_store_t";
    case ss_m::t_index:
	return "t_index";
    case ss_m::t_file:
	return "t_file";
    case ss_m::t_lgrec:
	return "t_lgrec";
    case ss_m::t_1page:
	return "t_1page";
    }
    return "unknown";
}
const char*
cvt_ndx_t(ss_m::ndx_t n)
{
    switch(n) {
    case ss_m::t_btree:
	return "t_btree";
    case ss_m::t_uni_btree:
	return "t_uni_btree";
    case ss_m::t_rtree:
	return "t_rtree";
    case ss_m::t_rdtree:
	return "t_rdtree";
    case ss_m::t_lhash:
	return "t_lhash";
    case ss_m::t_bad_ndx_t:
	return "t_bad_ndx_t";
    }
    return "unknown";
}
const char*
cvt_concurrency_t( ss_m::concurrency_t cc)
{
    switch(cc) {
    case ss_m::t_cc_none:
	return "t_cc_none";
    case ss_m::t_cc_record:
	return "t_cc_record";
    case ss_m::t_cc_page:
	return "t_cc_page";
    case ss_m::t_cc_file:
	return "t_cc_file";
    case ss_m::t_cc_vol:
	return "t_cc_vol";
    case ss_m::t_cc_kvl:
	return "t_cc_kvl";
    case ss_m::t_cc_modkvl:
	return "t_cc_modkvl";
    case ss_m::t_cc_im:
	return "t_cc_im";
    case ss_m::t_cc_append:
	return "t_cc_append";
    case ss_m::t_cc_bad: 
	return "t_cc_bad";
    }
    return "unknown";
}
const char *check_compress_flag(const char *kd)
{
    if(!force_compress) {
	return kd;
    }
    static char _result[100];
    switch(kd[0]) {
	case 'b':
		_result[0] = 'B';
		break;
	case 'i':
		_result[0] = 'I';
		break;
	case 'u':
		_result[0] = 'U';
		break;
	case 'f':
		_result[0] = 'F';
		break;
	default:
		_result[0] = kd[0];
		break;
    }
    memcpy(&_result[1], &kd[1], strlen(kd));
    return _result;
}

vid_t 
make_vid_from_lvid(const char* lv)
{
    // see if the lvid string represents a long lvid)
    if (!strchr(lv, '.')) {
	vid_t vid;
	istrstream anon(VCPP_BUG_1 lv, strlen(lv));
	anon >> vid;

	return vid;
    }
    lvid_t lvid;
    istrstream anon(VCPP_BUG_1 lv, strlen(lv)); anon >> lvid;
    return vid_t((uint2_t)lvid.low);
}

bool 
tcl_scan_boolean(char *rep, bool &result)
{
    if (strcasecmp(rep, "true") == 0) {
	    result = true;
	    return true;
    }
    if (strcasecmp(rep, "false") == 0) {
	    result = false;
	    return true;
    }
    /* could generate an error, or an rc, or ??? */
    return false;
}

ss_m::ndx_t cvt2ndx_t(char* s); //forward ref

void
cvt2lockid_t(char* str, lockid_t &n)
{
    stid_t stid;
    lpid_t pid;
    rid_t rid;
    kvl_t kvl;
    vid_t vid;
    extid_t extid;
    lockid_t::user1_t u1;
    lockid_t::user2_t u2;
    lockid_t::user3_t u3;
    lockid_t::user4_t u4;
    int len = strlen(str);

    istrstream ss(str, len);

    /* This switch conversion is used, because the previous,
       "try everything" was causing problems with I/O streams.
       It's better anyway, because it is deterministic instead
       of priority ordered. */
    switch (str[0]) {
    case 's':
	    ss >> stid;
	    break;
    case 'x':
	    str[0] = 's'; // GAK
	    ss >> stid;
	    extid.vol = stid.vol;
	    extid.ext = (snum_t) stid.store;
	    str[0] = 'x'; // GAK
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
    case 'u':
	    if (len < 2)  {
	    	IOS_FAIL(ss);
		break;
	    }
	    switch (str[1])  {
		case '1':
		    ss >> u1;
		    break;
		case '2':
		    ss >> u2;
		    break;
		case '3':
		    ss >> u3;
		    break;
		case '4':
		    ss >> u4;
		    break;
		default:
		    IOS_FAIL(ss);
		    break;
	    }
	    break;
    default:
	    /* volume id's are just XXX.YYY.  They should be changed  */
	    ss >> vid;
	    break;
    }

    if (!ss) {
	    cerr << "cvt2lockid_t: bad lockid -- " << str << endl;
	    abort();
	    return; 
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
    case 'x':
	    n = lockid_t(extid);
	    break;
    case 'k':
	    n = lockid_t(kvl);
	    break;
    case 'u':
	    switch (str[1])  {
		case '1':
		    n = lockid_t(u1);
		    break;
		case '2':
		    n = lockid_t(u2);
		    break;
		case '3':
		    n = lockid_t(u3);
		    break;
		case '4':
		    n = lockid_t(u4);
		    break;
	    }
	    break;
    default:
	    n = lockid_t(vid);
	    break;
    }

    return;
}

bool 
use_logical_id(Tcl_Interp* ip)
{
    extern const char* Logical_id_flag_tcl; // from ssh.cpp

    char* value = Tcl_GetVar(ip, TCL_CVBUG Logical_id_flag_tcl, TCL_GLOBAL_ONLY); 
    w_assert1(value != NULL);
    char result = value[0];
    bool ret = false;

    switch (result) {
    case '1':
	ret = true;
	break;
    case '0':
	ret = false;
	break;
    default:
	W_FATAL(fcINTERNAL);
    }
    return ret;
}

vec_t &
parse_vec(const char *c, int len) 
{
    DBG(<<"parse_vec("<<c<<")");
    static vec_t junk;
    smsize_t 	i;

    junk.reset();
    if(::strncmp(c, "zvec", 4)==0) {
	c+=4;
	i = objectsize(c);
	junk.set(zvec_t(i));
	DBG(<<"returns zvec_t("<<i<<")");
	    return junk;
    }
    DBG(<<"returns normal vec_t()");
    junk.put(c, len);
    return junk;
}

static int
t_checkpoint(Tcl_Interp* ip, int ac, char* /*av*/[])
{
    if (check(ip, "", ac, 1))  return TCL_ERROR;
    DO(sm->checkpoint());
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

    /* Instrument all our xcts */
    sm_stats_info_t *s = new sm_stats_info_t;
    memset(s, '\0', sizeof(sm_stats_info_t));
    DO( sm->begin_xct(s) );
    return TCL_OK;
}

static int
t_commit_xct(Tcl_Interp* ip, int ac, char* av[])
{
    if (check(ip, "[lazy]", ac, 1, 2)) return TCL_ERROR;
    bool lazy = false;
    if (ac == 2) lazy = (strcmp(av[1], "lazy") == 0);
    sm_stats_info_t *s=0;
    DO( sm->commit_xct(s, lazy) );
    /*
     * print stats
     */
    if(linked.instrument_flag && s) {
	tclout.seekp(ios::beg);
	tclout << *s << ends;
    }
    delete s;
    return TCL_OK;
}

static  int
t_chain_xct(Tcl_Interp* ip, int ac, char* av[])
{
    if (check(ip, "[lazy]", ac, 1, 2))  return TCL_ERROR;
    bool lazy = false;
    if (ac == 2) lazy = (strcmp(av[1], "lazy") == 0);
    /*
     * For chain, we have to hand in a new struct,
     * and we get back the old one.
     */
    sm_stats_info_t *s = new sm_stats_info_t;
    memset(s, '\0', sizeof(sm_stats_info_t));
    DO( sm->chain_xct(s, lazy) );
    /*
     * print stats
     */
    if(linked.instrument_flag && s) {
	tclout.seekp(ios::beg);
	tclout << *s << ends;
    }
    delete s;
    return TCL_OK;
}

static int
t_set_coordinator(Tcl_Interp* ip, int ac, char* av[])
{

    if (check(ip, "handle", ac, 2)) return TCL_ERROR;
    server_handle_t	h(av[1]);
    DO( sm->set_coordinator(h));
    return TCL_OK;
}

static int
t_enter2pc(Tcl_Interp* ip, int ac, char* av[])
{
    if (check(ip, "gtid_t", ac, 2)) return TCL_ERROR;
    gtid_t	g(av[1]);
    DO( sm->enter_2pc(g));
    return TCL_OK;
}

static int
t_recover2pc(Tcl_Interp* ip, int ac, char* av[])
{
    if (check(ip, "gtid_t", ac, 2)) return TCL_ERROR;

    gtid_t	g(av[1]);
    tid_t 		t;
    DO( sm->recover_2pc(g, true, t));
    tclout.seekp(ios::beg);
    tclout << t << ends;
    Tcl_AppendResult(ip, tclout.str(), 0);
    return TCL_OK;
}

static int
t_num_active(Tcl_Interp* ip, int ac, char*[] )
{
    if (check(ip, "", ac, 1)) return TCL_ERROR;

    int num = ss_m::num_active_xcts();
    tclout.seekp(ios::beg);
    tclout << num << ends;
    Tcl_AppendResult(ip, tclout.str(), 0);
    return TCL_OK;
}

static int
t_num_prepared(Tcl_Interp* ip, int ac, char*[] )
{
    if (check(ip, "", ac, 1)) return TCL_ERROR;

    int numu=0, num=0;
    DO( sm->query_prepared_xct(num));
#ifdef USE_COORD
    if(co) {
	DO( co->num_unresolved(numu));
    } 
#endif
    num += numu ;
    tclout.seekp(ios::beg);
    tclout << num << ends;
    Tcl_AppendResult(ip, tclout.str(), 0);
    return TCL_OK;
}

static int
t_prepare_xct(Tcl_Interp* ip, int ac, char* /*av*/[])
{
    if (check(ip, "", ac, 1)) return TCL_ERROR;
    sm_stats_info_t *s=0;
    vote_t	v;
    DO( sm->prepare_xct(s, v));
    /*
     * print stats
     */
    if(linked.instrument_flag && s) {
	tclout.seekp(ios::beg);
	tclout << *s << ends;
    }
    delete s;

    tclout.seekp(ios::beg);
    tclout << int(v) << ends;
    Tcl_AppendResult(ip, tclout.str(), 0);
    return TCL_OK;
}

int
t_abort_xct(Tcl_Interp* ip, int ac, char* /*av*/[])
{
    if (check(ip, "", ac, 1)) return TCL_ERROR;
    sm_stats_info_t *s=0;
    DO( sm->abort_xct(s));
    /*
     * print stats
     */
    if(linked.instrument_flag && s) {
	tclout.seekp(ios::beg);
	tclout << *s << ends;
    }
    delete s;
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
    istrstream anon(VCPP_BUG_1  av[1], strlen(av[1])); anon >> sp;
    DO( sm->rollback_work(sp));

    return TCL_OK;
}

static int
t_open_quark(Tcl_Interp* ip, int ac, char* /*av*/[])
{
    if (check(ip, "", ac, 1))  return TCL_ERROR;

    sm_quark_t *quark= new sm_quark_t;
    DO(quark->open());
    tclout.seekp(ios::beg);
    tclout << quark << ends;
    Tcl_AppendResult(ip, tclout.str(), 0);
    return TCL_OK;
}

static int
t_close_quark(Tcl_Interp* ip, int ac, char* av[])
{
    if (check(ip, "close_quark quark", ac, 2, 2))  return TCL_ERROR;

    sm_quark_t *quark = (sm_quark_t*) strtol(av[1], 0, 0);
    DO(quark->close());
    delete quark;
    return TCL_OK;
}

static int
t_xct(Tcl_Interp* ip, int ac, char* /*av*/[])
{
    if (check(ip, "", ac, 1))  return TCL_ERROR;

    tclout.seekp(ios::beg);
    tclout << unsigned(xct()) <<  ends;
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
    switch(t) {
	case ss_m::xct_stale:
		tclout << "xct_stale" << ends;
		break;
	case ss_m::xct_active:
		tclout << "xct_active" << ends;
		break;
	case ss_m::xct_prepared:
		tclout << "xct_prepared" << ends;
		break;
	case ss_m::xct_aborting:
		tclout << "xct_aborting" << ends;
		break;
	case ss_m::xct_chaining:
		tclout << "xct_chaining" << ends;
		break;
	case ss_m::xct_committing: 
		tclout << "xct_committing" << ends;
		break;
	case ss_m::xct_freeing_space: 
		tclout << "xct_freeing_space" << ends;
		break;
	case ss_m::xct_ended:
		tclout << "xct_ended" << ends;
		break;
    };
    Tcl_AppendResult(ip, tclout.str(), 0);
    return TCL_OK;
}

static int
t_tid_to_xct(Tcl_Interp* ip, int ac, char* av[])
{
    if (check(ip, "xct", ac, 2))
	return TCL_ERROR;

	tid_t t;
	istrstream anon(VCPP_BUG_1  av[1], strlen(av[1])); anon >> t;
    
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
t_dump_xcts(Tcl_Interp* ip, int ac, char*[])
{
    if (check(ip, "", ac, 1))
	return TCL_ERROR;
    
    DO( sm->dump_xcts(cout) );
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
    istrstream anon(VCPP_BUG_1 av[1], strlen(av[1])); anon >> lvid;
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
t_mem_stats(Tcl_Interp* ip, int ac, char* av[])
{
    if (check(ip, "[reset]", ac, 1,2))
	return TCL_ERROR;

    bool reset = false;
    if (ac == 2) {
	if (strcmp(av[1], "reset") == 0) {
	    reset = true;
	}
    }

    tclout.seekp(ios::beg);
    {
	size_t amt=0, num=0, hiwat;
	w_shore_alloc_stats(amt, num, hiwat);
	tclout << amt << " bytes allocated in main heap in " 
		<< num
		<< " calls, high water mark= " << hiwat
		<< ends;
    }
    Tcl_AppendResult(ip, tclout.str(), 0);
    return TCL_OK;
}

static int
t_gather_xct_stats(Tcl_Interp* ip, int ac, char* av[])
{
    if (check(ip, "[reset]", ac, 1,2))
	return TCL_ERROR;

    bool reset = false;
    if (ac == 2) {
	if (strcmp(av[1], "reset") == 0) {
	    reset = true;
	}
    }
    {
	sm_stats_info_t* stats = new sm_stats_info_t;
	w_auto_delete_t<sm_stats_info_t> 	autodel(stats);

	sm_stats_info_t& internal = *stats; 
	// internal gets initalized by gather_xct_stats
	DO( sm->gather_xct_stats(internal, reset));

	tclout.seekp(ios::beg);
	tclout << internal << ends;
    }
    Tcl_AppendResult(ip, tclout.str(), 0);
    return TCL_OK;
}

static int
t_gather_stats(Tcl_Interp* ip, int ac, char* av[])
{
    if (check(ip, "[reset]", ac, 1,2))
	return TCL_ERROR;

    bool reset = false;
    if (ac == 2) {
	if (strcmp(av[1], "reset") == 0) {
	    reset = true;
	}
    }

    {
	sm_stats_info_t* stats = new sm_stats_info_t;
	w_auto_delete_t<sm_stats_info_t> 	autodel(stats);

	sm_stats_info_t& internal = *stats; 
	// internal gets initalized by gather_stats
	DO( sm->gather_stats(*stats, reset));

	tclout.seekp(ios::beg);
	tclout << internal << ends;
    }
    Tcl_AppendResult(ip, tclout.str(), 0);
    return TCL_OK;
}

sm_config_info_t global_sm_config_info;

static int
t_config_info(Tcl_Interp* ip, int ac, char*[])
{
    if (check(ip, "", ac, 1))
	return TCL_ERROR;


    DO( sm->config_info(global_sm_config_info));

    tclout.seekp(ios::beg);
    tclout << global_sm_config_info << ends;
    Tcl_AppendResult(ip, tclout.str(), 0);
    return TCL_OK;
}

static int
t_set_disk_delay(Tcl_Interp* ip, int ac, char* av[])
{
    if (check(ip, "milli_sec", ac, 2))
	return TCL_ERROR;

    uint4_t msec = atoi(av[1]);
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

	istrstream anon(VCPP_BUG_1 av[1], strlen(av[1])); anon >> iid.lvid;
	DO( sm->vol_root_index(iid.lvid, iid.serial));
	tclout.seekp(ios::beg);
	tclout << iid << ends;

    } else {
	vid_t   vid;
	stid_t iid;

	istrstream anon(VCPP_BUG_1 av[1], strlen(av[1])); anon >> vid;
	DO( sm->vol_root_index(vid, iid));
	tclout.seekp(ios::beg);
	tclout << iid << ends;

    }
    Tcl_AppendResult(ip, tclout.str(), 0);
    return TCL_OK;
}

static int
t_get_volume_meta_stats(Tcl_Interp* ip, int ac, char* av[])
{
    if (check(ip, "vid [t_cc_none|t_cc_volume]", ac, 2, 3))  {
	return TCL_ERROR;
    }

    if (use_logical_id(ip))  {
	Tcl_AppendResult(ip, "logical id not supported", 0);
	return TCL_ERROR;
    }

    vid_t vid;
    istrstream anon(av[1], strlen(av[1])); anon >> vid;

    ss_m::concurrency_t cc = ss_m::t_cc_none;
    if (ac > 2)  {
	cc = cvt2concurrency_t(av[2]);
    }

    SmVolumeMetaStats volumeStats;
    DO( sm->get_volume_meta_stats(vid, volumeStats, cc) );

    tclout.seekp(ios::beg);
    tclout 
	<< "pages " << volumeStats.numPages << " "
	<< "sys_pages " << volumeStats.numSystemPages << " "
	<< "resv_pages " << volumeStats.numReservedPages << " "
	<< "alloc_pages " << volumeStats.numAllocPages << " "
	<< "stores " << volumeStats.numStores << " "
	<< "alloc_stores " << volumeStats.numAllocStores << ends;
    Tcl_AppendResult(ip, tclout.str(), 0);

    return TCL_OK;
}


#ifdef EXPLICIT_TEMPLATE
template class w_auto_delete_t<SmFileMetaStats>;
template class w_auto_delete_t<sm_stats_info_t>;
#endif

static int
t_get_file_meta_stats(Tcl_Interp* ip, int ac, char* av[])
{
    const char errString[] = "vid storeNumList [batch|dont_batch] [t_cc_none|t_cc_file]";

    if (check(ip, errString, ac, 3, 4, 5))  {
	return TCL_ERROR;
    }

    if (use_logical_id(ip))  {
	Tcl_AppendResult(ip, "logical id not supported", 0);
	return TCL_ERROR;
    }

    vid_t vid;
    istrstream anon(av[1], strlen(av[1])); anon >> vid;

    int numFiles;
    char** listElements;
    if (Tcl_SplitList(ip, av[2], &numFiles, &listElements) != TCL_OK)  {
	return TCL_ERROR;
    }

    SmFileMetaStats* fileStats = new SmFileMetaStats[numFiles];
    w_auto_delete_t<SmFileMetaStats> deleteOnExit(fileStats);

    for (int i = 0; i < numFiles; ++i)  {
	int snum;
	if (Tcl_GetInt(ip, listElements[i], &snum) != TCL_OK)  {
	    free(listElements);
	    return TCL_ERROR;
	}
	fileStats[i].smallSnum = (snum_t)snum;
    }
    free(listElements);

    bool batch = false;
    if (ac > 3)  {
	if (!strcmp(av[3], "batch"))  {
	    batch = true;
	}  else if (!strcmp(av[3], "dont_batch"))  {
	    Tcl_AppendResult(ip, errString, 0);
	    return TCL_ERROR;
	}
    }

    ss_m::concurrency_t cc = ss_m::t_cc_none;
    if (ac > 4)  {
	cc = cvt2concurrency_t(av[4]);
    }

    DO( sm->get_file_meta_stats(vid, numFiles, fileStats, batch, cc) );

    for (int j = 0; j < numFiles; ++j)  {
	tclout.seekp(ios::beg);
	tclout << fileStats[j].smallSnum << " "
			<< fileStats[j].largeSnum << " {"
			<< fileStats[j].small.numReservedPages << " "
			<< fileStats[j].small.numAllocPages << "} {"
			<< fileStats[j].large.numReservedPages << " "
			<< fileStats[j].large.numAllocPages << "}"
			<< ends;

	Tcl_AppendElement(ip, TCL_CVBUG tclout.str());
    }

    return TCL_OK;
}

static int
t_dump_buffers(Tcl_Interp* ip, int ac, char*[])
{
    if (check(ip, "", ac, 1)) return TCL_ERROR;
    DO(sm->dump_buffers(cout));
    return TCL_OK;
}

static int
t_get_volume_quota(Tcl_Interp* ip, int ac, char* av[])
{
    if (check(ip, "vid", ac, 2))  return TCL_ERROR;
    smlevel_0::smksize_t capacity, used;
    lvid_t lvid;
    istrstream anon(VCPP_BUG_1 av[1], strlen(av[1])); anon >> lvid;
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
    smlevel_0::smksize_t capacity, used;
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
    bool force = false;
    if (av[3] && streq(av[3], "force")) force = true;
    DO( sm->format_dev(av[1], objectsize(av[2]),force));
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
	Tcl_AppendElement(ip, TCL_CVBUG tclout.str());
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
	Tcl_AppendElement(ip, TCL_CVBUG tclout.str());
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
    istrstream anon(VCPP_BUG_1 av[2], strlen(av[2])); anon >> lvid;
    smsize_t quota = objectsize(av[3]);

    bool skip_raw_init = false;
    if (av[4] && streq(av[4], "skip_raw_init")) skip_raw_init = true;

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
    istrstream anon(VCPP_BUG_1 av[1], strlen(av[1])); anon >> lvid;
    DO( sm->destroy_vol(lvid));
    return TCL_OK;
}

static int
t_add_logical_id_index(Tcl_Interp* ip, int ac, char* av[])
{
    if (check(ip, "lvid reserved", ac, 3)) return TCL_ERROR;
   
    lvid_t lvid;
    istrstream anon(VCPP_BUG_1 av[1], strlen(av[1])); anon >> lvid;
    uint4_t reserved = atoi(av[2]);

    DO(sm->add_logical_id_index(lvid, reserved, reserved));
    return TCL_OK;
}

static int
t_has_logical_id_index(Tcl_Interp* ip, int ac, char* av[])
{
    if (check(ip, "lvid", ac, 2)) return TCL_ERROR;
   
    lvid_t lvid;
    istrstream anon(VCPP_BUG_1 av[1], strlen(av[1])); anon >> lvid;
    
    bool has_index;
    DO(sm->has_logical_id_index(lvid, has_index));
    tcl_append_boolean(ip, has_index);

    return TCL_OK;
}

static int
t_get_du_statistics(Tcl_Interp* ip, int ac, char* av[])
{
    // borthakur : June, 1994
    if (check(ip, "vid|stid [pretty] [noaudit]", ac, 2, 3, 4))  return TCL_ERROR;
    sm_du_stats_t stats;

    stats.clear();

    bool pretty = false;
    bool audit = true;
    if (ac == 4) {
	if (strcmp(av[3], "noaudit") == 0) {
	    audit = false;
	}
	if (strcmp(av[3], "pretty") == 0) {
	    pretty = true;
	}
    }
    if (ac >= 3) {
	if (strcmp(av[2], "pretty") == 0) {
	    pretty = true;
	}
	if (strcmp(av[2], "noaudit") == 0) {
	    audit = false;
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
    if (pretty) stats.print(cout, "du:");

    tclout.seekp(ios::beg);
    tclout <<  stats << ends; 
    Tcl_AppendResult(ip, tclout.str(), 0);

    return TCL_OK;
}

static int
t_preemption_simulated(Tcl_Interp* ip, int ac, char* [])
{
    if (check(ip, "", ac, 1)) return TCL_ERROR;
  
#if defined(USE_SSMTEST)
    Tcl_AppendResult(ip, (const char *)(preemption_simulated()? "1" : "0"), 0);
    return TCL_OK;

#else /* USE_SSMTEST */

    Tcl_AppendResult(ip, "0" , 0);
    return TCL_OK;
#endif /* USE_SSMTEST */
}

#ifndef PURIFY
#define av /*av not used*/
#endif
static int
t_purify_print_string(Tcl_Interp* ip, int ac, char* av[])
#undef av
{
    if (check(ip, "string", ac, 2))  {
	return TCL_ERROR;
    }

#ifdef PURIFY
    purify_printf("%s\n", av[1]);
#endif

    return TCL_OK;
}

#ifndef USE_SSMTEST
#define av
#endif

static int
t_simulate_preemption(Tcl_Interp* ip, int ac, char* av[])
#ifndef USE_SSMTEST
#undef av
#endif
{
    if (check(ip, "on|off|1|0", ac, 2)) return TCL_ERROR;
  
#ifdef USE_SSMTEST

    if( (strcmp(av[1],"on")==0)|| (strcmp(av[1],"1")==0) ||
	(strcmp(av[1],"yes")==0)) {
	simulate_preemption(true);
    } else
    if( (strcmp(av[1],"off")==0)|| (strcmp(av[1],"0")==0) ||
	(strcmp(av[1],"no")==0)) {
	simulate_preemption(false);
    } else {
	Tcl_AppendResult(ip, "usage: simulate_preemption on|off", 0);
	return TCL_ERROR;
    }
    return TCL_OK;

#else /* USE_SSMTEST */
    cerr << "Warning: simulate_preemption (USE_SSMTEST) not configured" << endl;
    Tcl_AppendResult(ip, "simulate_preemption (USE_SSMTEST) not configured", 0);
    return TCL_OK;
#endif /* USE_SSMTEST */

}
static int
t_set_debug(Tcl_Interp* ip, int ac, char** W_IFTRACE(av))
{
    if (check(ip, "[flag_string]", ac, 1, 2)) return TCL_ERROR;
  
#ifdef W_TRACE
    //
    // If "" is used, debug prints are turned off
    // Always returns the previous setting
    //
    Tcl_AppendResult(ip,  _debug.flags(), 0);
    if(ac>1) {
	 _debug.setflags(av[1]);
    }
#endif
    return TCL_OK;
}

static int
t_set_store_property(Tcl_Interp* ip, int ac, char* av[])
{
    if (check(ip, "fid property", ac, 3))  
	return TCL_ERROR;

    ss_m::store_property_t property = cvt2store_property(av[2]);

    if (use_logical_id(ip))  {
	lstid_t fid;
	istrstream anon(VCPP_BUG_1 av[1], strlen(av[1])); anon >> fid;
	DO( sm->set_store_property(fid.lvid, fid.serial, property) );
    } else {
	stid_t fid;
	istrstream anon(VCPP_BUG_1 av[1], strlen(av[1])); anon >> fid;
	DO( sm->set_store_property(fid, property) );
    }

    return TCL_OK;
}

static int
t_get_store_property(Tcl_Interp* ip, int ac, char* av[])
{
    if (check(ip, "fid", ac, 2))  
	return TCL_ERROR;

    ss_m::store_property_t property;

    if (use_logical_id(ip))  {
	lstid_t fid;
	istrstream anon(VCPP_BUG_1 av[1], strlen(av[1])); anon >> fid;
	DO( sm->get_store_property(fid.lvid, fid.serial, property) );
    } else {
	stid_t fid;
	istrstream anon(VCPP_BUG_1 av[1], strlen(av[1])); anon >> fid;
	DO( sm->get_store_property(fid, property) );
    }

    tclout.seekp(ios::beg);
    tclout << property << ends;

    Tcl_AppendResult(ip, tclout.str(), 0);
    return TCL_OK;
}

static int
t_set_lock_level(Tcl_Interp* ip, int ac, char* av[])
{
    if (check(ip, "file|page|record", ac, 2))  
	return TCL_ERROR;
    ss_m::concurrency_t cc = cvt2concurrency_t(av[1]);
    sm->set_xct_lock_level(cc);
    return TCL_OK;
}


static int
t_get_lock_level(Tcl_Interp* ip, int ac, char* [])
{
    if (check(ip, "", ac, 1))  
	return TCL_ERROR;
    ss_m::concurrency_t cc = ss_m::t_cc_bad;
    cc = sm->xct_lock_level();

    tclout.seekp(ios::beg);
    tclout << cvt_concurrency_t(cc) << ends;
    Tcl_AppendResult(ip, tclout.str(), 0);
    return TCL_OK;
}

static int
t_create_index(Tcl_Interp* ip, int ac, char* av[])
{
     //            1   2       3                                        4           5
    if (check(ip, "vid ndxtype [\"tmp|regular|load_file|insert_file\"] [keydescr] [t_cc_none|t_cc_kvl|t_cc_modkvl|t_cc_im] [small]", ac, 3, 4, 5,  6, 7))
	return TCL_ERROR;

    const char *keydescr="b*1000";

    bool small = false;
    ss_m::store_property_t property = ss_m::t_regular;
    if (ac > 3) {
	property = cvt2store_property(av[3]);
    }
    if (ac > 4) {
	if(cvt2type((keydescr = av[4])) == test_nosuch) {
	    tclout.seekp(ios::beg);
	    // Fake it
	    tclout << "E_WRONGKEYTYPE" << ends;
	    Tcl_AppendResult(ip, tclout.str(), 0);
	    return TCL_ERROR;
	    /*
	     * The reason that we allow this to go on is 
	     * that we have some tests that explicitly use
	     * combined key types, e.g. i2i2, and the
	     * tests merely check the conversion to and from
	     * the key type strings
	     */
	}
    }
    ss_m::concurrency_t cc = ss_m::t_cc_kvl;
    if (ac > 5) {
	cc = cvt2concurrency_t(av[5]);
	if(cc == ss_m::t_cc_bad) {
	    if(::strcmp(av[5],"small")==0) {
		cc = ss_m::t_cc_none;
		small = true;
		if (! use_logical_id(ip)) {
		   cerr 
		   << "ssh shell.cpp: not using logical ids; 6th argument ignored"
		<< endl;
		}
	    }
	}
    }
    if (ac > 6) {
	if(::strcmp(av[6],"small")!=0) {
	    Tcl_AppendResult(ip, 
	       "create_index: 5th argument must be \"small\" or empty", 0);
	    return TCL_ERROR;
	}
	small = true;
	if (! use_logical_id(ip)) {
	   cerr << "ssh shell.cpp: not using logical ids; 6th argument ignored"
		<< endl;
	}
    }
    const char *kd = check_compress_flag(keydescr);
    ss_m::ndx_t ndx = cvt2ndx_t(av[2]);

    if (use_logical_id(ip)) {
	lstid_t lstid;
	istrstream anon(VCPP_BUG_1 av[1], strlen(av[1])); 
	anon >> lstid.lvid;
	DO( sm->create_index(lstid.lvid, ndx,
			     property, kd, cc,
			     small?1:10, 
			     lstid.serial) );
	tclout.seekp(ios::beg);
	tclout << lstid << ends;
    } else {
	stid_t stid;
        vid_t  volid = vid_t(atoi(av[1]));
	DO( sm->create_index(
			volid,
			     ndx, property, kd, cc,
			     stid));
	tclout.seekp(ios::beg);
	tclout << stid << ends;
    }

    Tcl_AppendResult(ip, tclout.str(), 0);
    return TCL_OK;
}

static int
t_destroy_md_index(Tcl_Interp* ip, int ac, char* av[])
{
    if (check(ip, "fid", ac, 2))
        return TCL_ERROR;
    
    if (use_logical_id(ip)) {
	lstid_t fid;
	istrstream anon(VCPP_BUG_1 av[1], strlen(av[1])); anon >> fid;
	DO( sm->destroy_md_index(fid.lvid, fid.serial) );
    } else {
	stid_t fid;
	istrstream anon(VCPP_BUG_1 av[1], strlen(av[1])); anon >> fid;
	DO( sm->destroy_md_index(fid) );
    }

    return TCL_OK;
}
static int
t_destroy_index(Tcl_Interp* ip, int ac, char* av[])
{
    if (check(ip, "fid", ac, 2))
        return TCL_ERROR;
    
    if (use_logical_id(ip)) {
	lstid_t fid;
	istrstream anon(VCPP_BUG_1 av[1], strlen(av[1])); anon >> fid;
	DO( sm->destroy_index(fid.lvid, fid.serial) );
    } else {
	stid_t fid;
	istrstream anon(VCPP_BUG_1 av[1], strlen(av[1])); anon >> fid;
	DO( sm->destroy_index(fid) );
    }

    return TCL_OK;
}

//
// prepare for bulk load:
//  read the input file and put recs into a sort stream
//
static rc_t 
prepare_for_blkld(sort_stream_i& s_stream, 
	Tcl_Interp* ip, char* fid,
	const char* type, 
	const char* universe=NULL
)
{
    key_info_t info;
    sort_parm_t sp;

    info.where = key_info_t::t_hdr;

    typed_btree_test t = cvt2type(type);

    switch(t) {
    case  test_bv:
        //info.type = key_info_t::t_string;
        info.type = sortorder::kt_b;
    	info.len = 0;
	break;

    case  test_b1:
        // info.type = key_info_t::t_char;
        info.type = sortorder::kt_u1;
    	info.len = sizeof(char);
	break;

    case test_i1:
	// new
        info.type = sortorder::kt_i1;
    	info.len = sizeof(uint1_t);
	break;

    case test_i2:
	// new
        info.type = sortorder::kt_i2;
    	info.len = sizeof(uint2_t);
	break;

    case test_i4:
        // info.type = key_info_t::t_int;
        info.type = sortorder::kt_i4;
    	info.len = sizeof(uint4_t);
	break;

    case test_i8:
	// NB: Tcl doesn't know about 64-bit arithmetic
        // info.type = key_info_t::t_int;
        info.type = sortorder::kt_i8;
    	info.len = sizeof(w_base_t::int8_t);
	break;

    case test_u1:
	// new
        info.type = sortorder::kt_u1;
    	info.len = sizeof(uint1_t);
	break;

    case test_u2:
	// new
        info.type = sortorder::kt_u2;
    	info.len = sizeof(uint2_t);
	break;

    case test_u4:
        // new
        info.type = sortorder::kt_u4;
    	info.len = sizeof(uint4_t);
	break;

    case test_u8:
	// NB: Tcl doesn't handle 64-bit arithmetic
        // new
        info.type = sortorder::kt_u8;
    	info.len = sizeof(w_base_t::uint8_t);
	break;

    case test_f4:
        // info.type = key_info_t::t_float;
        info.type = sortorder::kt_f4;
    	info.len = sizeof(f4_t);
	break;

    case test_f8:
        // new test
        info.type = sortorder::kt_f8;
    	info.len = sizeof(f8_t);
	break;

    case test_spatial:
	{
	    // info.type = key_info_t::t_spatial;
	    info.type = sortorder::kt_spatial;
	    if (universe==NULL) {
		if (check(ip, "stid src type [universe]", 1, 0))
		    return RC(fcINTERNAL);
	    }
	    nbox_t univ(universe);
	    info.universe = univ;
	    info.len = univ.klen();
	}
	break;

    default:
	W_FATAL(fcNOTIMPLEMENTED);
	break;
    }
    
    sp.run_size = 3;
    sp.destructive = true;
    scan_file_i* f_scan = 0;

    if (use_logical_id(ip)) {
	lstid_t l_stid;
	istrstream anon(VCPP_BUG_1 fid, strlen(fid)); anon >> l_stid;
	vid_t tmp_vid;
	W_DO( sm->lvid_to_vid(l_stid.lvid, tmp_vid));
        sp.vol = tmp_vid;
	f_scan = new scan_file_i(l_stid.lvid, l_stid.serial, ss_m::t_cc_file);
    } else {
	stid_t stid;
	istrstream anon(VCPP_BUG_1 fid, strlen(fid)); anon >> stid;
        sp.vol = stid.vol;
	f_scan = new scan_file_i(stid, ss_m::t_cc_file);
    }

    HANDLE_FSCAN_FAILURE(f_scan);

    /*
     * scan the input file, put into a sort stream
     */
    s_stream.init(info, sp);
    pin_i* pin;
    bool eof = false;
    while (!eof) {
	W_DO(f_scan->next(pin, 0, eof));
	if (!eof && pin->pinned()) {
	    vec_t key(pin->hdr(), pin->hdr_size()),
		  el(pin->body(), pin->body_size());
		
if(t == test_spatial) {
	nbox_t k;
	k.bytes2box((char *)pin->hdr(), pin->hdr_size());

	DBG(<<"hdr size is " << pin->hdr_size());
	int dim = pin->hdr_size() / (2 * sizeof(int));
	const char *cp = pin->hdr();
	int tmp;
	for(int j=0; j<dim; j++) {
	    memcpy(&tmp, cp, sizeof(int));
	    cp += sizeof(int);
	    DBG(<<"int[" << j << "]= " << tmp);
	}
	DBG(<<"key is " << k);
	DBG(<<"body is " << (const char *)(pin->body()));
}
	    W_DO(s_stream.put(key, el));
	}
    }

    delete f_scan;
    
    return RCOK;
}

static int
t_blkld_ndx(Tcl_Interp* ip, int ac, char* av[])
{
    // arguments: 0         1    2     3
    // arguments: blkld_ndx stid nsrcs srcs [src*] [type [universe]]
    int first_src = 3;
    int stid_arg = 1;
    if (ac < 4) {
        Tcl_AppendResult(ip, "wrong # args; should be \"", 
			"stid nsrcs src [src*] [type [universe]]"
		         "\"", 0);
	return TCL_ERROR;
    }
    /* set up array of srcs */
    int nsrcs = atoi(av[2]);
    if(nsrcs < 1 || nsrcs > 10) {
        Tcl_AppendResult(ip, "Expected 1 ... 10 (arbitrary) sources", 0);
	return TCL_ERROR;
    }
    /* nsrcs == ac -1(blkld_ndx) 
		 -1(stid) -1(nsrcs) possibly -2 for type and universe */
    if(nsrcs < ac-5) {
        Tcl_AppendResult(ip, "Too many arguments provided for "
	   "number of sources given (2nd argument)", 0);
	return TCL_ERROR;
    }
    if(nsrcs > ac-3) {
        Tcl_AppendResult(ip, "Not enough sources arguments provided for "
	   "number of sources given (2nd argument)", 0);
	return TCL_ERROR;
    }
    int	excess_arguments = ac - 3 - nsrcs;
    const char *typearg = 0, *universearg = 0;
    if(excess_arguments == 2) {
	typearg = av[ac-2];
	universearg = av[ac-1];
    } else if(excess_arguments == 1) {
	typearg = av[ac-1];
    } else if(excess_arguments != 0) {
	w_assert1(0);
    }

    sm_du_stats_t stats;

    if (excess_arguments) {
	// using sorted stream
    	sort_stream_i s_stream;

	if(nsrcs!=1) {
	    Tcl_AppendResult(ip, "Bulk load with sort_stream "
	       " takes only one source file.", 0);
	}
    	if (excess_arguments==1) {
            DO( prepare_for_blkld(s_stream, ip, av[first_src], typearg) );
    	} else {
            DO( prepare_for_blkld(s_stream, ip, av[first_src], typearg, universearg) );
	}

	if (use_logical_id(ip)) {
	    lstid_t l_stid;
	    istrstream anon(VCPP_BUG_1 av[stid_arg], strlen(av[stid_arg])); 
	    anon >> l_stid;
	    DO( sm->bulkld_index(l_stid.lvid, l_stid.serial,
				s_stream,  stats) );
	} else {
	    stid_t stid;
	    istrstream anon(VCPP_BUG_1 av[stid_arg], strlen(av[stid_arg])); 
	    anon >> stid;
	    DO( sm->bulkld_index(stid, s_stream, stats) ); 
	}
    } else {
	// input file[s] already sorted
	if (use_logical_id(ip)) {
	    lstid_t l_stid;
	    istrstream anon(VCPP_BUG_1 av[stid_arg], strlen(av[stid_arg])); 
	    anon >> l_stid;

	    lstid_t* l_src = new lstid_t[nsrcs];
	    lvid_t* vids = new lvid_t[nsrcs];
	    serial_t*	serials= new serial_t[nsrcs];
	    for(int i=0; i<nsrcs; i++) {
		istrstream anon2(VCPP_BUG_1 av[first_src+i], 
			strlen(av[first_src+i])); 
		anon2  >> l_src[i];
		vids[i] = l_src[i].lvid;
		serials[i] = l_src[i].serial;
	    }
	    w_rc_t rc = sm->bulkld_index(l_stid.lvid, l_stid.serial,
				 nsrcs, vids, serials,
				 stats);
	    delete[] l_src;
	    delete[] vids;
	    delete[] serials;
	    DO(rc);

	} else {
	    stid_t stid;
	    stid_t* srcs = new stid_t[nsrcs];
	    istrstream anon(VCPP_BUG_1 av[stid_arg], strlen(av[stid_arg])); 
	    anon >> stid;
	    for(int i=0; i<nsrcs; i++) {
	       istrstream anon2(VCPP_BUG_1 av[first_src+i], 
			strlen(av[first_src+i])); 
	       anon2 >> srcs[i];
	    }
	    w_rc_t rc = sm->bulkld_index(stid, nsrcs, srcs, stats); 
	    delete[] srcs;
	    DO(rc);
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

    const char* type = "spatial";

    DO( prepare_for_blkld(s_stream, ip, av[2], type, av[5]) );

    if (use_logical_id(ip)) {
	lstid_t l_stid;
	istrstream anon(VCPP_BUG_1 av[1], strlen(av[1])); anon >> l_stid;
	DO( sm->bulkld_md_index(l_stid.lvid, l_stid.serial,
				  s_stream, stats,
				  // hff,      hef,         universe
				  atoi(av[3]), atoi(av[4]), &univ) );
    } else {
	stid_t stid;
	istrstream anon(VCPP_BUG_1 av[1], strlen(av[1])); anon >> stid;
	DO( sm->bulkld_md_index(stid, s_stream, stats,
				  // hff,      hef,         universe
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
	istrstream anon(VCPP_BUG_1 av[1], strlen(av[1])); anon >> stid;
	DO( sm->print_index(stid.lvid, stid.serial) );
    } else {
	stid_t stid;
	istrstream anon(VCPP_BUG_1 av[1], strlen(av[1])); anon >> stid;
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
	istrstream anon(VCPP_BUG_1 av[1], strlen(av[1])); anon >> stid;
	DO( sm->print_md_index(stid.lvid, stid.serial) );
    } else {
	stid_t stid;
	istrstream anon(VCPP_BUG_1 av[1], strlen(av[1])); anon >> stid;
	DO( sm->print_md_index(stid) );
    }

    return TCL_OK;
}

#include <crash.h>


static int
t_crash(Tcl_Interp* ip, int ac, char ** W_IFTRACE(av))
{

    if (check(ip, "str cmd", ac, 3))
	return TCL_ERROR;

    DBG(<<"crash " << av[1] << " " << av[2] );

    cout << flush;
    _exit(1);
    // no need --
    return TCL_OK;
}

/*
 * callback function for running out of log space
 */
class xct_i; class xct_t;
extern w_rc_t out_of_log_space (xct_i*, xct_t *&,
	smlevel_0::fileoff_t, smlevel_0::fileoff_t);

static int
t_restart(Tcl_Interp* ip, int ac, char* av[])
{
    if (check(ip, "clean:bool", ac, 1, 2))
	return TCL_ERROR;

    if (start_client_support)  {
	DO(stop_comm());  // stop ssh connection listen thread
#ifdef USE_COORD
	(void) t_co_retire(0,0,0);
#endif
    }
    bool clean = false;
    if(ac==2) {
	    /* XXX error ignored for now */
	    tcl_scan_boolean(av[1], clean);
    }
    sm->set_shutdown_flag(clean);
    delete sm;

    /* 
     * callback function for out-of-log-space
     */

    // Try without callback function.
    if(log_warn_callback) {
       sm = new ss_m(out_of_log_space);
    } else {
       sm = new ss_m();
    }
    w_assert1(sm);
    
    if (start_client_support) {
	DO(start_comm());  // restart connection listen thread
    }

    return TCL_OK;
}

#ifdef USE_COORD

static int
t_coord(Tcl_Interp* ip, int ac, char* av[])
{
    if (check(ip, "vid", ac, 2))
	return TCL_ERROR;

    lvid_t lvid;
    istrstream anon(VCPP_BUG_1 av[1], strlen(av[1])); anon >> lvid;

    co = new sm_coordinator(lvid, ip);
    w_assert1(co);
    return TCL_OK;
}
#endif

static int
t_create_file(Tcl_Interp* ip, int ac, char* av[])
{
    if (check(ip, "vid  [\"tmp|regular|load_file|insert_file\"] [cluster-page]",
	      ac, 2, 3, 4)) 
	return TCL_ERROR;
    
    ss_m::store_property_t property = ss_m::t_regular;
  
    shpid_t	cluster_page=0;
    if (ac > 3) {
       cluster_page = atoi(av[3]);
    }
    if (ac > 2) {
	property = cvt2store_property(av[2]);
    }

    if (use_logical_id(ip)) {
	lfid_t lfid;
	istrstream anon(VCPP_BUG_1 av[1], strlen(av[1])) ; anon >> lfid.lvid;
	DO( sm->create_file(lfid.lvid, lfid.serial, property, cluster_page) );
	tclout.seekp(ios::beg);
	tclout << lfid << ends;
    } else {
	stid_t stid;
        int volumeid = atoi(av[1]);
	DO( sm->create_file(vid_t(volumeid), stid, property, serial_t::null,
                cluster_page) );

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
	istrstream anon(VCPP_BUG_1 av[1], strlen(av[1])); anon >> fid;
	DO( sm->destroy_file(fid.lvid, fid.serial) );
    } else {
	stid_t fid;
	istrstream anon(VCPP_BUG_1 av[1], strlen(av[1])); anon >> fid;
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
    w_base_t::int8_t bl1, bl2; // used if keytype is 64 bits

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

    if (convert && (ac > 7)) {
	const char *keydescr = (const char *)av[7];

	enum typed_btree_test t = cvt2type(keydescr);
	if (t == test_nosuch) {
	    Tcl_AppendResult(ip, 
	       "create_scan: 7th argument must start with [bBiIuUfF]", 0);
	    return TCL_ERROR;
		
	}
	switch(t) {
	    case test_i8:
	    case test_u8: {
		    /* XXX this breaks with error checking strtoq,
		       need to use strtoq, strtouq.  Or add
		       w_base_t::ato8() that mimics atoi()
		       semantics. */
		    bl1 = w_base_t::strtoi8(av[3]); //bound1 
		    t1.reset();
		    t1.put(&bl1, sizeof(w_base_t::int8_t));
		    // DBG(<<"bound1 = " << bl1);

		    bl2 = w_base_t::strtoi8(av[5]); //bound2
		    t2.reset();
		    t2.put(&bl2, sizeof(w_base_t::int8_t));
		    // DBG(<<"bound2 = " << bl2);
		}
		break;
	    case test_i4:
	    case test_u4: {
		    b1 = atoi(av[3]); //bound1 
		    t1.reset();
		    t1.put(&b1, sizeof(int));
		    DBG(<<"bound1 = " << b1);

		    b2 = atoi(av[5]); // bound2
		    t2.reset();
		    t2.put(&b2, sizeof(int));
		    DBG(<<"bound2 = " << b2);
		}
		break;
	    case test_bv:
	    case test_b1:
	    case test_b23:
		break;
	    default:
		Tcl_AppendResult(ip, 
	       "ssh.create_scan unsupported for given keytype:  ",
		   keydescr, 0);
	    return TCL_ERROR;
	}
    }

    ss_m::concurrency_t cc = ss_m::t_cc_kvl;
    if (ac == 7) {
	cc = cvt2concurrency_t(av[6]);
    }
    DBG(<<"c1 = " << int(c1));
    DBG(<<"c2 = " << int(c2));
    DBG(<<"cc = " << int(cc));

    scan_index_i* s;
    if (use_logical_id(ip)) {
	lstid_t stid;
	istrstream anon(VCPP_BUG_1 av[1], strlen(av[1])); anon >> stid;
	s = new scan_index_i(stid.lvid, stid.serial, 
			     c1, *bound1, c2, *bound2, false, cc);
    } else {
	stid_t stid;
	istrstream anon(VCPP_BUG_1 av[1], strlen(av[1])); anon >> stid;
	s = new scan_index_i(stid, c1, *bound1, c2, *bound2, false, cc);
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

    typed_btree_test  t = test_bv; // default
    if (ac > 2) {
	t = cvt2type(av[2]);
    } 
    
    scan_index_i* s = (scan_index_i*) strtol(av[1], 0, 0);
    smsize_t klen, elen;
    klen = elen = 0;
    bool eof;
    DO( s->next(eof) );
    if (eof) {
	Tcl_AppendElement(ip, TCL_CVBUG "eof");
    } else {

	DO( s->curr(0, klen, 0, elen) );
	w_assert1(klen + elen + 2 <= ss_m::page_sz);

	vec_t key(outbuf, klen);
	vec_t el(outbuf + klen + 1, elen);
	DO( s->curr(&key, klen, &el, elen) );

	typed_value v;

	// For the time being, we assume that value/elem is string
	outbuf[klen] = outbuf[klen + 1 + elen] = '\0';

	bool done = false;
	switch(t) {
	case test_bv:
	    outbuf[klen] = outbuf[klen + 1 + elen] = '\0';
	    Tcl_AppendResult(ip, outbuf, " ", outbuf + klen + 1, 0);
	    done = true;
	    break;
	case test_b1:
	    char c[2];
	    c[0] = outbuf[0];
	    c[1] = 0;
	    w_assert3(klen == 1);
	    done = true;
	    Tcl_AppendResult(ip, c, " ", outbuf + klen + 1, 0);
	    break;

	case test_i1:
	case test_u1:
	case test_i2:
	case test_u2:
	case test_i4:
	case test_u4:
	case test_i8:
	case test_u8:
	case test_f4:
	case test_f8:
	    memcpy(&v._u, outbuf, klen);
	    v._length = klen;
	    break;
	case test_spatial:
	default:
	    W_FATAL(fcNOTIMPLEMENTED);
	}
        {
		const int bufsz=100;
		char* tmpbuf = new char[bufsz]; // small
		ostrstream tmp(tmpbuf, sizeof(tmpbuf));
		memset(tmpbuf, '\0', bufsz);

		switch(t) {
		case test_bv:
		case test_b1:
		    break;

		/* Casts force integer instead of character output */
		case test_i1:
		    tmp << (int)v._u.i1_num;
		    break;
		case test_u1:
		    tmp << (unsigned)v._u.u1_num;
		    break;

		case test_i2:
		    tmp << v._u.i2_num;
		    break;
		case test_u2:
		    tmp << v._u.u2_num;
		    break;

		case test_i4:
		    tmp << v._u.i4_num;
		    break;
		case test_u4:
		    tmp << v._u.u4_num;
		    break;

		case test_i8:
		    tmp << v._u.i8_num;
		    break;
		case test_u8:
		    tmp << v._u.u8_num;
		    break;

		case test_f4:
		    tmp << v._u.f4_num;
		    break;
		case test_f8:
		    tmp << v._u.f8_num;
		    break;

		case test_spatial:
		default:
		    W_FATAL(fcNOTIMPLEMENTED);
		}
		if(!done) {
		    tmp << ends;
		    Tcl_AppendResult(ip, tmp.str(), " ", outbuf + klen + 1, 0);
		}
		delete[] tmpbuf;
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
    int count = atoi(av[5]); //count -- can't be > signed#

    if (use_logical_id(ip)) {
        lstid_t  stid;
        lrid_t   rid;

        istrstream anon(VCPP_BUG_1 av[1], strlen(av[1])); anon >> stid;
        for (i=0; i<count; i++)
                DO( sm->create_rec(stid.lvid, stid.serial, hdr,
                           objectsize(av[3]), 
			   data, rid.serial) );
//      rid.lvid = stid.lvid;
//      tclout << rid << ends;

    } else {
        stid_t  stid;
        rid_t   rid;

        istrstream anon(VCPP_BUG_1 av[1], strlen(av[1])); anon >> stid;

	stime_t start(stime_t::now());
        for (i=0; i<count; i++)
                DO( sm->create_rec(stid, hdr, objectsize(av[3]), data, rid) );
        sinterval_t delta(stime_t::now() - start);
        cerr << "t_create_multi_recs: Write clock time = " 
		<< delta << " seconds" << endl;
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

    smsize_t len_hint = objectsize(av[3]);

    vec_t hdr, data;
    hdr.set(parse_vec(av[2], strlen(av[2])));
    data.set(parse_vec(av[4], strlen(av[4])));

    tclout.seekp(ios::beg);

    register int i;
    int count = atoi(av[5]); // count - can't be > signed int

    if (use_logical_id(ip)) {
        lrid_t   lrid;

        for (i=0; i<count; i++)
                DO( s->create_rec(hdr, len_hint,  data, lrid) );
    } else {
        rid_t   rid;
	stime_t start(stime_t::now());
        for (i=0; i<count; i++)
                DO( s->create_rec(hdr, len_hint, data, rid) );
        sinterval_t delta(stime_t::now() - start);
	if (linked.verbose_flag)  {
	    cerr << "t_multi_file_append: Write clock time = " 
		    << delta << " seconds" << endl;
	}
    }
    tclout << "success" << ends;
    Tcl_AppendResult(ip, tclout.str(), 0);
    return TCL_OK;
}

static int
t_multi_file_update(Tcl_Interp* ip, int ac, char* av[])
{
    if (check(ip, "scan objsize ", ac, 3))
        return TCL_ERROR;

    scan_file_i *s = (append_file_i *)strtol(av[1], 0, 0);
    scan_file_i &scan = *s;

    smsize_t osize = objectsize(av[2]);

    tclout.seekp(ios::beg);
    {
	char* d= new char[osize];
	vec_t data(d, osize);
	// random uninit data ok


	w_rc_t rc;
	bool eof=false;
	pin_i*  pin = 0;

	while ( !(rc=scan.next(pin, 0, eof)) && !eof) {
	    w_assert3(pin);
	    DO( pin->update_rec(0, data) );
	}
	delete[] d;
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
	lrid_t   lrid;

	//cout << "using logical ID interface for CREATE_REC" << endl;

	istrstream anon(VCPP_BUG_1 av[1], strlen(av[1])); anon >> stid;
	DO( sm->create_rec(stid.lvid, stid.serial, hdr, 
			   objectsize(av[3]), data, lrid.serial) );
	lrid.lvid = stid.lvid;

	tclout.seekp(ios::beg);
	tclout << lrid << ends;

    } else {
	stid_t  stid;
	rid_t   rid;
	
	istrstream anon(VCPP_BUG_1 av[1], strlen(av[1])); anon >> stid;
	DO( sm->create_rec(stid, hdr, objectsize(av[3]), data, rid) );
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

	istrstream anon(VCPP_BUG_1 av[1], strlen(av[1])); anon >> rid;
	DO( sm->destroy_rec(rid.lvid, rid.serial) );

    } else {
	rid_t   rid;

	istrstream anon(VCPP_BUG_1 av[1], strlen(av[1])); anon >> rid;
	DO( sm->destroy_rec(rid) );
    }

    Tcl_AppendElement(ip, TCL_CVBUG "update success");
    return TCL_OK;
}

static int
t_read_rec_1(Tcl_Interp* ip, int ac, char* av[], bool dump_body_too)
{
    if (check(ip, "rid start length [num_pins]", ac, 4, 5)) 
	return TCL_ERROR;
    
    smsize_t   start  = objectsize(av[2]);
    smsize_t   length = objectsize(av[3]);
    smsize_t   num_pins;
    if (ac == 5) {
	num_pins = numobjects(av[4]);		
    } else {
	num_pins = 1;
    }
    pin_i   handle;

    if (use_logical_id(ip)) {
	lrid_t   rid;
	istrstream anon(VCPP_BUG_1 av[1], strlen(av[1])); anon >> rid;

	for (smsize_t i=1; i<num_pins; i++) {
	    W_IGNORE(handle.pin(rid.lvid, rid.serial, start));
	    handle.unpin();
	}
	DO(handle.pin(rid.lvid, rid.serial, start));
	tclout.seekp(ios::beg);
	tclout << "rid=" << rid << ends;

    } else {
	rid_t   rid;
	istrstream anon(VCPP_BUG_1 av[1], strlen(av[1])); anon >> rid;

	for (smsize_t i=1; i<num_pins; i++) {
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

    tclout.seekp(ios::beg);
    dump_pin_hdr(tclout, handle);
    tclout << " " << ends;
    Tcl_AppendResult(ip, tclout.str(), 0);

    if(dump_body_too) {
	tclout.seekp(ios::beg);
	dump_pin_body(tclout, handle, start, length, ip);
	tclout << " " << ends;
	Tcl_AppendResult(ip, tclout.str(), 0);
    }

    return TCL_OK;
}
static int
t_read_rec(Tcl_Interp* ip, int ac, char* av[])
{
    return t_read_rec_1(ip, ac, av, true);
}
static int
t_read_hdr(Tcl_Interp* ip, int ac, char* av[])
{
    return t_read_rec_1(ip, ac, av, false);
}

static int
t_print_rec(Tcl_Interp* ip, int ac, char* av[])
{
    if (check(ip, "rid start length", ac, 4)) 
	return TCL_ERROR;
    
    smsize_t   start  = objectsize(av[2]);
    smsize_t   length = objectsize(av[3]);
    pin_i   handle;

    if (use_logical_id(ip)) {
	lrid_t   rid;
	istrstream anon(VCPP_BUG_1 av[1], strlen(av[1])); anon >> rid;

	DO(handle.pin(rid.lvid, rid.serial, start));
	if (linked.verbose_flag)  {
	    cout << "rid=" << rid << " "; 
	}
    } else {
	rid_t   rid;
	istrstream anon(VCPP_BUG_1 av[1], strlen(av[1])); anon >> rid;

	DO(handle.pin(rid, start)); 
	if (linked.verbose_flag)  {
	    cout << "rid=" << rid << " "; 
	}
    }

    if (length == 0) {
	length = handle.body_size();
    }

    if (linked.verbose_flag) {
	dump_pin_hdr(cout, handle);
    }
    tclout.seekp(ios::beg);
    dump_pin_body(tclout, handle, start, length, 0);
    if (linked.verbose_flag) {
	tclout << ends;
	cout << tclout.str();
    }

    return TCL_OK;
}

static int
t_read_rec_body(Tcl_Interp* ip, int ac, char* av[])
{
    if (check(ip, "rid [start [length]]", ac, 2, 3, 4)) 
	return TCL_ERROR;

    smsize_t start = (ac >= 3) ? objectsize(av[2]) : 0;
    smsize_t length = (ac >= 4) ? objectsize(av[3]) : 0;
    pin_i handle;

    if (use_logical_id(ip)) {
        lrid_t   rid;
	istrstream anon(VCPP_BUG_1 av[1], strlen(av[1])); anon >> rid;

        DO(handle.pin(rid.lvid, rid.serial, start));
    } else {
        rid_t   rid;
	istrstream anon(VCPP_BUG_1 av[1], strlen(av[1])); anon >> rid;

        DO(handle.pin(rid, start));
    }

    if (length == 0) {
        length = handle.body_size();
    }

    smsize_t offset = (start - handle.start_byte());
    smsize_t start_pos = start;
    smsize_t amount;

    Tcl_ResetResult(ip);

    bool eor = false;  // check end of record
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
    start = objectsize(av[2]);    

    if (use_logical_id(ip)) {
	lrid_t   rid;

	istrstream anon(VCPP_BUG_1 av[1], strlen(av[1])); anon >> rid;
	DO( sm->update_rec(rid.lvid, rid.serial, start, data) );

    } else {
	rid_t   rid;

	istrstream anon(VCPP_BUG_1 av[1], strlen(av[1])); anon >> rid;
	DO( sm->update_rec(rid, start, data) );
    }

    Tcl_AppendElement(ip, TCL_CVBUG "update success");
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
    start = objectsize(av[2]);    

    if (use_logical_id(ip)) {
	lrid_t   rid;

	istrstream anon(VCPP_BUG_1 av[1], strlen(av[1])); anon >> rid;
	DO( sm->update_rec_hdr(rid.lvid, rid.serial, start, hdr) );

    } else {
	rid_t   rid;

	istrstream anon(VCPP_BUG_1 av[1], strlen(av[1])); anon >> rid;
	DO( sm->update_rec_hdr(rid, start, hdr) );
    }

    Tcl_AppendElement(ip, TCL_CVBUG "update hdr success");
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
	istrstream anon(VCPP_BUG_1 av[1], strlen(av[1])); anon >> rid;
	DO( sm->append_rec(rid.lvid, rid.serial, data) );

    } else {
	rid_t   rid;
	istrstream anon(VCPP_BUG_1 av[1], strlen(av[1])); anon >> rid;
	DO( sm->append_rec(rid, data, false) );
    }

    Tcl_AppendElement(ip, TCL_CVBUG "append success");
    return TCL_OK;
}

static int
t_truncate_rec(Tcl_Interp* ip, int ac, char* av[])
{
    if (check(ip, "rid amount", ac, 3)) 
	return TCL_ERROR;
 
    smsize_t amount = objectsize(av[2]);
     
    if (use_logical_id(ip)) {
	lrid_t   rid;
	istrstream anon(VCPP_BUG_1 av[1], strlen(av[1])); anon >> rid;
	DO( sm->truncate_rec(rid.lvid, rid.serial, amount) );
    } else {
	rid_t   rid;
	istrstream anon(VCPP_BUG_1 av[1], strlen(av[1])); anon >> rid;
	DO( sm->truncate_rec(rid, amount) );
    }

    Tcl_AppendElement(ip, TCL_CVBUG "append success");
    return TCL_OK;
}

void
dump_pin_hdr( 
	ostream &out,
	pin_i &handle
) 
{
    int j;
    char *outbuf = new char[handle.hdr_size()+1];

    // DETECT ZVECS
    // Assume the header is reasonably small

    memcpy(outbuf, handle.hdr(), handle.hdr_size());
    outbuf[handle.hdr_size()] = 0;
    
    for (j=handle.hdr_size()-1; j>=0; j--) {
	    if(outbuf[j]!= '\0') break;
    }
    if(j== -1) {
	    out << " hdr=zvec" << handle.hdr_size() ;
    } else {
	    out << " hdr=" << outbuf ;
    }
    delete[] outbuf;
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

    char *buf = new char[buf_len];
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

	if (true || linked.verbose_flag) {
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
	}

	i += handle_len-offset;
	offset = 0;
	if (i < length) {
	    if(handle.next_bytes(eor)!=0) {
		cerr << "error in next_bytes" << endl;
		delete[] buf;
		return;
	    }
	}
    } /* while loop */
    delete[] buf;
}

w_rc_t
dump_scan( scan_file_i &scan, ostream &out, bool hex) 
{
    w_rc_t rc;
    bool eof=false;
    pin_i*  pin = 0;
    const	int buf_unit = 1024;	/* must be a power of 2 */
    char	*buf = 0;
    int		buf_len = 0;

    while ( !(rc=scan.next(pin, 0, eof)) && !eof) {
	if (linked.verbose_flag) {
	    out << pin->rid();
	    out << " hdr: " ;
	    if(pin->hdr_size() > 0) {
		if(pin->hdr_size() == sizeof(unsigned int)) {
		    // HACK:
		    unsigned int m;
		    memcpy(&m, pin->hdr(), sizeof(unsigned int));
		    out << m;
	        } else if(hex) {
		   for(unsigned int qq = 0; qq < pin->hdr_size(); qq++) {
		       const char *xxx = pin->hdr() + qq;
		       cout << int( (*xxx) ) << " ";
		   }
		} else {
		    // print as char string
		    out << pin->hdr();
		}
	    }
	    out << "(size=" << pin->hdr_size() << ")" ;
	    out << " body: " ;
	}
	vec_t w;
	bool eof = false;

	for(unsigned int j = 0; j < pin->body_size();) {
	    if(pin->body_size() > 0) {
		w.reset();

		/* Lazy buffer allocation, in buf_unit chunks.
		   There must be space for the nul string terminator */
		if ((int)pin->length() >= buf_len) {
			/* allocate the buffer in buf_unit increments */
			w_assert3(buf_len <= 8192);
			if (buf)
				delete[] buf;
			/* need a generic alignment macro (alignto) */
			buf_len = (pin->length() + 1 + buf_unit-1) &
				~(buf_unit - 1);
			buf = new char [buf_len];
			if (!buf)
				W_FATAL(fcOUTOFMEMORY);
		}

		w.put(pin->body(), pin->length());
		w.copy_to(buf, buf_len);
		buf[pin->length()] = '\0';
	    if (linked.verbose_flag) {
#ifdef W_DEBUG
		    if(hex) {
		       for(unsigned int qq = 0; qq < pin->length(); qq++) {
			   cout << int(buf[qq]) << " ";
		       }
		    } else{
		       out << buf ;
		    }
#endif /* W_DEBUG */
	         out << "(length=" << pin->length() << ")" 
				<< "(size=" << pin->body_size() << ")" 
				<< endl;
	    }
		rc = pin->next_bytes(eof);
		if(rc) break;
		j += pin->length();
	    } else {
	    // length 0
		if (linked.verbose_flag) {
		     out << "(length=" << pin->length() << ")" 
				    << "(size=" << pin->body_size() << ")" 
				    << endl;
		}
	    }
	    if(eof || rc) break;
	}
	if (linked.verbose_flag) {
	    out << endl;
	}
    }
    pin->unpin();
    if (buf)
	    delete[] buf;
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

	istrstream anon(VCPP_BUG_1 av[1], strlen(av[1])); anon >> fid;
	if (ac == 3) {
	    istrstream anon2(VCPP_BUG_1 av[2], strlen(av[2])); anon2 >> rid;
	    scan = new scan_file_i(fid.lvid, fid.serial, rid.serial);

	} else {
	    scan = new scan_file_i(fid.lvid, fid.serial);
	}
	if(scan) {
	    w_assert1(scan->error_code() || 
		    fid.lvid == scan->lvid() && fid.serial == scan->lfid());
	}
    } else {
	rid_t   rid;
	stid_t  fid;

	istrstream anon(VCPP_BUG_1 av[1], strlen(av[1])); anon >> fid;
	if (ac == 3) {
	    istrstream anon2(VCPP_BUG_1 av[2], strlen(av[2])); anon2 >> rid;
	    scan = new scan_file_i(fid, rid);
	} else {
	    scan = new scan_file_i(fid);
	}
    }
    TCL_HANDLE_FSCAN_FAILURE(scan);
    rc = dump_scan(*scan, cout);
    delete scan;

    DO( rc );

    Tcl_AppendElement(ip, TCL_CVBUG "scan success");
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

	istrstream anon(VCPP_BUG_1 av[1], strlen(av[1])); anon >> fid;

        if (ac == 3) {
	    istrstream anon2(VCPP_BUG_1 av[2], strlen(av[2])); anon2 >> rid;
            scan = new scan_file_i(fid.lvid, fid.serial, rid.serial, ss_m::t_cc_page);
        } else {
            scan = new scan_file_i(fid.lvid, fid.serial, ss_m::t_cc_page);
	}

	TCL_HANDLE_FSCAN_FAILURE(scan);
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

	istrstream anon(VCPP_BUG_1 av[1], strlen(av[1])); anon >> fid;

        if (ac == 3) {
	    istrstream anon2(VCPP_BUG_1 av[2], strlen(av[2])); anon2 >> rid;
            scan = new scan_file_i(fid, rid);
        } else {
            scan = new scan_file_i(fid);
	}
	TCL_HANDLE_FSCAN_FAILURE(scan);

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
    smsize_t start = objectsize(av[3]);
    lock_mode_t lmode = SH;
 
    if (ac == 5) {
	if (strcmp(av[4], "EX") == 0) {
	    lmode = EX;
	}
    }

    if (use_logical_id(ip)) {
	lrid_t   rid;
	istrstream anon2(VCPP_BUG_1 av[2], strlen(av[2])); anon2 >> rid;

	DO(p->pin(rid.lvid, rid.serial, start, lmode));

    } else {
	rid_t   rid;
	istrstream anon2(VCPP_BUG_1 av[2], strlen(av[2])); anon2 >> rid;

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
t_page_containing(Tcl_Interp* ip, int ac, char* av[])
{
    if (check(ip, "pin offset", ac, 3))
	return TCL_ERROR;
    
    pin_i* p = (pin_i*) strtol(av[1], 0, 0);
    smsize_t offset = strtol(av[2], 0, 0);
    smsize_t byte_in_page;
    lpid_t page = p->page_containing(offset, byte_in_page);

    tclout.seekp(ios::beg);
    tclout << page <<ends;

    Tcl_AppendResult(ip, tclout.str(), 0);

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
t_pin_large_impl(Tcl_Interp* ip, int ac, char* av[])
{
    if (check(ip, "pin", ac, 2))
	return TCL_ERROR;
    
    pin_i* p = (pin_i*) strtol(av[1], 0, 0);

    int j= p->large_impl();
    tclout.seekp(ios::beg);
    tclout << j <<ends;
    Tcl_AppendResult(ip, tclout.str(), 0);

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
   
    smsize_t start = objectsize(av[2]); 
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
   
    smsize_t start = objectsize(av[2]); 
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
   
    smsize_t amount = objectsize(av[2]); 

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

	istrstream anon(VCPP_BUG_1 av[1], strlen(av[1])); anon >> lfid;

	if (ac == 4) {
	    lrid_t   lrid;
	    istrstream anon2(VCPP_BUG_1 av[2], strlen(av[2])); anon2 >> lrid;
	    s = new scan_file_i(lfid.lvid, lfid.serial, lrid.serial, cc);
	} else {
	    if(cc ==  ss_m::t_cc_append) {
		s = new append_file_i(lfid.lvid, lfid.serial);
	    } else {
		s = new scan_file_i(lfid.lvid, lfid.serial, cc);
	    }
	}

	/* XXX isn't this handled by TCL_HANDLE_FSCAN_FAILURE ??? */
	if (!s) {
	    cerr << "Out of memory: file " << __FILE__ 
		 << " line " << __LINE__ << endl;
	    W_FATAL(fcOUTOFMEMORY);
	}

	TCL_HANDLE_FSCAN_FAILURE(s);
    } else {
	stid_t   fid;
	istrstream anon(VCPP_BUG_1 av[1], strlen(av[1])); anon >> fid;

	if (ac == 4) {
	    rid_t   rid;
	    istrstream anon2(VCPP_BUG_1 av[2], strlen(av[2])); anon2 >> rid;
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
	TCL_HANDLE_FSCAN_FAILURE(s);
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
   
    smsize_t start = objectsize(av[2]);
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
   
    smsize_t start = objectsize(av[2]);
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
	DO( s->create_rec(hdr, objectsize(av[3]), data, lrid) );
	tclout << lrid << ends;
    } else {
	DO( s->create_rec(hdr, objectsize(av[3]), data, rid) );
	tclout << rid << ends;
    }
    Tcl_AppendResult(ip, tclout.str(), 0);
    return TCL_OK;
}

static int
t_create_assoc(Tcl_Interp* ip, int ac, char* av[])
{
    if (check(ip, "stid key el [keydescr] [concurrency]", ac, 4, 5, 6)) 
		 // 1    2  3   4         5
	return TCL_ERROR;
    
    vec_t key, el;
    int i; // used iff keytype is int/unsigned
    unsigned int u; // used iff keytype is int/unsigned

    w_base_t::int8_t il; // used iff keytype is int8_t 
    w_base_t::uint8_t ul; // used iff keytype is uint8_t
    
    key.put(av[2], strlen(av[2]));
    el.put(av[3], strlen(av[3]));

    ss_m::concurrency_t cc = ss_m::t_cc_kvl;
    lrid_t lrid;
    rid_t rid;
    if (ac == 6) {
	cc = cvt2concurrency_t(av[5]);
	if(cc == ss_m::t_cc_im ) {
	    // Interpret the element as a rid

	    if (use_logical_id(ip)) {
		istrstream anon(VCPP_BUG_1 av[3], strlen(av[3])); anon >> lrid;
		DO( sm->serial_to_rid(lrid.lvid, lrid.serial, rid));
		static int warning = 0;
		if( !warning++) {
		    cerr << "WARNING:  Converting logical rids "
			<< " to physical rids " 
			<< " before insertion in index " << endl;
		}
	    } else {
		istrstream anon2(VCPP_BUG_1 av[3], strlen(av[3])); anon2 >> rid;
	    }
	    el.reset().put(&rid, sizeof(rid));
	} 
    }

    if (ac > 4) {
	const char *keydescr = (const char *)av[4];

	enum typed_btree_test t = cvt2type(keydescr);
	if (t == test_nosuch) {
	    Tcl_AppendResult(ip, 
	       "create_assoc: 4th argument must start with [bBiIuUfF]", 0);
	    return TCL_ERROR;
		
	}
	switch(t) {
	    case test_i4: {
		    i = strtol(av[2], 0, 0);
		    key.reset();
		    key.put(&i, sizeof(int));
		} break;
	    case test_u4: {
		    u = strtoul(av[2], 0, 0);
		    key.reset();
		    key.put(&u, sizeof(u));
		}
		break;

	    case test_i8: {
		    il = w_base_t::strtoi8(av[2]); 
		    key.reset();
		    key.put(&i, sizeof(w_base_t::int8_t));
		} break;
	    case test_u8: {
		    ul = w_base_t::strtou8(av[2]);
		    key.reset();
		    key.put(&u, sizeof(w_base_t::uint8_t));
		}
		break;

	    case test_bv:
	    case test_b1:
	    case test_b23:
		break;

	    default:
		Tcl_AppendResult(ip, 
	       "ssh.create_assoc function unsupported for given keytype:  ",
		   keydescr, 0);
	    return TCL_ERROR;
	}
    }

    if (use_logical_id(ip)) {
	lstid_t stid;
	istrstream anon(VCPP_BUG_1 av[1], strlen(av[1])); anon >> stid;
	DO( sm->create_assoc(stid.lvid, stid.serial, key, el) );
    } else {
	stid_t stid;
	istrstream anon(VCPP_BUG_1 av[1], strlen(av[1])); anon >> stid;
	DO( sm->create_assoc(stid, key, el) );
    }	
    return TCL_OK;
}

static int
t_destroy_assoc(Tcl_Interp* ip, int ac, char* av[])
{
    if (check(ip, "stid key el [keytype]", ac, 4, 5)) 
	return TCL_ERROR;
    
    vec_t key, el;
    
    key.put(av[2], strlen(av[2]));
    el.put(av[3], strlen(av[3]));

    int i;
    unsigned int u;

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
	if( keydescr[0] == 'i' ) {
	    i = atoi(av[2]);
	    key.reset();
	    key.put(&i, sizeof(int));
	} else if( keydescr[0] == 'u') {
	    u = strtoul(av[2], 0, 0);
	    key.reset();
	    key.put(&u, sizeof(u));
	}
    }

    if (use_logical_id(ip)) {
	lstid_t stid;
	istrstream anon(VCPP_BUG_1 av[1], strlen(av[1])); anon >> stid;
	DO( sm->destroy_assoc(stid.lvid, stid.serial, key, el) );
    } else {
	stid_t stid;
	istrstream anon(VCPP_BUG_1 av[1], strlen(av[1])); anon >> stid;
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
	istrstream anon(VCPP_BUG_1 av[1], strlen(av[1])); anon >> stid;
	DO( sm->destroy_all_assoc(stid.lvid, stid.serial, key, num_removed) );
    } else {
	stid_t stid;
	istrstream anon(VCPP_BUG_1 av[1], strlen(av[1])); anon >> stid;
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
    if (check(ip, "stid key [keytype]", ac, 3, 4))
	return TCL_ERROR;
    
    vec_t key;
    bool found;
    key.put(av[2], strlen(av[2]));

    int i;
    unsigned int u;
    
    if (ac > 3) {
	const char *keydescr = (const char *)av[3];

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
	if( keydescr[0] == 'i' ) {
	    i = atoi(av[2]);
	    key.reset();
	    key.put(&i, sizeof(int));
	} else if( keydescr[0] == 'u') {
	    u = strtoul(av[2], 0, 0);
	    key.reset();
	    key.put(&u, sizeof(u));
	}
    }
#define ELSIZE 4044
    char *el = new char[ELSIZE];
    smsize_t elen = ELSIZE-1;

    w_rc_t ___rc;

    if (use_logical_id(ip)) {
	lstid_t stid;
	istrstream anon(VCPP_BUG_1 av[1], strlen(av[1])); anon >> stid;
	___rc = sm->find_assoc(stid.lvid, stid.serial, key, el, elen, found);
    } else {
	stid_t stid;
	istrstream anon(VCPP_BUG_1 av[1], strlen(av[1])); anon >> stid;
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
	Tcl_AppendElement(ip, TCL_CVBUG el);
	delete[] el;
	return TCL_OK;
    } 

    Tcl_AppendElement(ip, TCL_CVBUG "not found");
    delete[] el;
    return TCL_ERROR;
}

static int
t_create_md_index(Tcl_Interp* ip, int ac, char* av[])
{
    if (check(ip, "vid dim ndxtype", ac, 4))
	return TCL_ERROR;

    int2_t dim = atoi(av[2]);

    if (use_logical_id(ip)) {
	lstid_t stid;
	istrstream anon(VCPP_BUG_1 av[1], strlen(av[1])); anon >> stid.lvid;
	DO( sm->create_md_index(stid.lvid, cvt2ndx_t(av[3]),
				ss_m::t_regular, stid.serial, dim) );
	tclout.seekp(ios::beg);
	tclout << stid << ends;
    } else {
	stid_t stid;
	DO( sm->create_md_index(atoi(av[1]), cvt2ndx_t(av[3]),
				ss_m::t_regular, stid, dim) );
	tclout.seekp(ios::beg);
	tclout << stid << ends;
    }
    Tcl_AppendResult(ip, tclout.str(), 0);
    return TCL_OK;
}

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
	istrstream anon(VCPP_BUG_1 av[1], strlen(av[1])); anon >> stid;
	DO( sm->create_md_assoc(stid.lvid, stid.serial, key, el) );
    } else {
	stid_t stid;
	istrstream anon(VCPP_BUG_1 av[1], strlen(av[1])); anon >> stid;
	DO( sm->create_md_assoc(stid, key, el) );
    }

    return TCL_OK;
}


static int
t_find_md_assoc(Tcl_Interp* ip, int ac, char* av[])
{
    if (check(ip, "stid box", ac, 3))
	return TCL_ERROR;

    nbox_t key(av[2]);
    bool found;

#define ELEN_ 80
    char el[ELEN_];
    smsize_t elen = sizeof(char) * ELEN_;

    if (use_logical_id(ip)) {
	lstid_t stid;
	istrstream anon(VCPP_BUG_1 av[1], strlen(av[1])); anon >> stid;
	DO( sm->find_md_assoc(stid.lvid, stid.serial, key, el, elen, found) );
    } else {
	stid_t stid;
	istrstream anon(VCPP_BUG_1 av[1], strlen(av[1])); anon >> stid;
	DO( sm->find_md_assoc(stid, key, el, elen, found) );
    }
    if (found) {
      el[elen] = 0;	// null terminate the string
      Tcl_AppendElement(ip, TCL_CVBUG el);
    }
    return TCL_OK;
}


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
	istrstream anon(VCPP_BUG_1 av[1], strlen(av[1])); anon >> stid;
	DO( sm->destroy_md_assoc(stid.lvid, stid.serial, key, el) );
    } else {
	stid_t stid;
	istrstream anon(VCPP_BUG_1 av[1], strlen(av[1])); anon >> stid;
	DO( sm->destroy_md_assoc(stid, key, el) );
    }

    return TCL_OK;
}

static void boxGen(nbox_t* rect[], int number, const nbox_t& universe)
{
    register int i;
    int box[4];
    int length, width;

    for (i = 0; i < number; i++) {
        // generate the bounding box
//    length = (int) (random_generator::generator.drand() * universe.side(0) / 10);
//    width = (int) (random_generator::generator.drand() * universe.side(1) / 10);
        length = width = 20;
        if (length > 10*width || width > 10*length)
             width = length = (width + length)/2;
        box[0] = ((int) (random_generator::generator.drand() * (universe.side(0) - length)) 
		  + universe.side(0)) % universe.side(0) 
                + universe.bound(0);
        box[1] = ((int) (random_generator::generator.drand() * (universe.side(1) - width)) 
		  + universe.side(1)) % universe.side(1)
                + universe.bound(1);
        box[2] = box[0] + length;
        box[3] = box[1] + width;
        rect[i] = new nbox_t(2, box); 
        }
}


/*
 XXX why are 'flag' and 'i' static?  It breaks multiple threads.
 It's that way so jiebing can have repeatable box generation.
 In the future, the random_generator::generator should be fixed.  There is also
 a problem with deleteing  possibly in-use boxes.
 */
static int
t_next_box(Tcl_Interp* ip, int ac, char* av[])
{
    if (check(ip, "universe", ac, 2))
        return TCL_ERROR;

    int j;
    static int flag = false;
    static int i = -1;
    static const int box_count = 50;
    static nbox_t* box[box_count]; // NB: static
    nbox_t universe(av[1]);

    if (++i >= box_count) {
        for (j=0; j<box_count; j++) delete box[j];
        flag = false;
    }

    if (!flag) {
        boxGen(box, box_count, universe);
        i = 0; flag = true;
    }

    Tcl_AppendElement(ip, TCL_CVBUG (*box[i]));

    return TCL_OK;
}


static int
t_draw_rtree(Tcl_Interp* ip, int ac, char* av[])
{
    if (check(ip, "stid", ac, 3))
        return TCL_ERROR;

    ofstream DrawFile(av[2]);
    if (!DrawFile) {
    	w_rc_t	e = RC(fcOS);
	cerr << "Can't open draw file: " << av[2] << ":" << endl
		<< e << endl;

	return TCL_ERROR;	/* XXX or should it be some other error? */
    }

    if (use_logical_id(ip)) {
	lstid_t stid;
	istrstream anon(VCPP_BUG_1 av[1], strlen(av[1])); anon >> stid;
	DO( sm->draw_rtree(stid.lvid, stid.serial, DrawFile) );
    } else {
	stid_t stid;
	istrstream anon(VCPP_BUG_1 av[1], strlen(av[1])); anon >> stid;
	DO( sm->draw_rtree(stid, DrawFile) );
    }
    return TCL_OK;
}

static int
t_rtree_stats(Tcl_Interp* ip, int ac, char* av[])
{
    if (check(ip, "stid", ac, 2))
        return TCL_ERROR;
    uint2_t level = 5;
    uint2_t ovp[5];
    rtree_stats_t stats;

    if (use_logical_id(ip)) {
	lstid_t stid;
	istrstream anon(VCPP_BUG_1 av[1], strlen(av[1])); anon >> stid;
	DO( sm->rtree_stats(stid.lvid, stid.serial, stats, level, ovp) );
    } else {
	stid_t stid;
	istrstream anon(VCPP_BUG_1 av[1], strlen(av[1])); anon >> stid;
	DO( sm->rtree_stats(stid, stats, level, ovp) );
    }

    if (linked.verbose_flag)  {
	cout << "rtree stat: " << endl;
	cout << stats << endl;
    }

    if (linked.verbose_flag)  {
	cout << "overlapping stats: ";
	for (uint i=0; i<stats.level_cnt; i++) {
	    cout << " (" << i << ") " << ovp[i];
	}
	cout << endl;
    }
 
    return TCL_OK;
}

static int
t_rtree_scan(Tcl_Interp* ip, int ac, char* av[])
{
    if (check(ip, "stid cond box [concurrency_t]", ac, 4, 5))
        return TCL_ERROR;

    nbox_t box(av[3]);
    nbox_t::sob_cmp_t cond = cvt2sob_cmp_t(av[2]);
    scan_rt_i* s = NULL;

    ss_m::concurrency_t cc = ss_m::t_cc_page;
    if (ac == 5) {
	cc = cvt2concurrency_t(av[4]);
    }

    if (use_logical_id(ip)) {
	lstid_t stid;
	istrstream anon(VCPP_BUG_1 av[1], strlen(av[1])); anon >> stid;

	s = new scan_rt_i(stid.lvid, stid.serial, cond, box, cc);
    } else {
	stid_t stid;
	istrstream anon(VCPP_BUG_1 av[1], strlen(av[1])); anon >> stid;

	s = new scan_rt_i(stid, cond, box, cc);
    }
    if (!s) {
	cerr << "Out of memory: file " << __FILE__
	     << " line " << __LINE__ << endl;
	W_FATAL(fcOUTOFMEMORY);
    }

    bool eof = false;
    char tmp[100];
    smsize_t elen = 100;
    nbox_t ret(box);

#ifdef SSH_VERBOSE
    if (linked.verbose_flag)  {
	cout << "------- start of query ----------\n";
	cout << "matching rects of " << av[2] 
	     << " query for rect (" << av[3] << ") are: \n";
    }
#endif

    if ( s->next(ret, tmp, elen, eof)!=0 ) {
        delete s; return TCL_ERROR;
        }
    while (!eof) {
	if (linked.verbose_flag)  {
	    ret.print(cout, 4);
#ifdef SSH_VERBOSE
	    tmp[elen] = 0;
	    cout << "\telem " << tmp << endl;
#endif
	}
        elen = 100;
        if ( s->next(ret, tmp, elen, eof)!=0 ) {
            delete s; return TCL_ERROR;
            }
        }

#ifdef SSH_VERBOSE
    if (linked.verbose_flag)  {
	cout << "-------- end of query -----------\n";
    }
#endif

    delete s;
    return TCL_OK;
}

static sort_stream_i* sort_container;

static int
t_begin_sort_stream(Tcl_Interp* ip, int ac, char* av[])
{

    const int vid_arg=1;
    const int runsize_arg=2;
    const int type_arg=3;
    const int rlen_arg=4;
    const int distinct_arg=5;
    const int updown_arg=6;
    const int universe_arg=7;
    const int derived_arg=8;
    const char *usage_string = 
    //1  2       3    4           5     	    6              7        8      
    "vid runsize type rlen \"distinct/duplicate\" \"up/down\" [universe [derived] ]";

    if (check(ip, usage_string, ac, updown_arg+1, universe_arg+1, derived_arg+1))
	return TCL_ERROR;

    key_info_t info;
    sort_parm_t sp;
    
    info.offset = 0;
    info.where = key_info_t::t_hdr;

    switch(cvt2type(av[type_arg])) {
	case test_bv:
	    // info.type = key_info_t::t_string;
	    info.type = sortorder::kt_b;
	    info.len = 0;
	    break;
	case test_b1:
	    // info.type = key_info_t::t_char;
	    info.type = sortorder::kt_u1;
	    info.len = sizeof(char);
	    break;
	case test_i1:
	    // new test
	    info.type = sortorder::kt_i1;
	    info.len = sizeof(int1_t);
	    break;
	case test_i2:
	    // new test
	    info.type = sortorder::kt_i2;
	    info.len = sizeof(int2_t);
	    break;
	case test_i4:
	    // info.type = key_info_t::t_int;
	    info.type = sortorder::kt_i4;
	    info.len = sizeof(int4_t);
	    break;
	case test_i8:
	    // NB: Tcl doesn't handle 64-bit arithmetic
	    // info.type = key_info_t::t_int;
	    info.type = sortorder::kt_i8;
	    info.len = sizeof(w_base_t::int8_t);
	    break;
	case test_u1:
	    // new test
	    info.type = sortorder::kt_u1;
	    info.len = sizeof(uint1_t);
	    break;
	case test_u2:
	    // new test
	    info.type = sortorder::kt_u2;
	    info.len = sizeof(uint2_t);
	    break;
	case test_u4:
	    // new test
	    info.type = sortorder::kt_u4;
	    info.len = sizeof(uint4_t);
	    break;
	case test_u8:
	    // new test
	    info.type = sortorder::kt_u8;
	    info.len = sizeof(w_base_t::uint8_t);
	    break;
	case test_f4:
	    // info.type = key_info_t::t_float;
	    info.type = sortorder::kt_f4;
	    info.len = sizeof(f4_t);
	    break;
	case test_f8:
	    // new test
	    info.type = sortorder::kt_f8;
	    info.len = sizeof(f8_t);
	    break;
	case test_spatial:
	    // info.type = key_info_t::t_spatial;
	    info.type = sortorder::kt_spatial;
	    info.len = 0; // to be set from universe
	    if (ac<=8) {
		if (check(ip, usage_string, 9, 10)) return TCL_ERROR;
	    }
	    break;
	default:
	    W_FATAL(fcNOTIMPLEMENTED);
	    break;
    }

    sp.run_size = atoi(av[runsize_arg]);
    sp.vol = atoi(av[vid_arg]);

    if (strcmp("normal", av[distinct_arg]))  {
	sp.unique = !strcmp("distinct", av[distinct_arg]);
	if (!sp.unique) {
	    if (check(ip, usage_string, ac, 0)) return TCL_ERROR;
        }
    }

    if (strcmp("up", av[updown_arg]))  {
	sp.ascending = (strcmp("down", av[updown_arg]) != 0);
	if (sp.ascending) {
	    if (check(ip, usage_string, ac, 0)) return TCL_ERROR;
        }
    }

    if (ac>universe_arg) {
	nbox_t univ(av[universe_arg]);
	info.universe = univ;
	info.len = univ.klen();
    }

    if (ac>derived_arg) {
	info.derived = (atoi(av[derived_arg]) != 0);
    }

    if (use_logical_id(ip)) {
        lvid_t lvid;
	istrstream anon(VCPP_BUG_1 av[vid_arg], strlen(av[vid_arg])); anon >> lvid;
	vid_t tmp_vid;
	DO(sm->lvid_to_vid(lvid, tmp_vid));
	sp.vol = tmp_vid;
    }
    
    if (sort_container) delete sort_container;
    sort_container = new sort_stream_i(info, sp, atoi(av[rlen_arg]));
    w_assert1(sort_container);
    return TCL_OK;
}
    
static int
t_sort_stream_put(Tcl_Interp* ip, int ac, char* av[])
{
    // Puts typed key in header, string data in body
    if (check(ip, "type key data", ac, 4))
	return TCL_ERROR;

    if (!sort_container)
	return TCL_ERROR;

    vec_t key, data;

    typed_value v;
    cvt2typed_value( cvt2type(av[1]), av[2], v);

    key.put(&v._u, v._length);

    data.put(av[3], strlen(av[3]));

    DO( sort_container->put(key, data) );
    return TCL_OK;
}

static int
t_sort_stream_get(Tcl_Interp* ip, int ac, char* av[])
{
    if (check(ip, "next type", ac, 3))
	return TCL_ERROR;

    if (strcmp(av[1],"next")) {
      if (check(ip, "next", ac, 0))
	return TCL_ERROR;
    }

    vec_t key, data;
    bool eof = true;
    if (sort_container) {
    	DO( sort_container->get_next(key, data, eof) );
    }

    if (eof) {
	Tcl_AppendElement(ip, TCL_CVBUG "eof");
	if (sort_container) {
	    delete sort_container;
	    sort_container = 0;
	}
    } else {
	typed_value v;

	typed_btree_test t = cvt2type(av[2]);
	tclout.seekp(ios::beg);

	switch(t) {
	case test_bv:{
		char* buf= new char [key.size()];
		key.copy_to(buf, key.size());

		tclout <<  buf;
		delete[] buf;
	    }
	    break;

	case test_spatial:
	    W_FATAL(fcNOTIMPLEMENTED);
	    break;

	default:
	    key.copy_to((void*)&v._u, key.size());
	    v._length = key.size();
	    break;
	}
	switch ( t ) {
	case  test_bv:
	    break;

	case  test_b1:
	    tclout <<  v._u.b1;
	    break;

	case  test_i1:
	    tclout << (int)v._u.i1_num;
	    break;
	case  test_u1:
	    tclout << (unsigned int)v._u.u1_num ;
	    break;

	case  test_i2:
	    tclout << v._u.i2_num ;
	    break;
	case  test_u2:
	    tclout << v._u.u2_num ;
	    break;

	case  test_i4:
	    tclout << v._u.i4_num ;
	    break;
	case  test_u4:
	    tclout << v._u.u4_num ;
	    break;

	case  test_i8:
// TODO: fix on NT
	    // tclout << v._u.i8_num ;
	    break;
	case  test_u8:
// TODO: fix on NT
	    // tclout << v._u.u8_num ;
	    break;
	case  test_f8:
	    tclout << v._u.f8_num ;
	    break;

	case  test_f4:
	    tclout << v._u.f4_num ;
	    break;

	case test_spatial:
	default:
	    W_FATAL(fcNOTIMPLEMENTED);
	    break;
	}
	{
	    char* buf = new char[data.size()+1];
	    data.copy_to(buf, data.size());
	    buf[data.size()] = '\0';

	    tclout << " "  << (char *)&buf[0] ;
		delete[] buf;
	}
	tclout << ends;
    	Tcl_AppendResult(ip, tclout.str(), 0);
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
t_link_to_remote_id(Tcl_Interp* ip, int ac, char* av[])
{
    if (check(ip, "local_volume remote_id", ac, 3))
	return TCL_ERROR;

    if (use_logical_id(ip)) {
	lid_t remote_id;
	lid_t new_id;
	istrstream anon(VCPP_BUG_1 av[1], strlen(av[1])); anon >> new_id.lvid;

	istrstream anon2(VCPP_BUG_1 av[2], strlen(av[2])); anon2 >> remote_id;

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
	istrstream anon(VCPP_BUG_1 av[1], strlen(av[1])); anon >> remote_id;
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
	istrstream anon(VCPP_BUG_1 av[1], strlen(av[1])); anon >> lrid;
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
	istrstream anon(VCPP_BUG_1 av[1], strlen(av[1])); anon >> lrid;
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
	istrstream anon(VCPP_BUG_1 av[1], strlen(av[1])); anon >> lstid;
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
	enable = true;
    } else if (strcmp(av[1], "disable") == 0) {
	enable = false;
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
    tcl_append_boolean(ip, enabled);

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
	istrstream anon(VCPP_BUG_1 av[1], strlen(av[1])); anon >> lvid;
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
    bool use_phys = true;
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
	    if (rc.err_num() != ss_m::eBADLOGICALID && 
		rc.err_num() != ss_m::eBADVOL) { 
		tclout.seekp(ios::beg);
		tclout << ssh_err_name(rc.err_num()) << ends;
		Tcl_AppendResult(ip, tclout.str(), 0);
		return TCL_ERROR;
	    }
        } else {
	    use_phys = false;
	}
    }
   
    if (use_phys) {
	lockid_t n;
	cvt2lockid_t(av[1], n);
	
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
t_dont_escalate(Tcl_Interp* ip, int ac, char* av[])
{
    if (check(ip, "entity [\"dontPassOnToDescendants\"]", ac, 2, 3))
	return TCL_ERROR;
    
    bool	use_phys = true;
    bool	passOnToDescendants = true;
    rc_t	rc;

    if (ac == 3)  {
	if (strcmp(av[2], "dontPassOnToDescendants"))  {
            Tcl_AppendResult(ip, "last parameter must be \"dontPassOnToDescendants\", not ", av[2], 0);
	    return TCL_ERROR;
	}  else  {
	    passOnToDescendants = false;
	}
    }

    if (use_logical_id(ip))  {
	return TCL_ERROR;

	lid_t   id;
	lvid_t	lvid;
	int 	len = strlen(av[1]);
	istrstream l(av[1], len); l  >> id;
	istrstream v(av[1], len); v  >> lvid;

	if (l) {
	    // this was a logical id (volume ID and serial #)
	    DO( sm->dont_escalate(id.lvid, id.serial, passOnToDescendants) );
	} else {
	    // this was only a logical volume id (volume ID and serial #)
	    DO( sm->dont_escalate(lvid, passOnToDescendants) );
	}

	if (rc)  {
	    if (rc.err_num() != ss_m::eBADLOGICALID && rc.err_num() != ss_m::eBADVOL) { 
	        DO( rc );
	    }
	    else  {
		use_phys = false;
	    }
        }
    } 

    if (use_phys)  {
	lockid_t n;
	cvt2lockid_t(av[1], n);
	DO( sm->dont_escalate(n, passOnToDescendants) );
    }

    return TCL_OK;
}

static int
t_get_escalation_thresholds(Tcl_Interp* ip, int ac, char* /*av*/[])
{
    if (check(ip, "", ac, 1))
	return TCL_ERROR;

    int4_t	toPage = 0;
    int4_t	toStore = 0;
    int4_t	toVolume = 0;

    DO( sm->get_escalation_thresholds(toPage, toStore, toVolume) );

    if (toPage == dontEscalate)
	toPage = 0;
    if (toStore == dontEscalate)
	toStore = 0;
    if (toVolume == dontEscalate)
	toVolume = 0;

    tclout.seekp(ios::beg);
    tclout << toPage << " " << toStore << " " << toVolume << ends;
    Tcl_AppendResult(ip, tclout.str(), 0);
    return TCL_OK;
}

static int4_t
parse_escalation_thresholds(char *s)
{
    int		result = atoi(s);

    if (result == 0)
	result = dontEscalate;
    else if (result < 0)
	result = dontModifyThreshold;
    
    return result;
}

static int
t_set_escalation_thresholds(Tcl_Interp* ip, int ac, char* av[])
{
    int		listArgc;
    char**	listArgv;
    const char*	errString = 
    "{toPage, toStore, toVolume} (0 =don't escalate, <0 =don't modify)";

    if (check(ip, errString, ac, 2))
	return TCL_ERROR;
    
    if (Tcl_SplitList(ip, av[1], &listArgc, &listArgv) != TCL_OK)
	return TCL_ERROR;
    
    if (listArgc != 3)  {
	ip->result = (char *) errString;
	return TCL_ERROR;
    }

    int4_t	toPage = parse_escalation_thresholds(listArgv[0]);
    int4_t	toStore = parse_escalation_thresholds(listArgv[1]);
    int4_t	toVolume = parse_escalation_thresholds(listArgv[2]);

    Tcl_Free( (char *) listArgv);

    DO( sm->set_escalation_thresholds(toPage, toStore, toVolume) );

    return TCL_OK;
}

#include <deadlock_events.h>

static int
t_start_deadlock_info_recording(Tcl_Interp* ip, int ac, char* /*av*/[])
{
    if (check(ip, "", ac, 1))
	return TCL_ERROR;
    
    DeadlockEventPrinter* x = new DeadlockEventPrinter(cerr);
    DO( sm->set_deadlock_event_callback(x) );

    return TCL_OK;
}

static int
t_stop_deadlock_info_recording(Tcl_Interp* ip, int ac, char* /*av*/[])
{
    if (check(ip, "", ac, 1))
	return TCL_ERROR;
    
    DO( sm->set_deadlock_event_callback(0) );

    return TCL_OK;
}


static int
t_lock_many(Tcl_Interp* ip, int ac, char* av[])
{
    if (check(ip, "num_requests entity mode [duration [timeout]]", 
	      ac, 4, 5, 6) )
	return TCL_ERROR;

    rc_t rc; // return code
    bool use_phys = true;
    smsize_t num_requests = numobjects(av[1]);
    smsize_t i;
    lock_mode_t mode = cvt2lock_mode(av[3]);

    if (use_logical_id(ip)) {
	lid_t   id;
	istrstream anon2(VCPP_BUG_1 av[2], strlen(av[2])); anon2 >> id;
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
	    use_phys = false;
	}
    }
   
    if (use_phys) {
	lockid_t n;
	cvt2lockid_t(av[2], n);
	
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
    bool use_phys = true;

    if (use_logical_id(ip)) {
	lid_t   id;
	istrstream anon(VCPP_BUG_1 av[1], strlen(av[1])); anon >> id;
	rc = sm->unlock(id.lvid, id.serial);
	if (rc)  {
	    if (rc.err_num() != ss_m::eBADLOGICALID && rc.err_num() != ss_m::eBADVOL) { 
		return TCL_ERROR;
	    }
        } else {
	    use_phys = false;
	}
    }
    if (use_phys) {
	lockid_t n;
	cvt2lockid_t(av[1], n);
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

    DO( sm->dump_locks(cout) );
    return TCL_OK;
}

static int
t_query_lock(Tcl_Interp* ip, int ac, char* av[])
{
    if (check(ip, "entity", ac, 2) )
	return TCL_ERROR;

    rc_t rc; // return code
    bool use_phys = true;
    lock_mode_t m = NL;

    if (use_logical_id(ip)) {
	lid_t   id;
	istrstream anon(VCPP_BUG_1 av[1], strlen(av[1])); anon >> id;
	rc = sm->query_lock(id.lvid, id.serial, m);
	if (rc)  {
	    if (rc.err_num() != ss_m::eBADLOGICALID && rc.err_num() != ss_m::eBADVOL) { 
		return TCL_ERROR;
	    }
        } else {
	    use_phys = false;
	}
    }
    if (use_phys) {
	lockid_t n;
	cvt2lockid_t(av[1], n);
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
	enable = true;
    } else if (strcmp(av[1], "disable") == 0) {
	enable = false;
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
    tcl_append_boolean(ip, enabled);

    return TCL_OK;
}

static int
t_print_lid_index(Tcl_Interp* ip, int ac, char* av[])
{
    if (check(ip, "vid", ac, 2) )
	return TCL_ERROR;

    if (use_logical_id(ip))  {
	lvid_t lvid;
	istrstream anon(VCPP_BUG_1 av[1], strlen(av[1])); anon >> lvid;
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
   
    smsize_t num_recs = numobjects(av[1]);
    smsize_t chunk_count = objectsize(av[6]);
    smsize_t len_hint = objectsize(av[4]);
    vec_t hdr, data;
    hdr.put(av[3], strlen(av[3]));
    data.put(av[5], strlen(av[5]));

    
    if (use_logical_id(ip)) {
	lstid_t  stid;
	lrid_t   rid;

	istrstream anon(VCPP_BUG_1 av[2], strlen(av[2])); anon >> stid;
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
	
	istrstream anon2(VCPP_BUG_1 av[2], strlen(av[2])); anon2 >> stid;
	cout << "creating " << num_recs << " records in " << stid
	     << " with hdr_len= " << hdr.size() << " chunk_len= "
	     << data.size() << " in " << chunk_count << " chunks."
	     << endl;

	for (uint i = 0; i < num_recs; i++) {
	    DO( sm->create_rec(stid, hdr, len_hint, data, rid) );
	    for (uint j = 1; j < chunk_count; j++) {
		DO( sm->append_rec(rid, data, false));
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
    
    smsize_t   start;
    vec_t   data;
    smsize_t	    num_updates = numobjects(av[1]);

    data.set(parse_vec(av[4], strlen(av[4])));
    start = objectsize(av[3]);    

    if (use_logical_id(ip)) {
	lrid_t   rid;

	istrstream anon2(VCPP_BUG_1 av[2], strlen(av[2])); anon2 >> rid;
	for (smsize_t i = 0; i < num_updates; i++) {
	    DO( sm->update_rec(rid.lvid, rid.serial, start, data) );
	}

    } else {
	rid_t   rid;

	istrstream anon2(VCPP_BUG_1 av[2], strlen(av[2])); anon2 >> rid;
	for (smsize_t i = 0; i < num_updates; i++) {
	    DO( sm->update_rec(rid, start, data) );
	}
    }

    Tcl_AppendElement(ip, TCL_CVBUG "update success");
    return TCL_OK;
}


static int
t_lock_record_blind(Tcl_Interp* ip, int ac, char* av[])
{
    if (check(ip, "num", ac, 2))
	return TCL_ERROR;

    const int page_capacity = 20;
    rid_t rid;
    rid.pid._stid.vol = 999;
    uint4_t n;
    istrstream anon(VCPP_BUG_1 av[1], strlen(av[1])); anon >> n;

    stime_t start(stime_t::now());
    
    for (uint i = 0; i < n; i += page_capacity)  {
	rid.pid.page = i+1;
	uint j = page_capacity;
	if (i + j >= n) j = n - i; 
	while (j--) {
	    rid.slot = j;
	    DO( sm->lock(rid, EX) );
	}
    }

    sinterval_t delta(stime_t::now() - start);
    cerr << "Time to get " << n << " record locks: "
    	<< delta << " seconds" << endl;

    return TCL_OK;
}


static int
t_testing(Tcl_Interp* ip, int ac, char* av[])
{
    if (check(ip, "stid key times", ac, 4))
	return TCL_ERROR;

    stid_t stid;
    istrstream anon(VCPP_BUG_1 av[1], strlen(av[1])); anon >> stid;

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
	Tcl_AppendElement(ip, TCL_CVBUG el);
	return TCL_OK;
    }

    Tcl_AppendElement(ip, TCL_CVBUG "not found");
    return TCL_ERROR;
}

static int
t_janet_setup(Tcl_Interp* ip, int ac, char* av[])
{
    if (check(ip, "stid nkeys", ac, 3))
	return TCL_ERROR;

    stid_t stid;
    istrstream anon(VCPP_BUG_1 av[1], strlen(av[1])); anon >> stid;

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
    istrstream anon(VCPP_BUG_1 av[1], strlen(av[1])); anon >> stid;

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

#ifdef USE_COORD
static int
t_startns(Tcl_Interp* ip, int ac, char* av[])
{
    if (check(ip, "nsnamefile ", ac, 2)) 
	return TCL_ERROR;
   
    const char *ns_name_file=av[1];
    DO( co->start_ns(ns_name_file));
    Tcl_AppendResult(ip, "Nameserver started.", 0);
    return TCL_OK;
}

static int
t_startco(Tcl_Interp* ip, int ac, char* av[])
{
    if (check(ip, "name ", ac, 2)) 
	return TCL_ERROR;
    DO( co->start_coord(av[1]));
    Tcl_AppendResult(ip, "Started coordinate ", 0);
    return TCL_OK;
}

static int
t_startsub(Tcl_Interp* ip, int ac, char* av[])
{
    if (check(ip, "myname ", ac, 3)) 
	return TCL_ERROR;
   
    const char *myname=av[1];
    const char *coname=av[2];
    DO( co->start_subord(myname, coname));
    Tcl_AppendResult(ip, "Started subordinate ", myname, 0);
    return TCL_OK;
}

static int
t_co_sendtcl(Tcl_Interp* ip, int ac, char* av[])
{
    if (check(ip, "where cmd", ac, 3)) 
	return TCL_ERROR;
   
    tclout.seekp(ios::beg);

    int error = 0;
    DO( co->remote_tcl_command( av[1], av[2], outbuf, sizeof(outbuf), error));
    if(error) {
	cerr << "ERROR REPLY: " << error <<endl;
	tclout.seekp(ios::beg); 
	tclout << ssh_err_name(error) << ends;
	Tcl_AppendResult(ip, tclout.str(), 0);
	return TCL_ERROR;
    } else {
	// cerr << "REMOTE REPLY OK: " << outbuf <<endl;
	Tcl_AppendResult(ip, outbuf, 0);
	return TCL_OK;
    }
}

int
t_co_retire(Tcl_Interp* , int , char*[])
{
    if(co) {
	delete co;
	co = 0;
    }
    return TCL_OK;
}

static int
t_co_newtid(Tcl_Interp* ip, int ac, char* [])
{
    if (check(ip, 0, ac, 1)) return TCL_ERROR;

    gtid_t g;
    DO( co->newtid(g));

    ostrstream s;
    s << g << ends;
    Tcl_AppendResult(ip, s.str(), 0);
    delete [] s.str();
    return TCL_OK;
}

static int
t_co_end(Tcl_Interp* ip, int ac, char* av[])
{
    if (check(ip, "gtid commit:true/false", ac, 3)) return TCL_ERROR;

    // end_transaction gtid true->commit, false->abort
    gtid_t g(av[1]);
    bool do_commit = false;
    if( 
	( strcmp(av[2],"true" )==0)
		||
	( strcmp(av[2],"commit" )==0)
    ) { 
	do_commit = true;
    }

    w_rc_t rc;
    if(co) {
	rc = co->end_transaction(do_commit, g);
	tclout.seekp(ios::beg);                                 
	if(rc) {
	    tclout << ssh_err_name(rc.err_num()) << ends;	
	    Tcl_AppendResult(ip, tclout.str(), 0);		
	    return TCL_ERROR;					
	}
	return TCL_OK;
    } else {
	Tcl_AppendResult(ip, "No coordinator.", 0);		
	return TCL_ERROR;
    }
}

static int
t_co_retry_commit(Tcl_Interp* ip, int ac, char* av[])
{
    if (check(ip, 0, ac, 2)) return TCL_ERROR;

    // retry_commit gtid 
    gtid_t g(av[1]);

    w_rc_t rc;
    if(co) {
	rc = co->end_transaction(true,g);
	tclout.seekp(ios::beg);                                 
	if(rc && rc.err_num() != smlevel_0::eVOTENO) {
	    tclout << ssh_err_name(rc.err_num()) << ends;	
	    Tcl_AppendResult(ip, tclout.str(), 0);		
	    return TCL_ERROR;					
	}
	if(rc && rc.err_num() == smlevel_0::eVOTENO) {
	    Tcl_AppendResult(ip, "aborted", 0);		
	} else {
	    Tcl_AppendResult(ip, "committed", 0);		
	}
	return TCL_OK;
    } else {
	Tcl_AppendResult(ip, "No coordinator.", 0);		
	return TCL_ERROR;
    }
}

static int
t_co_commit(Tcl_Interp* ip, int ac, char* av[])
{
    bool is_prepare = false;
    if(ac < 4) {
        Tcl_AppendResult(ip, 
	"wrong # args; commit gtid number server server server",0);
		return TCL_ERROR;
    }
    if(strcmp(av[1],"prepare")==0) {
	is_prepare = true;
    }
    if(co) {
	gtid_t g(av[1]);
	int    num;

	// commit gtid number server server server...
	num = atoi(av[2]);
	if(ac != num + 3) {
	    Tcl_AppendResult(ip, 
	    "Length of server name list doesn't match number given.",0);
	    w_assert3(0);
	    return TCL_ERROR;
	}
	w_rc_t rc;
	rc = co->commit(g, num, (const char **)(av+3), is_prepare);
	tclout.seekp(ios::beg);                                 
	if(rc && rc.err_num() != smlevel_0::eVOTENO) {
	    tclout << ssh_err_name(rc.err_num()) << ends;	
	    Tcl_AppendResult(ip, tclout.str(), 0);		
	    return TCL_ERROR;					
	}
	if(rc && rc.err_num() == smlevel_0::eVOTENO) {
	    Tcl_AppendResult(ip, "aborted", 0);		
	} else {
	    Tcl_AppendResult(ip, "committed", 0);		
	}
	return TCL_OK;
    } else {
	Tcl_AppendResult(ip, "No coordinator.", 0);		
	return TCL_ERROR;
    }
}

static int
t_co_addmap( Tcl_Interp* ip, int ac, char* av[])
{
    int    num;

    // addmap number server server server...
    num = atoi(av[1]);
    if(ac != num + 2) {
	Tcl_AppendResult(ip, 
	"Length of server name list doesn't match number given.",0);
	return TCL_ERROR;
    }
    w_rc_t rc;

if(false) {
	// TODO: clean up
    rc = co->add_map( num, (const char **)(av+2));
    if(rc && rc.err_num() == fcFOUND) {
	for(int i=0; i<num; i++) {
		const char *c = av[i+2];
		rc = co->refresh_map(c);
	}
    }
} else {
    for(int i=0; i<num; i++) {
	const char *c = av[i+2];
	rc = co->refresh_map(c);
    }
}
	
    if (rc)  {
	tclout.seekp(ios::beg); 
	tclout << ssh_err_name(rc.err_num()) << ends;
	Tcl_AppendResult(ip, tclout.str(), 0);
	return TCL_ERROR;
    }
    Tcl_AppendResult(ip, "ok", 0);
    // TODO: return a result?
    return TCL_OK;
}

static int
t_co_clearmap(Tcl_Interp* ip, int , char* [])
{
    DO( co->clear_map());
    return TCL_OK;
}

static int
t_local_add(Tcl_Interp* ip, int , char* av[])
{
    gtid_t g(av[1]);
    tid_t t;

    // co addlocal gtid tid
    {
	istrstream anon2(VCPP_BUG_1 av[2], strlen(av[2])); anon2 >> t;
    }
    DO( co->add(g, t));
    return TCL_OK;
}
static int
t_local_rm(Tcl_Interp* ip, int , char* av[])
{
    gtid_t g(av[1]);
    tid_t t;

    // co rmlocal gtid tid
    {
	istrstream anon2(VCPP_BUG_1 av[2], strlen(av[2])); anon2 >> t;
    }
    DO( co->remove(g, t));
    return TCL_OK;
}

static int
t_co_print(Tcl_Interp* , int , char* [])
{
    co->dump(cerr);

    return TCL_OK;
}

static int
t_gtid2tid(Tcl_Interp*ip , int ac , char* av[])
{
    if (check(ip, "", ac, 2))
	return TCL_ERROR;

    gtid_t 	g(av[1]);
    tid_t	t;

    DBG(<<"av[1]=" << av[1]
	<< " gtid=" << g);
    DO(co->tidmap()->gtid2tid(g, t));
    DBG(<<"t=" << t);

    tclout.seekp(ios::beg);
    tclout << t << ends;
    Tcl_AppendResult(ip, tclout.str(), 0);
    return TCL_OK;
}

/* name used to identify the deadlock server */
const char *GlobalDeadlockServerName = "DeadlockServer";

static int
t_startdeadlockclient(Tcl_Interp* ip, int ac, char* [])
{
    if (check(ip, "", ac, 1))
	return TCL_ERROR;
    
    DO( co->init_ep_map() );
    rc_t rc =  co->epmap()->add(1, (const char **)&GlobalDeadlockServerName);
    if (rc && rc.err_num() != fcFOUND)  {
	DO(rc);
    }
    server_handle_t deadlockServerHandle(GlobalDeadlockServerName);
    sm->set_global_deadlock_client(new CentralizedGlobalDeadlockClient(
					    10000,
					    5000,
					    new DeadlockClientCommunicator(
						    deadlockServerHandle,
						    co->epmap(),
						    *co->comm()
					    )
				    )
	);

    Tcl_AppendResult(ip, "deadlock client started.", 0);

    return TCL_OK;
}

static int
t_startdeadlockserver(Tcl_Interp* ip, int ac, char* [])
{
    if (check(ip, "", ac, 1))
	return TCL_ERROR;
    
    DO( co->init_ep_map() );
    Endpoint deadlockServerEndpoint;
    DO( co->comm()->make_endpoint(deadlockServerEndpoint) );
    DO( co->ns()->enter(GlobalDeadlockServerName, deadlockServerEndpoint) );
    DO( deadlockServerEndpoint.release() );
    DO( co->epmap()->add(1, (const char **)&GlobalDeadlockServerName) );
    server_handle_t deadlockServerHandle(GlobalDeadlockServerName);
    globalDeadlockServer = new CentralizedGlobalDeadlockServer(
				new DeadlockServerCommunicator(
					deadlockServerHandle,
					co->epmap(),
					*co->comm(),
					selectFirstVictimizer
				)
			    );

    Tcl_AppendResult(ip, "deadlock server started.", 0);

    return TCL_OK;
}

static int
t_startdeadlockvictimizer(Tcl_Interp* ip, int ac, char* [])
{
    if (check(ip, "", ac, 1))
	return TCL_ERROR;
    
    Tcl_AppendResult(ip, "not implemented.", 0);

    return TCL_OK;
}

static int
t_stopdeadlockclient(Tcl_Interp* ip, int ac, char* [])
{
    if (check(ip, "", ac, 1))
	return TCL_ERROR;
    
    sm->set_global_deadlock_client(0);

    Tcl_AppendResult(ip, "deadlock client stopped.", 0);

    return TCL_OK;
}

static int
t_stopdeadlockserver(Tcl_Interp* ip, int ac, char* [])
{
    if (check(ip, "", ac, 1))
	return TCL_ERROR;
    
    delete globalDeadlockServer;
    globalDeadlockServer = 0;

    Tcl_AppendResult(ip, "deadlock server stopped.", 0);

    return TCL_OK;
}

static int
t_stopdeadlockvictimizer(Tcl_Interp* ip, int ac, char* [])
{
    if (check(ip, "nsnamefile ", ac, 1))
	return TCL_ERROR;
    
    Tcl_AppendResult(ip, "not implemented.", 0);

    return TCL_OK;
}

#endif

#include <vtable_info.h>
#include <vtable_enum.h>
static  void
__printit(const char *what, int K, vtable_info_array_t  &x) {

    // Too much to fit in tclout
    if (linked.verbose_flag)  {
	/* method 1: */
	cout << what <<
	    " METHOD 1 - individually: n=" << x.quant() <<endl;
	for(int i=0; i< x.quant(); i++) {
	    cout << "------------- first=0, last=" << K 
		<< " (printing only non-null strings whose value != \"0\") "
		<<endl;
	    for(int j=0; j < K; j++) {
		if(strlen(x[i][j]) > 0) {
		    if(strcmp(x[i][j], "0") != 0) {
		    cout << i << "." << j << ":" << x[i][j]<< "|" << endl;
		    }
		}
	    }
	}
	cout << endl;
    }

    /* method 2: */
    // cout << what << " METHOD 2 - group" <<endl;
    // x.operator<<(cout);
}

static int
t_vt_class_factory(Tcl_Interp* ip, int , char* [])
{
    vtable_info_array_t  x;
    DO(ss_m::class_factory_collect(x));
    __printit("class factory", factory_last, x);
    // Tcl_AppendResult(ip, tclout.str(), 0);
    return TCL_OK;
}

static int
t_vt_class_factory_histo(Tcl_Interp* ip, int , char* [])
{
    vtable_info_array_t  x;
    DO(ss_m::class_factory_collect_histogram(x));
    __printit("class factory histograms", factory_histo_last, x);
    // Tcl_AppendResult(ip, tclout.str(), 0);
    return TCL_OK;
}
static int
t_vt_thread(Tcl_Interp* ip, int , char* [])
{
    vtable_info_array_t  x;
    DO(ss_m::thread_collect(x));
    __printit("threads", thread_last, x);
    // Tcl_AppendResult(ip, tclout.str(), 0);
    return TCL_OK;
}

static int
t_vt_stats(Tcl_Interp* ip, int , char* [])
{
    tclout << "vtable stats: not yet implemented" <<ends;
    Tcl_AppendResult(ip, tclout.str(), 0);
    return TCL_ERROR;
}

static int
t_vt_xct(Tcl_Interp* ip, int , char* [])
{
    vtable_info_array_t  x;
    DO(ss_m::xct_collect(x));

    __printit("transactions/xct", xct_last, x);
    // Tcl_AppendResult(ip, tclout.str(), 0);
    return TCL_OK;
}

static int
t_vt_bpool(Tcl_Interp* ip, int , char* [])
{
    vtable_info_array_t  x;
    DO(ss_m::bp_collect(x));

    __printit("buffer pool", bp_last, x);
    // Tcl_AppendResult(ip, tclout.str(), 0);
    return TCL_OK;
}

static int
t_vt_lock(Tcl_Interp* ip, int , char* [])
{
    vtable_info_array_t  x;
    DO(ss_m::lock_collect(x));

    __printit("locks", lock_last, x);
    // Tcl_AppendResult(ip, tclout.str(), 0);
    return TCL_OK;
}

static int
t_st_stats(Tcl_Interp *, int argc, char **)
{
	if (argc != 1)
		return TCL_ERROR;

	sthread_t::dump_stats(cout);
	return TCL_OK;
}

static int
t_st_rc(Tcl_Interp *, int argc, char **argv)
{
	if (argc < 2)
		return TCL_ERROR;

	for (int i = 1; i < argc; i++) {
		w_rc_t	e = w_rc_t("error", 0, atoi(argv[i]));
		cout << "RC(" << argv[i] << "):" <<
			endl << e << endl;
	}

	return TCL_OK;
}



struct cmd_t {
    const char* name;
    smproc_t* func;
};

#ifdef USE_COORD
static cmd_t co_cmd[] = {
    { "startns", t_startns },	// co, sub
    { "startco", t_startco },	// co
    { "startsub", t_startsub },	// co, sub
    { "sendtcl", t_co_sendtcl },// co
    { "retire", t_co_retire },	// co, sub
    { "newtid", t_co_newtid },  // co
    { "commit", t_co_commit },  // co
    { "prepare", t_co_commit },  // co -- yes, t_co_commit
    { "end_tx", t_co_end },  // co
    { "retry_commit", t_co_retry_commit },  // co
    { "addmap", t_co_addmap },  // co, sub
    { "clearmap", t_co_clearmap }, // co, sub 
    { "print", t_co_print },	// co, sub
    { "gtid2tid", t_gtid2tid },	// sub

    { "addlocal", t_local_add },
    { "rmlocal", t_local_rm },

    { "startdeadlockclient", t_startdeadlockclient },
    { "startdeadlockserver", t_startdeadlockserver },
    { "startdeadlockvictimizer", t_startdeadlockvictimizer },
    { "stopdeadlockclient", t_stopdeadlockclient },
    { "stopdeadlockserver", t_stopdeadlockserver },
    { "stopdeadlockvictimizer", t_stopdeadlockvictimizer }
};
#endif

static cmd_t cmd[] = {
    { "testing", t_testing },
    { "janet_setup", t_janet_setup },
    { "janet_lookup", t_janet_lookup },
    { "sleep", t_sleep },
    { "test_int_btree", t_test_int_btree },
    { "test_typed_btree", t_test_typed_btree },
    { "test_bulkload_int_btree", t_test_bulkload_int_btree },
    { "test_bulkload_rtree", t_test_bulkload_rtree },

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
    { "dump_xcts", t_dump_xcts},
    { "xct_to_tid", t_xct_to_tid},
    { "tid_to_xct", t_tid_to_xct},
    { "prepare_xct", t_prepare_xct},
    { "enter2pc", t_enter2pc},
    { "set_coordinator", t_set_coordinator},
    { "recover2pc", t_recover2pc},
    { "num_prepared", t_num_prepared},
    { "num_active", t_num_active},

    //
    // basic sm stuff
    //
    { "force_buffers", t_force_buffers },
    { "force_vol_hdr_buffers", t_force_vol_hdr_buffers },
    { "gather_stats", t_gather_stats },
    { "gather_xct_stats", t_gather_xct_stats },
    { "mem_stats", t_mem_stats },
    { "snapshot_buffers", t_snapshot_buffers },
    { "print_lid_index", t_print_lid_index },
    { "config_info", t_config_info },
    { "set_disk_delay", t_set_disk_delay },
    { "start_log_corruption", t_start_log_corruption },
    { "sync_log", t_sync_log },
    { "dump_buffers", t_dump_buffers },
    { "set_store_property", t_set_store_property },
    { "get_store_property", t_get_store_property },
    { "get_lock_level", t_get_lock_level },
    { "set_lock_level", t_set_lock_level },

    // Device and Volume
    { "format_dev", t_format_dev },
    { "mount_dev", t_mount_dev },
    { "unmount_dev", t_dismount_dev },
    { "unmount_all", t_dismount_all },
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
    { "get_volume_meta_stats", t_get_volume_meta_stats},
    { "get_file_meta_stats", t_get_file_meta_stats},

    { "get_du_statistics", t_get_du_statistics },   // DU DF
    //
    // Debugging
    //
    { "set_debug", t_set_debug },
    { "simulate_preemption", t_simulate_preemption },
    { "preemption_simulated", t_preemption_simulated },
    { "purify_print_string", t_purify_print_string },
    //
    //	Indices
    //
    { "create_index", t_create_index },
    { "destroy_index", t_destroy_index },
    { "destroy_md_index", t_destroy_md_index },
    { "blkld_ndx", t_blkld_ndx },
    { "create_assoc", t_create_assoc },
    { "destroy_all_assoc", t_destroy_all_assoc },
    { "destroy_assoc", t_destroy_assoc },
    { "find_assoc", t_find_assoc },
    { "find_assoc_typed", t_find_assoc_typed },
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
    { "compare_typed", t_compare_typed },
    { "create_typed_hdr_body_rec", t_create_typed_hdr_body_rec },
    { "create_typed_hdr_rec", t_create_typed_hdr_rec },
    { "get_store_info", t_get_store_info },
    { "scan_sorted_recs", t_scan_sorted_recs },
    { "print_md_index", t_print_md_index },

    { "begin_sort_stream", t_begin_sort_stream },
    { "sort_stream_put", t_sort_stream_put },
    { "sort_stream_get", t_sort_stream_get },
    { "sort_stream_end", t_sort_stream_end },

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
    { "multi_file_update", t_multi_file_update },
    { "destroy_rec", t_destroy_rec },
    { "read_rec", t_read_rec },
    { "read_hdr", t_read_hdr },
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
    { "pin_page_containing", t_page_containing },
    { "pin_pinned", t_pin_pinned },
    { "pin_is_small", t_pin_is_small },
    { "pin_large_impl", t_pin_large_impl },
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

    { "dont_escalate", t_dont_escalate },
    { "get_escalation_thresholds", t_get_escalation_thresholds },
    { "set_escalation_thresholds", t_set_escalation_thresholds },
    { "start_deadlock_info_recording", t_start_deadlock_info_recording },
    { "stop_deadlock_info_recording", t_stop_deadlock_info_recording },

#ifdef USE_COORD
    // Restart
    { "coord", t_coord }, // for 2pc recovery only
#endif
    { "restart", t_restart },
    // crash name "command"
    { "crash", t_crash },

    { "multikey_sort_file", t_multikey_sort_file },

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

static cmd_t vtable_cmd[] = {
    { "class_factory_histo", t_vt_class_factory_histo },
    { "class_factory", t_vt_class_factory },
    { "thread", t_vt_thread },
    { "lock", t_vt_lock },
    { "bp", t_vt_bpool },
    { "xct", t_vt_xct },
    { "stat", t_vt_stats },
    { "stats", t_vt_stats },

    //{ 0, 0}
};


static cmd_t	sthread_cmd[] = {
	{ "stats", t_st_stats },
	{ "rc", t_st_rc },
//	{ "events", t_st_events },
//	{ "threads", t_st_threads }
};


static int cmd_cmp(const void* c1, const void* c2) 
{
    cmd_t& cmd1 = *(cmd_t*) c1;
    cmd_t& cmd2 = *(cmd_t*) c2;
    return strcmp(cmd1.name, cmd2.name);
}

#ifndef numberof
#define	numberof(x)	(sizeof(x) / sizeof(x[0]))
#endif

void dispatch_init() 
{
    qsort( (char*)sthread_cmd, numberof(sthread_cmd), sizeof(cmd_t), cmd_cmp);
    qsort( (char*)vtable_cmd, numberof(vtable_cmd), sizeof(cmd_t), cmd_cmp);
#ifdef USE_COORD
    qsort( (char*)co_cmd, numberof(co_cmd), sizeof(cmd_t), cmd_cmp);
#endif
    qsort( (char*)cmd, numberof(cmd), sizeof(cmd_t), cmd_cmp);
}


int
_dispatch(
    cmd_t *_cmd,
    size_t sz,
    const char *module,
    Tcl_Interp* ip, int ac, char* av[]
)
{
    int ret = TCL_OK;
    cmd_t key;

    w_assert3(ac >= 2);	
    key.name = av[1];
    cmd_t* found = (cmd_t*) bsearch((char*)&key, 
				    (char*)_cmd, 
				    sz/sizeof(cmd_t), 
				    sizeof(cmd_t), cmd_cmp);
    if (found) {
        SSH_CHECK_STACK;
	ret = found->func(ip, ac - 1, av + 1);
        SSH_CHECK_STACK;
	return ret;
    }

    tclout.seekp(ios::beg);
    tclout << __FILE__ << ':' << __LINE__  << ends;
    Tcl_AppendResult(ip, tclout.str(), " unknown ", module, "  routine \"",
		     av[1], "\"", 0);
    return TCL_ERROR;
}

int st_dispatch(ClientData, Tcl_Interp *tcl, int argc, char **argv)
{
	return _dispatch(sthread_cmd, sizeof(sthread_cmd), "sthread",
			 tcl, argc, argv);
}

int
vtable_dispatch(ClientData, Tcl_Interp* ip, int ac, char* av[])
{
    return _dispatch( vtable_cmd,  sizeof(vtable_cmd), "vtable", ip, ac, av);
}

int
sm_dispatch(ClientData, Tcl_Interp* ip, int ac, char* av[])
{
    return _dispatch( cmd,  sizeof(cmd), "sm", ip, ac, av);
}

#ifdef USE_COORD
int
co_dispatch(ClientData, Tcl_Interp* ip, int ac, char* av[])
{
    return _dispatch( co_cmd,  sizeof(co_cmd), "co", ip, ac, av);
}
#endif

void check_sp(const char *file, int line)
{
    // a place to put a breakpoint in the debugger 
    bool	ok;
    ok = smthread_t::me()->isStackOK(file, line);
    if (!ok)
    	W_FATAL(fcINTERNAL);
    return;
}

smsize_t   
objectsize(const char *str)
{
    return  strtoul(str,0,0);
}

smsize_t   
numobjects(const char *str)
{
    return  strtoul(str,0,0);
}

static struct nda_t {
    const char* 	name;
ss_m::ndx_t 	type;
} nda[] = {
    { "btree",	ss_m::t_btree },
    { "uni_btree", 	ss_m::t_uni_btree },
    { "rtree",	ss_m::t_rtree },
    { "rdtree",	ss_m::t_rdtree },
    { 0,		ss_m::t_bad_ndx_t }
};

ss_m::ndx_t 
cvt2ndx_t(char* s)
{
    for (nda_t* p = nda; p->name; p++)  {
	if (streq(s, p->name))  return p->type;
    }
    return sm->t_bad_ndx_t;
}


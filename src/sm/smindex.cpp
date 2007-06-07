/*<std-header orig-src='shore'>

 $Id: smindex.cpp,v 1.99 1999/06/07 19:04:39 kupsch Exp $

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
#define SMINDEX_C
#include "sm_int_4.h"
#include "sm_du_stats.h"
#include "sm.h"

/*==============================================================*
 *  Physical ID version of all the index operations		*
 *==============================================================*/

/*********************************************************************
 *
 *  ss_m::create_index(vid, ntype, property, key_desc, stid, serial)
 *  ss_m::create_index(vid, ntype, property, key_desc, cc, stid, serial)
 *
 *********************************************************************/
rc_t
ss_m::create_index(
    vid_t 		vid, 
    ndx_t 		ntype, 
    store_property_t 	property,
    const char* 	key_desc,
    stid_t& 		stid, 
    const serial_t& 	serial
    )
{
    return 
    create_index(vid, ntype, property, key_desc, t_cc_kvl, stid, 
	serial);
}

rc_t
ss_m::create_index(
    vid_t 		vid, 
    ndx_t 		ntype, 
    store_property_t 	property,
    const char* 	key_desc,
    concurrency_t 	cc, 
    stid_t& 		stid, 
    const serial_t& 	serial
    )
{
    SM_PROLOGUE_RC(ss_m::create_index, in_xct, 0);
    SMSCRIPT(<<"create_index " 
	<<vid<<" " <<ntype<<" " <<property<<" " <<key_desc<<" "
	<<stid<<" " <<serial);
    stpgid_t stpgid;
    if(property == t_temporary) {
	return RC(eBADSTOREFLAGS);
    }
    W_DO(_create_index(vid, ntype, property, key_desc, cc,
		       false, stpgid, serial));
    stid = stpgid;

    return RCOK;
}

rc_t
ss_m::create_md_index(
    vid_t 		vid, 
    ndx_t 		ntype, 
    store_property_t 	property,
    stid_t& 		stid, 
    int2_t 		dim,
    const serial_t& 	serial
    )
{
    SM_PROLOGUE_RC(ss_m::create_md_index, in_xct, 0);
    SMSCRIPT(<<"create_index " 
	<<vid<<" " <<ntype<<" " <<property<<" " <<stid<<" "
	<<dim <<" " <<serial);
    W_DO(_create_md_index(vid, ntype, property,
			  stid, dim, serial));
    return RCOK;
}


/*--------------------------------------------------------------*
 *  ss_m::destroy_index()					*
 *--------------------------------------------------------------*/
rc_t
ss_m::destroy_index(const stid_t& iid)
{
    SM_PROLOGUE_RC(ss_m::destroy_index, in_xct, 0);
    SMSCRIPT(<<"destroy_index " <<iid);
    W_DO( _destroy_index(iid) );
    return RCOK;
}

rc_t
ss_m::destroy_md_index(const stid_t& iid)
{
    SM_PROLOGUE_RC(ss_m::destroy_md_index, in_xct, 0);
    SMSCRIPT(<<"destroy_md_index " <<iid);
    W_DO( _destroy_md_index(iid) );
    return RCOK;
}


/*--------------------------------------------------------------*
 *  ss_m::bulkld_index()					*
 *--------------------------------------------------------------*/
rc_t
ss_m::bulkld_index(
    const stid_t& 	stid, 
    int			nsrcs,
    const stid_t* 	source,
    sm_du_stats_t& 	stats,
    bool		sort_duplicates, // = true
    bool		lexify_keys // = true
    )
{
    SM_PROLOGUE_RC(ss_m::bulkld_index, in_xct, 0);
    SMSCRIPT(<<"bulkld_index " <<stid<<" "<<source<<" "<<stats);
    W_DO(_bulkld_index(stid, nsrcs, source, stats, sort_duplicates, lexify_keys) );
    return RCOK;
}

w_rc_t	ss_m::bulkld_index(
	const	stid_t	&stid,
	const	stid_t	&source,
	sm_du_stats_t	&stats,
	bool		sort_duplicates,
	bool		lexify_keys
	)
{
	return bulkld_index(stid, 1, &source, stats,
			    sort_duplicates, lexify_keys);
}

rc_t
ss_m::bulkld_md_index(
    const stid_t& 	stid, 
    int			nsrcs,
    const stid_t* 	source,
    sm_du_stats_t& 	stats,
    int2_t 		hff, 
    int2_t 		hef, 
    nbox_t* 		universe)
{
    SM_PROLOGUE_RC(ss_m::bulkld_md_index, in_xct, 0);
    SMSCRIPT(<<"bulkld_md_index " <<stid<<" "<<source<<" "<<stats
	 << " " <<hff<<" "<<hef<<" "<<universe);
    W_DO(_bulkld_md_index(stid, nsrcs, source, stats, hff, hef, universe));
    return RCOK;
}

w_rc_t	ss_m::bulkld_md_index(
	const stid_t	&stid,
	const stid_t	&source,
	sm_du_stats_t	&stats,
	int2_t		hff,
	int2_t		hef,
	nbox_t		*universe
	)
{
	return bulkld_md_index(stid, 1, &source, stats, hff, hef, universe);
}

rc_t
ss_m::bulkld_index(
    const stid_t& 	stid, 
    sort_stream_i& 	sorted_stream,
    sm_du_stats_t& 	stats)
{
    SM_PROLOGUE_RC(ss_m::bulkld_index, in_xct, 0);
    SMSCRIPT(<<"bulkld_index " <<stid<<" " << "{NOT IMPL:sorted_stream}"<<" " << stats);
    W_DO(_bulkld_index(stid, sorted_stream, stats) );
    return RCOK;
}

rc_t
ss_m::bulkld_md_index(
    const stid_t& 	stid, 
    sort_stream_i& 	sorted_stream,
    sm_du_stats_t& 	stats,
    int2_t 		hff, 
    int2_t 		hef, 
    nbox_t* 		universe)
{
    SM_PROLOGUE_RC(ss_m::bulkld_md_index, in_xct, 0);
    SMSCRIPT(<<"bulkld_md_index " <<stid<<" "<<"{NOT IMPL: sorted_stream}"<<" " << stats
	 << " " <<hff<<" "<<hef<<" "<<universe);
    W_DO(_bulkld_md_index(stid, sorted_stream, stats, hff, hef, universe));
    return RCOK;
}
    
/*--------------------------------------------------------------*
 *  ss_m::print_index()						*
 *--------------------------------------------------------------*/
rc_t
ss_m::print_index(stid_t stid)
{
    SM_PROLOGUE_RC(ss_m::print_index, in_xct, 0);
    SMSCRIPT(<<"print_index " <<stid);
    W_DO(_print_index(stid));
    return RCOK;
}

rc_t
ss_m::print_md_index(stid_t stid)
{
    SM_PROLOGUE_RC(ss_m::print_index, in_xct, 0);
    SMSCRIPT(<<"print_md_index " <<stid);
    W_DO(_print_md_index(stid));
    return RCOK;
}

/*--------------------------------------------------------------*
 *  ss_m::create_assoc()					*
 *--------------------------------------------------------------*/
rc_t
ss_m::create_assoc(stid_t stid, const vec_t& key, const vec_t& el
	)
{
    SM_PROLOGUE_RC(ss_m::create_assoc, in_xct, 0);
    SMSCRIPT(<<"create_assoc " <<stid <<" " << key <<" "<<el);
    W_DO(_create_assoc(stid, key, el));
    return RCOK;
}

/*--------------------------------------------------------------*
 *  ss_m::destroy_assoc()					*
 *--------------------------------------------------------------*/
rc_t
ss_m::destroy_assoc(stid_t stid, const vec_t& key, const vec_t& el
)
{
    SM_PROLOGUE_RC(ss_m::destroy_assoc, in_xct, 0);
    SMSCRIPT(<<"destroy_assoc " <<stid << " " <<key << " "<<el);
    W_DO(_destroy_assoc(stid, key, el));
    return RCOK;
}



/*--------------------------------------------------------------*
 *  ss_m::destroy_all_assoc()					*
 *--------------------------------------------------------------*/
rc_t
ss_m::destroy_all_assoc(stid_t stid, const vec_t& key, int& num)
{
    SM_PROLOGUE_RC(ss_m::destroy_assoc, in_xct, 0);
    RES_SMSCRIPT(<<"destroy_all_assoc " <<stid <<" "<< key );
    W_DO(_destroy_all_assoc(stid, key, num));
    return RCOK;
}

/*--------------------------------------------------------------*
 *  ss_m::find_assoc()						*
 *--------------------------------------------------------------*/
rc_t
ss_m::find_assoc(stid_t stid, const vec_t& key, 
	    	void* el, smsize_t& elen, bool& found
	      )
{
    SM_PROLOGUE_RC(ss_m::find_assoc, in_xct, 0);
    RES_SMSCRIPT(<<"find_assoc " <<stid <<" " << key << " " << el );
    W_DO(_find_assoc(stid, key, el, elen, found));
    return RCOK;
}

/*--------------------------------------------------------------*
 *  ss_m::create_md_assoc()					*
 *--------------------------------------------------------------*/
rc_t
ss_m::create_md_assoc(stid_t stid, const nbox_t& key, const vec_t& el)
{
    SM_PROLOGUE_RC(ss_m::create_md_assoc, in_xct, 0);
    W_DO(_create_md_assoc(stid, key, el));
    return RCOK;
}


/*--------------------------------------------------------------*
 *  ss_m::find_md_assoc()					*
 *--------------------------------------------------------------*/
rc_t
ss_m::find_md_assoc(stid_t stid, const nbox_t& key,
                    void* el, smsize_t& elen, bool& found)
{
    SM_PROLOGUE_RC(ss_m::find_assoc, in_xct, 0);
    W_DO(_find_md_assoc(stid, key, el, elen, found));
    return RCOK;
}


/*--------------------------------------------------------------*
 *  ss_m::destroy_md_assoc()                                    *
 *--------------------------------------------------------------*/
rc_t
ss_m::destroy_md_assoc(stid_t stid, const nbox_t& key, const vec_t& el)
{
    SM_PROLOGUE_RC(ss_m::destroy_md_assoc, in_xct, 0);
    W_DO(_destroy_md_assoc(stid, key, el));
    return RCOK;
}


/*--------------------------------------------------------------*
 *  ss_m::draw_rtree()						*
 *--------------------------------------------------------------*/
rc_t
ss_m::draw_rtree(const stid_t& stid, ostream &s)
{
    SM_PROLOGUE_RC(ss_m::draw_rtree, in_xct, 0);
    W_DO(_draw_rtree(stid, s));
    return RCOK;
}

/*--------------------------------------------------------------*
 *  ss_m::rtree_stats()						*
 *--------------------------------------------------------------*/
rc_t
ss_m::rtree_stats(const stid_t& stid, rtree_stats_t& stat, 
		uint2_t size, uint2_t* ovp, bool audit)
{
    SM_PROLOGUE_RC(ss_m::rtree_stats, in_xct, 0);
    W_DO(_rtree_stats(stid, stat, size, ovp, audit));
    return RCOK;
}

/*==============================================================*
 * Logical ID version of all the index operations		*
 *==============================================================*/

rc_t
ss_m::print_lid_index(const lvid_t& lvid)
{
//    SM_PROLOGUE_RC(ss_m::print_lid_index, not_in_xct, 0);
    SMSCRIPT(<<"print_lid_index " <<lvid);
    W_DO( lid->print_index(lvid) );
    return RCOK;
}
 

/*--------------------------------------------------------------*
 *  ss_m::create_index()					*
 *--------------------------------------------------------------*/
rc_t
ss_m::create_index(const lvid_t& lvid, ndx_t ntype,
		   store_property_t property,
		   const char* key_desc,
		   uint size_kb_hint,
		   serial_t& lstid
		   )
{
    return 
    create_index(lvid, ntype, property, key_desc, t_cc_kvl,
	size_kb_hint, lstid);
}

/*--------------------------------------------------------------*
 *  ss_m::create_index()					*
 *--------------------------------------------------------------*/
rc_t
ss_m::create_index(const lvid_t& lvid, ndx_t ntype,
		   store_property_t property,
		   const char* key_desc,
		   concurrency_t cc,
		   uint size_kb_hint,
		   serial_t& lstid
		   )
{
    SM_PROLOGUE_RC(ss_m::create_index, in_xct, 0);
    SMSCRIPT(<<"create_index " 
	<<lvid<<" " <<ntype<<" " <<property<<" " <<key_desc<<" "
	<<size_kb_hint<<" " <<lstid);
    vid_t  vid;  // physical volume ID (returned by generate_new_serial)

    W_DO(lid->generate_new_serials(lvid, vid, 1, 
				   lstid, lid_m::local_ref));

    //
    // usually, we want to start out with storing the btree in
    // a 1-page store and then growing it.  However, if the
    // btree is to be temporary or have no logging, we can't
    // do that (without a lot of hacking), so we start it large.
    //
    // The index also starts out large if the size hint is greater
    // than 1 page
    //
    bool use_1page_store = ((property == t_regular) &&
			    (size_kb_hint < page_sz/1024));

    //TODO: begin rollback
    DBG( << "TODO: put in rollback mechanism" );
    stpgid_t stpgid;
    W_DO(_create_index(vid, ntype, property,
		       key_desc, cc, use_1page_store, stpgid, lstid));
    W_DO(lid->associate(lvid, lstid, stpgid));

    return RCOK;
}

rc_t
ss_m::create_md_index(const lvid_t& lvid, ndx_t ntype,
		      store_property_t property,
		      serial_t& lstid, int2_t dim
		      )
{
    SM_PROLOGUE_RC(ss_m::create_md_index, in_xct, 0);
    vid_t  vid;  // physical volume ID (returned by generate_new_serial)
    stid_t stid;  // physical file ID

    W_DO(lid->generate_new_serials(lvid, vid, 1, 
				   lstid, lid_m::local_ref));

    //TODO: begin rollback
    DBG( << "TODO: put in rollback mechanism" );
    W_DO(_create_md_index(vid, ntype, property,
			  stid, dim, lstid));
    W_DO(lid->associate(lvid, lstid, stid));

    return RCOK;
}

/*--------------------------------------------------------------*
 *  ss_m::destroy_index()					*
 *--------------------------------------------------------------*/
rc_t
ss_m::destroy_index(const lvid_t& lvid, const serial_t& serial)
{
    SM_PROLOGUE_RC(ss_m::destroy_index, in_xct, 0);
    SMSCRIPT(<<"destroy_index " <<lvid<<" " <<serial);
    stpgid_t iid;  // physical file ID

    lid_t id(lvid, serial);
    LID_CACHE_RETRY_DO(id, stid_t, iid, _destroy_index(iid));

    //TODO: begin rollback
    DBG( << "TODO: put in rollback mechanism" );
    W_DO(lid->remove(lvid, serial));

    return RCOK;
}


rc_t
ss_m::destroy_md_index(const lvid_t& lvid, const serial_t& serial)
{
    SM_PROLOGUE_RC(ss_m::destroy_md_index, in_xct, 0);
    stid_t iid;  // physical file ID

    lid_t id(lvid, serial);
    LID_CACHE_RETRY_DO(id, stid_t, iid, _destroy_md_index(iid));

    //TODO: begin rollback
    DBG( << "TODO: put in rollback mechanism" );
    W_DO(lid->remove(lvid, serial));

    return RCOK;
}

rc_t
ss_m::_destroy_md_assoc(stid_t stid, const nbox_t& key, const vec_t& el)
{
    sdesc_t* sd;
    W_DO( dir->access(lpid_t(stid,0), sd, IX) );

    if (sd->sinfo().stype != t_index)   return RC(eBADSTORETYPE);
    switch (sd->sinfo().ntype) {
      case t_bad_ndx_t:
        return RC(eBADNDXTYPE);
      case t_rtree:
        W_DO( rt->remove(sd->root(), key, el) );
        break;
      case t_rdtree:
      case t_btree:
      case t_uni_btree:
      case t_lhash:
        return RC(eBADNDXTYPE);
      default:
        W_FATAL(eINTERNAL);
    }

    return RCOK;
}


/*--------------------------------------------------------------*
 *  ss_m::bulkld_index()					*
 *--------------------------------------------------------------*/
rc_t
ss_m::bulkld_index(const lvid_t& lvid, const serial_t& serial, 
		   int nsrcs,
		   const lvid_t* s_lvid, const serial_t* s_serial,
		   sm_du_stats_t& stats,
		   bool	sort_duplicates, // = true
		   bool	lexify_keys // = true
		   )
{
    SM_PROLOGUE_RC(ss_m::bulkld_index, in_xct, 0);
    SMSCRIPT(<<"bulkld_index " <<lvid <<" " <<serial <<" " <<s_lvid <<" " <<s_serial <<" " <<stats);
    
    stpgid_t stpgid; 
    lid_t  id(lvid, serial);
    LID_CACHE_RETRY_VALIDATE_STPGID_DO(id, stpgid);

    stid_t* srcs = new stid_t[nsrcs];
    for(int i=0; i< nsrcs; i++) {
	lid_t s_id(s_lvid[i], s_serial[i]);
	LID_CACHE_RETRY_VALIDATE_STID_DO(s_id, srcs[i]);
    }


    // see if btree is a real store or a 1 page btree 
    // convert to store if necessary
    stid_t new_stid;
    if (stpgid.is_stid()) {
	// real store, so no conversion necessary
	new_stid = stpgid;
    } else {
	// Convert the index to be loaded into one located on a 
	// real (rather than 1page) store.
	W_DO(_convert_to_store(id, stpgid, new_stid));
    }
    w_rc_t rc = 
    _bulkld_index(new_stid, nsrcs, srcs, stats, sort_duplicates, lexify_keys);
    delete[] srcs;
    W_DO(rc);
    return rc;
}

w_rc_t ss_m::bulkld_index(
	const lvid_t	&lvid,
	const serial_t	&serial, 
	const lvid_t	&s_lvid,
	const serial_t	&s_serial,
	sm_du_stats_t	&stats,
	bool		sort_duplicates,
	bool		lexify_keys
	)
{
	return bulkld_index(lvid, serial, 
			    1, &s_lvid, &s_serial,
			    stats, sort_duplicates, lexify_keys);
}

rc_t
ss_m::bulkld_md_index(const lvid_t& lvid, const serial_t& serial, 
		      int nsrcs,
		      const lvid_t* s_lvid, const serial_t* s_serial,
		      sm_du_stats_t& stats,
		      int2_t hff, int2_t hef, nbox_t* universe)
{
    SM_PROLOGUE_RC(ss_m::bulkld_md_index, in_xct, 0);
    
    stid_t stid; 
    lid_t  id(lvid, serial);
    LID_CACHE_RETRY_VALIDATE_STID_DO(id, stid);

    stid_t* srcs = new stid_t[nsrcs];
    for(int i=0; i< nsrcs; i++) {
	lid_t s_id(s_lvid[i], s_serial[i]);
	LID_CACHE_RETRY_VALIDATE_STID_DO(s_id, srcs[i]);
    }

    w_rc_t rc=
    _bulkld_md_index(stid, nsrcs, srcs, stats, hff, hef, universe);
    delete[] srcs;
    W_DO(rc);

    return RCOK;
}

w_rc_t ss_m::bulkld_md_index(
	const lvid_t	&lvid,
	const serial_t	&serial, 
	const lvid_t	&s_lvid,
	const serial_t	&s_serial,
	sm_du_stats_t	&stats,
	int2_t 		hff,
	int2_t		hef,
	nbox_t		*universe
	)
{
	return bulkld_md_index(lvid, serial,
			       1, &s_lvid, &s_serial,
			       stats, hff, hef, universe);
}

rc_t
ss_m::bulkld_index(const lvid_t& lvid, const serial_t& serial, 
		   sort_stream_i& sorted_stream,
		   sm_du_stats_t& stats)
{
    SM_PROLOGUE_RC(ss_m::bulkld_index, in_xct, 0);
    
    stpgid_t stpgid; 
    lid_t  id(lvid, serial);
    LID_CACHE_RETRY_VALIDATE_STPGID_DO(id, stpgid);

    // Convert the index to be loaded into one located on a 
    // real (rather than 1page) store.
    stid_t new_stid;
    if (stpgid.is_stid()) {
	// real store, so no conversion necessary
	new_stid = stpgid;
    } else {
	// Convert the index to be loaded into one located on a 
	// real (rather than 1page) store.
	W_DO(_convert_to_store(id, stpgid, new_stid));
    }

    W_DO(_bulkld_index(new_stid, sorted_stream, stats) );

    return RCOK;
}

rc_t
ss_m::bulkld_md_index(const lvid_t& lvid, const serial_t& serial, 
		      sort_stream_i& sorted_stream,
		      sm_du_stats_t& stats,
		      int2_t hff, int2_t hef, nbox_t* universe)
{
    SM_PROLOGUE_RC(ss_m::bulkld_md_index, in_xct, 0);
    
    stid_t stid; 
    lid_t  id(lvid, serial);
    LID_CACHE_RETRY_VALIDATE_STID_DO(id, stid);

    W_DO(_bulkld_md_index(stid, sorted_stream, stats, hff, hef, universe) );

    return RCOK;
}

/*--------------------------------------------------------------*
 *  ss_m::print_index()						*
 *--------------------------------------------------------------*/
rc_t
ss_m::print_index(const lvid_t& lvid, const serial_t& serial)
{
    SM_PROLOGUE_RC(ss_m::print_index, in_xct, 0);
    SMSCRIPT(<<"print_index " <<lvid <<" " <<serial);
    stpgid_t stpgid; 
    lid_t  id(lvid, serial);

    LID_CACHE_RETRY_DO(id, stpgid_t, stpgid, _print_index(stpgid));
    return RCOK;
}

rc_t
ss_m::print_md_index(const lvid_t& lvid, const serial_t& serial)
{
    SM_PROLOGUE_RC(ss_m::print_md_index, in_xct, 0);
    stid_t stid; 
    lid_t  id(lvid, serial);

    LID_CACHE_RETRY_DO(id, stid_t, stid, _print_md_index(stid));
    return RCOK;
}


/*--------------------------------------------------------------*
 *  ss_m::create_assoc()					*
 *--------------------------------------------------------------*/
rc_t
ss_m::create_assoc(const lvid_t& lvid, const serial_t& serial,
		   const vec_t& key, const vec_t& el
		   )
{
    SM_PROLOGUE_RC(ss_m::create_assoc, in_xct, 0);
    SMSCRIPT(<<"create_assoc " <<lvid <<" " <<serial
	<<" " << key << " "<<el);
    stpgid_t stpgid; 
    lid_t  id(lvid, serial);

    rc_t rc;
    LID_CACHE_RETRY(id, stpgid_t, stpgid, rc,
			_create_assoc(stpgid, key, el));
    if (rc && rc.err_num() == e1PAGESTORELIMIT) {
	stid_t new_stid;
	W_DO(_convert_to_store(id, stpgid, new_stid));

	W_DO(_create_assoc(new_stid, key, el));
    } else {
	if (rc) return rc;
    }
    return RCOK;
}

/*-------------------------------------------------------------*
 *  ss_m::create_md_assoc()					*
 *--------------------------------------------------------------*/
rc_t
ss_m::create_md_assoc(const lvid_t& lvid, const serial_t& serial,
		      const nbox_t& key, const vec_t& el)
{
    SM_PROLOGUE_RC(ss_m::create_md_assoc, in_xct, 0);
    stid_t stid; 
    lid_t  id(lvid, serial);

    LID_CACHE_RETRY_DO(id, stid_t, stid, _create_md_assoc(stid, key, el));
    return RCOK;
}

/*--------------------------------------------------------------*
 *  ss_m::destroy_assoc()					*
 *--------------------------------------------------------------*/
rc_t
ss_m::destroy_assoc(const lvid_t& lvid, const serial_t& serial,
		    const vec_t& key, const vec_t& el
		    )
{
    SM_PROLOGUE_RC(ss_m::destroy_assoc, in_xct, 0);
    SMSCRIPT(<<"destroy_assoc " <<lvid <<" " <<serial
	<<" " << key << " "<<el);
    stpgid_t stpgid; 
    lid_t  id(lvid, serial);

    LID_CACHE_RETRY_DO(id, stpgid_t, stpgid,
		       _destroy_assoc(stpgid, key, el));
    return RCOK;
}


/*--------------------------------------------------------------*
 *  ss_m::destroy_all_assoc()					*
 *--------------------------------------------------------------*/
rc_t
ss_m::destroy_all_assoc(const lvid_t& lvid, const serial_t& serial,
			const vec_t& key, int& num
			)
{
    SM_PROLOGUE_RC(ss_m::destroy_all_assoc, in_xct, 0);
    RES_SMSCRIPT(<<"destroy_all_assoc " <<lvid <<" " <<serial
	<<" " << key );
    stpgid_t stpgid; 
    lid_t  id(lvid, serial);

    LID_CACHE_RETRY_DO(id, stpgid_t, stpgid,
		       _destroy_all_assoc(stpgid, key, num));
    return RCOK;
}

/*--------------------------------------------------------------*
 *  ss_m::find_assoc()						*
 *--------------------------------------------------------------*/
rc_t
ss_m::find_assoc(const lvid_t& lvid, const serial_t& serial,
		 const vec_t& key, void* el, smsize_t& elen, 
		 bool& found
		 )
{
    SM_PROLOGUE_RC(ss_m::find_assoc, in_xct, 0);
    RES_SMSCRIPT(<<"find_assoc " <<lvid <<" " <<serial
	<<" " << key );
    stpgid_t stpgid; 
    lid_t  id(lvid, serial);

    LID_CACHE_RETRY_DO(id, stpgid_t, stpgid, 
		       _find_assoc(stpgid, key, el, elen, found));
    return RCOK;
}

/*--------------------------------------------------------------*
 *  ss_m::find_md_assoc()						*
 *--------------------------------------------------------------*/
rc_t
ss_m::find_md_assoc(const lvid_t& lvid, const serial_t& serial,
		 const nbox_t& key, void* el, smsize_t& elen, 
		bool& found
		)
{
    SM_PROLOGUE_RC(ss_m::find_md_assoc, in_xct, 0);
    stid_t stid; 
    lid_t  id(lvid, serial);
    LID_CACHE_RETRY_DO(id, stid_t, stid, 
		       _find_md_assoc(stid, key, el, elen, found));
    return RCOK;
}


/*--------------------------------------------------------------*
 *  ss_m::destroy_md_assoc()                                    *
 *--------------------------------------------------------------*/
rc_t
ss_m::destroy_md_assoc(const lvid_t& lvid, const serial_t& serial,
                     const nbox_t& key, const vec_t& el)
{
    SM_PROLOGUE_RC(ss_m::destroy_md_assoc, in_xct, 0);

    stid_t stid; 
    lid_t  id(lvid, serial);
    LID_CACHE_RETRY_DO(id, stid_t, stid, _destroy_md_assoc(stid, key, el));
    return RCOK;
}

/*--------------------------------------------------------------*
 *  ss_m::draw_rtree()						*
 *--------------------------------------------------------------*/
rc_t
ss_m::draw_rtree(const lvid_t& lvid, const serial_t& serial, ostream &s)
{
    SM_PROLOGUE_RC(ss_m::draw_rtree, in_xct, 0);
    stid_t stid; 
    lid_t  id(lvid, serial);

    LID_CACHE_RETRY_DO(id, stid_t, stid, _draw_rtree(stid, s));
    return RCOK;
}

/*--------------------------------------------------------------*
 *  ss_m::rtree_stats()						*
 *--------------------------------------------------------------*/
rc_t
ss_m::rtree_stats(const lvid_t& lvid, const serial_t& serial,
		  rtree_stats_t& stat, uint2_t size, uint2_t* ovp,
		  bool audit)
{
    SM_PROLOGUE_RC(ss_m::rtree_stats, in_xct, 0);
    stid_t stid; 
    lid_t  id(lvid, serial);

    LID_CACHE_RETRY_DO(id, stid_t, stid, _rtree_stats(stid, stat, size, ovp, audit));
    return RCOK;
}

/*--------------------------------------------------------------*
 *  ss_m::_create_index()					*
 *--------------------------------------------------------------*/
rc_t
ss_m::_create_index(
    vid_t 		vid, 
    ndx_t 		ntype, 
    store_property_t 	property,
    const char* 	key_desc,
    concurrency_t       cc, // = t_cc_kvl
    bool 		use_1page_store,
    stpgid_t& 		stpgid,
    const serial_t& 	logical_id
    )
{
    FUNC(ss_m::_create_index);
    uint4_t count = max_keycomp;
    key_type_s kcomp[max_keycomp];
    lpid_t root;

    W_DO( key_type_s::parse_key_type(key_desc, count, kcomp) );


    if (use_1page_store) {
	DBG(<< "creating a 1-page btree");
	W_DO(io->alloc_pages(stid_t(vid, store_id_1page), 
			     lpid_t::null, // near hint
			     1, &root, // npages, array for output pids
			     true, 	// may_realloc
			     NL,
			     true));
	stpgid = stpgid_t(root);
    } else {
	stid_t stid;
	W_DO( io->create_store(vid, 100/*unused*/, _make_store_flag(property), stid) );
	stpgid = stpgid_t(stid);
    }

    // Note: theoretically, some other thread could destroy
    //       the above store before the following lock request
    //       is granted.  The only forseable way for this to
    //       happen would be due to a bug in a vas causing
    //       it to destroy the wrong store.  We make no attempt
    //       to prevent this.
    W_DO(lm->lock(stpgid, EX, t_long, WAIT_SPECIFIED_BY_XCT));

    if( (cc != t_cc_none) && (cc != t_cc_file) &&
	(cc != t_cc_kvl) && (cc != t_cc_modkvl) &&
	(cc != t_cc_im)
	) return RC(eBADCCLEVEL);

    switch (ntype)  {
    case t_btree:
    case t_uni_btree:
	// compress prefixes only if the first part is compressed
	W_DO( bt->create(stpgid, root, kcomp[0].compressed != 0) );

	break;
    default:
	return RC(eBADNDXTYPE);
    }
    sinfo_s sinfo(stpgid.store(), t_index, 100/*unused*/, 
		  ntype,
		  cc,
		  root.page, logical_id, 
		  count, kcomp);
    W_DO( dir->insert(stpgid, sinfo) );

    return RCOK;
}

rc_t
ss_m::_create_md_index(
    vid_t 		vid, 
    ndx_t 		ntype, 
    store_property_t 	property,
    stid_t& 		stid, 
    int2_t 		dim,
    const serial_t& 	logical_id
    )
{
    W_DO( io->create_store(vid, 100/*unused*/, 
			   _make_store_flag(property), stid) );

    lpid_t root;

    // Note: theoretically, some other thread could destroy
    //       the above store before the following lock request
    //       is granted.  The only forseable way for this to
    //       happen would be due to a bug in a vas causing
    //       it to destroy the wrong store.  We make no attempt
    //       to prevent this.
    W_DO(lm->lock(stid, EX, t_long, WAIT_SPECIFIED_BY_XCT));

    switch (ntype)  {
    case t_rtree:
	W_DO( rt->create(stid, root, dim) );
	break;
    default:
	return RC(eBADNDXTYPE);
    }

    sinfo_s sinfo(stid.store, t_index, 100/*unused*/, 
	  	  ntype, t_cc_none, // cc not used for md indexes
		  root.page, logical_id, 
		  0, 0);
    W_DO( dir->insert(stpgid_t(stid), sinfo) );

    return RCOK;
}

/*--------------------------------------------------------------*
 *  ss_m::_destroy_index()					*
 *--------------------------------------------------------------*/
rc_t
ss_m::_destroy_index(const stpgid_t& iid)
{
    sdesc_t* sd;
    W_DO( dir->access(iid, sd, EX) );

    if (sd->sinfo().stype != t_index)   return RC(eBADSTORETYPE);
    switch (sd->sinfo().ntype)  {
    case t_btree:
    case t_uni_btree:
	if (iid.is_stid()) {
	    W_DO( io->destroy_store(iid) );
	} else {
	    W_DO( io->free_page(iid.lpid));
	}
	break;
    default:
	return RC(eBADNDXTYPE);
    }
    
    W_DO( dir->remove(iid) );
    return RCOK;
}

rc_t
ss_m::_destroy_md_index(const stid_t& iid)
{
    sdesc_t* sd;
    W_DO( dir->access(iid, sd, EX) );

    if (sd->sinfo().stype != t_index)   return RC(eBADSTORETYPE);
    switch (sd->sinfo().ntype)  {
    case t_rtree:
	W_DO( io->destroy_store(iid) );
	break;
    default:
	return RC(eBADNDXTYPE);
    }
    
    W_DO( dir->remove(iid) );
    return RCOK;
}

/*--------------------------------------------------------------*
 *  ss_m::_bulkld_index()					*
 *--------------------------------------------------------------*/

rc_t
ss_m::_bulkld_index(
    const stid_t& 	stid,
    int			nsrcs,
    const stid_t*	source,
    sm_du_stats_t& 	stats,
    bool		 sort_duplicates, //  = true
    bool		 lexify_keys //  = true
    )
{
    sdesc_t* sd;
    W_DO( dir->access(stid, sd, EX ) );

    if (sd->sinfo().stype != t_index)   return RC(eBADSTORETYPE);
    switch (sd->sinfo().ntype) {
    case t_btree:
    case t_uni_btree:
        DBG(<<"bulk loading root " << sd->root());
	W_DO( bt->bulk_load(sd->root(), 
	    nsrcs,
	    source,
	    sd->sinfo().nkc, sd->sinfo().kc,
	    sd->sinfo().ntype == t_uni_btree, 
	    (concurrency_t)sd->sinfo().cc,
	    stats.btree,
	    sort_duplicates,
	    lexify_keys
	    ) );
	break;
    default:
	return RC(eBADNDXTYPE);
    }
    {
	store_flag_t st;
	W_DO( io->get_store_flags(stid, st) );
	w_assert3(st != st_bad);
	if(st & (st_tmp|st_insert_file|st_load_file)) {
	    DBG(<<"converting stid " << stid <<
		" from " << st << " to st_regular " );
	    // After bulk load, it MUST be re-converted
	    // to regular to prevent unlogged arbitrary inserts
	    // Invalidate the pages so the store flags get reset
	    // when the pages are read back in
	    W_DO( io->set_store_flags(stid, st_regular) );
	    W_DO( bf->force_store(stid, true /*invalidate*/) );
	}
    }
    return RCOK;
}

rc_t
ss_m::_bulkld_md_index(
    const stid_t& 	stid, 
    int			nsrcs,
    const stid_t* 	source, 
    sm_du_stats_t& 	stats,
    int2_t 		hff, 
    int2_t 		hef, 
    nbox_t* 		universe)
{
    sdesc_t* sd;
    W_DO( dir->access(stid, sd, EX) );

    if (sd->sinfo().stype != t_index)   return RC(eBADSTORETYPE);
    switch (sd->sinfo().ntype) {
    case t_rtree:
	{
	    rtld_desc_t desc(universe,hff,hef);
	    W_DO( rt->bulk_load(sd->root(), nsrcs, source, desc, stats.rtree) ); 
	}
	break;
    case t_rdtree:
	return RC(eNOTIMPLEMENTED);
    default:
	return RC(eBADNDXTYPE);
    }
    {
	store_flag_t st;
	W_DO( io->get_store_flags(stid, st) );
	if(st & (st_tmp|st_insert_file|st_load_file)) {
	    // After bulk load, it MUST be re-converted
	    // to regular to prevent unlogged arbitrary inserts
	    // Invalidate the pages so the store flags get reset
	    // when the pages are read back in
	    W_DO( io->set_store_flags(stid, st_regular) );
	    W_DO( bf->force_store(stid, true /*invalidate*/) );
	}
    }

    return RCOK;
}

rc_t
ss_m::_bulkld_index(
    const stid_t& 	stid, 
    sort_stream_i& 	sorted_stream, 
    sm_du_stats_t& 	stats
    )
{
    sdesc_t* sd;
    W_DO( dir->access(stid, sd, EX) );

    if (sd->sinfo().stype != t_index)   return RC(eBADSTORETYPE);
    switch (sd->sinfo().ntype) {
    case t_btree:
    case t_uni_btree:
	W_DO( bt->bulk_load(sd->root(), sorted_stream,
			    sd->sinfo().nkc, sd->sinfo().kc,
			    sd->sinfo().ntype == t_uni_btree, 
			    (concurrency_t)sd->sinfo().cc, stats.btree) );
	break;
    default:
	return RC(eBADNDXTYPE);
    }

    return RCOK;
}

rc_t
ss_m::_bulkld_md_index(
    const stid_t& 	stid, 
    sort_stream_i& 	sorted_stream, 
    sm_du_stats_t&	stats,
    int2_t 		hff, 
    int2_t 		hef, 
    nbox_t* 		universe)
{
    sdesc_t* sd;
    W_DO( dir->access(stid, sd, EX) );

    if (sd->sinfo().stype != t_index)   return RC(eBADSTORETYPE);
    switch (sd->sinfo().ntype) {
    case t_rtree:
	{
	    rtld_desc_t desc(universe,hff,hef);
	    W_DO( rt->bulk_load(sd->root(), sorted_stream, desc, stats.rtree) );
	}
	break;
    case t_rdtree:
	return RC(eNOTIMPLEMENTED);
    default:
	return RC(eBADNDXTYPE);
    }

    return RCOK;
}

/*--------------------------------------------------------------*
 *  ss_m::_print_index()					*
 *--------------------------------------------------------------*/
rc_t
ss_m::_print_index(stpgid_t stpgid)
{
    sdesc_t* sd;
    W_DO( dir->access(stpgid, sd, IS) );

    if (sd->sinfo().stype != t_index)   return RC(eBADSTORETYPE);
    if (sd->sinfo().nkc > 1) {
	//can't handle multi-part keys
	return RC(eNOTIMPLEMENTED);
    }
    sortorder::keytype k = sortorder::convert(sd->sinfo().kc);
    switch (sd->sinfo().ntype) {
    case t_btree:
    case t_uni_btree:
	bt->print(sd->root(), k);
	break;
    case t_lhash:
	return RC(eNOTIMPLEMENTED);
    default:
	return RC(eBADNDXTYPE);
    }

    return RCOK;
}

rc_t
ss_m::_print_md_index(stid_t stid)
{
    sdesc_t* sd;
    W_DO( dir->access(stid, sd, IS) );

    if (sd->sinfo().stype != t_index)   return RC(eBADSTORETYPE);
    switch (sd->sinfo().ntype) {
    case t_rtree:
	W_DO( rt->print(sd->root()) );
	break;
    default:
	return RC(eBADNDXTYPE);
    }

    return RCOK;
}

/*--------------------------------------------------------------*
 *  ss_m::_create_assoc()					*
 *--------------------------------------------------------------*/
rc_t
ss_m::_create_assoc(
    const stpgid_t& 	stpgid, 
    const vec_t& 	key, 
    const vec_t& 	el
)
{
    // usually we will do kvl locking and already have an IX lock
    // on the index
    lock_mode_t		index_mode = NL;// lock mode needed on index

    // determine if we need to change the settins of cc and index_mode
    concurrency_t cc = t_cc_bad;
    xct_t* xd = xct();
    if (xd)  {
	lock_mode_t lock_mode;
	W_DO( lm->query(stpgid, lock_mode, xd->tid(), true) );
	// cc is off if file is EX/SH/UD/SIX locked
	if (lock_mode == EX) {
	    cc = t_cc_none;
	} else if (lock_mode == IX || lock_mode >= SIX) {
	    // no changes needed
	} else {
	    index_mode = IX;
	}
    }

    sdesc_t* sd;
    W_DO( dir->access(stpgid, sd, index_mode) );

    if (sd->sinfo().stype != t_index)   return RC(eBADSTORETYPE);
    if (cc == t_cc_bad ) cc = (concurrency_t)sd->sinfo().cc;

    switch (sd->sinfo().ntype) {
    case t_bad_ndx_t:
	return RC(eBADNDXTYPE);
    case t_btree:
    case t_uni_btree:
	W_DO( bt->insert(sd->root(), 
		     sd->sinfo().nkc, sd->sinfo().kc,
		     sd->sinfo().ntype == t_uni_btree, 
		     cc,
		     key, el, 50) );
	break;
    case t_rtree:
    case t_rdtree:
    case t_lhash:
	return RC(eNOTIMPLEMENTED);
    default:
	W_FATAL(eINTERNAL);
    }

    return RCOK;
}

/*--------------------------------------------------------------*
 *  ss_m::_destroy_assoc()					*
 *--------------------------------------------------------------*/
rc_t
ss_m::_destroy_assoc(
    const stpgid_t& 	stpgid, 
    const vec_t& 	key, 
    const vec_t& 	el
    )
{
    concurrency_t cc = t_cc_bad;
    // usually we will to kvl locking and already have an IX lock
    // on the index
    lock_mode_t		index_mode = NL;// lock mode needed on index

    // determine if we need to change the settins of cc and index_mode
    xct_t* xd = xct();
    if (xd)  {
	lock_mode_t lock_mode;
	W_DO( lm->query(stpgid, lock_mode, xd->tid(), true) );
	// cc is off if file is EX/SH/UD/SIX locked
	if (lock_mode == EX) {
	    cc = t_cc_none;
	} else if (lock_mode == IX || lock_mode >= SIX) {
	    // no changes needed
	} else {
	    index_mode = IX;
	}
    }
    DBG(<<"");

    sdesc_t* sd;
    W_DO( dir->access(stpgid, sd, index_mode) );
    DBG(<<"");

    if (sd->sinfo().stype != t_index)   return RC(eBADSTORETYPE);
    if (cc == t_cc_bad ) cc = (concurrency_t)sd->sinfo().cc;

    switch (sd->sinfo().ntype) {
    case t_bad_ndx_t:
	return RC(eBADNDXTYPE);
    case t_btree:
    case t_uni_btree:
	W_DO( bt->remove(sd->root(), 
		 sd->sinfo().nkc, sd->sinfo().kc,
		 sd->sinfo().ntype == t_uni_btree,
		 cc, key, el) );
	break;
    case t_rtree:
    case t_rdtree:
    case t_lhash:
	return RC(eNOTIMPLEMENTED);
    default:
	W_FATAL(eINTERNAL);
    }
    DBG(<<"");
    return RCOK;
}

/*--------------------------------------------------------------*
 *  ss_m::_destroy_all_assoc()					*
 *--------------------------------------------------------------*/
rc_t
ss_m::_destroy_all_assoc(const stpgid_t& stpgid, const vec_t& key, 
	int& num
	)
{
    sdesc_t* sd;
    W_DO( dir->access(stpgid, sd, IX) );

    if (sd->sinfo().stype != t_index)   return RC(eBADSTORETYPE);
    concurrency_t cc = (concurrency_t)sd->sinfo().cc;

    xct_t* xd = xct();
    if (xd)  {
	lock_mode_t lock_mode;
	W_DO( lm->query(stpgid, lock_mode, xd->tid(), true) );
	// cc is off if file is EX locked
	if (lock_mode == EX) cc = t_cc_none;
    }
    switch (sd->sinfo().ntype) {
    case t_bad_ndx_t:
	return RC(eBADNDXTYPE);
    case t_btree:
    case t_uni_btree:
	W_DO( bt->remove_key(sd->root(), 
		     sd->sinfo().nkc, sd->sinfo().kc,
		     sd->sinfo().ntype == t_uni_btree,
		     cc, key, num) );
	break;
    case t_rtree:
    case t_rdtree:
    case t_lhash:
	return RC(eNOTIMPLEMENTED);
    default:
	W_FATAL(eINTERNAL);
    }

    return RCOK;
}

/*--------------------------------------------------------------*
 *  ss_m::_find_assoc()						*
 *--------------------------------------------------------------*/
rc_t
ss_m::_find_assoc(
    const stpgid_t& 	stpgid, 
    const vec_t& 	key, 
    void* 		el, 
    smsize_t& 		elen, 
    bool& 		found
    )
{
    concurrency_t cc = t_cc_bad;
    // usually we will to kvl locking and already have an IS lock
    // on the index
    lock_mode_t		index_mode = NL;// lock mode needed on index

    // determine if we need to change the settins of cc and index_mode
    xct_t* xd = xct();
    if (xd)  {
	lock_mode_t lock_mode;
	W_DO( lm->query(stpgid, lock_mode, xd->tid(), true) );
	// cc is off if file is EX/SH/UD/SIX locked
	if (lock_mode >= SH) {
	    cc = t_cc_none;
	} else if (lock_mode >= IS) {
	    // no changes needed
	} else {
	    // Index isn't already locked; have to grab IS lock
	    // on it below, via access()
	    index_mode = IS;
	}
    }

    sdesc_t* sd;
    W_DO( dir->access(stpgid, sd, index_mode) );
    if (sd->sinfo().stype != t_index)   return RC(eBADSTORETYPE);
    if (cc == t_cc_bad ) cc = (concurrency_t)sd->sinfo().cc;

    switch (sd->sinfo().ntype) {
    case t_bad_ndx_t:
	return RC(eBADNDXTYPE);
    case t_uni_btree:
    case t_btree:
	W_DO( bt->lookup(sd->root(), 
	     sd->sinfo().nkc, sd->sinfo().kc,
	     sd->sinfo().ntype == t_uni_btree,
	     cc,
	     key, el, elen, found) );
	break;

    case t_rtree:
    case t_rdtree:
    case t_lhash:
	return RC(eNOTIMPLEMENTED);
    default:
	W_FATAL(eINTERNAL);
    }
    
    return RCOK;
}

/*--------------------------------------------------------------*
 *  ss_m::_convert_to_store()					*
 *--------------------------------------------------------------*/
rc_t
ss_m::_convert_to_store(
    const lid_t& 	id, 
    const stpgid_t& 	stpgid,
    stid_t& 		new_stid)
{
    FUNC(ss_m::_convert_to_store);
    // this btree is a 1-page store and needs to migrate
    // to a real store.
    w_assert3(!stpgid.is_stid());

    sdesc_t* sd;
    W_DO( dir->access(stpgid, sd, EX) );
    sinfo_s sinfo(sd->sinfo());
    lpid_t new_root;
    { 
	// create a new store for the index
	W_DO( io->create_store(stpgid.vol(), sinfo.eff, 
			       st_regular, new_stid) );
	W_DO(lm->lock(new_stid, EX, t_long, WAIT_SPECIFIED_BY_XCT));

	switch (sinfo.ntype)  {
	case t_btree:
	case t_uni_btree:
	    W_DO( bt->create(new_stid, new_root, sinfo.kc[0].compressed != 0) );
	    break;
	default:
	    return RC(eBADNDXTYPE);
	}

	sinfo.store = new_stid.store;
	sinfo.stype = t_index;
	sinfo.root = new_root.page;
	W_DO( dir->insert(new_stid, sinfo) );
    }

    // copy entries from the old page to the new one.
    W_DO(bt->copy_1_page_tree(stpgid.lpid, new_root));

    // update the logical ID index
    W_DO(lid->associate(id.lvid, id.serial, new_stid, true/*replace*/));
    W_DO(dir->remove(stpgid));
    W_DO(io->free_page(stpgid.lpid));

    return RCOK;
}

/*--------------------------------------------------------------*
 *  ss_m::_create_md_assoc()					*
 *--------------------------------------------------------------*/
rc_t
ss_m::_create_md_assoc(stid_t stid, const nbox_t& key, const vec_t& el)
{
    sdesc_t* sd;
    W_DO( dir->access(stid, sd, IX) );

    if (sd->sinfo().stype != t_index)   return RC(eBADSTORETYPE);
    switch (sd->sinfo().ntype) {
    case t_bad_ndx_t:
      return RC(eBADNDXTYPE);
    case t_rtree:
	W_DO( rt->insert(sd->root(), key, el) );
        break;
    case t_rdtree:
    case t_btree:
    case t_uni_btree:
    case t_lhash:
      return RC(eWRONGKEYTYPE);
    default:
      W_FATAL(eINTERNAL);
    }

    return RCOK;
}

/*--------------------------------------------------------------*
 *  ss_m::_find_md_assoc()					*
 *--------------------------------------------------------------*/
rc_t
ss_m::_find_md_assoc(
    stid_t 		stid, 
    const nbox_t& 	key,
    void* 		el, 
    smsize_t& 		elen,
    bool& 		found)
{
    sdesc_t* sd;
    W_DO( dir->access(stid, sd, IS) );

    if (sd->sinfo().stype != t_index)   return RC(eBADSTORETYPE);
    switch (sd->sinfo().ntype) {
    case t_bad_ndx_t:
      return RC(eBADNDXTYPE);
    case t_rtree:
	// exact match
	W_DO( rt->lookup(sd->root(), key, el, elen, found) );
        break;
    case t_rdtree:
    case t_btree:
    case t_uni_btree:
    case t_lhash:
      return RC(eWRONGKEYTYPE);

    default:
      W_FATAL(eINTERNAL);
    }

    return RCOK;
}

/*--------------------------------------------------------------*
 *  ss_m::_draw_rtree()						*
 *--------------------------------------------------------------*/
rc_t
ss_m::_draw_rtree(const stid_t& stid, ostream &s)
{
    sdesc_t* sd;
    W_DO( dir->access(stid, sd, IS) );

    if (sd->sinfo().stype != t_index)   return RC(eBADSTORETYPE);
    switch (sd->sinfo().ntype) {
    case t_bad_ndx_t:
      return RC(eBADNDXTYPE);
    case t_rtree:
      W_DO( rt->draw(sd->root(), s) );
      break;
    case t_rdtree:
    case t_lhash:
    case t_btree:
    case t_uni_btree:
      return RC(eNOTIMPLEMENTED);

    default:
      W_FATAL(eINTERNAL);
    }

    return RCOK;
}

rc_t
ss_m::_rtree_stats(const stid_t& stid, rtree_stats_t& stat,
		 uint2_t size, uint2_t* ovp, bool audit)
{
    sdesc_t* sd;
    W_DO( dir->access(stid, sd, IS) );

    if (sd->sinfo().stype != t_index)   return RC(eBADSTORETYPE);
    switch (sd->sinfo().ntype) {
    case t_bad_ndx_t:
      return RC(eBADNDXTYPE);
    case t_rtree:
      W_DO( rt->stats(sd->root(), stat, size, ovp, audit) );
      break;
    case t_rdtree:
    case t_lhash:
    case t_btree:
    case t_uni_btree:
      return RC(eNOTIMPLEMENTED);

    default:
      W_FATAL(eINTERNAL);
    }

    return RCOK;
}


/*--------------------------------------------------------------*
 *  ss_m::_get_store_info()					*
 *--------------------------------------------------------------*/
rc_t
ss_m::_get_store_info(
    const stpgid_t& 	stpgid, 
    sm_store_info_t&	info
)
{
    sdesc_t* sd;
    W_DO( dir->access(stpgid, sd, NL) );

    const sinfo_s& s= sd->sinfo();

    info.store = s.store;
    info.stype = s.stype;
    info.ntype = s.ntype;
    info.cc    = s.cc;
    info.eff   = s.eff;
    info.large_store   = s.large_store;
    info.root   = s.root;
    info.nkc   = s.nkc;

    switch (sd->sinfo().ntype) {
    case t_btree:
    case t_uni_btree:
	W_DO( key_type_s::get_key_type(info.keydescr, 
		info.keydescrlen,
		sd->sinfo().nkc, sd->sinfo().kc ));
	break;
    default:
	break;
    }
    return RCOK;
}


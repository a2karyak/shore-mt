/*<std-header orig-src='shore'>

 $Id: file.cpp,v 1.193 1999/12/07 22:53:32 bolo Exp $

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

#ifdef __GNUG__
#pragma implementation "file.h"
#pragma implementation "file_s.h"
#endif

#define FILE_C
#include "sm_int_2.h"
#include "lgrec.h"
#include "w_minmax.h"
#include "sm_du_stats.h"

#include "histo.h"
#include "crash.h"

#ifdef EXPLICIT_TEMPLATE
/* Used in sort.cpp, btree_bl.cpp */
template class w_auto_delete_array_t<file_p>;
#endif

// This macro is used to verify correct serial numbers on records.
// If "serial" is non-null then it is checked to make sure it
// matches the serial number in the record.
#define VERIFY_SERIAL(serial, rec)				\
        if ((serial) != serial_t::null && 			\
	    (rec)->tag.serial_no != serial) {			\
	    DBG(<<"VERIFY_SERIAL: " ); 				\
            return RC(eBADLOGICALID);				\
        }                               


lpid_t 
record_t::last_pid(const file_p& page) const
{
    lpid_t last;

#ifdef W_DEBUG
    // make sure record is on the page
    const char* check = (const char*)&page.persistent_part_const();
    w_assert3(check < (char*)this && (check+sizeof(page_s)) > (char*)this);
#endif /* W_DEBUG */

    w_assert3(body_size() > 0);
    if (tag.flags & t_large_0) {
        const lg_tag_chunks_h lg_hdl(page, *(lg_tag_chunks_s*)body());
        last = lg_hdl.last_pid();
    } else if (tag.flags & (t_large_1 | t_large_2)) {
	const lg_tag_indirect_h lg_hdl(page, *(lg_tag_indirect_s*)body(), page_count());
        last = lg_hdl.last_pid();
    } else {
    	W_FATAL(smlevel_0::eINTERNAL);
    }

    w_assert3(last.vol() > 0 && last.store() > 0 && last.page > 0);
    return last;
}

lpid_t 
record_t::pid_containing(uint4_t offset, uint4_t& start_byte, const file_p& page) const
{
#ifdef W_DEBUG
    // make sure record is on the page
    const char* check = (const char*)&page.persistent_part_const();
    w_assert3(check < (char*)this && (check+sizeof(page_s)) > (char*)this);
#endif /* W_DEBUG */
    if(body_size() == 0) return lpid_t::null;

    if (tag.flags & t_large_0) {
        shpid_t page_num = offset / lgdata_p::data_sz;
        const lg_tag_chunks_h lg_hdl(page, *(lg_tag_chunks_s*)body());

        start_byte = page_num*lgdata_p::data_sz;
        return lg_hdl.pid(page_num);
    } else if (tag.flags & (t_large_1 | t_large_2)) {
        shpid_t page_num = offset / lgdata_p::data_sz;
	const lg_tag_indirect_h lg_hdl(page, *(lg_tag_indirect_s*)body(), page_count());

        start_byte = page_num*lgdata_p::data_sz;
        return lg_hdl.pid(page_num);
    }
    W_FATAL(smlevel_0::eNOTIMPLEMENTED);
    lpid_t dummy; // keep compiler quit
    return dummy;
}

uint4_t 
record_t::page_count() const
{
    return body_size() == 0 ? 0 : (body_size()-1) / lgdata_p::data_sz +1;
}

INLINE 
smsize_t 
file_m::_space_last_page(smsize_t rec_sz)
{
    return (lgdata_p::data_sz - rec_sz % lgdata_p::data_sz) %
	    lgdata_p::data_sz;
}

INLINE 
smsize_t 
file_m::_bytes_last_page(smsize_t rec_sz)
{
    return rec_sz == 0 ? 0 :
			 lgdata_p::data_sz - _space_last_page(rec_sz);
}

file_m::file_m () 
{ 
    w_assert1(is_aligned(sizeof(rectag_t)));
    W_COERCE(histoid_t::initialize_table());
}

file_m::~file_m()   
{
    histoid_t::destroy_table();
}

rc_t 
file_m::create(stid_t stid, lpid_t& first_page)
{
    file_p  page;
    DBGTHRD(<<"file_m::create create first page in store " << stid);
    W_DO(_alloc_page( stid, 
	lpid_t::eof,  // hint bof/eof doesn't matter, but if eof,
		      // it will shorten the path a bit
	first_page,   // output
	page,
	true	      // immaterial here
	));
    // page.destructor() causes it to be unfixed
    w_assert3(page.is_fixed());

    histoid_update_t hu(page);
    hu.update();
    DBGTHRD(<<"file_m::create(d) first page is  " << first_page);
    return RCOK;
}


/* NB: argument order is similar to old create_rec */
rc_t
file_m::create_rec(
    const stid_t& 	fid,
    // no page hint
    smsize_t		len_hint,
    const vec_t& 	hdr,
    const vec_t& 	data,
    const serial_t&	serial_no, // output
    sdesc_t&		sd,
    rid_t& 		rid // output
    // no forward_alloc
)
{
    file_p	page;
    uint4_t     policy = t_cache | t_compact | t_append;

    DBG(<<"create_rec store " << fid);

    W_DO(_create_rec( fid, pg_policy_t(policy), 
	len_hint, sd, hdr, data, serial_no,
	rid, page));
    DBG(<<"create_rec created " << rid);
    return RCOK;
}
/* NB: order of arguments is same as old create_rec_at_end */
rc_t 
file_m::create_rec_at_end(
	file_p&		page, // in-out -- caller might have it fixed
	uint4_t 	len_hint,
	const vec_t& 	hdr,
	const vec_t& 	data,
	const serial_t& serial_no, // out
	sdesc_t& 	sd, 
	rid_t& 		rid	// out
)
{
    FUNC(file_m::create_rec_at_end);

    if(hdr.size() > file_p::data_sz - sizeof(rectag_t)) {
	return RC(eRECWONTFIT);
    }
    stid_t	fid = sd.stpgid().stid();

    DBG(<<"create_rec_at_end store " << fid);
    W_DO(_create_rec(fid, t_append, len_hint,
	sd, hdr, data, serial_no, rid, page));
    DBG(<<"create_rec_at_end created " << rid);
    return RCOK;
}

/*
 * Called from create_rec_at_end and from create_rec
 */
rc_t
file_m::_create_rec(
    const stid_t& 	fid,
    pg_policy_t 	policy,
    smsize_t		len_hint,
    sdesc_t&		sd,
    const vec_t& 	hdr,
    const vec_t& 	data,
    const serial_t&	serial_no,
    rid_t& 		rid,
    file_p&		page	// in-output
)
{
    smsize_t	space_needed;
    recflags_t	rec_impl; 
    slotid_t	slot = 0;

    {
	/*
	 * compute space needed, record implementation
	 */
	w_assert3(fid == sd.stpgid().stid());
	smsize_t est_data_len = MAX((uint4_t)data.size(), len_hint);
	rec_impl = file_p::choose_rec_implementation( hdr.size(), 
						  est_data_len,
						  space_needed);
    }
    DBG(<<"create_rec with policy " << int(policy)
	<< " space_needed=" << space_needed
	<< " rec_impl=" << int(rec_impl)
	);

    bool 	have_page = false;

    //
    // First time throough from append_file_i this isn't true:
    // if(policy == t_append) {
	// w_assert3(page.is_fixed());
    // }

    { // open scope for hu
	DBGTHRD(<<"About to copy sd");
	histoid_update_t hu(&sd);

	if(page.is_fixed()) {
	    w_assert3(policy == t_append);
	    w_assert3(page.latch_mode() == LATCH_EX);

	    rc_t rc = page.find_and_lock_next_slot(space_needed, slot);

	    if(rc) {
		page.unfix();
		DBG(<<"rc=" << rc);
		if (rc.err_num() != eRECWONTFIT) {
		    // error we can't handle
		    return RC_AUGMENT(rc);
		}
		w_assert3(!page.is_fixed());
		w_assert3(!have_page);
	    } else {
		DBG(<<"acquired slot " << slot);
		have_page = true;
		w_assert3(page.is_fixed());
		hu.replace_page(&page);
#ifdef W_DEBUG
		{
		    w_assert3(page.latch_mode() == LATCH_EX);
		    space_bucket_t b = page.bucket();
		    if((page.page_bucket_info.old() != b) && 
			page.page_bucket_info.initialized()) {
			DBG(<<"AH HA!");
			w_assert1(0);
		    }
		}
#endif
	    }
	} 

	if(!have_page) {
	    W_DO(_find_slotted_page_with_space(fid, policy, sd, 
		space_needed, page, slot));
	    hu.replace_page(&page);
	}

#ifdef W_DEBUG
    {
	w_assert3(page.is_fixed() && page.latch_mode() == LATCH_EX);
	space_bucket_t b = page.bucket();
	if((page.page_bucket_info.old() != b) && 
	    page.page_bucket_info.initialized()) {
	    DBG(<<"AH HA!");
	    w_assert1(0);
	}
    }
#endif

	// split into 2 parts so we don't hog the histoid, and
	// so we don't run into double-acquiring it in append_rec.

	W_DO(_create_rec_in_slot(page, slot, rec_impl, 
	    serial_no, hdr, data, sd, false,
	    rid));

	w_assert3(page.is_fixed() && page.latch_mode() == LATCH_EX);

	DBG(<<"");
	hu.update();
    } // close scope for hu

    if(rec_impl == t_large_0) {
	W_DO(append_rec(rid, data, sd, false, serial_t::null));
    }
    return RCOK;
}

rc_t
file_m::_find_slotted_page_with_space(
    const stid_t&	stid,
    pg_policy_t mask,
    sdesc_t&	sd,
    smsize_t	space_needed, 
    file_p&	page,	// output
    slotid_t&	slot	// output
)
{
    uint4_t 	policy = uint4_t(mask);

    DBG(<< "_find_slotted_page_with_space needed=" << space_needed 
	<< " policy =" << policy);

    bool	found=false;
    const histoid_t*	h = sd.store_utilization();

    w_assert3(!page.is_fixed());

    /*
     * First, if policy calls for it, look in the cache
     */
    if(policy & t_cache) {
	INC_TSTAT(fm_cache);
	while(!found) {
	    pginfo_t	info;
	    DBG(<<"looking in cache");
	    W_DO(h->find_page(space_needed, found, info, &page, slot));
	    if(found) {
		w_assert3(page.is_fixed());
		DBG(<<"found page " << info.page() 
			<< " slot " << slot);
		INC_TSTAT(fm_pagecache_hit);
		return RCOK;
	    } else {
		// else no candidates -- drop down
		w_assert3(!page.is_fixed());
		break;
	    }
	}
    }
    w_assert3(!found);
    w_assert3(!page.is_fixed());

    bool may_search_file = false;

    /*
     * Next, if policy calls for it, search the file for space.
     * We're going to be a little less aggressive here than when
     * we searched the cache.  If we read a page in from disk, 
     * we want to be sure that it will have enough space.  So we
     * bump ourselves up to the next bucket.
     */
    if(policy & t_compact) {
	INC_TSTAT(fm_compact);
	DBG(<<"looking at file histogram");
	smsize_t sn = page_p::bucket2free_space(
                      page_p::free_space2bucket(space_needed)) + 1;
	W_DO(h->exists_page(sn, found));
	if(found) {
	    INC_TSTAT(fm_histogram_hit);

	    // histograms says there are 
	    // some sufficiently empty pages in the file 
	    // It's worthwhile to search the file for some space.

	    lpid_t 		lpid;

	    W_DO(first_page(stid, lpid, NULL/*allocated pgs only*/) );
	    DBG(<<"first allocated page of " << stid << " is " << lpid);

	    // scan the file for pages with adequate space
	    bool 		eof = false;
	    while ( !eof ) {
		w_assert3(!page.is_fixed());
		W_DO( page.fix(lpid, LATCH_SH, 0/*page_flags */));

		INC_TSTAT(fm_search_pages);

		DBG(<<"usable space=" << page.usable_space()
			<< " needed= " << space_needed);
		if(page.usable_space() >= space_needed) {

		    W_DO(h->latch_lock_get_slot( 
			lpid.page, &page, space_needed,
			false, // not append-only 
			found, slot));
		    if(found) {
			w_assert3(page.is_fixed());
			DBG(<<"found page " << page.pid().page 
				<< " slot " << slot);
			INC_TSTAT(fm_search_hit);
			return RCOK;
		    }
		}
		page.unfix(); // avoid latch-lock deadlocks.

		// read the next page
		DBG(<<"get next page after " << lpid 
			<< " for space " << space_needed);
		W_DO(next_page_with_space(lpid, eof,
			file_p::free_space2bucket(space_needed) + 1));
		DBG(<<"next page is " << lpid);
	    }
            // This should never happen now that we bump ourselves up
            // to the next bucket.
	    INC_TSTAT(fm_search_failed);
	    found = false;
	} else {
	    DBG(<<"not found in file histogram - so don't search file");
	}
	w_assert3(!found);
	if(!found) {
	    // If a page exists in the allocated extents,
	    // allocate one and use it.
	    may_search_file = true;
	    // NB: we do NOT support alloc-in-file-with-search
	    // -but-don't-alloc-any-new-extents 
	    // because io layer doesn't offer that option
	}
    }
    w_assert3(!found);
    w_assert3(!page.is_fixed());

    /*
     * Last, if policy calls for it, append (possibly strict) 
     * to the file.  may_search_file indicates not strictly
     * appending; !may_search_file indicates strict append.
     */
    if(policy & t_append) {
	if(may_search_file) {
	    INC_TSTAT(fm_append);
	} else {
	    INC_TSTAT(fm_appendonly);
	}
	lpid_t	lastpid = lpid_t(stid, sd.hog_last_pid());

	DBG(<<"try to append to file lastpid.page=" << lastpid.page);

	// might not have last pid cached
	if(lastpid.page) {
	    DBG(<<"look in last page - which is " << lastpid );
	    // TODO: might get a deadlock here!!!!!

	    INC_TSTAT(fm_lastpid_cached);
	    sd.free_last_pid();

	    W_DO(h->latch_lock_get_slot( 
			lastpid.page, &page, space_needed,
			!may_search_file, // append-only
			found, slot));
	    if(found) {
		w_assert3(page.is_fixed());
		DBG(<<"found page " << page.pid().page 
			<< " slot " << slot);
		INC_TSTAT(fm_lastpid_hit);
		return RCOK;
	    }
	    DBG(<<"no slot in last page ");
	} else {
	    sd.free_last_pid();
	}

	DBGTHRD(<<"allocate new page" );

	/* Allocate a new page */
	lpid_t	newpid;
	/*
	 * Argument may_search_file determines behavior of _alloc_page:
	 * if true, it searches existing extents in the file besides
	 * the one indicated by the near_pid (lastpid here) argument,
	 * else it appends extents if it can't satisfy the request
	 * with the first extent inspected. 
	 *
	 * Furthermore, if may_search_file determines the treatment
	 * of that first extent inspected: if may_search_file is true,
	 * it looks for ANY free page in that extent; else it only
	 * looks for free pages at the "end" of the extent.
	 */
	W_DO(_alloc_page(stid,  lastpid, newpid,  page, may_search_file)); 
	w_assert3(page.is_fixed());
	w_assert3(page.latch_mode() == LATCH_EX);
	// Now have long-term IX lock on the page

	if(may_search_file) {
	    sd.set_last_pid(0); // don't know last page
		// and don't want to look for it now
	} else {
	    sd.set_last_pid(newpid.page);
	}

	// Page is already latched, but we don't have a
	// lock on a slot yet.  (Doesn't get doubly-latched by
	// calling latch_lock_get_slot, by the way.)
	W_DO(h->latch_lock_get_slot(
		newpid.page, &page, space_needed,
		!may_search_file,
		found, slot));

	if(found) {
	    w_assert3(page.is_fixed());
	    DBG(<<"found page " << page.pid().page 
		    << " slot " << slot);
	    INC_TSTAT(fm_alloc_pg);
	    return RCOK;
	}
	page.unfix();
    }
    w_assert3(!found);
    w_assert3(!page.is_fixed());

    INC_TSTAT(fm_nospace);
    DBG(<<"not found");
    return RC(eNOTFOUND);
}

/* 
 * add a record on the given page.  page is already
 * fixed; all we have to do is try to allocate a slot
 * for a record of the given size
 */


rc_t
file_m::_create_rec_in_slot(
    file_p 	&page,
    slotid_t	slot,
    recflags_t 	rec_impl, 	
    const serial_t& serial_no,
    const vec_t& hdr,
    const vec_t& data,
    sdesc_t&	sd,
    bool	do_append,
    rid_t&	rid // out
)
{
    FUNC(_create_rec_in_slot);
    w_assert3(page.is_fixed() && page.is_file_p());

    // page is already in the file and locked IX or EX
    // slot is already locked EX
    rid.pid = page.pid();
    rid.slot = slot;

    w_assert3(rid.slot >= 1);

    /*
     * create the record header and ...
     */
    rc_t	rc;
    rectag_t 	tag;
    tag.hdr_len = hdr.size();
    tag.serial_no = serial_no;

    switch (rec_impl) {
    case t_small:
	// it is small, so put the data in as well
	tag.flags = t_small;
	tag.body_len = data.size();
	rc = page.fill_slot(rid.slot, tag, hdr, data, 100);
	if (rc)  {
	    w_assert3(rc.err_num() != eRECWONTFIT);
	    return RC_AUGMENT(rc);
	}
	break;
    case t_large_0:
	// lookup the store to use for pages in the large record
	w_assert3(sd.large_stid().store > 0);

	// it is large, so create a 0-length large record
	{
	    lg_tag_chunks_s   lg_chunks(sd.large_stid().store);
	    vec_t	      lg_vec(&lg_chunks, sizeof(lg_chunks));

	    tag.flags = rec_impl;
	    tag.body_len = 0;
	    rc = page.fill_slot(rid.slot, tag, hdr, lg_vec, 100);
	    if (rc) {
		w_assert3(rc.err_num() != eRECWONTFIT);
		return RC_AUGMENT(rc);
	    } 

	    // now append the data to the record
	    if(do_append) {
		W_DO(append_rec(rid, data, sd, false, serial_t::null));
	    }

	}
	break;
    case t_large_1: case t_large_2:
	// all large records start out as t_large_0
    default:
	W_FATAL(eINTERNAL);
    }
    w_assert3(page.is_fixed() && page.is_file_p());
    return RCOK;
}

rc_t
file_m::destroy_rec(const rid_t& rid, const serial_t& verify)
{
    file_p    page;
    record_t*	    rec;

    DBGTHRD(<<"destroy_rec");
    W_DO(_locate_page(rid, page, LATCH_EX));

    /*
     * Find or create a histoid for this store.
     */
    w_assert3(page.is_fixed());
    histoid_update_t hu(page);

    W_DO( page.get_rec(rid.slot, rec) );
    DBGTHRD(<<"got rec for rid " << rid);
    VERIFY_SERIAL(verify, rec);

    if (rec->is_small()) {
	// nothing special
	DBG(<<"small");
    } else {
	DBG(<<"large -- truncate");
	W_DO(_truncate_large(page, rid.slot, rec->body_size()));
    }
    W_DO( page.destroy_rec(rid.slot) );

    if (page.rec_count() == 0) {
	DBG(<<"Now free page");
	W_DO(_free_page(page));
	hu.remove();
    } else {
	DBG(<<"Update page utilization");
	/*
	 *  Update the page's utilization info in the
	 *  cache.
	 *  (page_p::unfix updates the extent's histogram info)
	 */
	hu.update();
    }
    return RCOK;
}

rc_t
file_m::update_rec(const rid_t& rid, uint4_t start, const vec_t& data, const serial_t& verify)
{
    file_p    page;
    record_t*	    rec;

    DBGTHRD(<<"update_rec");
    W_DO(_locate_page(rid, page, LATCH_EX));

    W_DO( page.get_rec(rid.slot, rec) );
    VERIFY_SERIAL(verify, rec);

    /*
     *	Do some parameter checking
     */
    if (start > rec->body_size()) {
	return RC(eBADSTART);
    }
    if (data.size() > (rec->body_size()-start)) {
	return RC(eRECUPDATESIZE);
    }

    if (rec->is_small()) {
	W_DO( page.splice_data(rid.slot, u4i(start), data.size(), data) );
    } else {
	if (rec->tag.flags & t_large_0) {
	    lg_tag_chunks_h lg_hdl(page, *(lg_tag_chunks_s*)rec->body());
	    W_DO(lg_hdl.update(start, data));
	} else {
	    lg_tag_indirect_h lg_hdl(page, *(lg_tag_indirect_s*)rec->body(), rec->page_count());
	    W_DO(lg_hdl.update(start, data));
	}
    }
    return RCOK;
}

rc_t
file_m::append_rec(const rid_t& rid, const vec_t& data,
		   const sdesc_t& sd, bool allow_forward, const serial_t& verify)
{
    file_p    	page;
    record_t*	rec;
    smsize_t	orig_size;

    w_assert3(rid.stid() == sd.stpgid().stid());

    DBGTHRD(<<"append_rec");
    W_DO( _locate_page(rid, page, LATCH_EX) );

    /*
     * Find or create a histoid for this store.
     */
    w_assert3(page.is_fixed());
    histoid_update_t hu(page);

    W_DO( page.get_rec(rid.slot, rec));
    VERIFY_SERIAL(verify, rec);

    orig_size = rec->body_size();

    // make sure we don't grow the record to larger than 4GB
    if ( (record_t::max_len - orig_size) < data.size() ) {
#ifdef W_DEBUG
	cerr << "can't grow beyond 4GB=" << int(record_t::max_len)
		<< " orig_size " << int(orig_size)
		<< " append.size() " << data.size()
		<< endl;
#endif /* W_DEBUG */
	return RC(eBADAPPEND);
    }

    // see if record will remain small
    smsize_t space_needed;
    if ( rec->is_small() &&
	file_p::choose_rec_implementation(rec->hdr_size(), orig_size + data.size(),
				    space_needed) == t_small) {

	if (page.usable_space() < data.size()) {
	    if (allow_forward) {
		return RC(eNOTIMPLEMENTED);
	    } else {
		return RC(eRECWONTFIT);
	    }
	}

	W_DO( page.append_rec(rid.slot, data) );
	// reaquire since may have moved
	W_COERCE( page.get_rec(rid.slot, rec) );
	w_assert3(rec != NULL);
    } else if (rec->is_small()) {

	// Convert the record to a large implementation
	// copy the body to a temporary location

	char *tmp = new char[page_s::data_sz]; //auto-del
	w_auto_delete_array_t<char> autodel(tmp);

	memcpy(tmp, rec->body(), orig_size);
	vec_t   body_vec(tmp, orig_size);

	w_assert3(sd.large_stid().store > 0);

	// it is large, so create a 0-length large record
	lg_tag_chunks_s   lg_chunks(sd.large_stid().store);
	vec_t	      lg_vec(&lg_chunks, sizeof(lg_chunks));

	// put the large record root after the header and mark
	// the record as large
	W_DO(page.splice_data(rid.slot, 0, (slot_length_t)orig_size, lg_vec)); 
	W_DO(page.set_rec_flags(rid.slot, t_large_0));
	W_DO(page.set_rec_len(rid.slot, 0));

	// append the original data and the new data
	W_DO(_append_large(page, rid.slot, body_vec));
	W_DO(page.set_rec_len(rid.slot, orig_size));
	W_DO(_append_large(page, rid.slot, data));

    } else {
	w_assert3(rec->is_large());
	W_DO(_append_large(page, rid.slot, data));
    }
    W_DO( page.set_rec_len(rid.slot, orig_size+data.size()) );

    /*
     *  Update the page's utilization info in the
     *  cache.
     *  (page_p::unfix updates the extent's histogram info)
     */
    DBG(<<"append rec");
    hu.update();
    return RCOK;
}

rc_t
file_m::truncate_rec(const rid_t& rid, uint4_t amount, bool& should_forward, const serial_t& verify)
{
    FUNC(file_m::truncate_rec);
    file_p	page;
    record_t* 	rec;
    should_forward = false;  // no need to forward record at this time

    DBGTHRD(<<"trucate_rec");
    W_DO (_locate_page(rid, page, LATCH_EX));

    /*
     * Find or create a histoid for this store.
     */
    w_assert3(page.is_fixed());
    histoid_update_t hu(page);

    W_DO(page.get_rec(rid.slot, rec));
    VERIFY_SERIAL(verify, rec);

    if (amount > rec->body_size()) 
	return RC(eRECUPDATESIZE);

    uint4_t	orig_size  = rec->body_size();
    uint2_t	orig_flags  = rec->tag.flags;

    if (rec->is_small()) {
	W_DO( page.truncate_rec(rid.slot, amount) );
	rec = NULL; // no longer valid;
    } else {
	W_DO(_truncate_large(page, rid.slot, amount));
	W_COERCE( page.get_rec(rid.slot, rec) );  // re-establish rec ptr
	w_assert3(rec);
	// 
	// Now see it this record can be implemented as a small record
	//
	smsize_t len = orig_size-amount;
	smsize_t space_needed;
	recflags_t rec_impl;
	rec_impl = file_p::choose_rec_implementation(rec->hdr_size(), len, space_needed);
	if (rec_impl == t_small) {
	    DBG( << "rec was large, now is small");

	    vec_t data;  // data left in the body
	    uint4_t size_on_file_page;
	    if (orig_flags & t_large_0) {
		size_on_file_page = sizeof(lg_tag_chunks_s);
	    } else {
		size_on_file_page = sizeof(lg_tag_indirect_s);
	    }

	    if (len == 0) {
		DBG( << "rec is now is zero bytes long");
		w_assert3(data.size() == 0);
		W_DO(page.splice_data(rid.slot, 0, (slot_length_t)size_on_file_page, data));
		// record is now small 
		W_DO(page.set_rec_flags(rid.slot, t_small));
	    } else {

		// the the data for the record (must be on "last" page)
		lgdata_p lgdata;
		W_DO( lgdata.fix(rec->last_pid(page), LATCH_SH) );
		w_assert3(lgdata.tuple_size(0) == len);
		data.put(lgdata.tuple_addr(0), len);

		/*
		 * Remember (in lgtmp) the large rec hdr on the file_p 
		 */

	/*
		// This is small: < 60 bytes -- Probably should put on stack 
		char lgtmp[sizeof(lg_tag_chunks_s)+sizeof(lg_tag_indirect_s)];
	*/
		char  *lgtmp = new char[sizeof(lg_tag_chunks_s)+
					sizeof(lg_tag_indirect_s)];
	        if (!lgtmp)
			W_FATAL(fcOUTOFMEMORY);
		w_auto_delete_array_t<char>	autodel_lgtmp(lgtmp);

		lg_tag_chunks_s* lg_chunks = NULL;
		lg_tag_indirect_s* lg_indirect = NULL;
		if (orig_flags & t_large_0) {
		    memcpy(lgtmp, rec->body(), sizeof(lg_tag_chunks_s));
		    lg_chunks = (lg_tag_chunks_s*) lgtmp;
		} else {
		    memcpy(lgtmp, rec->body(), sizeof(lg_tag_indirect_s));
		    lg_indirect = (lg_tag_indirect_s*) lgtmp;
		}

		// splice body on file_p with data from lg rec
		rc_t rc = page.splice_data(rid.slot, 0, 
					   (slot_length_t)size_on_file_page, data);
		if (rc) {
		    if (rc.err_num() == eRECWONTFIT) {
			// splice failed ==> not enough space on page
			should_forward = true;
			DBG( << "record should be forwarded after trunc");
		    } else {
			return RC_AUGMENT(rc);
		    }			  
		} else {
		    rec = 0; // no longer valid;

		    // remove rest of data in lg rec
		    DBG( << "removing lgrec portion of truncated rec");
		    if (orig_flags & t_large_0) {
			// remove the 1 lgdata page
			w_assert3(lg_indirect == NULL);
			lg_tag_chunks_h lg_hdl(page, *lg_chunks);
			W_DO(lg_hdl.truncate(1));
		    } else {
			// remove the 1 lgdata page and any indirect pages
			w_assert3(lg_chunks == NULL);
			lg_tag_indirect_h lg_hdl(page, *lg_indirect, 1/*page_cnt*/);
			W_DO(lg_hdl.truncate(1));
		    }
		    // record is now small 
		    W_DO(page.set_rec_flags(rid.slot, t_small));
		}
	    }
	}
    }
    /*
     *  Update the page's utilization info in the
     *  cache.
     *  (page_p::unfix updates the extent's histogram info)
     */
    DBG(<<"truncate rec");
    hu.update();
    /* 
     * Update the extent histo info before logging anything
     */
    W_DO(page.update_bucket_info());

    W_DO( page.set_rec_len(rid.slot, orig_size-amount) );

    return RCOK;
}

rc_t
file_m::read_hdr(const rid_t& s_rid, int& len,
		 void* buf)
{
    rid_t rid(s_rid);
    file_p page;
    
    DBGTHRD(<<"read_hdr");
    W_DO(_locate_page(rid, page, LATCH_SH) );
    record_t* rec;
    W_DO( page.get_rec(rid.slot, rec) );
    
    w_assert1(rec->is_small());
    if (rec->is_small())  {
	if (len < rec->tag.hdr_len)  {
	    return RC(eBADLENGTH); // not long enough
	}
	if (len > rec->tag.hdr_len) 
	    len = rec->tag.hdr_len;
	memcpy(buf, rec->hdr(), len);
    }
    
    return RCOK;
}

rc_t
file_m::read_rec(const rid_t& s_rid,
		 int start, uint4_t& len, void* buf)
{
    rid_t rid(s_rid);
    file_p page;
    
    DBGTHRD(<<"read_rec");
    W_DO( _locate_page(rid, page, LATCH_SH) );
    record_t* rec;
    W_DO( page.get_rec(rid.slot, rec) );
    
    w_assert1(rec->is_small());
    if (rec->is_small())  {
	if (start + len > rec->body_size())  
	    len = rec->body_size() - start;
	memcpy(buf, rec->body() + start, (uint)len);
    }
    
    return RCOK;
}

rc_t
file_m::splice_hdr(rid_t rid, slot_length_t start, slot_length_t len, const vec_t& hdr_data, const serial_t& verify)
{
    file_p page;
    DBGTHRD(<<"splice_hdr");
    W_DO( _locate_page(rid, page, LATCH_EX) );

    record_t* rec;
    W_DO( page.get_rec(rid.slot, rec) );
    VERIFY_SERIAL(verify, rec);

    // currently header realignment (rec hdr must always
    // have an alignedlength) is not supported
    w_assert3(len == hdr_data.size());
    W_DO( page.splice_hdr(rid.slot, start, len, hdr_data));
    return RCOK;
}

rc_t
file_m::first_page(const stid_t& fid, lpid_t& pid, bool* allocated)
{
    if (fid.vol.is_remote()) {
	W_FATAL(eBADSTID);
    } else {
        rc_t rc = io->first_page(fid, pid, allocated);
        if (rc) {
	    w_assert3(rc.err_num() != eEOF);
	    if(rc.err_num() == eLOCKTIMEOUT) {
		// pid will have been returned.
		// Lock it while blocking and try again
		DBG(<<"");
		W_DO(lm->lock(pid, IX, t_long, WAIT_SPECIFIED_BY_XCT));
	    } else {
		DBG(<<"rc="<< rc);
		return RC_AUGMENT(rc);
	    }
        }
    }
    DBGTHRD(<<"first_page is " <<pid);
    return RCOK;
}

rc_t
file_m::last_page(const stid_t& fid, lpid_t& pid, bool* allocated)
{
    FUNC(file_m::last_page);
    rc_t rc;
    if (fid.vol.is_remote()) {
	W_FATAL(eBADSTID);
    } else do {
        rc = io->last_page(fid, pid, allocated, IX);
        if (rc) {
	    w_assert3(rc.err_num() != eEOF);
	    if(rc.err_num() == eLOCKTIMEOUT) {
		// pid will have been returned.
		// Lock it while blocking and try again
		DBG(<<"");
		W_DO(lm->lock(pid, IX, t_long, WAIT_SPECIFIED_BY_XCT));
	    } else {
		DBG(<<"rc="<< rc);
		return RC_AUGMENT(rc);
	    }
        }
    } while (rc);
    DBG(<<"last page is  "<< pid);
    return RCOK;
}

// If "allocated" is NULL then only allocated pages will be
// returned.  If "allocated" is non-null then all pages will be
// returned and the bool pointed to by "allocated" will be set to
// indicate whether the page is allocated.
rc_t
file_m::next_page_with_space(lpid_t& pid, bool& eof, space_bucket_t b)
{
    eof = false;

    DBGTHRD(<<"find next_page_with_space ");

    rc_t rc = io->next_page_with_space(pid, b);
    DBGTHRD(<<"next_page_with_space returns " << rc);
    if (rc)  {
	if (rc.err_num() == eEOF) {
	    eof = true;
	} else {
	    return RC_AUGMENT(rc);
	}
    }
#ifdef W_DEBUG
   if(pid.page) {
	file_p page;
	w_assert3(!page.is_fixed());
	W_DO( page.fix(pid, LATCH_SH, 0/*page_flags */));
	DBG(   
		<<" page=" << page.pid().page
		<<" page.usable_space=" << page.usable_space()
		<<" page.bucket=" << int(page.bucket())
		<<" need bucket" << int(b)
	);
	if( page.bucket() < b) {
	    w_assert3(0);
	}
   }
#endif
    DBG(<<"next page with bucket >= " << int(b) << " is " << pid);
    return RCOK;
}
rc_t
file_m::next_page(lpid_t& pid, bool& eof, bool* allocated)
{
    eof = false;

    if (pid.is_remote()) {
	W_FATAL(eBADPID);
    } else {
        rc_t rc = io->next_page(pid, allocated);
        if (rc)  {
    	    if (rc.err_num() == eEOF) {
	        eof = true;
	    } else {
	        return RC_AUGMENT(rc);
	    }
        }
    }
    return RCOK;
}

rc_t
file_m::_locate_page(const rid_t& rid, file_p& page, latch_mode_t mode)
{
    DBGTHRD(<<"file_m::_locate_page rid=" << rid);

    /*
     * pin the page unless it's remote; even if it's remote,
     * pin if we are using page-level concurrency
     */
    if (cc_alg == t_cc_page || (! rid.pid.is_remote())) {
        W_DO(page.fix(rid.pid, mode));
    } else {
	W_FATAL(eINTERNAL);
    }

    w_assert3(page.pid().page == rid.pid.page);
    // make sure page belongs to rid.pid.stid
    if (page.pid() != rid.pid) {
	page.unfix();
	return RC(eBADPID);
    }

    if (rid.slot < 0 || rid.slot >= page.num_slots())  {
	return RC(eBADSLOTNUMBER);
    }

    return RCOK;
}

//
// Free the page only if it is empty and it is not the first page of the file.
// Note: a valid file should always have at least one page allocated.
//
rc_t
file_m::_free_page(file_p& page)
{
    lpid_t pid = page.pid();

    DBGTHRD(<<"free_page " << pid << ";  rec count=" << page.rec_count());
    if (page.rec_count() == 0) {

	lpid_t first_pid;
	W_DO(first_page(pid.stid(), first_pid, NULL));

	if (pid != first_pid) {
	    DBGTHRD(<<"free_page: not first page -- go ahead");

	    rc_t rc = smlevel_0::io->free_page(pid);

	    if (rc.err_num() == eMAYBLOCK || rc.err_num() == eLOCKTIMEOUT) {
	        page.unfix();
	        W_DO(lm->lock_force(pid, EX, t_long,
					WAIT_SPECIFIED_BY_XCT));
	        W_DO(page.fix(pid, LATCH_EX));

	        if (page.rec_count() == 0) {
	            rc = smlevel_0::io->free_page(pid);
		    if(rc) {
			w_assert1(rc.err_num() != eMAYBLOCK && rc.err_num() != eLOCKTIMEOUT);
		    }
	        } else {
	            return RCOK;
	        }
	    }
	    return rc.reset();
	}
    }
    return RCOK;
}

rc_t
file_m::_alloc_page(
    stid_t fid,
    const lpid_t& near_p,
    lpid_t& allocPid,
    file_p &page,	 // leave it fixed here
    bool search_file
)
{
    /*
     *  Allocate a new page. 
     *
     *  Page formats are not undoable, so there's no need to compensate.
     *
     *  Pages get deallocated after commit when the extents are deallocated.
     */

    W_DO( io->alloc_pages(fid, 
		near_p,  // near hint
		1, 
		&allocPid,  // output array
		false, 	// not may_realloc
     /*
     *  The I/O layer locks the page instantly if may_realloc is passed
     *  in by the resource manager.  If not may_realloc, it acquires
     *	a long lock on the page.
     */
		IX,	// lock mode on allocated page
		search_file
		));
    
    store_flag_t store_flags;
    W_DO( io->get_store_flags(allocPid.stid(), store_flags));
    if (store_flags & st_insert_file)  {
	store_flags = (store_flag_t) (store_flags|st_tmp); 
	// is st_tmp and st_insert_file
    }

    /*
     * cause the page to be formatted
     * (fix figures out the store flags, but doesn't pass
     * it back to us, so we got it above)
     */
    W_DO( page.fix(allocPid, LATCH_EX, page.t_virgin, store_flags) );


#ifdef UNDEF
    // cluster is already set to zero when the page is formatted
    file_p_hdr_t hdr;
    hdr.cluster = 0;		// dummy cluster for now
    DO( page->set_hdr(hdr) );
#endif /* UNDEF */

    return RCOK;
}


rc_t
file_m::_append_large(file_p& page, slotid_t slot, const vec_t& data)
{
    FUNC(file_m::_append_large);
    smsize_t	left_to_append = data.size();
    record_t*   rec;
    W_DO( page.get_rec(slot, rec) );

    uint4_t 	rec_page_count = rec->page_count();

    smsize_t 	space_last_page = _space_last_page(rec->body_size());

    // add data to the last page
    if (space_last_page > 0) {
	lgdata_p last_page;
	W_DO( last_page.fix(rec->last_pid(page), LATCH_EX) );
	w_assert1(last_page.is_fixed());

	uint4_t append_amount = MIN(space_last_page, left_to_append);
	W_DO( last_page.append(data, 0, append_amount) );
	left_to_append -= append_amount;
    }

    // allocate pages to the record
    const smsize_t max_pages = 64;	// max pages to alloc per request
    smsize_t	num_pages = 0;	// number of new pages to allocate
    uint	pages_so_far = 0;	// pages appended so far
    bool	pages_alloced_already = false;
    lpid_t	*new_pages = new lpid_t[max_pages];

    if (!new_pages)
	W_FATAL(fcOUTOFMEMORY);
    w_auto_delete_array_t<lpid_t> ad_new_pages(new_pages);
  
    while(left_to_append > 0) {
	DBG(<<"left_to_append: " << left_to_append);
	pages_alloced_already = false;
	num_pages = (int) MIN(max_pages, 
			      ((left_to_append-1) / lgdata_p::data_sz)+1);
	DBG(<<"num_pages: " << num_pages
	    <<" max_pages: " << max_pages
	    <<" lgdata_p::data_sz: " << int(lgdata_p::data_sz));
	smsize_t append_cnt = MIN((smsize_t)(num_pages*lgdata_p::data_sz),
				   left_to_append);
	DBG(<<"append_cnt: " << append_cnt);

	// see if the record is implemented as chunks of pages
	if (rec->tag.flags & t_large_0) {
	    // attempt to add the new pages
	    const lg_tag_chunks_h lg_hdl(page, *(lg_tag_chunks_s*)rec->body());

	    DBGTHRD(<<" allocating " << num_pages << " new pages" );
	    W_DO(io->alloc_pages(lg_hdl.stid(), 
			lg_hdl.last_pid(),  // hint
		        num_pages, 
			new_pages,  // output -> array 
			false, // not may_realloc
			IX, // lock mode
			false
			)); 

#ifdef W_DEBUG
	    DBGTHRD(<< "Requested " << num_pages );
	    for(shpid_t j=0; j<num_pages; j++) {
		DBGTHRD(<<  new_pages[j] );
	    }
#endif /* W_DEBUG */

	    pages_alloced_already = true;
	    lg_tag_chunks_s new_lg_chunks(lg_hdl.chunk_ref());
	    lg_tag_chunks_h new_lg_hdl(page, new_lg_chunks);

	    rc_t rc = new_lg_hdl.append(num_pages, new_pages);
	    if (rc) {
		if (rc.err_num() != eBADAPPEND) {
		    return RC_AUGMENT(rc);
		} 
		// too many chunks, so convert to indirect-based
		// implementation.  rec->page_count() cannot be
		// used since it will be incorrect while we
		// are appending.
		DBG(<<"converting");
		W_DO( _convert_to_large_indirect(
			    page, slot, lg_hdl.page_count()));

		// record may have moved, so reacquire
		W_COERCE( page.get_rec(slot, rec) );
		w_assert3(rec->tag.flags & (t_large_1|t_large_2));

	    } else {
		DBGTHRD(<<"new_lg_hdl= " << new_lg_hdl);
		w_assert3(new_lg_hdl.page_count() == lg_hdl.page_count() + num_pages);
		// now update the actual lg_chunks on the page
		vec_t lg_vec(&new_lg_chunks, sizeof(new_lg_chunks));
		W_DO(page.splice_data(slot, 0, lg_vec.size(), lg_vec));
	    }
	} 

	// check agaIn for indirect-based implementation since
	// conversion may have been performed
	if (rec->tag.flags & (t_large_1|t_large_2)) {
	    const lg_tag_indirect_h lg_hdl(page, *(lg_tag_indirect_s*)rec->body(), rec->page_count()+pages_so_far);

	    if (!pages_alloced_already) {
		W_DO(
		    io->alloc_pages(
			lg_hdl.stid(), 
			lg_hdl.last_pid(), // hint
			num_pages, 
			new_pages,  // output -> array
			false, 	    // not may_realloc
			IX,	    // lock mode
			false
			)); 
		pages_alloced_already = true;
#ifdef W_DEBUG
		DBGTHRD(<< "Requested " << num_pages );
		for(shpid_t j=0; j<num_pages; j++) {
		    DBGTHRD(<<  new_pages[j] );
		}
#endif /* W_DEBUG */
	    }

	    lg_tag_indirect_s new_lg_indirect(lg_hdl.indirect_ref());
	    lg_tag_indirect_h new_lg_hdl(page, new_lg_indirect, rec_page_count+pages_so_far);
	    W_DO(new_lg_hdl.append(num_pages, new_pages));
	    if (lg_hdl.indirect_ref() != new_lg_indirect) {
		// change was made to the root
		// now update the actual lg_tag on the page
		vec_t lg_vec(&new_lg_indirect, sizeof(new_lg_indirect));
		W_DO(page.splice_data(slot, 0, lg_vec.size(), lg_vec));
	    }
	}
	W_DO(_append_to_large_pages(num_pages, new_pages, data, 
				    left_to_append) );
	w_assert3(left_to_append >= append_cnt);
	left_to_append -= append_cnt;

	pages_so_far += num_pages;
    }
    w_assert3(data.size() <= space_last_page ||
	    pages_so_far == ((data.size()-space_last_page-1) /
			     lgdata_p::data_sz + 1));
    return RCOK;
}

rc_t
file_m::_append_to_large_pages(int num_pages, const lpid_t new_pages[],
			       const vec_t& data, smsize_t left_to_append)
{
    int append_cnt;

    // Get the store flags in order to pass that info
    // down to for the fix() calls
    store_flag_t	store_flags;
    W_DO( io->get_store_flags(new_pages[0].stid(), store_flags));
    w_assert3(store_flags != st_bad);

    if (store_flags & st_insert_file)  {
	store_flags = (store_flag_t) (store_flags|st_tmp); 
	// is st_tmp and st_insert_file
    }

    for (int i = 0; i<num_pages; i++) {
	 
	append_cnt = MIN((smsize_t)lgdata_p::data_sz, left_to_append);
	w_assert3(append_cnt > 0);

	lgdata_p lgdata;
	/* NB:
	 * This is quite ugly when logging is considered:
	 * The first step results in 2 log records (keep in mind,
	 * this is for EACH page in the loop): page_init, page_insert,
	 * (NB: (neh) page_init, page_insert have been combined into
	 * page_format for lgdata_p)
	 * while the 2nd step results in a page splice.
	 * We *should* be able to do this in a way that generates
	 * ONE log record per page in this loop.
	 */

	/* NB: Causes page to be formatted: */
	W_DO(lgdata.fix(new_pages[i], LATCH_EX, lgdata.t_virgin, store_flags) );
    

	W_DO(lgdata.append(data, data.size() - left_to_append,
			   append_cnt));
	lgdata.unfix(); // make sure this is done early rather than in ~
	left_to_append -= append_cnt;
    }
    // This assert may not be true since data (and therefore
    // left_to_append) may be larger than the space on num_pages.
    // w_assert3(left_to_append == 0);
    return RCOK;
}

rc_t
file_m::_convert_to_large_indirect(file_p& page, slotid_t slot,
				   uint4_t rec_page_count)
// note that rec.page_count() cannot be used since the record
// is being appended to and it's body_size() is not accurate. 
{
    record_t*    rec;
    W_COERCE(page.get_rec(slot, rec));

    // const since only page update calls can update a page
    const lg_tag_chunks_h old_lg_hdl(page, *(lg_tag_chunks_s*)rec->body()) ;
    lg_tag_indirect_s lg_indirect(old_lg_hdl.stid().store);
    lg_tag_indirect_h lg_hdl(page, lg_indirect, rec_page_count);
    W_DO(lg_hdl.convert(old_lg_hdl));

    // overwrite the lg_tag_chunks_s with a lg_tag_indirect_s
    vec_t lg_vec(&lg_indirect, sizeof(lg_indirect));
    W_DO(page.splice_data(slot, 0, sizeof(lg_tag_chunks_s), lg_vec));

    //change type of object
    W_DO(page.set_rec_flags(slot, lg_hdl.indirect_type(rec_page_count)));

    return RCOK;
}

rc_t
file_m::_truncate_large(file_p& page, slotid_t slot, uint4_t amount)
{
    record_t*   rec;
    W_COERCE( page.get_rec(slot, rec) );

    uint4_t 	bytes_last_page = _bytes_last_page(rec->body_size());
    int		dealloc_count = 0; // number of pages to deallocate
    lpid_t	last_pid;

    // calculate the number of pages to deallocate
    if (amount >= bytes_last_page) {
	// 1 for last page + 1 for each full page
	dealloc_count = 1 + (int)(amount-bytes_last_page)/lgdata_p::data_sz;
    }

    if (rec->tag.flags & t_large_0) {
	const lg_tag_chunks_h lg_hdl(page, *(lg_tag_chunks_s*)rec->body()) ;
	lg_tag_chunks_s new_lg_chunks(lg_hdl.chunk_ref());
	lg_tag_chunks_h new_lg_hdl(page, new_lg_chunks) ;

	if (dealloc_count > 0) {
	    W_DO(new_lg_hdl.truncate(dealloc_count));
	    w_assert3(new_lg_hdl.page_count() == lg_hdl.page_count() - dealloc_count);
	    // now update the actual lg_tag on the page
	    vec_t lg_vec(&new_lg_chunks, sizeof(new_lg_chunks));
	    W_DO(page.splice_data(slot, 0, lg_vec.size(), lg_vec));
	}
	last_pid = lg_hdl.last_pid();
    } else {
	w_assert3(rec->tag.flags & (t_large_1|t_large_2));
	const lg_tag_indirect_h lg_hdl(page, *(lg_tag_indirect_s*)rec->body(), rec->page_count());
	lg_tag_indirect_s new_lg_indirect(lg_hdl.indirect_ref());
	lg_tag_indirect_h new_lg_hdl(page, new_lg_indirect, rec->page_count());

	if (dealloc_count > 0) {
	    W_DO(new_lg_hdl.truncate(dealloc_count));

	    // now update the actual lg_tag on the page
	    // if the tag has changed (change should only occur from
	    // reducing the number of levels of indirection
	    if (lg_hdl.indirect_ref() != new_lg_indirect) {
		vec_t lg_vec(&new_lg_indirect, sizeof(new_lg_indirect));
		W_DO(page.splice_data(slot, 0, lg_vec.size(), lg_vec));
	    }
	}
	last_pid = lg_hdl.last_pid();
    }

    if (amount < rec->body_size()) {
	/*
	 * remove data from the last page
	 */
	lgdata_p lgdata;
	W_DO( lgdata.fix(last_pid, LATCH_EX) );
	// calculate amount left on the new last page
	int4_t trunc_on_last_page = amount;
	if (dealloc_count > 0) {
	    trunc_on_last_page -= (dealloc_count-1)*lgdata_p::data_sz + bytes_last_page;
	}
	w_assert3(trunc_on_last_page >= 0);
	W_DO(lgdata.truncate(trunc_on_last_page));
    }
    return RCOK;
}


rc_t
file_p::format(const lpid_t& pid, tag_t tag, uint4_t flags, store_flag_t
store_flags)
{
    w_assert3(tag == t_file_p);

    file_p_hdr_t ctrl;
    ctrl.cluster = 0;		// dummy cluster ID
    vec_t vec;
    vec.put(&ctrl, sizeof(ctrl));

    /* first, don't log it */
    W_DO( page_p::format(pid, tag, flags, store_flags, false) );

    // always set the store_flag here -- see comments 
    // in bf::fix(), which sets the store flags to st_regular
    // for all pages, and lets the type-specific store manager
    // override (because only file pages can be insert_file)
    persistent_part().store_flags = store_flags;

    W_COERCE(page_p::reclaim(0, vec, false));

    /* Now, log as one (combined) record: -- 2nd 0 arg 
     * is to handle special case of reclaim */
    rc_t rc = log_page_format(*this, 0, 0, &vec);
    return rc;
}


rc_t
file_p::find_and_lock_next_slot(
    uint4_t                  space_needed,
    slotid_t&                idx
)
{
    return _find_and_lock_free_slot(true, space_needed, idx);
}
rc_t
file_p::find_and_lock_free_slot(
    uint4_t                  space_needed,
    slotid_t&                idx
)
{
    return _find_and_lock_free_slot(false, space_needed, idx);
}
rc_t
file_p::_find_and_lock_free_slot(
    bool		     append_only,
    uint4_t                  space_needed,
    slotid_t&                idx)
{
    FUNC(find_and_lock_free_slot);
    w_assert3(is_file_p());
    slotid_t start_slot = 0;  // first slot to check if free
    rc_t rc;

    for (;;) {
	if(append_only) {
	    W_DO(page_p::next_slot(space_needed, idx));
	} else {
	    W_DO(page_p::find_slot(space_needed, idx, start_slot));
	}
	// could be nslots() - new slot
	w_assert3(idx <= nslots());

	// try to lock the slot, but do not block
	rid_t rid(pid(), idx);
	rc = lm->lock(rid, EX, t_long, WAIT_IMMEDIATE);
	if (rc)  {
	    if (rc.err_num() == eLOCKTIMEOUT) {
		// slot is locked by another transaction, so find
		// another slot.  One choice is to start searching
		// for a new slot after the one just found.  
		// An alternative is to force the allocation of a 
		// new slot.  We choose the alternative potentially
		// attempting to get many locks on a page where
		// we've already noticed contention.

		DBG(<<"rc=" << rc);
		if(start_slot == nslots()) {
		    return RC(eRECWONTFIT);
		} else {
		    start_slot = nslots();  // force new slot allocation
		}
	    } else {
		// error we can't handle
		DBG(<<"rc=" << rc);
		return RC_AUGMENT(rc);
	    }
	} else {
	    // found and locked the slot
	    DBG(<<" found and locked slot " << idx);
	    break;
	}
    }
    return RCOK;
}

rc_t
file_p::get_rec(slotid_t idx, record_t*& rec)
{
    rec = 0;
    if (! is_rec_valid(idx)) 
	return RC(eBADSLOTNUMBER);
    rec = (record_t*) page_p::tuple_addr(idx);
    return RCOK;
}

// return slot with this serial number (0 if not found)
slotid_t file_p::serial_slot(const serial_t& s)
{
    slotid_t 	curr = 0;
    record_t*	rec = 0;

    curr = next_slot(curr); 
    while(curr != 0) {
	W_COERCE( get_rec(curr, rec) );
	if (rec->tag.serial_no == s) {
	    break;
	}
	curr = next_slot(curr); 
    }

    return curr;
}

rc_t
file_p::set_hdr(const file_p_hdr_t& new_hdr)
{
    vec_t v;
    v.put(&new_hdr, sizeof(new_hdr));
    W_DO( overwrite(0, 0, vec_t(&new_hdr, sizeof(new_hdr))) );
    return RCOK;
}

rc_t
file_p::splice_data(slotid_t idx, slot_length_t start, slot_length_t len, const vec_t& data)
{
    record_t*   rec;
    W_COERCE( get_rec(idx, rec) );
    w_assert3(rec != NULL);
    int         base = rec->body_offset();

    return page_p::splice(idx, base + start, len, data);
}

rc_t
file_p::append_rec(slotid_t idx, const vec_t& data)
{
    record_t*   rec;

    W_COERCE( get_rec(idx, rec) );
    w_assert3(rec != NULL);

    if (rec->is_small()) {
	W_DO( splice_data(idx, (int)rec->body_size(), 0, data) );
	W_COERCE( get_rec(idx, rec) );
	w_assert3(rec != NULL);
    } else {
	// not implemented here.  see file::append_rec
	return RC(smlevel_0::eNOTIMPLEMENTED);
    }

    return RCOK;
}

rc_t
file_p::truncate_rec(slotid_t idx, uint4_t amount)
{
    record_t*   rec;
    W_COERCE( get_rec(idx, rec) );
    w_assert3(rec);

    vec_t	empty(rec, 0);  // zero length vector 

    w_assert3(amount <= rec->body_size());
    w_assert1(rec->is_small());
    W_DO( splice_data(idx, (int)(rec->body_size()-amount), 
		      (int)amount, empty) );

    return RCOK;
}

rc_t
file_p::set_rec_len(slotid_t idx, uint4_t new_len)
{
    record_t*   rec;
    W_COERCE( get_rec(idx, rec) );
    w_assert3(rec != NULL);

    vec_t   hdr_update(&(new_len), sizeof(rec->tag.body_len));
    int     size_location = ((char*)&rec->tag.body_len) - ((char*)rec);

    W_DO(splice(idx, size_location, sizeof(rec->tag.body_len), hdr_update) );
    return RCOK;
}

rc_t
file_p::set_rec_flags(slotid_t idx, uint2_t new_flags)
{
    record_t*   rec;
    W_COERCE( get_rec(idx, rec) );
    w_assert3(rec);

    vec_t   flags_update(&(new_flags), sizeof(rec->tag.flags));
    int     flags_location = ((char*)&(rec->tag.flags)) - ((char*)rec);

    W_DO(splice(idx, flags_location, sizeof(rec->tag.flags), flags_update) );
    return RCOK;
}

slotid_t file_p::next_slot(slotid_t curr)
{
    w_assert3(curr >=0);

    for (curr = curr+1; curr < nslots(); curr++) {
	if (is_tuple_valid(curr)) {
	    return curr;
	}
    }

    // error
    w_assert3(curr == nslots());
    return 0;
}

int file_p::rec_count()
{
    int nrecs = 0;
    int nslots = page_p::nslots();

    for(slotid_t slot = 1; slot < nslots; slot++) {
        if (is_tuple_valid(slot)) nrecs++;
    }

    return nrecs;
}

recflags_t 
file_p::choose_rec_implementation(
    uint4_t 	est_hdr_len,
    smsize_t 	est_data_len,
    smsize_t& 	rec_size // output: size of stuff going into slotted pg
    )
{
    est_hdr_len = sizeof(rectag_t) + align(est_hdr_len);
    w_assert3(is_aligned(est_hdr_len));

    if ( (est_data_len+est_hdr_len) <= file_p::data_sz) {
	rec_size = est_hdr_len + align(est_data_len)+sizeof(slot_t);
	return(t_small);
    } else {
	rec_size = est_hdr_len + align(sizeof(lg_tag_chunks_s))+sizeof(slot_t);
	return(t_large_0);
    }
     
    W_FATAL(eNOTIMPLEMENTED);
    return t_badflag;  // keep compiler quite
}

void file_p::ntoh()
{
    // vid_t vid = pid().vol();
    // TODO: byteswap the records on the page
}

MAKEPAGECODE(file_p, page_p);

//DU DF
rc_t
file_m::get_du_statistics(
    const stid_t& fid, 
    const stid_t& lfid, 
    file_stats_t& file_stats,
    bool	  audit)
{
    FUNC(file_m::get_du_statistics)
    lpid_t	lpid;
    bool  	eof = false;
    file_p 	page;
    bool	allocated;
    base_stat_t file_pg_cnt = 0;
    base_stat_t unalloc_file_pg_cnt = 0;
    base_stat_t lgdata_pg_cnt = 0;
    base_stat_t lgindex_pg_cnt = 0;

    DBG(<<"analyze file " << fid);
    // analyzes an entire file

    {
	store_flag_t	store_flags;

	//
	// TODO: it would be more efficient if we
	// got the store flags passed down from the
	// caller, because it's more than likely figured out
	// by the caller.
	//
	// get store flags so we know whether we
	// can analyze the file or have to simply
	// count the #extents allocated to it.
	// This is the case for temp files (it's possible
	// that we just recovered and the pages aren't
	// intact, but the extents are, of courese.
	// (This is also the case for files that are marked
	// for deletion at the end of this xct, but they
	// don't appear in the directory, so we never
	// get called for such stores.)

	W_DO( io->get_store_flags(fid, store_flags));
	w_assert3(store_flags != st_bad);

	DBG(<<"store flags " << store_flags);
	if(store_flags & st_tmp) {
	    SmStoreMetaStats stats;
	    stats.Clear();

	    W_COERCE( io->get_store_meta_stats(fid, stats) );
	    DBG(<<"unalloc_file_pg_cnt +=" << stats.numReservedPages);
	    file_stats.unalloc_file_pg_cnt += stats.numReservedPages;

	    // Now do same for large object file
	    stats.Clear();
	    W_COERCE( io->get_store_meta_stats(lfid, stats) );
	    DBG(<<"unalloc_file_pg_cnt +=" << stats.numReservedPages);
	    file_stats.unalloc_file_pg_cnt += stats.numReservedPages;
	    return RCOK;
	}
    }

    W_DO(first_page(fid, lpid, &allocated) );
    DBG(<<"first page of " << fid << " is " << lpid << " (allocate=" << allocated << ")");

    // scan each page of this file (large file lfid is handled later)
    while ( !eof ) {
	if (allocated) {
	    file_pg_stats_t file_pg_stats;
	    lgdata_pg_stats_t lgdata_pg_stats;
	    lgindex_pg_stats_t lgindex_pg_stats;
	    file_pg_cnt++;

	    // In order to avoid latch-lock deadlocks,
	    // we have to lock the page first
	    W_DO(lm->lock_force(lpid, SH, t_long, WAIT_SPECIFIED_BY_XCT));

	    W_DO( page.fix(lpid, LATCH_SH, 0/*page_flags */));

	    W_DO(page.hdr_stats(file_pg_stats));

	    DBG(<< "getting slot stats for page " << lpid); 
	    W_DO(page.slot_stats(0/*all slots*/, file_pg_stats,
		    lgdata_pg_stats, lgindex_pg_stats, lgdata_pg_cnt,
		    lgindex_pg_cnt));
	    page.unfix(); // avoid latch-lock deadlocks.

	    if (audit) {
		W_DO(file_pg_stats.audit()); 
		W_DO(lgdata_pg_stats.audit()); 
		W_DO(lgindex_pg_stats.audit()); 
	    }
	    file_stats.file_pg.add(file_pg_stats);
	    file_stats.lgdata_pg.add(lgdata_pg_stats);
	    file_stats.lgindex_pg.add(lgindex_pg_stats);

	} else {
	    unalloc_file_pg_cnt++;
	}

	// read the next page
	W_DO(next_page(lpid, eof, &allocated) );
	DBG(<<"next page of " << fid << " is " << lpid << " (allocate=" << allocated << ")");

    }

    DBG(<<"analyze large object file " << lfid);
    W_DO(first_page(lfid, lpid, &allocated) );

    DBG(<<"first page of " << lfid << " is " << lpid << " (allocate=" << allocated << ")");

    base_stat_t lg_pg_cnt = 0;
    base_stat_t lg_page_unalloc_cnt = 0;
    eof = false;
    while ( !eof ) {
	if (allocated) {
	    lg_pg_cnt++;
	} else {
	    lg_page_unalloc_cnt++;
	}
	DBG( << "lpid=" << lpid
	    <<"lg_pg_cnt = " << lg_pg_cnt
	    << " lg_page_unalloc_cnt = " << lg_page_unalloc_cnt
	);
	W_DO(next_page(lpid, eof, &allocated) );
	DBG(<<"next page of " << lfid << " is " << lpid << " (allocate=" << allocated << ")");
    }

#ifdef W_DEBUG
    if(audit) {
	/*
	 * NB: this check is meaningful ONLY if the proper
	 * locks were grabbed, i.e., if audit==true.
	 * (Otherwise, another tx can be changing the file
	 * during the stats-gathering, such as is done in
	 * script alloc.2 (one tx is gathering stats,
	 * the other rolling back some deletes)
	 */
	if(lg_pg_cnt != lgdata_pg_cnt + lgindex_pg_cnt) {
	    cerr << "lg_pg_cnt= " << lg_pg_cnt
		 << " BUT lgdata_pg_cnt= " << lgdata_pg_cnt
		 << " + lgindex_pg_cnt= " << lgindex_pg_cnt
		 << " = " << lgdata_pg_cnt + lgindex_pg_cnt
		 << endl;
	    w_assert1(0);
	}
    }
#endif /* W_DEBUG */

    if(audit) {
	w_assert3((lg_pg_cnt + lg_page_unalloc_cnt) % smlevel_0::ext_sz == 0);
	w_assert3(lg_pg_cnt == lgdata_pg_cnt + lgindex_pg_cnt);
    }

    file_stats.file_pg_cnt += file_pg_cnt;
    file_stats.lgdata_pg_cnt += lgdata_pg_cnt;
    file_stats.lgindex_pg_cnt += lgindex_pg_cnt;
    file_stats.unalloc_file_pg_cnt += unalloc_file_pg_cnt;
    file_stats.unalloc_large_pg_cnt += lg_page_unalloc_cnt;
    
    DBG(<<"DONE analyze file " << fid);
    return RCOK;
}

rc_t
file_p::hdr_stats(file_pg_stats_t& file_pg_stats)
{
    // file_p overhead is:
    //	page hdr + first slot (containing file_p specific  stuff)
    file_pg_stats.hdr_bs += hdr_size() + sizeof(page_p::slot_t) + align(tuple_size(0));
    file_pg_stats.free_bs += persistent_part().space.nfree();
    return RCOK;
}

rc_t
file_p::slot_stats(slotid_t slot, file_pg_stats_t& file_pg,
	    lgdata_pg_stats_t& lgdata_pg,
	    lgindex_pg_stats_t& lgindex_pg, base_stat_t& lgdata_pg_cnt,
	    base_stat_t& lgindex_pg_cnt)
{
    FUNC(file_p::slot_stats);

    slotid_t 	start_slot = slot == 0 ? 1 : slot;
    slotid_t 	end_slot = slot == 0 ? num_slots()-1 : slot;
    record_t*	rec;
   
    DBG(<<"start_slot=" << start_slot << " end_slot=" << end_slot);
    //scan all valid records in this page
    for (slotid_t sl = start_slot; sl <= end_slot; sl++) {
	if (!is_rec_valid(sl)) {
	    file_pg.slots_unused_bs += sizeof(slot_t);
	} else {
	    file_pg.slots_used_bs += sizeof(slot_t);
	    rec = (record_t*) page_p::tuple_addr(sl);

	    file_pg.rec_tag_bs += sizeof(rectag_t);
	    file_pg.rec_hdr_bs += rec->hdr_size();
	    file_pg.rec_hdr_align_bs += align(rec->hdr_size()) -
					rec->hdr_size();

	    if ( rec->is_small() ) {
		DBG(<<"small rec");
		file_pg.small_rec_cnt++;
		file_pg.rec_body_bs += rec->body_size();
		file_pg.rec_body_align_bs += align(rec->body_size()) -
					     rec->body_size();
	    } else if ( rec->is_large() ) {
		DBG(<<"large rec");
		file_pg.lg_rec_cnt++;
		base_stat_t lgdata_cnt = rec->page_count();
		lgdata_pg_cnt += lgdata_cnt;

		lgdata_pg.hdr_bs += lgdata_cnt * (page_sz - lgdata_p::data_sz);
		lgdata_pg.data_bs += rec->body_size();
		lgdata_pg.unused_bs += lgdata_cnt*lgdata_p::data_sz - rec->body_size();
		if ( rec->tag.flags & t_large_0 ) {
		    file_pg.rec_lg_chunk_bs += align(sizeof(lg_tag_chunks_s));
#ifdef W_TRACE
		    {
			const lg_tag_chunks_h lg_hdl(*this, 
				*(lg_tag_chunks_s*)rec->body());
			DBG(
			    <<", npages " << lg_hdl.page_count()
			    <<", last page " << lg_hdl.last_pid()
			    <<", store " << lg_hdl.stid() );
		    }
#endif
		} else if (rec->tag.flags & (t_large_1|t_large_2)) {
#ifdef W_TRACE
		    {
			const lg_tag_indirect_h lg_hdl(*this, 
				*(lg_tag_indirect_s*)rec->body(), 
				rec->page_count());
			DBG(
			    <<", last page " << lg_hdl.last_pid()
			    <<", store " << lg_hdl.stid() );
		    }
#endif
		    file_pg.rec_lg_indirect_bs += align(sizeof(lg_tag_indirect_s));
		    base_stat_t lgindex_cnt = 0;
    		    if (rec->tag.flags & t_large_1) {
			lgindex_cnt = 1;
		    } else {
			lgindex_cnt += (lgdata_cnt-1)/lgindex_p::max_pids+2;
		    }
		    lgindex_pg.used_bs += lgindex_cnt * (page_sz - lgindex_p::data_sz);
		    lgindex_pg.used_bs += (lgindex_cnt-1 + lgdata_cnt)*sizeof(shpid_t);
		    lgindex_pg.unused_bs +=
			(lgindex_p::data_sz*lgindex_cnt) -
			((lgindex_cnt-1 + lgdata_cnt)*sizeof(shpid_t));

		    lgindex_pg_cnt += lgindex_cnt;

		} else {
		    W_FATAL(eINTERNAL);
		}
		DBG(<<"lgdata_cnt (rec->page_count()) = " << lgdata_cnt
			<< " rec is slot " << sl
			<< " lgdata_pg_cnt(sum) = " << lgdata_pg_cnt
		    );
	    } else {
		W_FATAL(eINTERNAL);
	    }
	}
    }
    DBG(<<"slot_stats returns lgdata_pg_cnt=" << lgdata_pg_cnt
	    << " lgindex_pg_cnt=" << lgindex_pg_cnt );
    return RCOK;
}


//
// Override the record tag. This is used for large object sort. 
//
rc_t
file_m::update_rectag(const rid_t& rid, uint4_t len, uint2_t flags)
{
    file_p page;
    
    W_DO(_locate_page(rid, page, LATCH_EX) );
    
    W_DO( page.set_rec_len(rid.slot, len) );
    W_DO( page.set_rec_flags(rid.slot, flags) );
    
    return RCOK;
}


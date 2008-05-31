/*<std-header orig-src='shore' incl-file-exclusion='PAGE_S_H'>

 $Id: page_s.h,v 1.32 2008/05/31 05:03:32 nhall Exp $

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

#ifndef PAGE_S_H
#define PAGE_S_H

#include "w_defines.h"

/*  -- do not edit anything above this line --   </std-header>*/

#ifdef __GNUG__
#pragma interface
#endif

/* BEGIN VISIBLE TO APP */

/*
 * Basic page structure for all pages.
 */
class xct_t;

class page_s {
public:
#ifdef LARGE_PAGE
    typedef w_base_t::int4_t  slot_offset_t; // -1 if vacant
    typedef w_base_t::uint4_t  slot_length_t;
#else
    typedef w_base_t::int2_t  slot_offset_t; // -1 if vacant
    typedef w_base_t::uint2_t slot_length_t;
#endif

    typedef slot_offset_t  page_bytes_t; // # bytes on a page, 
    		                         // reserved space, etc
					 // was int2.

    struct slot_t {
	slot_offset_t offset;		// -1 if vacant
	slot_length_t length;
    };
    
    class space_t {
    public:
	space_t()	{};
	~space_t()	{};
	
	void init(int nfree, bool rflag)  { 
	    _tid = tid_t(0, 0);
	    _nfree = nfree;
	    _nrsvd = _xct_rsvd = 0;
#ifndef BACKOUT_NEHFIX2
	    _nrsvd_hiwat = rflag ? 1 : 0;
#else
	    _rflag = rflag;
#endif
	}
	
	int nfree() const	{ return _nfree; }
#ifndef BACKOUT_NEHFIX2
	bool rflag() const	{ return _nrsvd_hiwat!=0; }
#else
	bool rflag() const	{ return _rflag!=0; }
#endif
	
	int			usable(xct_t* xd); // might free space
				// slot_bytes means bytes for new slots
	rc_t			acquire(int amt, int slot_bytes, xct_t* xd,
					bool do_it=true);
	void 			release(int amt, xct_t* xd);
	void 			undo_acquire(int amt, xct_t* xd);
	void 			undo_release(int amt, xct_t* xd);
	const tid_t&		tid() const { return _tid; }
	page_bytes_t		nrsvd() const { return _nrsvd; }
#ifndef BACKOUT_NEHFIX2
	page_bytes_t		nrsvd_hiwat() const { return _nrsvd_hiwat; }
#else
#endif
	page_bytes_t		xct_rsvd() const { return _xct_rsvd; }


    private:
   
	void _check_reserve();
	
	tid_t	_tid;		// youngest xct contributing to _nrsvd
	// 8 bytes
#if (SM_PAGESIZE >= 65536) && !defined(LARGE_PAGE)
#error Page sizes this large are not supported without --enable-largepages
#endif

	page_bytes_t	_nfree;		// free space counter
	// 10 bytes or 12 bytes
	page_bytes_t	_nrsvd;		// reserved space counter
	// 12 bytes or 16 bytes
	page_bytes_t	_xct_rsvd;	// amt of space contributed by _tid to _nrsvd
	// 14 bytes or 20 bytes
#ifndef BACKOUT_NEHFIX2
// NEHFIX2 5/11/06 Keep track of highwater mark for
// data use on the page. Don't let the slot table grow
// into that high water area until the tx commits.
				// in the interest of not using any more
				// space, we'll overload rflag
				// and _nrsvd_hiwat.
				// highwater is 0 only for _rflag == 0
				// and is a minimum of 1 for _rflag == 1
				// This works because the minimum amt
				// that can be reserved is 4 bytes.
	page_bytes_t	_nrsvd_hiwat;
	// 16/24 bytes
#else
	page_bytes_t	_rflag; 
	// 16/24 bytes
#endif
        // 16/24 -- 8-byte aligned

    };

    // This enum must match the data attributes below.
    enum {
	    hdr_sz = (0
		      + 2 * sizeof(lsn_t)  // start & end of page
		      + 2 * sizeof(slot_offset_t)
#if (ALIGNON == 0x8) && !defined(LARGE_PAGE)
		      + 1 * sizeof(fill4)
#endif
		      + 2 * sizeof(shpid_t) 
		      + 1 * sizeof(lpid_t)
		      + 1 * sizeof(space_t)
		      + 1 * sizeof(slot_offset_t)
		      + 1 * sizeof(uint2_t)
#if defined(LARGE_PAGE)
		      + 1 * sizeof(fill2)
#endif
		      + 2 * sizeof(w_base_t::int4_t)
#if (ALIGNON == 0x8) && !defined(LARGE_PAGE)
		      + 1 * sizeof(fill4)
#endif
		      + 2 * sizeof(slot_t)
		      + 0),
	    data_sz = (smlevel_0::page_sz - hdr_sz),
	    max_slot = data_sz / sizeof(slot_t) + 2
    };

 
    lsn_t	        lsn1; // 16 bytes
    // 16 -- 8-byte aligned
    slot_offset_t	nslots;		// number of slots : 2 or 4 bytes
    // 2/4
    slot_offset_t	nvacant;	// number of vacant slots : 2 or 4 bytes
    // 4/8
#if (ALIGNON == 0x8) && !defined(LARGE_PAGE)
    /* Yes, the conditions above are  correct.  The default is 8 byte
       alignment.  If compatability mode is wanted, it reverts to 4 ...
       unless 8 byte alignment is specified, in which case we need 
       the filler. */
    /* XXX really want alignment to the aligned object size for
       a particular architecture.   Fortunately this works for all
       common platforms and the above types.  A better solution would
       be to specify a desired alignment, or make 'data' a union which
       contains the type that has the most restrictive alignment
       constraints. */
    fill4	_fill0;			// align to 8-byte boundary
#endif
    // 8/8 
    // 8-byte aligned
    
    shpid_t	next;			// next page : 4 or 8 bytes in size
    // 4/8
    shpid_t	prev;			// previous page : 4 or 8 bytes in size
    // 8/16
    // 8-byte aligned
    
    lpid_t	pid;			// id of the page : 16 bytes with fill
    // 16
    // 8-byte aligned
   
    space_t 	space;			// space management : 16/24 bytes
    // 16/24
    // 8-byte aligned

    slot_offset_t end;	        // offset to end of data area  
    // 2/4   -------- 4-byte aligned if LARGE_PAGE
    uint2_t	tag;	        // page_p::tag_t 
    // 4/6
#if defined(LARGE_PAGE)
    fill2       _fill2;		
    // 4/8
#endif
    w_base_t::uint4_t	store_flags;    // page_p::store_flag_t
    // 8/12  
    w_base_t::uint4_t	page_flags;     // page_p::page_flag_t
    // 12/16  
    //
#if (ALIGNON == 0x8) && !defined(LARGE_PAGE)
    /* Yes, the conditions above are  correct.  The default is 8 byte
       alignment.  If compatability mode is wanted, it reverts to 4 ...
       unless 8 byte alignment is specified, in which case we need 
       the filler. */
    /* XXX really want alignment to the aligned object size for
       a particular architecture.   Fortunately this works for all
       common platforms and the above types.  A better solution would
       be to specify a desired alignment, or make 'data' a union which
       contains the type that has the most restrictive alignment
       constraints. */
    fill4	_fill1;			// align to 8-byte boundary
#endif
    // 16/16
    
    char 	data[data_sz];		// must be aligned
    slot_t	reserved_slot[1];	// 2nd slot (declared to align
					// end of _data)
    slot_t	slot[1];		// 1st slot
    lsn_t	lsn2;
    void ntoh(vid_t vid);

    // XXX this class is really intended to be a "STRUCT" as a bunch of
    // things layed out in memory.  Unfortunately it includes
    // some _classes_ which makes that a bit difficult.   Why?
    // ... well that means that it may need to be constructed or
    // destructed.   Probably the best way to handle this is to make
    // a static method  which creates page_s instances from a memory
    // buffer ... and destroy them the same way, thence fixing the
    // constructor/destructor problem.   This is a work-around for now.
    page_s() { }
    ~page_s() { }
};

/* END VISIBLE TO APP */

/*<std-footer incl-file-exclusion='PAGE_S_H'>  -- do not edit anything below this line -- */

#endif          /*</std-footer>*/

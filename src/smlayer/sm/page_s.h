/*<std-header orig-src='shore' incl-file-exclusion='PAGE_S_H'>

 $Id: page_s.h,v 1.27 2002/01/15 23:47:24 bolo Exp $

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
    typedef int2_t slot_offset_t; // -1 if vacant
    typedef uint2_t slot_length_t;
    struct slot_t {
	slot_offset_t offset;		// -1 if vacant
	slot_length_t length;
    };
    
    class space_t {
    public:
	space_t()	{};
	~space_t()	{};
	
	void init(int nfree, int rflag)  { 
	    _tid = tid_t(0, 0);
	    _nfree = nfree;
	    _nrsvd = _xct_rsvd = 0;
	    _rflag = rflag;
	}
	
	int nfree() const	{ return _nfree; }
	bool rflag() const	{ return _rflag!=0; }
	
	int			usable(xct_t* xd); // might free space
				// slot_bytes means bytes for new slots
	rc_t			acquire(int amt, int slot_bytes, xct_t* xd,
					bool do_it=true);
	void 			release(int amt, xct_t* xd);
	void 			undo_acquire(int amt, xct_t* xd);
	void 			undo_release(int amt, xct_t* xd);
	const tid_t&		tid() const { return _tid; }
	int2_t			nrsvd() const { return _nrsvd; }
	int2_t			xct_rsvd() const { return _xct_rsvd; }


    private:
   
	void _check_reserve();
	
	tid_t	_tid;		// youngest xct contributing to _nrsvd
	/* NB: this use of int2_t prevents us from having 65K pages */
#if SM_PAGESIZE >= 65536
#error Page sizes this big are not supported
#endif
	int2_t	_nfree;		// free space counter
	int2_t	_nrsvd;		// reserved space counter
	int2_t	_xct_rsvd;	// amt of space contributed by _tid to _nrsvd
	int2_t	_rflag;
    };
    enum {
	    hdr_sz = (0
		      + 2 * sizeof(lsn_t) 
		      + sizeof(lpid_t)
		      + 2 * sizeof(shpid_t) 
		      + sizeof(space_t)
		      + 4 * sizeof(int2_t)
		      + 2 * sizeof(int4_t)
		      + 2 * sizeof(slot_t)
#if !defined(SM_ODS_COMPAT_14) || (ALIGNON == 0x8)
		      + sizeof(fill4)
#endif
		      + 0),
	    data_sz = (smlevel_0::page_sz - hdr_sz),
	    max_slot = data_sz / sizeof(slot_t) + 2
    };

 
    lsn_t	lsn1;
    lpid_t	pid;			// id of the page
    shpid_t	next;			// next page
    shpid_t	prev;			// previous page
    space_t 	space;			// space management
    uint2_t	end;			// offset to end of data area
    int2_t	nslots;			// number of slots
    int2_t	nvacant;		// number of vacant slots
    uint2_t	tag;			// page_p::tag_t
    uint4_t	store_flags;		// page_p::store_flag_t
    uint4_t	page_flags;		// page_p::page_flag_t
#if !defined(SM_ODS_COMPAT_14) || (ALIGNON == 0x8)
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
    char 	data[data_sz];		// must be aligned
    slot_t	reserved_slot[1];	// 2nd slot (declared to align
					// end of _data)
    slot_t	slot[1];		// 1st slot
    lsn_t	lsn2;
    void ntoh(vid_t vid);
};

/* END VISIBLE TO APP */

/*<std-footer incl-file-exclusion='PAGE_S_H'>  -- do not edit anything below this line -- */

#endif          /*</std-footer>*/

/*<std-header orig-src='shore'>

  $Id: page.cpp,v 1.143 2007/05/18 21:43:26 nhall Exp $

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
#define PAGE_C
#ifdef __GNUG__
#   pragma implementation "page.h"
#   pragma implementation "page_s.h"
#   pragma implementation "page_h.h"
#endif
#include <sm_int_1.h>
#include <page.h>
#include <page_h.h>

/*
NEHFIX1 5/8/06: in bookkeeping for reserved space, we have to
subtract the total taken from nrsvd. See the comments
below for NEHFIX1
*/

/*********************************************************************
 *
 *  page_s::space_t::_check_reserve()
 *
 *  Check if reserved space on the page can be freed
 *
 *********************************************************************/
INLINE void page_s::space_t::_check_reserve()
{
	w_assert3(rflag());
	w_assert3(nfree() >= nrsvd());
	w_assert3(nrsvd() >= xct_rsvd());
	w_assert1(nfree() >= 0 && nrsvd() >= 0 && xct_rsvd() >= 0);

	if (_tid < xct_t::oldest_tid())  {
		/*
		 *  youngest xct to reserve space is completed and
		 * because it's < oldest tx, this means all
		 * tx to reserve space have completed.
		 *  --- all reservations can be released.
		 */
		_tid = _nrsvd = _xct_rsvd = 0;
#ifndef BACKOUT_NEHFIX2
		_nrsvd_hiwat =  1; // not zero because it
		// doubles as rflag(), and min space allocation is
		// at least 4 bytes anyway
#endif
	}
	w_assert3(nfree() >= nrsvd());
	w_assert3(nrsvd() >= xct_rsvd());
	w_assert1(nfree() >= 0 && nrsvd() >= 0 && xct_rsvd() >= 0);
}

/*********************************************************************
 *
 *  page_s::space_t::usable(xd)
 *
 *  Compute the usable space on the page for the transaction xd.
 *  Might free up space (by calling _check_reserve)
 *
 *********************************************************************/
int
page_s::space_t::usable(xct_t* xd) 
{
	w_assert3(nfree() >= nrsvd());
	w_assert3(nrsvd() >= xct_rsvd());
	w_assert1(nfree() >= 0 && nrsvd() >= 0 && xct_rsvd() >= 0);

	if (rflag()) _check_reserve();
	int avail = nfree() - nrsvd();

	if (rflag()) {
		if(xd)  {
			if (xd->state() == smlevel_1::xct_aborting)  {
				/*
				 *  An aborting transaction can use all reserved space
				 */
				avail += nrsvd();
			} else if (xd->tid() == _tid) {
				/*
				 *  An active transaction can reuse all space it freed
				 */
				avail += xct_rsvd();
			}
		} else if (smlevel_0::redo_tid &&
				*smlevel_0::redo_tid == _tid) {
			/*
			 *  This is the same as an active transaction (above)
			 *  during a restart.
			 */
			avail += xct_rsvd();
		}
	}
	w_assert3(nfree() >= nrsvd());
	w_assert3(nrsvd() >= xct_rsvd());
	w_assert1(nfree() >= 0 && nrsvd() >= 0 && xct_rsvd() >= 0);

#ifndef BACKOUT_NEHFIX2
	// available cannot be more than larger of (nfree() - nrsvd_hiwat(), 0)
	// (highwater can be greater than nfree because highwater
	// doesn't shrink)
	int maximum  = nfree() - nrsvd_hiwat(); 
	if(maximum < 0) maximum = 0;
	if(avail > maximum) avail = maximum;
#endif
	return avail;
}



/*********************************************************************
 *
 *  page_s::space_t::undo_release(amt, xd)
 *
 *  Undo the space release (i.e. re-acquire) for the xd. Amt bytes
 *  are reclaimed.
 *
 *********************************************************************/
	void 
page_s::space_t::undo_release(int amt, xct_t* xd)
{
#ifndef BACKOUT_NEHFIX2
	DBG(<<"{space_t::undo_release  amt=" << amt 
			<< " _tid=" << _tid
			<< " nfree()=" << nfree()
			<< " nrsvd()=" << nrsvd()
			<< " xct_rsvd()=" << xct_rsvd()
	   );
#else
	DBG(<<"{space_t::undo_release  amt=" << amt 
			<< " _tid=" << _tid
			<< " nfree()=" << nfree()
			<< " nrsvd()=" << nrsvd()
			<< " xct_rsvd()=" << xct_rsvd()
	   );
#endif
	w_assert3(nfree() >= nrsvd());
	w_assert3(nrsvd() >= xct_rsvd());
	w_assert1(nfree() >= 0 && nrsvd() >= 0 && xct_rsvd() >= 0);

	_nfree -= amt;

	// NB: in rollback, we don't free the slot bytes, so they don't get
	// added into the _nrsvd. Thus the _nrsvd can end up smaller than
	// the amount that we are wanting to reacquire.  
	_nrsvd -= amt;
#ifndef BACKOUT_NEHFIX2
	if(_nrsvd < 0) _nrsvd = 0;

	if (xd && xd->tid() == _tid) {
		_xct_rsvd -= amt;
		if(_xct_rsvd < 0) _xct_rsvd = 0;
	}
#else
	if (xd && xd->tid() == _tid) 
		_xct_rsvd -= amt;
#endif
    
#ifndef BACKOUT_NEHFIX2
        DBG(<<" space_t::undo_release amt=" << amt 
			<< " _tid=" << _tid
			<< " nfree()=" << nfree()
			<< " nrsvd()=" << nrsvd()
			<< " xct_rsvd()=" << xct_rsvd()
			<< " hiwat()=" << nrsvd_hiwat()
			<< "}"
			);
#else
        DBG(<<" space_t::undo_release amt=" << amt 
			<< " _tid=" << _tid
			<< " nfree()=" << nfree()
			<< " nrsvd()=" << nrsvd()
			<< " xct_rsvd()=" << xct_rsvd()
			<< "}"
			);
#endif
    w_assert1(nfree() >= 0 && nrsvd() >= 0 && xct_rsvd() >= 0);
    w_assert3(nrsvd() >= xct_rsvd());
    w_assert1(nfree() >= nrsvd()); // NEW
}


/*********************************************************************
 *
 *  page_s::space_t::undo_acquire(amt, xd)
 *
 *  Undo the space acquire (i.e. re-release) for the xd. Amt 
 *  bytes are freed.
 *
 *********************************************************************/
void 
page_s::space_t::undo_acquire(int amt, xct_t* xd)
{
    DBG(<<"{space_t::undo_acquire amt=" << amt 
			<< " _tid=" << _tid
			<< " nfree()=" << nfree()
			<< " nrsvd()=" << nrsvd()
			<< " xct_rsvd()=" << xct_rsvd()
			);
    w_assert3(nfree() >= nrsvd());
    w_assert3(nrsvd() >= xct_rsvd());
    w_assert1(nfree() >= 0 && nrsvd() >= 0 && xct_rsvd() >= 0);

    _nfree += amt;
    if (xd && _tid >= xd->tid())  {
	_nrsvd += amt; // update highwater mark below

	if (_tid == xd->tid())
	    _xct_rsvd += amt;

#ifndef BACKOUT_NEHFIX2
	// I don't think we have to bump up the highwater mark
	// during undo. We only need to keep track of what we
	// freed up during forward processing.
	if(nrsvd() > 0) {
            // it's possible to release 0 bytes for a zero-length
	    // record
            _nrsvd_hiwat = (nrsvd_hiwat() < nrsvd() ? nrsvd() : nrsvd_hiwat());
	}   
#endif
    }
    DBG(<<"space_t::undo_acquire amt=" << amt 
			<< " _tid=" << _tid
			<< " nfree()=" << nfree()
			<< " nrsvd()=" << nrsvd()
			<< " xct_rsvd()=" << xct_rsvd()
			<< "}"
			);
    w_assert3(nfree() >= nrsvd());
    w_assert3(nrsvd() >= xct_rsvd());
    w_assert1(nfree() >= 0 && nrsvd() >= 0 && xct_rsvd() >= 0);
}



/*********************************************************************
 *
 *  page_s::space_t::acquire(amt, xd, do_it)
 *
 *  Acquire amt bytes for the transaction xd.
 *  The amt bytes will affect the reserved space on the page. 
 *  The slot_bytes will not change the reserved space on the
 *  page.  This is necessary, since space for slots for destroyed
 *  records cannot be returned to the free pool since slot
 *  numbers cannot change.  (See comments for space_t::release)
 *
 *  If do_it is false, don't actually do the update, just
 *  return RCOK or RC(smlevel_0::eRECWONTFIT);
 *
 *********************************************************************/
rc_t
page_s::space_t::acquire(int amt, int slot_bytes, xct_t* xd,
	bool do_it /*=true */)
{
#ifndef BACKOUT_NEHFIX2
    DBG(<<"{space_t::acquire amt=" << amt << " slot_bytes=" << slot_bytes
			<< " _tid=" << _tid
			<< " nfree()=" << nfree()
			<< " nrsvd()=" << nrsvd()
			<< " xct_rsvd()=" << xct_rsvd()
			<< " hiwat()=" << nrsvd_hiwat()
			);
#else
    DBG(<<"{space_t::acquire amt=" << amt << " slot_bytes=" << slot_bytes
			<< " _tid=" << _tid
			<< " nfree()=" << nfree()
			<< " nrsvd()=" << nrsvd()
			<< " xct_rsvd()=" << xct_rsvd()
			);
#endif
    w_assert3(nfree() >= nrsvd());
    w_assert3(nrsvd() >= xct_rsvd());
    w_assert1(nfree() >= 0 && nrsvd() >= 0 && xct_rsvd() >= 0);

    if (do_it && rflag() && xd && xd->state() == smlevel_1::xct_aborting)  {
		/*
		 *  For aborting transaction ...
		 */
        DBG(<<"aborting tx. undo release of " << amt << "}" );
	undo_release(amt, xd);

        w_assert3(nfree() >= nrsvd());
        w_assert3(nrsvd() >= xct_rsvd());
        w_assert1(nfree() >= 0 && nrsvd() >= 0 && xct_rsvd() >= 0);

	return RCOK;
    }
    
    int avail = usable(xd);
    int total = amt + slot_bytes;

    if (avail < total)  {
        DBG(<<"eRECWONTFIT }" );
	return RC(smlevel_0::eRECWONTFIT);
    }
    if( !do_it)  return RCOK;
    
    /*
     *  Consume the space
     */
    w_assert1(nfree() >= total);
    _nfree -= total;

#ifndef BACKOUT_NEHFIX1
    // Q: Why are we not subtracting total below?
    // A: because the slot_bytes space cannot be re-used.
    // See comment above head of method.
    // But that comment really applies to the amount
    // released when we delete a record. The calling
    // method figures out what we free.  If the slot is
    // the last one on the page, presumably it can free
    // the slot_bytes too (but only if we know that
    // all slots after this one are either freed by this tx
    // or by a committed tx -- we don't keep track of that
    // at the moment).
    // In any case, we take the TOTAL amount acquired
    // out of the reserved bytes, and put back only
    // what we can. That maintains the proper invariants
    // that nfree >= nrsvd >= xct_rsvd.
#endif
    if (rflag() && xd && xd->tid() == _tid) {
	w_assert1(nrsvd() >= xct_rsvd());
#ifndef BACKOUT_NEHFIX1
	if (total > xct_rsvd())  // NEW
#else
	if (amt > xct_rsvd())  // OLD
#endif
	{
	    /*
	     *  Use all of xct_rsvd()
	     */
	    _nrsvd -= xct_rsvd();
	    _xct_rsvd = 0;
	} else {
	    /*
	     *  Use portion of xct_rsvd()
	     */
#ifndef BACKOUT_NEHFIX1
	    _nrsvd -= total; // NEW
	    _xct_rsvd -= total; // NEW
#else
	    _nrsvd -= amt;
	    _xct_rsvd -= amt;
#endif
	}
    }

#ifndef BACKOUT_NEHFIX2
    DBG(<<" space_t::acquire amt=" << amt 
		        << " slot_bytes=" << slot_bytes
			<< " _tid=" << tid()
			<< " nfree()=" << nfree()
			<< " nrsvd()=" << nrsvd()
			<< " xct_rsvd()=" << xct_rsvd()
			<< " hiwat()=" << nrsvd_hiwat()
			<< "}"
			);
#else
    DBG(<<" space_t::acquire amt=" << amt 
		        << " slot_bytes=" << slot_bytes
			<< " _tid=" << tid()
			<< " nfree()=" << nfree()
			<< " nrsvd()=" << nrsvd()
			<< " xct_rsvd()=" << xct_rsvd()
			<< "}"
			);
#endif
    
    w_assert3(nfree() >= nrsvd());
    w_assert3(nrsvd() >= xct_rsvd());
    w_assert1(nfree() >= 0 && nrsvd() >= 0 && xct_rsvd() >= 0);
    return RCOK;
}
	

/*********************************************************************
 *
 *  page_s::space_t::release(amt, xd)
 *
 *  Release amt bytes on the page for transaction xd.
 *  Amt does not include bytes consumed by the slot.  Those
 *  bytes are never returned to the free space.  The slots are
 *  marked free, and the file code searches for a free slot to
 *  nab before it tries to allocate a new one.  So slot bytes
 *  are consumed with space_t::acquire but never released.
 *
 *********************************************************************/
void page_s::space_t::release(int amt, xct_t* xd)
{
    DBG(<<"{space_t::release amt=" << amt 
			<< " _tid=" << _tid
			<< " nfree()=" << nfree()
			<< " nrsvd()=" << nrsvd()
			<< " xct_rsvd()=" << xct_rsvd()
		       );
    w_assert3(nfree() >= nrsvd());
    w_assert3(nrsvd() >= xct_rsvd());
    w_assert1(nfree() >= 0 && nrsvd() >= 0 && xct_rsvd() >= 0);

    if (rflag() && xd && xd->state() == smlevel_1::xct_aborting)  {
	/*
	 *  For aborting transaction ...
	 */
	undo_acquire(amt, xd);
        w_assert3(nfree() >= nrsvd());
        w_assert3(nrsvd() >= xct_rsvd());
        w_assert1(nfree() >= 0 && nrsvd() >= 0 && xct_rsvd() >= 0);
	DBG(<<" (undo_acquire) amt=" << amt 
			<< " _tid=" << _tid
			<< " nfree()=" << nfree()
			<< " nrsvd()=" << nrsvd()
			<< " xct_rsvd()=" << xct_rsvd()
				<< "}"
		       );
	return;
    }
    
    if (rflag()) _check_reserve();
    _nfree += amt;
    if (rflag()) {
	if (xd)  {
	    _nrsvd += amt;	// reserve space for this xct
#ifndef BACKOUT_NEHFIX2
	    // update highwater mark
	    if(nrsvd() > 0) {
		 // it's possible to release 0 bytes for a zero-length
		 // record
                 _nrsvd_hiwat = 
			 (nrsvd_hiwat() < nrsvd() ? nrsvd() : nrsvd_hiwat());
	    }
#endif
	    if ( _tid == xd->tid())  {
					// I am still the youngest...
		_xct_rsvd += amt; 	// add to my reserved
	    } else if ( _tid < xd->tid() ) {
					// I am the new youngest.
		_tid = xd->tid();	// assert: _tid >= xct()->tid
					// until I have completed
		_xct_rsvd = amt;
	    }
	}
    }
    DBG(<<"space_t::release amt=" << amt 
			<< " _tid=" << _tid
			<< " nfree()=" << nfree()
			<< " nrsvd()=" << nrsvd()
			<< " xct_rsvd()=" << xct_rsvd()
			<< "}" );
    w_assert3(nfree() >= nrsvd());
    w_assert3(nrsvd() >= xct_rsvd());
    w_assert1(nfree() >= 0 && nrsvd() >= 0 && xct_rsvd() >= 0);
}


/*********************************************************************
 *
 *  page_s::ntoh(vid)
 *
 *  Convert the page to host order. BUGBUG: need to be filled in.
 *
 *********************************************************************/
void page_s::ntoh(vid_t vid)
{
    /*
     *  BUGBUG: TODO: convert the generic parts of the page
     */
    pid._stid.vol = vid;
}



/*********************************************************************
 *
 *  page_p::tag_name(tag)
 *
 *  Return the tag name of enum tag. For debugging purposes.
 *
 *********************************************************************/
const char* const
page_p::tag_name(tag_t t)
{
    switch (t) {
    case t_extlink_p: 
	return "t_extlink_p";
    case t_stnode_p:
	return "t_stnode_p";
    case t_keyed_p:
	return "t_keyed_p";
    case t_btree_p:
	return "t_btree_p";
    case t_rtree_p:
	return "t_rtree_p";
    case t_file_p:
	return "t_file_p";
    default:
	W_FATAL(eINTERNAL);
    }

    W_FATAL(eINTERNAL);
    return 0;
}



/*********************************************************************
 *
 *  page_p::format(pid, tag, page_flags, store_flags, log_it)
 *
 *  Format the page with "pid", "tag", and "page_flags" 
 *	and store_flags (goes into the log record, not into the page)
 *
 *********************************************************************/
rc_t
page_p::format(const lpid_t& pid, tag_t tag, 
	       uint4_t 	    page_flags, 
	       store_flag_t /*store_flags*/,
	       bool 	    log_it
	       ) 
{
    uint4_t             sf;

    w_assert3((page_flags & ~t_virgin) == 0); // expect only virgin 
    /*
     *  Check alignments
     */
    w_assert3(is_aligned(data_sz));
    w_assert3(is_aligned(_pp->data - (char*) _pp));
    w_assert3(sizeof(page_s) == page_sz);
    w_assert3(is_aligned(_pp->data));

    /*
     *  Do the formatting...
     *  ORIGINALLY:
     *  Note: store_flags must be valid before page is formatted
     *  unless we're in redo and DONT_TRUST_PAGE_LSN is turned on.
     *  NOW:
     *  store_flags are passed in. The fix() that preceded this
     *  will have stuffed some store_flags into the page(as before)
     *  but they could be wrong. Now that we are logging the store
     *  flags with the page_format log record, we can force the
     *  page to have the correct flags due to redo of the format.
     *  What this does NOT do is fix the store flags in the st_node.
     * See notes in bf_m::_fix
     *
     *  The following code writes all 1's into the page (except
     *  for store-flags) in the hope of helping debug problems
     *  involving updates to pages and/or re-use of deallocated
     *  pages.
     */
    sf = _pp->store_flags; // save flags
#if defined(SM_FORMAT_WITH_GARBAGE) || defined(PURIFY_ZERO)
    memset(_pp, '\017', sizeof(*_pp)); // trash the whole page
#endif
    _pp->store_flags = sf; // restore flag
    // TODO: any assertions on store_flags?

#if defined(W_DEBUG)
    if(
     (smlevel_0::operating_mode == smlevel_0::t_in_undo)
     ||
     (smlevel_0::operating_mode == smlevel_0::t_forward_processing)
    )  // do the assert below
    w_assert3(sf != st_bad);
#endif /* W_DEBUG  */

    /*
     * Timestamp the page with an lsn.
     * Old way: page format always started it out with this low lsn:
	    _pp->lsn1 = _pp->lsn2 = lsn_t(0, 1);
     * Unfortunately, none of the page updates get logged if the page
     * is tmp, so we can end up with the following bogus situation
     * if a regular page gets deallocated and re-used as a tmp page
     * without any checkpoints happening, but with the page being flushed
     * to disk, then restart. 
     * The restart code says: is this log record(for a page update while
     * the page was regular)'s lsn > the page's lsn? if so, redo the
     * log record.  Ah, but formatting the page as tmp makes *sure* that's 
     * the case, because the lsn is set to be low and stays that way.
     * Unfortunately, sometimes the log record should NOT be applied, and
     * in particular, it should not here.
     * So we'll timestamp the page with the latest lsn from this xct.
     */
    lsn_t l = lsn_t(0, 1);
    xct_t *xd = xct();
    if(xd) l = xd->last_lsn(); 

    _pp->lsn1 = _pp->lsn2 = l;
    _pp->pid = pid;
    _pp->next = _pp->prev = 0;
    _pp->page_flags = page_flags;
    w_assert3(tag != t_bad_p);
    _pp->tag = tag;  // must be set before rsvd_mode() is called
    _pp->space.init(data_sz + 2*sizeof(slot_t), rsvd_mode() != 0);
    _pp->end = _pp->nslots = _pp->nvacant = 0;

    if(log_it) {
	/*
	 *  Log a Page Init
	 */
	W_DO( log_page_init(*this) );
    }

    if(_pp->tag != t_file_p) {
	/* 
	 * Could have been a t_file_p when fix() occured,
	 * but format could have changed it.  Make the
	 * bucket check unnecessary.
	 */
	// check on unfix is notnecessary 
	page_bucket_info.nochecknecessary();
    }

    return RCOK;
}


/*********************************************************************
 *
 *  page_p::_fix(bool,
 *    pid, ptag, mode, page_flags, store_flags, ignore_store_id, refbit)
 *
 *
 *  Fix a frame for "pid" in buffer pool in latch "mode". 
 *
 *  "Ignore_store_id" indicates whether the store ID
 *  on the page can be trusted to match pid.store; usually it can, 
 *  but if not, then passing in true avoids an extra assert check.
 *  "Refbit" is a page replacement hint to bf when the page is 
 *  unfixed.
 *
 *  NB: this does not set the tag() to ptag -- format does that
 *
 *********************************************************************/
rc_t
page_p::_fix(
    bool		condl,
    const lpid_t&	pid,
    tag_t		ptag,
    latch_mode_t	m, 
    uint4_t		page_flags,
    store_flag_t&	store_flags,//used only if page_flags & t_virgin
    bool		ignore_store_id, 
    int			refbit)
{
    w_assert3(!_pp || bf->is_bf_page(_pp, false));
    store_flag_t	ret_store_flags = store_flags;

    
    if (store_flags & st_insert_file)  {
	store_flags = (store_flag_t) (store_flags|st_tmp); 
	// is st_tmp and st_insert_file
    }
    /* allow these only */
    w_assert1((page_flags & ~t_virgin) == 0);

    if (_pp && _pp->pid == pid) {
	if(_mode == m)  {
	    /*
	     *  We have already fixed the page... do nothing.
	     */
	} else {
	    /*
	     *  We have already fixed the page, but we need
	     *  to upgrade the latch mode.
	     */
	    bf->upgrade_latch(_pp, m); // might block
	    _mode = bf->latch_mode(_pp);
	    w_assert3(_mode >= m);
	}
    } else {
	/*
	 * wrong page or no page at all
	 */

	if (_pp)  {
	    /*
	     * If the old page type calls for it, we must
	     * update the space-usage histogram info in its
	     * extent.
	     */
	    W_DO(update_bucket_info());
	    bf->unfix(_pp, false, _refbit);
	    _pp = 0;
	}


	if(condl) {
	    W_DO( bf->conditional_fix(_pp, pid, ptag, m, 
		      (page_flags & t_virgin) != 0, 
		      ret_store_flags,
		      ignore_store_id, store_flags) );
	} else {
	    W_DO( bf->fix(_pp, pid, ptag, m, 
		      (page_flags & t_virgin) != 0, 
		      ret_store_flags,
		      ignore_store_id, store_flags) );
	}

#ifdef W_DEBUG
	if( (page_flags & t_virgin) != 0  )  {
#ifdef DONT_TRUST_PAGE_LSN
	    if(
	     (smlevel_0::operating_mode == smlevel_0::t_in_undo)
	     ||
	     (smlevel_0::operating_mode == smlevel_0::t_forward_processing)
	    )  // do the assert below
#endif /*DONT_TRUST_PAGE_LSN */
	    w_assert3(ret_store_flags != st_bad);
	}
#endif /* W_DEBUG */
	_mode = bf->latch_mode(_pp);
	w_assert3(_mode >= m);
    }

    _refbit = refbit;
    // if ((page_flags & t_virgin) == 0)  {
	// file pages must have have reserved mode set
	// this is a bogus assert, since
	// the definition of rsvd_mode()
	// became tag() == t_file_p
	// w_assert3((tag() != t_file_p) || (rsvd_mode()));
    // }
    init_bucket_info(ptag, page_flags);
    w_assert3(_mode >= m);
    store_flags = ret_store_flags;
    return RCOK;
}

rc_t 
page_p::fix(
    const lpid_t& 	pid, 
    tag_t		ptag,
    latch_mode_t 	mode, 
    uint4_t 		page_flags,
    store_flag_t& 	store_flags, 
    bool 		ignore_store_id, 
    int  		refbit 
)
{
    return _fix(false, pid, ptag, mode, page_flags, store_flags,
		    ignore_store_id, refbit); 
}
rc_t 
page_p::conditional_fix(
    const lpid_t& 	pid, 
    tag_t		ptag,
    latch_mode_t 	mode, 
    uint4_t 		page_flags, 
    store_flag_t& 	store_flags,
    bool 		ignore_store_id, 
    int   		refbit 
)
{
    return _fix(true, pid, ptag, mode, page_flags, store_flags,
	    ignore_store_id, refbit);
}

/*********************************************************************
 *
 *  page_p::link_up(new_prev, new_next)
 *
 *  Sets the previous and next pointer of the page to point to
 *  new_prev and new_next respectively.
 *
 *********************************************************************/
rc_t
page_p::link_up(shpid_t new_prev, shpid_t new_next)
{
    /*
     *  Log the modification
     */
    W_DO( log_page_link(*this, new_prev, new_next) );

    /*
     *  Set the pointers
     */
    _pp->prev = new_prev, _pp->next = new_next;

    return RCOK;
}



/*********************************************************************
 *
 *  page_p::mark_free(idx)
 *
 *  Mark the slot at idx as free.
 *  This sets its length to 0 and offset to 1.
 *
 *********************************************************************/
rc_t
page_p::mark_free(slotid_t idx)
{
    /*
     *  Sanity checks
     */
    w_assert1(idx >= 0 && idx < _pp->nslots);
    //w_assert1(_pp->slot[-idx].length >= 0);
    w_assert1(_pp->slot[-idx].offset >= 0);

    /*
     *  Only valid for pages that need space reservation
     */
    w_assert1( rsvd_mode() );

    /*
     *  Log the action
     */
    W_DO( log_page_mark(*this, idx) );

    /*
     *  Release space and mark free
     *  We do not release the space for the slot table entry. The
     *  slot table is never shrunk on resverved-space pages.
     */
    _pp->space.release(int(align(_pp->slot[-idx].length)), xct());
    _pp->slot[-idx].length = 0;
    _pp->slot[-idx].offset = -1;
    ++_pp->nvacant;

    return RCOK;
}
    



/*********************************************************************
 *
 *  page_p::reclaim(idx, vec, log_it)
 *
 *  Reclaim the slot at idx and fill it with vec. The slot could
 *  be one that was previously marked free (mark_free()), or it
 *  could be a new slot at the end of the slot array.
 *
 *********************************************************************/
rc_t
page_p::reclaim(slotid_t idx, const cvec_t& vec, bool log_it)
{
#ifndef BACKOUT_NEHFIX1
{
    xct_t* xd = xct();
    tid_t _tid;
    if(xd) _tid=xd->tid();
#ifndef BACKOUT_NEHFIX2
    DBG(<<"{reclaim  idx=" << idx << " vec.size=" << vec.size()
			<< " _tid=" << _tid
			<< " nfree()=" << nfree()
			<< " nrsvd()=" << nrsvd()
			<< " xct_rsvd()=" << xct_rsvd()
			<< " hiwat()=" << nrsvd_hiwat()
			);
#else
    DBG(<<"{reclaim  idx=" << idx << " vec.size=" << vec.size()
			<< " _tid=" << _tid
			<< " nfree()=" << nfree()
			<< " nrsvd()=" << nrsvd()
			<< " xct_rsvd()=" << xct_rsvd()
			);
#endif
}
#endif
    /*
     *  Only valid for pages that need space reservation
     */
    w_assert1( rsvd_mode() );

    /*
     *  Sanity check
     */
    w_assert1(idx >= 0 && idx <= _pp->nslots);
    
    /*
     *  Compute # bytes needed. If idx is a new slot, we would
     *  need space for the slot as well.
     */
    smsize_t need = align(vec.size());
    smsize_t need_slots = (idx == _pp->nslots) ? sizeof(slot_t) : 0;

    /*
     *  Acquire the space ... return error if failed.
     */
    W_DO(_pp->space.acquire(need, need_slots, xct()));
    // Always true:
    // if( rsvd_mode() ) {
	W_DO(update_bucket_info());
    // }

    if(log_it) {
	/*
	 *  Log the reclaim. 
	 */
	w_rc_t rc = log_page_reclaim(*this, idx, vec);
	if (rc)  {
	    /*
	     *  Cannot log ... manually release the space acquired
	     */
	    // TODO: SHOULD THIS NOT BE need + need_slots?
            // maybe could test this by running out of log space
	    // allocate all pages to file, commit in many small tx
	    // remove all but one rec on each page, commit
	    // create many new recs to cause page reclaims

	    _pp->space.undo_acquire(need, xct());

	    // Always true:
	    // if( rsvd_mode() ) {
		W_COERCE(update_bucket_info());
	    // }
	    DBG(<<"log page reclaim failed at line " <<  __LINE__ );
	    return RC_AUGMENT(rc);
	}
    }

    /*
     *  Log has already been generated ... the following actions must
     *  succeed!
     */
    // Q : why is need_slots figured in contig_space() ?
    // A : because need_slots is 0 if we aren't allocating
    // a new slot.
    

    if (contig_space() < need + need_slots) {
	/*
	 *  Shift things around to get enough contiguous space
	 */
	_compress((idx == _pp->nslots ? -1 : idx));
    }
    w_assert1(contig_space() >= need + need_slots);
    
    slot_t& s = _pp->slot[-idx];
    if (idx == _pp->nslots)  {
	/*
	 *  Add a new slot
	 */
	_pp->nslots++;
    } else {
	/*
	 *  Reclaim a vacant slot
	 */
	w_assert1(s.length == 0);
	w_assert1(s.offset == -1);
	w_assert1(_pp->nvacant > 0);
	--_pp->nvacant;
    }
    
    /*
     *  Copy data to page
     */
    // make sure the slot table isn't getting overrun
    char* target = _pp->data + (s.offset = _pp->end);
    w_assert3((caddr_t)(target + vec.size()) <= 
	      (caddr_t)&_pp->slot[-(_pp->nslots-1)]);
    vec.copy_to(target);
    _pp->end += int(align( (s.length = vec.size()) ));

    W_IFDEBUG(W_COERCE(check()));
#ifndef BACKOUT_NEHFIX1
{
    xct_t* xd = xct();
    tid_t _tid;
    if(xd) _tid=xd->tid();
#ifndef BACKOUT_NEHFIX2
    DBG(<<" reclaim  idx=" << idx << " vec.size=" << vec.size()
			<< " _tid=" << _tid
			<< " nfree()=" << nfree()
			<< " nrsvd()=" << nrsvd()
			<< " xct_rsvd()=" << xct_rsvd()
			<< " hiwat()=" << nrsvd_hiwat()
			<< "}" );
#else
    DBG(<<" reclaim  idx=" << idx << " vec.size=" << vec.size()
			<< " _tid=" << _tid
			<< " nfree()=" << nfree()
			<< " nrsvd()=" << nrsvd()
			<< " xct_rsvd()=" << xct_rsvd()
			<< "}" );
#endif

}
#endif
    
    return RCOK;
}

 

   
/*********************************************************************
 *
 *  page_p::find_slot(space_needed, ret_idx, start_search)
 *
 *  Find a slot in the page that could accomodate space_needed bytes.
 *  Return the slot in ret_idx.  Start searching for a free slot
 *  at location start_search (default == 0).
 *
 *********************************************************************/
rc_t
page_p::find_slot(uint4_t space_needed, slotid_t& ret_idx, slotid_t start_search)
{
    /*
     *  Only valid for pages that need space reservation
     */
    w_assert3( rsvd_mode() );

    /*
     *  Check for sufficient space
     */
    if (usable_space() < space_needed)   {
	return RC(eRECWONTFIT);
    }
    /*
     * usable_space() has side effect of possibly
     * freeing space and changing the bucket
     * so we have to update the bucket info
     */
    W_DO(update_bucket_info());

    /*
     *  Find a vacant slot (could be a new slot)
     */
    slotid_t idx = _pp->nslots;
    if (_pp->nvacant) {
	for (slotid_t i = start_search; i < _pp->nslots; ++i) {
	    if (_pp->slot[-i].offset == -1)  {
		w_assert3(_pp->slot[-i].length == 0);
		idx = i;
		break;
	    }
	}
    }
    
    w_assert3(idx >= 0 && idx <= _pp->nslots);
    ret_idx = idx;

    return RCOK;
}

rc_t
page_p::next_slot(
	uint4_t space_needed, 
	slotid_t& ret_idx
)
{
    /*
     *  Only valid for pages that need space reservation
     */
    w_assert3( rsvd_mode() );

    /*
     *  Check for sufficient space
     */
    if (usable_space() < space_needed)   {
	return RC(eRECWONTFIT);
    }

    /*
     *  Find a vacant slot at the end of the
     *  slot table or get a new slot
     */
    slotid_t idx = _pp->nslots;
    if (_pp->nvacant) {
	// search backwards, stop at first non-vacant
	// slot, and return lowest vacant slot above that
	for (int i = _pp->nslots-1; i>=0;  i--) {
	    if (_pp->slot[-i].offset == -1)  {
		w_assert3(_pp->slot[-i].length == 0);
	    } else {
		idx = i+1;
		break;
	    }
	}
    }
    
    w_assert3(idx >= 0 && idx <= _pp->nslots);
    ret_idx = idx;

    return RCOK;
}



/*********************************************************************
 *
 *  page_p::insert_expand(idx, cnt, vec[], bool log_it, bool do_it)
 *  If we think of a page as <hdr><slots/data> ... <slot-table>,
 *  "left" means higher-numbered slots, and "right" means lower-
 *  numbered slots.
 *
 *  Insert cnt slots starting at index idx. Slots on the left of idx
 *  are pushed further to the left to make space for cnt slots. 
 *  By this it's meant that the slot table entries are moved; the
 *  data themselves are NOT moved.
 *  Vec[] contains the data for these new slots. 
 *
 *  If !do_it, just figure out if there's adequate space
 *  If !log_it, don't log it
 *
 *********************************************************************/
rc_t
page_p::insert_expand(slotid_t idx, int cnt, const cvec_t *vec, 
	bool log_it, bool do_it)
{
    w_assert1(! rsvd_mode() );	// needless to say, file pages can't do this
    w_assert1(idx >= 0 && idx <= _pp->nslots);
    w_assert1(cnt > 0);

    /*
     *  Compute the total # bytes needed 
     */
    uint total = 0;
    int i;
    for (i = 0; i < cnt; i++)  {
	total += int(align(vec[i].size()) + sizeof(slot_t));
    }

    /*
     *  Try to get the space ... could fail with eRECWONTFIT
     */
    W_DO( _pp->space.acquire(total, 0, xct(), do_it) );
    if(! do_it) return RCOK;

    w_assert3(! rsvd_mode() );	// Keep this here for clarity
    /*
     * NB: Don't have to update the bucket info because
     * this isn't called for rsvd_mode() pages.  If we decide
     * to use this for rsvd_mode() pages, have to update 
     * the bucket info here and below when logging fails.
     */

    if(log_it) {
	/*
	 *  Log the insertion
	 */
	rc_t rc = log_page_insert(*this, idx, cnt, vec);
	if (rc)  {
	    /*
	     *  Log failed ... manually release the space acquired
	     */
	    _pp->space.undo_acquire(total, xct());
	    return RC_AUGMENT(rc);
	}
    }

    /*
     *  Log has already been generated ... the following actions must
     *  succeed!
     */

    if (contig_space() < total)  {
	/*
	 *  Shift things around to get enough contiguous space
	 */
	_compress();
	w_assert3(contig_space() >= total);
    }

    if (idx != _pp->nslots)    {
	/*
	 *  Shift left neighbor slots further to the left
	 */
	memmove(&_pp->slot[-(_pp->nslots + cnt - 1)],
	        &_pp->slot[-(_pp->nslots - 1)], 
	        (_pp->nslots - idx) * sizeof(slot_t));
    }

    /*
     *  Fill up the slots and data
     */
    register slot_t* p = &_pp->slot[-idx];
    for (i = 0; i < cnt; i++, p--)  {
	p->offset = _pp->end;
	p->length = vec[i].copy_to(_pp->data + p->offset);
	_pp->end += int(align(p->length));
    }

    _pp->nslots += cnt;
    
    W_IFDEBUG( W_COERCE(check()) );

    return RCOK;
}




/*********************************************************************
 *
 *  page_p::remove_compress(idx, cnt)
 *
 *  Remove cnt slots starting at index idx. Up-shift slots after
 *  the hole to fill it up.
 *  If we think of a page as <hdr><slots/data> ... <slot-table>,
 *  "up" means low-numbered slots, and "after" means higher-
 *  numbered slots.
 *
 *********************************************************************/
rc_t
page_p::remove_compress(slotid_t idx, int cnt)
{
    w_assert1(! rsvd_mode() );
    w_assert1(idx >= 0 && idx < _pp->nslots);
    w_assert1(cnt > 0 && cnt + idx <= _pp->nslots);

#ifdef W_DEBUG
    int old_num_slots = _pp->nslots;
#endif /* W_DEBUG */

    /*
     *  Log the removal
     */
    W_DO( log_page_remove(*this, idx, cnt) );

    /*
     *	Compute space space occupied by tuples
     */
    register slot_t* p = &_pp->slot[-idx];
    register slot_t* q = &_pp->slot[-(idx + cnt)];
    int amt_freed = 0;
    for ( ; p != q; p--)  {
	w_assert3(p->length < page_s::data_sz+1);
	amt_freed += int(align(p->length) + sizeof(slot_t));
    }

    /*
     *	Compress slot array
     */
    p = &_pp->slot[-idx];
    q = &_pp->slot[-(idx + cnt)];
    for (slot_t* e = &_pp->slot[-_pp->nslots]; q != e; p--, q--) *p = *q;
    _pp->nslots -= cnt;

    /*
     *  Free space occupied
     */
    _pp->space.release(amt_freed, xct());

    
#ifdef W_DEBUG
    W_COERCE(check());
    // If we've compressed more than what we expected,
    // we'll catch that fact here.
    w_assert3(old_num_slots - cnt == _pp->nslots);
#endif
    return RCOK;
}


/*********************************************************************
 *
 *  page_p::set_byte(slotid_t idx, op, bits)
 *
 *  Logical operation on a byte's worth of bits at offset idx.
 *
 *********************************************************************/
rc_t
page_p::set_byte(slotid_t idx, u_char bits, logical_operation op)
{
    /*
     *  Compute the byte address
     */
    u_char* p = (u_char*) tuple_addr(0) + idx;
    // doesn't compile under vc++
    // DBG(<<"set_byte old_value=" << (unsigned(*p)) );

    /*
     *  Log the modification
     */
    W_DO( log_page_set_byte(*this, idx, *p, bits, op) );

    switch(op) {
    case l_none:
	break;

    case l_set:
	*p = bits;
	break;

    case l_and:
	*p = (*p & bits);
	break;

    case l_or:
	*p = (*p | bits);
	break;

    case l_xor:
	*p = (*p ^ bits);
	break;

    case l_not:
	*p = (*p & ~bits);
	break;
    }

    return RCOK;
}


/*********************************************************************
 *
 *  page_p::set_bit(slotid_t idx, bit)
 *
 *  Set a bit of slot idx.
 *
 *********************************************************************/
rc_t
page_p::set_bit(slotid_t idx, int bit)
{
    /*
     *  Compute the byte address
     */
    u_char* p = (u_char*) tuple_addr(idx);
    w_assert3( (smsize_t)((bit - 1)/8 + 1) <= tuple_size(idx) );
    
    /*
     *  Log the modification
     */
    W_DO( log_page_set_bit(*this, idx, bit) );

    /*
     *  Set the bit
     */
    bm_set(p, bit);

    return RCOK;
}




/*********************************************************************
 *
 *  page_p::clr_bit(idx, bit)
 *
 *  Clear a bit of slot idx.
 *
 *********************************************************************/
rc_t
page_p::clr_bit(slotid_t idx, int bit)
{ 
    /*
     *  Compute the byte address
     */
    u_char* p = (u_char*) tuple_addr(idx);
    w_assert3( (smsize_t)((bit - 1)/8 + 1) <= tuple_size(idx));

    /*
     *  Log the modification
     */
    W_DO( log_page_clr_bit(*this, idx, bit) );

    /*
     *  Set the bit
     */
    bm_clr(p, bit);

    return RCOK;
}



/*********************************************************************
 *
 *  page_p::splice(idx, cnt, info[])
 *
 *  Splice the tuple at idx. "Cnt" regions of the tuple needs to
 *  be modified.
 *
 *********************************************************************/
rc_t
page_p::splice(slotid_t idx, int cnt, splice_info_t info[])
{
    DBGTHRD(<<"page_p::splice idx=" <<  idx << " cnt=" << cnt);
    for (int i = cnt; i >= 0; i--)  {
	// for now, but We should use safe-point to bail out.
	W_COERCE(splice(idx, info[i].start, info[i].len, info[i].data));
    }
    return RCOK;
}




/*********************************************************************
 *
 *  page_p::splice(idx, start, len, vec)
 *
 *  Splice the tuple at idx. The range of bytes from start to
 *  start+len is replaced with vec. If size of "vec" is less than
 *  "len", the rest of the tuple is moved in to fill the void.
 *  If size of "vec" is more than "len", the rest of the tuple is
 *  shoved out to make space for "vec". If size of "vec" is equal
 *  to "len", then those bytes are simply replaced with "vec".
 *
 *********************************************************************/
rc_t
page_p::splice(slotid_t idx, slot_length_t start, slot_length_t len, const cvec_t& vec)
{
    FUNC(page_p::splice);
    DBGTHRD(<<"page_p::splice idx=" <<  idx 
		    << " start=" << start
		    << " len=" << len
	   );
    int vecsz = vec.size();
    w_assert1(idx >= 0 && idx < _pp->nslots);
    w_assert1(vecsz >= 0);

    slot_t& s = _pp->slot[-idx];		// slot in question
    w_assert1(start + len <= s.length);

    /*
     * need 	  : actual amount needed
     * adjustment : physical space needed taking alignment into acct 
     */
    int need = vecsz - len;
    int adjustment = int(align(s.length + need) - align(s.length));

    if (adjustment > 0) {
	/*
	 *  Need more ... acquire the space
	 */
	DBG(<<"space.acquire adjustment=" <<  adjustment );
	W_DO(_pp->space.acquire(adjustment, 0, xct()));
	if(rsvd_mode()) {
	    W_DO(update_bucket_info());
	}
    }

    /*
     *  Figure out if it's worth logging
     *  the splice as a splice of zeroed 
     *  old or new data
     *
     *  osave is # bytes of old data to be saved
     *  nsave is # bytes of new data to be saved
     *
     *  The new data must be in a zvec if we are to 
     *  skip anything.  The old data are inspected.
     */
    int	 osave=len, nsave=vec.size();
    bool zeroes_found=false;

    DBG(
	<<"start=" << start
	<<" len=" << len
	<<" vec.size = " << nsave
    ); 
    if(vec.is_zvec()) {
	DBG(<<"splice in " << vec.size() << " zeroes");
	nsave = 0; zeroes_found = true;
    }

    /*
    // Find out if the start through start+len are all zeroes.
    // Not worth this effort it if the old data aren't larger than
    // the additional logging info needed to save the space.
    */
#define FUDGE 0
    // check old
    if ((size_t)len > FUDGE + (2 * sizeof(int2_t))) {
	char	*c;
	int	l;
	for (l = len, c = (char *)tuple_addr(idx)+start;
	    l > 0  && *c++ == '\0'; l--)
		;

	DBG(<<"old data are 0 up to byte " << len - l
		<< " l = " << l
		<< " len = " << len
		);
	if(l==0) {
	    osave = 0;
	    zeroes_found = true;
	}
    }
    w_assert3(len <= smlevel_0::page_sz);

    /*
     *  Log the splice that we're about to do.
     */
    rc_t rc;

    if(zeroes_found) {
	DBG(<<"Z splice avoid saving old=" 
		<< (len - osave) 
		<< " new= " 
		<< (vec.size()-nsave));
	rc = log_page_splicez(*this, idx, start, len, osave, nsave, vec);
    } else {
	DBG(<<"log splice idx=" <<  idx << " start=" << start
	      << " len=" << len );
	rc = log_page_splice(*this, idx, start, len, vec);
    }
    if (rc)  {
	DBG(<<"LOG FAILURE rc=" << rc );
	/*
	 *  Log fail ... release any space we acquired
	 */
	if (adjustment > 0)  {
	    _pp->space.undo_acquire(adjustment, xct());
	    if(rsvd_mode()) {
		W_DO(update_bucket_info());
	    }
	}
	return RC_AUGMENT(rc);
    }
    DBGTHRD(<<"adjustment =" << adjustment);

    if (adjustment == 0) {
	/* do nothing */

    } else if (adjustment < 0)  {
	/*
	 *  We need less space: the tuple  got smaller.
	 */
	DBG(<<"release:  adjustment=" << adjustment );
	_pp->space.release(-adjustment, xct());
	if(rsvd_mode()) {
	    W_DO(update_bucket_info());
	}
	
    } else if (adjustment > 0)  {
	/*
	 *  We need more space. Move tuple of slot idx to the
	 *  end of the page so we can expand it.
	 */
	w_assert3(need > 0);
	if (contig_space() < (uint)adjustment)  {
	    /*
	     *  Compress and bring tuple of slot[-idx] to the end.
	     */
	    _compress(idx);
	    w_assert1(contig_space() >= (uint)adjustment);
	    
	} else {
	    /*
	     *  There is enough contiguous space for delta
	     */
	    if (s.offset + align(s.length) == _pp->end)  {
		/*
		 *  last slot --- we can simply extend it 
		 */
	    } else if (contig_space() > align(s.length + need)) {
		/*
		 *  copy this record to the end and expand from there
		 */
		memcpy(_pp->data + _pp->end,
		       _pp->data + s.offset, s.length);
		s.offset = _pp->end;
		_pp->end += int(align(s.length));
	    } else {
		/*
		 *  No other choices. 
		 *  Compress and bring tuple of slot[-idx] to the end.
		 */
		_compress(idx);
	    }
	}

	_pp->end += adjustment; // expand
    } 

    /*
     *  Put data into the slot
     */
    char* p = _pp->data + s.offset;
    if (need && (s.length != start + len))  {
	/*
	 *  slide tail forward or backward
	 *  NEH: we have to be careful here that we
	 *  don't clobber what we are moving.  If the
	 *  distance to be moved is greater than  
	 *  the amount to be moved, we are safe, 
	 *  but if there's some overlap, we
	 *  have to move the overlapped part FIRST
	 */
	int distance_moved = need;
	int amount_moved = s.length - start - len;

	w_assert1(amount_moved > 0);

	if(distance_moved > 0 && (amount_moved > distance_moved)) {
	    /* 
	     * do it in partial moves: first the part in the overlap,
	     * in small enough chunks that there's no overlap: move
	     * it all in distance_moved-sized chunks, starting at the tail
	     */
	    int  chunksize = distance_moved;
	    int  amt2move = amount_moved; 
	    char *from = ((p + start + len) + amt2move) - chunksize; 
	    char *to= from + distance_moved;

	    while (amt2move > 0) {
		w_assert1(amt2move > 0);
		w_assert1(chunksize > 0);
		DBG(<<"copying " << chunksize 
			<< " amt left=" << amt2move
			<< " from =" << W_ADDR(from)
			<< " to =" << W_ADDR(to)
			);
		memcpy(to, from, chunksize);
		amt2move -= chunksize;
		chunksize = amt2move > chunksize ? chunksize : amt2move;
		to -= chunksize;
		from -= chunksize;
	    }
	    w_assert3(amt2move == 0);
	} else { 
	    /*
	     * if distance_moved < 0, we're moving left and won't
	     * have any overlap
	     */
	    memcpy(p + start + len + distance_moved, 
	       p + start + len, 
	       amount_moved);
	}
    }
    if (vecsz > 0)  {
	w_assert3((int)(s.offset + start + vec.size() <= data_sz));
	// make sure the slot table isn't getting overrun
	w_assert3((caddr_t)(p + start + vec.size()) <= (caddr_t)&_pp->slot[-(_pp->nslots-1)]);
		
	vec.copy_to(p + start);
    }
    _pp->slot[-idx].length += need;


#ifdef W_DEBUG
    W_COERCE(check());
#endif /* W_DEBUG */

    return RCOK;
}


/*********************************************************************
 *
 *  page_p::merge_slots(idx, off1, off2)
 *
 *  Merge tuples idx, idx+1, removing whatever's at the end of
 *  tuple idx, and the beginning of idx+2.   We cut out from
 *  off1 to the end of idx (inclusive), 
 *  and from the beginning to off2 (not inclusive) of idx+1.
 *
 *********************************************************************/
rc_t
page_p::merge_slots(slotid_t idx, slot_offset_t off1, slot_offset_t off2)
{
    W_IFDEBUG( W_COERCE(check()) );
    int idx2 = idx+1;

    w_assert1(idx >= 0 && idx < _pp->nslots);
    w_assert1(idx2 >= 0 && idx2 < _pp->nslots);

    /*
     *  Log the merge as two splices (to save the old data)
     *  and a shift
     */
    rc_t rc;
    {
	slot_t& s = _pp->slot[-(idx)];

	/* 
	 * cut out out the end of idx
	 */
	DBG(<<"cut " << idx << ","
		<< off1 << ","
		<< s.length-off1);
	W_DO(cut(idx, off1, s.length-off1));

	/* 
	 * cut out out the beginning of idx2
	 */
	DBG(<<"cut " << idx2 << ","
		<< 0 << ","
		<< off2 );

	W_DO(cut(idx2, 0, off2));
    }
    slot_t& s = _pp->slot[-(idx)];
    slot_t& t = _pp->slot[-(idx2)];

    DBG(<<"shift " << idx2 << "," << 0 << ","
	    << t.length << "," << idx << "," << s.length);

    W_IFDEBUG( W_COERCE(check()) );
    /* 
     * Shift does a _shift_compress 
     */
    rc =  page_p::shift(idx2, 0, t.length, idx, s.length);
    W_IFDEBUG( W_COERCE(check()) );

    W_DO(remove_compress(idx2, 1));

    W_IFDEBUG( W_COERCE(check()) );
    return rc;
}

/*********************************************************************
 *
 *  page_p::split_slot(idx, off,  vec_t v1, vec_t v2)
 *
 *  Split slot idx at offset off (origin 0)
 *     (split is between off-1 and off) into two slots: idx, idx+1 
 *  AND insert v1 at the end of slot idx, 
 *  AND insert v2 at the beginning of slot idx+1.
 *
 *********************************************************************/
rc_t
page_p::split_slot(slotid_t idx, slot_offset_t off, const cvec_t& v1,
	const cvec_t& v2)
{
    FUNC(page_p::split_slot);
    W_IFDEBUG( W_COERCE(check()) );
    
    /*
     *  Pre-compute # bytes needed.
     */
    smsize_t need = align(v1.size()) + align(v2.size());
    smsize_t need_slots = sizeof(slot_t);

    // Acquire it, then immediately release it, because
    // the funcs that we call below will acquire it.
    // We do this just so that we can avoid hand-undoing the
    // parts of this function if we should get a RECWONTFIT
    // part-way through this mess.
    DBG(<<"v1.size " << v1.size() << " v2.size " << v2.size());
    DBG(<<"aligned: " << align(v1.size()) << " " << align(v2.size()));
    DBG(<<"slot size: " << sizeof(slot_t));
    DBG(<<"needed " << need << " need_slots " << need_slots);
    DBG(<<" usable is now " << _pp->space.usable(xct()) );
    DBG(<<" nfree() " << _pp->space.nfree()
		<<" nrsvd() " << _pp->space.nrsvd()
		<<" xct_rsvd() " << _pp->space.xct_rsvd()
		<<" _tid " << _pp->space.tid()
		<<" xct() " << xct()->tid()
    );
    if((int)(need + need_slots) > _pp->space.usable(xct())) {
		return RC(smlevel_0::eRECWONTFIT);
    }
    
    /*
     * add a slot at idx+1, and put v2 in it.
     */
    int idx2 = idx+1;
    W_DO(insert_expand(idx2, 1, &v2, true));
    DBG(<<" usable is now " << _pp->space.usable(xct()) );
    DBG(<<" nfree() " << _pp->space.nfree()
	<<" nrsvd() " << _pp->space.nrsvd()
	<<" xct_rsvd() " << _pp->space.xct_rsvd()
	<<" _tid " << _pp->space.tid()
	<<" xct() " << xct()->tid()
    );

    W_IFDEBUG( W_COERCE(check()) );

    w_assert1(idx >= 0 && idx < _pp->nslots);
    w_assert1(idx2 >= 0 && idx2 < _pp->nslots);

    slot_t& s = _pp->slot[-(idx)];
    slot_t& t = _pp->slot[-(idx2)];

    DBG(<<"shift " << idx2 << "," << 0 << ","
	    << t.length << "," << idx << "," << s.length);

    /* 
     * Shift does a _shift_compress 
     */
#ifdef W_DEBUG
    int savelength1 = s.length;
    int savelength2 = t.length;
#endif
    w_assert3(savelength2 == (int)v2.size());

    W_COERCE(page_p::shift(idx, off, s.length-off, idx2, t.length));
    DBG(<<" usable is now " << _pp->space.usable(xct()) );
    DBG(<<" nfree() " << _pp->space.nfree()
	<<" nrsvd() " << _pp->space.nrsvd()
	<<" xct_rsvd() " << _pp->space.xct_rsvd()
	<<" _tid " << _pp->space.tid()
	<<" xct() " << xct()->tid()
    );

    w_assert3(s.length == off);
    w_assert3(t.length == (savelength1-off) + savelength2);
    W_IFDEBUG( W_COERCE(check()) );

    /*
     *  v2 was already put into idx2, so insert v1 at the
     *  end of idx
     */
    DBG(<<"paste " << idx << "," << off << "," << v1.size());
    W_COERCE(paste(idx, off, v1));

    W_IFDEBUG( W_COERCE(check()) );
    return RCOK;
}

/*********************************************************************
 *
 *  page_p::shift(idx2, off2,  len2, idx1, off1)
 *
 *  Shift len2 bytes from slot idx2 to slot idx1, starting
 *    with the byte at off2 (origin 0).
 *  Shift them to slot idx1 starting at off1 (the first byte moved
 *  becomes the off1-th byte (origin 0) of idx1. 
 *
 *********************************************************************/
rc_t
page_p::shift(
    slotid_t idx2, slot_offset_t off2, slot_length_t len2, //from-slot
    slotid_t idx1,  slot_offset_t off1	// to-slot
)
{
    W_IFDEBUG( W_COERCE(check()) );

    /* 
     * shift the data from idx2 into idx1
     */
    w_rc_t rc;
    rc = log_page_shift(*this, idx2, off2, len2, idx1, off1);
    if (rc)  {
	return RC_AUGMENT(rc);
    }

    /*
     *  We need less space -- compute adjustment for alignment
     */
    {
	slot_t& t = _pp->slot[-idx1]; // to
	slot_t& s = _pp->slot[-idx2]; // from

	int adjustment = 
	    (	// amount needed after
		align(s.length - len2) // final length of idx1(from)
		+
		align(t.length + len2) // final length of idx2(to)
	    ) - ( // amount needed before
		align(s.length) 
		+
		align(t.length) 
	    );

	DBG(<<"adjustment=" << adjustment);
	if (adjustment > 0)  {
	    W_DO(_pp->space.acquire(adjustment, 0,  xct()));
	    if( rsvd_mode() ) {
		W_DO(update_bucket_info());
	    }
	} else {
	    _pp->space.release( -adjustment, xct());
	    if( rsvd_mode() ) {
		W_DO(update_bucket_info());
	    }
	}
    }
	
    /*
     *  Compress and bring tuple of slots idx1, idx2 to the end.
     */
    _shift_compress(idx2, off2, len2, idx1, off1);

    W_IFDEBUG( W_COERCE(check()) );


    return RCOK;
}


/*********************************************************************
 *
 *  page_p::_shift_compress(from, off-from, n, to, off-to)
 *
 *  Shift n bytes from slot from, starting at offset off-from
 *  to slot to, inserting them at(before) offset off-to.
 *
 *  Compresses the page in the process, putting from, to 
 *  at the end, in that order.
 *  
 *********************************************************************/
void
page_p::_shift_compress(slotid_t from, 
	slot_offset_t  from_off, 
	slot_length_t from_len,
	slotid_t to, 
	slot_offset_t  to_off)
{
    DBG(<<"end before " << _pp->end);
    /*
     *  Scratch area and mutex to protect it.
     */
    static smutex_t mutex("pgpshftcmprs");
    static char shift_scratch[sizeof(_pp->data)];

    /*
     *  Grab the mutex
     */
    W_COERCE( mutex.acquire() );
    
    w_assert3(from >= 0 && from < _pp->nslots);
    w_assert3(to >= 0 && to < _pp->nslots);
    
    /*
     *  Copy data area over to scratch
     */
    memcpy(&shift_scratch, _pp->data, sizeof(_pp->data));

    /*
     *  Move data back without leaving holes
     */
    register char* p = _pp->data;
    slotid_t  nslots = _pp->nslots;
    for (slotid_t  i = 0; i < nslots; i++) {
	if (i == from)  continue; 	// ignore this slot for now
	if (i == to)  continue; 	// ignore this slot, too
	slot_t& s = _pp->slot[-i];
	if (s.offset != -1)  { 		// it's in use
	    w_assert3(s.offset >= 0);
	    memcpy(p, shift_scratch+s.offset, s.length);
	    s.offset = p - _pp->data;
	    p += align(s.length);
	}
    }

    /*
     *  Move specified slots: from, to
     */
    {
	slot_offset_t	firstpartoff;
	slot_length_t	firstpartlen;
	slot_offset_t	middleoff;
	slot_length_t	middlelen;
	slot_offset_t	secondpartoff;
	slot_length_t	secondpartlen;
	char*   	base_p;
	slot_offset_t	s_old_offset;

	/************** from **********************/
	slot_t& s = _pp->slot[-from];
	DBG(<<" copy from slot " << from
		<< " with tuple size " << tuple_size(from)
		<< " offset " << s.offset
		);
	w_assert3(from_off <= s.length);
	w_assert3(s.offset != -1); // it's in use
	w_assert3(s.length <= from_off + from_len); 

	// copy firstpart: 0 -> from_off
	// skip from_off -> from_off + from_len
	// copy secondpart: from_off + from_len -> s.length
	firstpartoff = 0;
	firstpartlen = from_off;
	middleoff = firstpartoff + firstpartlen;
	middlelen = from_len;
	secondpartoff = middlelen + firstpartlen;
	secondpartlen = s.length - secondpartoff;
	base_p = p;


	if(firstpartlen) {
	    DBG(<<"memcpy("
		<< W_ADDR(p) << ","
		<< W_ADDR(shift_scratch + s.offset + firstpartoff) << ","
		<< firstpartlen );
	    memcpy(p, shift_scratch + s.offset + firstpartoff, firstpartlen);
	    p += firstpartlen;
	}
	// skip middle
	if(secondpartlen) {
	    DBG(<<"memcpy("
		<< W_ADDR(p) << ","
		<< W_ADDR(shift_scratch + s.offset + secondpartoff) << ","
		<< secondpartlen );
	    memcpy(p, shift_scratch + s.offset + secondpartoff , secondpartlen);
	    p += secondpartlen;
	}

	s.length -= middlelen;		// XXXX
	s_old_offset = s.offset;
	s.offset = base_p - _pp->data;
	p = base_p + align(s.length);

	/************** to **********************/
	slot_t& t = _pp->slot[-to];
	DBG(<<" copy into slot " << to
		<< " with tuple size " << tuple_size(to)
		<< " offset " << t.offset
		);
	w_assert3(t.offset != -1); // it's in use
	w_assert3(to_off <= t.length);

	// copy firstpart: 0 -> to_off-1
	// copy middle from s
	// copy secondpart: to_off -> t.length

	firstpartoff = 0;
	firstpartlen = to_off;
	secondpartoff = to_off; 
	secondpartlen = t.length - secondpartoff;
	base_p = p;

	if(firstpartlen) {
	    DBG(<<"before memcpy: t.offset " << t.offset
		<< " firstpartoff " << firstpartoff
		<< " firstpartlen " << firstpartlen);
	    DBG(<<"memcpy("
		<< W_ADDR(p) << ","
		<< W_ADDR(shift_scratch + t.offset + firstpartoff) << ","
		<< firstpartlen );
	    memcpy(p, shift_scratch + t.offset + firstpartoff, firstpartlen);
	    p += firstpartlen;
	}
	w_assert1(middlelen);
	DBG(<<"before memcpy: s.offset " << s_old_offset
		<< " middleoff " << middleoff
		<< " middlelen " << middlelen
		);
	DBG(<<"memcpy("
	    << W_ADDR(p) << ","
	    << W_ADDR(shift_scratch + s_old_offset + middleoff) << ","
	    << middlelen );
	memcpy(p, shift_scratch + s_old_offset + middleoff, middlelen);
	p += middlelen;

	if(secondpartlen) {
	    DBG(<<"memcpy("
		<< W_ADDR(p) << ","
		<< W_ADDR(shift_scratch + t.offset + secondpartoff) << ","
		<< secondpartlen );
	    memcpy(p, shift_scratch + t.offset + secondpartoff, secondpartlen);
	    p += secondpartlen;
	}

	t.offset = base_p - _pp->data;
	t.length += middlelen;		// XXXX
	p = base_p + align(t.length);
    }
    _pp->end = p - _pp->data;
    DBG(<<"end after " << _pp->end);

    /*
     *  Page is now compressed with a hole after _pp->end.
     *  to is now the last slot, and it's got data in slot "from"
     */

    /*
     *  Done 
     */
    mutex.release();
}

/*********************************************************************
 *
 *  page_p::_compress(idx)
 *
 *  Compress the page (move all holes to the end of the page). 
 *  If idx != -1, then make sure that the tuple of idx slot 
 *  occupies the bytes of occupied space. Tuple of idx slot
 *  would be allowed to expand into the hole at the end of the
 *  page later.
 *  
 *********************************************************************/
void
page_p::_compress(slotid_t idx)
{
    /*
     *  Scratch area and mutex to protect it.
     */
    static smutex_t mutex("pgpcmprs");
    static char scratch[sizeof(_pp->data)];

    /*
     *  Grab the mutex
     */
    W_COERCE( mutex.acquire() );
    
    w_assert3(idx < 0 || idx < _pp->nslots);
    
    /*
     *  Copy data area over to scratch
     */
    memcpy(&scratch, _pp->data, sizeof(_pp->data));

    /*
     *  Move data back without leaving holes
     */
    register char* p = _pp->data;
    slotid_t nslots = _pp->nslots;
    for (slotid_t i = 0; i < nslots; i++) {
	if (i == idx)  continue; 	// ignore this slot for now
	slot_t& s = _pp->slot[-i];
	if (s.offset != -1)  {
	    w_assert3(s.offset >= 0);
	    memcpy(p, scratch+s.offset, s.length);
	    s.offset = p - _pp->data;
	    p += align(s.length);
	}
    }

    /*
     *  Move specified slot
     */
    if (idx >= 0)  {
	slot_t& s = _pp->slot[-idx];
	if (s.offset != -1) {
	    w_assert3(s.offset >= 0);
	    memcpy(p, scratch + s.offset, s.length);
	    s.offset = p - _pp->data;
	    p += align(s.length);
	}
    }

    _pp->end = p - _pp->data;

    /*
     *  Page is now compressed with a hole after _pp->end.
     */

//    w_assert1(_pp->space.nfree() == contig_space());
//    W_IFDEBUG( W_COERCE(check()) );

    /*
     *  Done 
     */
    mutex.release();
}




/*********************************************************************
 *
 *  page_p::pinned_by_me()
 *
 *  Return true if the page is pinned by this thread (me())
 *
 *********************************************************************/
bool
page_p::pinned_by_me() const
{
    return bf->fixed_by_me(_pp);
}

/*********************************************************************
 *
 *  page_p::check()
 *
 *  Check the page for consistency. All bytes should be accounted for.
 *
 *********************************************************************/
rc_t
page_p::check()
{
    /*
     *  Map area and mutex to protect it. Each Byte in map corresponds
     *  to a byte in the page.
     */
    static char map[data_sz];
    static smutex_t mutex("pgpck");

    /*
     *  Grab the mutex
     */
    W_COERCE( mutex.acquire() );
    
    /*
     *  Zero out map
     */
    memset(map, 0, sizeof(map));
    
    /*
     *  Compute our own end and nfree counters. Mark all used bytes
     *  to make sure that the tuples in page do not overlap.
     */
    int END = 0;
    int NFREE = data_sz + 2 * sizeof(slot_t);

    slot_t* p = _pp->slot;
    for (int i = 0; i < _pp->nslots; i++, p--)  {
	int len = int(align(p->length));
	int j;
	for (j = p->offset; j < p->offset + len; j++)  {
	    w_assert1(map[j] == 0);
	    map[j] = 1;
	}
	if (END < j) END = j;
	NFREE -= len + sizeof(slot_t);
    }

    /*
     *  Make sure that the counters matched.
     */
    w_assert1(END <= _pp->end);
    w_assert1(_pp->space.nfree() == NFREE);
    w_assert1(_pp->end <= (data_sz + 2 * sizeof(slot_t) - 
			   sizeof(slot_t) * _pp->nslots));

    /*
     *  Done 
     */
    mutex.release();

    return RCOK;
}




/*********************************************************************
 *
 *  page_p::~page_p()
 *
 *  Destructor. Unfix the page.
 *
 *********************************************************************/
page_p::~page_p()
{
    destructor();
}



/*********************************************************************
 *
 *  page_p::operator=(p)
 *
 *  Unfix my page and fix the page of p.
 *
 *********************************************************************/
w_rc_t
page_p::_copy(const page_p& p) 
{
    _refbit = p._refbit;
    _mode = p._mode;
    _pp = p._pp;
    page_bucket_info.nochecknecessary();
    if (_pp) {
	if( bf->is_bf_page(_pp)) {
	    /* NB: 
	     * But for the const-ness of p,
	     * we would also update p.page_bucket_info if
	     * it were out-of-date, but
	     * for the time being, let's be
	     * sure that we aren't refixing
	     * file pages (those keeping track
	     * of buckets).  (Note that we  do a refix
	     * every time we log something and need to 
	     * hang onto _last_mod_page).
	     */
	    space_bucket_t b = p.bucket();
	    if((p.page_bucket_info.old() != b) && 
		p.page_bucket_info.initialized()) {
		DBG(<<"");
		w_assert3(! p.rsvd_mode());
	    }
	    W_DO(bf->refix(_pp, _mode));
	    init_bucket_info();
	}
    }
    return RCOK;
}

page_p& 
page_p::operator=(const page_p& p)
{
    if (this != &p)  {
	if(_pp) {
	    if (bf->is_bf_page(_pp))   {
		W_COERCE(update_bucket_info());
		bf->unfix(_pp, false, _refbit);
		_pp = 0;
	    }
	    page_bucket_info.nochecknecessary();
	}

	W_COERCE(_copy(p));
    }
    return *this;
}


/*********************************************************************
 *
 *  page_p::upgrade_latch(latch_mode_t m)
 *
 *  Upgrade latch, even if you have to block 
 *
 *********************************************************************/
void
page_p::upgrade_latch(latch_mode_t m)
{
    w_assert3(bf->is_bf_page(_pp));
    bf->upgrade_latch(_pp, m);
    _mode = bf->latch_mode(_pp);
}

/*********************************************************************
 *
 *  page_p::upgrade_latch_if_not_block()
 *
 *  Upgrade latch to EX if possible w/o blocking
 *
 *********************************************************************/
rc_t
page_p::upgrade_latch_if_not_block(bool& would_block)
{
    w_assert3(bf->is_bf_page(_pp));
    bf->upgrade_latch_if_not_block(_pp, would_block);
    if (!would_block) _mode = LATCH_EX;
    return RCOK;
}

/*********************************************************************
 *
 *  page_p::page_usage()
 *
 *  For DU DF.
 *
 *********************************************************************/
void
page_p::page_usage(int& data_size, int& header_size, int& unused,
		   int& alignment, page_p::tag_t& t, slotid_t& no_slots)
{
    // returns space allocated for headers in this page
    // returns unused space in this page

    // header on top of data area
    const int hdr_sz = page_sz - data_sz - 2 * sizeof (slot_t );

    data_size = header_size = unused = alignment = 0;

    // space occupied by slot array
    int slot_size =  _pp->nslots * sizeof (slot_t);

    // space used for headers
    if ( _pp->nslots == 0 )
	     header_size = hdr_sz + 2 * sizeof ( slot_t );
    else header_size = hdr_sz + slot_size;
    
    // calculate space wasted in data alignment
    for (int i=0 ; i<_pp->nslots; i++) {
	// if slot is not vacant
	if ( _pp->slot[-i].offset != -1 ) {
	    data_size += _pp->slot[-i].length;
	    alignment += int(align(_pp->slot[-i].length) -
			     _pp->slot[-i].length);
	}
    }
    // unused space
    if ( _pp->nslots == 0 ) {
	  unused = data_sz; 
    } else {
	unused = page_sz - header_size - data_size - alignment;
    }
//	W_FORM(cout)("hdr_sz = %d header_size = %d data_sz = %d 
//		data_size = %d alignment = %d unused = %d\n",
//		hdr_sz, header_size, data_sz, data_size,alignment,unused);
			    

    t        = tag();        // the type of page 
    no_slots = _pp->nslots;  // nu of slots in this page

    assert(data_size + header_size + unused + alignment == page_sz);
}

void	
page_p::init_bucket_info(page_p::tag_t ptag, 
    uint4_t		page_flags
)
{ 
    // so that when/if called with unknown (t_any_p) tag,
    // we can get the tag off the page (assuming it's been
    // formatted)
    if(ptag == page_p::t_any_p) ptag = tag(); 
    if(rsvd_mode(ptag)) {
	/*
	 * If the page type calls for it, we must init
	 * space-usage histogram info in the page handle.
	 */
	space_bucket_t	b;

	if(page_flags & t_virgin) {
	    // called from fix before formatting is done--
	    // give it the max bucket size
	    b = (space_num_buckets-1);
	} else {
	    b = bucket();
	}
	DBG(<<"page_bucket_info.init " << int(b));
	w_assert3(b != space_bucket_t(-1)); // better not look like uninit
	page_bucket_info.init(b);
	// check on unfix is necessary 
    } else {
	// check on unfix is notnecessary 
	page_bucket_info.nochecknecessary();
    }
}

w_rc_t	
page_p::update_bucket_info() 
{ 
    /*
     * DON'T CALL UNLESS _pp is non-null
     * and it's a legit bf page.
     */
    w_assert3(_pp);

    if(rsvd_mode() && bf->is_bf_page(_pp, true) ) {
        // even if page is clean
	DBG(<<"update_bucket_info " << pid()
	    << " old bucket=" << int(page_bucket_info.old())
	    << " new bucket=" << int(bucket())
	);

	/*
	// This doesn't work quite that way when we don't trust the page
	// lsn, because the extent/store head pages might be out-of-sync
	// with this page if we're in redo and
	// we later be re-formatting or reallocating this page.
	*/
#ifdef DONT_TRUST_PAGE_LSN
	if(
	     (smlevel_0::operating_mode == smlevel_0::t_in_undo)
	     ||
	     (smlevel_0::operating_mode == smlevel_0::t_forward_processing)
	)  {
#endif
	    space_bucket_t b = bucket();
	    if((page_bucket_info.old() != b) && 
		page_bucket_info.initialized()) {
		DBG(<<"updating extent histo for pg " <<  pid());
		W_DO(io->update_ext_histo(pid(), b));
		DBG(<<"page_bucket_info.init " << int(b));
		w_assert3(b != space_bucket_t(-1)); // better not look like uninit
		page_bucket_info.init(b);
	    }
#ifdef DONT_TRUST_PAGE_LSN
	}
#endif
	page_bucket_info.nochecknecessary();
    }

    return RCOK;
}


/*
 * Returns MAX free space that could be on the page
 */
smsize_t 	
page_p::bucket2free_space(space_bucket_t b) 
{
    static smsize_t X[space_num_buckets] = {
	bucket_0_max_free,
	bucket_1_max_free,
	bucket_2_max_free,
	bucket_3_max_free,
#if HBUCKETBITS>=3
	bucket_4_max_free,
	bucket_5_max_free,
	bucket_6_max_free,
	bucket_7_max_free,
#if HBUCKETBITS==4
	bucket_8_max_free,
	bucket_9_max_free,
	bucket_10_max_free,
	bucket_11_max_free,
	bucket_12_max_free,
	bucket_13_max_free,
	bucket_14_max_free,
	bucket_15_max_free
#endif
#endif
    };
    return X[b];
}

space_bucket_t 
page_p::free_space2bucket(smsize_t sp) 
{
    // for use in gdb :
    uint4_t mask = space_bucket_mask_high_bits;

    // A page that has this amt of free space
    // is assigned to the following bucket.
    // Also, if we need a page with this much 
    // free space, what bucket would that be?

    switch(sp & mask) {
    case bucket_0_min_free: return 0;
    case bucket_1_min_free: return 1;
    case bucket_2_min_free: return 2;
#if HBUCKETBITS>=3
    case bucket_3_min_free: return 3;
    case bucket_4_min_free: return 4;
    case bucket_5_min_free: return 5;
    case bucket_6_min_free: return 6;
#if HBUCKETBITS>=4
    case bucket_7_min_free: return 7;
    case bucket_8_min_free: return 8;
    case bucket_9_min_free: return 9;
    case bucket_10_min_free: return 10;
    case bucket_11_min_free: return 11;
    case bucket_12_min_free: return 12;
    case bucket_13_min_free: return 13;
    case bucket_14_min_free: return 14;
#endif
#endif
    }


#if defined(NOTDEF)
    DBG(<<"free_space2bucket: space =" << unsigned(sp)
	<< " masked= " << unsigned(sp&mask) );
    DBG(<<"bucket 0 =" << unsigned(bucket_0_min_free ));
    DBG(<<"bucket 1 =" << unsigned(bucket_1_min_free ));
    DBG(<<"bucket 2 =" << unsigned(bucket_2_min_free ));
    DBG(<<"bucket 3 =" << unsigned(bucket_3_min_free ));
#if HBUCKETBITS>=3
    DBG(<<"bucket 4 =" << unsigned(bucket_4_min_free ));
    DBG(<<"bucket 5 =" << unsigned(bucket_5_min_free ));
    DBG(<<"bucket 6 =" << unsigned(bucket_6_min_free ));
    DBG(<<"bucket 7 =" << unsigned(bucket_7_min_free ));
#if HBUCKETBITS>=4
    DBG(<<"bucket 8 =" << unsigned(bucket_8_min_free ));
    DBG(<<"bucket 9 =" << unsigned(bucket_9_min_free ));
    DBG(<<"bucket 10 =" << unsigned(bucket_10_min_free ));
    DBG(<<"bucket 11 =" << unsigned(bucket_11_min_free ));
    DBG(<<"bucket 12 =" << unsigned(bucket_12_min_free ));
    DBG(<<"bucket 13 =" << unsigned(bucket_13_min_free ));
    DBG(<<"bucket 14 =" << unsigned(bucket_14_min_free ));
    DBG(<<"bucket 15 =" << unsigned(bucket_15_min_free ));
#endif
#endif
#endif /* NOTDEF */

	
#if HBUCKETBITS>=4
    smsize_t maximum = bucket_15_min_free;
    space_bucket_t result = 15;
#elif HBUCKETBITS>=3
    smsize_t maximum = bucket_7_min_free;
    space_bucket_t result = 7;
#else
    smsize_t maximum = bucket_3_min_free;
    space_bucket_t result = 3;
#endif

    w_assert1(sp < smlevel_0::page_sz && sp >= maximum);
    return result;
}


ostream &
operator<<(ostream& o, const store_histo_t&s)
{
    for (shpid_t p=0; p < space_num_buckets; p++) {
	o << " " << s.bucket[p] << "/" ;
    }
    o<<endl;
    return o;
}

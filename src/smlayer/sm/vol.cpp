/*<std-header orig-src='shore'>

 $Id: vol.cpp,v 1.238 1999/11/19 22:42:39 bolo Exp $

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
#define VOL_C
#ifdef __GNUG__
#   pragma implementation
#endif


#include <w_stream.h>
#include <sys/types.h>
#include "sm_int_1.h"
#include <extent.h>
#include <vol.h>
#include "sm_du_stats.h"
#include <crash.h>
#include <page_h.h>

#include <vtable_info.h>
#include <vtable_enum.h>

#ifdef EXPLICIT_TEMPLATE
template class w_auto_delete_t<page_s>;
template class w_auto_delete_array_t<extlink_t>;
template class w_auto_delete_array_t<stnode_t>;
template class w_auto_delete_array_t<ext_log_info_t>;
#endif

W_FASTNEW_STATIC_DECL(vol_t, 32); // #chunk_size == #bf_cleaner_threads

/*********************************************************************
 *
 *  sector_size : reserved space at beginning of volume
 *
 *********************************************************************/
/* XXX */
static const int sector_size = 512; 



/*********************************************************************
 *
 *  extlink_t::extlink_t()
 *
 *  Create a zero-ed out extent link
 *
 *********************************************************************/
extlink_t::extlink_t() : next(0), prev(0), owner(0), 
	pspacemap(w_base_t::uint4_max) // all bits set
	// so that it will err on the all-pages-have-space
	// side, until corrected.
{
    /*
     * make sure that the size of the filler field forces
     * correct alignment of the "next" field.  This is important
     * for initializing all of extlink_t since extlink_t is
     * copied with memcmp causing Purify headaches if
     * filler is not initialized
     */
    w_assert3(offsetof(extlink_t, next) == sizeof(pmap));

    /* is the aligned pmap aligned properly? */
    w_assert3(sizeof(pmap)/2 == (sizeof(pmap)+1)/2);
}

inline space_bucket_t 			
extlink_t::get_page_bucket(int pgindex) const
{
    // Which bits are we interested in?
    uint4_t shiftwidth = (pgindex * space_bucket_size_in_bits);

#if defined(NOTDEF)
    DBG(<<"get_page_bucket: shiftwidth " << shiftwidth);
    DBG(<<"get_page_bucket: mask " << space_bucket_mask);
    DBG(<<"get_page_bucket: pspacemap " << unsigned(pspacemap));
#endif

    uint4_t result = (pspacemap >> shiftwidth) & space_bucket_mask;  
#if defined(NOTDEF)
    DBG(<<"get_page_bucket: result " << result);
#endif

    return space_bucket_t(result);
}

/*********************************************************************
 *
 *  extlink_p::ntoh()
 *
 *  Never called since extlink_p never travels on the net.
 *
 *********************************************************************/
void 
extlink_p::ntoh()
{
    W_FATAL(eINTERNAL);
}


/*********************************************************************
 *
 *  extlink_p::format(pid, tag, flags, store_flags)
 *
 *  Format an extlink page.
 *
 *********************************************************************/
rc_t
extlink_p::format(
    const lpid_t& pid, 
    tag_t tag, 
    uint4_t flags, 
    store_flag_t store_flags)
{
    w_assert3(tag == t_extlink_p);

    extlink_t* links = new extlink_t[max];
    w_auto_delete_array_t<extlink_t> auto_del(links);

    memset(links, 0, max * sizeof(extlink_t) );
    vec_t vec;
    vec.put(links, max * sizeof(extlink_t));

    /* Do the formatting and insert w/o logging them */
    W_DO( page_p::format(pid, tag, flags, store_flags, false) );
    W_DO(page_p::insert_expand(0, 1, &vec, false) );

    /* Now, log as one (combined) record: */
    rc_t rc = log_page_format(*this, 0, 1, &vec);
    return rc;
}
MAKEPAGECODE(extlink_p, page_p);




/*********************************************************************
 *
 *  stnode_p::ntoh()
 *
 *  Never called since stnode_p never travels on the net.
 *
 *********************************************************************/
void 
stnode_p::ntoh()
{
    /* stnode_p never travels on the net */
    W_FATAL(eINTERNAL);
}




/*********************************************************************
 *
 *  stnode_p::format(pid, tag, flags, store_flags)
 *
 *  Format an stnode page.
 *
 *********************************************************************/
rc_t
stnode_p::format(const lpid_t& pid, tag_t tag, uint4_t flags, store_flag_t store_flags)
{
    w_assert3(tag == t_stnode_p);
	
    stnode_t* stnode = new stnode_t[max];
    w_auto_delete_array_t<stnode_t> auto_del(stnode);
    memset(stnode, 0, max * sizeof(stnode_t));

    vec_t vec;
    vec.put(stnode, max * sizeof(stnode_t));

    /* Do the formatting and insert w/o logging them */
    W_DO( page_p::format(pid, tag, flags, store_flags, false) );
    W_DO( page_p::insert_expand(0, 1, &vec, false) );

    /* Now, log as one (combined) record: */
    rc_t rc = log_page_format(*this, 0, 1, &vec);
    return rc;
}
MAKEPAGECODE(stnode_p, page_p);
    

ostream& operator<<(ostream &o, const extlink_t &e)
{
      o    << " num_set:" << e.num_set()
	   << " owner:" << e.owner 
	   << " prev:" << e.prev 
	   << " next:" << e.next ;
      return o;
}

#ifdef COMMENT

Volume layout:

   volume header 
   extent map -- fixed size determined by # extents given when volume
		 is formatted; part of store 0
   store map -- rest of store 0 
   data pages -- rest of volume

   The extent map pages pages of subtype extlink_p; each one is just a
   set of extlink_t structures, which contain a store number (if the
   extent is assigned to a store), a bit map indicating which of its
   pages are allocated, a next pointer, and a previous pointer.

   A store is a list of extents, whose head is in the "store map",
   and whose body is the linked extlink_t structures.
   
   Each page is mapped to a known extent by simple arithmetic.
   Each extent is inserted or deleted from a store by manipulating the
   linked list (gak).

   The protocol for manipulating the extent lists is as follows:

   1) latch the page containing the extent, if there is a multiple page
      update then the pages must be latched in ascending order to prevent
      deadlock.
   2) check if extent is still valid
   3) acquire an IX lock on the extent (this will never block, since IX mode 
      is the only mode ever held long-term (EX locks are acquired when an
      extent is being released to prevent others from reusing it until all
      the extlink_t pointers are updated or all the extents of a store are
      freed)
   4) read/write the page
   5) unlatch page

   the IX locks on extents serve to record which active xcts have modified an
   extent.  this is used to determine the case when there is only one active
   xct which have modified the extent (when there is only one and that one is
   completing is the only time when it is safe to free an empty extent).

   Extent locks do NOT fit into the lock granularity hierarchy with pages.

#endif /*COMMENT*/

/* Update the pmap in the desired fashion */
inline	void	
extlink_i::update_pmap(extnum_t idx,
   const Pmap &pmap,
   page_p::logical_operation how)
{
    xct_log_switch_t toggle(smlevel_0::OFF);

    slotid_t slot = (slotid_t) (idx % extlink_p::max);
    DBGTHRD(<<"update_pmap extent  " << idx << " pmap="  << pmap
	<< " how= " << int(how));
    _page.set_bytes(slot, 0, pmap.size(), pmap.bits, how);
}

/* Update the pspacemap in the desired fashion */
inline	void	
extlink_i::update_pspacemap(extnum_t idx,
   uint4_t	map,
   page_p::logical_operation how)

{
    xct_log_switch_t toggle(smlevel_0::OFF);

    slotid_t slot = (slotid_t) (idx % extlink_p::max);
    DBGTHRD(<<"update_pSPACEmap extent  " 
	<< idx << " map="  << unsigned(map)
	<< " how= " << int(how));
    _page.set_bytes(slot,  
	offsetof(extlink_t, pspacemap), sizeof(map), 
	(uint1_t *)&map,  how);
}

/*********************************************************************
 * 
 *  extlink_i::unfix()
 *
 *  Unfix the page if it's fixed
 *
 *********************************************************************/
void 
extlink_i::unfix()
{
    if(_page.is_fixed()) {
	 _page.unfix();
    }
}

#ifdef notdef
/*********************************************************************
 *  not used now; was once here for undoing partial updates
 * 
 *  extlink_i::discard()
 *
 *  Unfix the page and discard it
 *
 *********************************************************************/
void 
extlink_i::discard()
{
    w_assert1(_page.is_fixed());
    smlevel_0::bf->discard_pinned_page(&_page);
}
#endif /* notdef*/

/*********************************************************************
 *
 *  extlink_i::on_same_page(ext1, ext2)
 *
 *  Return true if the two extents are on the same page
 *
 *********************************************************************/
bool
extlink_i::on_same_page(extnum_t ext1, extnum_t ext2)  const
{
    w_assert3(ext1);
    w_assert3(ext2);
    return (ext1 / (extlink_p::max)) == (ext2 / extlink_p::max);
}


/*********************************************************************
 *
 *  extlink_i::get(idx, const extlink *&res)
 *  extlink_i::get_copy(idx, extlink &res)
 *
 *  Return the extent link at index "idx".
 *
 *********************************************************************/

/* XXX This is a hack for ANSI C++, which doesn't allow pointers
   to become "const" because of evil things that can happen.
   Unfortunately, that completely ruins passing mandatory "return via"
   arguments as a reference!  That is a serious loss of functionality.
   This function is a hack to make get() work for the places that
   modify extents that they get. */

rc_t	extlink_i::get(extnum_t idx, extlink_t* &res)
{
	const extlink_t *hack = res;
	w_rc_t	e;
	e = get(idx, hack);
	res = (extlink_t *) hack;
	return e;
}

rc_t
extlink_i::get(extnum_t idx, const extlink_t* &res)
{
    w_assert3(idx);
    lpid_t pid = _root;
    pid.page += idx / (extlink_p::max);

   // DBGTHRD(<<"extlink_i::get(" << idx <<  " )");

    bool was_already_fixed = _page.is_fixed();

    W_COERCE( _page.fix(pid, LATCH_SH) );

    if( !was_already_fixed ) {
	_dirty_on_pin = _page.is_dirty();
    }

    slotid_t slot = (slotid_t)(idx%(extlink_p::max));
    res = &_page.get(slot);

    // DBGTHRD(<<" get() returns " << *res  );
    w_assert3(res->next != idx); // no loops
    return RCOK;
}

rc_t
extlink_i::get_copy(extnum_t idx, extlink_t &res)
{
    const extlink_t *x;
    W_DO(get(idx, x));
    res = *x;
    w_assert3(res.next != idx); // no loops
    return RCOK;
}


/*********************************************************************
 * 
 *  extlink_i::fix_EX(idx)
 *
 *  Latch the extlink_p containing idx in EX mode
 *
 *********************************************************************/
rc_t 
extlink_i::fix_EX(extnum_t idx)
{
    w_assert3(idx);
    lpid_t pid = _root;
    pid.page += idx / (extlink_p::max);

    DBGTHRD(<<"extlink_i::fix_EX(" << idx << ")" );

    W_COERCE( _page.fix(pid, LATCH_EX) );

    return RCOK;
}


/*********************************************************************
 * 
 *  extlink_i::put(idx, e)
 *
 *  Copy e onto the slot at index "idx".
 *
 *********************************************************************/
rc_t 
extlink_i::put(extnum_t idx, const extlink_t& e)
{
    FUNC(extlink_i::put)

    w_assert3(idx);
#ifdef W_DEBUG
    Pmap pmap;
    e.getmap(pmap);
    DBGTHRD(<<"above getmap for ext " <<idx);
    w_assert3(e.owner || pmap.is_empty());
#endif /* W_DEBUG */

    lpid_t pid = _root;
    pid.page += idx / (extlink_p::max);

    DBGTHRD(<<"extlink_i::put(" << idx <<  " ) e =" << e );
    w_assert3(e.next != idx);

    W_COERCE( _page.fix(pid, LATCH_EX) );

    slotid_t slot = (slotid_t)(idx % (extlink_p::max));
    _page.put(slot, e);

    return RCOK;
}


/*********************************************************************
 *
 *  extlink_i::set_bit(idx, bit)
 *
 *  Set the bit at "bit" offset of the slot at "idx".
 *
 *********************************************************************/
rc_t 
extlink_i::set_bit(extnum_t idx, int bit)
{
    Pmap	pmap;
    pmap.set(bit);
    DBGTHRD(<<"set_bit " << bit << " for extent " << idx);
    W_DO( set_bits(idx, pmap) );

    return RCOK;
}

/*********************************************************************
 *
 *  extlink_i::set_bits(idx, &pmap)
 *
 *  Set the bits that are set in pmap
 *
 *********************************************************************/
rc_t
extlink_i::set_bits(extnum_t idx, const Pmap& pmap)
{
    DBGTHRD(<<"set_bits for extent " << idx 
	<< " bits= " << pmap);
    w_assert3(idx);
    uint4_t poff = _root.page + idx / extlink_p::max;

    _id.vol = _root.vol();
    _id.ext = idx;

    lpid_t pid = _root;
    pid.page = poff;
    W_COERCE( _page.fix(pid, LATCH_EX) );
    w_assert3( _page.latch_mode() == LATCH_EX );

    /*
     * set "ref bit" to indicate that this
     * is a hot page.  Let it sit in the buffer pool
     * for a few sweeps of the cleaner.
    _page.set_ref_bit(8);
    */

    w_assert3(!pmap.is_empty());  // should alloc at least one page

    // extlink_p::set_byte
    // idx tells what extent# -- convert that to an extlink_t
    // index relative to the page:

    update_pmap(idx, pmap, page_p::l_or);
    W_DO( log_alloc_pages_in_ext(_page, idx, pmap) );

    return RCOK;
}



/*********************************************************************
 *
 *  extlink_i::clr_bit(idx, bit)
 *
 *  Reset the bit at "bit" offset of the slot at "idx".
 *
 *********************************************************************/
rc_t 
extlink_i::clr_bit(extnum_t idx, int bit)
{
    Pmap	pmap;
    pmap.set(bit);
    W_DO( clr_bits(idx, pmap) );

    return RCOK;
}



/*********************************************************************
 *
 *  extlink_i::clr_bits(idx, &pmap)
 *
 *  Reset the bit at "bit" offset of the slot at "idx".
 *
 *********************************************************************/
rc_t 
extlink_i::clr_bits(extnum_t idx, const Pmap& pmap)
{
    DBGTHRD(<<"clr_bits for extent " << idx << " bits= " << pmap);
    w_assert3(idx);
    uint4_t poff = _root.page + idx / extlink_p::max;

    _id.vol = _root.vol();
    _id.ext = idx;

    lpid_t pid = _root;
    pid.page = poff;

    W_COERCE( _page.fix(pid, LATCH_EX) );
    w_assert3( _page.latch_mode() == LATCH_EX );

    w_assert3(!pmap.is_empty());	// should free at least one page

    update_pmap(idx, pmap, page_p::l_not);	    
    W_DO( log_free_pages_in_ext(_page, idx, pmap) );

    return RCOK;
}


/*********************************************************************
 *
 *  extlink_i::set_next(ext, new_next, log_it)
 *
 *  Reset the bit at "bit" offset of the slot at "idx".
 *
 *********************************************************************/
rc_t 
extlink_i::set_next(extnum_t ext, extnum_t new_next, bool log_it)
{
    w_assert3(ext);
    w_assert3(new_next);
    extlink_t	link;

    {
	xct_log_switch_t toggle(smlevel_0::OFF);
	W_DO( get_copy(ext, link) );
	link.next = new_next;
	W_DO( put(ext, link) );
    }

    if (log_it)  {
	W_DO( log_set_ext_next(_page, ext, new_next) );
    }

    return RCOK;
}



// moved the class definition of stnode_i into the header file so that
// it could be used by others


/*********************************************************************
 *
 *  stnode_i::get(idx)
 *
 *  Return the stnode at index "idx".
 *
 *********************************************************************/

// examine in-place interface
w_rc_t stnode_i::get(snum_t idx, const stnode_t *&stnodep)
{
    w_assert3(idx);
    lpid_t pid = _root;
    pid.page += idx / (stnode_p::max);
    W_DO( _page.fix(pid, LATCH_SH) );
    slotid_t slot = (slotid_t)(idx % (stnode_p::max));
    stnodep = &_page.get(slot);
    return RCOK;
}

// Copy-out interface
w_rc_t stnode_i::get(snum_t idx, stnode_t &stnode)
{
	const	stnode_t	*st;

	W_DO(get(idx, st));

	stnode = *st;
	return RCOK;
}

/*********************************************************************
 *
 *  stnode_i::put(idx, stnode)
 *
 *  Copy stnode to entry corresponding to index "idx".
 *
 *********************************************************************/
w_rc_t
stnode_i::put(snum_t idx, const stnode_t& stnode)
{
    lpid_t pid = _root;
    pid.page += idx / (stnode_p::max);
    W_DO(_page.fix(pid, LATCH_EX));
    slotid_t slot = (slotid_t) (idx % (stnode_p::max));
    return _page.put(slot, stnode);
}


/*********************************************************************
 *
 *  stnode_i::store_operation(param)
 *
 *  Perform the store operation described by param.
 *
 *********************************************************************/
w_rc_t
stnode_i::store_operation(const store_operation_param& param)
{
    w_assert3(param.snum());
    lpid_t pid = _root;
    pid.page += param.snum() / (stnode_p::max);
    W_DO( _page.fix(pid, LATCH_EX) );

    store_operation_param new_param(param);
    stnode_t& stnode = _page.item(param.snum() % (stnode_p::max));

    switch (param.op())  {
	case t_delete_store:
	    {
		stnode.head	= 0;
		stnode.eff	= 0;
		stnode.flags	= st_bad;
		stnode.deleting	= t_not_deleting_store;
	    }
	    break;
	case t_create_store:
	    {
		w_assert1(stnode.head == 0);

		stnode.head	= 0;
		stnode.eff	= param.eff();
		stnode.flags	= param.new_store_flags();
		stnode.deleting	= t_not_deleting_store;
	    }
	    break;
	case t_set_deleting:
	    {
		w_assert3(stnode.deleting != param.new_deleting_value());
		w_assert3(param.old_deleting_value() == t_unknown_deleting
				|| stnode.deleting == param.old_deleting_value());

		new_param.set_old_deleting_value((store_operation_param::store_deleting_t)stnode.deleting);

		stnode.deleting	= param.new_deleting_value();
	    }
	    break;
	case t_set_store_flags:
	    {
		if (stnode.flags == param.new_store_flags())  {;
		    // xct may have converted file type to regular and 
		    // then the automatic
		    // conversion at commit from insert_file 
		    // to regular needs to be ignored
		    DBG(<<"store flags already set");
		    return RCOK;
		} else  {
		    w_assert3(param.old_store_flags() == st_bad
			    || stnode.flags == param.old_store_flags());

		    new_param.set_old_store_flags(
			    (store_operation_param::store_flag_t)stnode.flags);

		    stnode.flags	= param.new_store_flags();
		}
		w_assert3(stnode.flags != st_bad);
	    }
	    break;
	case t_set_first_ext:
	    {
		w_assert3(stnode.head == 0);
		w_assert3(param.first_ext());

		stnode.head	= param.first_ext();
	    }
	    break;
    }

    W_DO( log_store_operation(_page, new_param) );

    return RCOK;
}


int				
vol_t::max_extents_on_page() 
{
    return extlink_p::max;
}

/*********************************************************************
 *
 *  vol_t::num_free_exts(nfree)
 *
 *  Compute and return the number of free extents in "nfree".
 *
 *********************************************************************/
rc_t
vol_t::num_free_exts(extnum_t& nfree)
{
    extlink_i ei(_epid);
    nfree = 0;
    for (uint i = _hdr_exts; i < _num_exts; i++)  {
	const extlink_t* ep;
	W_DO(ei.get(i, ep));
	if (ep->owner == 0)  {
	    ++nfree;
	}
    }
    return RCOK;
}



/*********************************************************************
 *
 *  vol_t::num_used_exts(nused)
 *
 *  Compute and return the number of used extents in "nused".
 *
 *********************************************************************/
rc_t
vol_t::num_used_exts(extnum_t& nused)
{
    extnum_t nfree;
    W_DO( num_free_exts(nfree) );
    nused = _num_exts - nfree;
    return RCOK;
}



/*********************************************************************
 *
 *  vol_t::sync()
 *
 *  Sync the volume.
 *
 *********************************************************************/
rc_t
vol_t::sync()
{
    smthread_t* t = me();
    W_COERCE_MSG(t->fsync(_unix_fd), << "volume id=" << vid());
    return RCOK;
}

    

/*********************************************************************
 *
 *  vol_t::mount(devname, vid)
 *
 *  Mount the volume at "devname" and give it a an id "vid".
 *
 *********************************************************************/
rc_t
vol_t::mount(const char* devname, vid_t vid)
{
    if (_unix_fd >= 0) return RC(eALREADYMOUNTED);

    /*
     *  Save the device name
     */
    w_assert1(strlen(devname) < sizeof(_devname));
    strcpy(_devname, devname);

    /*
     *  Check if device is raw, and open it.
     */
    W_DO(log_m::check_raw_device(devname, _is_raw));

    w_rc_t e;
    int	open_flags = smthread_t::OPEN_RDWR;
#ifdef _WIN32
    /* Windows NT has a bug.  If volumes are greater than 4GB in size,
       the buffer cache screws up and maps stuff at 4GB+X to 0+X.  This
       only happens when files are buffered.  Uncached files are
       not affected.  The default on NT will be to run with a
       "raw volume" instead of an ordinary volume until this NT bug
       is fixed. */
    open_flags |= smthread_t::OPEN_RAW;
#endif
    {
    	char *s = getenv("SM_VOL_RAW");
	if (s && s[0] && atoi(s) > 0)
		open_flags |= smthread_t::OPEN_RAW;
	else if (s && s[0] && atoi(s) == 0)
		open_flags &= ~smthread_t::OPEN_RAW;
    }
    e = me()->open(devname, open_flags, 0666, _unix_fd);
    if (e != RCOK) {
	_unix_fd = -1;
	return e;
    }

    /*
     *  Read the volume header on the device
     */
    volhdr_t vhdr;
    {
	rc_t rc = read_vhdr(_devname, vhdr);
	if (rc)  {
	    W_COERCE_MSG(me()->close(_unix_fd), << "volume id=" << vid);
	    _unix_fd = -1;
	    return RC_AUGMENT(rc);
	}
    }
    if ( smlevel_0::log ) {
	/* Someone wants to violate these assumptions if
	 * logging is turned off! */
        w_assert3(vhdr.ext_size() == ext_sz);
    }
    /*
     *  Save info on the device
     */
    _vid = vid;
#if defined(W_DEBUG) || defined(W_DEBUG_NAMES)
    char buf[64];
    ostrstream sbuf(buf, sizeof(buf));
    sbuf << "vol(vid=" << (int) _vid.vol << ")" << ends;
    _mutex.rename("m:", buf);
    /* XXX how about restoring the old mutex name when done? */
#endif /* W_DEBUG */
    _lvid = vhdr.lvid();
    _num_exts =  vhdr.num_exts();
    _epid = lpid_t(vid, 0, vhdr.epid());
    _spid = lpid_t(vid, 0, vhdr.spid());
    _hdr_exts = vhdr.hdr_exts();

    _min_free_ext_num = _hdr_exts;
    DBG(<<"_min_free_ext_num=" << _min_free_ext_num);

    W_COERCE( bf->enable_background_flushing(_vid));

    _stats.reset();

    return RCOK;
}



/*********************************************************************
 *
 *  vol_t::dismount(flush)
 *
 *  Dismount the volume. 
 *
 *********************************************************************/
rc_t
vol_t::dismount(bool flush)
{
    /*
     *  Flush or force all pages of the volume cached in bf.
     */
    w_assert1(_unix_fd >= 0);
    W_COERCE_MSG( flush ? 
	      bf->force_volume(_vid, true) : 
	      bf->discard_volume(_vid), << "volume id=" << vid() );
    W_COERCE_MSG( bf->disable_background_flushing(_vid), << "volume id=" << vid());

    /*
     *  Close the device
     */
    w_rc_t e;
    e = me()->close(_unix_fd);
    if (e != RCOK)
	    return e;

    _unix_fd = -1;
    
    _stats.reset();

    return RCOK;
}



/*********************************************************************
 *
 *  vol_t::check_disk()
 *
 *  Print out meta info on the disk.
 *
 *********************************************************************/
rc_t
vol_t::check_disk()
{
    FUNC(vol_t::check_disk);
    volhdr_t vhdr;
    W_DO( read_vhdr(_devname, vhdr));
    smlevel_0::errlog->clog << info_prio << "vol_t::check_disk()\n";
    smlevel_0::errlog->clog << info_prio << "\tvolid      : " << vhdr.lvid() << flushl;
    smlevel_0::errlog->clog << info_prio << "\tnum_exts   : " << vhdr.num_exts() << flushl;
    smlevel_0::errlog->clog << info_prio << "\text_size   : " << vhdr.ext_size() << flushl;

//jk BOTH ST AND EXT
    stnode_i st(_spid);
    extlink_i ei(_epid);
    for (extnum_t i = 1; i < _num_exts; i++)  {
	stnode_t stnode;
	W_DO(st.get(i, stnode));
	if (stnode.head)  {
	    smlevel_0::errlog->clog << info_prio << "\tstore " << i << " is active: ";
	    const extlink_t *link_p;
	    for (snum_t j = stnode.head; j; ){
		smlevel_0::errlog->clog << info_prio << '[' << j << "] ";
		W_DO(ei.get(j, link_p));
		j = link_p->next;
	    }
	    smlevel_0::errlog->clog << info_prio << '.' << flushl;
	}
    }

    return RCOK;
}



/*********************************************************************
 *
 *  vol_t::first_ext(store, result)
 *
 *  Return the first extent of store.
 *  A value of 0 means that the  store is not active.
 *
 *********************************************************************/
rc_t
vol_t::first_ext(snum_t snum, extnum_t &result)
{
    stnode_t stnode;
    {
	stnode_i st(_spid);
	W_DO(st.get(snum, stnode));
    }
    result = stnode.head;


#ifdef W_DEBUG
    if(result) {
	extlink_i ei(_epid);
	extlink_t link;
	W_COERCE(ei.get_copy(result, link));
	w_assert3(link.prev == 0);
	w_assert3(link.owner == snum);

    } else {
	DBG(<<"Store has no extents");
    }
#endif /* W_DEBUG */
    return RCOK;
}



/*********************************************************************
 *
 *  vol_t::fill_factor(store)
 *
 *  Return the extent fill factor of store.
 *
 *********************************************************************/
int
vol_t::fill_factor(snum_t snum)
{
    stnode_i st(_spid);
    const stnode_t *stnode;
    W_COERCE(st.get(snum, stnode));
    return stnode->eff;
}


/*********************************************************************
 *
 *  vol_t::alloc_page_in_ext(append_only, ext, eff, snum, cnt, pids, 
 *      allocated, remainint, is_last, may_realloc, lockmode)
 *
 *  Attempt to allocate "cnt" pages in the extent "ext" for store
 *  "snum". The number of pages successfully allocated is returned
 *  in "allocated", and the pids allocated is returned in "pids".
 *
 *********************************************************************/
rc_t
vol_t::alloc_page_in_ext(
    bool	append_only,
    extnum_t 	ext,
    int 	eff,
    snum_t 	snum,
    int 	cnt,
    lpid_t 	pids[],
    int& 	allocated,
    int&	remaining,
    bool&	is_last,   // return true if ext has next==0
    bool	may_realloc,  // = false
    lock_mode_t desired_lock_mode // =IX;  used iff may_realloc==false
    )
{
    FUNC(vol_t::alloc_page_in_ext);
    /*
     *  Sanity checks
     */
    w_assert1(eff >= 0 && eff <= 100);
    w_assert1(_is_valid_ext(ext));
    w_assert1(cnt > 0);

    lockid_t*	name = 0;

    INC_TSTAT(alloc_page_in_ext); // count the attempts

    allocated = 0;

    /*
     *  Try to lock the extent. If failed, return 0 page allocated.
     */
    {
	extid_t extid;
	extid.vol = _vid;
	extid.ext = ext;

	// force not required here, since extents do not appear in hierarchy
	W_DO( lm->lock(extid, IX, t_long, WAIT_IMMEDIATE, 0, 0, &name) );
    }
    
    /*
     *  Count number of usable pages in the extent, taking into
     *	account the extent fill factor.
     */
    extlink_i ei(_epid);
    extlink_t link;
    extlink_t bits_allocated; // for compressing multiple
			    // bit_set operations into one.

    W_DO(ei.get_copy(ext, link)); // FIXes ext link
    w_assert1(link.owner == snum);

    int nfree = link.num_clr() * eff/100;
    remaining = nfree;

    is_last = (link.next == 0);

    DBG(<<"extent " << ext
    	<< " eff " << eff
	<<" nfree " << nfree);

    allocated = 0;
    if (nfree > 0)  {
	/*
	 *  Some pages free
	 */
	if (nfree > cnt) nfree = cnt;
	shpid_t base = ext2pid(snum, ext); // for assigning pid
	int start;
	if(append_only) {
	    start = link.last_set(0);
	} else {
	    start = -1;
	}
	for (int i = 0; i < nfree; i++, allocated++)  {

	    lock_mode_t m;

	    /*
	     *  Find a free page that nobody has locked.
	     *  This means NOBODY, including me (this transaction).
	     *  NB: this relies on EACH page lock being explicitly acquired 
	     *  during page allocation -- it is NOT OK to bypass the lock 
	     *  table because a volume lock subsumes the page lock.
	     *
	     * Correction: Now that extents remain in the store
	     * until commit time, some of the grunge of checking
	     * for acceptable pages is obviated.
	     * We only have to get an instant lock on the page;
	     * if that works, we can allocate the page.  There's no
	     * way that the page could move from one kind of store
	     * (physically logged) to be allocated to another (preventing
	     * rollback from working) anymore.
	     */
	    do {
		++start;
		start = (start >= ext_sz ? -1 : link.first_clr(start));
		if (start < 0) break;
		
		pids[i]._stid = stid_t(_vid, snum);
		pids[i].page = base + start;
		

		/* 
		 * ASSUMPTIONS:
		 * 1) pages do not leave stores until commit.
		 * 2) whoever deallocated this page has an EX lock on it
		 * 3) Calling resource managers are either:
		 *    -like file_m:  do physical undo, and therefore
		 *       cannot cope with reallocating (and therefore
		 *       re-formatting) a page within the file (UNLESS
		 *       it logs the whole page format),
		 *    -like btree_m:  does only logical undo (where
		 *       page allocation/dealloc are concerned), and
		 *       indeed wants to reallocate a page that it
		 *       deallocated within this xct
		 *   The argument may_realloc distinguishes these two
		 *   cases.  In fact, the resource manager might
		 *   say it's ok to realloc a page only on rollback.
		 */
		if(may_realloc) {
		    w_rc_t rc = io_lock_force(pids[i], EX, 
			t_instant, WAIT_IMMEDIATE);
		    if(rc){
			// Skip this page if other tx has lock on it
			m = EX;
		        continue; // reevaluate the condition test
		    }
		    // accept this page
		    m = NL;
		} else {
		    // Skip this page if ANY tx has a lock on it

		    /* NB: RACE
		     * In this case (in order to avoid the race
		     * condition between finding the right page
		     * and locking it), we need a lock manager
		     * method to acquire the lock if we don't 
		     * already have it, and tell us if we do already
		     * have it
		     */

		    // old:
		    //
		    // W_DO( lm->query(pids[i], m) );

		    DBG(<<"vol_t::alloc_page_in_ext: locking page "
			<< pids[i]  << " in mode " << int(desired_lock_mode));
		    w_rc_t rc = io_lock_force(pids[i], desired_lock_mode, 
					    t_long, WAIT_IMMEDIATE,
					    &m);
		    if(rc){
			// Skip this page if other tx has lock on it
			DBG(<<"skipping: prior mode=" << int(m)
				<< "rc=" <<rc );
			m = EX; // anything other than NL will do
		        continue; //evaluate while condition
		    }
		    if(m != NL) {
			DBG(<<"skipping: prior mode=" << int(m));
			// Skip this page if THIS tx had a lock on it
			// already
		        continue; //evaluate while condition
		    }
		    w_assert3(m == NL);
		}

	    } while (m != NL);

	    if (start < 0) break;

	    DBGTHRD( << "   allocating page " << pids[i] );
	    link.set(start);
	    bits_allocated.set(start);
	}

        if (allocated > 0) {
	    // Set the bits all at once.
	    Pmap tmp;
	    bits_allocated.getmap(tmp);
	    DBGTHRD( << "    setting bits " << tmp << " in ext " << ext );
	    W_COERCE( ei.set_bits(ext, tmp) );

	    // allocated a page in this extent, mark it as so
	    name->set_ext_has_page_alloc(true);
        }
    }

    xct()->set_alloced();
    remaining -= allocated;
    return RCOK;
}




/*********************************************************************
 *
 *  vol_t::recover_pages_in_ext(ext, pmap, is_alloc)
 *
 *  allocs or frees pages in pmap from ext, newPmap is returned.
 *
 *********************************************************************/
rc_t
vol_t::recover_pages_in_ext(extnum_t ext, const Pmap& pmap, bool is_alloc)
{
    extlink_i ei(_epid);

    extid_t	extid;
    extid.vol = _vid;
    extid.ext = ext;
    lockid_t*	name = 0;
    W_DO( lm->lock(extid, IX, t_long, WAIT_IMMEDIATE, 0, 0, &name) );

    DBGTHRD(<<"recover_pages_in_ext " << ext
	<< "map=" << pmap 
	<< " is_alloc=" << is_alloc);

    if (is_alloc)  {
	W_COERCE( ei.set_bits(ext, pmap) );
	if (name)
	    name->set_ext_has_page_alloc(true);
    }  else  {
	W_COERCE( ei.clr_bits(ext, pmap) );
	extlink_t	link;
	W_COERCE( ei.get_copy(ext, link) );
	if (name && link.num_set() == 0)
	    name->set_ext_has_page_alloc(false);
    }

    return RCOK;
}



/*********************************************************************
 *
 *  vol_t::store_operation(snum, value)
 *
 *  sets the store deleting flag to value.
 *
 *********************************************************************/
rc_t
vol_t::store_operation(const store_operation_param& param)
{
    stnode_i si(_spid);
    W_DO( si.store_operation(param) );
    return RCOK;
}



/*********************************************************************
 *
 *  vol_t::free_stores_during_recovery(typeToRecover)
 *
 *  search all the stnodes looking for stores which have the deleting
 *  attribute set to the desired value and free those stores.
 *
 *  called only during recovery.
 *
 *********************************************************************/
rc_t
vol_t::free_stores_during_recovery(store_deleting_t typeToRecover)
{
    w_assert3(in_recovery());

    stnode_i	si(_spid);
    stnode_t	stnode;
    int		i = 0;
    stid_t	stid;
    stid.vol = _vid;

    while (is_valid_store(++i))  {
	W_DO(si.get(i, stnode));
	if (stnode.deleting == typeToRecover)  {
	    lock_mode_t		m = NL;

	    stid.store = i;
	    W_DO( lm->query(stid, m) );
	    if (m == NL)  {
	        W_DO( free_store_after_xct(i) );
	    }
	}
    }

    return RCOK;
}


/*********************************************************************
 *
 *  vol_t::free_exts_during_recovery()
 *
 *  search all the extlink_t's looking for exts which are allocated,
 *  are empty, and isn't the first extent in a store.
 *
 *  called only during recovery.
 *
 *********************************************************************/
rc_t
vol_t::free_exts_during_recovery()
{
    w_assert3(in_recovery());

    extnum_t	i = 0;
    while (_is_valid_ext(++i))  {
	snum_t dummy=0; // ignored
	W_DO( free_ext_after_xct(i, dummy) );
    }

    return RCOK;
}


/*********************************************************************
 *
 *  vol_t::free_page(pid)
 *
 *  Free the page "pid".
 *
 *********************************************************************/
rc_t
vol_t::free_page(const lpid_t& pid)
{
    w_assert1(pid.store());
    extnum_t ext = pid2ext(pid);
    int offset = int(pid.page % ext_sz);

    /*
     *  Set long lock to prevent this page from being reused until 
     *  the xct commits.
     */
    /* NB: force required -- see comments in io_lock_force */
    //jk TODO if multiple xct were acting on page, it's possible we can't
    // get this lock, in that case we should just return and not free the page
    W_DO( io_lock_force(pid, EX, t_long, WAIT_IMMEDIATE) );

    extid_t	extid;
    extid.vol = pid._stid.vol;
    extid.ext = ext;
    lockid_t*	name = 0;
    W_DO(lm->lock(extid, IX, t_long, WAIT_IMMEDIATE, 0, 0, &name));

    extlink_i ei(_epid);
    extlink_t link;
    W_DO(ei.get_copy(ext, link));
    w_assert1(link.owner == pid.store());
    w_assert1(link.is_set(offset));

    W_DO( ei.clr_bit(ext, offset) );
    if (link.num_set() == 1)  {
	// freeing the last page in an extent, mark it as so
	DBG(<<"name is now " << *name);
	name->set_ext_has_page_alloc(false);
	DBG(<<"name is now " << *name);
    }

    xct()->set_freed();
    DBGTHRD(<<"freed pid " <<pid);
    return RCOK;
}



/*********************************************************************
 *
 *  vol_t::next_page(pid, allocated)
 *
 *  Given page "pid" of a particular store, return the next
 *  allocated pid of the store in "pid" if "allocated" is NULL.
 *  If "allocated" is not NULL, return the next pid regardless
 *  of its allocation status and return the allocation status
 *  in "allocated".
 *
 *********************************************************************/
rc_t
vol_t::next_page(lpid_t& pid, bool* allocated)
{
    FUNC(vol_t::next_page);
#ifdef W_TRACE
    lpid_t save_pid = pid;
#endif
    extnum_t ext = pid2ext(pid);
    int offset = int(pid.page % ext_sz);

    extlink_i ei(_epid);
    const extlink_t *linkp;
    W_DO(ei.get(ext, linkp));

    // had better be allocated, and to the right store
    w_assert1(linkp->owner == pid.store());
    w_assert1(linkp->is_set(offset) || allocated);

    /*
     *  Loop skips over unallocated pages in extent
     *  assuming that allocated is NULL
     */
    do {
	if (++offset >= ext_sz)  {
	    offset = 0;
	    if (linkp->next) {
		W_DO(ei.get(ext = linkp->next, linkp));
	    } else {
		pid = lpid_t::null;
		return RC(eEOF);
	    }
	}
    } while (linkp->is_clr(offset) && !allocated);
    
    pid.page = ext * ext_sz + offset;
#ifdef W_DEBUG
    {
	Pmap pmap;
	linkp->getmap(pmap);
	DBG(<<"in next_page, computing allocated, pmap = " << pmap);
    }
#endif /* W_DEBUG */
    if (allocated) *allocated = linkp->is_set(offset);
    
    DBG(<< "next_page after " << save_pid << " == " << pid);
    return RCOK;
}

/*
 *  vol_t::next_page_with_space(lpid_t& pid, space_bucket_t needed)
 *  
 *  Find next page with adequate space - i.e., its bucket is
 *  >= "needed".   Search for exact fit; if not found in first
 *  extent page, but first fit was found, returns first fit.
 */

rc_t
vol_t::next_page_with_space(lpid_t& pid, space_bucket_t needed)
{
    FUNC(vol_t::next_page_with_space);
    extnum_t ext = pid2ext(pid);
    int offset = int(pid.page % ext_sz);

    extlink_i ei(_epid);
    const extlink_t *linkp;
    W_DO(ei.get(ext, linkp));

    // had better be allocated, and to the right store
    w_assert1(linkp->owner == pid.store());
    w_assert1(linkp->is_set(offset));

    DBG(<< "Find next page after " << pid << "; need bucket " 
	    << int(needed));
    /*
     *  Loop skips over allocated pages in extent
     */
    lpid_t first_fit = lpid_t::null;
    space_bucket_t b;
    do {
#if defined(NOTDEF)
	DBG(<<"ext=" << ext << " offset = " << offset);
#endif
	if (++offset >= ext_sz)  {
	    // No more pages in extent
	    // Go on to next extent if we
	    // have found nothing yet (not first_fit)
	    offset = 0;
	    extnum_t	nxt = linkp->next;
	    DBG(<< ext << " -> next ="  << nxt);
	    if (nxt) {
		DBG(<<"first_fit=" << first_fit);
		if( extlink_p::on_same_page(ext, nxt) || !first_fit.page) {
		    W_DO(ei.get(ext = linkp->next, linkp));
		} else {
		    // Not on same page and have a first fit
		    pid = first_fit;
		    DBG(<< "next ext pg but have first fit: " << pid);
		    return RCOK;
		}
	    } else {
		DBG(<< "no more extents, first_fit is " << first_fit);
		// no more extents in file
		pid = first_fit;
		return first_fit.page ? RCOK : RC(eEOF);
	    }
	}
	// Got a new page to check: is it allocated?

	if( !linkp->is_clr(offset) ) {
	    // allocated
	    // Is its bucket big enough ?
	    b = linkp->get_page_bucket(offset);
	    // Lower bucket#  --> fuller page
	    // Higher bucket# --> page less full

	    DBG(<<" offset " << offset << 
		" is allocated, in ext.bucket=" << int(b));
	    if(b == needed){
		// exact match
		pid.page = ext * ext_sz + offset;
		DBG(<< "exact match " << pid);
		return RCOK;
	    }
	    if(b > needed && !first_fit.page) {
		first_fit = lpid_t(pid.stid(), ext*ext_sz + offset);
		DBG(<<"set first fit to " << first_fit
			<< " with bucket " << int(b));
	    } 
#ifdef W_DEBUG
   {
	lpid_t lpid = pid;
	lpid.page = ext * ext_sz + offset;
	page_p page;
	w_assert3(!page.is_fixed());
	store_flag_t sf = st_bad;
	W_DO( page.fix(lpid, page_p::t_any_p, LATCH_SH, 0, sf));

        DBG(    <<"page.usable_space=" << page.usable_space()
		<<" page.bucket=" << int(page.bucket()) );
	if( page.bucket() < b) {
	    // This is ok -we'll find out when we
	    // look at the page.
	    // w_assert3(0);
	    cerr << "extent bucket info is high " <<endl;
	}
	if( page.bucket() > b) {
	    // This is NOT ok because the
	    // page will be skipped.
	    smlevel_0::errlog->clog << error_prio 
	    	<< "Warning: page will be skipped; extent bucket is low \n";
	    // w_assert3(0);
	}
   }
#endif /* W_DEBUG */

	}
	// continue search
    } while(1);
    W_FATAL(eINTERNAL);
}




/*********************************************************************
 *
 *  vol_t::find_free_exts(cnt, exts, found, first_ext)
 *
 *  Find "cnt" free extents starting from "first_ext". The number
 *  of extents found and their ids are returned in "found" and "exts"
 *  respectively.
 *
 *********************************************************************/
rc_t
vol_t::find_free_exts(
    uint 	cnt, 
    extnum_t 	exts[], 
    int& 	found, 
    extnum_t 	first_ext)
{
    FUNC(vol_t::find_free_exts);
    extlink_i ei(_epid);
    extid_t extid;
    extid.vol = _vid;
    DBGTHRD(<<"find_free_exts(cnt="<<cnt<<", first_ext="<<first_ext<<")");

    /*
     *  i: # extents 
     *  j: extent offset starting from first_ext.
     */
    if (first_ext == 0)  {
	first_ext = _min_free_ext_num;
    }
    bool alloced_min_free_ext = false;
    bool passed_zero = false;
    uint ext = first_ext;
    DBGTHRD(<<"_min_free_ext_num=" << _min_free_ext_num
	<< " starting loop with extent " << ext);
    uint i;
    for (i = 0; i < cnt; ++i) {
	/*
	 *  Loop to find an extent that is both free and not locked.
	 *
	 *  An extent that is truly free will have both its owner set to 0
	 *  and will not have any locks held on it.  An extent will have
	 *  the owner set to 0 and a lock held on it only if 1) some xct is in
	 *  the process of freeing the extent and it clears the owner before
	 *  releasing the lock; and 2) some other xct just called this routine
	 *  and hasn't allocated the extents which this routine returned (this
	 *  shouldn't happen because all the routines that call this are 
	 *  protected by the io_m mutex and they all allocate the extents 
	 *  before releasing the io_m mutex).
	 * 
	 *  There is no race condition between (checking the owner being 0 and
	 *  checking the extent lock) and (acquiring the extent lock) since
	 *  this is the only routine that gets a lock on an unowned extent
	 *  and the extent page latch prevents some other xct from 
	 *  also getting the lock.
	 */

	do  {
	    extlink_t link;
	    W_DO( ei.get_copy(ext, link) ); 
	    DBG( << "    ext =" << ext << ", owner=" << link.owner
		    << ", mytid=" << xct()->tid() );

	    if (ext == _min_free_ext_num)  {
		alloced_min_free_ext = true;
	    }

	    if (link.owner == 0) {
		extid.ext = ext;
	        lock_mode_t m = NL;
	        W_DO( lm->query(extid, m) );
	        if (m == NL)  {
	            // it's free and acceptable
	            DBG( << "found " << ext );
	            break;
	        }
	    }

	    if (++ext >= _num_exts)  {
		ext = _hdr_exts;
		passed_zero = true;
	    }
	}  while (!passed_zero || ext < first_ext);

	if (passed_zero && ext >= first_ext)  {
	    found = i;
	    W_RETURN_RC_MSG(eOUTOFSPACE, << "volume id = " << _vid);
	}

	/* force not required here, since extents do not
	 * appear in a hierarchy; and since we are using
	 * WAIT_IMMEDIATE, we don't need io_lock_force.
	 * this should always succeed.
	 */
	W_DO( lm->lock(extid, IX, t_long, WAIT_IMMEDIATE) );

	DBG(<<"Got the lock for " << ext);

	{
	    // verify still not owned after getting the lock.
	    // this should always succeed.
	    const extlink_t *lp;
	    W_DO(ei.get(ext, lp));
	    w_assert1( ! lp->owner);
	}

	exts[i] = ext;
    }
    found = i;

    if (alloced_min_free_ext)  {
	extnum_t new_min_free_ext_num = exts[i - 1] + 1;
	if (new_min_free_ext_num < _num_exts)  {
	    _min_free_ext_num = new_min_free_ext_num;
	    DBG(<<"_min_free_ext_num=" << _min_free_ext_num);
	}  else  {
	    _min_free_ext_num = _hdr_exts;
	    DBG(<<"_min_free_ext_num=" << _min_free_ext_num);
	}
    }

    w_assert3(_is_valid_ext(_min_free_ext_num));
	    
    return RCOK;
}




/*********************************************************************
 *
 *  vol_t::alloc_exts(snum, prev, cnt, exts)
 *
 *  Give the store id, the previous extent number, and an array of
 *  "cnt" extents in "exts", allocate these extents for the store
 *  and hook them  up to "prev". 
 *
 *  prev must be the last extent in the store.  new extents are
 *  not allowed to inserted into the middle of the extent list.
 *
 *  first the list of extents are linked together logging all the
 *  extents which fit on an extlink_p as one log record.  this is
 *  done from the tail of the list to the beginning.
 *
 *  only one page is pinned at a time.  after the extent list is
 *  linked together, the store head is updated if this is the first
 *  extent added to a store.
 *
 *********************************************************************/
rc_t
vol_t::alloc_exts(
    snum_t 		snum,
    extnum_t 		prev,
    int 		cnt, 
    const extnum_t 	exts[])
{
    FUNC(vol_t::alloc_exts);

    W_DO( _append_ext_list(snum, prev, cnt, exts) );

    SSMTEST("extent.3");
    if (prev == 0)  {
        DBG( << " first extent in store " << exts[0] );

        W_DO( set_store_first_ext(snum, exts[0]) );
    }

    return RCOK;
}

/*********************************************************************
 *
 *  rc_t vol_t::update_ext_histo()
 *
 *  Given an extent and page id, update the bucket info for that page. 
 *
 *********************************************************************/
rc_t 
vol_t::update_ext_histo(const lpid_t& pid, space_bucket_t bucket)
{
    xct_log_switch_t toggle(smlevel_0::OFF);

    extlink_i ei(_epid);
    extnum_t ext = pid2ext(pid._stid.store/*unused*/,  pid.page );
    w_assert1(_is_valid_ext(ext));
    extnum_t which = int(pid.page % ext_sz);
    W_DO(ei.update_histo(ext, which, bucket));
    return RCOK;
}

rc_t
extlink_i::update_histo(extnum_t ext, int offset, space_bucket_t bucket)
{
    W_DO(fix_EX(ext));

    slotid_t slot = (slotid_t)(ext%(extlink_p::max));
    const extlink_t* link = &_page.get(slot);

    w_assert1(link->owner);

    {
	// This had better not be a no-op
	// We want to track all these down if possible...
	space_bucket_t b = link->get_page_bucket(offset);
	if(b == bucket) {
	    INC_TSTAT(fm_ext_touch_nop);
	    return RCOK;
	}
    }
    // make a copy of the space map
    uint4_t map = link->pspacemap;

    // Set the right bucket bits :
    {
	// bucket# must fit in mask bits!
	w_assert3((bucket & space_bucket_mask)==bucket);

	uint4_t shiftwidth = (offset * space_bucket_size_in_bits);
	uint4_t bits  = bucket << shiftwidth;
	uint4_t mask  = space_bucket_mask << shiftwidth;

	map &= ~mask;  // bucket value = 0
	map |= bits;   // bucket value = i
	DBGTHRD(<<"set_page_bucket for offset(pid) " << offset
	    << " to " << int(bucket)
	    << " oldmap= " << int(link->pspacemap)
	    << " newmap= " << unsigned(map) );
    }
    // update the bits on the extent page
    update_pspacemap(ext, map, page_p::l_set);
    INC_TSTAT(fm_ext_touch);

#ifdef W_DEBUG
    map = link->pspacemap;
    DBGTHRD(<<"after update_pspacemap, it looks like this: " << unsigned(map));
#endif /* W_DEBUG */

    return RCOK;
}



/*********************************************************************
 *
 *  rc_t vol_t::next_ext(ext, ext& result)
 *
 *  Given an extent, return the extent that is linked to it.
 *
 *********************************************************************/
rc_t vol_t::next_ext(extnum_t ext, extnum_t &result)
{
    extlink_i ei(_epid);
    w_assert1(_is_valid_ext(ext));
    const extlink_t* link;
    W_DO(ei.get(ext, link)); 
    w_assert1(link->owner);
    result = link->next;
    return RCOK;
}




/*********************************************************************
 *
 *  rc_t vol_t::dump_exts(extnum_t start, extnum_t end)
 *
 *  Dump extents from start to end.
 *
 *********************************************************************/
rc_t vol_t::dump_exts(ostream &o, extnum_t start, extnum_t end)
{
    if (!_is_valid_ext(start))
	start = _num_exts - 1;
    else if (start == 0)
	start = 1;

    if (end == 0)
	end = _num_exts - 1;
    else if (!_is_valid_ext(end))
	end = _num_exts - 1;
    
    extlink_i ei(_epid);
    for (extnum_t i = start / 5 * 5; i <= end; i++)  {
	if (i % 5 == 0)
	{
	    W_FORM2(o, ("%5d:", i));
	}

	if (i < start)  {
	    o << "                      ";
	}  else  {
	    const extlink_t* link;
	    W_DO( ei.get(i, link) );
	    Pmap theMap;
	    link->getmap(theMap);
	    W_FORM2(o,("%5d<%5d %5d>", link->owner, link->prev, link->next));
	    o << theMap << "#";
	}

	if (i % 5 == 4)
	    o << endl;
    }

    if (end % 5 != 4)
        o << endl;
    
    return RCOK;
}



/*********************************************************************
 *
 *  rc_t vol_t::dump_stores(int start, int end)
 *
 *  Dump stores from start to end.
 *
 *********************************************************************/
rc_t vol_t::dump_stores(ostream &o, int start, int end)
{
    if (!is_valid_store(start))
	start = _num_exts - 1;
    else if (start == 0)
	start = 1;

    if (end == 0)
	end = _num_exts - 1;
    else if (!is_valid_store(end))
	end = _num_exts - 1;
    
    stnode_i si(_spid);
    for (int i = start; i <= end; i++)  {
	const stnode_t	*_stnode;
	W_DO(si.get(i, _stnode));
	const stnode_t &stnode = *_stnode;
	W_FORM(o)("stnode_t(%5d) = {head=%-5d eff=%3d%%", i, stnode.head, stnode.eff);
	o << " deleting=" << (store_deleting_t)stnode.deleting << '=' << stnode.deleting
	     << " flags=" << (store_flag_t)stnode.flags << '=' << stnode.flags << "}\n";
    }

    return RCOK;
}


/*********************************************************************
 *
 *  vol_t::find_free_store(snum)
 *
 *  Find an unused store and return it in "snum".
 *
 *********************************************************************/
rc_t
vol_t::find_free_store(snum_t& snum)
{
    snum = 0;
    stnode_i st(_spid);

    stid_t stid;
    stid.vol = _vid;
    stid.store = 0;
    
    /* lock the volume in IX and wait.  do this so that if the
     * volume is locked in EX by another xct it will block here
     * instead of trying all the stores, returning immediately
     * (since the volume is locked) and then returning OUTOFSPACE
     */
    W_DO( io_lock_force(_vid, IX, t_long, WAIT_SPECIFIED_BY_XCT) );

    for (uint i = 1; i < _num_exts; i++)  {
	const stnode_t *_stnode;
	W_DO(st.get(i, _stnode));
	const stnode_t	&stnode = *_stnode;
	if (stnode.head == 0) {
	    stid.store = i;
	    /* 
	     * Lock the store that we're allocating
	     * If we can't do so immediately, we keep looking.
	     *
	     * Locks "reserve" stores, so 
	     * force is necessary -- see comments in io_lock_force.
	     */
	    w_rc_t rc = lm->lock_force(stid, EX, t_long, WAIT_IMMEDIATE);
	    if (rc)  {
		continue;
	    }
	    snum = i;
	    return RCOK;
	}
    }

    W_RETURN_RC_MSG(eOUTOFSPACE, << "volume id = " << _vid);
}




/*********************************************************************
 *
 *  vol_t::set_store_flags(snum, flags, sync_volume)
 *
 *  Set the store flag to "flags".  sync the volume if sync_volume is
 *  true and flags is regular.
 *
 *********************************************************************/
rc_t
vol_t::set_store_flags(snum_t snum, store_flag_t flags, bool sync_volume)
{
    w_assert3(flags & st_regular
	   || flags & st_tmp
	   || flags & st_insert_file);

    if (snum == 0 || !is_valid_store(snum))    {
	DBG(<<"set_store_flags: BADSTID");
	return RC(eBADSTID);
    }

    store_operation_param param(snum, t_set_store_flags, flags);
    W_DO( store_operation(param) );

    if (flags & st_regular)  {

	/* BUGBUG: (performance) The proper thing to do here
	 * is to set the store flags on the clean & dirty pages,
	 * write the dirty ones, keep the clean ones, and don't 
	 * discard them from the BP.  Until we do that, we must
	 * discard them because their store flags are wrong and
	 * are corrected only on read.
	 */
	W_DO( bf->force_store(stid_t(vid(), snum), true) ); // discard

        if (sync_volume )  {
	    W_DO( sync() );
	}
    }

    return RCOK;
}

    
/*********************************************************************
 *
 *  vol_t::get_store_flags(snum, flags)
 *
 *  Return the store flags for "snum" in "flags".
 *
 *********************************************************************/
rc_t
vol_t::get_store_flags(snum_t snum, store_flag_t& flags)
{
    if (!is_valid_store(snum))    {
	DBG(<<"get_store_flags: BADSTID");
	return RC(eBADSTID);
    }

    if (snum == 0)  {
	flags = smlevel_0::st_bad;
	return RCOK;
    }

    stnode_i st(_spid);
    stnode_t stnode;
    W_DO(st.get(snum, stnode));

    /*
     *  Make sure the store for this page is marked as allocated.
     *  However, this is not necessarily true during recovery-redo
     *  since it depends on the order pages made it to disk before
     *  a crash.
     */
    if (!stnode.head && !in_recovery()) {
	DBG(<<"get_store_flags: BADSTID");
	return RC(eBADSTID);
    }

    flags = (store_flag_t)stnode.flags;

    return RCOK;
}



/*********************************************************************
 *
 *  vol_t::alloc_store(snum, eff, flags)
 *
 *  Allocate a store at "snum" with attributes "eff" and "flags".
 *
 *********************************************************************/
rc_t
vol_t::alloc_store(snum_t snum, int eff, store_flag_t flags)
{
    w_assert3(flags & st_regular
	   || flags & st_tmp
	   || flags & st_insert_file);

    if (!is_valid_store(snum))    {
	DBG(<<"alloc_store: BADSTID");
	return RC(eBADSTID);
    }

    if (eff < 20 || eff > 100)
	eff = 100;
    
    store_operation_param param(snum, t_create_store, flags, eff);
    W_DO( store_operation(param) );

    return RCOK;
}



/*********************************************************************
 *
 *  vol_t::set_store_first_ext(snum, head)
 *
 *  Set the first extent to store "snum" to "head".
 *
 *********************************************************************/
rc_t
vol_t::set_store_first_ext(snum_t snum, extnum_t head)
{
    if (!is_valid_store(snum))    {
	DBG(<<"set_store_first_ext: BADSTID");
	return RC(eBADSTID);
    }

    store_operation_param param(snum, t_set_first_ext, head);
    W_DO( store_operation(param) );

    return RCOK;
}



/*********************************************************************
 *
 *  vol_t::free_store(snum, acquire_lock)
 *
 *  Free the store at "snum".  acquire_lock should always be true
 *  except during shutdown when called from destroy_temps, when a
 *  prepared xct might have a share lock on the store (it's still
 *  valid to destroy it).
 *
 *********************************************************************/
rc_t
vol_t::free_store(snum_t snum, bool acquire_lock)
{
    stnode_i st(_spid);
    stnode_t stnode;
    W_DO(st.get(snum, stnode));

    if (stnode.head) {
	w_assert3(!stnode.deleting);

	stid_t stid;
	stid.vol = _vid;
	stid.store = snum;

	if (acquire_lock)  {
	    //jk probably don't need lock_force, but it doesn't hurt, too much
	    W_COERCE( lm->lock_force(stid, EX, t_long, WAIT_IMMEDIATE) );
	}
#ifdef W_DEBUG
	else {
	    lockid_t lockid(stid);
	    lock_mode_t m = NL;
	    W_COERCE( lm->query(lockid, m) );
	    w_assert3(m != EX && m != IX && m != SIX);
	}
#endif /* W_DEBUG */

	store_operation_param param(snum, t_set_deleting, t_deleting_store);
	W_DO( st.store_operation(param) );

	xct_t*	xd = xct();
	w_assert3(xd);

	xd->AddStoreToFree(stid);

	SSMTEST("store.1");
    }
    return RCOK;
}



/*********************************************************************
 *
 * vol_t::free_store_after_xct(snum)
 *
 * removes the stores which were marked for deletion by the xct.
 * this code runs after the xct is completed and the deletion of
 * the store must succeed here or in recover.  not redoable.
 *
 *********************************************************************/

rc_t
vol_t::free_store_after_xct(snum_t snum)
{
    extnum_t		head = 0;

    {
	stnode_i	st(_spid);
	stnode_t	stnode;
	W_DO(st.get(snum, stnode));

	/*
	 * check to seeing deleting is actually set, if not then a partial rollback
	 * could have reset the bit, but the store is not removed from the list of
	 * stores to check.  instead the check below weeds these out.
	 */
	if (stnode.deleting == t_not_deleting_store)
	    return RCOK;
	
	head = stnode.head;
	w_assert3(head);

	/*
	 * mark the store as freeing extents all of these must be fully released
	 * during restart before undo otherwise the next fields could be modified.
	 */
	store_operation_param param(snum, t_set_deleting, t_store_freeing_exts);
	W_DO( st.store_operation(param) );
    }
    // the store page should now be unlocked

    W_DO( _free_ext_list(head, snum) );

    store_operation_param param(snum, t_delete_store);
    W_DO( store_operation(param) );

    return RCOK;
}



/*********************************************************************
 *
 * pick_ei(ext, exts, extlinks, num_ext_pages, ei)
 *
 * returns a pointer to the extlink_i which has the page pinned for
 * ext.  exts is an array of num_ext_pages for which the corresponding
 * extlinks array has the page pinned.  used to map the ext to the
 * correct extlink_i.
 *
 *********************************************************************/

static extlink_i*
pick_ei(extnum_t ext, extnum_t* exts, extlink_i** extlinks, extnum_t num_ext_pages, extlink_i& ei)
{
    if (ext == 0)
	return 0;
    
    for (extnum_t i = 0; i < num_ext_pages; i++)  {
	if (ei.on_same_page(ext, exts[i]))  {
	    return extlinks[i];
	}
    }
    w_assert1(0);
    return 0;
}
    
    

/*********************************************************************
 *
 * vol_t::free_ext_after_xct(ext, sum)
 *
 * frees the ext from the store.  only called after an xct is complete
 * or during recovery.
 *
 * This is called by the lock mgr when locks are freed.  The idea
 * is this: where 2 or more xcts are using pages that get freed,
 * the last one to commit/abort (i.e., free its locks) is the one
 * that determines whether the page really gets freed or not. 
 *
 * Also called during recovery when the extents are searched for ones
 * that can be freed.
 *
 * If freed, incr the sum 
 *
 *********************************************************************/

rc_t
vol_t::free_ext_after_xct(extnum_t ext, snum_t& old_owner)
{
    FUNC(vol_t::free_ext_after_xct)
    w_assert3(ext);

    extnum_t	next_ext = 0;
    extnum_t	prev_ext = 0;
    extlink_t	link;

    {
	extlink_i	ei(_epid);

	W_DO( ei.get_copy(ext, link) );
	if ( link.owner == 0) {
	    DBG(<<"ext " << ext << " already freed");
	    return RCOK;
	}
	if (link.num_set() != 0) {
	    DBG(<<"ext " << ext << " not really freeable");
	    return RCOK;
	}
	if (link.prev == 0)  {
	    stnode_i	st(_spid);
	    stnode_t	stnode;
	    W_DO(st.get(link.owner, stnode));

	    if (stnode.head == ext) {
		DBG(<<"ext " << ext << " first in store -- not freed");
		return RCOK;
	    }
	}
        DBGTHRD( << "freeing extent " << ext 
		<< " from store " << link.owner 
		<< "(prev=" << link.prev 
		<< ", next=" << link.next << ")" );

	// this ext meets the criteria for being freed
	next_ext = link.next;
	prev_ext = link.prev;
	old_owner = link.owner;

#ifdef W_DEBUG
	extid_t		extid;
	extid.vol = _vid;
	extid.ext = ext;
	W_COERCE( lm->lock(extid, EX, t_long, WAIT_IMMEDIATE) );
	w_assert3(link.num_set() == 0);
#endif /* W_DEBUG */
    }

    if (ext < _min_free_ext_num) {
	_min_free_ext_num = ext;
	DBG(<<"_min_free_ext_num=" << _min_free_ext_num);
    }

    while (1)  {
	{
	    extlink_i	ei1(_epid);
	    extlink_i	ei2(_epid);
	    extlink_i	ei3(_epid);

	    extlink_i*  ei_p = 0;
	    extlink_i*  prev_ei_p = 0;
	    extlink_i*  next_ei_p = 0;

	    extnum_t	exts[3] = {prev_ext, ext, next_ext};
	    extlink_i*	extlinks[3] = {&ei1, &ei2, &ei3};

	    /* sort exts */
	    int i;
	    for (i = 0; i < 2; i++)  {
		for (int j = i+1; j <= 2; j++)  {
		    if (exts[i] > exts[j])  {
			extnum_t t = exts[i];
			exts[i] = exts[j];
			exts[j] = t;
		    }
		}
	    }

	    /* remove ext 0's */
	    int num_ext_pages = 0;
	    for (i = 0; i <= 2; i++)  {
		if (exts[i])  {
		    exts[num_ext_pages++] = exts[i];
		}
	    }
	    w_assert3(num_ext_pages > 0);

	    /* remove exts which map to the same ext page */
	    int num_unique_ext_pages = 1;
	    for (i = 1; i < num_ext_pages; i++)  {
		if (!ei1.on_same_page(exts[num_unique_ext_pages - 1], exts[i]))  {
		    exts[num_unique_ext_pages++] = exts[i];
		}
	    }

	    /* fix the extent pages in ascending order */
	    for (i = 0; i < num_unique_ext_pages; i++)  {
		W_DO( extlinks[i]->fix_EX(exts[i]) );
	    }

	    /* associate the extent pages with the extents */
	    ei_p = pick_ei(ext, exts, extlinks, num_unique_ext_pages, ei1);
	    w_assert3(ei_p);
	    next_ei_p = pick_ei(next_ext, exts, extlinks, num_unique_ext_pages, ei1);
	    prev_ei_p = pick_ei(prev_ext, exts, extlinks, num_unique_ext_pages, ei1);

	    /* free the extent if the prev and next haven't changed */
	    W_DO( ei_p->get_copy(ext, link) );
	    if (link.next == next_ext && link.prev == prev_ext)  {
		//jk remove this when put's are removed to an "allocating" log record.
		xct_log_switch_t toggle(smlevel_0::ON);

		lsn_t anchor;
		xct_t* xd = xct();
		w_assert3(xd);
		anchor = xd->anchor();

		link.zero();
		link.owner = 0;
		link.next = 0;
		link.prev = 0;
		X_DO( ei_p->put(ext, link), anchor );

		if (prev_ext)  {
		    X_DO( prev_ei_p->get_copy(prev_ext, link), anchor );
		    w_assert1(link.next == ext);
		    // link.next might not equal ext if a crash occured between
		    // writing a create_ext_list and the set_ext_next.
		    if (link.next == ext)  {
			link.next = next_ext;
			X_DO( prev_ei_p->put(prev_ext, link), anchor );
		    }
		}

		if (next_ext)  {
		    X_DO( next_ei_p->get_copy(next_ext, link), anchor );
		    w_assert1(link.prev == ext);
		    link.prev = prev_ext;
		    X_DO( next_ei_p->put(next_ext, link), anchor );
		}

		xd->compensate(anchor, false);

		return RCOK;
	    }  else  {
		/* some other thread modified the next or prev field while we were
		 * fixing things in the correct order.  retry. */
		next_ext = link.next;
		prev_ext = link.prev;
	    }

	    /* ei1, ei2, ei3 are unfixed by leaving this scope */
	}
    }
    
    return RCOK;
}



/*********************************************************************
 *
 * vol_t::_free_ext_list(ext, snum)
 *
 * free the list of exts from the store snum.  this is done by locking
 * all the extents in EX mode to reserve the extent from being reused
 * before all the extents are released.  this is done so that on
 * recover the next links can still be followed to complete the
 * freeing.  then consecutive exts on the same extlink_i page are
 * released at a time until all the exts are free.
 *
 *********************************************************************/

rc_t
vol_t::_free_ext_list(extnum_t ext, snum_t snum)
{
    DBG(<<"_free_ext_list ( ext=" << ext << " snum=" <<snum <<")");

    w_assert1(ext > 0);
    w_assert1(snum > 0);

    extlink_i	ei(_epid);
    extnum_t	count = 0;
    extnum_t	head = ext;

    extid_t	extid;
    extid.vol = _vid;

    extlink_t	link;

    /*
     * make a list of all the consecutive exts on the same extlink_p
     * and then free all at once.  the lock serves to reserve the extent
     * until all of the store's extents are freed
     */
    while (ext)  {
	extid.ext = ext;
	W_DO( ei.get_copy(ext, link) );

	w_assert3(link.owner == snum);

	W_DO( lm->lock(extid, EX, t_long, WAIT_IMMEDIATE) );

	count++;

	if (ext < _min_free_ext_num) {
	    _min_free_ext_num = ext;
	    DBG(<<"_min_free_ext_num=" << _min_free_ext_num);
	}

	if (!link.next || !ei.on_same_page(ext, link.next))  {
	    W_DO( free_exts_on_same_page(head, snum, count) );
	    count = 0;
	    head = link.next;
	}

	ext = link.next;
    }

    return RCOK;
}



/*********************************************************************
 *
 * vol_t::free_exts_on_same_page(head, snum, count)
 *
 * called through io_m during crash recovery; called
 * by _free_ext_list() in forward processing (,which is called
 * from free_ext_after_xct(), which frees all the extents that
 * are freeable after a set of xcts commit or abort.  (Triggered
 * by release of locks)).
 *
 * frees all the exts that are linked to head and  are on the same
 * extlink_i page.  count and snum are used for consistency checks
 * only.
 *
 *********************************************************************/

rc_t
vol_t::free_exts_on_same_page(extnum_t head, snum_t snum, extnum_t count)
{
    DBG(<< "free_exts_on_same_page  head="  << head
	<< " snum=" << snum
	<< " count=" << count);
    extlink_i	ei(_epid);
    extlink_t	link;
    extnum_t	myCount = 0;	// number of exts this routine frees
    extnum_t	ext = head;

    stid_t	stid;
    stid.vol = _vid;
    stid.store = snum;

    {
	xct_log_switch_t toggle(smlevel_0::OFF);

	while (ext)  {
	    myCount++;

	    W_DO( ei.get_copy(ext, link) );
	    w_assert3(link.owner == snum);

	    link.owner = 0;
	    link.prev = 0;
	    link.zero();
	    DBG(<<" freed ext " << ext);
	    W_DO( ei.put(ext, link) );

	    if (ext < _min_free_ext_num) {
		_min_free_ext_num = ext;
		DBG(<<"_min_free_ext_num=" << _min_free_ext_num);
	    }
	    DBG(<<"link.next=" << link.next
		<< " ext= " << ext);

	    if (!link.next || !ei.on_same_page(ext, link.next))
		break;

	    ext = link.next;
	}
    }

    w_assert1(myCount == count);

    W_DO( log_free_ext_list(ei.page(), stid, head, count) );

    return RCOK;
}



/*********************************************************************
 *
 * vol_t::set_ext_next(ext)
 *
 * sets the next field the extent ext.
 * called only during recovery.
 *
 *********************************************************************/

rc_t
vol_t::set_ext_next(extnum_t ext, extnum_t new_next)
{
    w_assert3(in_recovery());

    extlink_i	ei(_epid);
    W_DO( ei.set_next(ext, new_next) );
    return RCOK;
}



/*********************************************************************
 *
 * vol_t::_append_ext_list(snum, prev, count, list)
 *
 * the count elements of list are appended to the store snum which
 * has the last extent of prev.  the extents are allocated from the
 * end of the list to the beginning with all the extents on the same
 * ext_link page being allocated at the same time (also changes the
 * prev's next it is on the same page).  then the prev's next is
 * set to the first element of the list if it hasn't been set by the
 * create_ext_list_on_same_page call.
 *
 *********************************************************************/

rc_t
vol_t::_append_ext_list(snum_t snum, extnum_t prev, extnum_t count, const extnum_t* list)
{
    extlink_i	ei(_epid);
    extlink_i	prev_ei(_epid);
    extlink_t	prev_link;
    extnum_t	next = 0;

    w_assert1(count > 0);

    extnum_t	num_on_cur_page = 1;
    extnum_t	first_ext_on_page = count;
    while (first_ext_on_page--)  {
	if (first_ext_on_page == 0 || 
		!ei.on_same_page(list[first_ext_on_page], 
			list[first_ext_on_page - 1]))  {
	    if (first_ext_on_page == 0 && prev != 0)  {
		while (1)  {
		    if (prev < list[0])  {
			W_DO( prev_ei.fix_EX(prev) );
			W_DO( ei.fix_EX(list[0]) );
		    }  else  {
			W_DO( ei.fix_EX(list[0]) );
			W_DO( prev_ei.fix_EX(prev) );
		    }

		    W_DO(prev_ei.get_copy(prev, prev_link));
		    if (prev_link.next == 0)  {
			break;
		    }  else  {
			ei.unfix();
			while (prev_link.next != 0)  {
			    prev = prev_link.next;
			    W_DO(prev_ei.get_copy(prev, prev_link));
			}
			prev_ei.unfix();
		    }
		}
	    }

	    W_DO( create_ext_list_on_same_page(snum, first_ext_on_page == 0 ? prev : list[first_ext_on_page - 1],
						next, num_on_cur_page, &list[first_ext_on_page]) );

	    if (first_ext_on_page == 0 && prev != 0 && !ei.on_same_page(prev, list[0]))  {
		/*
		 * if prev and the list[0] are on the same extlink_p page,
		 * then create_ext_list_on_same_page performs this operation
		 */
#ifdef W_DEBUG
		extlink_t link;
		W_DO( prev_ei.get_copy(prev, link) );
		w_assert3( link.next == 0 );
#endif /* W_DEBUG */
		W_DO( prev_ei.set_next(prev, list[0]) );
	    }

	    num_on_cur_page = 1;
	    next = list[first_ext_on_page];
	}  else  {
	    num_on_cur_page++;
	}
    }
    return RCOK;
}



/*********************************************************************
 *
 * vol_t::create_ext_list_on_same_page(snum, prev, next, count, list)
 *
 * allocate the count extents in the list to the store snum and set
 * the list's first's prev to prev and the list's last's next to next.
 *
 *********************************************************************/

rc_t
vol_t::create_ext_list_on_same_page(snum_t snum, extnum_t prev, extnum_t next, extnum_t count, const extnum_t* list)
{
    extlink_i	ei(_epid);
    extlink_t	link;

    stid_t	stid;
    stid.vol = _vid;
    stid.store = snum;

    {
	xct_log_switch_t toggle(smlevel_0::OFF);

	for (extnum_t i = 0; i < count; i++)  {
	    W_DO( ei.get_copy(list[i], link) );

	    w_assert3(link.owner == 0);
	    w_assert3(link.prev == 0);
	    w_assert3(link.num_set() == 0);

	    link.owner = snum;
	    link.prev = (i == 0) ? prev : list[i - 1];
	    link.next = (i == count - 1) ? next : list[i + 1];
	    link.zero();

	    W_DO( ei.put(list[i], link) );
	}

	if (prev && ei.on_same_page(prev, list[0]))  {
#ifdef W_DEBUG
	    extlink_t prev_link;
	    W_DO( ei.get_copy(prev, prev_link) );
	    w_assert3( prev_link.next == 0 );
#endif /* W_DEBUG */
	    W_DO( ei.set_next(prev, list[0], false /*dont log*/) );
	}
    }

    W_DO( log_create_ext_list(ei.page(), stid, prev, next, count, list) );

    return RCOK;
}


/*********************************************************************
 *
 *  vol_t::max_store_id_in_use(snum)
 *
 *  Returns the last store which is in use in the volume.
 *
 *********************************************************************/
snum_t
vol_t::max_store_id_in_use() const
{
    snum_t snum = _num_exts;

    stnode_i st(_spid);
    while (--snum > 0)  {
	const stnode_t *_stnode;
	W_COERCE(st.get(snum, _stnode));
	const stnode_t &stnode = *_stnode;
	if (stnode.head != 0)  {
	    break;
	}
    }
    return snum;
}


/*********************************************************************
 *
 *  vol_t::get_volume_meta_stats(volume_stats)
 *
 *  Collects simple space utilization statistics on the volume.
 *  Includes number of pages, number of pages reserved by stores,
 *  number of pages allocated to stores, number of available stores,
 *  number of stores in use.
 *
 *********************************************************************/
rc_t
vol_t::get_volume_meta_stats(SmVolumeMetaStats& volume_stats)
{
    volume_stats.numStores = _num_exts;

    {
	stnode_i st(_spid);
	for (snum_t snum = 1; snum < _num_exts; ++snum)  {
	    const stnode_t *_stnode;
	    W_DO(st.get(snum, _stnode));
	    const stnode_t &stnode = *_stnode;
	    if (stnode.head != 0)  {
		++volume_stats.numAllocStores;
	    }
	}
    } // unpins stnode_p

    volume_stats.numPages = _num_exts * ext_sz;
    volume_stats.numSystemPages = _hdr_exts * ext_sz;

    {
	extlink_i ei(_epid);
	const extlink_t* link;
	for (extnum_t extnum = 1; extnum < _num_exts; ++extnum)  {
	    W_DO( ei.get(extnum, link) );
	    if (link->owner != 0)  {
		volume_stats.IncrementPages(ext_sz, link->num_set());
	    }
	}
    } // unpins extlink_p

    return RCOK;
}


/*********************************************************************
 *
 *  vol_t::get_file_meta_stats(num_files, file_stats)
 *
 *  Collects simple individual statistics on the stores which are part
 *  of the list of files requested.  Includes number of pages reserved
 *  by the stores and the number of pages allocated to the stores.
 *
 *  This method looks up the statistics on a one by one basis.
 *
 *********************************************************************/
rc_t
vol_t::get_file_meta_stats(uint4_t num_files, SmFileMetaStats* file_stats)
{
    for (uint4_t i = 0; i < num_files; ++i)  {
	W_DO( get_store_meta_stats(file_stats[i].smallSnum, file_stats[i].small) );
	if (file_stats[i].largeSnum)  {
	    W_DO( get_store_meta_stats(file_stats[i].largeSnum, file_stats[i].large) );
	}
    }
    return RCOK;
}


/*********************************************************************
 *
 *  vol_t::get_file_meta_stats_batch(max_store, mapping)
 *
 *  Collects simple individual statistics on the stores which are part
 *  of the list of files requested.  Includes number of pages reserved
 *  by the stores and the number of pages allocated to the stores.
 *
 *  This method makes one pass over the extent information only to
 *  calculate the stores in the mapping.
 *
 *********************************************************************/
rc_t
vol_t::get_file_meta_stats_batch(uint4_t max_store, SmStoreMetaStats** mapping)
{
    extlink_i ei(_epid);
    const extlink_t* link;

    for (extnum_t extnum = 1; extnum < _num_exts; ++extnum)  {
	W_DO( ei.get(extnum, link) );
	if (link->owner != 0)  {
	    if (link->owner < max_store && mapping[link->owner])  {
		mapping[link->owner]->IncrementPages(ext_sz, link->num_set());
	    }
	}
    }
    return RCOK;
}


/*********************************************************************
 *
 *  vol_t::get_store_meta_stats(snum, storeStats)
 *
 *  Collects simple statistics on the store requested.  Includes number
 *  of pages reserved by the stores and the number of pages allocated
 *  to the stores.
 *
 *********************************************************************/
rc_t
vol_t::get_store_meta_stats(snum_t snum, SmStoreMetaStats& store_stats)
{
    extnum_t extnum = 0;

    // find the first extent in the store
    {
	stnode_i st(_spid);
	const stnode_t *_stnode;
	W_DO(st.get(snum, _stnode));
	const stnode_t &stnode = *_stnode;
	if (stnode.head == 0)  {
	    DBG(<<"get_store_meta_stats: BADSTID");
	    return RC(eBADSTID);
	}
	extnum = stnode.head;
    } // unpins stnode_p

    // now check all the extents of the store
    {
	extlink_i ei(_epid);
	const extlink_t* link;
	while (extnum != 0)  {
	    W_DO( ei.get(extnum, link) );
	    if (link->owner != snum )  {
		return RC(eRETRY);
	    }
	    store_stats.IncrementPages(ext_sz, link->num_set());
	    extnum = link->next;
	}
    } // unpins extlink_p

    return RCOK;
}


/*********************************************************************
 *
 *  vol_t::first_page(snum, pid, allocated)
 *
 *  Return the first allocated pid of "snum" in "pid" if "allocated"
 *  is NULL. Otherwise, return the first pid of "snum" regardless
 *  of its allocation status, and return the allocation status
 *  in "allocated".
 *
 *********************************************************************/
rc_t
vol_t::first_page(snum_t snum, lpid_t& pid, bool* allocated)
{
    pid = pid.null;

    stnode_t stnode;
    {
	stnode_i st(_spid);
	W_DO(st.get(snum, stnode));
    }

    if (!stnode.head) {
	DBG(<<"first_page: BADSTID");
	return RC(eBADSTID);
    }

    pid._stid = stid_t(_vid, snum);

    extlink_i ei(_epid);
    extnum_t ext = stnode.head;
    const extlink_t* link;
    int first = -1;

    while ( (first < 0) && (ext != 0) )  {
	W_DO(ei.get(ext, link));
	if (allocated) {
	    // we care about unallocated pages as well, 
	    // so pick first page of extent
	    first = 0;
	    pid.page = ext * ext_sz /* + first */;
	    *allocated = link->is_set(first);
	    return RCOK;
	} else {
	    // only return allocated pages
	    // first < 0 of none allocated in this extent
	    first = link->first_set(0);
	    if (first >= 0)  {
		pid.page = ext * ext_sz + first;
		return RCOK;
	    }
	}
	ext = link->next;
    }
    return RC(eEOF);
}




/*********************************************************************
 *
 *  vol_t::last_page(snum, pid, allocated)
 *
 *  Return the last allocated page of "snum" in "pid" if
 *  allocated is NULL. Otherwise, return the last page 
 *  of "snum" regardless of its allocation status, and
 *  return the allocation status in "allocated".
 *
 *********************************************************************/
rc_t
vol_t::last_page(snum_t snum, lpid_t& pid, bool* allocated)
{
    pid = pid.null;
    stnode_t stnode;
    {
	stnode_i st(_spid);
	W_DO(st.get(snum, stnode));
    }

    if ( ! stnode.head) {
	DBG(<<"last_page: BADSTID");
	return RC(eBADSTID);
    }

    extlink_i ei(_epid);
    shpid_t page = 0;
    extnum_t ext = stnode.head;
    const extlink_t* linkp;
    while (ext)  {
	/*
	 * this is a very inefficient implementation. should 
	 * scan to the last extent and find the last page in there.
	 * if the last extent is empty, deallocate and retry.
	 */
	W_DO(ei.get(ext, linkp));
	if(allocated) {
	    // return the last page and its allocation status
	    const int i = ext_sz-1;
	    w_assert3(i>=0);
	    page = ext * ext_sz + i;
	    *allocated = linkp->is_set(i);
	} else {
	    // return last allocated page, if there is one
	    int i = linkp->last_set(ext_sz - 1);
	    if(i >= 0) page = ext * ext_sz + i;
	}
	ext = linkp->next;
    }
    if (!page) return RC(eEOF);

    pid._stid = stid_t(_vid, snum);
    pid.page = page;

    return RCOK;
}




/*********************************************************************
 *
 *  vol_t::num_pages(snum, cnt)
 *
 *  Compute and return the number of allocated pages of store "snum"
 *  in "cnt".
 *
 *********************************************************************/
rc_t
vol_t::num_pages(snum_t snum, uint4_t& cnt)
{
    cnt = 0;
    stnode_t stnode;
    {
	stnode_i st(_spid);
	W_DO(st.get(snum, stnode));
    }

    if ( ! stnode.head) {
	DBG(<<"num_pages: BADSTID");
	return RC(eBADSTID);
    }

    extlink_i ei(_epid);
    extnum_t ext = stnode.head;
    const extlink_t* linkp;
    while (ext)  {
	W_DO(ei.get(ext, linkp));
	cnt += linkp->num_set();
	ext = linkp->next;
    }

    return RCOK;
}



/*********************************************************************
 *
 *  vol_t::num_exts(snum, cnt)
 *
 *  Compute and return the number of allocated extents of
 *  store "snum" in "cnt".
 *
 *********************************************************************/
rc_t
vol_t::num_exts(snum_t snum, extnum_t& cnt)
{
    cnt = 0;
    stnode_t stnode;
    {
	stnode_i st(_spid);
	W_DO(st.get(snum, stnode));
    }

    if ( ! stnode.head) {
	DBG(<<"num_exts: BADSTID");
	return RC(eBADSTID);
    }

    extlink_i ei(_epid);
    extnum_t ext = stnode.head;
    const extlink_t* linkp;
    while (ext)  {
	W_DO(ei.get(ext, linkp));
	++cnt;
	ext = linkp->next;
    }

    return RCOK;
}




/*********************************************************************
 *
 *  vol_t::is_alloc_ext_of(ext, s)
 *
 *  Return true if extent "ext" is allocated to s. false otherwise.
 *
 *********************************************************************/
bool vol_t::is_alloc_ext_of(extnum_t e, snum_t s) const 
{
    w_assert3(_is_valid_ext(e));
    
    extlink_i ei(_epid);
    const extlink_t* linkp;
    W_COERCE(ei.get(e, linkp));
    return (linkp->owner == s);
}

/*********************************************************************
 *
 *  vol_t::is_alloc_ext(ext)
 *
 *  Return true if extent "ext" is allocated. false otherwise.
 *
 *********************************************************************/
bool vol_t::is_alloc_ext(extnum_t e) const
{
    w_assert3(_is_valid_ext(e));
    
    extlink_i ei(_epid);
    const extlink_t* linkp;
    W_COERCE(ei.get(e, linkp));
    return (linkp->owner != 0);
}




/*********************************************************************
 *
 *  vol_t::is_alloc_store(store)
 *
 *  Return true if the store "store" is allocated. false otherwise.
 *
 *********************************************************************/
bool vol_t::is_alloc_store(snum_t f) const
{
    stnode_i st(_spid);
    const stnode_t *stnode;
    W_COERCE(st.get(f, stnode));
    return (stnode->head != 0);
}



/*********************************************************************
 *
 *  vol_t::is_alloc_page_of(pid, snum_t s)
 *
 *  Return true if the page "pid" is allocated to s. false otherwise.
 *
 *********************************************************************/
bool vol_t::is_alloc_page_of(const lpid_t& pid, snum_t s) const
{
    extnum_t ext = pid2ext(pid);
    if(!is_alloc_ext_of(ext, s)) {
	return false;
    }

    extlink_i ei(_epid);
    const extlink_t* linkp;

    // Don't have a way to deal with errors here... BUGBUG
    W_COERCE(ei.get(ext, linkp));

    return linkp->is_set(int(pid.page - ext2pid(pid.store(), ext)));
}

/*********************************************************************
 *
 *  vol_t::is_alloc_page(pid)
 *
 *  Return true if the page "pid" is allocated. false otherwise.
 *
 *********************************************************************/
bool vol_t::is_alloc_page(const lpid_t& pid) const
{
    extnum_t ext = pid2ext(pid);
    extlink_i ei(_epid);
    const extlink_t* linkp;

    // Don't have a way to deal with errors here... BUGBUG
    W_COERCE(ei.get(ext, linkp));

    return linkp->is_set(int(pid.page - ext2pid(pid.store(), ext)));
}




/*********************************************************************
 *
 *  vol_t::read_page(pnum, page)
 *
 *  Read the page at "pnum" of the volume into the buffer "page".
 *
 *********************************************************************/
rc_t
vol_t::read_page(shpid_t pnum, page_s& page)
{
    w_assert1(pnum > 0 && pnum < (shpid_t)(_num_exts * ext_sz));
    fileoff_t offset = fileoff_t(pnum) * sizeof(page);

    smthread_t* t = me();

#ifdef PURIFY_ZERO
    /*
     * Fix a purify bug caused by purify itself!  The thread-half
     * diskrw code zeros out the I/O buffer when it is created, making
     * the I/O buffer initialized.  However, when a write into the buffer
     * pool of potentially uninitialized memory occurs (such as padding)
     * there is a purify supressed to keep the SM from being gigged
     * for the SM-using application's legitimate behavior.  However, this
     * uninitialized memory writes to a page in the buffer pool
     * colors the corresponding bytes in the buffer pool with the 
     * "uninitialized" memory color.  When a new page is read in from 
     * disk, nothing changes the color of the page back to "initialized",
     * and you suddenly see UMR or UMC errors from valid buffer pool pages.
     */
    memset(&page, '\0', sizeof(page));
#endif
	/* XXX return errors to caller */
    W_COERCE_MSG(t->pread(_unix_fd, (char *) &page, sizeof(page), offset), << "volume id=" << vid());

    /*
     *  place the vid on the page since since vid can change
     *  page.pid.vol = vid();
     *  NOTE: now done in byteswap code in io_m
     */
    /*
     * cannot check this condition ... 
     * invalid for unformatted page.
     * w_assert1(pnum == page.pid.page);
     */

    _stats.vol_reads++;
    _stats.vol_blks_read++;

    return RCOK;
}




/*********************************************************************
 *
 *  vol_t::write_page(pnum, page)
 *
 *  Write the buffer "page" to the page at "pnum" of the volume.
 *
 *********************************************************************/
rc_t
vol_t::write_page(shpid_t pnum, page_s& page)
{
    w_assert1(pnum > 0 && pnum < (shpid_t)(_num_exts * ext_sz));
    w_assert1(pnum == page.pid.page);
    fileoff_t offset = fileoff_t(pnum) * sizeof(page);

    smthread_t* t = me();

	/* XXX return errors to caller */
    W_COERCE_MSG(t->pwrite(_unix_fd, (char *) &page, sizeof(page), offset), << "volume id=" << vid());

    _stats.vol_writes++;
    _stats.vol_blks_written++;

    return RCOK;
}




/*********************************************************************
 *
 *  vol_t::write_many_pages(pnum, pages, cnt)
 *
 *  Write "cnt" buffers in "pages" to pages starting at "pnum"
 *  of the volume.
 *
 *********************************************************************/
rc_t
vol_t::write_many_pages(shpid_t pnum, page_s** pages, int cnt)
{
    w_assert1(pnum > 0 && pnum < (shpid_t)(_num_exts * ext_sz));
    fileoff_t offset = fileoff_t(pnum) * sizeof(page_s);
    int i;

    smthread_t* t = me();

    w_assert1(cnt > 0 && cnt <= max_many_pages);
    smthread_t::iovec_t iov[max_many_pages];
    for (i = 0; i < cnt; i++)  {
	iov[i].iov_base = (caddr_t) pages[i];
	iov[i].iov_len = sizeof(page_s);
	w_assert1(pnum + i == pages[i]->pid.page);
    }

	/* XXX return errors to caller */
    if (cnt == 1)
	W_COERCE_MSG(t->pwrite(_unix_fd, iov[0].iov_base, iov[0].iov_len, offset), << "volume id=" << vid());
    else {
	/* XXX if multiple write_many_pages can occur at once, there
	   needs to be a per-volumn mutex that makes the seek/writev
	   combination thread-safe. */
	W_COERCE_MSG(t->lseek(_unix_fd, offset, sthread_t::SEEK_AT_SET), << "volume id=" << vid());
	W_COERCE_MSG(t->writev(_unix_fd, iov, cnt), << "volume id=" << vid());
    }

    _stats.vol_writes++;
    _stats.vol_blks_written += cnt;

    return RCOK;
}

const char* vol_t::prolog[] = {
    "%% SHORE VOLUME VERSION ",
    "%% device quota(KB)  : ",
    "%% volume_id         : ",
    "%% ext_size          : ",
    "%% num_exts          : ",
    "%% hdr_exts          : ",
    "%% epid              : ",
    "%% spid              : ",
    "%% page_sz           : "
};

rc_t
vol_t::format_dev(
    const char* devname,
    shpid_t num_pages,
    bool force)
{
    FUNC(vol_t::format_dev);

    // WHOLE FUNCTION is a critical section
    xct_log_switch_t log_off(OFF);
    
    DBG( << "formating device " << devname);
    int flags = smthread_t::OPEN_CREATE | smthread_t::OPEN_RDWR
	    | (force ? smthread_t::OPEN_TRUNC : smthread_t::OPEN_EXCL);
    int fd;
    w_rc_t e;
    e = me()->open(devname, flags | smthread_t::OPEN_LOCAL, 0666, fd);
    if (e)
	return e;
    
    extnum_t num_exts = (num_pages - 1) / ext_sz + 1;

    volhdr_t vhdr;
    vhdr.set_format_version(volume_format_version);
    vhdr.set_device_quota_KB(num_pages*(page_sz/1024));
    vhdr.set_ext_size(0);
    vhdr.set_num_exts(num_exts);
    vhdr.set_hdr_exts(0);
    vhdr.set_epid(0);
    vhdr.set_spid(0);
    vhdr.set_page_sz(page_sz);
   
    // determine if the volume is on a raw device
    bool raw;
    rc_t rc = me()->fisraw(fd, raw);
    if (rc) {
	W_IGNORE(me()->close(fd));
	return RC_AUGMENT(rc);
    }
    rc = write_vhdr(fd, vhdr, raw);
    if (rc)  {
	W_IGNORE(me()->close(fd));
	return RC_AUGMENT(rc);
    }

    W_COERCE_MSG(me()->close(fd), << "device name=" << devname);

    return RCOK;
}



/*********************************************************************
 *
 *  vol_t::format_vol(devname, lvid, num_pages, skip_raw_init)
 *
 *  Format the volume "devname" for logical volume id "lvid" and
 *  a size of "num_pages". "Skip_raw_init" indicates whether to
 *  zero out all pages in the volume during format.
 *
 *********************************************************************/
rc_t
vol_t::format_vol(
    const char* 	devname,
    lvid_t 		lvid,
    shpid_t 		num_pages,
    bool 		skip_raw_init)
{
    FUNC(vol_t::format_vol);

    /*
     *  No log needed.
     *  WHOLE FUNCTION is a critical section
     */
    xct_log_switch_t log_off(OFF);
    
    /*
     *  Read the volume header
     */
    volhdr_t vhdr;
    W_DO(read_vhdr(devname, vhdr));
    if (vhdr.lvid() == lvid) return RC(eVOLEXISTS);
    if (vhdr.lvid() != lvid_t::null) return RC(eDEVICEVOLFULL); 

	/* XXX possible bit loss */
    extnum_t quota_pages = (extnum_t) (vhdr.device_quota_KB()/(page_sz/1024));

    if (num_pages > quota_pages) {
	return RC(eVOLTOOLARGE);
    }

    /*
     *  Determine if the volume is on a raw device
     */
    bool raw;
    rc_t rc = log_m::check_raw_device(devname, raw);
    if (rc)  {
	return RC_AUGMENT(rc);
    }


    DBG( << "formating volume " << lvid << " <"
	 << devname << ">" );
    int flags = smthread_t::OPEN_RDWR;
    if (!raw) flags |= smthread_t::OPEN_TRUNC;
    int fd;
    rc = me()->open(devname, flags | smthread_t::OPEN_LOCAL, 0666, fd);
    if (rc)
	return rc;
    
    /*
     *  Compute:
     *		num_exts: 	# extents for num_pages
     *		ext_pages:	# pages for extent info 
     *		stnode_pages:	# pages for store node info
     *		hdr_pages:	total # pages for volume header
     *				including ext_pages and stnode_pages
     *		hdr_exts:	total # exts for hdr_pages
     */
    extnum_t num_exts = (num_pages) / ext_sz;
    lpid_t pid;
    shpid_t ext_pages = (num_exts - 1) / extlink_p::max + 1;
    shpid_t stnode_pages = (num_exts - 1) / stnode_p::max + 1;
    shpid_t hdr_pages = ext_pages + stnode_pages + 1;
    extnum_t hdr_exts = (hdr_pages - 1) / ext_sz + 1;

    /*
     *  Compute:
     *		epid:		first page of ext_pages
     *		spid:		first page of stnode_pages
     */
    lpid_t epid, spid;
    epid = spid = pid;
    epid.page = 1;
    spid.page = epid.page + ext_pages;

    /*
     *  Set up the volume header
     */
    vhdr.set_format_version(volume_format_version);
    vhdr.set_lvid(lvid);
    vhdr.set_ext_size(ext_sz);
    vhdr.set_num_exts(num_exts);
    vhdr.set_hdr_exts(hdr_exts);
    vhdr.set_epid(epid.page);
    vhdr.set_spid(spid.page);
    vhdr.set_page_sz(page_sz);
   
    /*
     *  Write volume header
     */
    rc = write_vhdr(fd, vhdr, raw);
    if (rc)  {
	W_IGNORE(me()->close(fd));
	return RC_AUGMENT(rc);
    }

    /*
     *  Skip first page ... seek to first extent info page.
     */
    rc = me()->lseek(fd, sizeof(page_s), sthread_t::SEEK_AT_SET);
    if (rc) {
	W_IGNORE(me()->close(fd));
	return rc;
    }

    {
	page_s* buf = new page_s; // auto-del
	if (! buf) return RC(eOUTOFMEMORY);
	w_auto_delete_t<page_s> auto_del(buf);
#ifdef PURIFY_ZERO
	// zero out data portion of page to keep purify happy.
	memset(((char*)buf), '\0', sizeof(page_s));
#endif

	/*
	 *  Format extent link region
	 */
	{
	    DBG(<<" formatting extent region for " << num_exts << " extents");
	    extlink_p ep(buf, st_regular);
	    extnum_t i;
	    extlink_t link;
	    for (i = 0; i < num_exts; i += ep.max, ++epid.page)  {
		W_COERCE( ep.format(epid, 
				    extlink_p::t_extlink_p,
				    ep.t_virgin,  st_regular
				    ));
                // j  counts extent #s but it's really a slot id
                w_assert3(ep.max <= w_base_t::int2_max);
		slotid_t j;
		for (j = 0; extnum_t(j) < ep.max; j++)  {
		    link.zero();
		    if (extnum_t(j) + i < hdr_exts)  {
			if ((link.next = extnum_t(j) + i + 1) == hdr_exts)
			    link.next = 0;
			link.owner = 0;
			link.fill();
		    }
		    ep.put(j, link);
		}
		for (j = 0; extnum_t(j) < ep.max; j++)  {
		    link = ep.get(j);
		    w_assert1(link.owner == 0);
		    if (extnum_t(j) + i < hdr_exts) {
			w_assert1(link.next == ((extnum_t(j) + i + 1 
                                == hdr_exts) ? 
					      0 : extnum_t(j) + i + 1));
			w_assert1(link.first_clr(0) == -1);
		    } else {
			w_assert1(link.next == 0);
		    }
		}
		page_s& page = ep.persistent_part();
		w_assert3(buf == &page);

		rc = me()->write(fd, &page, sizeof(page));
		if (rc) {
		    W_IGNORE(me()->close(fd));
		    return rc;
		}
	    }
	}
	DBG(<<" done formatting extent region");

	/*
	 *  Format store node region
	 */
	{ 
	    stnode_p fp(buf, st_regular);
	    extnum_t i;
	    DBG(<<" formatting store node region for " << num_exts<< " stores");
	    for (i = 0; i < num_exts; i += fp.max, spid.page++)  {
		DBGTHRD(<<"");
		W_COERCE( fp.format(spid, 
				    stnode_p::t_stnode_p, 
				    fp.t_virgin, st_regular));
                w_assert3(fp.max <= w_base_t::int2_max);
		for (slotid_t j = 0; j < fp.max; j++)  {
		    stnode_t stnode;
		    stnode.head = 0;
		    stnode.eff = 0;
		    stnode.flags = 0;
		    stnode.deleting = 0;
		    W_DO(fp.put(j, stnode));
		}
		page_s& page = fp.persistent_part();
		rc = me()->write(fd, &page, sizeof(page));
		if (rc) {
		    W_IGNORE(me()->close(fd));
		    return rc;
		}
	    }
	}
	DBG(<<" done formatting store node region");
    }

    /*
     *  For raw devices, we must zero out all unused pages
     *  on the device.  This is needed so that the recovery algorithm
     *  can distinguish new pages from used pages.
     */
    if (raw) {
	/*
	 *  Get an extent size buffer and zero it out
	 */
	const int ext_bytes = page_sz * ext_sz;
	char* cbuf = new char[ext_bytes]; // auto-del
	w_assert1(cbuf);
	w_auto_delete_array_t<char> auto_del(cbuf);
	memset(cbuf, 0, ext_bytes);

	DBG(<<" raw device: zeroing...");

        /*
	 *  zero out bytes left on first extent
	 */
	fileoff_t curr_off=0;
	rc = me()->lseek(fd, 0L, sthread_t::SEEK_AT_CUR, curr_off);
	if (rc) {
	    W_IGNORE(me()->close(fd));
	    return rc;
	}
	int consumed = CAST(int, (curr_off % ext_bytes));
	int leftover = ext_bytes - consumed;
	w_assert3( (leftover % page_sz) == 0);
	rc = me()->write(fd, cbuf, leftover);
	if (rc) {
	    W_IGNORE(me()->close(fd));
	    return rc;
	}
	W_COERCE(me()->lseek(fd, 0L, sthread_t::SEEK_AT_CUR, curr_off));
	w_assert3( (curr_off % ext_bytes) == 0);

	/*
	 *  This is expensive, so see if we should skip it
	 */
	if (skip_raw_init) {
	    DBG( << "skipping zero-ing of raw device: " << devname );
	} else {
#ifndef DONT_TRUST_PAGE_LSN
	    DBG( << "zero-ing of raw device: " << devname << " ..." );
	    // zero out rest of extents
	    while (curr_off < 
		fileoff_t((fileoff_t(page_sz) * ext_sz) * num_exts)) {
		rc = me()->write(fd, cbuf, ext_bytes);
		if (rc) {
		    W_IGNORE(me()->close(fd));
		    return rc;
		}
		curr_off += ext_bytes;
	    }
            w_assert3(curr_off == 
		fileoff_t((fileoff_t(page_sz) * ext_sz) * num_exts));
	    DBG( << "finished zero-ing of raw device: " << devname);
#endif
    	}

    } else {
	/*
	 * Since the volume is not a raw device, seek to the last byte
	 * and write out a 0.  This way, for any page read from the
	 * volume where the page was never written, the page will be
	 * all zeros.
	 */

	fileoff_t where = fileoff_t(sizeof(page_s)) * ext_sz * num_exts - 1;

DBG(<<"format_vol: num_pages= " << num_pages);
DBG(<<"format_vol: seeking to offset " << where << " to write last page " );
	rc = me()->lseek(fd, where, sthread_t::SEEK_AT_SET);
	if (rc) {
	    W_IGNORE(me()->close(fd));
	    return rc;
	}

	rc = me()->write(fd, "", 1);
	if (rc) {
	    W_IGNORE(me()->close(fd));
	    return rc;
	}
    }

    W_COERCE(me()->close(fd));

    return RCOK;
}





/*********************************************************************
 *
 *  vol_t::write_vhdr(fd, vhdr, raw_device)
 *
 *  Write the volume header to the volume.
 *
 *********************************************************************/
rc_t
vol_t::write_vhdr(int fd, volhdr_t& vhdr, bool raw_device)
{
    /*
     *  The  volume header is written after the first 512 bytes of
     *  page 0.
     *  This is necessary for raw disk devices since disk labels
     *  are often placed on the first sector.  By not writing on
     *  the first 512bytes of the volume we avoid accidentally 
     *  corrupting the disk label.
     *  
     *  However, for debugging its nice to be able to "cat" the
     *  first few bytes (sector) of the disk (since the volume header is
     *  human-readable).  So, on volumes stored in a unix file,
     *  the volume header is replicated at the beginning of the
     *  first page.
     */
    if (raw_device) w_assert1(page_sz >= 1024);

    /*
     *  tmp holds the volume header to be written
     */
    const int tmpsz = page_sz/2;
    char* tmp = new char[tmpsz]; // auto-del
    if(!tmp) {
	return RC(eOUTOFMEMORY);
    }
    w_auto_delete_array_t<char> autodel(tmp);
    int i;
    for (i = 0; i < tmpsz; i++) tmp[i] = '\0';

    /*
     *  Open an ostream on tmp to write header bytes
     */
    ostrstream s(tmp, tmpsz);
    if (!s)  {
	    /* XXX really eCLIBRARY */
	return RC(eOS);
    }
    s.seekp(0, ios::beg);
    if (!s)  {
	return RC(eOS);
    }

    // write out the volume header
    i = 0;
    s << prolog[i++] << vhdr.format_version() << endl;
    s << prolog[i++] << vhdr.device_quota_KB() << endl;
    s << prolog[i++] << vhdr.lvid() << endl;
    s << prolog[i++] << vhdr.ext_size() << endl;
    s << prolog[i++] << vhdr.num_exts() << endl;
    s << prolog[i++] << vhdr.hdr_exts() << endl;
    s << prolog[i++] << vhdr.epid() << endl;
    s << prolog[i++] << vhdr.spid() << endl;
    s << prolog[i++] << vhdr.page_sz() << endl;
    if (!s)  {
	return RC(eOS);
    }

    if (!raw_device) {
	/*
	 *  Write a non-official copy of header at beginning of volume
	 */
	W_DO(me()->lseek(fd, 0, sthread_t::SEEK_AT_SET));
	W_DO(me()->write(fd, tmp, tmpsz));
    }

    /*
     *  write volume header in middle of page
     */
    W_DO(me()->lseek(fd, sector_size, sthread_t::SEEK_AT_SET));
    W_DO(me()->write(fd, tmp, tmpsz));

    return RCOK;
}



/*********************************************************************
 *
 *  vol_t::read_vhdr(fd, vhdr)
 *
 *  Read the volume header from the file "fd".
 *
 *********************************************************************/
rc_t
vol_t::read_vhdr(int fd, volhdr_t& vhdr)
{
    /*
     *  tmp place to hold header page (need only 2nd half)
     */
    const int tmpsz = page_sz/2;
    char* tmp = new char[tmpsz]; // auto-del
    if(!tmp) {
	return RC(eOUTOFMEMORY);
    }
    w_auto_delete_array_t<char> autodel(tmp);

    // for (int i = 0; i < tmpsz; i++) tmp[i] = '\0';
	memset(tmp, 0, tmpsz);

    /* 
     *  Read in first page of volume into tmp. 
     */

    /* Attempt to maintain the file pointer through errors;
	panic if it isn't possible.  */
    w_rc_t e;
    fileoff_t file_pos=0;
    W_DO(me()->lseek(fd, 0, sthread_t::SEEK_AT_CUR, file_pos));
    e = me()->lseek(fd, sector_size, sthread_t::SEEK_AT_SET);
    if (e) {
        W_DO_PUSH(me()->lseek(fd, file_pos, sthread_t::SEEK_AT_SET), e);
	return e;
    }
    e = me()->read(fd, tmp, tmpsz);
    W_DO_PUSH(me()->lseek(fd, file_pos, sthread_t::SEEK_AT_SET), e);
    if (e)
	return e;

    /*
     *  Read the header strings from tmp using an istream.
     */
    istrstream s(tmp, tmpsz);
    s.seekg(0, ios::beg);
    if (!s)  {
	    /* XXX c library */ 
	return RC(eOS);
    }

    /* XXX magic number should be maximum of strlens of the
       various prologs. */
    {
    char buf[80];
    uint4_t temp;
    int i = 0;
    s.read(buf, strlen(prolog[i++])) >> temp;
	vhdr.set_format_version(temp);
    s.read(buf, strlen(prolog[i++])) >> temp;
	vhdr.set_device_quota_KB(temp);

    lvid_t t;
    s.read(buf, strlen(prolog[i++])) >> t; vhdr.set_lvid(t);

    s.read(buf, strlen(prolog[i++])) >> temp; vhdr.set_ext_size(temp);
    s.read(buf, strlen(prolog[i++])) >> temp; vhdr.set_num_exts(temp);
    s.read(buf, strlen(prolog[i++])) >> temp; vhdr.set_hdr_exts(temp);
    s.read(buf, strlen(prolog[i++])) >> temp; vhdr.set_epid(temp);
    s.read(buf, strlen(prolog[i++])) >> temp; vhdr.set_spid(temp);
    s.read(buf, strlen(prolog[i++])) >> temp; vhdr.set_page_sz(temp);
    }

    if ( !s || vhdr.page_sz() != page_sz ||
	vhdr.format_version() != volume_format_version ) {

        cout << "Volume format bad" << endl;

        if (smlevel_0::log) {
	    return RC(eBADFORMAT);
	}
	/*
	 * Some users want to keep running without logging.
	 */
    }

    return RCOK;
}
    
    

/*********************************************************************
 *
 *  vol_t::read_vhdr(devname, vhdr)
 *
 *  Read the volume header for "devname" and return it in "vhdr".
 *
 *********************************************************************/
rc_t
vol_t::read_vhdr(const char* devname, volhdr_t& vhdr)
{
    w_rc_t e;
    int fd;

    e = me()->open(devname, smthread_t::OPEN_RDONLY | smthread_t::OPEN_LOCAL,
		   0, fd);
    if (e)
	return e;
    
    e = read_vhdr(fd, vhdr);

    W_IGNORE(me()->close(fd));

    if (e)  {
        W_DO_MSG(e, << "device name=" << devname);
    }

    return RCOK;
}




/*--------------------------------------------------------------*
 *  vol_t::get_du_statistics()	   DU DF
 *--------------------------------------------------------------*/
rc_t vol_t::get_du_statistics(struct volume_hdr_stats_t& stats, bool audit)
{
    volume_hdr_stats_t new_stats;
    uint4_t unalloc_ext_cnt;
    uint4_t alloc_ext_cnt;
    W_DO(num_free_exts(unalloc_ext_cnt) );
    W_DO(num_used_exts(alloc_ext_cnt) );
    new_stats.unalloc_ext_cnt = (unsigned) unalloc_ext_cnt;
    new_stats.alloc_ext_cnt = (unsigned) alloc_ext_cnt;
    new_stats.alloc_ext_cnt -= _hdr_exts;
    new_stats.hdr_ext_cnt = _hdr_exts;
    new_stats.extent_size = ext_sz;

    if (audit) {
	if (!(new_stats.alloc_ext_cnt + new_stats.hdr_ext_cnt + new_stats.unalloc_ext_cnt == _num_exts)) {
	    // return RC(fcINTERNAL);
	    W_FATAL(eINTERNAL);
	};
	W_DO(new_stats.audit());
    }
    stats.add(new_stats);
    return RCOK;
}

void			
vol_t::acquire_mutex() 
{
    w_assert1(! _mutex.is_mine());
    if(_mutex.is_locked() && ! _mutex.is_mine()) {
	INC_TSTAT(await_vol_monitor);
    }
    W_COERCE(_mutex.acquire());
}


/*********************************************************************
 *
 * vol_t::fill_histo(ext, snum)
 *
 * scan the extent list for the store snum, and
 * update the histogram for this store.  Does not lock
 * the store, so it just stops and returns an error 
 * if it encounters a bad owner along the way.
 *
 *********************************************************************/

rc_t
vol_t::init_histo(store_histo_t* h,  snum_t snum,
	pginfo_t *pages, int& numpages)
{
    w_assert1(snum > 0);

    extnum_t ext=0;
    {
	stnode_i st(_spid);
	stnode_t stnode;
	W_DO(st.get(snum, stnode));

	ext = stnode.head;
    }
    int n=0; // num pages found

    if(ext) {
	extlink_i	ei(_epid);
	extid_t		extid;
	const extlink_t*link;

	extid.vol = _vid;

	while (ext)  {
	    extid.ext = ext;
	    W_DO( ei.get(ext, link) );

	    if(link->owner != snum) {
		// actually store changed, but this is close enough
	        numpages = n;
		return  RC(ePAGECHANGED);
	    }
	    shpid_t base_page = ext * ext_sz;
	    DBG(<< "base page " << base_page);

	    /*
	     * Extract map of allocated pages, so
	     * we can skip any unallocated ones.
	     */
	    Pmap  	pagemap;
	    link->getmap(pagemap);

	    for(int i=0; i < smlevel_0::ext_sz; i++) {
		// only interested in allocated pages
		if(pagemap.is_clear(i)) continue;
		
		space_bucket_t b = link->get_page_bucket(i);
		DBG(<< "base_page + " << i << 
			    " is in bucket " << int(b));
		h->incr(b);
		if(n < numpages) {
		    // page is base_page + i
		    pages[n++].set_bucket(base_page+i, b);
		}
	    }
	    ext = link->next;
	}
    }
    numpages = n;
    return RCOK;
}

void
vol_t::vtable_collect(vtable_info_t &t)
{
    t.set_uint(vol_id_attr, vid().vol);
    t.set_uint(vol_writes_attr, _stats.vol_writes);
    t.set_uint(vol_blks_written_attr, _stats.vol_blks_written);
    t.set_uint(vol_reads_attr, _stats.vol_reads);
    t.set_uint(vol_blks_read_attr, _stats.vol_blks_read);
}

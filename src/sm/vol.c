/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */


/*
 *  $Id: vol.c,v 1.149 1996/07/09 20:41:28 nhall Exp $
 */
#define SM_SOURCE
#define VOL_C
#ifdef __GNUG__
#   pragma implementation
#endif

#define DBGTHRD(arg) DBG(<<" th."<<me()->id << " " arg)

#include <fstream.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/uio.h>
#include "sm_int.h"
#include "sm_du_stats.h"
#include "srvid_t.h"
#ifdef MULTI_SERVER
#include "cache.h"
#endif
#include <crash.h>

#ifdef __GNUG__
template class w_auto_delete_t<page_s>;
template class w_auto_delete_array_t<ext_log_info_t>;
#endif


/*********************************************************************
 *
 *  sector_size : reserved space at beginning of volume
 *
 *********************************************************************/
static const sector_size = 512; 



/*********************************************************************
 *
 *  extlink_t::extlink_t()
 *
 *  Create a zero-ed out extent link
 *
 *********************************************************************/
extlink_t::extlink_t() : next(0), prev(0), owner(0)
{
    zero();

    /*
     * make sure that the size of the filler field forces
     * correct alignment of the "next" field.  This is important
     * for initializing all of extlink_t since extlink_t is
     * copied with memcmp causing Purify headaches if
     * filler is not initialized
     */
    w_assert3(offsetof(extlink_t, next) == sizeof(pmap) + sizeof(filler));
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
 *  extlink_p::format(pid, tag, flags)
 *
 *  Format an extlink page.
 *
 *********************************************************************/
rc_t
extlink_p::format(const lpid_t& pid, tag_t tag, uint4_t flags)
{
    W_DO( page_p::format(pid, tag, flags) );
    extlink_t links[max];
    memset(links, 0, sizeof(links));
    vec_t vec;
    vec.put(links, sizeof(links));
    W_COERCE(page_p::insert_expand(0, 1, &vec) );
    return RCOK;
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
 *  stnode_p::format(pid, tag, flags)
 *
 *  Format an stnode page.
 *
 *********************************************************************/
rc_t
stnode_p::format(const lpid_t& pid, tag_t tag, uint4_t flags)
{
    W_DO( page_p::format(pid, tag, flags) );
    stnode_t stnode[max];
    memset(stnode, 0, sizeof(stnode));

    vec_t vec;
    vec.put(stnode, sizeof(stnode));
    W_COERCE(page_p::insert_expand(0, 1, &vec) );
    return RCOK;
}
MAKEPAGECODE(stnode_p, page_p);
    

ostream& operator<<(ostream &o, const extlink_t &e)
{
      o    << " num_set:" << e.num_set()
	   << " owner:" << e.owner 
      	   << " tid:" << e.tid
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
   pages are allocated, and a next pointer.

   A store is a list of extents, whose head is in the "store map",
   and whose body is the linked extlink_t structures.
   
   Each page is mapped to a known extent by simple arithmetic.
   Each extent is inserted or deleted from a store by manipulating the
   linked list (gak).

   The protocol for manipulating the extent lists is as follows:

   1) acquire the extent lock before latching the page containing the extent.
   2) latch the page
   3) read/write the page
   4) unlatch it

   EX locks on extents serve to reserve them (for the purpose of 
   serializing txs that allocate and deallocate extents).  They also serve
   to serialize access to the "next" pointers in the extlink_t data structures,
   so inserting a new extent into a list means acquiring EX locks on 2 (or 3)
   extents.

   IX locks are acquired when bits are set/clr in extents (pages allocated 
   in the extent).

   Extent locks do NOT fit into the lock granularity hierarchy with pages.
   They are anomalous.  If a volume is locked, the extent locks can be
   avoided, but since extents move around from store to store, they are
   really at the same level as stores.

#endif /*COMMENT*/

/*********************************************************************
 *
 *  class extlink_i
 *
 *  Iterator on extent link area of the volume.
 *
 *  The general scheme for managing extents is described in the comment
 *  above (COMMENT)
 *
 *********************************************************************/
class extlink_i {
public:
    NORET			extlink_i(const lpid_t& root) : _root(root) {
				DBG(<<"construct extlink_i on root page " << root);
				};
    		
    bool 			on_same_root(int idx);

    rc_t 			get(int idx, const extlink_t* &);
    rc_t 			get_copy(int idx, extlink_t &);
    rc_t 			put(int idx, const extlink_t&);
    bool                        on_same_page(extnum_t ext1, extnum_t ext1) const ;

    rc_t 			insert(extnum_t idx, snum_t s, 
					extnum_t prev, extnum_t next, 
					extlink_t *t = 0,
					bool complete = true);

    rc_t 			remove(extnum_t idx, snum_t &s,
					extnum_t &prev, extnum_t &next, 
					extlink_t *t=0,
					bool complete = true);

    void  			unfix();
    rc_t 			set_bit(int idx, int bit);
    rc_t 			clr_bit(int idx, int bit);
    const extlink_p&            page() const { return _page; } // for logging purposes
#ifdef notdef
    void                        discard(); // throw away partial updates
#endif /* notdef */
    bool                        was_dirty_on_pin() const { return _dirty_on_pin; }

private:
    bool                        _dirty_on_pin;
    extid_t			_id;
    lpid_t			_root;
    extlink_p			_page;
};


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
rc_t
extlink_i::get(int idx, const extlink_t* &res)
{
    w_assert3(idx);
    lpid_t pid = _root;
    pid.page += idx / (extlink_p::max);

    DBGTHRD(<<"extlink_i::get(" << idx <<  " )");

    bool was_already_fixed = _page.is_fixed();

    W_COERCE( _page.fix(pid, LATCH_SH) );

    if( !was_already_fixed ) {
	_dirty_on_pin = _page.is_dirty();
    }

    res = &_page.get(idx % (extlink_p::max));

    DBGTHRD(<<" get() returns " << *res  );
    w_assert3(res->next != idx); // no loops
    return RCOK;
}

rc_t
extlink_i::get_copy(int idx, extlink_t &res)
{
    const extlink_t *x;
    W_DO(get(idx, x));
    res = *x;
    w_assert3(res.next != idx); // no loops
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
extlink_i::put(int idx, const extlink_t& e)
{
    w_assert3(idx);
    lpid_t pid = _root;
    pid.page += idx / (extlink_p::max);

    DBGTHRD(<<"extlink_i::put(" << idx <<  " ) e =" << e );
    w_assert3(e.next != idx);

    W_COERCE( _page.fix(pid, LATCH_EX) );
    _page.put(idx % (extlink_p::max), e);
    _page.set_dirty();

    return RCOK;
}

/*********************************************************************
 * 
 *  extlink_i::insert(idx, snum, prev, next, linkp=0, bool complete=true)
 *  extlink_i::remove(idx, &snum, &prev, &next, linkp=0, bool complete=true)
 *
 *  Insert idx between prev, next.
 *  or
 *  Remove idx and return prev, next.
 * 
 *  If a linkp is provided, it's assumed to be a pointer to an
 *  already pinned link in *this, and the link represents extent #idx.
 *
 *  IT IS ASSUMED THAT YOU ALREADY HAVE ALL LOCKS NEEDED
 *
 *  The Boolean "complete" argument is for avoiding redundant updates
 *  of next and prev links when we are processing a group of links.
 *
 *********************************************************************/
rc_t 
extlink_i::remove(extnum_t idx, 
	snum_t   &owner,
	extnum_t &prev_extnum, 
	extnum_t &next_extnum, 
	extlink_t *linkp,  // = 0
	bool     complete // = true
	)
{
    FUNC(extlink_i::remove);

    smlevel_0::stats.ext_free++;

    extlink_t link;
    w_assert3(idx);
    DBGTHRD(<<"extlink_i::removing " << idx );

    if(!linkp) {
	linkp = &link;
	W_DO(this->get_copy(idx, link));
    }
    prev_extnum = linkp->prev;
    next_extnum = linkp->next;
    owner = linkp->owner;

    DBGTHRD(<<"extlink_i::removing "
	<< idx
	<< " from store " << owner 
    	<< " between " << prev_extnum 
    	<< " and " << next_extnum 
    	<< " update-neighbors " << complete 
	);

    /* update the subject link */

    linkp->zero(); // clear page-allocated bits
    w_assert3(linkp->num_set()==0);
    w_assert3(linkp->owner != 0);

    linkp->owner = linkp->next = linkp->prev = 0;
    w_assert3(xct());
    linkp->tid = xct()->tid();
    DBG(<<"ei.put(" << idx << ")");
    W_COERCE(put(idx, *linkp));

    /// NB: from here on, use link. not linkp-> (there could be a difference)

    if(prev_extnum) {
	/* update prev->next */
	if(complete) {
	    W_COERCE(get_copy(prev_extnum, link));
	    // make prev point directly to next
	    w_assert3(link.next == idx);
	    link.next = next_extnum;
	    W_COERCE(put(prev_extnum, link));
#ifdef DEBUG
	} else {
	    W_COERCE(get_copy(prev_extnum, link));
	    // We're removing a series of
	    // links.  Removal goes forward, so
	    // the prev one should already have
	    // been done: prev is removed or 
	    // prev is linked to next.
	    w_assert3(link.next == next_extnum
		|| link.next == 0
		);
#endif
	}
    }

    if(next_extnum) {
	/* update next->prev */
	if(complete) {
	    W_COERCE(get_copy(next_extnum, link));
	    // make next point directly to prev
	    w_assert3(link.prev == idx);
	    link.prev = prev_extnum;
	    W_COERCE(put(next_extnum, link));
#ifdef DEBUG
	} else {
	    W_COERCE(get_copy(next_extnum, link));
	    // We're removing a series of
	    // links.  Removal goes forward, so
	    // the next one should not have
	    // been done.
	    w_assert3(link.prev == idx);
#endif
	}
    }

    return RCOK;
}
// see comments above
rc_t 
extlink_i::insert(extnum_t idx,  snum_t snum,
	extnum_t prev_extnum, 
	extnum_t next_extnum, 
	extlink_t *linkp,  // = 0
	bool     complete // == true
	)
{
    FUNC(extlink_i::insert);
    extlink_t link;
    w_assert3(idx);

    smlevel_0::stats.ext_alloc++;
    DBGTHRD(<<"extlink_i::insert(" << idx 
	<<  " after " << prev_extnum 
	<< " and before " << next_extnum);

    if(!linkp) {
	linkp = &link;
	W_DO(this->get_copy(idx, link));
	// the following assert does not necessarily hold
	// if we're passing in a link
	w_assert3(linkp->num_set()==0);
    }

    /* update the subject link */

    linkp->owner = snum;
    linkp->next = next_extnum;
    linkp->prev = prev_extnum;
    w_assert3(xct());
    linkp->tid = xct()->tid();
    DBG(<<"ei.put(" << idx << ")");
    W_COERCE(put(idx, *linkp));

    /* update prev->next */
    if( prev_extnum) {
	W_COERCE(get_copy(prev_extnum, link));
	if(complete) {
	    w_assert3(link.next == next_extnum || link.next == 0);
	    link.next = idx;
	    W_COERCE(put(prev_extnum, link));
	} else {
	    // not yet done
	    w_assert3(link.next != idx);

	    // but we're about to do it on the
	    // next insert() call
	    // NB: here we are assuming that allocations
	    // are going backwards.
	}
    }

    /* update next->prev */
    if(next_extnum) {
	W_COERCE(get_copy(next_extnum, link));
	if(complete) { 
	    if (link.prev != idx) {
		// it could already point to idx, but we're
		// trying to avoid that inefficiency
		// Unfortunately, we might have called this
		// with complete== true to update ONE of the
		// two other links...

		w_assert3(link.prev == prev_extnum);
		link.prev = idx;
		W_COERCE(put(next_extnum, link));
	    }
	} else {
	    // already done-- we don't have to update it
	    // NB: here we are assuming that allocations
	    // are going backwards.
	    w_assert3(link.prev == idx);
	}
    }
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
extlink_i::set_bit(int idx, int bit)
{
    w_assert3(idx);
    uint4 poff = _root.page + idx / extlink_p::max;

    _id.vol = _root.vol();
    _id.ext = idx;
    // W_DO( io_lock_force(/*ext*/_id, IX, t_long, WAIT_SPECIFIED_BY_XCT) );

    if ((_page == 0) || (_page.pid().page != poff)) {
	lpid_t pid = _root;
	pid.page = poff;
	W_COERCE( _page.fix(pid, LATCH_EX) );
    }
    /*
     * set "ref bit" to indicate that this
     * is a hot page.  Let it sit in the buffer pool
     * for a few sweeps of the cleaner.
    _page.set_ref_bit(8);
    */
    _page.set_bit(idx % extlink_p::max, bit);
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
extlink_i::clr_bit(int idx, int bit)
{
    w_assert3(idx);

    _id.vol = _root.vol();
    _id.ext = idx;

    // don't keep it fixed while we await a lock
    _page.unfix();

    // W_DO( io_lock_force(/*ext*/_id, IX, t_long, WAIT_SPECIFIED_BY_XCT) );

    uint4 poff = _root.page + idx / extlink_p::max;
    if ((_page == 0) || (_page.pid().page != poff)) {
	lpid_t pid = _root;
	pid.page = poff;
	W_COERCE( _page.fix(pid, LATCH_EX) );

    }
    /*
     * set "ref bit" to indicate that this
     * is a hot page.  Let it sit in the buffer pool
     * for a few sweeps of the cleaner.
    _page.set_ref_bit(8);
    */
    _page.clr_bit(idx % extlink_p::max, bit);
    return RCOK;
}



/*********************************************************************
 *
 *  class stnode_i
 *
 *  Iterator over store node area of volume.
 *
 *********************************************************************/
class stnode_i {
public:
    NORET			stnode_i(const lpid_t& root) : _root(root)  {};
    const stnode_t& 		get(int idx);
    w_rc_t 			put(
	int 			    idx, 
	const stnode_t& 	    f, 
	bool 			    create = false);
private:
    lpid_t			_root;
    stnode_p			_page;
};


/*********************************************************************
 *
 *  stnode_i::get(idx)
 *
 *  Return the stnode at index "idx".
 *
 *********************************************************************/
const stnode_t&
stnode_i::get(int idx)
{
    w_assert3(idx);
    lpid_t pid = _root;
    pid.page += idx / (stnode_p::max);
    W_COERCE( _page.fix(pid, LATCH_SH) );
    return _page.get(idx % (stnode_p::max));
}



/*********************************************************************
 *
 *  stnode_i::put(idx, f, create_flag)
 *
 *  Put "f" at index "idx". If "create_flag" is true, then
 *  generate a Store Create Log.
 *
 *********************************************************************/
w_rc_t
stnode_i::put(int idx, const stnode_t& f, bool create)
{
    w_assert3(idx);
    lpid_t pid = _root;
    pid.page += idx / (stnode_p::max);
    W_COERCE( _page.fix(pid, LATCH_EX) );

    if (!create) {
	W_DO(_page.put(idx % (stnode_p::max), f));
    } else {
	/*
	 *  Turn off log to do the put() then write
	 *  a Store Create Log.
	 */
	{
	    xct_log_switch_t toggle(smlevel_0::OFF);
	    W_DO(_page.put(idx % (stnode_p::max), f));
	}
	W_DO(log_store_create(_page, idx, f.eff, f.flags));
    }
    return RCOK;
}

    
    

/*********************************************************************
 *
 *  vol_t::num_free_exts(nfree)
 *
 *  Compute and return the number of free extents in "nfree".
 *
 *********************************************************************/
rc_t
vol_t::num_free_exts(uint4& nfree)
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
vol_t::num_used_exts(uint4& nused)
{
    uint4 nfree;
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
    if (t->fsync(_unix_fd))  {
	W_FATAL(eOS);
    }
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

    if (me()->open(devname, O_RDWR, 0666, _unix_fd))  {
	_unix_fd = -1;
	return RC(eOS);
    }

    /*
     *  Read the volume header on the device
     */
    volhdr_t vhdr;
    {
	rc_t rc = read_vhdr(_devname, vhdr);
	if (rc)  {
	    W_COERCE(me()->close(_unix_fd));
	    _unix_fd = -1;
	    return RC_AUGMENT(rc);
	}
    }
    w_assert1(vhdr.ext_size == ext_sz);

    /*
     *  Save info on the device
     */
    _vid = vid;
    _lvid = vhdr.lvid;
    _num_exts = vhdr.num_exts;
    _epid = lpid_t(vid, 0, vhdr.epid);
    _spid = lpid_t(vid, 0, vhdr.spid);
    _hdr_exts = CAST(int, vhdr.hdr_exts);

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
    W_COERCE( flush ? 
	      bf->force_volume(_vid, TRUE) : 
	      bf->discard_volume(_vid) );

    /*
     *  Close the device
     */
    if (me()->close(_unix_fd)) return RC(eOS);

    _unix_fd = -1;
    
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
    ss_m::errlog->clog << info_prio << "vol_t::check_disk()\n";
    ss_m::errlog->clog << info_prio << "\tvolid      : " << vhdr.lvid << flushl;
    ss_m::errlog->clog << info_prio << "\tnum_exts   : " << vhdr.num_exts << flushl;
    ss_m::errlog->clog << info_prio << "\text_size   : " << ext_sz << flushl;

    stnode_i st(_spid);
    extlink_i ei(_epid);
    for (uint i = 1; i < _num_exts; i++)  {
	stnode_t stnode = st.get(i);
	if (stnode.head)  {
	    ss_m::errlog->clog << info_prio << "\tstore " << i << " is active: ";
	    extlink_t *link_p;
	    for (int j = stnode.head; j; ){
		ss_m::errlog->clog << info_prio << '[' << j << "] ";
		W_DO(ei.get(j, link_p));
		j = link_p->next;
	    }
	    ss_m::errlog->clog << info_prio << '.' << flushl;
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
    stnode_i st(_spid);
    const stnode_t& stnode = st.get(snum);
    result = stnode.head;

#ifdef DEBUG
    if(result) {
	extlink_i ei(_epid);
	extlink_t link;
	W_COERCE(ei.get_copy(result, link));
	w_assert3(link.prev == 0);
	w_assert3(link.owner == snum);

    } else {
	DBG(<<"Store has no extents");
    }
#endif
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
    const stnode_t& stnode = st.get(snum);
    return stnode.eff;
}


/*********************************************************************
 *
 *  vol_t::alloc_page_in_ext(ext, eff, snum, cnt, pids, allocated)
 *
 *  Attempt to allocate "cnt" pages in the extent "ext" for store
 *  "snum". The number of pages successfully allocated is returned
 *  in "allocated", and the pids allocated is returned in "pids".
 *
 *********************************************************************/
rc_t
vol_t::alloc_page_in_ext(
    extnum_t 	ext,
    int 	eff,
    snum_t 	snum,
    int 	cnt,
    lpid_t 	pids[],
    int& 	allocated,
    int&	remaining,
    bool&	is_last   // return true if ext has next==0
    )
{
    FUNC(vol_t::alloc_page_in_ext);
    /*
     *  Sanity checks
     */
    w_assert1(eff >= 0 && eff <= 100);
    w_assert1(is_valid_ext(ext));
    w_assert1(cnt > 0);

    smlevel_0::stats.alloc_page_in_ext++;

    allocated = 0;

    /*
     *  Try to lock the extent. If failed, return 0 page allocated.
     */
    {
	extid_t extid;
	extid.vol = _vid;
	extid.ext = ext;


	// 
	// Avoid acquiring lock if volume is  already exclusively
	// locked
	// 
	{ // NEH
	    lock_mode_t m;
	    W_COERCE( lm->query(extid.vol, m) ); // NEH

	    if(m != EX) { // NEH

		// If we can't acquire this instant lock,
		// it means some other tx has the extent EX-locked

		if (lm->lock_force(extid, IX, t_instant, WAIT_IMMEDIATE)) {
		    return RCOK;
		}
	    } //NEh

	}// NEh
    }
    
    /*
     *  Count number of usable pages in the extent, taking into
     *	account the extent fill factor.
     */
    extlink_i ei(_epid);
    extlink_t link;
    W_DO(ei.get_copy(ext, link)); // FIXes ext link
    if(link.owner != snum) {
	// it's no longer in the store that it was when we
	// first grabbed the extent number
	// Let the caller pick another extent.
	return RCOK;
    }
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
	int start = -1;
	for (int i = 0; i < nfree; i++, allocated++)  {

	    lock_mode_t m;
	    uint4 numsites = 0;
#ifdef MULTI_SERVER
	    cb_site_t sites[smlevel_0::max_servers];
#endif

	    /*
	     *  Find a free page that nobody has locked.
	     */
	    do {
		start = (++start >= ext_sz ? -1 : link.first_clr(start));
		if (start < 0) break;
		
		pids[i]._stid = stid_t(_vid, snum);
		pids[i].page = base + start;
		
		W_DO( lm->query(pids[i], m) );

#ifdef MULTI_SERVER
		cm->find_copies(pids[i], sites, numsites, srvid_t::null);
#endif
	    } while (m != NL || numsites != 0);

	    if (start < 0) break;
	    link.set(start);
	    W_DO(ei.set_bit(ext, start));
	}
    }
    xct()->set_alloced();
    remaining -= allocated;
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

    /* BUGBUG(NANCY) why Must use WAIT_IMMEDIATE with optimistic ? */
    W_DO(io_lock_force(pid, EX, t_long, WAIT_IMMEDIATE, TRUE/*optimistic*/));

    extlink_i ei(_epid);
    extlink_t link;
    W_DO(ei.get_copy(ext, link));
    w_assert1(link.owner == pid.store());
    w_assert1(link.is_set(offset));

    link.clr(offset);
    W_DO(ei.clr_bit(ext, offset));

    xct()->set_freed();
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
#ifdef DEBUG
    // for debugging prints
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
    if (allocated) *allocated = linkp->is_set(offset);
    
    DBG("next_page after " << save_pid << " == " << pid);
    return RCOK;
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

    /*
     *  i: # extents 
     *  j: extent offset starting from first_ext.
     */
    uint i, j;
    for (i = 0, j = (first_ext ? first_ext : _hdr_exts - 1); i < cnt; ++i) {
	// lock_mode_t m = NL;
	/*
	 *  Loop to find an extent that is both free and not locked.
	 *
	 *  Also, extents that were freed by this tx cannot be
	 *  realloced by this tx if this reallocation is inside
	 *  nested compensated operations (a top-level action inside
	 *  a top-level action).
	 *  (Remember that the extent is reserved for this tx by the
	 *  EX lock.)  But in the case that, say, we freed this extent
	 *  from a file, now we're allocating a btree page, since the
	 *  btree operation is itself a top-level action, we'll never
	 *  be able to undo the embedded extent allocation in order to
	 *  roll back the extent freeing.  Our hack around this is to
	 *  allow the re-use of the extent IFF we are not allocating
	 *  inside a top-level action. 
	 * 
	 */
	bool reuse_ok = ! xct()->in_nested_compensated_op();
	w_rc_t rc;
	extlink_t link;

// again:
	do {
	    while (++j < _num_exts)  {
		W_DO(ei.get_copy(j, link)); 
		DBG(
			<<"reuse_ok=" << reuse_ok
			<< ", ext =" << j
			<< ", owner=" << link.owner
			<< ", tid=" << link.tid
			<< ", mytid=" << xct()->tid()
		    );
		ei.unfix(); // to avoid latch-lock deadlocks

		if (link.owner == 0) {
#ifndef TID_CHECK
		    if(reuse_ok) {
			  DBG(<<"re-use ok: found");
			  break;
		    }
		    if(link.tid != xct()->tid()) {
			  DBG(<<"different tids: found");
			  break;
		    }
#else
		    // it's free and acceptable
		    DBG(<<"found " << j);
		    break;
#endif
		}
	    }
	    if (j >= _num_exts)  {
		found = i;
		return RC(eOUTOFSPACE);
	    }

	    extid.ext = j;

	    // Old way: query returned lock if *anyone* had it
	    // W_DO(lm->query(extid, m));
	    // New way:
	    // Acquire the lock. If this tx already had it, the
	    // acquire will work. This means that we can re-alloc
	    // some extents that we've dealloced in this transaction.
	    // A little earlier, (above), we selected out the
	    // cases in which it's not ok to reallocate.

	    rc = io_lock_force(extid, EX, t_long, WAIT_IMMEDIATE);
	    if(rc && (
		rc.err_num() == eMAYBLOCK || 
		rc.err_num() == eLOCKTIMEOUT || 
		rc.err_num() == eDEADLOCK
		)) {
		continue;
	    } 
	    DBG(<<"Got the lock for " << j);
	    break;

	} while (1); //  while (m == EX);

	{
	    //
	    // verify still not owned after
	    // getting the lock. This is a check for
	    // having been hit through the window between
	    // checking the owner and acquiring the lock
	    //
	    const extlink_t *lp;
	    W_DO(ei.get(j, lp));
	    w_assert1( ! lp->owner);
	}

	DBG(<<"found " << j);
	exts[i] = j;
    }
    found = i;
	    
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
 *  The protocol for allocating extents and logging their allocation
 *  goes something like this:
 *  Logically log the deallocation one-at-a-time (even though
 *  the log records have the machinery for logging several).
 *  Logicall log chunks of allocations.
 *  Physically log the updates on each page for redo purposes, and 
 *  compensate back to the logical log record for undo purposes.
 *  Hence...
 *     1) get anchor
 *     2) crab through the list of new extents, from tail to head,
 *        linking them as you go, and physically logging these
 *        updates
 *     3) write logical log record, and make it compensate back to the anchor
 *     4) unpin the last page used in the crabbing... might be the store head
 *
 *  NB: the pin/unpin are buried in the extlink_i structures' methods.
 *
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
    int 	i;
    stid_t 	stid(vid(), snum);

    extlink_i 	ei(_epid);
    extlink_t 	link;

    CRASHTEST("extent.2");

    ext_log_info_t  *bunch = new ext_log_info_t[cnt];
    w_auto_delete_array_t<ext_log_info_t> auto_del_info(bunch);

    // prev == 0 means we're allocating for a brand-new
    // store, in which case, setting the store head is done
    // elsewhere, and the whole store is locked.
    if(prev) {
	/* lock the prev extent in order to 
	 * allow us to preserve order on abort 
	 */
	extid_t extid;
	extid.vol = vid();
	extid.ext = prev;

	//
	// TODO: possible latch-lock deadlocks here.(??)
	//
	W_DO(io_lock_force(extid, EX, t_long, WAIT_SPECIFIED_BY_XCT));
	/* ok, we have the lock! */
    } 
    /* 
     * all the "next" extents are already locked: we only allocate
     * at the end of the file in forward processing (the only time
     * this function is called -- IMPORTANT!)
     * and all of these extents were locked by the function that
     * located them.
     */
	
    // Assign "next" numbers for ordering purposes ...
    for (i = 0; i < cnt; i++)  {
	bunch[i].ext = exts[i];
	bunch[i].next = (i==cnt-1) ? 0 : exts[i+1];
	bunch[i].prev = (i==0) ? prev : exts[i-1];
	// pmaps will be zero
	DBG(<<"extent " << exts[i] 
		<< " prev " << bunch[i].prev 
		<< " next " << bunch[i].next 
		);
    }

    /*
     * We have to compensate around all the individual 
     * page updates, so that the effect is that we
     * have individually- physically-logged page updates,
     * until we have completed the whole operation. 
     * 
     * We write a logical log record for undo-purposes.
     * The undo has to be logical, and compensates the
     * physical series of updates.
     */
    lsn_t anchor;
    xct_t* xd = xct();
    if(xd) {
	anchor = xct()->anchor();
    }
    w_assert3(xd); 


    /*
    // No longer a need to allocate backwards,
    // since we've now got a doubly-linked list,
    // but since we are assuming in extlink_i::insert()
    // that the case in which it doesn't have to
    // update the next and prev links, we're allocating
    // backwards, we'll leave it so
    */
    for (i = cnt - 1; i >= 0; i--)  {
	DBG(<<"allocating extent " << bunch[i].ext
		<< " to store " << snum
		<< " to be between " << bunch[i].prev
		<< " and " << bunch[i].next
		);
	W_COERCE(ei.get_copy(bunch[i].ext, link));
	w_assert1( ! link.owner);
	CRASHTEST("extent.6");
	link.zero();
	W_COERCE(ei.insert(bunch[i].ext, snum,
		bunch[i].prev, bunch[i].next, &link,
		(bool)((i==0)||(i==cnt-1))));

	CRASHTEST("extent.2");
    }

    CRASHTEST("extent.1");
    if (prev==0)  {
	DBG(<<"first extent in store : "
	    << bunch[0].ext
	    <<" after " << bunch[0].prev
	    <<" before " << bunch[0].next
	);
	w_assert3(bunch[0].prev == 0);
	w_assert3(bunch[0].ext == exts[0]);
	X_DO( this->set_store_first_ext(snum, exts[0]));
    }
#ifdef DEBUG
    else {
	// should already have been set
	X_DO(ei.get_copy(prev, link));

	w_assert3(link.owner == snum);
	w_assert3(link.next == bunch[0].ext);

	X_DO(ei.get_copy(bunch[0].ext, link));
	w_assert3(link.prev == prev);
    }
#endif

    W_COERCE(log_ext_insert(ei.page(), stid, cnt, bunch) );

    /* 
     * Compensate the physical allocation -- this is 
     * a special case of compensation.
     */
    if (xd)  {
	CRASHTEST("extent.5");
	xd->compensate(anchor, true); // undoable
    }

    // The unpins are done when the extlink_i are destroyed, if 
    // not before.

    return RCOK;
}

/*********************************************************************
 *
 *  vol_t::alloc_exts_in_order(snum, cnt, exts)
 *
 *  A slightly simpler case of alloc_exts, used only in rollback.
 *  1) no locks are acquired-- it's assumed that we already have them
 *  2) this function gets, no only the extents to be allocated,
 *     but info about where they are to be located. It allocates
 *     them in a given place, whereas alloc_exts() allocated them
 *     at the end of the store.
 *
 *  Give the store id, the previous extent number, and an array of
 *  "cnt" extents with prev & next information in "exts",
 *  hook them up to their respective "prev" and "next"s.
 *
 *  This is an important assumption (used on rollback only)--
 *  since no logical log record is written by this routine. (Compensate_
 *  undo immedidately compensates around anything we do here).
 *
 *
 *********************************************************************/
rc_t
vol_t::alloc_exts_in_order(
    snum_t 			snum,
    int 			cnt, 
    const ext_log_info_t 	exts[]
)
{
    FUNC(vol_t::alloc_exts_in_order);
    extlink_i 	ei(_epid);
    extlink_t 	link;

#ifdef DEBUG
    w_assert3(xct());
    w_assert3(me()->xct()->state() == xct_t::xct_aborting); 
#endif

    w_assert3(cnt >= 1);

    int i;
    for (i = cnt - 1; i >= 0; i--, cnt--)  {
	W_COERCE(ei.get_copy(exts[i].ext, link));
	w_assert1(! link.owner);

	link.zero();
	link.setmap(exts[i].pmap);

	// Since we have the prev & next info for a series of extents,
	// and we're linking them all together, we don't have to have
	// the ei.insert() method update the prev & next links (it would
	// be redundant) except on the first & last calls.

	W_COERCE(ei.insert(exts[i].ext, snum, exts[i].prev, exts[i].next, &link,
		(bool)((i==0)||(i==cnt-1))));

	w_assert1(i==0); // we can handle only one, for the time being .
	w_assert1(i==cnt-1); // we can handle only one, for the time being .

    }
    return RCOK;
}




/*********************************************************************
 *
 *  vol_t::free_exts(head)
 *
 *  Free the extent "head" and all extents linked to it.
 *  Does not matter if these extents have bits set-- wipe them out.
 *  Called when destroying an entire store. (Store has been EX locked)
 *
 *********************************************************************/
rc_t
vol_t::free_exts(extnum_t head)
{
    w_assert1(head > 0);
    extlink_i ei(_epid);
    snum_t	storenum=0;

    extid_t extid;
    extid.vol = _vid;
    extid.ext = head; // start with first extent in store.
    extlink_t link;

#ifdef DEBUG
    int num_logged=0;
    int num_found=0;
#endif
    /*
     * Traverse the list, building a list of 
     * extents with their prev,next info.
     * We'll try to do this in chunks for two reasons:
     * 1) to avoid alternatly pinning & unpinning multiple extent 
     *    pages when we have a large number of extents, and
     * 2) we'd like to log the deallocations 
     *    in the fewest number of log records possible.
     */
    ext_log_info_t  *bunch = new ext_log_info_t[extlink_p::max + 1];
    w_assert3(bunch);
    w_auto_delete_array_t<ext_log_info_t> auto_del_info(bunch);
    ext_log_info_t  *info = bunch;

    lsn_t anchor;
    xct_t* xd = xct();
    w_assert3(xd); 


    /*
     * we're deallocating as we go forward.
     * re-allocation (during undo) will be backward.
     */
    stid_t stid(vid(), 0);
    extnum_t nxt=0;

    /*
     * Make sure the 
     * logical log record for the removal
     * is both undoable and compensates around
     * the physical updates.
     */
    w_assert1(xd);
    if(xd) {
	anchor = xd->anchor();
    }

    while (extid.ext)  { // while there are extents left in the store
	W_COERCE(ei.get_copy(extid.ext, link));
	w_assert1(link.owner);
	stid.store = link.owner;

	nxt = link.next; // save it for a moment

	info->ext = extid.ext;
	//info.pmap = link.pmap;
	link.getmap(info->pmap);
	W_COERCE(ei.remove(extid.ext, storenum, 
		info->prev, info->next, &link, false));
		// last argument==false means avoid any redundant link updates
#ifdef DEBUG
	num_found ++;
	// assert that all the extents belong to the same store
	w_assert3(storenum == stid.store);
#endif

	info++;
	w_assert3((info - bunch) >= 1);
	w_assert3(info - bunch <= extlink_p::max);
	if(nxt == 0 || ei.on_same_page(extid.ext, nxt)==false ) {
#ifdef DEBUG
	    w_assert3(info - bunch > 0);
	    num_logged += (int)(info - bunch);
#endif

	    // Log this group, and reset the list.
	    w_assert3(stid.store != 0);
	    W_COERCE(log_ext_remove(ei.page(), stid, (int)(info-bunch), bunch, true));

	    /* 
	     * Compensate the deallocation  -- special case
	     * as this is undoable
	     */
	    if (xd)  {
		CRASHTEST("extent.7");
		xd->compensate(anchor, true); // undoable
		// get a new anchor
		if(nxt) {
		    // if we get an anchor and don't stop_crit()
		    // we have a problem
		    anchor = xd->anchor();
		}
	    }
	    info = bunch;
	}
	extid.ext = nxt;
    }
    w_assert3(nxt == 0);
    w_assert3(info == bunch);
    w_assert3(num_found == num_logged);
    return RCOK;
}



/*********************************************************************
 *
 *  vol_t::free_extent(snum, extnum, bool &)
 *
 *  Deallocate the extent. Called from io_m::_free_page.
 *  The extent is de-allocated if:
 *	a. it is empty AND
 *	b. it is not the first extent of the store AND
 *	c. no other xct holds any kind of lock on any of the extent's pages.
 *  We have already acquired an EX lock on the extent by clr_bit()
 *  (deallocation of a page in the extent).  We have to explicitly
 * 
 *  Returns in the Boolean result argument 'true' iff the extent
 *  was deallocated.
 *
 *********************************************************************/
rc_t
vol_t::free_extent(snum_t snum, extnum_t extnum, bool & was_freed)
{
    extnum_t  prev_extnum=0;
    extnum_t  first_extnum;
    extnum_t  next_extnum;

    extlink_i ei(_epid);

    extlink_t link;
    const extlink_t *linkp = &link;
    W_DO(ei.get(extnum, linkp));

    was_freed = false;
    DBG(<<" free_ext store=" << snum << " ext=" << extnum << " link= " << linkp);
    DBG(<< " num_set()" << linkp->num_set());

    if (linkp->num_set() == 0) { // it is empty

	W_DO(first_ext(snum, first_extnum));

	DBG(<<" first extent for " << snum << " is " << first_extnum);

	if (first_extnum != extnum) { // not first ext of store
	    prev_extnum = linkp->prev;
	    next_extnum = linkp->next;
	    w_assert3(prev_extnum != 0);


	    /* see if all the pages are unlocked */
	    lpid_t pid;
	    pid._stid = stid_t(_vid, snum);

	    for (int i = 0; i < smlevel_0::ext_sz; i++) {
	        pid.page = smlevel_0::ext_sz * extnum + i;
		 /*
		  * see if any other tx's have EX locks on any
		  * of the pages in the extent
		  */
		DBG(<<"test lock on page " << pid);
#ifndef MULTI_SERVER
	        rc_t rc = lm->lock_force(pid, EX, t_instant, WAIT_IMMEDIATE);
#else
	        rc_t rc = lm->lock_force(pid, EX, t_instant, WAIT_IMMEDIATE, TRUE/*optimistic*/);
#endif
	        if (rc && (
		    rc.err_num() == eMAYBLOCK || 
		    rc.err_num() == eLOCKTIMEOUT ||
		    rc.err_num() == eDEADLOCK 
		)) {
		    return RCOK;
		} else if (rc) {
		    return rc.reset();
		}
	    }

	    // No locks held on any of the pages.

	    // BUG: in the preemptive case, there is a race condition here--
	    // the above locks should be short, not instant

	    {
		/* lock the prev extent in order to 
		 * allow us to preserve order on abort 
		 */
		extid_t extid;
		extid.vol = vid();
		extid.ext = prev_extnum;

		//
		// If we can't get the lock immediately, we can't
		// deallocate the extent
		// The reason for this is that if we were to block
		// here, we'd have a slew of possible
		// lock-latch deadlocks. (BUG: we need to do one
		// of the following: find all cases where one could
		// deallocate an extent while holding a page fixed, or
		// find somewhere else (e.g. when leaving the SM in those
		// cases) to re-try to clean up the file)
		//
		rc_t rc = io_lock_force(extid, EX, t_long, WAIT_IMMEDIATE);
	        if (rc && (
			(rc.err_num() == eMAYBLOCK) || 
			(rc.err_num() == eLOCKTIMEOUT) ||
			(rc.err_num() == eDEADLOCK) 
		    )) {
		    return RCOK;
		} else if (rc) {
		    return rc.reset();
		}

		/* ok, we have the lock! */
	    }
	    {
		/* lock the next extent in order to 
		 * allow us to preserve order on abort 
		 */
		extid_t extid;
		extid.vol = vid();
		extid.ext = linkp->next;

		//
		// If we can't get the lock immediately, we can't
		// deallocate the extent
		// The reason for this is that if we were to block
		// here, we'd have a slew of possible
		// lock-latch deadlocks. (BUG: we need to do one
		// of the following: find all cases where one could
		// deallocate an extent while holding a page fixed, or
		// find somewhere else (e.g. when leaving the SM in those
		// cases) to re-try to clean up the file)
		//
		rc_t rc = io_lock_force(extid, EX, t_long, WAIT_IMMEDIATE);
	        if (rc && (
			(rc.err_num() == eMAYBLOCK) || 
			(rc.err_num() == eLOCKTIMEOUT) ||
			(rc.err_num() == eDEADLOCK) 
		    )) {
		    return RCOK;
		} else if (rc) {
		    return rc.reset();
		}

		/* ok, we have the lock! */
	    }

	    {
		/*  
		 * OK, do the de-allocation.  
		 * We logically-log it and compensate it 
		 * so that it's physically re-done, 
		 * but logically un-done.
		 */
		stid_t stid(vid(), snum);

		// For now, log every one individually 

		lsn_t anchor;
		xct_t* xd = xct();
		if(xd) {
		    anchor = xct()->anchor();
		}
		w_assert3(xd); 

		// do the de-allocation.  
		ext_log_info_t info;
		{
		    snum_t s;
		    linkp->getmap(info.pmap);
		    info.ext = extnum;
		    W_COERCE(ei.remove(extnum, s, info.prev, info.next));
		    w_assert1(snum == s);
		}

		W_COERCE(log_ext_remove(ei.page(), stid, 1, &info, false) );
		/* 
		 * Compensate the deallocation 
		 */
		if (xd)  {
		    CRASHTEST("extent.4");
		    xd->compensate(anchor, true);
		}
	    }
	    was_freed = true;
        } else {
	    w_assert3(prev_extnum == 0);
	}
    }
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
    w_assert1(is_valid_ext(ext));
    const extlink_t* link;
    W_DO(ei.get(ext, link)); 
    w_assert1(link->owner);
    result = link->next;
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
    
    for (uint i = 1; i < _num_exts; i++)  {
	const stnode_t& stnode = st.get(i);
	if (stnode.head == 0) {
	    stid.store = i;
	    /* 
	     * lock the store that we're allocating
	     * If we can't do so immediately, we keep looking
	     */
#ifndef MULTI_SERVER
	    if (lm->lock_force(stid, EX, t_long, WAIT_IMMEDIATE))  {
#else
	    if (lm->lock_force(stid, EX, t_long, WAIT_IMMEDIATE, TRUE/*optimistic*/))  {
#endif
		continue;
	    }
	    snum = i;
	    return RCOK;
	}
    }
    return RC(eOUTOFSPACE);
}




/*********************************************************************
 *
 *  vol_t::set_store_flags(snum, flags)
 *
 *  Set the store flag to "flags".
 *
 *********************************************************************/
rc_t
vol_t::set_store_flags(snum_t snum, uint4_t flags)
{
    if (snum <= 0 || snum >= _num_exts)   
	return RC(eBADSTID);

    stnode_i st(_spid);
    stnode_t stnode = st.get(snum);

    if (! stnode.head)
	return RC(eBADSTID);

    w_assert1(stnode.head);

    stnode.flags = flags;
    W_DO(st.put(snum, stnode));

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
vol_t::get_store_flags(snum_t snum, uint4_t& flags)
{
    if (snum >= _num_exts)   
	return RC(eBADSTID);

    if (snum == 0)  {
	flags = 0;
	return RCOK;
    }

    stnode_i st(_spid);
    stnode_t stnode = st.get(snum);

    /*
     *  Make sure the store for this page is marked as allocated.
     *  However, this is not necessarily true during recovery-redo
     *  since it depends on the order pages made it to disk before
     *  a crash.
     */
    if (! stnode.head && !in_recovery)
	return RC(eBADSTID);

    flags = stnode.flags;

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
vol_t::alloc_store(snum_t snum, int eff, uint4_t flags)
{
    if (snum >= _num_exts)   
	return RC(eBADSTID);

    stnode_i st(_spid);
    stnode_t stnode = st.get(snum);
    w_assert1(!stnode.head);

    if (eff < 20 || eff > 100)  eff = 100;
    
    stnode.head = 0;
    stnode.eff = eff;
    stnode.flags = flags;

    W_DO(st.put(snum, stnode, true /*creating*/));

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
    if (snum <= 0 || snum >= _num_exts)   
	return RC(eBADSTID);

    stnode_i st(_spid);
    stnode_t stnode = st.get(snum);
    w_assert1(!stnode.head);

    stnode.head = head;
    W_DO(st.put(snum, stnode));
#ifdef DEBUG
    {
	extlink_i ei(_epid);
	extlink_t link;
	W_COERCE(ei.get_copy(head, link));
    }
#endif
    return RCOK;
}




/*********************************************************************
 *
 *  vol_t::free_store(snum)
 *
 *  Free the store at "snum".
 *
 *********************************************************************/
rc_t
vol_t::free_store(snum_t snum)
{
    stnode_i st(_spid);
    stnode_t stnode = st.get(snum);

    if(stnode.head) {

	/*
	 *  Lock the store.
	 */
	stid_t stid;
	stid.vol = _vid;
	stid.store = snum;
	/*
	 *  Keep another tx from allocating the store before we're done.
	 *  BUGBUG (NANCY) : why is this IMMEDIATE and OPTIMISTIC?
	 */
#ifndef MULTI_SERVER
	W_COERCE( lm->lock_force(stid, EX, t_long, WAIT_IMMEDIATE));
#else
	W_COERCE( lm->lock_force(stid, EX, t_long, WAIT_IMMEDIATE, TRUE/*optimistic */) );
#endif

	stnode.head = stnode.eff = 0;
	W_DO(st.put(snum, stnode));

	xct()->set_freed();

	CRASHTEST("store.1");
    }
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

    stnode_i st(_spid);
    stnode_t stnode = st.get(snum);

    if (!stnode.head)  return RC(eBADSTID);

    extlink_i ei(_epid);
    extnum_t ext = stnode.head;
    const extlink_t* link;
    int first;
    while (ext)  {
	W_DO(ei.get(ext, link));
	if (allocated) {
	    // we care about unallocated pages, so use first one
	    first = 0;
	} else {
	    // only return allocated pages
	    first = link->first_set(0);
	}
	if (first >= 0)  {
	    pid._stid = stid_t(_vid, snum);
	    pid.page = ext * ext_sz + first;
	    if (allocated) *allocated = link->is_set(first);
	    break;
	}
	ext = link->next;
    }
    if ( ! ext) return RC(eEOF);

    return RCOK;
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
	stnode = st.get(snum);
    }

    if ( ! stnode.head)  return RC(eBADSTID);

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
	int i;
	for (i = ext_sz-1; i >= 0 && (linkp->is_clr(i)&&!allocated); i--);
	if (i >= 0) {
	    page = ext * ext_sz + i;
	    if (allocated) *allocated = linkp->is_set(i);
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
	stnode = st.get(snum);
    }

    if ( ! stnode.head)  return RC(eBADSTID);

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
vol_t::num_exts(snum_t snum, uint4_t& cnt)
{
    cnt = 0;
    stnode_t stnode;
    {
	stnode_i st(_spid);
	stnode = st.get(snum);
    }

    if ( ! stnode.head)  return RC(eBADSTID);

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
 *  vol_t::is_alloc_ext(ext)
 *
 *  Return true if extent "ext" is allocated. False otherwise.
 *
 *********************************************************************/
bool vol_t::is_alloc_ext(extnum_t e) 
{
    w_assert3(is_valid_ext(e));
    
    extlink_i ei(_epid);
    const extlink_t* linkp;
    W_DO(ei.get(e, linkp));
    return (linkp->owner != 0) ? TRUE : FALSE;
}




/*********************************************************************
 *
 *  vol_t::is_alloc_store(store)
 *
 *  Return true if the store "store" is allocated. False otherwise.
 *
 *********************************************************************/
bool vol_t::is_alloc_store(snum_t f)
{
    stnode_i st(_spid);
    const stnode_t& stnode = st.get(f);
    return (stnode.head != 0) ? TRUE : FALSE;
}



/*********************************************************************
 *
 *  vol_t::is_alloc_page(pid)
 *
 *  Return true if the page "pid" is allocated. False otherwise.
 *
 *********************************************************************/
bool vol_t::is_alloc_page(const lpid_t& pid)
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
    w_assert1(pnum > 0 && pnum < _num_exts * ext_sz);
    off_t offset = off_t(pnum * sizeof(page));

    smthread_t* t = me();
    off_t pos;
    if (t->lseek(_unix_fd, offset, SEEK_SET, pos))
	W_FATAL(eOS);

    if (t->read(_unix_fd, (char*) &page, sizeof(page)))
	W_FATAL(eOS);

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
    w_assert1(pnum > 0 && pnum < _num_exts * ext_sz);
    w_assert1(pnum == page.pid.page);
    off_t offset = off_t(pnum * sizeof(page));

    smthread_t* t = me();
    off_t pos;
    if (t->lseek(_unix_fd, offset, SEEK_SET, pos))
        W_FATAL(eOS);

    if (t->write(_unix_fd, (char*) &page, sizeof(page)))
        W_FATAL(eOS);

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
    w_assert1(pnum > 0 && pnum < _num_exts * ext_sz);
    off_t offset = off_t(pnum * sizeof(page_s));
    int i;

    smthread_t* t = me();
    off_t pos;
    if (t->lseek(_unix_fd, offset, SEEK_SET, pos))
        W_FATAL(eOS);

    w_assert1(cnt > 0 && cnt <= 8);
    iovec iov[8];
    for (i = 0; i < cnt; i++)  {
	iov[i].iov_base = (caddr_t) pages[i];
	iov[i].iov_len = sizeof(page_s);
	w_assert1(pnum + i == pages[i]->pid.page);
    }

    if (t->writev(_unix_fd, iov, cnt)) {
        W_FATAL(eOS);
    }

    return RCOK;
}

const char* vol_t::prolog[] = {
    "%% SHORE VOLUME VERSION ",
    "%% device quota(KB)  : ",
    "%% volume_id    	  : ",
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
    xct_log_switch_t log_off(OFF);
    
    DBG( << "formating device " << devname);
    int flags = O_CREAT | O_RDWR | (force ? O_TRUNC : O_EXCL);
    int fd = open(devname, flags, 0666);
    if (fd < 0) return RC(eOS);
    
    u_long num_exts = (num_pages - 1) / ext_sz + 1;

    volhdr_t vhdr;
    vhdr.format_version = volume_format_version;
    vhdr.device_quota_KB = num_pages*page_sz/1024;
    vhdr.ext_size = 0;
    vhdr.num_exts = num_exts;
    vhdr.hdr_exts = 0;
    vhdr.epid = 0;
    vhdr.spid = 0;
    vhdr.page_sz = page_sz;
   
    // determine if the volume is on a raw device
    bool raw;
    rc_t rc;
    if (rc = log_m::check_raw_device(devname, raw))  {
	close(fd);
	return RC_AUGMENT(rc);
    }

    if (rc = write_vhdr(fd, vhdr, raw))  {
	close(fd);
	return RC_AUGMENT(rc);
    }

    close(fd);
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
     */
    xct_log_switch_t log_off(OFF);
    
    /*
     *  Read the volume header
     */
    volhdr_t vhdr;
    W_DO(read_vhdr(devname, vhdr));
    if (vhdr.lvid == lvid) return RC(eVOLEXISTS);
    if (vhdr.lvid != lvid_t::null) return RC(eDEVICEVOLFULL); 

    uint4 quota_pages = vhdr.device_quota_KB/(page_sz/1024);
    if (num_pages > quota_pages) {
	return RC(eVOLTOOLARGE);
    }

    /*
     *  Determine if the volume is on a raw device
     */
    bool raw;
    rc_t rc;
    if (rc = log_m::check_raw_device(devname, raw))  {
	return RC_AUGMENT(rc);
    }


    DBG( << "formating volume " << lvid << " <"
	 << devname << ">" );
    int flags = O_RDWR;
    if (!raw) flags |= O_TRUNC;
    int fd = open(devname, flags, 0666);
    if (fd < 0) return RC(eOS);
    
    /*
     *  Compute:
     *		num_exts: 	# extents for num_pages
     *		ext_pages:	# pages for extent info 
     *		stnode_pages:	# pages for store node info
     *		hdr_pages:	total # pages for volume header
     *				including ext_pages and stnode_pages
     *		hdr_exts:	total # exts for hdr_pages
     */
    u_long num_exts = (num_pages) / ext_sz;
    lpid_t pid;
    long ext_pages = (num_exts - 1) / extlink_p::max + 1;
    long stnode_pages = (num_exts - 1) / stnode_p::max + 1;
    long hdr_pages = ext_pages + stnode_pages + 1;
    uint hdr_exts = (hdr_pages - 1) / ext_sz + 1;

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
    vhdr.format_version = volume_format_version;
    vhdr.lvid = lvid;
    vhdr.ext_size = ext_sz;
    vhdr.num_exts = num_exts;
    vhdr.hdr_exts = hdr_exts;
    vhdr.epid = epid.page;
    vhdr.spid = spid.page;
    vhdr.page_sz = page_sz;
   
    /*
     *  Write volume header
     */
    if (rc = write_vhdr(fd, vhdr, raw))  {
	close(fd);
	return RC_AUGMENT(rc);
    }

    /*
     *  Skip first page ... seek to first extent info page.
     */
    if (lseek(fd, SIZEOF(page_s), SEEK_SET) != sizeof(page_s))  {
	close(fd);
	return RC(eOS);
    }

    {
	page_s* buf = new page_s;
	if (! buf) return RC(eOUTOFMEMORY);
	w_auto_delete_t<page_s> auto_del(buf);
#ifdef PURIFY
	{
	    // zero out data portion of page to keep purify happy.
	    memset(((char*)buf), '\0', sizeof(page_s));
	}
#endif
    
	/*
	 *  Format extent link region
	 */
	{
	    extlink_p ep(buf, extlink_p::st_regular);
	    uint i;
	    for (i = 0; i < num_exts; i += ep.max, ++epid.page)  {
		W_COERCE( ep.format(epid, 
				    extlink_p::t_extlink_p,
				    ep.t_virgin) );
		uint j;
		for (j = 0; j < ep.max; j++)  {
		    extlink_t link;
		    if (j + i < hdr_exts)  {
			if ((link.next = j + i + 1) == hdr_exts)
			    link.next = 0;
			link.owner = 0;
			link.fill();
		    }
		    ep.put(j, link);
		}
		for (j = 0; j < ep.max; j++)  {
		    extlink_t link = ep.get(j);
		    w_assert1(link.owner == 0);
		    if (j + i < hdr_exts) {
			w_assert1(link.next == ((j + i + 1 == hdr_exts) ? 
					      0 : j + i + 1));
			w_assert1(link.first_clr(0) == -1);
		    } else {
			w_assert1(link.next == 0);
		    }
		}
		page_s& page = ep.persistent_part();
		w_assert3(buf == &page);
		if (write(fd, 
#ifdef I860
			  (char *)
#endif
			  &page, sizeof(page)) != sizeof(page))  {
		
		    close(fd);
		    return RC(eOS);
		}
	    }
	}

	/*
	 *  Format store node region
	 */
	{ 
	    stnode_p fp(buf, stnode_p::st_regular);
	    uint i;
	    for (i = 0; i < num_exts; i += fp.max, spid.page++)  {
		W_COERCE( fp.format(spid, 
				    stnode_p::t_stnode_p, 
				    fp.t_virgin) ) ;
		for (int j = 0; j < fp.max; j++)  {
		    stnode_t stnode;
		    stnode.head = 0;
		    stnode.eff = 0;
		    stnode.flags = 0;
		    W_DO(fp.put(j, stnode));
		}
		page_s& page = fp.persistent_part();
		if (write(fd, 
#ifdef I860
			  (char *)
#endif
			  &page, sizeof(page)) != sizeof(page))  {
		    close(fd);
		    return RC(eOS);
		}
	    }
	}
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
	const ext_bytes = page_sz * ext_sz;
	char* cbuf = new char[ext_bytes];
	w_assert1(cbuf);
	w_auto_delete_array_t<char> auto_del(cbuf);
	memset(cbuf, 0, ext_bytes);

        /*
	 *  zero out bytes left on first extent
	 */
	off_t curr_off = lseek(fd, 0L, SEEK_CUR); // get current offset
	if (curr_off < 0) {
	    close(fd);
	    return RC(eOS);
	}
	int leftover = CAST(int, ext_bytes - (curr_off % ext_bytes));
	w_assert3( (leftover % page_sz) == 0);
    	if (write(fd, cbuf, leftover) != leftover) {
	    close(fd);
	    return RC(eOS);
	}
	curr_off = lseek(fd, 0L, SEEK_CUR); // get current offset
	w_assert3( (curr_off % ext_bytes) == 0);

	/*
	 *  This is expensive, so see if we should skip it
	 */
	if (skip_raw_init) {
	    DBG( << "skipping zero-ing of raw device: " << devname );
	} else {
	    DBG( << "zero-ing of raw device: " << devname << " ..." );
	    // zero out rest of extents
	    while (curr_off < (off_t)(page_sz * ext_sz * num_exts)) {
		if (write(fd, cbuf, ext_bytes) != ext_bytes) {
		    close(fd);
		    return RC(eOS);
		}
		curr_off += ext_bytes;
	    }
	    w_assert3(curr_off == (off_t)(page_sz * ext_sz * num_exts));
	    DBG( << "finished zero-ing of raw device: " << devname);
    	}

    } else {
	/*
	 * Since the volume is not a raw device, seek to the last byte
	 * and write out a 0.  This way, for any page read from the
	 * volume where the page was never written, the page will be
	 * all zeros.
	 */

	if (lseek(fd, SIZEOF(page_s) * ext_sz * num_exts - 1, SEEK_SET) !=
	    (off_t)(SIZEOF(page_s) * ext_sz * num_exts - 1))  {
	    close(fd);
	    return RC(eOS);
	}

	if (write(fd, "", 1) != 1)  {
	    close(fd);
	    return RC(eOS);
	}
    }

    close(fd);
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
    //if ((fd = dup(fd)) == -1) return RC(eOS);

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
    const tmpsz = page_sz/2;
    char* tmp = new char[tmpsz];
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
	return RC(eOS);
    }
    s.seekp(0, ios::beg);
    if (!s)  {
	return RC(eOS);
    }

    // write out the volume header
    i = 0;
    s << prolog[i] << vhdr.format_version << endl;
    s << prolog[++i] << vhdr.device_quota_KB << endl;
    s << prolog[++i] << vhdr.lvid << endl;
    s << prolog[++i] << vhdr.ext_size << endl;
    s << prolog[++i] << vhdr.num_exts << endl;
    s << prolog[++i] << vhdr.hdr_exts << endl;
    s << prolog[++i] << vhdr.epid << endl;
    s << prolog[++i] << vhdr.spid << endl;
    s << prolog[++i] << vhdr.page_sz << endl;;
    if (!s)  {
	return RC(eOS);
    }

    if (!raw_device) {
	/*
	 *  Write a non-official copy of header at beginning of volume
	 */
	if (lseek(fd, 0, SEEK_SET) != 0) { return RC(eOS); }
	if (write(fd, tmp, tmpsz) != tmpsz) { return RC(eOS); }
    }

    /*
     *  write volume header in middle of page
     */
    if (lseek(fd, sector_size, SEEK_SET) != sector_size)  {
	return RC(eOS);
    }
    if (write(fd, tmp, tmpsz) != tmpsz) {
	return RC(eOS);
    }

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
    const tmpsz = page_sz/2;
    char* tmp = new char[tmpsz];
    if(!tmp) {
	return RC(eOUTOFMEMORY);
    }
    w_auto_delete_array_t<char> autodel(tmp);
    int i;
    for (i = 0; i < tmpsz; i++) tmp[i] = '\0';

    /* 
     *  Read in first page of volume into tmp. 
     *  Dup the fd so we can seek without affecting the
     *  current position of fd.
     */
    if ((fd = dup(fd)) == -1)  { return RC(eOS); }
    if (lseek(fd, sector_size, SEEK_SET) != sector_size) { 
	return RC(eOS); 
    }
    if (read(fd, tmp, tmpsz) != tmpsz) { return RC(eOS); }
    (void) close(fd);

    /*
     *  Read the header strings from tmp using an istream.
     */
    istrstream s(tmp, tmpsz);
    s.seekg(0, ios::beg);
    if (!s)  {
	return RC(eOS);
    }

    char buf[80];
    i = 0;
    s.read(buf, strlen(prolog[i])) >> vhdr.format_version;
    s.read(buf, strlen(prolog[++i])) >> vhdr.device_quota_KB;
    s.read(buf, strlen(prolog[++i])) >> vhdr.lvid;
    s.read(buf, strlen(prolog[++i])) >> vhdr.ext_size;
    s.read(buf, strlen(prolog[++i])) >> vhdr.num_exts;
    s.read(buf, strlen(prolog[++i])) >> vhdr.hdr_exts;
    s.read(buf, strlen(prolog[++i])) >> vhdr.epid;
    s.read(buf, strlen(prolog[++i])) >> vhdr.spid;
    s.read(buf, strlen(prolog[++i])) >> vhdr.page_sz;

    if ( !s || 
	 vhdr.page_sz != page_sz ||
	 vhdr.format_version != volume_format_version ) {

	return RC(eBADFORMAT); 
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
    int fd = open(devname, O_RDONLY);
    if (fd < 0)  {
	return RC(eOS);
    }
    
    rc_t rc = read_vhdr(fd, vhdr);
    if (rc)  {
	close(fd);
	return RC_AUGMENT(rc);
    }

    close(fd);

    return RCOK;
}




/*--------------------------------------------------------------*
 *  vol_t::get_du_statistics()	   DU DF
 *--------------------------------------------------------------*/
rc_t vol_t::get_du_statistics(struct volume_hdr_stats_t& stats, bool audit)
{
    volume_hdr_stats_t new_stats;
    uint4 unalloc_ext_cnt;
    uint4 alloc_ext_cnt;
    W_DO(num_free_exts(unalloc_ext_cnt) );
    W_DO(num_used_exts(alloc_ext_cnt) );
    new_stats.unalloc_ext_cnt = (unsigned) unalloc_ext_cnt;
    new_stats.alloc_ext_cnt = (unsigned) alloc_ext_cnt;
    new_stats.alloc_ext_cnt -= _hdr_exts;
    new_stats.hdr_ext_cnt = _hdr_exts;
    new_stats.extent_size = ext_sz;

    if (audit) {
	if (!(new_stats.alloc_ext_cnt + new_stats.hdr_ext_cnt + new_stats.unalloc_ext_cnt == _num_exts)) {
	    return RC(fcINTERNAL);
	};
	W_DO(new_stats.audit());
    }
    stats.add(new_stats);
    return RCOK;
}


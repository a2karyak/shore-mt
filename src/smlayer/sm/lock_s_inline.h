/*<std-header orig-src='shore' incl-file-exclusion='LOCK_S_INLINE_H'>

 $Id: lock_s_inline.h,v 1.8 1999/06/07 19:04:13 kupsch Exp $

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

#ifndef LOCK_S_INLINE_H
#define LOCK_S_INLINE_H

#include "w_defines.h"

/*  -- do not edit anything above this line --   </std-header>*/

/*
 *  STRUCTURE of a lockid_t
 *
 *    word 0: | lspace  s[0]  |   volid s[1] |
 *    word 1: |       store number           |
 *    word 2: |  page number <or> kvl.g      |
 *    word 3: union {
 *	    |  slotid_t s[6] |   0         |
 *	    |         kvl.h   w[3]         |    
 *	    }
 */

#ifdef W_DEBUG
// This was #included in lock.cpp
#define INLINE
#else
// This was #included in lock_s.h
#define INLINE inline
#endif

INLINE
uint1_t		 	
lockid_t::lspace_bits() const 
{
    // use high byte
    return (s[0] >> 8);
}
INLINE
lockid_t::name_space_t	 	
lockid_t::lspace() const 
{
    return  (lockid_t::name_space_t) lspace_bits();
}

INLINE
void	 		
lockid_t::set_lspace(lockid_t::name_space_t value) 
{
    // lspace is high byte
    uint1_t low_byte = s[0] & 0xff;
    w_assert3((low_byte == 1) || (low_byte == 0));
    s[0] = (value << 8) | low_byte;
}

INLINE
uint2_t	 		
lockid_t::slot_bits() const 
{
    return  s[6];
}

INLINE
uint4_t	 		
lockid_t::slot_kvl_bits() const 
{
    return  w[3];
}

INLINE
const slotid_t&	 	
lockid_t::slot() const 
{
    w_assert3((s[0]&0xff) == 0);
    return  *(slotid_t *) &s[6];
}

INLINE
void	 		
lockid_t::set_slot(const slotid_t & e) 
{
    w_assert3((s[0]&0xff) == 0);
    w_assert3(sizeof(slotid_t) == sizeof(s[6]));
    w[3] = 0; // clear lower part
    s[6] = e;
}

INLINE
const shpid_t&	 	
lockid_t::page() const 
{
    w_assert3(sizeof(shpid_t) == sizeof(w[2]));
    w_assert3((s[0]&0xff) == 0);
    return *(shpid_t *) (&w[2]);
}

INLINE
void	 		
lockid_t::set_page(const shpid_t & p) 
{
    w_assert3((s[0]&0xff) == 0);
    w_assert3(sizeof(shpid_t) == sizeof(w[2]));
    w[2] = (uint4_t) p;
}

INLINE
uint4_t	 		
lockid_t::page_bits() const 
{
    w_assert3(sizeof(shpid_t) <= sizeof(w[2]));
    w_assert3(sizeof(uint4_t) <= sizeof(w[2]));
    return w[2];
}


INLINE
vid_t	 		
lockid_t::vid() const 
{
    return (vid_t) s[1];
}

INLINE
void	 		
lockid_t::set_vid(const vid_t & v) 
{
    w_assert3(sizeof(vid_t) == sizeof(s[1]));
    s[1] = (uint2_t) v.vol;
}

INLINE
uint2_t		 	
lockid_t::vid_bits() const 
{
    return s[1];
}

INLINE
const snum_t&	 	
lockid_t::store() const 
{
    w_assert3(sizeof(snum_t) == sizeof(w[1]));
    w_assert3(lspace() != t_extent);
    w_assert3((s[0]&0xff) == 0);
    return *(snum_t *) (&w[1]);
}

INLINE
void	 		
lockid_t::set_snum(const snum_t & s) 
{
    w_assert3(sizeof(snum_t) == sizeof(w[1]));
    w_assert3(lspace() != t_extent);
    w_assert3((this->s[0]&0xff) == 0);
    w[1] = (uint4_t) s;
}

INLINE
void	 		
lockid_t::set_store(const stid_t & s) 
{
    w_assert3(lspace() != t_extent);
    w_assert3((this->s[0]&0xff) == 0);
    set_snum(s.store);
    set_vid(s.vol);
}

INLINE
uint4_t		 	
lockid_t::store_bits() const 
{
    w_assert3(sizeof(uint4_t) == sizeof(w[1]));
    w_assert3(((s[0]&0xff) == 1) || ((s[0]&0xff) == 0));
    // w_assert3(lspace() != t_extent);
    return w[1];
}

INLINE
const extnum_t&	 	
lockid_t::extent() const 
{
    w_assert3(sizeof(extnum_t) == sizeof(w[1]));
    w_assert3(lspace() == t_extent);
    w_assert3(((s[0]&0xff) == 1) || ((s[0]&0xff) == 0));
    return *(extnum_t *) (&w[1]);
}

INLINE
void	 		
lockid_t::set_extent(const extnum_t & e) 
{
    w_assert3(sizeof(extnum_t) == sizeof(w[1]));
    w_assert3(lspace() == t_extent);
    w_assert3(((s[0]&0xff) == 1) || ((s[0]&0xff) == 0));
    w[1] = (uint4_t) e;
}

INLINE
uint4_t	 		
lockid_t::extent_bits() const 
{
    w_assert3(sizeof(uint4_t) == sizeof(w[1]));
    w_assert3(lspace() == t_extent);
    w_assert3(((s[0]&0xff) == 1) || ((s[0]&0xff) == 0));
    return w[1];
}

INLINE
void			
lockid_t::set_ext_has_page_alloc(bool value) 
{
    // low byte
    w_assert3(lspace() == t_extent);
    w_assert3(((s[0]&0xff) == 1) || ((s[0]&0xff) == 0));
    DBG(<<"s[0] = " << s[0] << " set low byte to " << value);
    uint2_t high_byte = s[0] & 0xff00;
    if(value) {
	s[0] = high_byte | 0x1;
    } else {
	s[0] = high_byte | 0x0;
    }
    w_assert3(((s[0]&0xff) == 1) || ((s[0]&0xff) == 0));
    DBG(<<"s[0] = " << (unsigned int)s[0]);
    w_assert3(lspace() == t_extent);
}

INLINE
bool			
lockid_t::ext_has_page_alloc() const 
{
    w_assert3(lspace() == t_extent);
    w_assert3(((s[0]&0xff) == 1) || ((s[0]&0xff) == 0));
    return ((s[0] & 0xff) != 0);
}

INLINE
void			
lockid_t::extract_extent(extid_t &e) const 
{
    w_assert3( lspace() == t_extent);
    w_assert3(((s[0]&0xff) == 1) || ((s[0]&0xff) == 0));
    e.vol = vid();
    e.ext = extent();
}

INLINE
void			
lockid_t::extract_stid(stid_t &s) const 
{
    w_assert3(
	lspace() == t_store || 
	lspace() == t_page || 
	lspace() == t_kvl || 
	lspace() == t_record);
    w_assert3((this->s[0]&0xff) == 0);
    s.vol = vid();
    s.store = store();
}

INLINE
void			
lockid_t::extract_lpid(lpid_t &p) const 
{
    w_assert3(lspace() == t_page || lspace() == t_record);
    w_assert3((s[0]&0xff) == 0);
    extract_stid(p._stid);
    p.page = page();
}

INLINE
void			
lockid_t::extract_rid(rid_t &r) const 
{
    w_assert3(lspace() == t_record);
    w_assert3((s[0]&0xff) == 0);
    extract_lpid(r.pid);
    r.slot = slot();
}

INLINE
void			
lockid_t::extract_kvl(kvl_t &k) const 
{
    w_assert3((s[0]&0xff) == 0);
    w_assert3(lspace() == t_kvl);
    extract_stid(k.stid);
    k.h = w[2];
    k.g = w[3];
}

INLINE
void
lockid_t::extract_user1(user1_t &u) const
{
    w_assert3((s[0]&0xff) == 0);
    w_assert3(
	lspace() == t_user1 ||
	lspace() == t_user2 ||
	lspace() == t_user3 ||
	lspace() == t_user4);
    u.u1 = s[1];
}

INLINE
void
lockid_t::extract_user2(user2_t &u) const
{
    w_assert3(
	lspace() == t_user2 ||
	lspace() == t_user3 ||
	lspace() == t_user4);
    extract_user1(u);
    u.u2 = w[1];
}

INLINE
void
lockid_t::extract_user3(user3_t &u) const
{
    w_assert3(
	lspace() == t_user3 ||
	lspace() == t_user4);
    extract_user2(u);
    u.u3 = w[2];
}

INLINE
void
lockid_t::extract_user4(user4_t &u) const
{
    w_assert3(lspace() == t_user4);
    extract_user3(u);
    u.u4 = w[3];
}

INLINE
bool
lockid_t::IsUserLock() const
{
    return (lspace() >= t_user1 && lspace() <= t_user4);
}

INLINE
uint2_t
lockid_t::u1() const
{
    return s[1];
}

INLINE
void
lockid_t::set_u1(uint2_t u)
{
    s[1] = u;
}

INLINE
uint4_t
lockid_t::u2() const
{
    return w[1];
}

INLINE
void
lockid_t::set_u2(uint4_t u)
{
    w[1] = u;
}

INLINE
uint4_t
lockid_t::u3() const
{
    return w[2];
}

INLINE
void
lockid_t::set_u3(uint4_t u)
{
    w[2] = u;
}

INLINE
uint4_t
lockid_t::u4() const
{
    return w[3];
}

INLINE
void
lockid_t::set_u4(uint4_t u)
{
    w[3] = u;
}



INLINE void
lockid_t::zero()
{
    // equiv of
    // set_lspace(t_bad);
    // set-ext_has_page_alloc(0);
    s[0] = (t_bad << 8);

    set_vid(0);
    set_snum(0);
    set_page(0);
    // set_slot(0);
    w[3] = 0; // have to get entire thing
}


INLINE NORET
lockid_t::lockid_t()
{
    zero(); 
}

INLINE NORET
lockid_t::lockid_t(const vid_t& vid)
{
    zero();
    set_lspace(t_vol);
    set_vid(vid);
}

INLINE NORET
lockid_t::lockid_t(const extid_t& extid)
{
    zero();
    set_lspace(t_extent);
    set_vid(extid.vol);
    set_extent(extid.ext);
}

INLINE NORET
lockid_t::lockid_t(const stid_t& stid)
{
    zero();
    set_lspace(t_store);
    set_vid(stid.vol);
    set_snum(stid.store);
}

INLINE NORET
lockid_t::lockid_t(const stpgid_t& stpgid)
{
    zero();
    if (stpgid.is_stid()) {
	set_lspace(t_store);
	set_vid(stpgid.vol());
	set_snum(stpgid.store());
    } else {
	set_lspace(t_page);
	set_vid(stpgid.lpid.vol());
	set_snum(stpgid.lpid.store());
	set_page(stpgid.lpid.page);
    }
}

INLINE NORET
lockid_t::lockid_t(const lpid_t& lpid)
{
    zero();
    set_lspace(t_page);
    set_vid(lpid.vol());
    set_snum(lpid.store());
    set_page(lpid.page);
}

INLINE NORET
lockid_t::lockid_t(const rid_t& rid)
{
    zero();
    set_lspace(t_record);
    // w[1-3] is assumed (elsewhere) to
    // look just like the following sequence
    // (which is the beginning of a rid_t-- see sm_s.h):
    // shpid_t	page;
    // snum_t	store;
    // slotid_t	slot;
    set_vid(rid.pid.vol());
    set_snum(rid.pid.store());
    set_page(rid.pid.page);
    set_slot(rid.slot);
}

INLINE NORET
lockid_t::lockid_t(const kvl_t& kvl)
{
    zero();
    set_lspace(t_kvl);
    w_assert3(sizeof(kvl_t) == sizeof(w[1])*2 + sizeof(stid_t));
    set_store(kvl.stid);
    w[2] = kvl.h;
    w[3] = kvl.g;
}

INLINE NORET
lockid_t::lockid_t(const user1_t& u)
{
    zero();
    set_lspace(t_user1);
    s[1] = u.u1;
}

INLINE NORET
lockid_t::lockid_t(const user2_t& u)
{
    zero();
    set_lspace(t_user2);
    s[1] = u.u1;
    w[1] = u.u2;
}

INLINE NORET
lockid_t::lockid_t(const user3_t& u)
{
    zero();
    set_lspace(t_user3);
    s[1] = u.u1;
    w[1] = u.u2;
    w[2] = u.u3;
}

INLINE NORET
lockid_t::lockid_t(const user4_t& u)
{
    zero();
    set_lspace(t_user4);
    s[1] = u.u1;
    w[1] = u.u2;
    w[2] = u.u3;
    w[3] = u.u4;
}


INLINE lockid_t&
lockid_t::operator=(const lockid_t& i)
{
    /* Could use set_xxx(xxx_bits()) but
    * then we have to switch on lspace()
    * in order to cope with extent and kvl
    * special cases 
    */
    w[0] = i.w[0]; 
    w[1] = i.w[1]; 
    w[2] = i.w[2]; 
    w[3] = i.w[3]; 
    return *this;
}

INLINE bool
lockid_t::operator==(const lockid_t& l) const
{
    // ext_has_page_alloc() is true if extent has pages allocated
    // ext_has_page_alloc() does not participate in testing for equality
    return !((lspace_bits() ^ l.lspace_bits()) | 
	(vid_bits() ^ l.vid_bits()) | 
	(store_bits() ^ l.store_bits())  |
	(page_bits() ^ l.page_bits())   |
	(slot_kvl_bits() ^ l.slot_kvl_bits()) );
    /*
    the above is the same as the following, but runs 
    faster since it doesn't have conditions on the &&

	return ((lspace_bits() == l.lspace_bits()) && 
	    (vid_bits() == l.vid_bits()) &&
	    (store_bits() == l.store_bits())  &&
	    (page_bits() == l.page_bits())   &&
	    (slot_kvl_bits() == l.slot_kvl_bits()) );
    */
}

INLINE NORET
lockid_t::lockid_t(const lockid_t& i)
{
    (void) this->operator=(i);
}

INLINE bool
lockid_t::operator!=(const lockid_t& l) const
{
    return ! (*this == l);
}

#define HASH_FUNC 3
// 3 seems to be the best combination, so far

#undef DEBUG_HASH

#if HASH_FUNC>=3
INLINE uint4_t
lockid_t::hash() const
{
    uint4_t value =  lspace_bits() ^ vid_bits() ^ page_bits() ^ slot_kvl_bits();
    if (lspace_bits() >= t_user1)  {
	value ^= store_bits();
    }
    return value;
}
#endif

/*
 * Lock ID hashing functions
 */
#if HASH_FUNC<3
INLINE uint4_t
lockid_t::hash() const
{
    uint4_t t;

    // volume + store
    t = vid_bits() ^ store_bits();

    if(lspace() == t_kvl)
        return t ^ page_bits() + slot_kvl_bits();

    // type
    t ^= lspace()<<2;

    // page
    t ^= page_bits() ;

    // slot
    t ^= slot_kvl_bits();

    return t;
}
#endif

INLINE
ostream& operator<<(ostream& o, const lockid_t::user1_t& u)
{
    return o << "u1(" << u.u1 << ")";
}

INLINE
ostream& operator<<(ostream& o, const lockid_t::user2_t& u)
{
    return o << "u2(" << u.u1 << "," << u.u2 << ")";
}

INLINE
ostream& operator<<(ostream& o, const lockid_t::user3_t& u)
{
    return o << "u3(" << u.u1 << "," << u.u2 << "," << u.u3 << ")";
}

INLINE
ostream& operator<<(ostream& o, const lockid_t::user4_t& u)
{
    return o << "u4(" << u.u1 << "," << u.u2 << "," << u.u3 << "," << u.u4 << ")";
}


/*<std-footer incl-file-exclusion='LOCK_S_INLINE_H'>  -- do not edit anything below this line -- */

#endif          /*</std-footer>*/

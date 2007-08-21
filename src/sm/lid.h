/*<std-header orig-src='shore' incl-file-exclusion='LID_H'>

 $Id: lid.h,v 1.65 2007/08/21 19:50:42 nhall Exp $

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

#ifndef LID_H
#define LID_H

#include "w_defines.h"

/*  -- do not edit anything above this line --   </std-header>*/

/*********************************************************************

		__The Logical ID (lid) Manager__

   The logical ID (LID) manager supports location indepenent
   identifiers for volumes, indexes, files, and records.  A LID
   consists of two parts:  a logical volume ID (lvid) and a serial
   number.  Except for implementation limitation listed below, lvid's
   are unique across the universe of volumes.  Serial numbers are 4
   byte integers unique to a volume.  Serial numbers are never reused.
   There is a parallel set of logical ID type names identical to the
   physical ID types except that they have "l" as a prefix (see sm.h).

   LID Index
   ---------
   Each volume has an index (LID index) used for mapping serial numbers
   to physical IDs.  An entry in this LID index mapps a serial number
   to either a rid_t (record) or snum_t (store) on the volume, or a
   serial number on a remote volume (identified with a lvid). 

   A second index (remote_index) is maintained for all serial-remote
   mappings.  This index is keyed on a full logical ID and maps
   to a serial number on the volume.  This is used to avoid generating
   duplicate remote entries.

   For each mounted volume the lid_m maintains a record (vol_lid_info_t)
   containing the physical volume id, LID index physical ID, and
   information about what serial numbers can be used for new
   logical IDs.

   LID Index Cache
   ---------------
   The lid_m object maintains an LRU cache of recent lookups in the
   LID index.  The size of the cache is fixed when lid_m is created.
   Currently the size of this cache is based on the heuristic that it
   need not be bigger than the maximum number of record that can fit on
   all of the buffer pool pages.  Additions are made the the cache
   after a miss and whenever a page of records is cached (each record's
   ID is added to othe cache).  Entries are removed when objects
   are destroyed or when an entry is found to be incorrect (ie.
   out-of-date).  Entries can become out-of-date because of operations
   like transaction rollback which only effect the index, not the cache.

   Serial Numbers
   --------------
   Serial numbers have a number of bits which are reserved for special
   purposes.  First, since serial numbers are often used in place of
   memory addresses, applications such as SHORE require that low-order
   bits be reserved for use in distinguishing serial numbers from
   memory addresses.  For this purpose the constant "low_reserve"
   specifies how many low-order bits are reserved for applications.
   These bits are guaranteed to have the value 1 for all serial
   numbers.  In addition, the Shore Server itself reserves the
   high-order bit to distinguish between references to local records
   and stores versus references to logical IDs on a different volume.
   Local references have the high-order bit set to 0.

   The use of an additional bit for distinguishing between
   local and remote references, complicates the allocation of
   serial numbers by dividing the "address space" of serial numbers
   into two parts.

   Serial numbers are allocated in a monotonically increasing order
   and are never reused.  If necessary, dummy records can be
   placed in the index to guarantee correct allocation.

   IMPLEMENTATION LIMITATIONS
   --------------------------
   Logical volume IDs are made unique by making them a combination of
   the network address of the machine they are formatted on, and the
   physical ID given to the volume when it is formatted.

   Serial numbers are currently 4bytes in size.  We expect to use
   8byte serial numbers in the final release.

**********************************************************************/

#ifndef HASH_LRU_H
#include <hash_lru.h>
#endif

#ifdef __GNUG__
#pragma interface
#endif

// HP CC does not support using a nested class as a type parameter
// to a template.  So, we must include these structures outside of
// lid_m.
#ifdef HP_CC_BUG_3
#ifndef LID_S_H
#   include <lid_s.h>
#endif
#endif

/*
 * Logical ID Manager class.
 */
class lid_m : public smlevel_4 {
public:


#ifdef HP_CC_BUG_3
    typedef ::vol_lid_info_t vol_lid_info_t;
#   else
#ifndef LID_S_H
#   	include <lid_s.h>
#endif
#   endif


    

    NORET			lid_m(
	int 			    max_vols
	);
    NORET			~lid_m();

    rc_t			add_local_volume(
	vid_t 			    vid, 
	const lvid_t& 		    lvid
	);
    rc_t			add_remote_volume(
	vid_t 			    vid, 
	const lvid_t& 		    lvid);
    rc_t			remove_volume(lvid_t lvid);
    rc_t			remove_all_volumes();

    bool			is_mounted(const lvid_t& lvid);

    rc_t			lookup(const lvid_t& lvid, vid_t& vid);

    rc_t			get_lvid(
	const vid_t& 		    vid, 
	lvid_t& 		    lvid);


    rc_t			generate_new_volid(lvid_t& lvid);

private:

   
    // Now, without LIDs, it is just the association of
    // logical id and physical id volume ids
    w_hash_t<vol_lid_info_t, lvid_t>* _vol_table;
    int				_num_vols;
    smutex_t			_vol_mutex;

    rc_t			_get_vol_info(
	const lvid_t& 	            lvid, 
	vol_lid_info_t& 	    vol_info);
    rc_t			_add_volume(
	vid_t 			    vid,
	const lvid_t& 		    lvid
	);
    rc_t			_remove_volume(lvid_t lvid);
    rc_t			_get_lvid(const vid_t& vid, lvid_t& lvid);

    vol_lid_info_t* 		_lookup_lvid(
	const lvid_t&		    lvid
	);

    // disabled
    NORET			lid_m(const lid_m&);
    lid_m& 			operator=(const lid_m&);
};

/*
 *
  The following LID_CACHE_RETRY* macros are used for invalidating lid
  cache entries when they are found to be out-of-date.  The id
  parameter is of type lid_t and is the logical ID to be looked up.
  The phys parameter is a variable to hold the physical ID looked up,
  and phys_type is it's type.  The rc parameter is for holding w_rc_t
  return codes from calls to use_func.  The use_func parameter is the
  function called to use the returned physical ID.  If the use_func
  fails, it may be due to a lid cache inconsistency.  Therefore, the ID
  is removed from the cache and the lookup_func and use_func are called
  again.  The loop continues until no error occurs or until the lookup
  return the same value as the previous attempt.

 */
#define LID_CACHE_RETRY(id, phys_type, phys, rc, use_func)	\
    {								\
	W_DO(lid->lookup((id), (phys)));			\
	do {							\
	    rc = use_func;					\
	    if (rc && rc.err_num() == smlevel_0::eDEADLOCK) {	\
		/* No use retrying if the call to use_func */	\
		/* led to deadlock. If we do retry, then the */	\
		/* same deadlock will be detected again which */\
		/* is expensive in the distributed case and */ 	\
		/* may also cause other problems. A deadlock */	\
		/* IS possible to occur when looking up a */	\
		/* remote id. */ 				\
		break;						\
	    } else if (rc) {					\
		phys_type phys_old = phys;			\
		lid->cache_remove(id);				\
		W_DO(lid->lookup((id), (phys)));		\
		if (phys_old == phys) {				\
		    /* retrying should not change the result */	\
		    /* so just exit loop with rc*/		\
		    break;					\
		}						\
	    }							\
	} while (rc);						\
    }

#define LID_CACHE_RETRY_DO(id, phys_type, phys, use_func)	\
    {								\
	w_rc_t __rc;						\
	LID_CACHE_RETRY(id, phys_type, phys, __rc, use_func)	\
	if (__rc) return __rc;					\
    }

#define LID_CACHE_RETRY_VALIDATE_STID(id, stid, rc)		\
    {								\
	sdesc_t* __sd;						\
	LID_CACHE_RETRY(id, stid_t, stid, rc, dir->access(stid, __sd, IS))\
    }

#define LID_CACHE_RETRY_VALIDATE_STID_DO(id, stid)		\
    {								\
	w_rc_t __rc;						\
	LID_CACHE_RETRY_VALIDATE_STID(id, stid, __rc)		\
	if (__rc) return __rc;					\
    }

#define LID_CACHE_RETRY_VALIDATE_STPGID(id, stpgid, rc)		\
    {								\
	sdesc_t* __sd;						\
	LID_CACHE_RETRY(id, stpgid_t, stpgid, rc, dir->access(stpgid, __sd, IS))\
    }

#define LID_CACHE_RETRY_VALIDATE_STPGID_DO(id, stpgid)		\
    {								\
	w_rc_t __rc;						\
	LID_CACHE_RETRY_VALIDATE_STPGID(id, stpgid, __rc)	\
	if (__rc) return __rc;					\
    }


inline rc_t
lid_m::add_local_volume(
    vid_t 		vid, 
    const lvid_t& 	lvid
    )
{
    return _add_volume(vid, lvid
	    );
}

inline rc_t
lid_m::add_remote_volume(
    vid_t 		vid, 
    const lvid_t& 	lvid) 
{
    lpid_t	dummy_lid_index;
    dummy_lid_index._stid.vol = vid;  // mark as volume on remote site
    return _add_volume(vid, lvid
		);
}

inline rc_t
lid_m::get_lvid(
    const vid_t& 	vid, 
    lvid_t& 		lvid) 
{
    return _get_lvid(vid, lvid);
}


/*<std-footer incl-file-exclusion='LID_H'>  -- do not edit anything below this line -- */

#endif          /*</std-footer>*/

/*<std-header orig-src='shore' incl-file-exclusion='LID_S_H'>

 $Id: lid_s.h,v 1.30 2001/06/26 16:48:46 bolo Exp $

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

#ifndef LID_S_H
#define LID_S_H

#include "w_defines.h"

/*  -- do not edit anything above this line --   </std-header>*/

#ifdef __GNUG__
#pragma interface
#endif

/* 
 * The vol_lid_info_t struct contains information about a volume
 * that has a logical ID facility.  One of these structs is
 * stored in lid_m for each volume mounted.
 */
#define VOL_LID_INFO_T
class vol_lid_info_t {
public:

    lvid_t      lvid;
    vid_t	vid;
    lpid_t      lid_index;	// the volume's logical rec ID index
    lpid_t      remote_index;	// index mapping from a remote ID
				// to the ID local to the volume.

    /*
     * Serial number allocation information
     * max_id is used only by the server which manages the volume
     * The first array element refers to IDs for local references
     * (those on the same volume).
     * The second array element refers to IDs for remote references
     * (those on a different volume).
     *
     */
    enum ref_type_t {local_ref = 0, remote_ref = 1};
    serial_t	curr_id[2];	    // current allocated by this server
    serial_t  	allowed_id[2];   // max to allocate on this server
    serial_t  	max_id[2];	    // max serial allocated ever
    w_link_t  	link;

    void init_serials(const serial_t& max)
	{ ref_type_t type = max.is_remote() ? remote_ref : local_ref;
	  curr_id[type] = allowed_id[type] = max_id[type] = max;
	};

    lvid_t hash_key() { return lvid; }
};

// lid index entries can be of the following types
enum entry_type_t { t_invalid = 0x00, // invalid, deleted, unknown 
		    t_rid     = 0x01, // physical record ID
		    t_store   = 0x02, // store (index, file) number
		    t_page    = 0x04, // single page store/index/file
		    t_lid     = 0x08, // logcal ID of remote reference
		    t_max     = 0x10  // marks highest allocated reference 
};

/*
 * The lid_entry_t struct is stored in a volumes logical ID index
 * mapping a serial number to a record, store, or remote record.
 */
class lid_entry_t {
public:
    lid_entry_t() {_init_id(); _type = t_invalid;};
    lid_entry_t(const lid_entry_t& e) {*this = e;};
    lid_entry_t(const shrid_t& shrid)
	    {_init_id(); *(shrid_t*)_id = shrid; _type = t_rid; };
    lid_entry_t(const snum_t& snum)
	    {_init_id(); *(snum_t*)_id = snum; _type = t_store; };
    lid_entry_t(const spid_t& spid)
	    {_init_id(); *(spid_t*)_id = spid; _type = t_page; };
    lid_entry_t(const lid_t& remote)
	    {_init_id(); *(lid_t*)_id = remote; _type = t_lid; };

    // use this to construct a record marking highest allocated #
    lid_entry_t(entry_type_t type)
	    {_init_id(); w_assert1(type == t_max); _type = type; };

#ifdef HP_CC_BUG_3
    lid_entry_t(int type)  // needed since entry_type_t is in lid_m
	    {_init_id(); w_assert1(type == t_max); _type = (entry_type_t)type; };
#endif

    entry_type_t type() const { return _type; }

    const shrid_t& shrid() const { return *(shrid_t*)_id; }
    const snum_t&  snum()  const { return *(snum_t*)_id; }
    const spid_t&  spid()  const { return *(spid_t*)_id; }
    const lid_t&   lid()   const { return *(lid_t*)_id; }

    void set_type(entry_type_t type) { _type = type; }

    // amount of entry that must be saved in the lid mapping index
    int save_size() const;

    enum { max_id_size = sizeof(lid_t) };
    // verify that lid_entry_t::_id has the correct size
    static void check_id_size()
	{ size_t sizeof_lid = sizeof(lid_t);  // for gcc warning bug
					      // must match max_id_size
	    size_t a = sizeof(shrid_t);
	    size_t b = sizeof(spid_t);
	    size_t c = sizeof(snum_t);
	    size_t d = MAX(sizeof_lid, MAX(a, MAX(b, c)));
	    w_assert1(((size_t)max_id_size) == d);
	}

    friend ostream& operator<<(ostream& o, const lid_entry_t& entry);

private:
    // _type is about 3 bytes larger than it needs to be, but
    // this simplifies alignment.  Feel free to use the extra
    // bytes if needed in the furture for more flags.
    entry_type_t    _type;
    // lid_t is the largest thing stored    
    unsigned char   _id[max_id_size];

    void _init_id() {memset(_id, 0, sizeof(_id));}

    /*
     * _id is a substitute for the union below.  This is needed
     * since the types in the union have constructors.
     * The lid_m() verifies that _id is the proper size.
	union {
	    shrid_t _shrid_t;
	    snum_t  _snum;
	    lid_t   _lid;
	} _value;
     */
}; // lid_entry_t

class lid_cache_entry_t : public lid_entry_t {
public:
    //lid_cache_entry_t(const lid_entry_t& lid_entry, vid_t vid);
    vid_t	vid() {return _vid;}
    void	set(const lid_entry_t& l, vid_t& v) 
			{(lid_entry_t&)(*this) = l; _vid = v;}
private:
    vid_t 	_vid;
};


typedef lid_t lid_cache_key_t;
#ifdef notdef
// This is one possible defn of a keys for the lid cache.
// Note that it is smaller than lid_t since vid_t is used
// instead of an lvid_t.  However, there is a performance
// tradeoff in converting from lvid_t to vid_t, so the
// above typedef is used instead.
class lid_cache_key_t {
public:
    lid_cache_key_t(vid_t vid_, const serial_t& serial_) :
 	vid(vid_), serial(serial_)	{};
    vid_t vid;
    serial_t serial;
}
inline w_base_t::uint4_t w_hash(const lid_cache_key_t& k) {return hash}
#endif

/*<std-footer incl-file-exclusion='LID_S_H'>  -- do not edit anything below this line -- */

#endif          /*</std-footer>*/

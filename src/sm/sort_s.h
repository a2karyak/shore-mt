/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

#ifndef SORT_S_H
#define SORT_S_H

#ifdef __GNUG__
#pragma interface
#endif

//
// info on keys
//
struct key_info_t {
    enum key_type_t 	{ t_char=0, t_int, t_float, t_string, t_spatial };
    enum where_t 	{ t_hdr=0, t_body };

    key_type_t  type;	    // key type
    nbox_t 	universe;   // for spatial object only
    bool	derived;    // if true, the key must be the only item in rec
			    // header, and the header will not be copied to
			    // the result record (allow user to store derived
			    // key temporarily for sorting purpose).

    // following applies to file sort only
    where_t 	where;      // where the key resides
    uint4	offset;	    // offset fron the begin
    uint4	len;	    // key length
    
    key_info_t() {
      type = t_int;
      where = t_body;
      offset = 0;
      len = sizeof(int);
      derived = FALSE;
    }
};

//
// sort parameter
//
struct sort_parm_t {
    uint2    run_size;		// size for each run (# of pages)
    vid_t    vol;		// volume for files
    serial_t logical_id;	// logical oid
    bool   unique;		// result unique ?
    bool   ascending;		// ascending order ?
    bool   destructive;	// destroy the input file at the end ?
    bool   keep_lid;		// preserve logical oid for recs in sorted
				// file -- only for destructive sort
    lvid_t   lvid;		// logical volume id
    sm_store_property_t property; // temporary file ?

    sort_parm_t() : run_size(10), unique(false), ascending(true),
		    destructive(false), keep_lid(false),
		    lvid(lvid_t::null), property(t_regular) {}
};

#endif // _SORT_S_H_

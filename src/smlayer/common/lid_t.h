/*<std-header orig-src='shore' incl-file-exclusion='LID_T_H'>

 $Id: lid_t.h,v 1.35 2001/06/26 16:48:30 bolo Exp $

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

#ifndef LID_T_H
#define LID_T_H

#include "w_defines.h"

/*  -- do not edit anything above this line --   </std-header>*/

/*
 * NB -- THIS FILE MUST BE LEGITIMATE INPUT TO cc and RPCGEN !!!!
 * Please do as follows:
 * a) keep all comments in traditional C style.
 * b) If you put something c++-specific make sure it's 
 * 	  got ifdefs around it
 */
#ifndef BASICS_H
#include "basics.h"
#endif
#ifndef SERIAL_T_H
#include "serial_t.h"
#endif

#ifdef __GNUG__
#pragma interface
#endif

/*********************************************************************
 * Logical IDs
 *
 ********************************************************************/

/*
    Physical volume IDs (vid_t) are currently used to make unique
    logical volume IDs.  This is a temporary hack which we support with
    this typedef:
*/
typedef uint2_t VID_T;

	/* 
	// logical volume ID 
	*/

#define LVID_T
struct lvid_t {
    /* usually generated from net addr of creating server */
    uint4_t high;
    /* usually generated from timeofday when created */
    uint4_t low;

#ifdef __cplusplus
    /* do not want constructors for things embeded in objects. */
    lvid_t() : high(0), low(0) {}
    lvid_t(uint4_t hi, uint4_t lo) : high(hi), low(lo) {}
	
    bool operator==(const lvid_t& s) const
			{return (low == s.low) && (high == s.high);}
    bool operator!=(const lvid_t& s) const
			{return (low != s.low) || (high != s.high);}

    // in lid_t.cpp:
    friend ostream& operator<<(ostream&, const lvid_t&);
    friend istream& operator>>(istream&, lvid_t&);

    // defined in lid_t.cpp
    static const lvid_t null;
#endif 
};

/*
// short logical record ID (serial number)
// defined in serial_t.h
*/

class lid_t {
public:
    lvid_t	lvid;
    serial_t    serial;

#ifdef __cplusplus

    lid_t() {};
    lid_t(const lvid_t& lvid_, const serial_t& serial_) :
		lvid(lvid_), serial(serial_)
    {};

    lid_t(uint4_t hi, uint4_t lo, uint4_t ser, bool remote) :
		lvid(hi, lo), serial(ser, remote)
    {};

    inline bool operator==(const lid_t& s) const { 
		return ( (lvid == s.lvid) && (serial == s.serial)) ;
	};
    inline bool operator!=(const lid_t& s) const {
		return (serial != s.serial) || (lvid != s.lvid);
	};

	// in lid_t.cpp:
    friend ostream& operator<<(ostream&, const lid_t& s);
    friend istream& operator>>(istream&, lid_t& s);

    static const lid_t null;

    /* this is the key descriptor for using serial_t's as btree keys */
    static const char* key_descr; 
#endif

};

typedef	lid_t lrid_t;

#ifdef __cplusplus

inline w_base_t::uint4_t w_hash(const lvid_t& lv)
{
    return lv.high + lv.low;
}

inline w_base_t::uint4_t w_hash(const lid_t& l)
{
    return w_hash(l.serial) * w_hash(l.lvid);
}
#endif /*__cplusplus*/

/*<std-footer incl-file-exclusion='LID_T_H'>  -- do not edit anything below this line -- */

#endif          /*</std-footer>*/

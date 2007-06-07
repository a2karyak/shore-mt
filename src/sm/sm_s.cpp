/*<std-header orig-src='shore'>

 $Id: sm_s.cpp,v 1.28 2006/03/14 05:31:26 bolo Exp $

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
#ifdef __GNUG__
#pragma implementation "sm_s.h"
#endif

#include <sm_int_0.h>

#include <w_strstream.h>

const rid_t rid_t::null;

const lpid_t lpid_t::bof;
const lpid_t lpid_t::eof;
const lpid_t lpid_t::null;

const lsn_t lsn_t::null(0, 0);
const lsn_t lsn_t::max(lsn_t::file_hwm, sm_diskaddr_max);


#ifdef SM_DISKADDR_LARGE
sm_diskaddr_t  
lsn_t::rba() const {
    if(i_am_aligned()) {
	return _rba; 
    } else {
	sm_diskaddr_t l;
        // Disallow optimization of memcpy by compiler:
        volatile unsigned int i = sizeof(_rba);
	memcpy(&l, &_rba, i);
	return l;
    }
}

void 
lsn_t::copy_rba(const lsn_t &other) {
    if( i_am_aligned() && other.i_am_aligned()) {
       _rba = other._rba;
    }  else  {
       // Disallow optimization of memcpy by compiler:
       volatile unsigned int i = sizeof(_rba);
       memcpy(&_rba, &other._rba, i);
    }
}

void 
lsn_t::set_rba(sm_diskaddr_t &other) {
    if( ! i_am_aligned()) {
        // Disallow optimization of memcpy by compiler:
        volatile unsigned int i = sizeof(_rba);
	memcpy(&_rba, &other, i);
    } else  {
	_rba = other;
    }
    DBG(<<"set rba to " << _rba);
}
#endif

/*
 * Used only for temporary lsn's assigned to remotely created log recs.
 */
void 
lsn_t::increment()
{
    if (_rba < sm_diskaddr_max-1) {
#ifdef SM_DISKADDR_LARGE
	if(i_am_aligned()) {
	    _rba++;
	} else {
	    sm_diskaddr_t d = rba();
	    d ++;
	    set_rba(d);
	}
#else
	_rba++;
#endif
    } else {
	_rba = 0;
	_file++;
	w_assert1(_file < (uint)max_uint4);
    }
}

/* 
 * used only for computing stats
 */
sm_diskaddr_t 
lsn_t::operator-(const lsn_t& l) const
{
    if (_file == l._file) { 
#ifdef SM_DISKADDR_LARGE
	sm_diskaddr_t other = l.rba();
	sm_diskaddr_t mine = rba();
	return mine - other;
#else
	return _rba - l._rba;
#endif
    } else if (_file == l._file - 1) { 
	return rba();
    } else {
	// should never happen
	W_FATAL(fcINTERNAL);
	return sm_diskaddr_max;
    }
}

lpid_t::operator bool() const
{
#ifdef W_DEBUG
    // try to stomp out uses of this function
    if(_stid.vol.vol && ! page) w_assert3(0);
#endif /* W_DEBUG */
    return _stid.vol.vol != 0;
}

int pull_in_sm_export()
{
    return smlevel_0::eNOERROR;
}

/*********************************************************************
 *
 * key_type_s: used in btree manager interface and stored
 * in store descriptors
 *
 ********************************************************************/

w_rc_t key_type_s::parse_key_type(
    const char* 	s,
    uint4_t& 		count,
    key_type_s		kc[])
{
    w_istrstream is(s);

    uint i;
    for (i = 0; i < count; i++)  {
	kc[i].variable = 0;
	if (! (is >> kc[i].type))  {
	    if (is.eof())  break;
	    return RC(smlevel_0::eBADKEYTYPESTR);
	}
	if (is.peek() == '*')  {
	    // Variable-length keys look like:
	    // ... b*<max>

	    if (! (is >> kc[i].variable))
		return RC(smlevel_0::eBADKEYTYPESTR);
	}
	if (! (is >> kc[i].length))
	    return RC(smlevel_0::eBADKEYTYPESTR);
	if (kc[i].length > key_type_s::max_len)
	    return RC(smlevel_0::eBADKEYTYPESTR);

	switch (kc[i].type)  {

	case key_type_s::I: // signed compressed
	case key_type_s::U: // unsigned compressed
	    kc[i].compressed = true;
	    // drop down
	case key_type_s::i:
	case key_type_s::u:
	    if ( (kc[i].length != 1 
		&& kc[i].length != 2 
		&& kc[i].length != 4 
		&& kc[i].length != 8 )
		|| kc[i].variable)
		return RC(smlevel_0::eBADKEYTYPESTR);
	    break;

	case key_type_s::F: // float compressed
	    kc[i].compressed = true;
	    // drop down
	case key_type_s::f:
	    if (kc[i].length != 4 && kc[i].length != 8 || kc[i].variable)
		return RC(smlevel_0::eBADKEYTYPESTR);
	    break;

	case key_type_s::B: // uninterpreted bytes, compressed
	    kc[i].compressed = true;
	    // drop down
	case key_type_s::b: // uninterpreted bytes
	    w_assert3(kc[i].length > 0);
	    break;

	default:
	    return RC(smlevel_0::eBADKEYTYPESTR);
        }
    }
    count = i;

    for (i = 0; i < count; i++)  {
	if (kc[i].variable && i != count - 1)
	    return RC(smlevel_0::eBADKEYTYPESTR);
    }
    
    return RCOK;
}


w_rc_t key_type_s::get_key_type(
    char* 		s,	// out
    int			buflen, // in
    uint4_t 		count,  // in
    const key_type_s*	kc)   // in
{
    w_ostrstream o(s, buflen);

    uint i;
    char c;
    for (i = 0; i < count; i++)  {
	if(o.fail()) break;

	switch (kc[i].type)  {
	case key_type_s::I: // compressed int
	    c =  'I';
	    break;
	case key_type_s::i:
	    c =  'i';
	    break;
	case key_type_s::U: // compressed unsigned int
	    c = 'U';
	    break;
	case key_type_s::u:
	    c = 'u';
	    break;
	case key_type_s::F: // compressed float
	    c =  'F';
	    break;
	case key_type_s::f:
	    c =  'f';
	    break;
	case key_type_s::B: // uninterpreted bytes, compressed
	    c =  'B';
	    break;
	case key_type_s::b: // uninterpreted bytes
	    c =  'b';
	    break;
	default:
	    return RC(smlevel_0::eBADKEYTYPESTR);
        }
	o << c;
	if(kc[i].variable) o << '*';
	o << kc[i].length;
    }
    o << ends;
    if(o.fail()) return RC(smlevel_0::eRECWONTFIT);
    return RCOK;
}


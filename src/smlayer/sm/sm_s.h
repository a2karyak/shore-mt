/*<std-header orig-src='shore' incl-file-exclusion='SM_S_H'>

 $Id: sm_s.h,v 1.81 1999/06/15 15:11:56 nhall Exp $

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

#ifndef SM_S_H
#define SM_S_H

#include "w_defines.h"

/*  -- do not edit anything above this line --   </std-header>*/

extern "C" int pull_in_sm_export();

typedef uint4_t	extnum_t;

#ifndef STHREAD_H
#include <sthread.h>
#endif
#ifndef STID_T_H
#include <stid_t.h>
#endif

#ifdef __GNUG__
#pragma interface
#endif

class extid_t {
public:
    vid_t	vol; 
    fill2	filler; // because vol is 2 bytes & ext is 4
    extnum_t	ext;

    friend ostream& operator<<(ostream&, const extid_t& x);
};

#define LPID_T
class lpid_t {
public:
    stid_t	_stid;
    shpid_t	page;
    
    lpid_t();
    lpid_t(const stid_t& s, shpid_t p);
    lpid_t(vid_t v, snum_t s, shpid_t p);
    operator bool() const;

    vid_t	vol()   const {return _stid.vol;}
    snum_t	store() const {return _stid.store;}
    const stid_t& stid() const {return _stid;}

    bool	is_remote() const { return _stid.vol.is_remote(); }
    // necessary and sufficient conditions for
    // is_null() are determined by default constructor, q.v.
    bool	is_null() const { return page == 0; }

    bool operator==(const lpid_t& p) const;
    bool operator!=(const lpid_t& p) const;
    bool operator<(const lpid_t& p) const;
    bool operator<=(const lpid_t& p) const;
    bool operator>(const lpid_t& p) const;
    bool operator>=(const lpid_t& p) const;
    friend ostream& operator<<(ostream&, const lpid_t& p);
    friend istream& operator>>(istream&, lpid_t& p);

    static const lpid_t bof;
    static const lpid_t eof;
    static const lpid_t null;
};


// stpgid_t is used to identify a page or a store.  It is used
// where small stores are implemented as only a page within
// a special store on a volume;
class stpgid_t {
public:
    lpid_t lpid;
    stpgid_t();
    stpgid_t(const lpid_t& p);
    stpgid_t(const stid_t& s);
    stpgid_t(vid_t v, snum_t s, shpid_t p);

    bool	is_stid() const {return lpid.page == 0;}
    operator	stid_t() const;

    vid_t  	vol() const {return lpid.vol();}
    snum_t  	store() const {return lpid.store();}
    const stid_t& stid() const;

    bool operator==(const stpgid_t&) const;
    bool operator!=(const stpgid_t&) const;
};


class rid_t;

// SHort physical Record IDs.  
class shrid_t {
public:
    shpid_t	page;
    snum_t	store;
    slotid_t	slot;
    fill2	filler; // because page, snum_t are 4 bytes, slotid_t is 2

    shrid_t();
    shrid_t(const rid_t& r);
    shrid_t(shpid_t p, snum_t st, slotid_t sl) : page(p), store(st), slot(sl) {}
    friend ostream& operator<<(ostream&, const shrid_t& s);
    friend istream& operator>>(istream&, shrid_t& s);
};

// Store PID (ie. pid with no volume id)
// For now, only used by lid_m for logical ID index entries.
class spid_t {
public:
    shpid_t	page;
    snum_t	store;
    // fill2	filler; no longer needed

    spid_t() : page(0), store(0) {}
    spid_t(const lpid_t& p) : page(p.page), store(p.store()) {}
};

#define RID_T

// physical Record IDs
class rid_t {
public:
    lpid_t	pid;
    slotid_t	slot;
    fill2	filler;  // for initialization of last 2 unused bytes

    rid_t();
    rid_t(vid_t vid, const shrid_t& shrid);
    rid_t(const lpid_t& p, slotid_t s) : pid(p), slot(s) {};

    stid_t stid() const;

    bool operator==(const rid_t& r) const;
    bool operator!=(const rid_t& r) const;
    friend ostream& operator<<(ostream&, const rid_t& s);
    friend istream& operator>>(istream&, rid_t& s);

    static const rid_t null;
};

#define LSTID_T
class lstid_t : public lid_t {	// logical store ID
public:
    lstid_t() {};
	lstid_t( const lvid_t& lvid_, const serial_t& serial_) :
		lid_t(lvid_,serial_) {};

    lstid_t(uint4_t high, uint4_t low, uint4_t ser, bool remote) :
		lid_t(high, low, ser, remote) {};
};
typedef lstid_t lfid_t;		// logical file ID


/*
 * This is the SM's idea of on-disk and in-structure 
 * things that reflect the size of a "disk".  It
 * is different from fileoff_t because the two are
 * orthogonal.  A system may be constructed that
 * has big addresses, but is only running on 
 * a "small file" environment.
 */
#if defined(SM_DISKADDR_LARGE) && !defined(SM_ODS_COMPAT_13)
typedef	w_base_t::int8_t	sm_diskaddr_t;
const sm_diskaddr_t		sm_diskaddr_max = w_base_t::int8_max;
#else
typedef	w_base_t::int4_t	sm_diskaddr_t;
const sm_diskaddr_t		sm_diskaddr_max = w_base_t::int4_max;
#endif


#define LSN_T
class lsn_t {
#ifdef SM_DISKADDR_LARGE
private:
#endif
public:
#ifdef SM_DISKADDR_LARGE
     /* When _rba is an 8-byte and the log record is not
      * aligned (skip_log), we can't use the standard constructors
      * on Sparcs
      */
    lsn_t() : _file(0) { 
	memset(&_rba, '\0', sizeof(_rba)); 
    }
    lsn_t(uint4_t f, sm_diskaddr_t r) : _file(f) {
	memcpy(&_rba, &r, sizeof(_rba)); 
    }
#else
    lsn_t() : _file(0), _rba(0)   {}
    lsn_t(uint4_t f, sm_diskaddr_t r) : _file(f), _rba(r) {}
#endif

    lsn_t(const lsn_t& l);
    lsn_t& operator=(const lsn_t& l);

    uint4_t hi() const  { return _file; }

    bool i_am_aligned() const {
#ifdef SM_DISKADDR_LARGE
	char *p = (char *)alignon(&_rba, sizeof(sm_diskaddr_t));
        return ( p  == (char *)&_rba);
#else
	return true;
#endif
    }

    sm_diskaddr_t lo()	const  { 
	return rba();
    }

    sm_diskaddr_t  rba() const 
#ifndef SM_DISKADDR_LARGE
    {
        return _rba; 
    }
#else
    ; /* not inlined : defined in sm_s.cpp */
#endif

    void copy_rba(const lsn_t &other) 
#ifndef SM_DISKADDR_LARGE
    {
	_rba = other._rba;
    }
#else
    ; /* not inlined : defined in sm_s.cpp */
#endif

    void set_rba(sm_diskaddr_t &other) 
#ifndef SM_DISKADDR_LARGE
    {
	_rba = other;
    }
#else
    ; /* not inlined : defined in sm_s.cpp */
#endif 


    lsn_t& advance(int amt);
    void increment();

    // flaky: return # bytes of log space diff
    sm_diskaddr_t  operator-(const lsn_t &l) const;

    bool operator>(const lsn_t& l) const;
    bool operator<(const lsn_t& l) const;
    bool operator>=(const lsn_t& l) const;
    bool operator<=(const lsn_t& l) const;
    bool operator==(const lsn_t& l) const;
    bool operator!=(const lsn_t& l) const;

    operator bool() const;
    friend inline ostream& operator<<(ostream&, const lsn_t&);
    friend inline istream& operator>>(istream&, lsn_t&);

    enum { 
	file_hwm = max_int4
    };

    static const lsn_t null;
    static const lsn_t max;
    
private:
    uint4_t	_file;		// log file number in log directory
#if defined(SM_DISKADDR_LARGE)
    /* Have to align to 8-bytes for Sparcs, so we do it for
     * everything (gak) so that volumes can be moved from one
     * architecture to another
     */
    fill4	_fill;
#endif
    sm_diskaddr_t _rba;	        // relative byte address of (first
				// byte) record in file
};

struct key_type_s {
    enum type_t {
	i = 'i',		// integer (1,2,4)
	I = 'I',		// integer (1,2,4), compressed
	u = 'u',		// unsigned integer (1,2,4)
	U = 'U',		// unsigned integer (1,2,4), compressed
	f = 'f',		// float (4,8)
	F = 'F',		// float (4,8), compressed
	b = 'b',		// binary (uninterpreted) (vbl or fixed-len)
	B = 'B'			// compressed binary (uninterpreted) (vbl or fixed-len)
	// NB : u1==b1, u2==b2, u4==b4 semantically, 
	// BUT
	// u2, u4 must be  aligned, whereas b2, b4 need not be,
	// AND
	// u2, u4 may use faster comparisons than b2, b4, which will 
	// always use umemcmp (possibly not optimized). 
    };
    enum { max_len = 2000 };
    char	type;
    char	variable; // Boolean - but its size is unpredictable
    uint2_t	length;	
    char	compressed; // Boolean - but its size is unpredictable
#ifdef __GNUG__	/* XXX PURIFY_ZERO canidate? */
    fill1	dummy; 
#endif

    key_type_s(type_t t = (type_t)0, char v = 0, uint2_t l = 0) 
	: type((char) t), variable(v), length(l), compressed(false)  {};

    // This function parses a key descriptor string "s" and
    // translates it into an array of key_type_s, "kc".  The initial
    // length of the array is passed in through "count" and
    // the number of elements filled in "kc" is returned through
    // "count". 
    static w_rc_t parse_key_type(const char* s, uint4_t& count, key_type_s kc[]);
    static w_rc_t get_key_type(char* s, int buflen, uint4_t count, const key_type_s *kc);

};
#define null_lsn (lsn_t::null)
#define max_lsn  (lsn_t::max)

inline lsn_t::operator bool() const 
{ 
    return _file != 0; 
}

inline lsn_t::lsn_t(const lsn_t& l) : 
	_file(l._file)
#ifndef SM_DISKADDR_LARGE
	, _rba(l._rba) 
{ 
}
#else
{
    copy_rba(l);
}
#endif

inline lsn_t& lsn_t::operator=(const lsn_t& l)
{
    _file = l._file; 
#ifdef SM_DISKADDR_LARGE
    copy_rba(l);
#ifdef PURIFY_ZERO
    (void) new (&_fill) fill4; // get it initialized
#endif
#else
    _rba = l._rba; 
#endif
    return *this;
}

/* XXX what if overflows out of 0 ... diskaddr_max */
inline lsn_t& lsn_t::advance(int amt)
{
#ifdef SM_DISKADDR_LARGE
    if(i_am_aligned()) {
        _rba += amt;
    } else {
	sm_diskaddr_t d = rba();
        d += amt;
	set_rba(d);
    }
#else
    _rba += amt;
#endif
    return *this;
}


inline bool lsn_t::operator>(const lsn_t& l) const
{
    return _file > l._file || (_file == l._file && 
	rba() > l.rba()); 
}

inline bool lsn_t::operator<(const lsn_t& l) const
{
    return _file < l._file || (_file == l._file && 
	rba() < l.rba());
}

inline bool lsn_t::operator>=(const lsn_t& l) const
{
    return _file > l._file || (_file == l._file && 
	rba() >= l.rba());
}

inline bool lsn_t::operator<=(const lsn_t& l) const
{
    return _file < l._file || (_file == l._file && 
	rba() <= l.rba());
}

inline bool lsn_t::operator==(const lsn_t& l) const
{
    return _file == l._file && rba() == l.rba();
}

inline bool lsn_t::operator!=(const lsn_t& l) const
{
    return ! (*this == l);
}

inline ostream& operator<<(ostream& o, const lsn_t& l)
{
    return o << l._file << '.' << l.rba();
}

inline istream& operator>>(istream& i, lsn_t& l)
{
    sm_diskaddr_t d;
    char c;
    i >> l._file >> c >> d;
    l.set_rba(d);
    return i;
}

inline lpid_t::lpid_t() : page(0) {}

inline lpid_t::lpid_t(const stid_t& s, shpid_t p) : _stid(s), page(p)
{}

inline lpid_t::lpid_t(vid_t v, snum_t s, shpid_t p) :
	_stid(v, s), page(p)
{}


inline stpgid_t::stpgid_t()
{
}


inline stpgid_t::stpgid_t(const stid_t& s)
    : lpid(s, 0)
{
}


inline stpgid_t::stpgid_t(const lpid_t& p)
    : lpid(p) 
{
}


inline stpgid_t::stpgid_t(vid_t v, snum_t s, shpid_t p) 
    : lpid(v, s, p) 
{
}


inline stpgid_t::operator stid_t() const
{
    w_assert3(is_stid()); return lpid._stid;
}


inline const stid_t& stpgid_t::stid() const
{
    // This function is to extract the store id from the
    // stpgid... not to make sure it always IS a store id.
    // w_assert3(is_stid()); 
    //
    return lpid.stid();
}


inline shrid_t::shrid_t() : page(0), store(0), slot(0)
{}
inline shrid_t::shrid_t(const rid_t& r) :
	page(r.pid.page), store(r.pid.store()), slot(r.slot)
{}

inline rid_t::rid_t() : slot(0)
{}

inline rid_t::rid_t(vid_t vid, const shrid_t& shrid) :
	pid(vid, shrid.store, shrid.page), slot(shrid.slot)
{}

inline stid_t rid_t::stid() const
{
    return pid.stid();
}

inline bool lpid_t::operator==(const lpid_t& p) const
{
    return (page == p.page) && (stid() == p.stid());
}

inline bool lpid_t::operator!=(const lpid_t& p) const
{
    return !(*this == p);
}

inline bool lpid_t::operator<=(const lpid_t& p) const
{
    return _stid == p._stid && page <= p.page;
}

inline bool lpid_t::operator>=(const lpid_t& p) const
{
    return _stid == p._stid && page >= p.page;
}

inline u_long hash(const lpid_t& p)
{
    return p._stid.vol ^ (p.page + 113);
}

inline u_long hash(const vid_t v)
{
    return v;
}

inline bool stpgid_t::operator==(const stpgid_t& s) const
{
    return lpid == s.lpid;
}

inline bool stpgid_t::operator!=(const stpgid_t& s) const
{
    return lpid != s.lpid;
}


inline bool rid_t::operator==(const rid_t& r) const
{
    return (pid == r.pid && slot == r.slot);
}

inline bool rid_t::operator!=(const rid_t& r) const
{
    return !(*this == r);
}

/*<std-footer incl-file-exclusion='SM_S_H'>  -- do not edit anything below this line -- */

#endif          /*</std-footer>*/

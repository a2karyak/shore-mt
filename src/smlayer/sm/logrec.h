/*<std-header orig-src='shore' incl-file-exclusion='LOGREC_H'>

 $Id: logrec.h,v 1.61 1999/06/15 15:11:53 nhall Exp $

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

#ifndef LOGREC_H
#define LOGREC_H

#include "w_defines.h"

/*  -- do not edit anything above this line --   </std-header>*/

class nbox_t;
class rangeset_t;

#include "logfunc_gen.h"
#include "xct.h"

#ifdef __GNUG__
#pragma interface
#endif

class logrec_t {
public:
    friend void xct_t::give_logbuf(logrec_t*, const page_p *);
    NORET			W_FASTNEW_CLASS_DECL(logrec_t);

#include "logtype_gen.h"
    void 			fill(
	const lpid_t*		    pid,
	uint2_t			    tag,
	smsize_t		    length);
    void 			fill_xct_attr(
	const tid_t& 		    tid,
	const lsn_t& 		    last_lsn);
    bool 			is_page_update() const;
    bool 			is_redo() const;
    bool 			is_skip() const;
    bool 			is_undo() const;
    bool 			is_cpsn() const;
    bool 			is_undoable_clr() const;
    bool 			is_logical() const;
    bool			valid_header(const lsn_t & lsn_ck) const;

    void 			redo(page_p*);
    void 			undo(page_p*);

    enum {
	max_sz = 3 * sizeof(page_s),
	hdr_sz = (
#ifdef SM_ODS_COMPAT_13
		sizeof(uint2_t) +	// _len
		sizeof(u_char) +	// _type
		sizeof(u_char) +	// _cat
		sizeof(uint2_t) +	// _page_tag
		sizeof(fill2) +		// _filler
		sizeof(lpid_t) +	// _pid
		sizeof(tid_t) +		// _tid
		sizeof(lsn_t) +		// _prev
		// sizeof(lsn_t)	// _undo_nxt -- not used
#else
		sizeof(uint2_t) +   	// _len
		sizeof(u_char) +  	// _type
		sizeof(u_char) +  	//  _cat
		sizeof(tid_t) + 	// _tid
		// sizeof(lpid_t) + 	// _pid replaced by next 3:
		sizeof(shpid_t) +	// _shpid
		sizeof(vid_t) +		// _vid
		sizeof(uint2_t) + 	// _page_tag
		sizeof(snum_t) +	// _snum
		sizeof(lsn_t) + 	// _prev // ctns possibly 4 extra
					// bytes
		// sizeof(lsn_t)	// _undo_nxt -- not used
#endif
		0
		) 
	};
    enum {
	data_sz = max_sz - (hdr_sz + sizeof(lsn_t))
	};
    const tid_t& 		tid() const;
    const vid_t& 		vid() const;
#ifdef SM_ODS_COMPAT_13
    const lpid_t&	        shpid() const;	// fake everyone out
#else
    const shpid_t&	        shpid() const;
#endif
    // put construct_pid() here just to make sure we can
    // easily locate all non-private/non-protected uses of pid()
    lpid_t 		        construct_pid() const;
protected:
    lpid_t 		        pid() const;
private:
    void			set_pid(const lpid_t& p);
public:
    bool 		        null_pid() const; // needed in restart.cpp
    uint2_t			tag() const;
    smsize_t			length() const;
    const lsn_t&		undo_nxt() const;
    const lsn_t& 		prev() const;
    void 			set_clr(const lsn_t& c);
#ifdef UNDOABLE_CLRS
    void 			set_undoable_clr(const lsn_t& c);
#endif /* UNDOABLE_CLRS */
    kind_t 			type() const;
    const char* 		type_str() const;
    const char* 		cat_str() const;
    const char* 		data() const;
    const lsn_t& 		lsn_ck() const {  return *_lsn_ck(); }
    const lsn_t 		get_lsn_ck() const { 
	lsn_t	tmp = *_lsn_ck();
	return tmp;
    }
    void			set_lsn_ck(const lsn_t &lsn_ck) {
	// put lsn in last bytes of data
	lsn_t& where = *_lsn_ck();
	where = lsn_ck;
    }
    void			corrupt();

    friend ostream& operator<<(ostream&, const logrec_t&);

protected:
    enum category_t {
	t_bad_cat = 0,
	t_status = 01,
	t_undo = 02,
	t_redo = 04,
	t_logical = 010,
	    // Note: compensation records are not undo-able
	    // (ie. they compensate around themselves as well)
	    // So far this limitation has been fine.
	// old: t_cpsn = 020 | t_redo,
	t_cpsn = 020
    };
#ifdef SM_ODS_COMPAT_13
    uint2_t			_len;  // length of the log record
    u_char			_type; // kind_t (included from logtype_gen.h)
    u_char			_cat;  // category_t
    uint2_t			_page_tag;
    fill2			_filler;
    /* 8 */
    lpid_t			_pid;
    /* +12 = 20 */
    tid_t			_tid;
    /* 20 + 8 = 28 NOT 8-byte aligned! */
    // TODO : Call bolo to find out what he wants to do about this 
    lsn_t			_prev;
#else

    uint2_t			_len;  // length of the log record
    u_char			_type; // kind_t (included from logtype_gen.h)
    u_char			_cat;  // category_t
    /* 4 */

    tid_t			_tid;      // (xct)tid of this xct
    /* 4+8=12 */


    // Was _pid; broke down to save 2 bytes:
    // May be used ONLY in set_pid() and pid()
    // lpid_t			_pid;  // page on which action is performed
    shpid_t			_shpid; // 4 bytes
    /* 12 + 4=16 */
    vid_t			_vid;   // 2 bytes
    uint2_t                     _page_tag; // page_p::tag_t 2 bytes
    /* 16 + 4= 20 */
    snum_t			_snum; // 4 bytes
    /* 20 + 4= 24 */
    lsn_t			_prev;     // (xct)previous logrec of this xct
    /* With SM_DISKADDR_LARGE, size is 16, else size is 8 */
    /* 24+16/8 = 40/32 */
#endif

    
    // lsn_t			_undo_nxt; // (xct) used in CLR only
    /*
     * NB: you might think it would be nice to use one lsn_t for _prev and
     * for _undo_lsn, but for the moment we need both because
     * at the last minute, fill_xct_attr() is called and that fills in 
     * _prev, clobbering its value with the prior generated log record's lsn.
     * It so happens that set_clr() is called prior to fill_xct_attr().
     * It might do to set _prev iff it's not already set, in fill_xct_attr().
     */

    /* 
     * NOTE re sizeof header:
     * When diskaddr_t is 8 bytes, lsn_t is 16
     *
     */
    char			_data[data_sz + sizeof(lsn_t)];

    // The last sizeof(lsn_t) bytes of data are used for
    // recording the lsn.
    // No guarantee that it's aligned to 8 bytes.
    lsn_t*			_lsn_ck() const {
    	size_t lo_offset = _len - (hdr_sz + sizeof(lsn_t));
	lsn_t *where = (lsn_t*)(_data+lo_offset);
    	return where;
    }
};

/* for logging,  recovering and undoing extent alloc/dealloc:  */
class      ext_log_info_t {
public:
    extnum_t prev;  // order info
    extnum_t next;  // order info

    extnum_t ext; // 4 bytes
    Pmap_Align4	pmap;	// 4 bytes
    NORET ext_log_info_t() : 
	prev(0), 
	next(0),
	ext(0) {
    }
};


struct chkpt_bf_tab_t {
    struct brec_t {
	lpid_t	pid;
#ifdef SM_DISKADDR_LARGE
	fill4	fill; // for purify
#endif
	lsn_t	rec_lsn;
    };

    // max is set to make chkpt_bf_tab_t fit in logrec_t::data_sz
    enum { max = (logrec_t::data_sz - 2 * sizeof(uint4_t)) / sizeof(brec_t) };
    uint4_t  			count;
    fill4  			filler;
    brec_t 			brec[max];

    NORET			chkpt_bf_tab_t(
	int 			    cnt, 
	const lpid_t* 		    p, 
	const lsn_t* 		    l);
	
    int				size();
};

struct prepare_stores_to_free_t  {
    enum { max = (logrec_t::data_sz - sizeof(uint4_t)) / sizeof(stid_t) };
    uint4_t			num;
    stid_t			stids[max];

    prepare_stores_to_free_t(uint4_t theNum, const stid_t* theStids)
	: num(theNum)
	{
	    w_assert3(theNum <= max);
	    for (uint4_t i = 0; i < num; i++)
		stids[i] = theStids[i];
	};
    
    int size()  { return sizeof(uint4_t) + num * sizeof(stid_t); };
};

struct chkpt_xct_tab_t {
    struct xrec_t {
	tid_t 			    tid;
	lsn_t			    last_lsn;
	lsn_t			    undo_nxt;
	smlevel_1::xct_state_t	    state;
    };

    // max is set to make chkpt_xct_tab_t fit in logrec_t::data_sz
    enum { 	max = ((logrec_t::data_sz - sizeof(tid_t) -
			2 * sizeof(uint4_t)) / sizeof(xrec_t))
	};
    tid_t			youngest;	// maximum tid in session
    uint4_t			count;
    fill4			filler;
    xrec_t 			xrec[max];
    
    NORET			chkpt_xct_tab_t(
	const tid_t& 		    youngest,
	int 			    count,
	const tid_t* 		    tid,
	const smlevel_1::xct_state_t* state,
	const lsn_t* 		    last_lsn,
	const lsn_t* 		    undo_nxt);
    int 			size();
};

struct chkpt_dev_tab_t {
    struct devrec_t {
	char		dev_name[smlevel_0::max_devname+1];
	vid_t		vid;  // (won't be needed in future)
    };

    // max is set to make chkpt_dev_tab_t fit in logrec_t::data_sz
    enum { max = ((logrec_t::data_sz - 2*sizeof(uint4_t)) / sizeof(devrec_t))
	};
    uint4_t			count;
    fill4			filler;
    devrec_t 			devrec[max];
    
    NORET			chkpt_dev_tab_t(
	int 			    count,
	const char 		    **dev_name,
	const vid_t* 		    vid);
    int 			size();
};


/************************************************************************
 * Structures for prepare records
 *
 ***********************************************************************/
struct prepare_lock_totals_t {
	int	num_EX;
	int	num_IX;
	int	num_SIX;
	int	num_extents;
	fill4   filler; //for 8-byte alignment
	lsn_t	first_lsn;
	prepare_lock_totals_t(int a, int b, int c, int d, const lsn_t &l) :
		num_EX(a), num_IX(b), num_SIX(c), num_extents(d),
		first_lsn(l){ }
	int size() const 	// in bytes
	    { return 4 * sizeof(int) + sizeof(lsn_t) + sizeof(fill4); }
};
struct prepare_info_t {
	// don't use bool - its size changes with compilers
	char			   is_external;
	fill1			   dummy1;
	fill2			   dummy2;
	server_handle_t  	   h;
	gtid_t		   	   g;
	prepare_info_t(const gtid_t *_g, 
		const server_handle_t &_h) 
	{ 
#ifdef PURIFY_ZERO
            memset(&g, '\0', sizeof(g));
            memset(&h, '\0', sizeof(h));
#endif
	    if(_g) {
		is_external = 1; g = *_g;
	    } else is_external = 0;
	    h = _h; 
	}
	int size() const { 
		return sizeof(is_external) + 
		sizeof(dummy1) + sizeof(dummy2) +
		sizeof(server_handle_t) +
		(is_external? sizeof(gtid_t) :0);
	    }
};

struct prepare_lock_t {
	// -tid is stored in the log rec hdr
	// -all locks are long-term

	lock_mode_t	mode; // for this group of locks
	uint4_t 	num_locks; // in the array below
	enum            { max_locks_logged = (logrec_t::data_sz - sizeof(lock_mode_t) - sizeof(uint4_t)) / sizeof(lockid_t) };

	lockid_t	name[max_locks_logged];

	prepare_lock_t(uint4_t num, lock_base_t::mode_t _mode, 
		lockid_t *locks){
		num_locks = num;
		mode =  _mode;
		uint4_t i;
		for(i=0; i<num; i++) { name[i]=locks[i]; }
	}
	int size() const 	// in bytes
		{ 
		    w_assert3(((num_locks * sizeof(lockid_t)) 
			+ sizeof(mode) + sizeof(num_locks)) <=
			logrec_t::data_sz); 
		    return (num_locks * sizeof(lockid_t)) 
			+ sizeof(mode) + sizeof(num_locks); 
		}
};

struct prepare_all_lock_t {
	// -tid is stored in the log rec hdr
	// -all locks are long-term
	// 
	struct LockAndModePair {
	    lockid_t	name;
	    lock_mode_t	mode; // for this lock
	};

	uint4_t 			num_locks; // in the array below
	enum            { max_locks_logged = (logrec_t::data_sz - sizeof(uint4_t)) / sizeof(LockAndModePair) };

	LockAndModePair pair[max_locks_logged];


	prepare_all_lock_t(uint4_t num, 
		lockid_t *locks,
		lock_mode_t *modes
		){
		num_locks = num;
		uint4_t i;
		for(i=0; i<num; i++) { pair[i].name=locks[i]; pair[i].mode = modes[i]; }
	}
	int size() const 	// in bytes
		{ return num_locks * sizeof(pair[0]) + sizeof(num_locks); }
};

inline const tid_t&
logrec_t::tid() const
{
    return _tid;
}

#ifdef SM_ODS_COMPAT_13
inline const lpid_t& logrec_t::shpid() const
{
	return _pid;
}
#else
inline const shpid_t&
logrec_t::shpid() const
{
    return _shpid;
}
#endif

inline const vid_t&
logrec_t::vid() const
{
#ifdef SM_ODS_COMPAT_13
    return _pid._stid.vol;
#else
    return _vid;
#endif
}

inline lpid_t
logrec_t::pid() const
{
#ifdef SM_ODS_COMPAT_13
    return _pid;
#else
    return lpid_t(_vid, _snum, _shpid);
#endif
}

inline lpid_t
logrec_t::construct_pid() const
{
#ifdef SM_ODS_COMPAT_13
    return _pid;
#else
// public version of pid(), renamed for grepping 
    return lpid_t(_vid, _snum, _shpid);
#endif
}

inline void
logrec_t::set_pid(const lpid_t& p)
{
#ifdef SM_ODS_COMPAT_13
    _pid = p;
#else
    _shpid = p.page;
    _vid = p.vol();
    _snum = p.store();
#endif
}

inline bool 
logrec_t::null_pid() const
{
    // see lpid_t::is_null() for necessary and 
    // sufficient conditions
#ifdef SM_ODS_COMPAT_13
    bool result = _pid == lpid_t::null;
#else
    bool result = (_shpid == 0);
    w_assert3(result == (pid().is_null())); 
#endif
    return result;
}

inline uint2_t
logrec_t::tag() const
{
    return _page_tag;
}

inline smsize_t
logrec_t::length() const
{
    return _len;
}

inline const lsn_t&
logrec_t::undo_nxt() const
{
    // To shrink log records,
    // we've taken out _undo_nxt and 
    // overloaded _prev.
    // return _undo_nxt;
    return _prev;
}

inline const lsn_t&
logrec_t::prev() const
{
    return _prev;
}

inline logrec_t::kind_t
logrec_t::type() const
{
    return (kind_t) _type;
}

inline const char* 
logrec_t::data() const
{
    return _data;
}

inline void 
logrec_t::set_clr(const lsn_t& c)
{
    _cat &= ~t_undo; // can't undo compensated
		     // log records, whatever kind they might be
		     // except for special case below
    _cat |= t_cpsn;

    // To shrink log records,
    // we've taken out _undo_nxt and 
    // overloaded _prev.
    // _undo_nxt = c;
    _prev = c;
}

inline bool 
logrec_t::is_undoable_clr() const
{
    return false;
    // DISABLED -- see comment below
    // return (_cat & (t_cpsn|t_undo)) == (t_cpsn|t_undo);
}

#ifdef UNDOABLE_CLRS
/* 
 * NB: In order to allow piggybacking of compensations onto log records,
 * while avoiding keeping _last_log buffered in the xct_t, and
 * therefore keeping _last_modified_page latched, we disabled
 * undoable_clr records. At the time we did this, noone needed
 * undoable clrs anyway.  It was needed by a now-obsolete implementation
 * of the volume layer.
 */
inline void 
logrec_t::set_undoable_clr(const lsn_t& ) // arg was named c for _undo_nxt
{
    // DISABLED! 
    w_assert3(0);
    /*
    // special case!!!!!!!!!!! for logical extent logging recs
    w_assert3(_cat & t_logical);
    _cat |= t_cpsn;
    _undo_nxt = c;
    */
}
#endif /* UNDOABLE_CLRS */

inline bool 
logrec_t::is_redo() const
{
    return (_cat & t_redo) != 0;
}

inline bool
logrec_t::is_skip() const
{
    return type() == t_skip;
}


inline bool
logrec_t::is_undo() const
{
    return (_cat & t_undo) != 0;
}


inline bool 
logrec_t::is_cpsn() const
{
    return (_cat & t_cpsn) != 0;
}

inline bool 
logrec_t::is_page_update() const
{
    // old: return is_redo() && ! is_cpsn();
    return is_redo() && !is_cpsn() && (!null_pid());
}

inline bool 
logrec_t::is_logical() const
{
    return (_cat & t_logical) != 0;
}

inline int
chkpt_bf_tab_t::size()
{
    return (char*) &brec[count] - (char*) this;
}

inline int
chkpt_xct_tab_t::size()
{
    return (char*) &xrec[count] - (char*) this; 
}

inline int
chkpt_dev_tab_t::size()
{
    return (char*) &devrec[count] - (char*) this; 
}

/*<std-footer incl-file-exclusion='LOGREC_H'>  -- do not edit anything below this line -- */

#endif          /*</std-footer>*/

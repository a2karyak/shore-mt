/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

/*
 *  $Id: remote_msg.h,v 1.18 1996/02/28 22:13:32 markos Exp $
 */
#ifndef REMOTE_MSG_H
#define REMOTE_MSG_H

#ifdef __GNUG__
#pragma interface
#endif

// This file declares the messages structures sent between SHORE SM
// servers.  This file is included by remote.h inside class remote_m,
// but is separate since it is only needed by remote.c.


public:

typedef int portid_t;

enum {max_msg_size = 4096};

private:

class msg_hdr_t {
public: 
    msg_hdr_t() : _type(bad_msg), _purged_list_size(0), _error_code(eNOERROR) {}

    msg_hdr_t(msg_type_t t) : _type(t),
		  _purged_list_size(0), _error_code(eNOERROR) {}

    rc_t	error() const
			{ return w_rc_t(__FILE__, __LINE__, _error_code); }

    void	set_error(const rc_t& rc)	{ _error_code = rc.err_num();}
    msg_type_t	type() const			{ return _type; }
    void	set_type(msg_type_t t)	 	{ _type = t; }
    void	set_purged_list_size(int sz)	{ _purged_list_size = sz;}
    void	incr_purged_list_size(int sz)	{ _purged_list_size += sz; }
    int		get_purged_list_size() const	{ return _purged_list_size; }

    int size() const {
        switch (_type) {
        case connect_msg:
	case shutdown_conn_msg:
            return align(sizeof(msg_connect_t));
        case mount_volume_msg:
            return align(sizeof(msg_mount_t));
        case spread_xct_msg:
	case prepare_xct_msg:
	case commit_xct_msg:
	case abort_xct_msg:
            return align(sizeof(msg_xct_t));
	case log_msg:
	    smsize_t size = ((msg_log_t*) this)->size;
	    w_assert3(::is_aligned(size));
	    return size;
	case pids_msg:
	    return align(sizeof(msg_pids_t)) +
		   ((msg_pids_t*) this)->nfound * sizeof(shpid_t);
        case lookup_lid_msg:
            return align(sizeof(msg_lid_t));
        case acquire_lock_msg:
            return  align(sizeof(msg_lock_t));
        case read_page_msg:
	    if (((msg_page_t*) this)->recmap_included)
		return  align(sizeof(msg_page_t)) + smlevel_0::recmap_sz;
	    else
		return  align(sizeof(msg_page_t));
	case callback_msg:
	    return align(sizeof(msg_callback_t)) +
		   ((msg_callback_t*) this)->num_blockers * sizeof(blocker_t);
	case abort_callback_msg:
	    return align(sizeof(msg_abort_callback_t));
	case adaptive_msg:
	    w_assert1(w_base_t::is_aligned(sizeof(adaptive_xct_t)));
	    return align(sizeof(msg_adaptive_t)) +
		   sizeof(adaptive_xct_t) * ((msg_adaptive_t*)this)->nxacts;
	case purged_pages_msg:
	    return align(sizeof(msg_purged_pages_t));
	case ping_xct_msg:
	    return align(sizeof(msg_ping_xct_t));

#ifdef SHIV
// ****************** SHIV **********************
	case send_page_msg:
	    return align(sizeof(msg_send_t));
        case read_send_page_msg:
	    return  align(sizeof(msg_read_send_page_t));
	case fwd_page_msg:
	    return  align(sizeof(msg_fwd_page_t));
// ***************** SHIV ***********************	    
#endif /* SHIV */
        default:
            W_FATAL(eINTERNAL);
        }
	W_FATAL(eINTERNAL); // should never reach here
	return 0;
    }

private: 
    msg_type_t		_type;
    int			_purged_list_size;
    w_base_t::int4_t	_error_code;
};


class msg_connect_t : public msg_hdr_t {
public:
    msg_connect_t() {}
    msg_connect_t(msg_type_t t) : msg_hdr_t(t) {}
};


// Message for mount/dismount of volumes
class msg_mount_t : public msg_hdr_t {
public:
    msg_mount_t(msg_type_t t, const lvid_t& l) :
		msg_hdr_t(t), lvid(l), local_vid(0), dir_root(lpid_t::null) { }

    lvid_t	lvid;		// volume to mount/unmount
    vid_t 	local_vid;	// vid on owning server
    lpid_t	dir_root;	// root page of the volume directory index
};


class msg_xct_t : public msg_hdr_t {
public:
    msg_xct_t(msg_type_t t) : msg_hdr_t(t), trace_level(0) {}
    
    int         trace_level;
    tid_t	tid;		// in the reply it carries the remote tid
};


class msg_log_t : public msg_hdr_t {
public:
    msg_log_t() : msg_hdr_t(log_msg),
		  size(align(sizeof(msg_log_t))), nlogs(0), 
		  commit(FALSE), commit_tid(tid_t::null), last(FALSE) {}

    uint4       size;		// total size of msg (including size of *this)
    uint2       nlogs;		// # of logrecs packed into this msg
    bool	commit;		// are the logrecs flushed by a committing xct?
    tid_t	commit_tid;	// tid of committing xact (if "commit" is true)
    bool	last;		// is this the last msg carrying logrecs from
				// the same log flush?
};


/*
 * Message for getting pids for pages belonging to a store that
 * is being scanned remotelly.
 */
class msg_pids_t : public msg_hdr_t {
public:
    enum { max_pids = 1000 };

    msg_pids_t(stid_t& s, bool lk, bool last, shpid_t f, int n) :
	msg_hdr_t(pids_msg), stid(s), lock(lk), last_only(last),
	first(f), num(n), nfound(0)
    {
	w_assert3(!last_only || num == 1);
	w_assert1(num <= max_pids);
    }

    stid_t      stid;
    bool	lock;		// if true, lock the pages in IS mode to
				// prevent their de-allocation
    bool	last_only;	// get the pid of the last store page only
    shpid_t     first;		// get the pids of the "num" pages following
				// after "first" (if 0 then start from the
				// first page of the  store)
    uint4       num;		// # of pids asked
    uint4       nfound;		// # of pids returned
};


class msg_lid_t : public msg_hdr_t {
public:
    msg_lid_t(msg_type_t t, const lid_t& l) : msg_hdr_t(t), lid(l) {}

    lid_t		lid;
    lid_m::lid_entry_t	lid_entry;
};


class msg_lock_t : public msg_hdr_t {
public:
    msg_lock_t(msg_type_t type, const lockid_t& n, lock_mode_t m,
					lock_duration_t d, long t, bool f) :
		msg_hdr_t(type), name(n), mode(m), duration(d),
		timeout(t), force(f) {}

    lockid_t		name;
    lock_mode_t		mode;
    lock_duration_t	duration;
    long		timeout;
    bool		force;
    bool		adaptive;
#ifdef DEBUG
    int value;
#endif
};


class msg_page_t : public msg_hdr_t {
public:
    msg_page_t(msg_type_t t) : msg_hdr_t(t) {}

    lockid_t            name;
    uint2		page_tag;
    lock_mode_t         mode;
    bool		detect_race;
    bool		parallel;
    bool		recmap_included;
// ********** SHIV ******************
    bool              forwarded;
    portid_t            fwd_port;
// ********** SHIV ******************    
};


class msg_callback_t : public msg_hdr_t {
public:
    msg_callback_t(
	msg_type_t		type,
	long			id,
	const lockid_t&		n,
	lock_mode_t		m,
	bool			pdirty,
	const srvid_t&		cbsrv,
	const tid_t&		remotetid,
	const tid_t& 		mastertid,
	int			trace):
	    msg_hdr_t(type), cb_id(id), name(n), mode(m), 
	    page_dirty(pdirty), cb_srv(cbsrv),
	    remote_tid(remotetid), master_tid(mastertid), cb_tid(tid_t::null),
	    outcome(callback_op_t::NONE), num_blockers(0), trace_level(trace) {}

    long		cb_id;		// for debugging
    lockid_t		name;		// id of data to be called back
    lock_mode_t		mode;		// mode to lock "name" in at client
    bool		page_dirty;	// has the page been updated by the
					// calling back or other xacts already?.
					// If yes, then we don't need to check
					// for page SH locks during record cb
    srvid_t             cb_srv;         // id of server where the cb is sent
    tid_t		remote_tid;	// id of remote proxy xct
    tid_t		master_tid;	// id of master proxy xct
    tid_t		cb_tid;		// id of callback proxy (set in reply)
    cb_outcome_t	outcome;	// set in reply
    uint2		num_blockers;	// num of xacts the cb had to wait for
    uint2		block_level;	// whether blocked on page or record
					// (for record level locking only)
    int			trace_level;
};


class msg_abort_callback_t :  public msg_hdr_t {
public:
    msg_abort_callback_t(
	msg_type_t		type,
	long			id,
	const lockid_t&		n,
	const srvid_t&		cbsrv,
	const tid_t&		remotetid,
	const tid_t&		cbtid):
		msg_hdr_t(type), cb_id(id), name(n),
		cb_srv(cbsrv), remote_tid(remotetid), cb_tid(cbtid) {}

    long		cb_id;		// for debugging
    lockid_t		name;
    srvid_t		cb_srv;
    tid_t		remote_tid;
    tid_t		cb_tid;
};


class msg_adaptive_t : public msg_hdr_t {
public:
    NORET	msg_adaptive_t(const lpid_t& p, int n) :
				msg_hdr_t(adaptive_msg), pid(p), nxacts(n) {}

    long	cb_id;
    lpid_t	pid;
    int		nxacts;
};


class msg_purged_pages_t : public msg_hdr_t {
public:
    msg_purged_pages_t(msg_type_t type) : msg_hdr_t(type) {}
};


class msg_ping_xct_t : public msg_hdr_t {
public:
    msg_ping_xct_t(msg_type_t type) : msg_hdr_t(type) {}
};



#ifdef SHIV

// ************ SHIV ********************
class msg_send_t : public msg_hdr_t {
public:
    msg_send_t(msg_type_t t)
		: msg_hdr_t(t) {}
    lpid_t	 pid;
    bool       found;
    bool       discard_only;
    portid_t	 send_port() {return _port;}
    portid_t     set_port(const node_port_t& p) {_port = p.portid();}
private:
    portid_t     _port;
};


class msg_read_send_page_t : public msg_hdr_t {
public:
    msg_read_send_page_t(msg_type_t t)
		: msg_hdr_t(t) {}
    lpid_t		read_pid;
    lock_mode_t         mode;
    bool		detect_race;
    lpid_t		send_pid;
    bool              send_back;
    bool              found;
    bool              forwarded;
    portid_t            fwd_port;        
};


class msg_fwd_page_t : public msg_hdr_t {
public:
    msg_fwd_page_t(msg_type_t t)
		: msg_hdr_t(t) {}
    lpid_t		pid;
    portid_t            fwd_port;
    portid_t            fwd_nport;
    Address             fwd_addr;
};
// ***************** SHIV ******************
#endif /* SHIV */


#endif /* REMOTE_MSG_H */


/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

/*
 *  $Id: remote_s.h,v 1.14 1996/02/28 22:13:33 markos Exp $
 */
#ifndef REMOTE_S_H
#define REMOTE_S_H

#ifdef __GNUG__
#pragma interface
#endif

#ifndef MULTI_SERVER

// define this for xct_t when MULTI_SERVER not defined
class master_xct_proxy_t {
public:
    static uint4 link_offset() {return offsetof(master_xct_proxy_t, _link);}
private:
    w_link_t	_link;  	// for list of proxies for a xct_t
};

#else /* MULTI_SERVER is defined */

#include "hash_lru.h"

class rxct_thread_t;
class cbxct_thread_t;
class logbuf_t;

#include "srvid_t.h"


//
// remote volume information
//
class rvol_t {
public:
    rvol_t() : _owner(srvid_t::null) {}
    void	init(vid_t alias, vid_t owner_vid, srvid_t owner)
		{_alias_vid = alias; _owner_vid = owner_vid; _owner = owner;}
    srvid_t	owner() const { return _owner; }
    vid_t	owner_vid() const { return _owner_vid; }
    vid_t	alias_vid() const { return _alias_vid; }

// private:
    w_link_t	_inv_link;
    vid_t	_alias_vid;	// local alias for this volume id
    vid_t	_owner_vid;	// vid on owner server
    srvid_t	_owner;		// owner server for this volume
};


struct global_tid_t {
    global_tid_t(tid_t t, const Endpoint& n) : tid(t), server(n) {}
    tid_t	tid;
    Endpoint	server; // master server for the xct

    bool operator==(const global_tid_t& g) const {
	return tid == g.tid && server == g.server;
    }

    global_tid_t operator=(global_tid_t& g){
        g.tid = tid; g.server = server;
        return *this;
    }

    friend inline ostream& operator<<(ostream& o, const global_tid_t&);
};

inline ostream& operator<<(ostream& o, const global_tid_t& l)
{
   o << l.tid << ":";
   return l.server.print(o);
}


/*
 * When a transaction on a local server spreads to a remote
 * server, an master_xct_proxy_t object is created on the local
 * server (and linked in a list on xct_t).  This master_xct_proxy_t
 * object maintains information about how to access the remote server.
 */
class master_xct_proxy_t {
public:

    master_xct_proxy_t(xct_t* x, const srvid_t& s);
    ~master_xct_proxy_t();

    const Endpoint&	req_port() const	{return _req_to_port;}	
    const Endpoint&	reply_port() const	{return _reply_from_port;}	
    xct_t*		xct() const		{return _xct;}	
    srvid_t		srvid() const		{return _srvid;}
    Buffer*		req_buf()		{ return _req_to_buf; }
    tid_t		remote_tid() const	{return _remote_tid;}
    //void		set_remote_tid(tid_t t)	{_remote_tid = t;}

    lsn_t		first_lsn()		{ return _first_lsn; }
    lsn_t		last_lsn()		{ return _last_lsn; }
    w_list_t<logbuf_t>&	logs()			{ return _logs; }

    void		set_first_lsn(const lsn_t& l)	{ _first_lsn = l; }
    void		set_last_lsn(const lsn_t& l)	{ _last_lsn = l; }

    void		init(const tid_t& remote_tid, const Endpoint& rp) {
		_remote_tid = remote_tid; _req_to_port = rp;
    }
    static uint4	 link_offset() {
		return offsetof(master_xct_proxy_t, _link);
    }

#ifdef SENDBACK
    Endpoint  fwd_port;    //port to which forwarded pages are sent
#endif    

private:
    xct_t*		_xct;		// controlling xct
    srvid_t		_srvid;		// remote server
    tid_t		_remote_tid;	// tid on remote site
    w_link_t		_link;  	// for list of proxies for a xct_t

    lsn_t		_first_lsn;
    lsn_t		_last_lsn;
    w_list_t<logbuf_t>	_logs;		// remote logrecs which are still
					// locally cached

    Endpoint		_req_to_port;		// send requests here 	(remote)
    const Endpoint&	_reply_from_port;	// receive replies here	(local)

    Buffer*		_req_to_buf;	// for sending reqs & receiving replies.
};


/*
 * When a transaction on a local server spreads to a remote
 * server, an remote_xct_proxy_t object is created on the remote
 * server (and linked in a list on xct_t).  This remote_xct_proxy_t
 * is a proxy for the transaction at the local server.
 */
class remote_xct_proxy_t {
public:

    remote_xct_proxy_t(tid_t master_tid, srvid_t master_server, const Endpoint& reply_port);
    ~remote_xct_proxy_t();

    xct_t*		xct() const		{ return _xct; }
    void		set_xct(xct_t* x)	{ _xct = x; }
    tid_t		master_tid() const	{ return _tid_on_master; }
    srvid_t		server() const		{ return _server; }
    Buffer*		req_buf() const		{ return _req_from_buf; }
    const Endpoint&	req_port() const	{ return _req_from_port; }
    const Endpoint&	reply_port() const	{ return _reply_to_port; }
    rxct_thread_t*	thread()		{ return _req_thread; }
    void		set_thread(rxct_thread_t* t)	{ _req_thread = t; }

private:
    xct_t*			_xct;		// controlling xct
    srvid_t			_server;	// master server
    tid_t			_tid_on_master;	// tid on master site
    //w_link_t			_link;  	// for list of proxies for a xct

    const Endpoint&		_req_from_port; // receive requests here (local)
    Endpoint			_reply_to_port; // send replies here	(remote)

    Buffer*			_req_from_buf;

    rxct_thread_t*		_req_thread;
};


class callback_xct_proxy_t {
public:
    callback_xct_proxy_t(long id, const lockid_t& n, lock_mode_t m, bool pdirty,
			const srvid_t& remsrv, const Endpoint& mastersrv,
			const tid_t& remtid, const tid_t& mastertid,
			Endpoint reply_port);

    ~callback_xct_proxy_t();

    global_tid_t	global_id() {
	return global_tid_t(_master_tid, _master_server); 
    }

    xct_t*		xct() const		{ return _xct; }
    void		set_xct(xct_t* x)	{ _xct = x; }
    const lockid_t&	name() const		{ return _name; }
    lock_mode_t		mode() const		{ return _mode; }

#ifdef HIER_CB
    bool		page_dirty() const	{ return _page_dirty; }
#endif /* HIER_CB */

    const Endpoint&	reply_port() const	{ return _reply_to_port; }
    Buffer*		reply_buf() const	{ return _reply_buf; }
    srvid_t		remote_server() const	{ return _remote_server; }
    tid_t		remote_tid()		{ return _remote_tid; }
    cbxct_thread_t*     get_thread() const      { return _cb_thread; }
    void                set_thread(cbxct_thread_t* cb) { _cb_thread = cb; }

    long                cb_id;          // for debugging

private:
    xct_t*		_xct;		// controlling xct
    srvid_t		_remote_server;	// remote proxy server
    Endpoint		_master_server;	// master proxy server
    tid_t		_remote_tid;	// tid of remote proxy xct
    tid_t		_master_tid;	// tid of master proxy xct
    lockid_t		_name;		// id of data (volume or file or page
					// or record) to callback.
    lock_mode_t		_mode;

#ifdef HIER_CB
    bool		_page_dirty;	// has the page been updated by the
					// calling back or other xacts already?.
					// If yes, then we don't need to check
					// for page SH locks during record cb
#endif /* HIER_CB */

    Endpoint		_reply_to_port;	// send replies here       (remote)
    Buffer*		_reply_buf;	// buffer to build the reply msg in

    // Note: the only request that a remote proxy may send to a callback proxy
    // is an abort request. But the callback proxy will be unable to service 
    // request since it will probably be blocked. So, the abort req is sent as
    // a 'top-level' req to the server request port.

    cbxct_thread_t*	_cb_thread;
};


struct callback_t;
struct blocker_t;

//
// Info to keep track of a "callback operation".
// A callback operation is the operation to callback a page (or file, or volume,
// or record) from every remote server where it is cached.
//
class callback_op_t {
public:
    enum cb_outcome_t {
	NONE=0, PINV=1, OINV=2, BLOCKED=4, DEADLOCK=8,
	KILLED=16, LOCAL_DEADLOCK = 32
    };

    NORET       callback_op_t();
    NORET       ~callback_op_t();
    void        clear();

    long		cb_id;		// for debugging
    lockid_t            name;           // id of the data being called back
    lock_mode_t		mode;		// mode to lock "name" in at client

#ifdef HIER_CB
    bool		page_dirty;	// is the page already updated by the
					// calling back, or other, xact?
    bool		myreq_pending;	// is the page already updated by the
					// calling back xact (TRUE if no)
#endif /* HIER_CB */

    uint2               outcome;
    bool		blocked;
    bool		local_deadlock;
    uint2               num_no_replies;

    w_list_t<callback_t> callbacks;             // outstanding callback reqs.

#ifdef OBJECT_CC
    uint2               page_blockers;
#endif

    const Endpoint&	reply_from_port;	// local : replies from callback
                                                // proxies are received here.
    Buffer*             reply_from_buf;
};


//
// callback_t struct keeps info about an outstanding callback request.
//
struct callback_t {
public:
    NORET       callback_t(const srvid_t& srv, int races);
    NORET	~callback_t() { link.detach(); }

    w_link_t            link;
    srvid_t             server;         // server where the callback req is sent
    tid_t               cb_tid;         // tid of the associated callback proxy.
    uint2               outcome;

    uint2		read_races;	// read race counter given by cache_m

#ifdef OBJECT_CC
    bool                page_blocked;   // was rec cb blocked at page level?
#endif

};


struct blocker_t {
public:
    tid_t		tid;
    uint4		mode;
    fill4		filler;
};


/*
 * Info included inside an adaptive callback msg
 */
struct adaptive_xct_t {
    tid_t       tid;
    u_char      recs[smlevel_0::recmap_sz];
};

#endif /*!MULTI_SERVER*/

#endif /* REMOTE_S_H */

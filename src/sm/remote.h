/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

/*
 *  $Id: remote.h,v 1.27 1996/05/03 04:07:49 kupsch Exp $
 */
#ifndef REMOTE_H
#define REMOTE_H

#ifdef __GNUG__
#pragma interface
#endif


class Buffer;
class Endpoint;
class rxct_thread_t;
class adaptive_xct_t;

typedef lid_m::lid_entry_t remote_lid_entry_t;

#ifndef MULTI_SERVER

class remote_m : public smlevel_4 {
    friend remote_client_m;
public:
    remote_m() {}
    ~remote_m() {}
    static rc_t register_volume(const lvid_t& /*lvid*/)
		{W_FATAL(eINTERNAL); return RCOK;}
    static rc_t mount_volume(const lvid_t& /*lvid*/)
		{W_FATAL(eINTERNAL); return RCOK;}
    static rc_t spread_xct(const vid_t& /*vid*/)
		{W_FATAL(eINTERNAL); return RCOK;}
    static rc_t commit_xct(xct_t& /*xct*/)
		{W_FATAL(eINTERNAL); return RCOK;}
    static rc_t abort_xct(xct_t& /*xct*/)
		{W_FATAL(eINTERNAL); return RCOK;}
    static rc_t lookup_lid(vid_t /*vid*/, const lid_t& /*lid*/, remote_lid_entry_t& /*entry*/) 
		{W_FATAL(eINTERNAL); return RCOK;}
    static rc_t acquire_lock(const lockid_t& /*n*/, lock_mode_t /*m*/,
			lock_duration_t /*d*/, long /*timeout*/, bool /*force*/, int* /*value*/)
		{W_FATAL(eINTERNAL); return RCOK;}
    static rc_t read_page(const lockid_t& /*name*/, bfcb_t* /*b*/, uint2 /*ptag*/,
			bool /*detect_race*/, bool /*parallel*/, bool /*is_new*/)
       		{W_FATAL(eINTERNAL); return RCOK;}
    static rc_t send_page(const lpid_t& /*pid*/, page_s& /*buf*/)
		{W_FATAL(eINTERNAL); return RCOK;}    
    static rc_t register_purged_page(const lpid_t& /*pid*/)
		{W_FATAL(eINTERNAL); return RCOK;}
};

#else /* MULTI_SERVER is defined */

#include "hash_lru.h"

typedef callback_op_t::cb_outcome_t cb_outcome_t;



//
// Information about local volumes exported to a remote server
// Struct rexport_t should be nested in server_t, but can't
// due to a limitation of HP CC when instatiating templates
// with nested classes.
//
struct rexport_t {
    rexport_t(vid_t v) : vid(v) {};
    ~rexport_t() { hash_link.detach(); }

    vid_t	vid;
    w_link_t	hash_link;
};


//
// pid of remote page which was cached in this server but has been purged
// recently. This info is piggybacked to the next msg going to the server
// who owns the page, so that the copy table of that server is updated.
//
struct purged_page_t {
    purged_page_t(const lpid_t& p): pid(p) {};
    ~purged_page_t() {link.detach();};
    w_link_t		link;
    lpid_t		pid;
};


class server_req_thread_t;
class log_thread_t;
class log_cache_m;


//
// Elements of the server table
//
struct server_s {
    enum { max_servers = smlevel_0::max_servers };

    server_s();
    ~server_s();
    void init(const srvid_t& id, const Endpoint& connect_at);
    void clear();
    void free_buffer(Buffer*& b);

    bool      in_use() const {
        return connected_from || vol_mount_cnt > 0 || xct_cnt > 0;
    }

    smutex_t	req_mutex;

    srvid_t	serverid;	// server id; local alias
    Endpoint	conn_port;      // connection port of remote server (remote)
    bool	connected_to;	// we have requested connect to server
    bool	connected_from;	// we have received connection req from server

    Endpoint	req_to_port;	// send requests here 		(remote)
    Endpoint	req_from_port;	// receive requests here	(local)
    Endpoint	reply_to_port;	// send replies here		(remote)
    Endpoint	reply_from_port; // receive replies here	(local)

    Buffer*	req_out_buf;	// for making rpc's and receiving reply
    Buffer*	req_in_buf;	// for receiving rpc's and responding

    int		vol_mount_cnt;	// # of volumes mounted from server_t
    int		xct_cnt;	// # of transactions spread to server_t
				// if TRUE, we must remain connected
				// even if vol/xct counts are 0

    // thread managing reqs from this server.
    server_req_thread_t*	req_thread;

    // list of local volumes exported to this server 
    w_hash_t<rexport_t, vid_t>*	export_tab;


    smutex_t			msg_mutex;
    // list of purged pages for this server
    w_list_t<purged_page_t>*	purged_pages;


    smutex_t		log_mutex;
    smutex_t            flush_mutex;		// serialize log flushes.
    log_cache_m*	log_cache;

    Endpoint		log_to_port;		// remote: send logs here
    Endpoint		log_from_port;		// local: receive logs here
    Endpoint            log_reply_in_port;	// local: receive reply here
    Endpoint		log_reply_out_port;	// remote: send reply here

    Buffer*		log_out_buf;	// for sending logs and receiving reply
    Buffer*		log_in_buf;     // for receiving logs and sending reply

    // thread to receive and redo logs from the remote server.
    log_thread_t*		log_thread;

    friend ostream& operator<<(ostream&, const server_s& srv);
};


enum srv_access_type_t {NONE, REQ, MSG, LOG, FLUSH_LOG};


//
// Handle for server_s.  This handle guarantees that the
// server_s is properly pinned (much as a page_p manages a page_s).
//
class server_h : public smlevel_0 {
public:
    server_h() : _srv(NULL), _access(NONE) {}
    ~server_h();

    // get access to server
    rc_t 	connect(const Endpoint& conn_port, srvid_t& srvid);
    rc_t 	fix(srvid_t srvid, srv_access_type_t access);
    void 	unfix();

    //rc_t 	init(srvid_t srvid);
    bool	in_use() const { return _srv->in_use(); }

    smutex_t&		msg_mutex()		{return _srv->msg_mutex;}

    w_list_t<purged_page_t>& purge_pages_list() {
	 return *(_srv->purged_pages);
    }

    void register_purged_page(purged_page_t* page) {
	_srv->purged_pages->push(page);
    }

    smutex_t&		log_mutex()		{return _srv->log_mutex;}
    log_cache_m&	log_cache()		{return *(_srv->log_cache);}
    const Endpoint&	log_to_port()		{return _srv->log_to_port;}
    const Endpoint&	log_reply_in_port()	{return _srv->log_reply_in_port;}
    Buffer*		log_out_buf()		{ return _srv->log_out_buf; }

    void        clear()                         {_srv->clear();}
    bool      fixed()                         {return _srv && _access!=NONE;}
    bool	is_connected_to() const		{return _srv->connected_to; }
    bool	is_connected_from() const	{return _srv->connected_from; }
    srvid_t 	id() const			{return _srv->serverid;}
    Endpoint	conn_port() const		{return _srv->conn_port;}
    Endpoint	req_to_port() const		{return _srv->req_to_port;}
    Endpoint	req_from_port() const		{return _srv->req_from_port;}
    Endpoint	reply_to_port() const		{return _srv->reply_to_port;}
    Endpoint	reply_from_port() const		{return _srv->reply_from_port;}

    Buffer*	req_out_buf() const		{return _srv->req_out_buf;}
    Buffer*	req_in_buf() const		{return _srv->req_in_buf;}

    bool	is_mounted(vid_t vid) const;
    rc_t	mark_mounted(vid_t vid);

    void        incr_vol_cnt(int i)             { _srv->vol_mount_cnt += i; }
    int		vol_mount_cnt() const		{ return _srv->vol_mount_cnt; }
    void	incr_xct_cnt(int i)		{ _srv->xct_cnt += i; }
    int		xct_cnt() const			{ return _srv->xct_cnt; }

    NORET       operator const void*() const	{ return _srv; }

    friend ostream& operator<<(ostream&, const server_h& srv);

    static server_h	null;

private:
    server_s*		_srv;
    srv_access_type_t	_access;
};


class server_tab_m {
public:
			server_tab_m();
			~server_tab_m();

    rc_t		grab(
	const Endpoint&		conn_port,
	srvid_t&		srvid,
	server_s*&		srv,
	bool&			found,
	bool&			is_new);

    rc_t		fix(srvid_t id, srv_access_type_t access, server_s*& srv);
    void		unfix(struct srvid_t, srv_access_type_t access);
    void		publish();
    srvid_t		find_srvid(const Endpoint& global_id);

private:
    smutex_t		_mutex;
    server_s*		_tab[smlevel_0::max_servers];
};


struct logbuf_t {
    logbuf_t();
    ~logbuf_t();

    w_link_t    link;
    w_link_t	xct_link;
    lsn_t       lsn;
    char*	rec;
};


class log_cache_m {
    friend class remote_m;
    friend class remote_log_m;
public:
		log_cache_m();
		~log_cache_m();

    rc_t	insert(logrec_t& r, master_xct_proxy_t* mxct, lsn_t& lsn);

    int		numlogs() { return log_cache->num_members();}

private:
    long                        log_size;      // sum of sizes of cached logrecs
    w_list_t<logbuf_t>*         log_cache;

    // TODO: A more efficient data structure for the log cache may be needed.
    //       The linked list used for now offers true simplicity, but is not
    //       very efficient due to frequent space allocations and deallocations.

    lsn_t			curr_lsn;	// to be assigned to next rec.
    lsn_t			durable_lsn;	// all logs with lsn's < than
						// durable_lsn have been flushed
};


class remote_log_m : public smlevel_1 {
public:
    NORET		remote_log_m() {};
    NORET		~remote_log_m() {};

    rc_t		insert(
	logrec_t&	    r,
	lsn_t&		    lsn);

    rc_t		flush(
	const lsn_t&	    lsn,
	int&		    nlogs,
	master_xct_proxy_t* mxct = 0,
	const lpid_t&	    pid = lpid_t::null);

    rc_t		purge(master_xct_proxy_t& mxct);

    bool		is_dirty(bfcb_t* b);
};


class endpoints_t;

/*
 *
   The remote access manager (remote_m) is at the same level
   as the ss_m.  It can access any lower level subsystem.
   However, some low level subsystems, such as io_m will
   need access to remote_m.  For this there is remote_client_m,
   which is a friend of remote_m.  This division is important
   because it means low-level subsystems will not need to include remote.h
  
   All rcp calls are made with _call().  All rpc service
   requests are dispatched with _dispatch_msg().
   All rpc server functions have the formate:
   rc_t		foo_src(
			Buffer&			msg_buf,
			server_h&		from_server,
			remote_xct_proxy_t*	from_xct,
			const Endpoint&		reply_port)

	NOTE reply_port is non-const ref so that it can
	  // be changed by the dispatched function.   This is
	  important since _dispatch_msg() replies in the case of
	  errors, but the dispatched function replies on success.
 *
 */
class remote_m : public smlevel_4 {

friend class remote_client_m;
friend class server_tab_m;
friend class remote_log_m;
friend class test_m;

public:
    enum { max_servers = smlevel_0::max_servers };
    enum { max_volumes = 20};

    class connection_thread_t : public smthread_t {
    public:
	NORET		connection_thread_t(remote_m* rm) :
			    smthread_t(t_regular, false, false,
				       "ConnectThread"),
			    _remote_manager(rm) {}
	NORET		connection_thread_t() {}

	virtual void 	run() {_remote_manager->monitor_conn_port();}

    private:
	remote_m*	_remote_manager;

	//disabled
	NORET		connection_thread_t(const connection_thread_t&);
	connection_thread_t&	operator= (const connection_thread_t&);
    };

    remote_m();
    ~remote_m();

    /*
     * Functions used by other layers of the storage manager.
     */
    static rc_t register_volume(const lvid_t& lvid);

    // mount a volume from another server
    static rc_t mount_volume(const lvid_t& lvid);

    // transaction related
    static xct_t*	master_to_remote(
	const tid_t&	    tid,
	const srvid_t&	    srvid);
    static xct_t*	remote_to_master(const tid_t& tid);
    static rc_t		spread_xct(const vid_t& vid);
    static rc_t		commit_xct(xct_t& xct);
    static rc_t		abort_xct(xct_t& xct);

    // callback related
    static rc_t		send_callbacks(const callback_op_t& cbop);
    static rc_t		send_callback_aborts(callback_op_t& cbop);
    static rc_t		get_cb_replies(callback_op_t& cbop);
    static rc_t		send_cb_reply(
	const callback_xct_proxy_t& cbxct,
	cb_outcome_t	    outcome,
	int		    numblockers = 0,
	blocker_t*	    blockers = 0,
	uint2		    block_level = 0);
    static rc_t		send_deadlock_notification(
	callback_op_t*	    cbop,
	const tid_t&	    sender,
	const tid_t&	    victim);
    static rc_t		adaptive_callback(
	const lpid_t&	    pid,
	const srvid_t&	    srv,
	int		    n,
	adaptive_xct_t*	    xacts,
	long		    cbid);
    static rc_t		register_purged_page(const lpid_t& pid);

    static rc_t		get_pids(
	stid_t		    stid,
	bool		    lock,
	bool		    last,
	shpid_t		    first,
	int&		    num,
	shpid_t*	    pids);

    static rc_t		lookup_lid(
	vid_t		    vid,
	const lid_t&	    lid,
	remote_lid_entry_t& entry); 

    static rc_t		acquire_lock(
	const lockid_t&	    n,
	lock_mode_t	    m,
	lock_duration_t	    d,
	long		    timeout,
	bool		    force,
	int*		    value = 0);

    static rc_t		read_page(
	const lockid_t&	    name,
	bfcb_t*		    b,
	uint2		    ptag,
	bool		    detect_race,
	bool		    parallel,
	bool              is_new);

    static rc_t send_page(const lpid_t& pid,
			 page_s& buf);

    static rc_t ping_xct(const srvid_t& srvid, bool direct);

    /*
     * Functions used by objects which directly support
     * the remote_m such as server_h and various thread classes.
     */
    // This function is run by threads monitoring a server request port
    static void monitor_server_req_port(server_s& server);

    // This function is run by threads monitoring the arrival of remotely 
    // generated log records.
    static void monitor_log_port(server_s& server);

    // This function is run by threads monitoring the server connection port
    static void monitor_conn_port();

    // This function is run by threads monitoring a transaction request port
    static void monitor_xct_port(remote_xct_proxy_t* rxct);

    static rc_t		connect_to(
	const Endpoint&	    conn_port,
	srvid_t&	    srvid,
	server_s*&	    server);
    static rc_t		disconnect_from(
	server_s*	    server);
    static rc_t		fix_server(
	const srvid_t&	    srvid,
	server_s*&	    server,
	srv_access_type_t   access);
    static void		unfix_server(
	server_s*	    server,
	srv_access_type_t   access);

    static void         print_msg_stats(ostream& o);
    static void		reset_msg_stats();
    static void		msg_stats(sm_stats_info_t& stats);

    // all message types belong to enum msg_type_t located here.
    #include "remote_msg_enum.i"

    class msg_stats_t {
    public:
        msg_stats_t() : req_send_cnt(0), req_rcv_cnt(0), reply_rcv_cnt(0),
                        reply_send_cnt(0), reply_send_error_cnt(0),
                        reply_send_skip_cnt(0) {};

        friend ostream& operator<<(ostream& o, const msg_stats_t& stats) {
            o << "req_send_cnt         : " << stats.req_send_cnt        << endl
              << "req_rcv_cnt          : " << stats.req_rcv_cnt         << endl
              << "reply_rcv_cnt        : " << stats.reply_rcv_cnt       << endl
              << "reply_send_cnt       : " << stats.reply_send_cnt      << endl
              << "reply_send_error_cnt : " << stats.reply_send_error_cnt<< endl
              << "reply_send_skip_cnt  : " << stats.reply_send_skip_cnt << endl;
	    return o;
        }

        uint4       req_send_cnt;           // requests sent
        uint4       req_rcv_cnt;            // requests received
        uint4       reply_rcv_cnt;          // replies received
        uint4       reply_send_cnt;         // replied with success
        uint4       reply_send_error_cnt;   // replied with error
        uint4       reply_send_skip_cnt;    // replies not requested
    };

    static endpoints_t*	xact_ports;

private:

    #include "remote_msg.h"

    /*
     * "Client" RPCs
     */
    static rc_t		_mount_volume_cli(
	server_h&		server,
	const lvid_t&		lvid,
	vid_t&			vid,
	lpid_t&			dir_root);
    static rc_t		_spread_xct_cli(
	master_xct_proxy_t*	mxct,
	server_h&		server,
	tid_t&			remote_tid,
	Endpoint&		req_port);
    static rc_t		_prepare_xct_cli(master_xct_proxy_t& mxct);
    static rc_t		_commit_xct_cli(master_xct_proxy_t& mxct);
    static rc_t		_abort_xct_cli(master_xct_proxy_t& mxct);
    static rc_t		_ping_xct_cli(master_xct_proxy_t* mxct, bool direct);

    /*
     * "Server" end of RPCs
     */
    static rc_t 	_mount_volume_srv(
	Buffer&		    msg_buf,
	server_h&	    from_server);
    static rc_t		_spread_xct_srv(
	Buffer&		    msg_buf,
	EndpointBox&	    master_port,
	server_h&	    from_server);
    static rc_t		_prepare_xct_srv(
	Buffer&		    msg_buf,
	remote_xct_proxy_t* from_xct,
	const Endpoint&     reply_port);
    static rc_t		_commit_xct_srv(
	Buffer&		    msg_buf,
	remote_xct_proxy_t* from_xct,
	const Endpoint&     reply_port);
    static rc_t		_abort_xct_srv(
	Buffer&		    msg_buf,
	remote_xct_proxy_t* from_xct,
	const Endpoint&     reply_port);
    static void		_redo_logs_srv(
	Buffer&		    buf,
	const srvid_t&	    srvid,
	const Endpoint&	    reply_port);
    static rc_t		_get_pids_srv(
	Buffer&		    msg_buf,
	remote_xct_proxy_t* from_xct,
	const Endpoint&	    reply_port);
    static rc_t		_lookup_lid_srv(
	Buffer&		    msg_buf,
	remote_xct_proxy_t* from_xct,
	const Endpoint&     reply_port);
    static rc_t		_acquire_lock_srv(
	Buffer&		    msg_buf,
	remote_xct_proxy_t* from_xct,
	const Endpoint&     reply_port);
    static rc_t		_read_page_srv(
	Buffer&		    msg_buf,
	remote_xct_proxy_t* from_xct,
	const Endpoint&     reply_port);
    static rc_t		_callback_srv(
	Buffer&		    msg_buf,
	EndpointBox&	    reply_port,
	server_h&	    from_server);
    static rc_t		_abort_callback_srv(
	Buffer&		    msg_buf,
	server_h&	    from_server);
    static rc_t		_adaptive_callback_srv(
	Buffer&		    buf,
	server_h&	    from_server);
    static rc_t		_purged_pages_srv();

    static rc_t		_ping_xct_srv(
	Buffer&             msg_buf,
	EndpointBox&	    dest_port,
	server_h&           from_server,
	remote_xct_proxy_t* from_xct,
	const Endpoint&     reply_port);
    static rc_t		_print_stats_srv(Buffer& msg_buf);

    /*
     * Support functions
     */
    static rc_t         _connect_srv(
        Buffer&                 msg_buf,
	EndpointBox&		ports);
    static rc_t         _disconnect_srv(
        Buffer&          	msg_buf,
        server_h&       	from_server,
        const Endpoint& 	reply_port);
    static rc_t		_find_volume_server(
	const lvid_t&		lvid,
	Endpoint&		server);
    static rc_t		_add_new_volume(
	srvid_t			srvid,
	vid_t 			wner_vid,
	vid_t&			alias);
    static rc_t		_spread_xct(master_xct_proxy_t* mxct);
    static rc_t		_cleanup_xct(xct_t& xct);
    static void		_purge_dirty(xct_t& xct);
    static const rvol_t* _lookup_volume(vid_t);
    static const rvol_t* _inv_lookup_volume(vid_t owner_vid);
    static rc_t         _lookup_master_proxy(
        vid_t                   vid,
        master_xct_proxy_t*&    mxct,
        vid_t&                  remote_vid);

#ifdef OBJECT_CC
    static void		_merge(
	bfcb_t*			b,
	page_s*			curr_copy,
	page_s*			new_copy);
    static bool		_rec_dirty(
	const lpid_t&		pid,
	int			slot,
	bool&			implicit_EX);

    static void		_install_recmap(
	const lockid_t&		name,
	bfcb_t*			b,
	u_char*			recmap,
	page_s*			page,
	bool			is_cached);
#endif /* OBJECT_CC */

    static void		_build_recmap(
	const lockid_t&		name,
	const srvid_t&		srv,
	int			nslots,
	Buffer&			buf);

    // send an rpc to the server
    // The buf_vec are assumed to point to vector of Buffer
    static rc_t		_call_server(
	Buffer&			buf,
	EndpointBox&		ports,
	server_h&		server,
	bool			reply);
    static rc_t		_call_xct_server(
	Buffer&			buf,
	master_xct_proxy_t&	xct);
    static rc_t		_call(
	Buffer&			buf,
	EndpointBox&		out_ports,
	EndpointBox&		in_ports,
	const srvid_t&		srvid,
	server_h&		server,
	const Endpoint&		send_to,
	const Endpoint&		reply_from);

    static rc_t		_reply(
	Buffer&			buf,
	EndpointBox&		ports,
	rc_t			rc,
	const Endpoint&		reply_to_port,
	const srvid_t&		srvid);

    static void		_dispatch_msg(
	Buffer&			buf,
	EndpointBox&		ports,
	server_h&		from_server,
	remote_xct_proxy_t*	from_xct,
	const Endpoint&		reply_to,
	const Endpoint&		req_from);

    static rc_t		_piggyback_purged_pages(
	const srvid_t&		srvid,
	server_h&		server,
	msg_hdr_t&		msg,
	const Endpoint&		port);
    static void		_delete_copies(
	const srvid_t&		srvid,
	msg_hdr_t&		msg,
	Buffer&			buf);

#ifdef SHIV
// ************************ SHIV *************************************
    static rc_t _read_send_page_cli(master_xct_proxy_t* mxct,
				    const lpid_t& pid, page_s& pbuf,
				    lock_mode_t mode,  bool detect_race);
    static rc_t _send_page_cli(server_h &server,
			       const lpid_t pid,  page_s& pbuf);


    static rc_t _fwd_page_srv(
        Buffer& msg_buf,
	server_h& from_server,
	remote_xct_proxy_t* from_xct,
	const Endpoint& reply_port,
	const Endpoint& req_port);


    static rc_t		_send_page_srv(
	Buffer&		    msg_buf,
	server_h&	    from_server,
	remote_xct_proxy_t* from_xct,
	const Endpoint&     reply_port,
 	const Endpoint&     req_port);
    static rc_t		_read_send_page_srv(
	Buffer&		    msg_buf,
	server_h&	    from_server,
	remote_xct_proxy_t* from_xct,
	const Endpoint&     reply_port,
 	const Endpoint&     req_port);

    static rc_t _send_server(const vec_t& bufs, const Endpoint& send_to,
			     srvid_t srvid);	
    static rc_t _receive_response(const vec_t& bufs, const Endpoint& reply_from,
				  srvid_t srvid);

// *************************** SHIV ***************************************
#endif /* SHIV */

    // server table
    static server_tab_m*		_server_tab;

    // port for connecting to this server
    static Endpoint			_conn_port;

    // remote volume mount table : implements mapping from alias vid to rvol_t
    static hash_lru_t<rvol_t, vid_t>*	_vol_tab;

    // inverse remote volume mount table :
    // implements mapping from remote vid to rvol_t; needed for callbacks.
    static w_hash_t<rvol_t, vid_t>*   _inv_vol_tab;

    // counter used to generate alias volume IDs as keys to _vol_table
    static vid_t			_alias_counter;
    // protection for the counter
    static smutex_t			_alias_counter_mutex;

 
    // Table of all transaction proxies for which this server is a master
    //hash_lru_t<remote_xct_proxy_t, global_tid_t>*	_xct_tab;
    //smutex_t	_xtab__mutex;

    // Thread to monitor connection requests
    static connection_thread_t*		_conn_thread; 

    // interface for log caches to remote logs
    static remote_log_m*		_rlog;


    static const char*			_msg_str[];

    static msg_stats_t                  _msg_stats[max_msg+1];
    static bool send_back;
    static bool discard_only;
    static bool hate_hints;

    // disabled
    remote_m(const remote_m&);
    operator=(const remote_m&);
};


class endpoints_t {
public:
    enum { n_alloc = 128 };

    NORET	endpoints_t();
    NORET	~endpoints_t();

    inline const Endpoint&	get();
    inline void			put(const Endpoint& port);

#ifndef NOT_PREEMPTIVE
    smutex_t	mutex;
#endif
    Endpoint	tab[n_alloc];
    bool	avail[n_alloc];
};

#endif /*!MULTI_SERVER*/

#endif /* REMOTE_H */

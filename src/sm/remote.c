/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

/*
 *  $Id: remote.c,v 1.43 1996/04/09 20:44:44 nhall Exp $
 */

#define SM_SOURCE
#define REMOTE_C

#ifdef __GNUG__
#pragma implementation "remote_s.h"
#pragma implementation "remote.h"
#pragma implementation "remote_msg.h"
#pragma implementation "srvid_t.h"
#endif

#include <sys/time.h>

#include <sm_int.h>
#include <bf_s.h>
#include <auto_release.h>
#include <cache.h>
#include <lock_x.h>
#include <lock_core.h>
#include <callback.h>
#include <remote_client.h>
#include <remote.h>
#include <pin.h>
#include <sm.h>

#include <unix_stats.h>

#ifdef MULTI_SERVER

srvid_t srvid_t::null;

#ifdef __GNUG__
template class w_auto_delete_t<remote_xct_proxy_t>;
template class w_auto_delete_t<master_xct_proxy_t>;
template class w_auto_delete_t<callback_xct_proxy_t>;

template class w_list_t<callback_t>;
template class w_list_t<purged_page_t>;
template class w_list_t<rexport_t>;
template class w_list_t<rvol_t>;
template class w_list_t<logbuf_t>;

template class w_list_i<callback_t>;
template class w_list_i<purged_page_t>;
template class w_list_i<rexport_t>;
template class w_list_i<rvol_t>;
template class w_list_i<logbuf_t>;

template class w_hash_t<rexport_t, vid_t>;
template class w_hash_t<rvol_t, vid_t>;

template class w_hash_i<pid_hash_t, lpid_t>;

template class hash_lru_t<rvol_t, vid_t>;
template class w_list_t<hash_lru_entry_t<rvol_t, vid_t> >;
template class w_list_i<hash_lru_entry_t<rvol_t, vid_t> >;
template class w_hash_t<hash_lru_entry_t<rvol_t, vid_t>, vid_t>;
#endif


/*****************************************************************************/

// This is a thread class for running monitor_server_req_port()
class server_req_thread_t : public smthread_t {
public:
    NORET		server_req_thread_t(server_s* server);
    NORET		~server_req_thread_t()   {};

    virtual void	run();

private:
    server_s*		_server;

    //disabled
    NORET			server_req_thread_t(const server_req_thread_t&);
    server_req_thread_t&	operator=(const server_req_thread_t&);

};


inline NORET server_req_thread_t::server_req_thread_t(server_s* server)
	: smthread_t(t_regular, false, FALSE, "ServerThread"),
	  _server(server)
{
}


void server_req_thread_t::run()
{
    remote_m::monitor_server_req_port(*_server);
}

/*----------------------------------------------------------------------------*/

class log_thread_t : public smthread_t {
public:
    NORET               log_thread_t(server_s* server);
    NORET               ~log_thread_t()   {};

    virtual void        run();

private:
    server_s*           _server;

    //disabled
    NORET               log_thread_t(const log_thread_t&);
    log_thread_t&	operator=(const log_thread_t&);

};


inline NORET log_thread_t::log_thread_t(server_s* server)
        : smthread_t(t_regular, false, FALSE, "LogThread"),
          _server(server)
{
}


void log_thread_t::run()
{
    remote_m::monitor_log_port(*_server);
}

/*----------------------------------------------------------------------------*/

// This is a thread class for running monitor_rxct_req_port()
class rxct_thread_t : public smthread_t {
public:
    NORET                       rxct_thread_t(remote_xct_proxy_t* rxct);
    NORET                       ~rxct_thread_t()   {};

    virtual void                run();

private:
    remote_xct_proxy_t*		_rxct;

    //disabled
    NORET			rxct_thread_t(const rxct_thread_t&);
    rxct_thread_t&		operator=(const rxct_thread_t&);

};


inline NORET
rxct_thread_t::rxct_thread_t(remote_xct_proxy_t* rxct)
	: smthread_t(t_regular, false, TRUE, "XactThread"),
	  _rxct(rxct)
{
}


void rxct_thread_t::run()
{
    remote_m::monitor_xct_port(_rxct);
}

/*----------------------------------------------------------------------------*/

class cbxct_thread_t: public smthread_t {
public:
    NORET			cbxct_thread_t(callback_xct_proxy_t* cbxct);
    NORET			~cbxct_thread_t() {};

    virtual void		run();

private:
    callback_xct_proxy_t*	_cbxct;

    //disabled
    NORET			cbxct_thread_t(const cbxct_thread_t&);
    cbxct_thread_t&		operator=(const cbxct_thread_t&);
};


inline NORET
cbxct_thread_t::cbxct_thread_t(callback_xct_proxy_t* cbxct) :
	smthread_t(t_regular, false, TRUE, "CallbackThread"),
	_cbxct(cbxct)
{
}


void cbxct_thread_t::run()
{
    callback_m::handle_cb_req(_cbxct);
}


/******************************************************************************/


master_xct_proxy_t::master_xct_proxy_t(xct_t* x, const srvid_t& s)
        : _xct(x), _srvid(s), _first_lsn(0, 0), _last_lsn(0, 0),
	  _logs(offsetof(logbuf_t, xct_link)),
	  _reply_from_port(remote_m::xact_ports->get())
{
    _req_to_buf = new Buffer(remote_m::max_msg_size);
    if (!_req_to_buf) W_FATAL(eOUTOFMEMORY);
}


master_xct_proxy_t::~master_xct_proxy_t()
{
    remote_m::xact_ports->put(_reply_from_port);
    // W_COERCE(_req_to_port.destroy());
    delete(_req_to_buf); _req_to_buf = NULL;
    _link.detach();
    w_assert3(_logs.is_empty());
}

/*----------------------------------------------------------------------------*/

remote_xct_proxy_t::remote_xct_proxy_t(tid_t master_tid, srvid_t master_server,
                        const Endpoint& reply_port)
    : _xct(NULL), _server(master_server), _tid_on_master(master_tid),
      _req_from_port(remote_m::xact_ports->get()), _reply_to_port(reply_port),
      _req_from_buf(NULL), _req_thread(NULL)
{
    _req_from_buf = new Buffer(remote_m::max_msg_size);
    if (!_req_from_buf) W_FATAL(eOUTOFMEMORY);

    // Start thread to monitor request port
    _req_thread = new rxct_thread_t(this);
    if (!_req_thread) W_FATAL(eOUTOFMEMORY);
    W_COERCE(_req_thread->fork());
}


remote_xct_proxy_t::~remote_xct_proxy_t()
{
    remote_m::xact_ports->put(_req_from_port);
    // W_COERCE(_reply_to_port.destroy());
    delete(_req_from_buf); _req_from_buf = NULL;
    _xct->set_master_site(0);
    delete _xct; _xct = NULL;

    // thread does not need to be delete as it is type "auto delete"
    // and will be deleted when it's "run" function is finished.
    w_assert3(_req_thread == 0);
}

/*----------------------------------------------------------------------------*/

callback_xct_proxy_t::callback_xct_proxy_t(
	long id,
	const lockid_t&	n,
	lock_mode_t	m,
	bool		pdirty,
	const srvid_t&	remsrv,
	const Endpoint&	mastersrv,
	const tid_t&	remtid,
	const tid_t&	mastertid,
	Endpoint	reply_port) :
	cb_id(id), _xct(0), _remote_server(remsrv), _master_server(mastersrv),
	_remote_tid(remtid), _master_tid(mastertid), _name(n), _mode(m),
#ifdef HIER_CB
	_page_dirty(pdirty),
#endif
	_reply_to_port(reply_port), _cb_thread(NULL)
{
    _reply_buf = new Buffer(remote_m::max_msg_size);
    if (!_reply_buf) W_FATAL(eOUTOFMEMORY);
}


callback_xct_proxy_t::~callback_xct_proxy_t()
{
    // W_COERCE(_reply_to_port.destroy());
    delete(_reply_buf); _reply_buf = NULL;
    w_assert3(! _xct->is_remote_proxy());
    _xct->set_cb_proxy(0);
    delete _xct; _xct = NULL;
    w_assert3(_cb_thread == NULL);
}

/*----------------------------------------------------------------------------*/

callback_op_t::callback_op_t()
           : cb_id(-1), outcome(NONE), blocked(FALSE), local_deadlock(FALSE),
	     num_no_replies(0),
	     callbacks(offsetof(callback_t, link)),
	     reply_from_port(remote_m::xact_ports->get())
{
    name.zero();
    name.lspace() = lockid_t::t_bad;
    // callbacks.set_offset(offsetof(callback_t, link));
    // blockers.set_offset(offsetof(blocker_t, link));

    reply_from_buf = new Buffer(remote_m::max_msg_size);
    w_assert1(reply_from_buf);

#ifdef OBJECT_CC
    page_blockers = 0;
#endif /* OBJECT_CC */

#ifdef HIER_CB
    page_dirty = FALSE;
    myreq_pending = FALSE;
#endif /* HIER_CB */
}


void callback_op_t::clear()
{
    w_assert3(callbacks.num_members() == 0);
    // w_assert3(blockers.num_members() == 0);
    name.zero();
    name.lspace() = lockid_t::t_bad;
    outcome = NONE;
    blocked = FALSE;
    local_deadlock = FALSE;
#ifdef OBJECT_CC
    page_blockers = 0;
#endif
    num_no_replies = 0;
    cb_id = -1;
}


callback_op_t::~callback_op_t()
{
    w_assert3(callbacks.num_members() == 0);
    // w_assert3(blockers.num_members() == 0);
    remote_m::xact_ports->put(reply_from_port);
    delete reply_from_buf;  reply_from_buf = NULL;
}


callback_t::callback_t(const srvid_t& srv, int races) :
			server(srv), cb_tid(tid_t::null),
			outcome(callback_op_t::NONE), read_races(races)
#ifdef OBJECT_CC
			, page_blocked(FALSE)
#endif
{
}


/*****************************************************************************/


server_tab_m::server_tab_m() : _mutex("server_tab_mx")
{
    for (int i = 0; i < smlevel_0::max_servers; i++) _tab[i] = 0;
}


server_tab_m::~server_tab_m()
{
    for (int i = 0; i < smlevel_0::max_servers; i++) {
        if (_tab[i]) {
            // TODO: shut down connections
        }
    }
}


rc_t server_tab_m::fix(srvid_t id, srv_access_type_t access, server_s*& srv)
{
    w_assert3(id.id > 0 && id.id < smlevel_0::max_servers);

    MUTEX_ACQUIRE(_mutex);
    if (_tab[id.id]) {
	srv = _tab[id.id];
        switch (access) {
        case REQ:
            W_COERCE(srv->req_mutex.acquire());
            break;
        case MSG:
            MUTEX_ACQUIRE(srv->msg_mutex);
            break;
        case LOG:
	    W_COERCE(srv->log_mutex.acquire());
	    break;
	case FLUSH_LOG:
	    W_COERCE(srv->flush_mutex.acquire());
	    break;
	case NONE:
	    break;
        default:
            W_FATAL(eINTERNAL);
        }
    } else {
	MUTEX_RELEASE(_mutex);
	return RC(smlevel_0::eSERVERNOTCONNECTED);
    }
    MUTEX_RELEASE(_mutex);
    return RCOK;
}


void server_tab_m::unfix(srvid_t id, srv_access_type_t access)
{
    w_assert3(id.id > 0 && id.id < smlevel_0::max_servers);

    server_s* srv = _tab[id.id];
    w_assert3(srv);
    switch (access) {
    case REQ:
	srv->req_mutex.release();
	break;
    case MSG:
	MUTEX_RELEASE(srv->msg_mutex);
	break;
    case LOG:
	srv->log_mutex.release();
	break;
    case FLUSH_LOG:
	srv->flush_mutex.release();
	break;
    case NONE:
	break;
    default:
	W_FATAL(eINTERNAL);
    }
}


void server_tab_m::publish()
{
    _mutex.release();
}


rc_t server_tab_m::grab(const Endpoint& conn_port, srvid_t& srvid,
				server_s*& srv, bool& found, bool& is_new)
{
    W_COERCE(_mutex.acquire());

    for (int i = 1; i < smlevel_0::max_servers; i++) {
	if (_tab[i] && _tab[i]->conn_port == conn_port) {
	      srvid = srvid_t(i);
	      srv = _tab[i];
	      found = TRUE;
	      is_new = FALSE;
	      W_COERCE(_tab[i]->req_mutex.acquire());
	      return RCOK;
	}
    }

    // server not found

    found = FALSE;

    for (i = 1; i < smlevel_0::max_servers; i++) {
	if (!_tab[i]) {
	    _tab[i] = new server_s;
	    srvid = srvid_t(i);
	    srv = _tab[i];
	    srv->serverid = srvid;
	    is_new = TRUE;

            W_COERCE(srv->req_mutex.acquire());
	    return RCOK;
	}
    }

    // No empty slot found in the server tab; need to do replacement.

    is_new = FALSE;

    for (i = 1; i < smlevel_0::max_servers; i++) {
	if (! _tab[i]->in_use()) break;
    }

    if (i == smlevel_0::max_servers) { W_FATAL(smlevel_0::eNOFREESERVER); }

    srvid = i;
    srv = _tab[i];
    srv->serverid = srvid;
    W_COERCE(srv->req_mutex.acquire());

    return RCOK;
}


srvid_t server_tab_m::find_srvid(const Endpoint& global_id)
{
    MUTEX_ACQUIRE(_mutex);
    int i = 1;
    while (i < smlevel_0::max_servers && _tab[i] && 
				_tab[i]->conn_port != global_id) i++;

    if (i == smlevel_0::max_servers) {
        MUTEX_RELEASE(_mutex);
        return srvid_t::null;
    }
    MUTEX_RELEASE(_mutex);
    return i;
}

/*----------------------------------------------------------------------------*/

server_h	server_h::null;


server_h::~server_h()
{
    if (_srv) {
	remote_m::unfix_server(_srv, _access);
	_srv = NULL;
    }
    _access = NONE;
}


rc_t server_h::connect(const Endpoint& conn_port, srvid_t& srvid)
{
    if (_srv) {
	remote_m::unfix_server(_srv, _access);
    }
    W_DO(remote_m::connect_to(conn_port, srvid, _srv));
    _access = REQ;
    return RCOK;
}


rc_t server_h::fix(srvid_t srvid, srv_access_type_t access)
{
    if (_srv) {
	unfix();
    }
    W_DO(remote_m::fix_server(srvid, _srv, access));
    _access = access;
    return RCOK;
}


void server_h::unfix()
{
    if (_srv) {
	remote_m::unfix_server(_srv, _access);
	_srv = NULL;
	_access = NONE;
    }
    return ;
}


bool server_h::is_mounted(vid_t vid) const {
    rexport_t* r = _srv->export_tab->lookup(vid);
    if (r) {
        w_assert3(r->vid == vid);
        return TRUE;
    }
    return FALSE;
}


rc_t server_h::mark_mounted(vid_t vid) {
    rexport_t* r = _srv->export_tab->lookup(vid);
    w_assert3(!r);
    r = new rexport_t(vid);
    if (!r) return RC(eOUTOFMEMORY);
    _srv->export_tab->push(r);
    return RCOK;
}


ostream& operator<<(ostream& o, const server_h& srv)
{
    return o << "server_h with server_s: " << srv._srv;
}

/*----------------------------------------------------------------------------*/

server_s::server_s()
	: req_mutex("srv_req_mx"), req_out_buf(NULL), req_in_buf(NULL),
	  req_thread(NULL), export_tab(NULL),
	  msg_mutex("srv_msg_mutex"), purged_pages(NULL),
	  log_mutex("srv_log_mx"), flush_mutex("log_flush_mx"),
	  log_cache(NULL), log_thread(NULL)
{
    serverid = srvid_t::null;
    connected_to = FALSE;
    connected_from = FALSE;
    vol_mount_cnt = 0;
    xct_cnt = 0;

    export_tab = new w_hash_t<rexport_t, vid_t>(smlevel_0::max_vols,
		offsetof(rexport_t, vid), offsetof(rexport_t, hash_link));
    if (!export_tab) W_FATAL(eOUTOFMEMORY);

    purged_pages = new w_list_t<purged_page_t>(offsetof(purged_page_t, link));
    if (!purged_pages) W_FATAL(eOUTOFMEMORY);

    log_cache = new log_cache_m();
    if (!log_cache) W_FATAL(eOUTOFMEMORY);

    req_in_buf = new Buffer(remote_m::max_msg_size);
    if (!req_in_buf) W_FATAL(eOUTOFMEMORY);

    req_out_buf = new Buffer(remote_m::max_msg_size);
    if (!req_out_buf) W_FATAL(eOUTOFMEMORY);

    log_in_buf = new Buffer(remote_m::max_msg_size);
    if (!log_in_buf) W_FATAL(eOUTOFMEMORY);

    log_out_buf = new Buffer(remote_m::max_msg_size);
    if (!log_out_buf) W_FATAL(eOUTOFMEMORY);

    return;
}


void server_s::init(const srvid_t& srvid, const Endpoint& connect_at)
{
    clear();

    serverid = srvid;
    conn_port = connect_at;
    W_COERCE(comm_m::comm_system()->make_endpoint(req_from_port));
    W_COERCE(comm_m::comm_system()->make_endpoint(reply_from_port));
    W_COERCE(comm_m::comm_system()->make_endpoint(log_from_port));
    W_COERCE(comm_m::comm_system()->make_endpoint(log_reply_in_port));

    return;
}


void server_s::clear()
{
    serverid = srvid_t::null;
    W_COERCE(conn_port.destroy());
    W_COERCE(req_to_port.destroy());
    W_COERCE(req_from_port.destroy());
    W_COERCE(reply_to_port.destroy());
    W_COERCE(reply_from_port.destroy());
    W_COERCE(log_to_port.destroy());
    W_COERCE(log_from_port.destroy());
    W_COERCE(log_reply_in_port.destroy());
    W_COERCE(log_reply_out_port.destroy());

    connected_to = FALSE;
    connected_from = FALSE;
    vol_mount_cnt = 0;
    xct_cnt = 0;

    /* If any threads are running, wait for them to complete */
    if (req_thread) {
        W_COERCE(req_thread->wait());
        delete req_thread;
        req_thread = NULL;
    }

    if (log_thread) {
	W_COERCE(log_thread->wait());    
	delete log_thread;
	log_thread = NULL;
    }

    w_assert1(export_tab && export_tab->num_members() == 0);
    w_assert1(purged_pages && purged_pages->num_members() == 0);
    w_assert1(log_cache && log_cache->numlogs() == 0);

    return;
}


server_s::~server_s()
{
    clear();

    if (export_tab)	{ delete export_tab; export_tab = NULL; }
    if (purged_pages)	{ delete purged_pages; purged_pages = NULL; }
    if (log_cache)	{ delete log_cache; log_cache = NULL; }
    free_buffer(req_out_buf);
    free_buffer(req_in_buf);
    free_buffer(log_in_buf);
    free_buffer(log_out_buf);

    return;
}


void server_s::free_buffer(Buffer*& b) 
{
    if (b != NULL) {
	delete b;
	b = NULL;
    }
}


ostream& operator<<(ostream& o, const server_s& srv)
{
    return o << "server_s with id: " << srv.serverid;
}


/*****************************************************************************/


logbuf_t::logbuf_t()
{
}

logbuf_t::~logbuf_t()
{
    delete [] rec;
    xct_link.detach();
    link.detach();
}


log_cache_m::log_cache_m()
{
    log_cache = new w_list_t<logbuf_t>(offsetof(logbuf_t, link));
    if (!log_cache) W_FATAL(eOUTOFMEMORY);

    log_size = 0;
    curr_lsn = durable_lsn = lsn_t(1, 0);
}


log_cache_m::~log_cache_m()
{
    w_assert1(log_cache->num_members() == 0);
    w_assert1(curr_lsn == durable_lsn);
}


rc_t log_cache_m::insert(logrec_t& r, master_xct_proxy_t* mxct, lsn_t& lsn)
{
    logbuf_t* buf = new logbuf_t;
    if (!buf) W_FATAL(eOUTOFMEMORY);

    buf->rec = new char[r.length()];
    if (!buf->rec) W_FATAL(eOUTOFMEMORY);

    memcpy((void *)buf->rec, (void *)&r, r.length());
    buf->lsn = curr_lsn;

    w_assert3(mxct->last_lsn() < durable_lsn || mxct->logs().top() != 0);

    log_cache->append(buf);
    log_size += r.length();

    // TODO: flush log cache if it grows too big, or
    // flush whenever there are enough logrecs to fill one msg of the max size

    mxct->logs().push(buf);
    mxct->set_last_lsn(curr_lsn);
    if (!mxct->first_lsn()) mxct->set_first_lsn(curr_lsn);

    lsn = curr_lsn;
    curr_lsn.increment();

    return RCOK;
}

/*----------------------------------------------------------------------------*/

rc_t remote_log_m::insert(logrec_t& r, lsn_t& lsn)
{
    const lpid_t& pid = r.pid();

    master_xct_proxy_t* mxct = 0;
    vid_t rvid;
    W_DO(remote_m::_lookup_master_proxy(pid.vol(), mxct, rvid));

    server_h server;
    W_COERCE(server.fix(mxct->srvid(), LOG));

    W_DO(server.log_cache().insert(r, mxct, lsn));
    return RCOK;
}


rc_t remote_log_m::flush(const lsn_t& lsn, int& nlogs,
				master_xct_proxy_t* mxct, const lpid_t& pid)
{
    nlogs = 0;

    if (mxct && (mxct->logs().is_empty())) return RCOK;

    srvid_t srvid;
    if (mxct) {
	w_assert3(pid == lpid_t::null);
	srvid = mxct->srvid();
    } else {
	w_assert3(pid.is_remote());
	const rvol_t* rvol = remote_m::_lookup_volume(pid.vol());
	w_assert3(rvol);
	srvid = rvol->owner();
    }

    server_h server;
    W_COERCE(server.fix(srvid, FLUSH_LOG));

    log_cache_m& log = server.log_cache();

    if (lsn < log.durable_lsn) {
	 w_assert3(!mxct || mxct->logs().is_empty());
	 return RCOK;
    }
    w_assert3(!mxct || !mxct->logs().is_empty());

    W_COERCE(server.log_mutex().acquire());

    w_list_i<logbuf_t> log_iter(*(log.log_cache));
    logbuf_t* logbuf = 0;

    Buffer& buf = *(server.log_out_buf());
    remote_m::msg_log_t& msg = *(new (buf.start()) remote_m::msg_log_t());
    if (mxct) {
        msg.commit = TRUE;
        msg.commit_tid = xct()->tid();
    }
    buf.set(msg.size);

    while ((logbuf = log_iter.next()) && logbuf->lsn <= lsn) {

	int length = ((logrec_t *)(logbuf->rec))->length();
        const lpid_t& pid = ((logrec_t *)(logbuf->rec))->pid();

	const rvol_t* rvol = remote_m::_lookup_volume(pid.vol());
	lpid_t rem_pid = pid;
	rem_pid._stid.vol = rvol->owner_vid();
	((logrec_t *)(logbuf->rec))->set_pid(rem_pid);

	if (buf.remaining() > align(length)) {
	    msg.nlogs++;
	    msg.size += align(length);
	    memcpy(buf.next(), (void *)(logbuf->rec), length);
	    buf.adjust(align(length));

	} else {
	    W_COERCE(remote_m::_call(buf, emptyBox, emptyBox, srvid, server,
				server.log_to_port(), Endpoint::null));

	    msg.size = align(sizeof(remote_m::msg_log_t));
	    buf.set(msg.size);
	    msg.nlogs = 1;
	    msg.size += align(length);
	    memcpy(buf.next(), (void *)(logbuf->rec), length);
	    buf.adjust(align(length));
	}

	nlogs++;
	delete logbuf;
	log.log_size -= length;
    }

    w_assert3(buf.size() > align(sizeof(remote_m::msg_log_t)));
    w_assert3(!mxct || mxct->logs().is_empty());
    w_assert3(log.log_size >= 0);

    msg.last = TRUE;

    log.durable_lsn = lsn;
    log.durable_lsn.increment();

    server.log_mutex().release();

    W_DO(remote_m::_call(buf, emptyBox, emptyBox, srvid, server,
			server.log_to_port(), server.log_reply_in_port()));

    server.unfix();
    return RCOK;
}


rc_t remote_log_m::purge(master_xct_proxy_t& mxct)
{
    if (mxct.logs().is_empty()) return RCOK;

    server_h server;
    W_COERCE(server.fix(mxct.srvid(), LOG));

    log_cache_m& log = server.log_cache();

    if (mxct.last_lsn() < log.durable_lsn) {
         w_assert3(mxct.logs().is_empty());
         return RCOK;
    }
    w_assert3(!mxct.logs().is_empty());

    w_list_i<logbuf_t> iter(mxct.logs());
    logbuf_t* logbuf = 0;

    while (logbuf = iter.next()) {
	log.log_size -= ((logrec_t *)(logbuf->rec))->length();
	delete logbuf;
    }

    w_assert3(log.log_size >= 0); 
    log.durable_lsn = log.numlogs() ? log.log_cache->top()->lsn : log.curr_lsn;

    return RCOK;
}


bool remote_log_m::is_dirty(bfcb_t* b)
{
    if (!b->dirty) return FALSE;

    const lpid_t& pid = b->pid;
    w_assert3(pid.is_remote());

    const rvol_t* rvol = remote_m::_lookup_volume(pid.vol());
    w_assert3(rvol);
    srvid_t srvid = rvol->owner();

    server_h server;
    W_COERCE(server.fix(srvid, LOG));

    log_cache_m& log = server.log_cache();

    if (b->frame->lsn1 < log.durable_lsn) {
	b->dirty = FALSE;
	return FALSE;
    }
    return TRUE;
}


/*****************************************************************************/

/* XXX 
   The Endpoint::pos() method is a kludge for the server-to-server
   code which has nothing at all to do with communications.  I've
   replaced it with a brute-force linear search.  The better solution
   is to implement the reverse mapping with a hash table -- bolo */

endpoints_t::endpoints_t()
{
    for (int i = 0; i < n_alloc; i++) {
        W_COERCE(comm_m::comm_system()->make_endpoint(tab[i]));
#if 0
	tab[i].pos = i;
#endif
	avail[i] = TRUE;
    }
}


endpoints_t::~endpoints_t()
{
    for (int i = 0; i < n_alloc; i++) {
       if (avail[i]) W_COERCE(tab[i].destroy());
       avail[i] = FALSE;
    }
}


inline
const Endpoint& endpoints_t::get()
{
    MUTEX_ACQUIRE(mutex);
    for (int i = 0; i < n_alloc; i++) {
	if (avail[i]) {
	    avail[i] = FALSE;
	    MUTEX_RELEASE(mutex);
	    return tab[i];
	}
    }
    W_FATAL(eINTERNAL);
}


inline
void endpoints_t::put(const Endpoint& port)
{
#if 0
    w_assert1(port.pos >= 0 && port.pos < n_alloc);

    MUTEX_ACQUIRE(mutex);
    avail[port.pos] = TRUE;
    MUTEX_RELEASE(mutex);
#else
    int	i;

    MUTEX_ACQUIRE(mutex);
    for (i = 0; i < n_alloc; i++)
	    if (tab[i] == port) {
		    avail[i] = TRUE;
		    break;
	    }
    MUTEX_RELEASE(mutex);
#endif
}


/*****************************************************************************/


// Static members of class remote_m
server_tab_m*			remote_m::_server_tab = NULL;
Endpoint			remote_m::_conn_port;
endpoints_t*			remote_m::xact_ports = NULL;
hash_lru_t<rvol_t, vid_t>*	remote_m::_vol_tab = NULL;
w_hash_t<rvol_t, vid_t>*	remote_m::_inv_vol_tab = NULL;
vid_t				remote_m::_alias_counter;
smutex_t			remote_m::_alias_counter_mutex("rmtalias");
remote_log_m*			remote_m::_rlog = NULL;
remote_m::connection_thread_t*	remote_m::_conn_thread = NULL;
remote_m::msg_stats_t 		remote_m::_msg_stats[remote_m::max_msg+1];

#ifdef USE_HATESEND
bool                            remote_m::send_back;    
bool                            remote_m::discard_only;
bool                            remote_m::hate_hints=false;
#endif

const char*				remote_m::_msg_str[] = {
					"bad_msg             ",
					"connect_msg         ",
					"disconnect_msg      ",
					"shutdown_conn_msg   ",
					"mount_volume_msg    ",
					"spread_xct_msg      ",
					"prepare_xct_msg     ",
					"commit_xct_msg      ",
					"abort_xct_msg       ",
					"log_msg             ",
					"callback_msg        ",
					"abort_callback_msg  ",
					"adaptive_msg        ",
					"pids_msg            ",
					"lookup_lid_msg      ",
					"acquire_lock_msg    ",
					"read_page_msg       ",
					"purged_pages_msg    ",
					"ping_xct_msg        ",
/* SHIV */
					"read_send_page_msg  ",
					"send_page_msg       ",
					"fwd_page_msg        "
/* SHIV */
				};


remote_m::remote_m()
{
    if (comm_m::use_comm()) {

	if (!(_server_tab = new server_tab_m)) {
	    W_FATAL(eOUTOFMEMORY);
	}

	// create connection port for this server
	W_COERCE(comm_m::comm_system()->make_endpoint(_conn_port));

	// set up remote volume mount table
	_alias_counter.init_alias();
	if (!(_vol_tab = new hash_lru_t<rvol_t, vid_t>(max_volumes, "RemoteVolTab"))) {
	    W_FATAL(eOUTOFMEMORY);
	}

	if (!(_inv_vol_tab = new w_hash_t<rvol_t, vid_t>(
		max_volumes * 2, 
		offsetof(rvol_t, _owner_vid), offsetof(rvol_t, _inv_link)))) {
	    W_FATAL(eOUTOFMEMORY);
	}

	// fork thread to monitor the connection port
	_conn_thread = new connection_thread_t(this);
	if (_conn_thread == NULL) W_FATAL(eOUTOFMEMORY);
	W_COERCE(_conn_thread->fork());

	_rlog = new remote_log_m;
	if (_rlog == NULL) W_FATAL(eOUTOFMEMORY);

	xact_ports = new endpoints_t;
	if (xact_ports == NULL) W_FATAL(eOUTOFMEMORY);
    }

#ifdef USE_HATESEND    
	if (getenv("SEND")) send_back = true;
	 else send_back = false;

	if (getenv("HATE")) hate_hints = true;
	 else hate_hints = false;
    
	// NOTE THAT THIS VARIABLE HAS TO BE SET AT BOTH ENDS
	if (getenv("DISCARD"))  discard_only = true;
	 else  discard_only = false;
#endif    
    
}

remote_m::~remote_m()
{
    if (comm_m::use_comm()) {

	// shutdown the connection monitor thread
	msg_connect_t msg;
	msg.set_type(shutdown_conn_msg);
	EndpointBox my_conn_port;
	Buffer buf(&msg, msg.size());
	W_COERCE(my_conn_port.set(0, _conn_port));
	_msg_stats[msg.type()].req_send_cnt++;
	W_COERCE(_conn_port.send(buf, my_conn_port));

	// wait for thread to end
	W_COERCE(_conn_thread->wait());
	delete _conn_thread;

    }

    if (xact_ports) {
	delete xact_ports;
	xact_ports = NULL;
    }
    if (_server_tab) {
	delete _server_tab;
	_server_tab = NULL;
    }
    if (_rlog) {
	delete _rlog;
	_rlog = NULL;
    }
}


// This function monitors requests from a server.
// This function is run in a server_req_thread_t defined below.

void remote_m::monitor_server_req_port(server_s& server)
{
    Buffer&	buf = *server.req_in_buf;
    msg_hdr_t&	msg = *(msg_hdr_t*)buf.start();
    EndpointBox	new_ports;

    rc_t 	rc;
    bool	done = FALSE;

    server_h    server_fixed;

    while(!done) {
	rc = server.req_from_port.receive(buf, new_ports);
	if (rc) {
	    // TODO: implement error handling from server receive
	   // TEMPORARY FIX BUGBUG in order to remove server, we need
	   // to unmount the volume
  	   done = TRUE;
	   continue;
	    // TODO: implement error handling from server receive
	    W_FATAL(eNOTIMPLEMENTED);
	}

	TRACE(204, "server_req_port: msg received from server: " <<
	     server.serverid << " msg type: " << msg.type()
        );

	int size = msg.size() + msg.get_purged_list_size() * sizeof(lpid_t);
	w_assert3(size == buf.size());
	w_assert3(is_aligned(size));

	_delete_copies(server.serverid, msg, buf);

	W_COERCE( server_fixed.fix(server.serverid, NONE) );

	switch (msg.type()) {
	case mount_volume_msg:
	    _dispatch_msg(buf, new_ports, server_fixed, NULL,
				server.reply_to_port, Endpoint::null);
	    break;
#ifdef SHIV
	case send_page_msg:
	    _dispatch_msg(buf, new_ports, server_fixed, NULL,
				server.reply_to_port, server.req_from_port);
	    break;
	case fwd_page_msg:
	    _dispatch_msg(buf, new_ports, server_fixed, NULL,
			      server.reply_to_port, server.req_from_port);
	    break;
#endif /* SHIV */
	case spread_xct_msg:
	    _dispatch_msg(buf, new_ports, server_fixed, NULL,
				server.reply_to_port, Endpoint::null);
	    break;
        case callback_msg:
	    _dispatch_msg(buf, new_ports, server_fixed, NULL,
				Endpoint::null, Endpoint::null);
	    break;
	case abort_callback_msg:
	    _dispatch_msg(buf, new_ports, server_fixed, NULL,
				Endpoint::null, Endpoint::null);
	    break;
#ifdef OBJECT_CC
	case adaptive_msg:
	    _dispatch_msg(buf, new_ports, server_fixed, NULL,
				Endpoint::null, Endpoint::null);
	    break;
#endif

	case ping_xct_msg:
	    _dispatch_msg(buf, new_ports, server_fixed, NULL,
				server.reply_to_port, Endpoint::null);
	    break;
	case purged_pages_msg:
	    _msg_stats[msg.type()].req_rcv_cnt++;
            break;
	default:
	    ss_m::errlog->clog << error_prio << "Unknown server message at " <<
					__FILE__ << ":" << __LINE__ << flushl;
	    W_IGNORE(_reply(buf, emptyBox, RC(eBADMSGTYPE),
				    server.reply_to_port, server.serverid));
	}
	server_fixed.unfix();
    }
}


void remote_m::monitor_log_port(server_s& server)
{
    Buffer&     buf = *server.log_in_buf;
    msg_hdr_t&	msg = *(msg_hdr_t*)buf.start();
    EndpointBox	reply_port;

    rc_t        rc;
    bool      done = FALSE;

    while(!done) {
        rc = server.log_from_port.receive(buf, reply_port);
        if (rc) {
            // TODO: implement error handling from server receive
            W_FATAL(eNOTIMPLEMENTED);
        }

	TRACE(204, "log_port: msg received from server: " <<
             server.serverid << " msg type: " << msg.type()
        );

	int size = msg.size() + msg.get_purged_list_size() * sizeof(lpid_t);
        w_assert3(size == buf.size());
        w_assert3(is_aligned(size));

	_delete_copies(server.serverid, msg, buf);

	switch (msg.type()) {
	case log_msg:
	    _msg_stats[log_msg].req_rcv_cnt++;
	    _redo_logs_srv(buf, server.serverid, server.log_reply_out_port);
	    break;
	case purged_pages_msg:
            _msg_stats[msg.type()].req_rcv_cnt++;
            break;
	case disconnect_msg:
	    W_FATAL(eNOTIMPLEMENTED);
	default:
	    cerr << "Unknown log msg at " << __FILE__ <<":"<< __LINE__ << endl;
	}
    }
}


// This function monitors requests from a server.
// This function is run in a server_req_thread_t defined below.

void remote_m::monitor_xct_port(remote_xct_proxy_t* rxct)
{
    Buffer&	buf = *rxct->req_buf();
    msg_hdr_t&	msg = *(msg_hdr_t*)buf.start();
    EndpointBox	dummy;

    rc_t 	rc;
    bool	done = FALSE;

    // attache this thread to the transaction
    me()->attach_xct(rxct->xct());

    while(!done) {
	rc = rxct->req_port().receive(buf, dummy);
	if (rc) {
	    // TODO: implement error handling from xct port receive
	    W_FATAL(eNOTIMPLEMENTED);
	}
        w_assert3(dummy.is_empty());

	TRACE(205, "xct_req_port: msg received: " << " msg type: " <<
	    msg.type() << " from server: " << rxct->server() << 
	    " master xct: " << rxct->master_tid() 
        );

	int size = msg.size() + msg.get_purged_list_size() * sizeof(lpid_t);
	w_assert3(size == buf.size());
	w_assert3(is_aligned(size));

	_delete_copies(rxct->server(), msg, buf);

	switch (msg.type()) {
	case prepare_xct_msg:
	    break;
	case commit_xct_msg:
	case abort_xct_msg:
	    done = TRUE;
	    break;
	case pids_msg:
	    break;
	case lookup_lid_msg:
	    break;
	case acquire_lock_msg:
	    break;
	case read_page_msg:
	    break;
#ifdef SHIV
	case read_send_page_msg:
	    break;
#endif /* SHIV */
	case ping_xct_msg:
	    break;
	case purged_pages_msg:
	    _msg_stats[msg.type()].req_rcv_cnt++;
	    continue;
	default:
	    ss_m::errlog->clog << error_prio << "Unknown xact message at " <<
					__FILE__ << ":" << __LINE__ << flushl;
	    W_IGNORE(_reply(buf, emptyBox, RC(eBADMSGTYPE),
				    rxct->reply_port(), rxct->server()));
	}
	w_assert3(rxct->xct() == xct());

	// call the appropriate _srv function
	_dispatch_msg(buf, dummy, server_h::null, rxct,
			      rxct->reply_port(), rxct->req_port());
    }
    rxct->set_thread(0);
    delete rxct;
}


void remote_m::monitor_conn_port() 
{
    msg_connect_t	msg;
    EndpointBox		requestor_ports;
    Endpoint		connector_port;
    Buffer		buf(&msg, align(sizeof(msg_connect_t)));

    rc_t 	rc;
    bool	done = FALSE;

    while(!done) {
	W_COERCE(_conn_port.receive(buf, requestor_ports));

	switch (msg.type()) {
	case connect_msg:
	    break;
	case shutdown_conn_msg:
	    W_COERCE(requestor_ports.get(0, connector_port));
	    if (_conn_port == connector_port) {
		done = TRUE;
	    } else {
		//ignore the message
		ss_m::errlog->clog << error_prio << "shutdown_conn from illegal port" << flushl;
		continue;
	    }
	    break;
	default:
	    W_FATAL(eINTERNAL);
	}
	if (done) continue;

	_dispatch_msg(buf, requestor_ports, server_h::null, NULL,
					Endpoint::null, Endpoint::null);
    }
}


void remote_m::print_msg_stats(ostream& o)
{
    for (int i = 0; i <= remote_m::max_msg; i++) {
	o << info_prio
	  << _msg_str[i] << "req_send    " << _msg_stats[i].req_send_cnt << endl
	  << _msg_str[i] << "req_rcv     " << _msg_stats[i].req_rcv_cnt << endl
	  << _msg_str[i] << "reply_rcv   " << _msg_stats[i].reply_rcv_cnt <<endl
	  << _msg_str[i] << "reply_send  " << _msg_stats[i].reply_send_cnt<<endl
	  << _msg_str[i] << "reply_error " << _msg_stats[i].reply_send_error_cnt
	  << endl << endl;
    }
}


void remote_m::reset_msg_stats()
{
    for (int i = 0; i <= remote_m::max_msg; i++) {
	_msg_stats[i].req_send_cnt = 0;
	_msg_stats[i].req_rcv_cnt = 0;
	_msg_stats[i].reply_rcv_cnt = 0;
	_msg_stats[i].reply_send_cnt = 0;
	_msg_stats[i].reply_send_error_cnt = 0;
	_msg_stats[i].reply_send_skip_cnt = 0;
    }
}


void remote_m::msg_stats(sm_stats_info_t& stats)
{
    stats.connect_req_sent = _msg_stats[connect_msg].req_send_cnt;
    stats.disconnect_req_sent = _msg_stats[disconnect_msg].req_send_cnt;
    stats.shutdown_conn_req_sent = _msg_stats[shutdown_conn_msg].req_send_cnt;
    stats.mount_vol_req_sent = _msg_stats[mount_volume_msg].req_send_cnt;
    stats.spread_xct_req_sent = _msg_stats[spread_xct_msg].req_send_cnt;
    stats.prepare_xct_req_sent = _msg_stats[prepare_xct_msg].req_send_cnt;
    stats.commit_xct_req_sent = _msg_stats[commit_xct_msg].req_send_cnt;
    stats.abort_xct_req_sent = _msg_stats[abort_xct_msg].req_send_cnt;
    stats.log_req_sent = _msg_stats[log_msg].req_send_cnt;
    stats.callback_req_sent = _msg_stats[callback_msg].req_send_cnt;
    stats.abort_callback_req_sent = _msg_stats[abort_callback_msg].req_send_cnt;
    stats.pids_req_sent = _msg_stats[pids_msg].req_send_cnt;
    stats.lookup_lid_req_sent = _msg_stats[lookup_lid_msg].req_send_cnt;
    stats.acquire_lock_req_sent = _msg_stats[acquire_lock_msg].req_send_cnt;
    stats.read_page_req_sent = _msg_stats[read_page_msg].req_send_cnt;
    stats.purged_pages_req_sent = _msg_stats[purged_pages_msg].req_send_cnt;
    stats.ping_xct_req_sent = _msg_stats[ping_xct_msg].req_send_cnt;

    stats.connect_req_rcv = _msg_stats[connect_msg].req_rcv_cnt;
    stats.disconnect_req_rcv = _msg_stats[disconnect_msg].req_rcv_cnt;
    stats.shutdown_conn_req_rcv = _msg_stats[shutdown_conn_msg].req_rcv_cnt;
    stats.mount_vol_req_rcv = _msg_stats[mount_volume_msg].req_rcv_cnt;
    stats.spread_xct_req_rcv = _msg_stats[spread_xct_msg].req_rcv_cnt;
    stats.prepare_xct_req_rcv = _msg_stats[prepare_xct_msg].req_rcv_cnt;
    stats.commit_xct_req_rcv = _msg_stats[commit_xct_msg].req_rcv_cnt;
    stats.abort_xct_req_rcv = _msg_stats[abort_xct_msg].req_rcv_cnt;
    stats.log_req_rcv = _msg_stats[log_msg].req_rcv_cnt;
    stats.callback_req_rcv = _msg_stats[callback_msg].req_rcv_cnt;
    stats.abort_callback_req_rcv = _msg_stats[abort_callback_msg].req_rcv_cnt;
    stats.pids_req_rcv = _msg_stats[pids_msg].req_rcv_cnt;
    stats.lookup_lid_req_rcv = _msg_stats[lookup_lid_msg].req_rcv_cnt;
    stats.acquire_lock_req_rcv = _msg_stats[acquire_lock_msg].req_rcv_cnt;
    stats.read_page_req_rcv = _msg_stats[read_page_msg].req_rcv_cnt;
    stats.purged_pages_req_rcv = _msg_stats[purged_pages_msg].req_rcv_cnt;
    stats.ping_xct_req_rcv = _msg_stats[ping_xct_msg].req_rcv_cnt;

    stats.callback_reply_rcv = _msg_stats[callback_msg].reply_rcv_cnt;

    stats.lookup_lid_reply_send_err = _msg_stats[lookup_lid_msg].reply_send_error_cnt;
    stats.acquire_lock_reply_send_err = _msg_stats[acquire_lock_msg].reply_send_error_cnt;
    stats.read_page_reply_send_err = _msg_stats[read_page_msg].reply_send_error_cnt;
}


xct_t* remote_m::master_to_remote(const tid_t& tid, const srvid_t& srvid)
{
    xct_t* xct;
    xct_i  xct_iter;

    while (xct = xct_iter.next()) {
        if (xct->master_site() &&
                xct->master_site()->server() == srvid &&
                xct->master_site()->master_tid() == tid){
            break;
        }
    }
    w_assert3(xct != NULL);
    return xct;
}


xct_t* remote_m::remote_to_master(const tid_t& tid)
{
    xct_t* xct;
    xct_i  xct_iter;

    while (xct = xct_iter.next()) {
        if (xct->is_master_proxy()) {
            master_xct_proxy_t* master;
            w_list_i<master_xct_proxy_t> iter(xct->master_proxy_list());
            while(master = iter.next()) {
                if (master->remote_tid() == tid) break;
            }
            if (master) break;
        }
    }
    return xct;
}


/*
 * Function fix_server returns a latched server_s when it.
 * completes without error.  This server_s must be unlatched by the caller.
 */
rc_t remote_m::fix_server(const srvid_t& srvid, server_s*& server,
						srv_access_type_t access)
{
    W_DO(_server_tab->fix(srvid, access, server));
    return RCOK;
}


void remote_m::unfix_server(server_s* server, srv_access_type_t access)
{
    _server_tab->unfix(server->serverid, access);
    return;
}


const rvol_t* remote_m::_lookup_volume(vid_t alias_vid)
{
    w_assert3(alias_vid.is_alias());

    bool      found = FALSE;
    const rvol_t* rvol = _vol_tab->find(alias_vid, found);

    // it is safe to release the mutex since rvol is used in such
    // a way that it will never be removed while we are using it
    if (rvol) _vol_tab->release_mutex();
    return rvol;
}


const rvol_t* remote_m::_inv_lookup_volume(vid_t owner_vid)
{
    w_assert3(! owner_vid.is_alias());

    _vol_tab->acquire_mutex();
    const rvol_t* rvol = _inv_vol_tab->lookup(owner_vid);

    // it is safe to release the mutex since rvol is used in such
    // a way that it will never be removed while we are using it
    _vol_tab->release_mutex();
    return rvol;
}


rc_t remote_m::register_volume(const lvid_t& lvid)
{
    char tmp[80];
    ostrstream s(tmp, sizeof(tmp));
    s << lvid << ends;

    TRACE(251, "Registering volume: lvid: " << lvid << " my conn_port: ";
	_conn_port.print(_debug.clog)
    );

    if (comm_m::use_comm()) {
	rc_t rc = comm_m::name_service()->cancel(tmp);	// for now
	if (rc && rc.err_num() != nsNOTFOUND) return rc.reset();

        W_DO(comm_m::name_service()->enter(tmp, _conn_port));
    }
    return RCOK;
}


rc_t remote_m::mount_volume(const lvid_t& lvid)
{
    vid_t       vid;    // volume id on this server
    vid_t       rvid;   // volume id on remote server
    lpid_t	dir_root; // root page of vol directory index at remote server

    Endpoint owner_conn_port;
    W_DO(_find_volume_server(lvid, owner_conn_port));

    if (&owner_conn_port == &Endpoint::null) return RC(eBADVOL);
    if (owner_conn_port == _conn_port) {
        W_FATAL(eINTERNAL);	// we're the server for this volume!
    }

    srvid_t srvid;
    server_h server;

    // cout << "remote_m::mount_volume -- Attempting connection" << endl;

    W_COERCE(server.connect(owner_conn_port, srvid)); // connect if necessary

    // cout << "remote_m::mount_volume -- Attempting remote mount" << endl;

    // Ask the remote server to mount the volume.
    // It is possible it is already mounted.  In that case just exit.
    rc_t rc = _mount_volume_cli(server, lvid, rvid, dir_root);
    if (rc) {
        if (rc.err_num() == eALREADYMOUNTED) {
            // make sure the volume is mounted
            w_assert3(lid->is_mounted(lvid));
            return RCOK;
        } else {
	    return rc.reset();
	}
    }

    // cout << "remote_m::mount_volume -- remote mount done" << endl;

    W_COERCE(_add_new_volume(srvid, rvid, vid));

    dir_root._stid.vol = vid;
    W_COERCE(dir->mount_remote(vid, dir_root));

    server.incr_vol_cnt(1);

    // cout << "remote_m::mount_volume" << "lvid: " << lvid << "vid: " << vid << endl;

    // add volume to lid table
    W_DO(lid->add_remote_volume(vid, lvid));

    return RCOK;
}


rc_t remote_m::_find_volume_server(const lvid_t& lvid, Endpoint& owner)
{
    char lvid_tmp[80];
    ostrstream s(lvid_tmp, sizeof(lvid_tmp));
    s << lvid << ends;

    if (comm_m::use_comm()) {
        W_DO_PUSH(comm_m::name_service()->lookup(lvid_tmp, owner), eCOMM);
    }

    TRACE(252, "Looked up volume: lvid: " << lvid << " owner conn_port: ";
	owner.print(_debug.clog)
    );

    return RCOK;
}


// Add a volume to _vol_tab and to lid_m
rc_t remote_m::_add_new_volume(srvid_t srvid, vid_t owner_vid, vid_t& vid)
{
    W_DO(_alias_counter_mutex.acquire());
    auto_release_t<smutex_t> auto_release(_alias_counter_mutex);

    _alias_counter.incr_alias();

    // Try to add the volume Id in _alias_counter.  If it
    // is already in use, try another
    rvol_t*     rvol = NULL;
    bool      found;
    bool      is_new;
    while (!rvol) { // try until we mount a volume
        rvol = _vol_tab->grab(_alias_counter, found, is_new);
        if (found) {
            // try some other volume id
            _vol_tab->release_mutex();
            rvol = NULL;
            _alias_counter.incr_alias();
        } else {
            if (!is_new) {
                // dismount the volume
                W_FATAL(eNOTIMPLEMENTED);
            } else {
                vid = _alias_counter;
                rvol->init(vid, owner_vid, srvid);
		_inv_vol_tab->push(rvol);
            }
        }
    }

    w_assert3(rvol);
    _vol_tab->release_mutex();

    return RCOK;
}


rc_t remote_m::_mount_volume_cli(server_h& server,
			const lvid_t& lvid, vid_t& rvid, lpid_t& dir_root)
{
    w_assert3(server.fixed());

    Buffer& buf = *server.req_out_buf();
    msg_mount_t& msg = *(new (buf.start()) msg_mount_t(mount_volume_msg, lvid));
    buf.set(msg.size());
    EndpointBox dummy;
    W_DO(_call_server(buf, dummy, server, TRUE));
    w_assert3(dummy.is_empty());

    rvid = msg.local_vid;
    dir_root = msg.dir_root;
    w_assert3(rvid != 0);
    return RCOK;
}


rc_t remote_m::_mount_volume_srv(Buffer& msg_buf, server_h& from_server)
{
    msg_mount_t& msg = *(msg_mount_t*)msg_buf.start();

    // cout << "remote_m::_mount_volume_srv -- received request" << endl;

    W_DO(lid->lookup(msg.lvid, msg.local_vid));

    if (from_server.is_mounted(msg.local_vid)) {
        return RC(eALREADYMOUNTED);
    }
    from_server.mark_mounted(msg.local_vid);

    // return the root page of the volume directory index
    stid_t stid;
    stid.vol = msg.local_vid;
    stid.store = 1;
    W_DO(io->first_page(stid, msg.dir_root));

    // cout << "remote_m::_mount_volume_srv -- sending reply" << endl;

    W_COERCE(_reply(msg_buf, emptyBox, RCOK, from_server.reply_to_port(),
							from_server.id()));
    return RCOK;
}


/*
 * Function _connect_to returns a latched server_s when it
 * completes without error.  This server_s must be unlatched by the caller.
 */
rc_t remote_m::connect_to(const Endpoint& conn_port, srvid_t& srvid,
							server_s*& server)
{
    bool	found;
    bool 	is_new;
    rc_t	rc;

    W_DO(_server_tab->grab(conn_port, srvid, server, found, is_new));
    w_assert1(server); 

    // Need to send the connection req to the remote server if either no
    // connection exists or a connection exists but it has been established
    // after a request from the remote server, not from this server.

    if (!found || !server->connected_to) {

	// If an active (but not "in_use" (see server_s)) connection is chosen
	// as a victim for replecement, then shut down the connection.

	if (!is_new && server->connected_to) {
	    w_assert1(!server->connected_from);
	    disconnect_from(server);
	}

	if (!found) server->init(srvid, conn_port);

	// send death notification to this port
	// W_COERCE(server->req_from_port.notify_us_about(conn_port));
    
	msg_connect_t msg(connect_msg);
	EndpointBox my_ports;
	EndpointBox remote_ports;

	W_COERCE(my_ports.set(0, _conn_port));
	W_COERCE(my_ports.set(1, server->req_from_port));
	W_COERCE(my_ports.set(2, server->reply_from_port));
	W_COERCE(my_ports.set(3, server->log_from_port));
	W_COERCE(my_ports.set(4, server->log_reply_in_port));

	Buffer buf(&msg, msg.size());
	server_h dummy;
	rc = _call(buf, my_ports, remote_ports, srvid_t::null, dummy,
				server->conn_port, server->reply_from_port);
	if (rc) {
	    server->clear();
	} else {
	    server->connected_to = TRUE;
	    W_COERCE(remote_ports.get(0, server->req_to_port));
	    W_COERCE(remote_ports.get(1, server->reply_to_port));
	    W_COERCE(remote_ports.get(2, server->log_to_port));
	    W_COERCE(remote_ports.get(3, server->log_reply_out_port));
	}

	_server_tab->publish();

	if (rc) {
	    _server_tab->unfix(srvid, REQ);
	    return rc.reset();
	}
	
	w_assert3(server->connected_to == TRUE);

	// start a thread to monitor requests
	server->req_thread = new server_req_thread_t(server);
	if (!server->req_thread) W_FATAL(eOUTOFMEMORY);
	W_COERCE(server->req_thread->fork());

    } else {
	_server_tab->publish();
    }
    return RCOK;
}


rc_t remote_m::_connect_srv(Buffer& msg_buf, EndpointBox& ports)
{
    FUNC(remote_m::_connect_srv);

    // cout << "remote_m::_connect_srv -- received req" << endl;

    msg_connect_t& msg = *(msg_connect_t*)msg_buf.start();
    w_assert1(msg.size() == msg_buf.size());

    Endpoint conn_port;
    W_COERCE(ports.get(0, conn_port));

    srvid_t	srvid;
    server_s*   server;
    bool      found;
    bool      is_new;
    rc_t        rc;

    W_DO(_server_tab->grab(conn_port, srvid, server, found, is_new));
    w_assert3(server);
   
    if (!found) {
        w_assert1(is_new || (!server->connected_to && !server->connected_from));

        server->init(srvid, conn_port);

        W_COERCE(ports.get(1, server->req_to_port));
        W_COERCE(ports.get(2, server->reply_to_port));
	W_COERCE(ports.get(3, server->log_to_port));
	W_COERCE(ports.get(4, server->log_reply_out_port));

        // send death notification to this port
        // W_COERCE(server->req_from_port.notify_us_about(global_id));

	W_COERCE(ports.set(0, server->req_from_port));
	W_COERCE(ports.set(1, server->reply_from_port));
	W_COERCE(ports.set(2, server->log_from_port));
	W_COERCE(ports.set(3, server->log_reply_in_port));

        rc = _reply(msg_buf, ports, RCOK, server->reply_to_port, srvid_t::null);
        if (rc) {
            server->clear();
            DBG(<< "connection response failed to node ");
        } else {
            server->connected_from = TRUE;
            DBG(<< "connection response sent to node ");
        }
        _server_tab->publish();

    } else {

        w_assert1(!server->connected_from);
        w_assert1(&(server->req_to_port) != &Endpoint::null);
        server->connected_from = TRUE;
	_server_tab->publish();
    }

    // if the connection was made, start threads to monitor requests and logs
    if (server->connected_from == TRUE) {
        w_assert3(!server->req_thread);
        server->req_thread = new server_req_thread_t(server);
        if (!server->req_thread) W_FATAL(eOUTOFMEMORY);
	W_COERCE(server->req_thread->fork());

        w_assert3(!server->log_thread);
        server->log_thread = new log_thread_t(server);
        if (!server->log_thread) W_FATAL(eOUTOFMEMORY);
	W_COERCE(server->log_thread->fork());
    }

    _server_tab->unfix(srvid, REQ);
    return RCOK;
}


/*
 * Function _disconnect_from accepts a pinned server_s.
 */
rc_t remote_m::disconnect_from(server_s* server)
{
    FUNC(remote_m::disconnect_from);
    //TODO: implement remote_m::_disconnect_from
    W_FATAL(eNOTIMPLEMENTED);

    return RCOK;
}


rc_t remote_m::_disconnect_srv(Buffer& msg_buf, server_h& from_server,
                const Endpoint& reply_port)
{
    FUNC(remote_m::_disconnect_srv);
    //TODO: implement remote_m::_disconnect_srv()
    W_FATAL(eNOTIMPLEMENTED);
    return RCOK;
}


rc_t remote_m::spread_xct(const vid_t& vid)
{
    master_xct_proxy_t* mxct = 0;
    vid_t remote_vid;
    W_DO(_lookup_master_proxy(vid, mxct, remote_vid));
    return RCOK;
}


// Lookup master_xct_proxy_t for volume vid.  Also, return
// the vid as known on the remote_server
rc_t remote_m::_lookup_master_proxy(vid_t vid, master_xct_proxy_t*& mxct,
							vid_t& remote_vid)
{
    xct_t* xct = me()->xct();
    w_assert3(xct);

    // find server for this volume
    const rvol_t* rvol = _lookup_volume(vid);
    if (!rvol) { return RC(eBADVOL); }
    remote_vid = rvol->owner_vid();

    // find master_xct_proxy for volume owning server 
    w_list_i<master_xct_proxy_t> scan((xct->master_proxy_list()));
    while(scan.next()) {
	if (scan.curr()->srvid() == rvol->owner()) {
	    mxct = scan.curr();
	    return RCOK;
	}
    }
    // no master_xct_proxy_t for this server, so spread xct to server

    master_xct_proxy_t* tmp = new master_xct_proxy_t(xct, rvol->owner());
    if (!tmp) return RC(eOUTOFMEMORY);
    w_auto_delete_t<master_xct_proxy_t> auto_del(tmp);
    W_DO(_spread_xct(tmp));
    auto_del.set(0);  //cancel auto delete

    xct->master_proxy_list().append(tmp);
    mxct = tmp;
    return RCOK;
}


rc_t remote_m::_spread_xct(master_xct_proxy_t* mxct)
{
    server_h server;
    W_COERCE(server.fix(mxct->srvid(), REQ));

    Endpoint	req_port;
    tid_t 	remote_tid;
    W_DO(_spread_xct_cli(mxct, server, remote_tid, req_port));

    mxct->init(remote_tid, req_port);
    w_assert3(mxct->remote_tid() != tid_t::null);
    server.incr_xct_cnt(1);

    TRACE(180, "spread_xct; remote tid = " << mxct->remote_tid());

    return RCOK;
}


rc_t remote_m::_spread_xct_cli(master_xct_proxy_t* mxct,
		 server_h& server, tid_t& remote_tid, Endpoint& req_port)
{
    Buffer& buf = *mxct->req_buf();
    msg_xct_t& msg = *(new (buf.start()) msg_xct_t(spread_xct_msg));
    buf.set(msg.size());

    msg.tid = mxct->xct()->tid();
    msg.trace_level = me()->trace_level;

    EndpointBox port;
    W_COERCE(port.set(0, mxct->reply_port()));

    W_DO(_call_server(buf, port, server, TRUE));

    // remember tid and req_port at remote server
    remote_tid = msg.tid;
    W_COERCE(port.get(0, req_port));

    return RCOK;
}


rc_t remote_m::_spread_xct_srv(Buffer& msg_buf, EndpointBox& master_port,
							server_h& from_server)
{
    msg_xct_t&  msg = *(msg_xct_t*)msg_buf.start();
    Endpoint xct_reply_port;
    W_COERCE(master_port.get(0, xct_reply_port));

    remote_xct_proxy_t* rxct = new remote_xct_proxy_t(msg.tid,
                                        from_server.id(), xct_reply_port);
    if (!rxct) return RC(eOUTOFMEMORY);
    w_auto_delete_t<remote_xct_proxy_t> auto_del(rxct);

    // see if we already have an xct_t for this transaction
    xct_i       xct_iter;
    xct_t*      xct;
    while (xct = xct_iter.next()) {
        if (xct->master_site() != NULL) {
            if (xct->master_site()->server() == rxct->server() &&
		xct->master_site()->master_tid() == rxct->master_tid()) {
                break;
            }
        }
        xct = NULL;
    }
    if (xct) {
        // already have an xct for this site.
        // make sure it's not the server we just got a spread_xct request from
        w_assert3(xct->master_site()->server() != rxct->server());

        // we don't actually support being a remote server
        // for the same transaction at multiple client sites
        W_FATAL(eNOTIMPLEMENTED);
    } else {
        xct = new xct_t(WAIT_FOREVER, xct_t::t_remote);
        if (!xct) return RC(eOUTOFMEMORY);
	me()->detach_xct(xct);
        xct->set_master_site(rxct);
    }
    rxct->set_xct(xct);
    rxct->thread()->trace_level = msg.trace_level;

    // start thread to monitor request port
    // already done in rxct constructor

    // return the tid at this site and
    // the port where reqs from this xact are accepted.
    msg.tid = xct->tid();
    W_COERCE(master_port.set(0, rxct->req_port()));

    auto_del.set(0);  //cancel auto delete
    W_IGNORE(_reply(msg_buf, master_port, RCOK, from_server.reply_to_port(),
							from_server.id()));
    return RCOK;
}


rc_t remote_m::send_callbacks(const callback_op_t& cbop)
{
    Buffer& buf = *(cbop.reply_from_buf);
    callback_t* cb;
    w_list_i<callback_t> iter( (w_list_t<callback_t>&) cbop.callbacks );

    while (cb = iter.next()) {

	int msg_size = 0;

	EndpointBox reply_port;
	W_COERCE(reply_port.set(0, cbop.reply_from_port));

	if (xct()->is_remote_proxy()) {
            msg_callback_t& msg = *(new (buf.start()) msg_callback_t(
		callback_msg, cbop.cb_id, cbop.name, cbop.mode,
#ifdef HIER_CB
		cbop.page_dirty,
#else
		FALSE,
#endif /* HIER_CB */
		cb->server, xct()->tid(), xct()->master_site()->master_tid(),
		me()->trace_level));
	    msg_size = msg.size();

	    // it is ok to access the server_s struct of the master server of
	    // the xact without protection since this connection cannot be
	    // replaced (it is in use).
	    server_h server;
	    W_COERCE(server.fix(xct()->master_site()->server(), NONE));
	    W_COERCE(reply_port.set(1, server.conn_port()));
	} else {
	    msg_callback_t& msg = *(new (buf.start()) msg_callback_t(
	        callback_msg, cbop.cb_id, cbop.name, cbop.mode,
#ifdef HIER_CB
		cbop.page_dirty,
#else
		FALSE,
#endif /* HIER_CB */
	        cb->server, xct()->tid(), xct()->tid(), me()->trace_level));
	    msg_size = msg.size();

	    W_COERCE(reply_port.set(1, _conn_port));
        }

        buf.set(msg_size);

	server_h server;
	W_COERCE(server.fix(cb->server, REQ)); // Assume connection exists ??
	server.incr_xct_cnt(1);

	TRACE(101, "sending cb req for cb: "<<cbop.cb_id<<" to: "<< cb->server);

        W_DO(_call_server(buf, reply_port, server, FALSE));
    }
    return RCOK;
}


rc_t remote_m::get_cb_replies(callback_op_t& cbop)
{
    Buffer&		buf = *cbop.reply_from_buf;
    msg_callback_t&	msg = *(msg_callback_t*)buf.start();
    EndpointBox		dummy;

    bool done = false;

    while (!done) {
	// TODO: handle error
        W_COERCE(cbop.reply_from_port.receive(buf, dummy));
	w_assert3(dummy.is_empty());

 	w_assert3(msg.type() == callback_msg || msg.type() == purged_pages_msg);
        int size = msg.size() + msg.get_purged_list_size() * sizeof(lpid_t);
        w_assert3(size == buf.size());
	w_assert3(is_aligned(size));

	if (msg.type() == purged_pages_msg) {
	    TRACE(106, "received purged_pages_msg while waiting for cb reply");
        } else {
	    _msg_stats[callback_msg].reply_rcv_cnt++;
	    if (msg.outcome != callback_op_t::LOCAL_DEADLOCK) {
	        TRACE(107, "received cb reply: id: " << msg.cb_id <<
			" name: " << msg.name << " from client: " <<
			msg.cb_srv << " outcome: " << msg.outcome << endl
			<< " block_level: " << msg.block_level
	    	);
	    } else {
		TRACE(107, "received local deadlock notification: cb-id: " <<
			msg.cb_id << " name: " << msg.name << " from xct: " <<
			msg.cb_tid
		);
	    }
	}

	_delete_copies(msg.cb_srv, msg, buf);

	if (msg.type() == purged_pages_msg) {
	    _msg_stats[msg.type()].req_rcv_cnt++;
	    continue;
	}

	// TODO: handle remote error
	if (msg.error()) { W_FATAL(eNOTIMPLEMENTED); }
	w_assert3(msg.name == cbop.name);
	w_assert3(msg.remote_tid == xct()->tid());
	w_assert3(msg.outcome != callback_op_t::NONE);
	w_assert3(msg.cb_id == cbop.cb_id);

	callback_t* cb = 0;
	if (msg.outcome != callback_op_t::LOCAL_DEADLOCK) {
	    w_list_i<callback_t> iter(cbop.callbacks);
	    while ((cb = iter.next()) && (cb->server != msg.cb_srv));
	    w_assert1(cb);
	}

	blocker_t* blockers = 0;
	if (msg.num_blockers > 0) {
	    blockers = new blocker_t[msg.num_blockers];
	    w_assert1(blockers);
	    char* off = (char *)&msg + align(sizeof(msg_callback_t));
	    for (int i = 0; i < msg.num_blockers; i++) {
		memcpy((char *)&blockers[i], off, sizeof(blocker_t));
		off += sizeof(blocker_t);
	    }
	    TRACE(108, "blockers in cb reply: id: "<< msg.cb_id<<" blockers: ";
		for (int j = 0; j < msg.num_blockers; j++) {
		    _debug.clog << blockers[j].tid << ' ';
		}
		_debug.clog
	    );
	}
	rc_t rc = cbm->handle_cb_reply(cbop, cb, msg.outcome, done,
				msg.num_blockers, msg.block_level, blockers);

#ifdef DEBUG
	if (msg.outcome != callback_op_t::LOCAL_DEADLOCK && (
			msg.outcome == callback_op_t::PINV ||
			msg.outcome == callback_op_t::OINV ||
			msg.outcome == callback_op_t::DEADLOCK ||
			msg.outcome == callback_op_t::KILLED )) {
	    w_assert3(cb == 0);
	}
#endif

	if (msg.outcome != callback_op_t::LOCAL_DEADLOCK && cb == 0) {
	    w_assert3(msg.outcome == callback_op_t::PINV ||
			msg.outcome == callback_op_t::OINV ||
			msg.outcome == callback_op_t::DEADLOCK ||
			msg.outcome == callback_op_t::KILLED);
	    server_h server;
	    server.fix(msg.cb_srv, REQ);
	    server.incr_xct_cnt(-1);
	}

	if (rc) return rc.reset();
    }
    return RCOK;
}


rc_t remote_m::send_callback_aborts(callback_op_t& cbop)
{
    Buffer* buf = new Buffer(remote_m::max_msg_size);
    w_assert1(buf);
    callback_t* cb;

    w_list_i<callback_t> iter(cbop.callbacks);
    while (cb = iter.next()) {
	msg_abort_callback_t* msg = new (buf->start()) msg_abort_callback_t(
		abort_callback_msg, cbop.cb_id, cbop.name,
		cb->server, xct()->tid(), cb->cb_tid);
	buf->set(msg->size());

	server_h server;
	W_COERCE(server.fix(cb->server, REQ));

	TRACE(109,
	    "sending abort-cb req for: id: " << cbop.cb_id << " name: " <<
	    cbop.name << " to server: " << cb->server
	);

	W_DO(_call_server(*buf, emptyBox, server, FALSE));
    }
    delete buf;
    return RCOK;
}


rc_t remote_m::_callback_srv(Buffer& msg_buf, EndpointBox& reply_port,
							server_h& from_server)
{
    msg_callback_t&  msg = *(msg_callback_t*)msg_buf.start();
    Endpoint cb_reply_port, master_conn_port;
    W_COERCE(reply_port.get(0, cb_reply_port));
    W_COERCE(reply_port.get(1, master_conn_port));

    const rvol_t* vol = _inv_lookup_volume(msg.name.vid());
    lockid_t name = msg.name;
    name.vid() = vol->alias_vid();

    callback_xct_proxy_t* cbxct = new callback_xct_proxy_t(msg.cb_id, name, 
			msg.mode, msg.page_dirty,
			from_server.id(), master_conn_port,
			msg.remote_tid, msg.master_tid, cb_reply_port);
    if (!cbxct) return RC(eOUTOFMEMORY);
    w_auto_delete_t<callback_xct_proxy_t> auto_del(cbxct);

#ifdef DEBUG
    // Check that there is no callback proxy for the same xact
    {
	xct_i  xct_iter;
	xct_t* xd;
	while (xd = xct_iter.next()) {
	    if (xd->is_cb_proxy()) {
		if (xd->cb_proxy()->global_id() == cbxct->global_id()) {
		     W_FATAL(eCBEXISTS);
		}
	    }
	}
    }
#endif

    xct_t* xct = new xct_t(WAIT_FOREVER, xct_t::t_callback);
    if (!xct) return RC(eOUTOFMEMORY);
    cbxct->set_xct(xct);
    xct->set_cb_proxy(cbxct);

    cbxct_thread_t* cb_thread = new cbxct_thread_t(cbxct);
    if (!cb_thread) { return RC(eOUTOFMEMORY); }

    rc_t e = cb_thread->fork();
    if (e != RCOK) {
	    delete cb_thread;
	    return e;
    }
    
    cbxct->set_thread(cb_thread);
    cbxct->get_thread()->trace_level = msg.trace_level;

    me()->detach_xct(xct);

    msg.cb_tid = xct->tid();
    cbxct->reply_buf()->get(msg_buf);	// copy msg into the reply buf of cbxct

    TRACE(102,
	"received cb req: id: " << msg.cb_id << " name: " << msg.name <<
	"owner_srv: " << from_server.id() << 
//	" master_srv: " << master_conn_port <<
	" cb_xct_id: " << xct->tid());

    auto_del.set(0);  //cancel auto delete
    return RCOK;
}


rc_t remote_m::_abort_callback_srv(Buffer& msg_buf, server_h& from_server)
{
    msg_abort_callback_t& msg = *(msg_abort_callback_t*)msg_buf.start();

// BUGBUG: Critical Section; xd pointer needs protection in case of
// preemptive threads

    TRACE(104,
	"received abort-cb req: id: " << msg.cb_id << " name: " << msg.name <<
	endl << "remote_tid: " << msg.remote_tid << " cb_tid: " << msg.cb_tid);

    bool found;
    xct_i  xct_iter;
    xct_t* xd;
    while (xd = xct_iter.next()) {

	if (msg.cb_tid != tid_t::null) {
	    found = xd->is_cb_proxy() && xd->tid() == msg.cb_tid;
	} else {
	    found = xd->is_cb_proxy() &&
			xd->cb_proxy()->remote_tid() == msg.remote_tid &&
			xd->cb_proxy()->remote_server() == from_server.id();
	}

	if (found) {
	    TRACE(105, "cb xct found for cb id: " << msg.cb_id);

	     // assume xacts release their mutex when blocked
	    me()->attach_xct(xd);

	    w_assert3(xd->state() == xct_active);
	    xd->change_state(xct_ended);

	    if (xd->cb_proxy()->get_thread()->status()==smthread_t::t_blocked) {
		TRACE(105, "cb xct found for cb id: " << msg.cb_id <<
						" xct is waiting for lock");
		w_assert3(xd->lock_info()->wait->status() == lock_m::t_waiting);
		xd->lock_info()->wait->status(lock_m::t_aborted);
		xd->lock_info()->wait = 0;

		// We unblock the callback xct with a special error so that it
		// will be able to differentiate the case where we get a
		// deadlock notification from the server from the case where
		// the cb xct is aborted due to a locally detected deadlock.
	        xd->cb_proxy()->get_thread()->unblock(RC(eREMOTEDEADLOCK));
	    } else {
	       TRACE(105, "cb xct found for cb id: " << msg.cb_id <<
						" xct is NOT waiting for lock");
	    }
	    me()->detach_xct(xd);
	    break;
	}
    }
    if (xd == 0) { TRACE(105, "cb xct NOT found for cb id: " << msg.cb_id); }
    return RCOK;
}


rc_t remote_m::send_cb_reply(const callback_xct_proxy_t& cbxct,
		cb_outcome_t outcome,
		int numblockers, blocker_t* blockers, uint2 block_level)
{
    Buffer& buf = *cbxct.reply_buf();
    msg_callback_t&  msg = *(msg_callback_t*)buf.start();
    w_assert3(msg.size() == buf.size());
    w_assert3(msg.cb_tid == cbxct.xct()->tid());

    msg.outcome = outcome;
    msg.num_blockers = numblockers;
    msg.block_level = block_level;

    if (numblockers > 0) {
	char* off = (char *)&msg + align(sizeof(msg_callback_t));
	for (int i = 0; i < numblockers; i++) {
	    memcpy(off, (char *)&blockers[i], sizeof(blocker_t));
	    off += sizeof(blocker_t);
	}
	delete[] blockers;
    }
    buf.set(msg.size());

    TRACE(103, "sending cb reply: id: " << msg.cb_id << " name: "
		<< msg.name << " outcome: " << outcome);

    W_COERCE(_reply(buf, emptyBox, RCOK, cbxct.reply_port(),
						cbxct.remote_server()));

    return RCOK;
}


//
// Send a notification to an xact which was victimized during deadlock detection
// and which is in the middle of a callback operation. 
//
rc_t remote_m::send_deadlock_notification(callback_op_t* cbop,
				const tid_t& sender, const tid_t& victim)
{
    Buffer* buf = new Buffer(align(sizeof(msg_callback_t)));
    w_assert1(buf);

    msg_callback_t& msg = *(new (buf->start()) msg_callback_t(
	callback_msg, cbop->cb_id, cbop->name, NL, FALSE,
	srvid_t::null, victim, tid_t::null, 0));

    msg.cb_tid = sender;
    msg.num_blockers = 0;
    msg.outcome = callback_op_t::LOCAL_DEADLOCK;

    buf->set(msg.size());
    server_h dummy;
    W_DO(_call(*buf, emptyBox, emptyBox, srvid_t::ME, dummy,
				cbop->reply_from_port, Endpoint::null));
    return RCOK;
}

#ifdef OBJECT_CC

rc_t remote_m::adaptive_callback(const lpid_t& pid, const srvid_t& srv, int n,
					adaptive_xct_t* xacts, long cbid)
{
    w_assert3(cc_adaptive);
    w_assert1(is_aligned(sizeof(adaptive_xct_t)));

    server_h server;
    W_COERCE(server.fix(srv, REQ));

    Buffer& buf = *server.req_out_buf();
    msg_adaptive_t& msg = *(new (buf.start()) msg_adaptive_t(pid, n));
    msg.cb_id = cbid;
    buf.set(align(sizeof(msg_adaptive_t)));

    memcpy(buf.next(), (char*)xacts, n * sizeof(adaptive_xct_t));
    buf.adjust(n * sizeof(adaptive_xct_t));

    W_DO(_call_server(buf, emptyBox, server, TRUE));

    buf.set(align(sizeof(msg_adaptive_t)));
    memcpy((char*)xacts, buf.next(), n * sizeof(adaptive_xct_t));

    return RCOK;
}


rc_t remote_m::_adaptive_callback_srv(Buffer& buf, server_h& server)
{
    msg_adaptive_t& msg = *(msg_adaptive_t*)buf.start();
    buf.set(align(sizeof(msg_adaptive_t)));
    adaptive_xct_t* xacts = (adaptive_xct_t*)buf.next();

    const rvol_t* vol = _inv_lookup_volume(msg.pid.vol());
    lpid_t pid = msg.pid;
    pid._stid.vol = vol->alias_vid();

    TRACE(139, "Adaptive Req: pid: " << msg.pid << " xacts: ";
	for (int j=0; j < msg.nxacts; j++) { _debug.clog <<xacts[j].tid<<' '; }
	_debug.clog << " CB ID = " << msg.cb_id << flushl);

    for (int i = 0; i < msg.nxacts; i++) {
	bm_zero(xacts[i].recs, max_recs);

	xct_t* xd = remote_to_master(xacts[i].tid);
	if (xd && xd->state() != xct_ended) {
	    MUTEX_ACQUIRE(xd->lock_info()->mutex);
	    adaptive_lock_t* lock =xd->lock_info()->adaptive_locks->lookup(pid);
	    if (lock) {
		memcpy((char*)&xacts[i].recs, (char*)&lock->recs, recmap_sz);
		xd->lock_info()->adaptive_locks->remove(lock);
		delete lock;
	    } else {
		xacts[i].tid = tid_t::null;
	    }
	    MUTEX_RELEASE(xd->lock_info()->mutex);
	}
    }

    buf.set(msg.size());
    W_COERCE(_reply(buf, emptyBox, RCOK, server.reply_to_port(), server.id()));

    return RCOK;
}

#endif /* OBJECT_CC */


rc_t remote_m::get_pids(stid_t stid, bool lock, bool last,
					shpid_t first, int& num, shpid_t* pids)
{
    w_assert1(xct());
    master_xct_proxy_t* mxct;
    vid_t remote_vid;
    W_DO(_lookup_master_proxy(stid.vol, mxct, remote_vid));
    w_assert3(mxct);

    Buffer& buf = *mxct->req_buf();
    msg_pids_t& msg = *(new (buf.start())
				msg_pids_t(stid, lock, last, first, num));
    msg.stid.vol = remote_vid;
    buf.set(msg.size());

    W_DO(_call_xct_server(buf, *mxct));

    w_assert3(msg.nfound <= num);
    buf.set(align(sizeof(msg_pids_t)));
    memcpy((char*)pids, (char*)buf.next(), msg.nfound * sizeof(shpid_t));
    num = msg.nfound;
    return RCOK;
}


rc_t remote_m::_get_pids_srv(Buffer& msg_buf,
		remote_xct_proxy_t* from_xct, const Endpoint& reply_port)
{
    msg_pids_t& msg = *(msg_pids_t*)msg_buf.start();
    lpid_t pid;
    int num = msg.num;
    w_assert3(num <= msg_pids_t::max_pids);

    if (msg.last_only) {
	W_DO(io->last_page(msg.stid, pid, 0, msg.lock));
	*((shpid_t*)msg_buf.next()) = pid.page;
	msg_buf.adjust(sizeof(shpid_t));
	msg.nfound = 1;
    } else {
	if (msg.first == 0) {
	    W_DO(io->first_page(msg.stid, pid, 0, msg.lock));
	    *((shpid_t*)msg_buf.next()) = pid.page;
	    msg_buf.adjust(sizeof(shpid_t));
	    msg.nfound = 1;
	    num--;
	} else {
	    pid._stid = msg.stid;
	    pid.page = msg.first;
	}

	for (int i = 0; i < num; i++) {
	    rc_t rc = io->next_page(pid, 0, msg.lock);
	    if (rc) {
		if (rc.err_num() == eEOF)	break;
		else				return rc.reset();
	    } else {
		*((shpid_t*)msg_buf.next()) = pid.page;
		msg_buf.adjust(sizeof(shpid_t));
		msg.nfound++;
	    }
	}
    }

    W_IGNORE(_reply(msg_buf, emptyBox, RCOK, reply_port, from_xct->server()));
    return RCOK;
}


rc_t remote_m::lookup_lid(vid_t vid, const lid_t& lid, lid_m::lid_entry_t& entry
)
{
    // get the master_xct_proxy_t for this xct at the server for vid
    master_xct_proxy_t* mxct;
    vid_t remote_vid;
    W_DO(_lookup_master_proxy(vid, mxct, remote_vid));
    w_assert3(mxct);

    Buffer& buf = *mxct->req_buf();
    msg_lid_t& msg = *(new (buf.start()) msg_lid_t(lookup_lid_msg, lid));
    buf.set(msg.size());

    TRACE(110, "sending lid req for lid: " << lid << " to server: " <<
	 mxct->srvid() << " master xct: " << xct()->tid() <<
	 " remote xct: " << mxct->remote_tid()
    );

    W_DO(_call_xct_server(buf, *mxct));

    TRACE(110, "received lid reply for lid: " << lid << " from server: " <<
	mxct->srvid() << " master xct: " << xct()->tid() <<
	" lid entry: " << msg.lid_entry
    );

    //lid = msg.lid;
    entry = msg.lid_entry;
    return RCOK;
}


rc_t remote_m::_lookup_lid_srv(Buffer& msg_buf,
                remote_xct_proxy_t* from_xct, const Endpoint& reply_port)
{
    msg_lid_t& msg = *(msg_lid_t*)msg_buf.start();

    TRACE(111, "received lid req: lid: " << msg.lid << " from server: " <<
	 from_xct->server() << " master xct: " << from_xct->master_tid() <<
	 " remote xct: " << xct()->tid()
    );

    W_DO(lid->lookup_srv(msg.lid, msg.lid_entry));

    TRACE(112, "sending reply to lid req: lid: " << msg.lid <<
	" to server: " << from_xct->server() << " master xct: " <<
	from_xct->master_tid() << " remote xct: " << xct()->tid()
    );

    W_IGNORE(_reply(msg_buf, emptyBox, RCOK, reply_port, from_xct->server()));
    return RCOK;
}


rc_t remote_m::acquire_lock(const lockid_t& n, lock_mode_t m,
                        lock_duration_t d, long timeout, bool force,
			int* value)
{
    vid_t vid = n.vid();

    // get the master_xct_proxy_t for this xct at the server for vid
    master_xct_proxy_t* mxct;
    vid_t remote_vid;
    W_DO(_lookup_master_proxy(vid, mxct, remote_vid));
    w_assert3(mxct);

    lockid_t rn = n;
    rn.vid() = remote_vid;

    Buffer& buf = *mxct->req_buf();
    msg_lock_t& msg = *(new (buf.start()) msg_lock_t(acquire_lock_msg,
                                                rn, m, d, timeout, force));
#ifdef DEBUG
    msg.value = max_int4;
#endif
    buf.set(msg.size());

    TRACE(113, "sending lock req for lockid: " << rn << " mode: " << m <<
	" to server: " << mxct->srvid() << "master xct: " << xct()->tid() <<
        " remote xct: " << mxct->remote_tid()
    );

    W_DO(_call_xct_server(buf, *mxct));

#ifdef DEBUG
    if (value) *value = msg.value;
#endif

    TRACE(114, "received lock reply for lockid: " << rn << " from server: " <<
        mxct->srvid() << " master xct: " << xct()->tid()
    );

#ifdef OBJECT_CC
    if (msg.adaptive) {
#ifdef DEBUG
	w_assert3(cc_adaptive && n.lspace() == lockid_t::t_record && m == EX);
	adaptive_lock_t* adaptive_lock = 
			xct()->lock_info()->adaptive_locks->lookup(n.pid());
	if (adaptive_lock) {
	    w_assert3(bm_num_set(adaptive_lock->recs, max_recs) == 0);
	}
#endif
    } else if (cc_adaptive && n.lspace() == lockid_t::t_record && m == EX) {
	adaptive_lock_t* adaptive_lock =
			xct()->lock_info()->adaptive_locks->lookup(n.pid());
	w_assert3(adaptive_lock && bm_num_set(adaptive_lock->recs,max_recs)==0);
	xct()->lock_info()->adaptive_locks->remove(adaptive_lock);
	delete adaptive_lock;
    }
#endif /* OBJECT_CC */

    return RCOK;
}


rc_t remote_m::_acquire_lock_srv(Buffer& msg_buf, 
                remote_xct_proxy_t* from_xct, const Endpoint& reply_port)
{
    msg_lock_t& msg = *(msg_lock_t*)msg_buf.start();

    TRACE(115, "received lock req: lockid: " << msg.name << " mode: " <<
	msg.mode << " from server: " << from_xct->server() << " master xct: "
	<< from_xct->master_tid() << " remote xct: " << xct()->tid()
    );

    if (msg.name.lspace() == lockid_t::t_record && cc_alg != t_cc_record)
	return RC(eBADCCLEVEL);

    lock_mode_t prev_mode, prev_pgmode;
    rc_t rc;

    do {
	if (msg.force) {
	    rc = llm->lock_force(msg.name, msg.mode, msg.duration, msg.timeout,
						&prev_mode, &prev_pgmode);
	} else {
	    rc = llm->lock(msg.name, msg.mode, msg.duration, msg.timeout, 
						&prev_mode, &prev_pgmode);
	}
	if (!rc || rc.err_num() != ePREEMPTED) break;
    } while(1);
    if (rc) return rc.reset();

    if (msg.mode == EX || (msg.name.lspace() == lockid_t::t_page &&
				(msg.mode == SIX || msg.mode == IX))) {
        W_DO(cbm->callback(msg.name, msg.mode, from_xct->server(),
				prev_pgmode, msg.timeout, &msg.adaptive));
    }

#ifdef DEBUG
    if (msg.mode == EX && msg.name.lspace() == lockid_t::t_record) {
	pin_i handle;
	W_COERCE(handle.pin(msg.name.rid(), 0, SH));
	msg.value = * ((int *)handle.body());
    }
#endif

    TRACE(116, "sending reply to lock req: lockid: " << msg.name <<
        " to server: " << from_xct->server() << " master xct: " <<
        from_xct->master_tid() << " remote xct: " << xct()->tid()
    );

    W_COERCE(_reply(msg_buf, emptyBox, RCOK, reply_port, from_xct->server()));

    return RCOK;
}


rc_t remote_m::read_page(const lockid_t& name, bfcb_t* b, uint2 ptag,
			bool detect_race, bool parallel, bool is_new)
{
    // get the master_xct_proxy_t for this xct at the server for vid
    master_xct_proxy_t* mxct;
    vid_t rem_vid;
    W_DO(_lookup_master_proxy(b->pid.vol(), mxct, rem_vid));
    w_assert3(mxct);
    lockid_t rem_name = name;
    rem_name.vid() = rem_vid;

#ifdef USE_HATESEND
    
    if (!is_new && buf.pid.vol().is_remote()){
        W_DO(_read_send_page_cli(mxct, rem_pid, buf, mode, detect_race));

    } else {
#endif

    const lpid_t& pid = b->pid;
    w_assert3(pid == name.pid());

    bool page_lock = FALSE;
    lock_mode_t mode = NL;

    if (name.lspace() == lockid_t::t_record) {
        w_assert3(cc_alg == t_cc_record && ptag == page_p::t_file_p);
	W_DO(lm->query(pid, mode, xct()->tid(), TRUE));
	w_assert3(mode != NL);
	if (mode != IS && mode != IX)	page_lock = TRUE;
	else			 	mode = SH;
	/*
	 * It is possible to have page_lock==TRUE and name.lspace()==t_record
	 * if an xact has acquired a page lock, then the page gets replaced
	 * from the cache, and then the xact reads a record in the page using
	 * record level locking
	 */
    } else {
	if (ptag == page_p::t_file_p) {
	    W_DO(lm->query(pid, mode, xct()->tid(), TRUE));
	    w_assert3(mode != IS && mode != IX);
	    page_lock = TRUE;
	}
    }

    Buffer& buf = *mxct->req_buf();
    msg_page_t& msg = *(new (buf.start()) msg_page_t(read_page_msg));
    msg.name = rem_name;
    if (page_lock) msg.name.truncate(lockid_t::t_page);
    msg.page_tag = ptag;
    msg.mode = mode;
    msg.detect_race = detect_race;
    msg.parallel = parallel;
    msg.recmap_included = FALSE;

    buf.set(msg.size());

#ifdef OBJECT_CC
    if (cc_alg == t_cc_record && ptag == page_p::t_file_p) {
        MUTEX_ACQUIRE(b->recmap_mutex);
        b->remote_io++;
        MUTEX_RELEASE(b->recmap_mutex);
    }
#endif

    TRACE(117, "sending page req for name: " << rem_name << " to server: " <<
	mxct->srvid() << " master xct: " << xct()->tid() <<
	" remote xct: " << mxct->remote_tid()
    );

    rc_t rc = _call_xct_server(buf, *mxct);

    TRACE(118, "received page reply for name: " << rem_name<< " from server: "<<
	    mxct->srvid() << " master xct: " << xct()->tid()
    );

    if (cc_alg == t_cc_record && ptag == page_p::t_file_p) {
#ifndef OBJECT_CC
	W_FATAL(eINTERNAL);
#else
        W_COERCE(b->recmap_mutex.acquire());

        if (rc) {
	    b->remote_io--;
	    b->recmap_mutex.release();
	    return rc.reset();
        }

	page_s* page = b->frame;
	page_s tmp_pbuf;

	bool re_acquire_latch = FALSE;
	rc_t lrc = b->latch.acquire(LATCH_EX, 0);
	if (lrc) {
	    w_assert1(lrc.err_num() == smthread_t::stTIMEOUT);
	    if (b->latch.held_by(me())) {
	        b->latch.release();
		re_acquire_latch = TRUE;
	        w_assert3(!b->latch.held_by(me()));
	    }
	    W_COERCE(b->latch.acquire(LATCH_EX));
	}

	bool is_cached = bf->is_cached(b);
	if (is_cached && _rlog->is_dirty(b)) {
		// w_assert3(!page_lock);	// wrong assertion
		page = &tmp_pbuf;
	} else {
	    // merging is not needed, so we can place the page directly
	    // in the buffer pool since we have already EX-latched it.

	    if (!is_cached) bm_zero(b->recmap, smlevel_0::max_recs);
	}

	// receive the page
	EndpointBox dummy;
	Buffer page_buf(page, sizeof(page_s));
	W_COERCE(mxct->reply_port().receive(page_buf, dummy));
	w_assert3(dummy.is_empty());

	TRACE(119, "received page: name: " << name << " from server: " <<
	    mxct->srvid() << " master xct: " << xct()->tid()
	);
	w_assert3(b->frame->nslots == page->nslots);

	if (page == &tmp_pbuf) _merge(b, b->frame, page);

	u_char* recmap = 0;
	if (! page_lock)
	    recmap = (u_char*)((char *)buf.start() + align(sizeof(msg_page_t)));
	_install_recmap(name, b, recmap, page, is_cached);

#ifdef DEBUG
	TRACE(118, "New Recmap: "); 
	for (int i = 0; i < b->frame->nslots; i++) {
	    if (bm_is_set(b->recmap, i)) {
		TRACE_NONL(118, '1');
	    } else {
		TRACE_NONL(118, '0');
	    }
	}
	TRACE(118, endl);
#endif

	if (re_acquire_latch) W_COERCE(b->latch.acquire(LATCH_SH));
	b->latch.release();
	b->remote_io--;
	b->recmap_mutex.release();

#endif /* OBJECT_CC */

    } else {
	if (rc) return rc.reset();

        // receive the page
        EndpointBox dummy;
        Buffer page_buf(b->frame, sizeof(page_s));
        W_DO(mxct->reply_port().receive(page_buf, dummy));
        w_assert3(dummy.is_empty());

        DBG( << " read_page_cli recd page " << name);
        TRACE(119, "received page: name: " << rem_name << " from server: " <<
    	    mxct->srvid() << " master xct: " << xct()->tid()
        );
    }

#ifdef USE_HATESEND
    }
#endif
    return RCOK;
}


#ifdef OBJECT_CC

void remote_m::_merge(bfcb_t* b, page_s* curr_copy, page_s* new_copy)
{
    smlevel_0::stats.page_merging_cnt++;

    bool implicit_EX = FALSE;

    char scratch[sizeof(new_copy->data)];
    char* off = scratch;

    w_assert3(new_copy->nslots < smlevel_0::max_recs-1);
    w_assert3(new_copy->pid.page == curr_copy->pid.page);
    TRACE(140, "MERGING for page: " << curr_copy->pid);

    for (int i = 0; i < new_copy->nslots; i++) {
	page_s::slot_t& ns = new_copy->slot[-i];
	page_s::slot_t& cs = curr_copy->slot[-i];

	/*
	 * NOTE: the 2nd "if" MUST come before the 3rd "if" in the following
	 *	 lines.
	 */
	if (ns.offset < 0) {			// slot is vacant in new
	    w_assert3(bm_is_clr(b->recmap, i));
	    cs.offset = -1;

	} else if (bm_is_clr(b->recmap, i)) {	// slot unavailable in curr
	    memcpy(off, new_copy->data+ns.offset, ns.length);
	    cs.offset = off - scratch;
	    off += align(ns.length);

	} else if (_rec_dirty(b->pid, i, implicit_EX)) {  // slot dirty in curr
	    memcpy(off, curr_copy->data+cs.offset, cs.length);
	    cs.offset = off - scratch;
	    off += align(cs.length);

	} else {
	    memcpy(off, new_copy->data+ns.offset, ns.length);
	    cs.offset = off - scratch;
	    off += align(ns.length);
	}
	w_assert1(off - scratch < sizeof(new_copy->data));
    }

#ifdef DEBUG
    if (curr_copy->nslots > new_copy->nslots) {
	for (i = new_copy->nslots; i < curr_copy->nslots; i++)
	   w_assert3(bm_is_clr(b->recmap, i));
    }
#endif

    memcpy(curr_copy->data, scratch, off - scratch);
    curr_copy->nslots = new_copy->nslots;
}


bool remote_m::_rec_dirty(const lpid_t& pid, int slot, bool& implicit_EX)
{
    if (implicit_EX) return TRUE;

    rid_t rid;
    rid.pid = pid;
    rid.slot = slot;

    lockid_t name(rid);
    lockid_t tmp_name;
    lock_mode_t mode;

    do {
        W_COERCE(lm->query(name, mode));

	if (mode == EX) {
	    if (name.lspace() != lockid_t::t_record) implicit_EX = TRUE;
	    return TRUE;
	}
	tmp_name = name;
    } while (llm->get_parent(tmp_name, name));

    implicit_EX = FALSE;
    return FALSE;
}


void remote_m::_install_recmap(const lockid_t& name, bfcb_t* b,
				 u_char* recmap, page_s* page, bool is_cached)
{
    w_assert3(name.lspace() == lockid_t::t_record || recmap == 0);

    rid_t req_rid = *(rid_t*)name.name();
    rid_t rid = req_rid;

    for (int i = 0; i < page->nslots; i++) {

	rid.slot = i;

        cb_race_t* race = 0;
        if (bf->race_tab->num_members() > 0) {
            race = bf->race_tab->lookup(rid);
        }

	if (! (page->slot[-i].offset >= 0)) {   // slot is vacant
	    if (is_cached) {
		w_assert3(! (b->frame->slot[-i].offset >= 0));
		w_assert3(bm_is_clr(b->recmap, i) && bm_is_clr(recmap, i));
	    }

	} else if (recmap == 0) {		// page lock has been acquired

	    // The next assertion is wrong. Consider C1 is trying to read a rec
	    // in page P. P is cached but the rec is unavailable. A read req is
	    // sent to the server. Before getting the read reply, a cb arrives
	    // for P, so a race will be marked. Next, the read reply arrives
	    // but a deadlock has occured and C1 has been victimized. So, no
	    // page is fetched and the race is not cleared. C1 restarts and
	    // tries to access P with a page lock. A read req is sent, and when
	    // the read reply arrives it finds the race.
	    // w_assert3(race == 0);

	    bm_set(b->recmap, i);

	} else {

            if (bm_is_clr(recmap, i)) {         // rec is marked unavailable
                if (bm_is_set(b->recmap, i)) {
                    w_assert3(is_cached);
	        } else if (rid == req_rid) {
		    bm_set(b->recmap, i);
		} else {
		    w_assert3(bm_is_clr(b->recmap, i));
		}

            } else {                            // rec is marked available
                if (race) {
                    w_assert3(bm_is_clr(b->recmap, i));
		    w_assert3(race->rid != req_rid);
                } else {
                    bm_set(b->recmap, i);
                }
            }
	}

        if (race) {
	    bf->race_tab->remove(race);
	    delete race;
	    race = 0;
	}
    }

    w_assert3(bm_is_set(b->recmap, req_rid.slot));

    // check the dummy record
    if (recmap == 0) {
	bm_set(b->recmap, smlevel_0::max_recs-1);
    } else {
	cb_race_t* race = 0;
	if (bf->race_tab->num_members() > 0) {
	    rid.slot = smlevel_0::max_recs-1;
	    race = bf->race_tab->lookup(rid);
	}

	i = smlevel_0::max_recs-1;

	if (bm_is_clr(recmap, i)) {         // rec is marked unavailable
            if (bm_is_set(b->recmap, i)) {
                w_assert3(is_cached);
            } else {
                w_assert3(bm_is_clr(b->recmap, i));
	    }
        } else {                            // rec is marked available
            if (race) {
                w_assert3(bm_is_clr(b->recmap, i));
            } else {
                bm_set(b->recmap, i);
            }
        }

	if (race) {
	    w_assert3(bm_is_clr(b->recmap, i));
	    bf->race_tab->remove(race);
	    delete race;
	}
    }
}
#endif /* OBJECT_CC */


rc_t remote_m::_read_page_srv(Buffer& msg_buf,
                 remote_xct_proxy_t* from_xct, const Endpoint& reply_port)
{
    FUNC(remote_m::_read_page_srv);

    const srvid_t& srv = from_xct->server();
    msg_page_t& msg = *(msg_page_t*)msg_buf.start();

    DBG( << " read_page_serv called" << msg.name );    
    TRACE(120, "received page req: name: " << msg.name << " mode: " <<
        msg.mode << " from server: " << srv << " master xct: "
	<< from_xct->master_tid() << " remote xct: " << xct()->tid()
    );
    w_assert3((msg.page_tag != page_p::t_file_p) || 
		(msg.name.lspace() == lockid_t::t_record && msg.mode == SH) ||
		(msg.mode == SH || msg.mode == SIX || msg.mode == EX));
    w_assert3(msg.page_tag == page_p::t_file_p || msg.mode == NL);

    if (msg.name.lspace() == lockid_t::t_record && cc_alg != t_cc_record)
	return RC(eBADCCLEVEL);

    const lpid_t& pid = *(lpid_t*)msg.name.name();

    // lock the data
    if (msg.mode != NL) {
	rc_t rc = llm->lock(msg.name, msg.mode, t_long);
	w_assert3(!rc || rc.err_num() != ePREEMPTED);
	if (rc) return rc.reset();
    }

    DBG( << " read_page_srv: fix the page" << pid );    
    page_p page;
#ifndef USE_HATESEND    
    W_DO(page.fix(pid, (page_p::tag_t) msg.page_tag, LATCH_SH, 0));
#else
    // mark a page as hated
    if (hate_hints){
       W_DO(page.fix(msg.pid, LATCH_SH, 0, false, bf_m::t_hated));
    } else {
       W_DO(page.fix(msg.pid, LATCH_SH, 0));       
    }
#endif    

    if (msg.name.lspace() == lockid_t::t_record) {
#ifdef OBJECT_CC
        w_assert3(cc_alg == t_cc_record && msg.page_tag == page_p::t_file_p);
	cm->mutex_acquire();
	_build_recmap(msg.name, srv, page.nslots(), msg_buf);
	msg.recmap_included = TRUE;
	cm->register_copy(pid, srv, msg.detect_race, msg.parallel);
	cm->mutex_release();
#else
	W_FATAL(eINTERNAL);
#endif
    } else {
        cm->register_copy(pid, srv, msg.detect_race, msg.parallel);
    }

    TRACE(121, "sending reply to page req: name: " << msg.name <<
	  " to server: " << srv << " master xct: " <<
	  from_xct->master_tid() << " remote xct: " << xct()->tid());

    // reply that we will send the page
    W_COERCE(_reply(msg_buf, emptyBox, RCOK, reply_port, srv));

    TRACE(121, "sending page: pid: " << pid <<
	  " to server: " << srv << " master xct: " <<
	  from_xct->master_tid() << " remote xct: " << xct()->tid());


    // send the page
    Buffer page_buf(&(page.persistent_part()), sizeof(page.persistent_part()));
    W_COERCE(reply_port.send(page_buf, emptyBox));

    DBG( << " read_page_srv: page sent" << pid );    
    TRACE(122, "page sent: pid: " << pid << " to server: " << srv);
 
    return RCOK;
}


#ifdef OBJECT_CC

void remote_m::_build_recmap(const lockid_t& name, const srvid_t& srv,
						int nslots, Buffer& buf)
{
    w_assert3(buf.size() == align(sizeof(msg_page_t)));

    rid_t req_recid = *(rid_t *)name.name();
    rid_t recid = req_recid;
    u_char* recmap = (u_char *) buf.next();
    bm_zero(recmap, smlevel_0::max_recs);
    int nset = 0;

    for (int i = 0; i < nslots; i++) {
	recid.slot = i;
	lock_head_t* lock = llm->core()->find_lock(recid, FALSE);

	if (lock && (lock->granted_mode == EX || lock->pending())) {
	    xct_t* locker = lock->queue.top()->xd;
	    if (locker->is_remote_proxy() &&
				locker->master_site()->server() == srv) {
		bm_set(recmap, i);
		nset++;
	    }
#ifdef DEBUG
	    if (lock->granted_mode != EX) {
		w_assert3(lock->granted_mode == SH);
		w_assert3(lock->queue.top()->mode == SH);
		w_assert3(locker->callback()->cb_id >= 0);
		w_assert3(*(rid_t *)locker->callback()->name.name() == recid);
	    }
#endif
	} else {
	    bm_set(recmap, i);
	    nset++;
	}
	if (lock) MUTEX_RELEASE(lock->mutex);
    }

    if (nset == nslots) {
	// if no unavailable objects, then check the page for an IX lock held
	// by xacts not on the same server as xct(). If such a lock is found,
	// then the "dummy" record of the page is marked unavailable so that
	// page-level read locks from "srv" are propagated to the server.
	const lpid_t& pid = name.pid();
	lock_head_t* lock = llm->core()->find_lock(pid, FALSE);
	w_assert3(lock && lock->granted_mode != NL);

	if (lock->granted_mode == IS ||
			(lock->granted_mode == SH && !lock->waiting)) {
	    bm_set(recmap, smlevel_0::max_recs-1);
	    MUTEX_RELEASE(lock->mutex);
	    buf.adjust(align(smlevel_0::recmap_sz));
	    return;
	}

	bool dummy_available = TRUE;
	lock_request_t* req = 0;
	w_list_i<lock_request_t> iter(lock->queue);

	while (req = iter.next()) {
	    if (req->xd == xct()) {
		w_assert3(req->mode == IX || req->mode == IS);
		continue;
	    }
	    if (req->status() == lock_m::t_waiting) break;

	    if ((req->mode == IX || req->mode == SIX ||
				(req->mode == SH && lock->waiting)) &&
				(!req->xd->is_remote_proxy() ||
				req->xd->master_site()->server() != srv)) {
		dummy_available = FALSE;
		break;
	    }
	}
	if (dummy_available) bm_set(recmap, smlevel_0::max_recs-1);
	MUTEX_RELEASE(lock->mutex);
    }
	
    buf.adjust(align(smlevel_0::recmap_sz));

    return;
}
#endif /* OBJECT_CC */


rc_t remote_m::commit_xct(xct_t& xct)
{
    int         site_cnt = 0; // number of site xct has spread to
    int         vote_cnt = 0;  // number of sites voting to commit

    xct.flush_logbuf();

    // prepare on all servers
    {
        w_list_i<master_xct_proxy_t> scan((xct.master_proxy_list()));
        while(scan.next()) {
            site_cnt++;

            // tell the site to prepare -- ignore any failure
            if (_prepare_xct_cli(*scan.curr())) {
                // prepare failed, so don't count vote
            } else {
                // prepare successful
                vote_cnt++;
            }
        }
        // handle local site
        site_cnt++;
        if (xct.prepare() || xct.vote() == vote_abort ) {
            // prepare failed, so don't count vote
        } else {
            // prepare successful
	    w_assert3(xct.vote()==vote_commit ||
		xct.vote()==vote_readonly);
            vote_cnt++;
        }
    }

    if (vote_cnt == site_cnt) {
        // everyone agreed to commit, so do so. But first clean the pages that
	// the committing xact has dirtied.

	w_hash_i<pid_hash_t, lpid_t> iter(xct.dirty_pages());
	pid_hash_t* page = 0;
	while (page = iter.next()) {
#ifndef OBJECT_CC
	    bf->set_clean(page->pid);
#endif /* OBJECT_CC */
	    xct.dirty_pages().remove(page);
	    delete page;
	}

        w_list_i<master_xct_proxy_t> scan((xct.master_proxy_list()));
        while(scan.next()) {
            W_COERCE(_commit_xct_cli(*scan.curr()));
        }

        W_COERCE(xct.commit());
        W_DO(_cleanup_xct(xct));

    } else {
        W_DO(abort_xct(xct));
    }

    return RCOK;
}


rc_t remote_m::abort_xct(xct_t& xct)
{
    xct.flush_logbuf();

    w_assert1(xct.status() == xct_active || xct.status() == xct_prepared);

    w_list_i<master_xct_proxy_t> scan((xct.master_proxy_list()));

    while(scan.next()) {
	// discard any log recs of the aborting xact from the local log cache.
	W_COERCE(_rlog->purge(*scan.curr()));

	// abort the remote agent for the aborting xact
        W_COERCE(_abort_xct_cli(*scan.curr()));
    }

    // purge data dirtied by the aborting xact
    _purge_dirty(xct);

    W_COERCE(xct.abort());
    W_DO(_cleanup_xct(xct));
    return RCOK;
}


void remote_m::_purge_dirty(xct_t& xct)
{
    for (int i = 0; i < lock_m::NUM_DURATIONS; i++) {
        w_list_i<lock_request_t> lock_iter(xct.lock_info()->EX_list[i]);
        lock_request_t* req = 0;

	while (req = lock_iter.next()) {
	    const lockid_t& name = req->get_lock_head()->name;

	    w_assert3(req->status() == lock_m::t_granted);
	    w_assert3(req->mode == EX && name.vid().is_remote());

	    if (name.lspace()==lockid_t::t_page ||
				 name.lspace()==lockid_t::t_record) {
		rc_t rc;
#ifdef OBJECT_CC
		if (name.lspace()==lockid_t::t_record)
		    rc = bf->discard_record(name.rid(), TRUE);
		else
#endif
		    rc = bf->discard_page(name.pid());
		if (rc && rc.err_num() != fcNOTFOUND) W_FATAL(rc.err_num());
	    }
	}
    }

    w_hash_i<pid_hash_t, lpid_t> iter(xct.dirty_pages());
    pid_hash_t* page = 0;
    while (page = iter.next()) {
#ifdef OBJECT_CC
        rc_t rc;
        if (cc_alg == t_cc_record && page->recmap)
            rc = bf->discard_many_records(page->pid, page->recmap);
        else
            rc = bf->discard_page(page->pid);
#else
        rc_t rc = bf->discard_page(page->pid);
#endif /* OBJECT_CC */
        if (rc && rc.err_num() != fcNOTFOUND) W_FATAL(rc.err_num());
        xct.dirty_pages().remove(page);
        delete page;
    }
}


rc_t remote_m::_prepare_xct_cli(master_xct_proxy_t& mxct)
{
    rc_t rc;
    Buffer& buf = *mxct.req_buf();
    msg_xct_t& msg = *(new (buf.start()) msg_xct_t(prepare_xct_msg));
    buf.set(msg.size());

    TRACE(123, "sending prepare req: to server: " << mxct.srvid() <<
	"master xct: " << xct()->tid() << " remote xct: " << mxct.remote_tid()
    );

    // Flush the log to the server and receive server's vote.
    // Notice that the server treats "commit" log records as an
    // implicit "prepare" request as well.
    int nlogs;
    rc = _rlog->flush(mxct.last_lsn(), nlogs, &mxct);

    // if there were no log records to flush, send explicit "prepare" request 
    if (nlogs == 0) {
        rc = _call_xct_server(buf, mxct);
    }

    if (!rc) { 
        TRACE(124, "received prepare YES reply: from server: " << mxct.srvid()
	    << " master xct: " << xct()->tid()<<" remote xct: "<<mxct.remote_tid()
        );
    } else {
	TRACE(124, "received prepare NO reply: from server: " << mxct.srvid()
	    << " master xct: " << xct()->tid()<<" remote xct: "<<mxct.remote_tid()
	);
    }

    return RCOK;
}


rc_t remote_m::_prepare_xct_srv(Buffer& msg_buf,
                remote_xct_proxy_t* from_xct, const Endpoint& reply_port)
{
    TRACE(125, "received explicit prepare req: from server: " <<
        from_xct->server() << " master xct: " << from_xct->master_tid() <<
        " remote xct: " << xct()->tid()
    );

    //msg_xct_t& msg = *(msg_xct_t*)msg_buf.start();
    W_DO(from_xct->xct()->prepare());

    TRACE(126, "sending prepare reply: to server: " << from_xct->server() <<
	" master xct: " <<from_xct->master_tid() <<" remote xct: "<<xct()->tid()
    );

    vote_t v = from_xct->xct()->vote();
    if(v==vote_readonly || v==vote_commit) {
	W_COERCE(_reply(msg_buf, emptyBox, RCOK, reply_port, from_xct->server()));
    } else {
	W_IGNORE(_reply(msg_buf, emptyBox, RC(eVOTENO), reply_port, from_xct->server()));
    }
    return RCOK;
}


rc_t remote_m::_commit_xct_cli(master_xct_proxy_t& mxct)
{
    Buffer& buf = *mxct.req_buf();
    msg_xct_t& msg = *(new (buf.start()) msg_xct_t(commit_xct_msg));
    buf.set(msg.size());

    TRACE(127, "sending commit req: to server: " << mxct.srvid() <<
        "master xct: " << xct()->tid() << " remote xct: " << mxct.remote_tid()
    );

    W_DO(_call_xct_server(buf, mxct));

    server_h server;
    W_COERCE(server.fix(mxct.srvid(), REQ));
    server.incr_xct_cnt(-1);

    TRACE(128, "received commit reply: from server: " << mxct.srvid() <<
        "master xct: " << xct()->tid() << " remote xct: " << mxct.remote_tid()
    );

    return RCOK;
}


rc_t remote_m::_commit_xct_srv(Buffer& msg_buf,
                remote_xct_proxy_t* from_xct, const Endpoint& reply_port)
{
    tid_t tid = xct()->tid();

    TRACE(129, "received commit req: from server: " << from_xct->server() <<
	" master xct: " << from_xct->master_tid() << " remote xct: " << tid
    );

    W_DO(from_xct->xct()->commit());

    TRACE(130, "sending commit reply: to server: " << from_xct->server() <<
	" master xct: " << from_xct->master_tid() << " remote xct: " << tid
    );

    W_IGNORE(_reply(msg_buf, emptyBox, RCOK, reply_port, from_xct->server()));
    return RCOK;
}


rc_t remote_m::_abort_xct_cli(master_xct_proxy_t& mxct)
{
    Buffer& buf = *mxct.req_buf();
    msg_xct_t& msg = *(new (buf.start()) msg_xct_t(abort_xct_msg));
    buf.set(msg.size());

    TRACE(131, "sending abort req: to server: " << mxct.srvid() <<
        "master xct: " << xct()->tid() << " remote xct: " << mxct.remote_tid()
    );

    W_DO(_call_xct_server(buf, mxct));

    server_h server;
    W_COERCE(server.fix(mxct.srvid(), REQ));
    server.incr_xct_cnt(-1);

    TRACE(132, "received abort reply: from server: " << mxct.srvid() <<
        "master xct: " << xct()->tid() << " remote xct: " << mxct.remote_tid()
    );

    return RCOK;
}


rc_t remote_m::_abort_xct_srv(Buffer& msg_buf,
                 remote_xct_proxy_t* from_xct, const Endpoint& reply_port)
{
    tid_t tid = xct()->tid();

    TRACE(133, "received abort req: from server: " << from_xct->server() <<
	" master xct: " << from_xct->master_tid() << " remote xct: " << tid
    );

    //msg_xct_t& msg = *(msg_xct_t*)msg_buf.start();
    W_DO(from_xct->xct()->abort());

    TRACE(134, "sending abort reply: to server: " << from_xct->server() <<
        " master xct: " << from_xct->master_tid() << " remote xct: " << tid
    );

    W_IGNORE(_reply(msg_buf, emptyBox, RCOK, reply_port, from_xct->server()));
    return RCOK;
}


void remote_m::_redo_logs_srv(Buffer& buf, const srvid_t& srvid,
						const Endpoint& reply_port)
{
    msg_log_t& msg = *(msg_log_t*)buf.start();
    buf.set(align(sizeof(msg_log_t)));

    tid_t curr_tid = tid_t::null;
    xct_t* xct = 0;
    xct_t* commit_xct = 0;

    for (int i = 0; i < msg.nlogs; i++) {

	logrec_t* rec = (logrec_t *) buf.next();

	w_assert3(rec->is_page_update());

	if (rec->tid() != curr_tid) {
	    xct = master_to_remote(rec->tid(), srvid);
	    if (rec->tid() == msg.commit_tid) commit_xct = xct;
	    curr_tid = rec->tid();
	}
	w_assert3(xct->last_log() == 0);

        me()->attach_xct(xct);

	bf->mutex_acquire();
	if (!bf->has_frame(rec->pid())) smlevel_0::stats.vol_redo_reads++;
	bf->mutex_release();

	page_p page;
	W_COERCE(page.fix(rec->pid(), page_p::t_any_p, LATCH_EX, 0));
	rec->redo(&page);

	/*
	 * No need to explicitly place the received logrec into the xact log
	 * buffer since this will be done by the redo method.
	 */
        xct->flush_logbuf();

        me()->detach_xct(xct);

	buf.adjust(align(rec->length()));
    }

    if (msg.commit && msg.last) {

	me()->attach_xct(commit_xct);

	remote_xct_proxy_t* master = commit_xct->master_site();
        w_assert3(master);

	TRACE(125, "received implicit prepare req: from server: " <<
	    master->server() << " master xct: " << master->master_tid() <<
	    " remote xct: " << commit_xct->tid()
	);

	W_COERCE(commit_xct->prepare());

	TRACE(126, "sending implicit prepare reply: to server: " <<
	    master->server() << " master xct: " << master->master_tid() <<
	    " remote xct: " << commit_xct->tid()
	);

	msg.size = align(sizeof(msg_log_t));
	buf.set(msg.size);

	vote_t v = commit_xct->vote();
        if(v==vote_readonly || v==vote_commit) {
            W_COERCE(_reply(buf, emptyBox, RCOK, reply_port, srvid));
        } else {
            W_COERCE(_reply(buf, emptyBox, RC(eVOTENO), reply_port, srvid));
        }

	me()->detach_xct(commit_xct);

    } else if (msg.last) {

	msg.size = align(sizeof(msg_log_t));
	buf.set(msg.size);

	W_COERCE(_reply(buf, emptyBox, RCOK, reply_port, srvid));
    }
}


rc_t remote_m::_purged_pages_srv()
{
    W_FATAL(eINTERNAL);
    return RCOK;        // to keep compiler quiet.
}


rc_t remote_m::ping_xct(const srvid_t& srvid, bool direct)
{
    master_xct_proxy_t* mxct = 0;
    w_list_i<master_xct_proxy_t> scan((xct()->master_proxy_list()));
    while((mxct = scan.next()) && mxct->srvid() != srvid) ;

    if (mxct == 0) {
	master_xct_proxy_t* tmp = new master_xct_proxy_t(xct(), srvid);
	if (!tmp) return RC(eOUTOFMEMORY);
	w_auto_delete_t<master_xct_proxy_t> auto_del(tmp);
	W_DO(_spread_xct(tmp));
	auto_del.set(0);  //cancel auto delete

	xct()->master_proxy_list().append(tmp);
	mxct = tmp;
    }

    W_COERCE(_ping_xct_cli(mxct, direct));
    return RCOK;
}


rc_t remote_m::_ping_xct_cli(master_xct_proxy_t* mxct, bool direct)
{
    Buffer& buf = *mxct->req_buf();
    msg_ping_xct_t& msg = *(new (buf.start()) msg_ping_xct_t(ping_xct_msg));

    buf.set(msg.size());

    if (direct) {
	W_DO(_call_xct_server(buf, *mxct));
    } else {
	msg_type_t msg_type = ping_xct_msg;
	srvid_t srvid = mxct->srvid();
	server_h server;
        W_COERCE(server.fix(srvid, REQ));

	W_COERCE(_piggyback_purged_pages(srvid, server, msg,
						server.req_to_port()));
	buf.adjust(sizeof(lpid_t)*msg.get_purged_list_size());

	EndpointBox dest_port;
	W_COERCE(dest_port.set(0, mxct->req_port()));

	_msg_stats[msg.type()].req_send_cnt++;
	W_DO_PUSH(server.req_to_port().send(buf, dest_port), eCOMM);
	server.unfix();

	while (TRUE) {
	    EndpointBox dummy;
	    W_DO_PUSH(mxct->reply_port().receive(buf, dummy), eCOMM);

	    w_assert3(msg.type() == msg_type || msg.type() == purged_pages_msg);
	    int size = msg.size() + msg.get_purged_list_size() * sizeof(lpid_t);
	    w_assert3(size == buf.size());
	    w_assert3(is_aligned(size));

	    if (!srvid.is_me()) _delete_copies(srvid, msg, buf);

	    if (msg.type() == msg_type) {
		_msg_stats[msg.type()].reply_rcv_cnt++;
		break;
	    } else {
		_msg_stats[msg.type()].req_rcv_cnt++;
	    }
	}
    }
    return RCOK;
}


rc_t remote_m::_ping_xct_srv(Buffer& msg_buf, EndpointBox& dest_port, 
		server_h& from_server, remote_xct_proxy_t* from_xct,
		const Endpoint& reply_port)
{
    if (from_server != 0) {
	Endpoint send_to;
	W_COERCE(dest_port.get(0, send_to));
	W_DO_PUSH(send_to.send(msg_buf), eCOMM);
    } else {
        msg_hdr_t&  msg = *(msg_hdr_t*)msg_buf.start();
        w_assert3(msg.size() == msg_buf.size());
        w_assert3(is_aligned(msg.size()));

        W_COERCE(reply_port.send(msg_buf));
    }
    return RCOK;
}


rc_t remote_m::_cleanup_xct(xct_t& xct)
{
    w_list_i<master_xct_proxy_t> scan((xct.master_proxy_list()));
    while(scan.next()) {
	delete scan.curr();
    }
    return RCOK;
}


rc_t remote_m::register_purged_page(const lpid_t& pid)
{
    purged_page_t* page = new purged_page_t(pid);
    const rvol_t* vol = _lookup_volume(pid.vol());
    server_h server;
    W_DO(server.fix(vol->owner(), MSG));
    page->pid._stid.vol = vol->owner_vid();
    server.register_purged_page(page);

    TRACE(135, "pushed page into purged list: pid: " << pid);

    return RCOK;
}


rc_t remote_m::_piggyback_purged_pages(const srvid_t& srvid, server_h& server,
				msg_hdr_t& msg, const Endpoint& port)
{
    int pidsz = sizeof(lpid_t);

    bool server_fixed = FALSE;
    if (server.fixed()) {
	w_assert3(server.id() == srvid);
	server_fixed = TRUE;
	MUTEX_ACQUIRE(server.msg_mutex());
    } else {
	W_DO(server.fix(srvid, MSG));
    }

    msg.set_purged_list_size(0);
    char* off = (char *)&msg + msg.size();

    purged_page_t* p = 0;
    w_list_i<purged_page_t> iter(server.purge_pages_list());

    while ((off + pidsz - (char*)&msg) <= max_msg_size && (p = iter.next())) {
        memcpy(off, (char*)&p->pid, pidsz);
        msg.incr_purged_list_size(1);
        off += pidsz;

	TRACE(136, "piggybacked purged page: pid: " << p->pid <<
	    " to server: " << srvid << " in msg of type: " << msg.type()
	);

	delete p;
    }

    if (server.purge_pages_list().num_members() != 0) {

	Buffer& buf = *(new Buffer(max_msg_size));
	msg_purged_pages_t& msg = *(new (buf.start())
					msg_purged_pages_t(purged_pages_msg));
	while (TRUE) {

	    msg.set_purged_list_size(0);
	    buf.set(msg.size());
	    off = (char *)buf.next();

	    while ((off + pidsz - (char*)&msg) <= max_msg_size &&
							 (p = iter.next())) {
        	memcpy(off, (char*)&p->pid, pidsz);
        	msg.incr_purged_list_size(1);

		TRACE(136, "piggybacked purged page: pid: " << p->pid <<
		    " to server: " << srvid << " in msg of type: " << msg.type()
		);

        	delete p;
        	off += pidsz;
    	    }

	    buf.adjust(pidsz * msg.get_purged_list_size());
	    _msg_stats[msg.type()].req_send_cnt++;
	    rc_t rc = port.send(buf);
	    if (rc) {
		if (server_fixed)	server.msg_mutex().release();
		else			server.unfix();
	        W_DO_PUSH(rc, eCOMM);
	    }
	    _msg_stats[msg.type()].req_send_cnt++;

	    if (server.purge_pages_list().num_members() == 0) break;
	}
    }

    w_assert3(server.purge_pages_list().num_members() == 0);
    if (server_fixed) {
	    MUTEX_RELEASE(server.msg_mutex());
    }
    else
	    server.unfix();

    return RCOK;
}


void remote_m::_delete_copies(const srvid_t& srvid, msg_hdr_t& msg, Buffer& buf)
{
    int npages = msg.get_purged_list_size();
    char* off = (char *)&msg + msg.size();

    for (int i = 0; i < npages; i++) {
	w_assert1(off + sizeof(lpid_t) - (char *)&msg <= max_msg_size);
	cm->delete_copy(*(lpid_t*)off, srvid);
	off += sizeof(lpid_t);
    }
    msg.set_purged_list_size(0);
    buf.set(msg.size());
}


// Send an rpc to the srv req port, reply indicates whether a reply is expected.
rc_t remote_m::_call_server(Buffer& buf, EndpointBox& ports,
						server_h& server, bool reply)
{
    w_assert3(server.fixed());
    return _call(buf, ports, ports, server.id(), server,
		server.req_to_port(), reply ?
		server.reply_from_port() : Endpoint::null);
}


// Send an rpc to the remote_xct_proxy_t
rc_t remote_m::_call_xct_server(Buffer& buf, master_xct_proxy_t& mxct)
{
    server_h server;
    return _call(buf, emptyBox, emptyBox, mxct.srvid(), server,
					mxct.req_port(), mxct.reply_port());
    w_assert3(emptyBox.is_empty());
}


// Send an rpc to a port on a server, if reply is non-NULL a reply is waited for
rc_t remote_m::_call(Buffer& buf,
		EndpointBox& out_ports, EndpointBox& in_ports,
		const srvid_t& srvid, server_h& server,
		const Endpoint& send_to, const Endpoint& reply_from)
{
    struct timeval start_time, end_time;
    rc_t comm_err;
    msg_hdr_t&  msg = *(msg_hdr_t*)buf.start();
    msg_type_t  msg_type = msg.type();
    int         msg_size = msg.size();
    w_assert3(msg_size == buf.size());

    _msg_stats[msg.type()].req_send_cnt++;

    if (srvid.is_valid() && !srvid.is_me()) {
        W_COERCE(_piggyback_purged_pages(srvid, server, msg, send_to));
        buf.adjust(sizeof(lpid_t)*msg.get_purged_list_size());
    }

    if (msg_type != log_msg && &reply_from != &Endpoint::null) {
	gettimeofday(&start_time, 0);
	smlevel_0::stats.small_msg_size += buf.size();
	smlevel_0::stats.small_msg_cnt++;
    }

    TRACE(202, "sending msg type: "<<msg.type()<<" to server: "<<srvid);
    W_DO_PUSH(send_to.send(buf, out_ports), eCOMM);
    TRACE(202, " msg sent to server: "<<srvid<<" msg type: "<<msg.type());

    if (&reply_from != &Endpoint::null) {	// reply is expected

	while (TRUE) {

            if (comm_err = reply_from.receive(buf, in_ports)) {
                return RC_PUSH(comm_err, eCOMM);

            } else {

		TRACE(203, "reply received from server: " << srvid <<
                    " msg type: " << msg.type()
                );

                w_assert3(msg.type()==msg_type || msg.type()==purged_pages_msg);

		if (msg_type != log_msg) {
		    gettimeofday(&end_time, 0);
		    smlevel_0::stats.small_msg_time +=
					compute_time(&start_time, &end_time);
		}

		int size = msg.size()+msg.get_purged_list_size()*sizeof(lpid_t);
		w_assert3(size == buf.size());
		w_assert3(is_aligned(size));

                if (!srvid.is_me()) _delete_copies(srvid, msg, buf);

		if (msg.type() == msg_type) {
		    _msg_stats[msg.type()].reply_rcv_cnt++;
                    if (msg.error()) return msg.error();
		    break;
		} else {
		    _msg_stats[msg.type()].req_rcv_cnt++;
		}
	    }
        }
    }
    return RCOK;
}


rc_t remote_m::_reply(Buffer& buf, EndpointBox& ports, rc_t rc,
			const Endpoint& reply_to_port, const srvid_t& srvid)
{
    msg_hdr_t&  msg = *(msg_hdr_t*)buf.start();
    w_assert3(msg.size() == buf.size());
    w_assert3(is_aligned(msg.size()));

    if (rc)     _msg_stats[msg.type()].reply_send_error_cnt++;
    else        _msg_stats[msg.type()].reply_send_cnt++;
    msg.set_error(rc);

    if (srvid.is_valid() && !srvid.is_me()) {
	server_h server;
        W_COERCE(_piggyback_purged_pages(srvid, server, msg, reply_to_port));
        buf.adjust(sizeof(lpid_t) * msg.get_purged_list_size());
    }

    TRACE(201, "sending reply: msg type: " << msg.type() <<
	" to server: " << srvid
    );
    W_DO_PUSH(reply_to_port.send(buf, ports), eCOMM);
    TRACE(201, "reply sent to server: "<<srvid<<" msg type: "<<msg.type());

    return RCOK;
}


void remote_m::_dispatch_msg(Buffer& buf, EndpointBox& ports,
		server_h& from_server, remote_xct_proxy_t* from_xct,
		const Endpoint& reply_port, const Endpoint& req_port)
{
    msg_hdr_t&  msg = *(msg_hdr_t*)buf.start();
    w_assert3(msg.type() <= max_msg);
    _msg_stats[msg.type()].req_rcv_cnt++;

    rc_t rc;

    switch (msg.type()) {
    case connect_msg:
        rc = _connect_srv(buf, ports);
        break;

    case disconnect_msg:
        rc = _disconnect_srv(buf, from_server, reply_port);
        break;
    case mount_volume_msg:
        rc = _mount_volume_srv(buf, from_server);
        break;
    case callback_msg:
        rc = _callback_srv(buf, ports, from_server);
        break;
    case abort_callback_msg:
	rc = _abort_callback_srv(buf, from_server);
	break;
    case spread_xct_msg:
        rc = _spread_xct_srv(buf, ports, from_server);
        break;
#ifdef OBJECT_CC
    case adaptive_msg:
	rc = _adaptive_callback_srv(buf, from_server);
	break;
#endif
    case prepare_xct_msg:
        rc = _prepare_xct_srv(buf, from_xct, reply_port);
        break;
    case commit_xct_msg:
        rc = _commit_xct_srv(buf, from_xct, reply_port);
        break;
    case abort_xct_msg:
        rc = _abort_xct_srv(buf, from_xct, reply_port);
        break;
    case pids_msg:
	rc = _get_pids_srv(buf, from_xct, reply_port);
	break;
    case lookup_lid_msg:
        rc = _lookup_lid_srv(buf, from_xct, reply_port);
        break;
    case acquire_lock_msg:
        rc = _acquire_lock_srv(buf, from_xct, reply_port);
        break;
    case read_page_msg:
        rc = _read_page_srv(buf, from_xct, reply_port);
        break;

    case purged_pages_msg:
        rc = _purged_pages_srv();
        break;
    case ping_xct_msg:
        rc = _ping_xct_srv(buf, ports, from_server, from_xct, reply_port);
        break;
#ifdef SHIV
    case send_page_msg:
	rc = _send_page_srv(buf, from_server, from_xct, reply_port);
	break;
    case read_send_page_msg:
        rc = _read_send_page_srv(buf, from_server, from_xct,
							reply_port, req_port);
        break;
    case fwd_page_msg:
        rc = _fwd_page_srv(buf, from_server, from_xct, reply_port, req_port);
        break;
#endif /* SHIV */
    default:
        W_FATAL(eINTERNAL)
    };

    srvid_t srvid = srvid_t::null;
    if (from_server != server_h::null) {
        srvid = from_server.id();
    } else if (from_xct != 0) {
        srvid = from_xct->server();
    }

    if (&reply_port != &Endpoint::null) {
        if (rc) {
            W_COERCE(_reply(buf, emptyBox, rc, reply_port, srvid));
        } else {
            // assume the function already replied
        }
    } else {
        _msg_stats[msg.type()].reply_send_skip_cnt++;
    }
}


#ifdef SHIV

#endif /* SHIV */
#endif /* MULTI_SERVER */


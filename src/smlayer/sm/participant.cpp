/*<std-header orig-src='shore'>

 $Id: participant.cpp,v 1.55 1999/08/06 15:35:42 bolo Exp $

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

/*
 *  Stuff common to both coordinator and subordnates.
 */

#define SM_SOURCE
#define COORD_C /* yes - COORD_C */


#include <sm_int_0.h>
#include <w_windows.h>

#ifdef USE_COORD
#include <coord.h>

#ifdef EXPLICIT_TEMPLATE
template class w_list_t<twopc_thread_t>;
template class w_list_i<twopc_thread_t>;
#endif
typedef class w_list_t<twopc_thread_t> twopc_thread_list_t;

/* delegate is used in debug case */
#define RETURN_RC( _err) \
    do { w_rc_t rc = rc_t(_err); return  rc; } while(0)


/* Seconds -> milliseconds converter for timeouts. */
#define	SECONDS(n)	((n) * 1000)

/* 
 * version 2 of the protocol doesn't ship endpoints around;
 * instead it relies on the name-to-endpoint mapping on receive
 * as well as send.  It ships the names around instead
 */


/* message timestamp methods */

message_t::stamp_t::stamp_t(const stime_t &r)
: sec(r.secs()),
  usec(r.usecs())
{
}


message_t::stamp_t	&message_t::stamp_t::operator=(const stime_t &r)
{
	sec = r.secs();
	usec = r.usecs();

	return *this;
}


void	message_t::stamp_t::net_to_host()
{
	sec = ntohl(sec);
	usec = ntohl(usec);
}


void	message_t::stamp_t::host_to_net()
{
	sec = htonl(sec);
	usec = htonl(usec);
}


ostream &message_t::stamp_t::print(ostream &o) const
{
	return o << sec << "." << usec;
}


ostream &operator<<(ostream &o, const message_t::stamp_t &m)
{
	return m.print(o);
}


/***************************************************
 * class message_t
 **************************************************/
void 	
message_t::audit(bool audit_sender) const
{
    DBG(<<"message_t::audit"
        << " sequence=" << sequence
	<< " tid.len=" << tid().length()
	<< " sender.len=" << sender().length()
	<< " sender=" << sender()
	<< " typ=" << type()
    );
    // in all cases, the gtid should be legit
    w_assert3(tid().wholelength() <= sizeof(gtid_t));
    w_assert3(tid().length() < sizeof(gtid_t));

    w_assert3(sender().wholelength() <= sizeof(server_handle_t));
    w_assert3(sender().length() < sizeof(server_handle_t));

    w_assert3(tid().length() > 0);
    if(audit_sender) {
	w_assert1(sender().length() > 0);
    }

    switch(type()) {
	case smsg_prepare:
	case smsg_abort:
	case smsg_commit:
	    w_assert3(error() == 0);
	    break;

	case sreply_ack:
	    w_assert3(
		(_u.typ_acked == smsg_abort)
		||(_u.typ_acked == smsg_commit)
		);
	    // for now, let's say it must be 0 because
	    // we're not yet issuing any errors
	    w_assert3(error() == 0);
	    break;
	case sreply_status:
	case sreply_vote:
	    if(_u.vote == vote_bad) {
		w_assert3(error() > 0);
	    } else {
		w_assert3(
		(_u.vote == vote_readonly)
		||(_u.vote == vote_abort)
		||(_u.vote == vote_commit)
		);
	    }
	    break;

	case smsg_bad:
	    // We could be sending an error
	    w_assert3(error() > 0);
	    break;

	default:
	    // w_assert3(0);
	    break;
    }
}

void	message_t::ntoh() 
{
	_typ = ntohl(_typ);

	error_num = ntohl(error_num);
	sequence = ntohl(sequence);

	stamp.net_to_host();

	_gtid.ntoh();
	_server_handle.ntoh();
}


void	message_t::hton() 
{
	_typ = htonl(_typ);

	error_num = htonl(error_num);
	sequence = htonl(sequence);

	stamp.host_to_net();

	_gtid.hton();
	_server_handle.hton();
}

void  		
message_t::put_tid(const gtid_t &t)
{
    /* insert global transaction id t */
    gtid_t *tp = _settable_tid();
    w_assert1(tp);
    *tp = t;
    _clear_sender();
}

void
message_t::_clear_sender() 
{
	_server_handle.clear();
}

server_handle_t *
message_t::_settable_sender() 
{
    if(tid().length() > 0) {
	 w_assert3(tid().wholelength() < sizeof(gtid_t));
	 return &_server_handle;
    } else {
	return 0;
    }
}

const server_handle_t &
message_t::sender() const 
{
    w_assert1(tid().length() > 0);
    const server_handle_t *s = &_server_handle;
    return *s;
}

void  		
message_t::put_sender(const server_handle_t &s)
{
    server_handle_t *sp = _settable_sender();
    w_assert1(sp);
    sp->clear();
    *sp = s;
}

/***************************************************
 * class particpant
 **************************************************/
NORET			
participant::participant(
	commit_protocol p,
        CommSystem *c,
	NameService *ns,
	name_ep_map *f
	) :
    _proto(p),
    _mutex("participant"),
    // _error,
    _comm(c),
    _ns(ns),
    // _me
    // _meBox
    _message_handler(0),
    _threads(0),
    _name_ep_map(f)
#ifdef PRESUMED_NOTHING
    ,
    _activate("participant")
#endif
{
    w_assert1(p == presumed_nothing ||
	p == presumed_abort);
    w_assert1(_name_ep_map);

    _threads = new twopc_thread_list_t(offsetof(twopc_thread_t, _list_link));
    if(!_threads) {
	W_FATAL(eOUTOFMEMORY);
    }
}

void 
participant::_thread_insert(twopc_thread_t *t) 
{
   W_COERCE(_mutex.acquire());
   _threads->push(t);
   _mutex.release();
}

void 
participant::_thread_retire(twopc_thread_t *t) 
{
   W_COERCE(_mutex.acquire());
   t->_list_link.detach();
   _mutex.release();

   // await mutex in *t:
   t->retire();
}

void
participant::_init_base()
{
    w_assert3(_mutex.is_mine());
    _error = _meBox.set(0,_me);

    if (_error) {
	W_FATAL(_error.err_num());
    }
}

NORET			
participant::~participant()
{
    w_assert3(_me);

    /*
     * Wake up anyone waiting on receive
     * This is only needed if the sm is doing
     * the receives (rather than the vas).
     */
    if(_message_handler) {

	_message_handler->retire();
        W_IGNORE( _message_handler->wait() );
        delete _message_handler;
	_message_handler=0;
	if(_me.is_valid()) {
	    while(_me.refs() > 0) {
		W_COERCE(_me.release()); // ep.release()
	    }
	}
    }

    // retire all threads
    // wait for them to finish
    // destroy them

    if(_threads) {
	twopc_thread_t *t;
	while( (t = _threads->pop()) ) {
	    t->retire();
	    W_IGNORE(t->wait());
	    delete t;
	}
	delete _threads;
	_threads =0;
    }
}



/*
 * receive() for CASE1
 * This function is responsible for releasing
 * any eps sent in sentbox
 */
rc_t
participant::receive(
    coord_thread_kind 	__purpose,
    Buffer&		buf,	
    EndpointBox&	
#ifndef VERSION_2
	sentbox
#endif
	,
    const EndpointBox& 	mebox  // what to send with any response generated
)
{
    FUNC(twopc_thread_t::receive);

    // handle 1 request, then return and let
    // caller check _retire

    Endpoint 		_ep;
    server_handle_t 	srvaddr;

    Endpoint		sender;

    /*
     * set up the ref to the message  in the buffer
     */
    if (buf.size() != sizeof(message_t)) {
	    smlevel_0::errlog->clog <<error_prio
		    << "Coord message wrong size " << buf.size()
		    << ", expecting size " << sizeof(message_t) << endl;
	    return RC(scWRONGSIZE);	/* XXX choose a better errno */
    }

    message_t	&m = * (message_t *) buf.start();

    rc_t    		rc;

    m.ntoh();
    m.audit();

#ifdef VERSION_2
    srvaddr = m.sender();
    rc = mapping()->name2endpoint(m.sender(), sender);//recv.V2.CASE1
    if(rc) {
	// treat as "not found"
	// log this stray message and drop it
	smlevel_0::errlog->clog <<error_prio 
		<< m.stamp << ":"
		<< " STRAY message type " << m.type()
		<< " sequence " << m.sequence 
		<< " from "  << m.sender()
		<< " cannot convert from endpoint to name"
		<< flushl;

	return RCOK;
    }

#else
    /* 
     * Version 1
     * find out who sent this message. If we can't
     * map it, we can't process the message.
     */
    W_COERCE(mebox.get(0,sender) );
    rc = mapping()->endpoint2name(sender, srvaddr);//recv.V1.CASE1

    if(rc) {
	// treat as "not found"
	// log this stray message and drop it
	smlevel_0::errlog->clog <<error_prio 
		<< m.stamp << ":"
		<< " STRAY message type " << m.typ
		<< " sequence " << m.sequence 
		<< " from " ;
		<< sender
		<< " cannot convert from endpoint to name"
		<< flushl;

	W_COERCE(sender.release());
	return RCOK;
    }
#endif

    rc =   __receive(__purpose, buf, m, sender, srvaddr, mebox);
    W_COERCE(sender.release());
    return rc;
}

/*
 * receive() for CASE2
 */
rc_t
participant::receive(
    coord_thread_kind 	__purpose,
    Buffer&		buf,	
    Endpoint& 		_ep, 
			     // this is the endpoint
			     // on which to do the receive. (CASE2- not
			     // tested)
    const EndpointBox& 	mebox  // what to send with any response generated
)
{   // CASE 2: we do the receive
    FUNC(twopc_thread_t::receive);

    // handle 1 request, then return and let
    // caller check _retire

    rc_t    		rc;
    server_handle_t 	srvaddr;

    message_t 	holder;
    EndpointBox 	senderbox;
    Endpoint		sender;

    w_assert3(_ep);

    /*
     * set up the ref to the message  in the buffer
     */
    if (buf.size() != sizeof(message_t)) {
	    smlevel_0::errlog->clog <<error_prio
		    << "Coord message wrong size " << buf.size()
		    << ", expecting size " << sizeof(message_t) << endl;
	    return RC(scWRONGSIZE);	/* XXX choose a better errno */
    }

    message_t	&m = * (message_t *) buf.start();

    { /* do the receive */
#ifdef W_DEBUG
	DBGTHRD(<<"participant_receive: listening on: " << _ep << flushl); 
#endif
	Buffer	mybuf(&holder, sizeof(holder)); // maximum size
	rc = _ep.receive(mybuf, senderbox);
	if(rc) {
	   // TODO: handle death notification
	   DBG(<<"rc = " << rc);
	   W_FATAL(rc.err_num());
	}

	    if (mybuf.size() != sizeof(message_t)) {
		    smlevel_0::errlog->clog <<error_prio
			    << "Coord message wrong size " << mybuf.size()
			    << ", expecting size " << sizeof(message_t) << endl;
		    W_FATAL(scWRONGSIZE);	/* XXX choose a better errno */
	    }

        m.ntoh();

#ifdef VERSION_2
	w_assert3(m.sender().length() > 0);
	rc = mapping()->name2endpoint(m.sender(), sender);//recv.V2.CASE2
	/* mapping did sender.acquire() */
#else
	rc = senderbox.get(0, sender);
#endif /* VERSION */

	if(rc) {
	    // what if scDEAD? nsNOTFOUND?
	    W_FATAL(rc.err_num()); // for now
	}
	w_assert3(sender.refs() >= 1);

    }
    DBGTHRD(<<"sender.REF " << sender.refs());

    m.audit();

    DBGTHRD(<<"ep.REF " << sender.refs());
#ifdef VERSION_2
    srvaddr = m.sender();
#else
    /* 
     * Version 1
     * find out who sent this message. If we can't
     * map it, we can't process the message.
     */
    rc = mapping()->endpoint2name(sender, srvaddr);//recv.V1.CASE2
    if(rc) {
	// treat as "not found"
	// log this stray message and drop it
	smlevel_0::errlog->clog <<error_prio 
		<< m.stamp << ":"
		<< " STRAY message type " << m.typ
		<< " sequence " << m.sequence 
		<< " from "
		<< sender
		<< " cannot convert from endpoint to name"
		<< flushl;
	return RCOK;
    }
#endif
    rc =  __receive(__purpose, buf, m, sender, srvaddr, mebox);

    if(sender.is_valid()) {
	W_COERCE(sender.release()); // ep.release()
    }
    return rc;
}

rc_t
participant::__receive(
    coord_thread_kind 	__purpose,
    Buffer&		buf,	
    message_t &	m,
    Endpoint& 		sender,  // ep of sender
    server_handle_t&	srvaddr, // name of sender
    const EndpointBox& 	mebox
)
{
    bool  is_subordinate = (
	__purpose == subord_recovery_handler ||
	__purpose == subord_message_handler
    );

    if(m.error()) {
	DBGTHRD(<< "received (error=" << m.error_num << ") typ=" << m.type());
	if(is_subordinate) {
	    INC_2PCSTAT(s_errors_recd);
	} else {
	    INC_2PCSTAT(c_errors_recd);
	}
    } else {
	switch(m.type()) {

	case smsg_prepare:
	    INC_2PCSTAT(s_prepare_recd);
	    w_assert3(is_subordinate);
	    break;
	case smsg_abort:
	    INC_2PCSTAT(s_abort_recd);
	    w_assert3(is_subordinate);
	    break;
	case smsg_commit:
	    INC_2PCSTAT(s_commit_recd);
	    w_assert3(is_subordinate);
	    break;
	case sreply_ack:
	    INC_2PCSTAT(c_acks_recd);
	    w_assert3(! is_subordinate);
	    break;
	case sreply_status:
	    INC_2PCSTAT(c_status_recd);
	    w_assert3(! is_subordinate);
	    break;
	case sreply_vote:
	    INC_2PCSTAT(c_votes_recd);
	    break;
	default:
	    break;
	}
    }
    DBGTHRD(<< "calling handle_message() for " << m.type()
                << " error " << m.error()
                << " gtid "
		<< m.tid()
		);
    if(smlevel_0::errlog->getloglevel() >= log_info) {
	    smlevel_0::errlog->clog <<info_prio 
		<< m.stamp;

	    smlevel_0::errlog->clog <<info_prio 
			<< " " << me()->id  << ":";
	    if(m.sequence>0) {
		smlevel_0::errlog->clog <<info_prio 
		    << " RDUP: t:" << m.type() ;
	    } else {
		smlevel_0::errlog->clog <<info_prio 
		    << " RECV: t:" << m.type() ;
	    }

	switch(m.type()) {
	    case sreply_status:
	    case sreply_vote:
		smlevel_0::errlog->clog <<info_prio 
		    << " v:" << int(m._u.vote) ;
		break;
	    case sreply_ack:
		smlevel_0::errlog->clog <<info_prio 
		    << " a:" << m._u.typ_acked ;
		break;
	    default:
		break;
	}
	/*
	 * Here we print from: and an ep -- this is the sending
	 * endpoint, and it's identified in one of two ways:
	 * what came in the box with the message (VERSION 1), 
	 * or what's in the message (VERSION 2);
	 * although that's only convention. We have no idea what
	 * endpoint this was really received FROM.
	 */
	smlevel_0::errlog->clog <<info_prio 
		<< " s:" << m.sequence 
		<< " e:" << m.error_num
		<< endl << "    "
		<< " from:"
		<< sender
		<< "(" << srvaddr << ")"
		<< endl << "    "
		<< " to:" ;

	Endpoint id;
	W_IGNORE(mebox.get(0,id));
	if(id.is_valid()) {
	    /*
	     * Here we print to: and an ep -- this is the recipient
	     * endpoint, and it's identified by what came in mebox,
	     * although that's only convention. We have no idea what
	     * endpoint this was really received ON.
	     */
	    smlevel_0::errlog->clog <<info_prio 
		<< id;
	} else {
	    smlevel_0::errlog->clog <<info_prio 
		<< "empty box ";
		
	}
	smlevel_0::errlog->clog <<info_prio 
		<< endl << "    "
		<< " gtid:" << m.tid()
		<< flushl;

    }

    return handle_message(__purpose, buf, sender, srvaddr, mebox);
}

static const char *const twopcthreadname = "2PC____________________";
NORET
twopc_thread_t::twopc_thread_t(
    participant *p, 
    coord_thread_kind k, 
    bool otf
) :
    // make its name long enough to rename safely
    smthread_t(t_regular, false, false, twopcthreadname),
    _on_the_fly(otf),
    _purpose(k),
    _proto(p?p->proto():smlevel_0::presumed_abort),
    _retire(false),
    _party(p),
    // _error
    _mutex(twopcthreadname), 
    // _link
    _condition(twopcthreadname), 
    _sleeptime(SECONDS(10)),
    _timeout(WAIT_FOREVER)
{
    FUNC(twopc_thread_t::twopc_thread_t);

    _2pcstats = new sm2pc_stats_t;
    // These things have no constructor
    memset(_2pcstats, 0, sizeof(sm2pc_stats_t));
}

NORET
twopc_thread_t::~twopc_thread_t()
{
    smlevel_0::stats.summary_2pc += twopc_stats();
    delete _2pcstats;
}

void
twopc_thread_t::set_thread_error(int error_num) 
{ 
   w_assert3(_mutex.is_mine());
   _error = RC(error_num);

   DBGTHRD(<<"coord_thread_t::set_thread_error() signalling condition");
   _condition.signal();
}

void			
twopc_thread_t::handle_message(coord_thread_kind k) 
{
    FUNC(twopc_thread_t::handle_message);

    rc_t	rc;
    Buffer	nullbuf;

    W_COERCE(_mutex.acquire());

    while(!_retire) {
	_mutex.release();
	
	switch(k) {

	case participant::coord_message_handler:
	case participant::subord_message_handler:

	    /* CASE2 - not implemented, much less, tested */
	    W_FATAL(fcNOTIMPLEMENTED); 

	    rc = _party->receive(k, nullbuf, _party->self(), _party->box());
	    if(rc) {

		 DBGTHRD(<< "unexpected error with commit" << rc);
	         this->retire();
	    }
	    break;

	default:
	    // dealt with by caller -- 
	    // should never get here
	    W_FATAL(smlevel_0::eINTERNAL);
	    break;
	}

	W_COERCE(_mutex.acquire());
    } /* while */
    _mutex.release();
}

rc_t 
participant::send_message(
    coord_thread_kind	__purpose,
    Buffer&		buf, 
    Endpoint& 		destination,
    server_handle_t& 	dest_name,
    const EndpointBox& 	mebox
)
{
    rc_t		rc;
    message_t		&m = *(message_t *) buf.start();

    DBGTHRD(<<"participant::sendmessage() "
	    << " destination " << destination
	    << " destination.name=" << dest_name
	    );

    if(smlevel_0::errlog->getloglevel() >= log_info) {
	m.stamp = stime_t::now();    

	smlevel_0::errlog->clog <<info_prio 
		<< m.stamp;
	smlevel_0::errlog->clog <<info_prio 
	    << " " << me()->id  << ":";
	if(m.sequence>0) {
	    smlevel_0::errlog->clog <<info_prio 
		<< " SDUP: t:" << m.type() ;
	} else {
	    smlevel_0::errlog->clog <<info_prio 
		<< " SEND: t:" << m.type() ;
	}

	switch(m.type()) {
	    case sreply_status:
	    case sreply_vote:
		smlevel_0::errlog->clog <<info_prio 
		    << " v:" << int(m._u.vote) ;
		break;
	    case sreply_ack:
		smlevel_0::errlog->clog << info_prio 
		    << " a:" << m._u.typ_acked ;
		break;
	    default:
		break;
	}
	smlevel_0::errlog->clog <<info_prio 
		<< " s:" << m.sequence 
		<< " e:" << m.error_num
		<< endl << "    "
		<< " to:" 
		<< destination
		<< "(" << dest_name << ")"
		<< endl << "    "
		<< " from:" ;
	    {
		Endpoint id;
		W_IGNORE(mebox.get(0,id));
		if(id.is_valid()) {
		    smlevel_0::errlog->clog <<info_prio 
			<< id;
		} else {
		    smlevel_0::errlog->clog <<info_prio 
			<< " empty box" ;
		}
	    }
	    smlevel_0::errlog->clog <<info_prio 
		<< endl << "    "
		<< " gtid:" << m.tid()
		<< flushl;
    }

#ifdef VERSION_2
    // Constructor for subordinate, coordinator
    // set _myname
    m.put_sender(_myname);
#endif


    DBGTHRD(<< "sending message " << m.type()
	    << " error " << w_error_t::error_string(m.error())
	    << " (vote=" << int(m._u.vote)
	    << ")  gtid=" << m.tid()
	    << " sender= " << m.sender()
	    );


    m.audit();

    bool  is_subordinate = (
	__purpose== subord_recovery_handler ||
	__purpose == subord_message_handler
	);
    if(m.error()) {
	if(is_subordinate) {
	    INC_2PCSTAT(s_errors_sent);
	} else {
	    INC_2PCSTAT(c_errors_sent);
	}
    } else {
	switch(m.type()) {
	case smsg_prepare:
	    INC_2PCSTAT(c_prepare_sent);
	    w_assert3(! is_subordinate);
	    break;
	case smsg_abort:
	    INC_2PCSTAT(c_abort_sent);
	    w_assert3(! is_subordinate);
	    break;
	case smsg_commit:
	    INC_2PCSTAT(c_commit_sent);
	    w_assert3(! is_subordinate);
	    break;
	case sreply_ack:
	    INC_2PCSTAT(s_acks_sent);
	    w_assert3(is_subordinate);
	    break;
	case sreply_status:
	    INC_2PCSTAT(s_status_sent);
	    w_assert3(is_subordinate);
	    break;
	case sreply_vote:
	    INC_2PCSTAT(s_votes_sent);
	    w_assert3(is_subordinate);
	    break;
	default:
	    break;
	}
    }

    DBGTHRD(<<"participant::sendmessage() "
	    << " buf.size=" << buf.size()
	    << " destination=" << destination
	    );

    m.hton();
    rc = destination.send(buf, mebox);
    m.ntoh();
    if( rc ) {
	smlevel_0::errlog->clog <<error_prio
		<< m.stamp
		<< " Cannot send message: "  
		<< m.type()
		<< rc << flushl;
	switch(rc.err_num()) {
	case scTRANSPORT:
	case scGENERIC:
	case fcOS:
	case fcWIN32:
	    return rc;

	case scDEAD:
	   // ok-- caller will decide what to 
	   // do - retry, send death notice, whatever
	   break;

	case scUNUSABLE:
	default:
	   W_FATAL(rc.err_num());
	}
    }

    DBGTHRD(<<"participant::sendmessage() "
	    << " destination.refs=" << destination.refs()
	    );
    return RCOK;
}

void
twopc_thread_t::retire()
{
    FUNC(twopc_thread_t::retire);
    W_COERCE(_mutex.acquire());
    _retire = true;
    _mutex.release();
}

bool
twopc_thread_t::retired() 
{
    bool b;
    W_COERCE(_mutex.acquire());
    b = _retire;
    _mutex.release();
    return b;
}

#include <vtable_info.h>
#include <vtable_enum.h>

void		
twopc_thread_t::vtable_collect(vtable_info_t& t) 
{
    smthread_t::vtable_collect(t);

    const char *c=0;
    switch (purpose()) {
    case participant::coord_message_handler: // handles all replies
	c = "coord msg reply handler";
	break;
    case participant::coord_commit_handler: // commits single gtx
	c = "coord commit 1 tx" ;
	break;
    case participant::coord_abort_handler: // aborts single gtx
	c = "coord abort 1 tx";
	break;
    case participant::coord_recovery_handler: // for recovery in presumed_*
	c = "coord recover";
	break;
    case participant::coord_reaper: // reaps coord_recovery_handler threads
	c = "coord reaper";
	break;

    case participant::subord_recovery_handler: // prepared gtxs (for presumed_abort)
	c = "subord recover";
	break;
    case participant::subord_message_handler:	// handles all commands
	c = "subord msg handler";
	break;
    default:
    case participant::coord_thread_kind_bad: 
	c = "bad";
	break;
    };
    t.set_string(twopc_thread_purpose_attr, c);

#ifdef NOTDEF
/* moved back to static struct */
#define TMP_GET_STAT(x) GET_MY2PCSTAT(x)
#include "sm2pc_stats_t_collect_gen.cpp"
#endif
}

bool 
participant::reap_one(twopc_thread_t *t,bool killit) 
{
    DBGTHRD(<< "reaping resolving thread!" );

    if(t->status() != smthread_t::t_defunct) {
	if(killit) {
	    t->retire();
	    W_IGNORE( t->wait() );
	} else {
	    return false;
	}
    }
    w_assert3(t->status() == smthread_t::t_defunct);
    _thread_retire(t);
    DBG(<< "reaping deleting thread " << t->id );
    delete t;
    return true;
}

int 
participant::reap_finished(bool killthem) 
{
   twopc_thread_t *t;
   W_COERCE(_mutex.acquire());
   w_list_i<twopc_thread_t> i(*_threads);
   int left=0;
   bool reaped=false;
   while((t = i.next())) {
	reaped=false;
	if(t->on_the_fly()) {
	    _mutex.release();
	    reaped=reap_one(t, killthem);
	    W_COERCE(_mutex.acquire());
	    if(!reaped) {
		left++;
	    }
	}
   }
   _mutex.release();
   // return # on-the-fly threads that aren't
   // finished
   DBG(<<left << "resolver threads are still running");
   return left;
}
#endif /* USE_COORD */


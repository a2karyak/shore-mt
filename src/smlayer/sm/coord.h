/*<std-header orig-src='shore' incl-file-exclusion='COORD_H'>

 $Id: coord.h,v 1.42 2002/01/25 01:25:55 bolo Exp $

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

#ifndef COORD_H
#define COORD_H

#include "w_defines.h"

/*  -- do not edit anything above this line --   </std-header>*/

/*
 * This file describes three classes to be used by a
 * value-added server (VAS) for two-phase (distributed) commit (2PC):
 * coordinator, subordinate, and message_t.  The rest of the classes here
 * are supporting classes.
 * 
 * Certain implementation details of these classes affect the
 * architecture of the VAS:
 *  1) restrictions are placed on the communication protocol
 *     between master and slave VASs:  the VAS receives all
 *     messages between cooperating servers, and hands off 2PC
 *     messages to the SM.   The VAS must therefore recognize 2PC
 *     messages, and the VAS's protocol must not conflict with the 
 *     2PC protocol.
 *
 *  2) All communication between cooperating servers is on 
 *     Endpoints for which there is a 1:1 mapping to a name (an opaque
 *     value determined by the VAS).  The VAS must maintain the mapping.
 *  
 *  3) The names given to all cooperating VASs are logged, and so the
 *     same names must be used after crash recovery.
 *
 *  4) For the time being, only the Presumed Abort (PA) variant of 2PC 
 *     is implemented:
 *     if either a subordinate or a coordinator crashes, or the
 *     communication between them is broken for any reason, it is the
 *     responsibility of the subordinate to query the coordinator to
 *     learn the disposition of any in-doubt transactions. The subordinate
 *     must be able to rendez-vous with the coordinator prior to
 *     the completion of the in-doubt transactions.   Thus, when a
 *     subordinate starts, it must establish its endpoint-to-name mapping
 *     and invoke the Subordinate class immediately after recovery.
 *     For the coordinator's part, immediately after recovery, it must 
 *     refresh its endpoint-to-name  mapping in order to handle messages 
 *     subordinates and it must then invoke the Coordinator class to recover
 *     in-doubt transactions.
 *
 *
 *     In class coordinator and subordinate, the VAS-writer need only
 *     use the methods so marked: For use by VAS 
 */

#define VERSION_2
/* VAS uses this: */
enum	message_type_t { 
    smsg_bad = 0,
    smsg_prepare,
    smsg_abort, 
    smsg_commit,
    sreply_ack,
    sreply_vote,
    sreply_status
};

extern ostream &operator<<(ostream &, const message_type_t);


#ifndef COORD_LOG_H
#include <coord_log.h>
#endif
#ifndef CRASH_H
#include <crash.h>
#endif

class  		coord_thread_t; // forward
class		Buffer;

// forward:
class twopc_thread_t;
class subord_thread_t;
class coord_thread_t;
class message_t;

class participant : public smlevel_0 {
    friend class  	twopc_thread_t;

protected:
    typedef smlevel_0::commit_protocol commit_protocol;

public:
    enum coord_thread_kind { 
	coord_thread_kind_bad,
	coord_message_handler, // handles all replies
	coord_commit_handler, // commits single gtx
	coord_abort_handler, // aborts single gtx
	coord_recovery_handler, // for recovery in presumed_*
	coord_reaper, // reaps coord_recovery_handler threads

	subord_recovery_handler, // prepared gtxs (for presumed_abort)
	subord_message_handler	// handles all commands
    };
public:

    NORET		participant( commit_protocol p, // only presumed_abort
			   	CommSystem *c,
			   	NameService *ns,
				name_ep_map *f
			   	);

    virtual NORET	~participant();
    virtual void	dump(ostream &o) const=0;



    virtual rc_t	received(Buffer& what, 
				EndpointBox &,		// sent with message
				const EndpointBox &	// sent with reply
				)=0; 
			// received() takes responsibility for releasing
			// any endpoints in the first box
			// received() does not release any endpoints in 2nd box

    virtual rc_t	died(server_handle_t &)=0; // got death 
				// notice, mapping may be invalid
    virtual rc_t	recovered(server_handle_t &)=0; // recovered,
				// mapping is up-to-date

protected:
    rc_t		receive(coord_thread_kind __purpose,
				Buffer&,
				EndpointBox&,		// what sent
				const EndpointBox&	// sent with response
				);
    rc_t		receive(coord_thread_kind __purpose,
				Buffer&,
				Endpoint&,		// where to listen
				const EndpointBox&	// sent with response
				);

    // helper function:
    rc_t		__receive(coord_thread_kind __purpose,
				Buffer&,
				message_t &,
				Endpoint&,     // where to listen
				server_handle_t&,
				const EndpointBox&  // sent with response
				);

protected:

    // send_message must not be tied to a thread
    rc_t		send_message(
				coord_thread_kind	__purpose,
				Buffer& 		buf, 
				Endpoint& 		dest_ep,
				server_handle_t& 	dest_name,
				const EndpointBox&	mebox) ;

    // third argument is for case when VAS endpoints are used
    virtual rc_t	handle_message(
				coord_thread_kind 	__purpose,
				Buffer& 		buf, 
				Endpoint&		sender_ep,
				server_handle_t&	sender_name,
				const EndpointBox&	meBox)=0;
public:

    Endpoint&   	self() { return _me; }
    EndpointBox&   	box() { return _meBox; }

    static rc_t		register_ep( CommSystem *c,
			   	NameService *ns,
				const char *uniquename,
				Endpoint &ep);
    static rc_t		un_register_ep( NameService *ns,
				const char *uniquename);

    name_ep_map*	mapping() { return _name_ep_map; }
    commit_protocol	proto() const { return _proto; }
protected:
    void 		_thread_retire(twopc_thread_t *t) ;
    void 		_thread_insert(twopc_thread_t *t) ;
    int			reap_finished(bool killit);
    bool		reap_one(twopc_thread_t *t, bool killit);

    void		_init_base();

    commit_protocol 	_proto;
    smutex_t		_mutex;	// There's really not a lot of state
				// kept in participant and its derived
				// classes, but we put _mutex in here
				// for protection of that little bit.

    rc_t		_error; // ONLY for start-up
    // _comm and _ns aren't used for presumed_nothing, but
    // will be for presumed_abort
    CommSystem*		_comm;
    NameService*	_ns;
    Endpoint        	_me;
    EndpointBox        	_meBox;
    twopc_thread_t*	_message_handler;

    // 1 for each commit/resolve in coordinator case
    // 1 for each resolve in PA subordinate case
    w_list_t<twopc_thread_t>*	_threads; 
    name_ep_map *	_name_ep_map;
    server_handle_t   	_myname;
};

class subordinate : public participant {
    friend class  	subord_thread_t;
public:
    /* 
     *  Constructor for CASE 1: VAS listens for messages,
     *  The SM will not listen on this endpoint, but it
     *  will send on this endpoint during recovery.
     * 
     *  The VAS has the job of listening on this endpoint
     *  and calling subordinate::received when it
     *   	gets a message for the subordinate,
     *  and calling coordinator::received when it
     *  	gets a message for the coordinator.
     *
     * VAS also has to provide a name for the coordinator,
     *   so the subordinates can log this name, for the purpose
     *   rendez-vous with the coordinator after a crash.
     *
     */
			/* For use by VAS: */
    NORET		subordinate(commit_protocol p, // only presumed_abort
			    name_ep_map *f,
			    tid_gtid_map *g,
			    Endpoint &ep
			    );

    /*
     * Constructor for CASE 2:  (NOT TESTED) SM listens
     * 	VAS chooses a name for the subordinate, but does
     *  not create an endpoint and register with the name service.
     *  SM uses the name to create an endpoint; VAS may not use
     *  that name or endpoint.
     */
    NORET		subordinate(commit_protocol p, // only presumed_abort
			    CommSystem *c,
			    NameService *ns,
			    name_ep_map *f,
			    tid_gtid_map *g,
			    const char *uniquename,
			    const char *cname
			    );

    NORET		~subordinate();

			/* For use by VAS: */
    rc_t		received(Buffer&, 
				// Endpoint& sender, OLD
				EndpointBox& came_with_endpoint, 
				// EndpointBox& b // bx ctg destination ep
				const EndpointBox& what_to_send_with_response 
			    );
    rc_t		died(server_handle_t &); // got death notice
    rc_t		recovered(server_handle_t &); // came back up

    void		dump(ostream &o) const;


    rc_t		handle_message(
			    coord_thread_kind 	__purpose,
			    Buffer&		buf, 
			    Endpoint& 		sender,
			    server_handle_t&    srvaddr,
			    const EndpointBox& 	mebox);

protected:
    int			resolve(gtid_t &g);
    int			resolve(xct_t *x);
private:
    rc_t		_init(bool, const char *, bool);
    tid_gtid_map*	_tid_gtid_map;
    char *		_cname;
    subord_thread_t*	_recovery_handler;
};

/*
 * coordinator class 
 * -- one per server process 
 * -- starts up threads as needed
 */

class coordinator: public participant {
    friend class  	coord_thread_t;
    friend class  	log_entry;

    // TYPES
public:
    struct server_info {
         server_state		status;
         server_handle_t	name;
    };

    // METHODS
public:
    /*
     * NB: only CASE 1 is tested: VAS does all the listening
     * on endpoints, and it passes along 2PC messages to the
     * SM.
     *
     * Constructor for CASE 1: VAS listens
     *  The VAS has the job of listening on this endpoint
     *  and calling coordinator::handle_message when it
     *  gets a message for the coordinator.
     *
     */

			/* For use by VAS: */
    NORET		coordinator( 
				commit_protocol p, // only presumed_abort
				name_ep_map *f,
				Endpoint &ep,
				lvid_t&  vid,
				int  commit_timeout =
				    smlevel_0::dcommit_timeout
				);

    /*
     * Constructor for CASE 2: SM listens (NOT TESTED)
     */
    NORET		coordinator( commit_protocol p, // only presumed_abort
				CommSystem *c,
				NameService *ns,
				name_ep_map *f,
				const char *myname,
				lvid_t&  vid,
				int  commit_timeout =
				    smlevel_0::dcommit_timeout
				);

    NORET		~coordinator();

			/* For use by VAS: */
    rc_t		received(Buffer& what, 
				// Endpoint& sender, OLD
				EndpointBox& came_with_endpoint, 
				// EndpointBox& b // bx ctg destination ep
				const EndpointBox& what_to_send_with_response 
				); 
    rc_t		died(server_handle_t &); // got death notice, mapping may be invalid
    rc_t		recovered(server_handle_t &); // recovered, mapping is up-to-date

    void		dump(ostream &o) const;

			/* For optional use by VAS: 
			 *  administrative functions 
		         */
    rc_t        	new_tid(gtid_t &result);
    rc_t 		status(int &num_coordinated);
    rc_t 		status(int, gtid_t*, coord_state*);
    rc_t 		status(const gtid_t &g, int &num_servers);
    rc_t 		status(const gtid_t &g, int num, server_info *list);


    /*
     * coordinated commit: each local thread must already
     * have done ss_m::enter2pc. 
     */
			/* For use by VAS:  */
    rc_t		commit(const gtid_t& d, int num_threads, 
			    const server_handle_t *list,
			    bool prepare_only = false
			    );

			/* For use by VAS: to retry after a commit
			 * timed out and left an in-doubt transaction,
			 * or after commit(..., true) prepared but
			 * did not commit.
			 */
    rc_t		end_transaction(bool commit, const gtid_t& d);

protected:
    rc_t		_retry(const gtid_t& d, coord_thread_kind);
    rc_t		handle_message(
				coord_thread_kind 	__purpose,
				Buffer& 		buf, 
				Endpoint& 		sender,
				server_handle_t&    	srvaddr,
				const EndpointBox&	meBox);
private:
    rc_t		_init(bool);
    coord_action 	action(coord_thread_t *t, server_state ss);
    coord_thread_t*	gtid2thread(const gtid_t &g) ;
    void 		gtid2thread_retire(coord_thread_t *t) ;
    void 		gtid2thread_insert(coord_thread_t *t) ;
    coord_thread_t*	fork_resolver(const gtid_t &g, 
					participant::coord_thread_kind) ;


    // DATA MEMBERS
private:
    static const char*	_key_descr; /* string describing key format */
    static const char *	_dkeystring;
    static gtid_t	_dkeyspace;
    static vec_t	_dkey;
    lvid_t		_vid;
    coord_thread_t*	_reaper;
protected:
    stid_t		_stid;
private:
    DTID_T		_last_used_tid;
    int			_timeout; // give up commit after this time--
				// this is passed to the thread

};

class		message_t {
public:
    uint4_t		_typ;
    /*  VAS: the first part of every message must look like the above:
     *  all VAS messages must start with an enumerated  value that does
     *  not conflict with message_type_t. The rest of the message
     *  structures are private to the VAS or SM, except that the
     *  code that does the Endpoint::receive must use a buffer large
     *  enough to accommodate an entire message_t.
     */

    uint4_t	error_num; /* 0 -> OK, else an rc code */ 
    uint4_t	sequence; /* 0 for detecting retransmissions */

    struct stamp_t {
	    unsigned	sec;		// seconds 
	    unsigned	usec;		// microseconds

	    stamp_t() : sec(0), usec(0) { };	    
	    stamp_t(const stime_t &);
	    
	    stamp_t	&operator=(const stime_t &);
	    ostream 	&print(ostream  &) const;

	    void	clear() { sec = 0; usec = 0; }

	    void	net_to_host();
	    void	host_to_net();
    };
    stamp_t		stamp;	  /* for debugging */

    union {
	/* smsg_bad */
	/* smsg_prepare */
	/* smsg_abort */
	/* smsg_commit */
	    /*nothing more*/

	/* sreply_ack */
	    message_type_t typ_acked;

	/* sreply_vote */
	    vote_t	vote;
    }_u;

    message_type_t	type() const { return (message_type_t) _typ; }
    void		setType(message_type_t t) { _typ = (uint4_t) t; }

    const gtid_t &	tid() const { return _gtid; }
    void  		put_tid(const gtid_t &);
    const server_handle_t &sender() const;
    void  		put_sender(const server_handle_t &);

protected:
    gtid_t		_gtid;
    server_handle_t	_server_handle;

    gtid_t *		_settable_tid() { return &_gtid; }
    server_handle_t *	_settable_sender();
    void 		_clear_sender();
public:
    NORET 	message_t() { clear();}
    NORET 	~message_t() {}

    void  	clear() {
		    setType(smsg_bad);
		    error_num = 0;
		    sequence = 0;
		    stamp.clear();
		    _u.typ_acked = smsg_bad;
		    w_assert3(_settable_tid());
		    _settable_tid()->clear();
		    _clear_sender();
		}

    int 	error() const { return error_num; }

    int 	minlength() const { return sizeof(message_t); }
    int 	wholelength() const { return sizeof(message_t); }

    void 	audit(bool audit_sender = true) const; 
    void 	hton(); 
    void 	ntoh(); 
	
};


extern ostream &operator<<(ostream &, const message_t::stamp_t &);


class twopc_thread_t : public smthread_t {
    friend class participant;

public:
    typedef enum participant::coord_thread_kind coord_thread_kind;
    typedef smlevel_0::commit_protocol commit_protocol;


    // for per-gtx threads
    NORET 		twopc_thread_t(participant *p, 
		    		coord_thread_kind,
				bool otf
			);
    NORET 		~twopc_thread_t();
    virtual void        vtable_collect(vtable_info_t& t);

    w_rc_t&		error() { return _error; }
    virtual void 	run()=0;
    void 		retire();
    bool 		retired();
    coord_thread_kind 	purpose() const { return _purpose; }
    bool 		on_the_fly() const { 
			    w_assert3(_on_the_fly ==
				(purpose() == 
					participant::coord_recovery_handler)
					);
				return _on_the_fly; 
			}
    commit_protocol 	proto() { return _proto; }

    inline sm2pc_stats_t&	
				twopc_stats() { return *_2pcstats; }
#ifdef NOTDEF

#define GET_2PCSTAT(x) if(_message_handler) { _message_handler->twopc_stats().x } else { assert(0); }
#define INC_2PCSTAT(x) if(_message_handler) { _message_handler->twopc_stats().x++; } else { assert(0); }
#define ADD_2PCSTAT(x,y) if(_message_handler) { _message_handler->twopc_stats().x += (y); } else { assert(0); }
#define SET_2PCSTAT(x,y) if(_message_handler) { _message_handler->twopc_stats().x = (y); } else { assert(0); }

#define GET_MY2PCSTAT(x)  this->twopc_stats().x 
#define INC_MY2PCSTAT(x)  this->twopc_stats().x++
#define ADD_MY2PCSTAT(x,y) this->twopc_stats().x += (y)
#define SET_MY2PCSTAT(x,y) this->twopc_stats().x = (y);

#else

#define GET_2PCSTAT(x) smlevel_0::stats.summary_2pc.x
#define INC_2PCSTAT(x) smlevel_0::stats.summary_2pc.x++
#define ADD_2PCSTAT(x,y) smlevel_0::stats.summary_2pc.x  += (y)
#define SET_2PCSTAT(x,y) smlevel_0::stats.summary_2pc.x = (y)

#define GET_MY2PCSTAT(x) smlevel_0::stats.summary_2pc.x
#define INC_MY2PCSTAT(x) smlevel_0::stats.summary_2pc.x++
#define ADD_MY2PCSTAT(x,y) smlevel_0::stats.summary_2pc.x  += (y)
#define SET_MY2PCSTAT(x,y) smlevel_0::stats.summary_2pc.x = (y)
#endif

protected:
    void		handle_message(coord_thread_kind k);
    void 		set_thread_error(int) ;

protected:
    bool  	        _on_the_fly; 
    coord_thread_kind	_purpose;
    commit_protocol	_proto;
    bool		_retire;
    participant* 	_party;
    w_rc_t		_error;
    smutex_t		_mutex;
    sm2pc_stats_t* 	_2pcstats;

    // for linking threads together in _threads list
    w_link_t		_list_link; 

    scond_t		_condition; 
	// reaper : _condition -> there's a thread to reap
	// reply handler, commit handler : _condition -> got event of interest
	// all (twopc_thread): _condition -> error occurred- wake up waiter
	//

    int4_t		_sleeptime; // pause between retrans:
	// Subordinates sleep in resolve() during recovery after kicking
	// the coordinator
	// Coord/presumed_nothing - initialization (called by constructor)
	//     in for worker threads to resolve xcts
	// Coord/presumed_abort - reaper sleeps/ wakes up to reap workers 
	// Coord/workers wait before retransmitting
	// Coord/reaper waits(forever) to be kicked 
	//	by a fork of new worker thread

    int4_t		_timeout; // give up whatever you're doing
				// after this time
};

class subord_thread_t : public twopc_thread_t {
    friend class subordinate;
public:
    NORET 		subord_thread_t(subordinate *c, coord_thread_kind);
    NORET 		~subord_thread_t();
    void       		vtable_collect(vtable_info_t& t) ;
    void 		run() ;

protected:
    rc_t 		resolve_all(); // for PA

    void 		died(server_handle_t &);  // died, mapping may be invalid
    void 		recovered(server_handle_t &s); // recovered, mapping is up-to-date

    static Endpoint&	NullEP;

private:
    subordinate*	_subord;

    // TODO: allow coordination by more
    // than one coordinator
    bool		_coord_alive;
};

class coord_thread_t : public twopc_thread_t {
    friend class coordinator;

public:
    // for coord_commit_handler: provide num_threads and spec_list
    NORET 	coord_thread_t(coordinator *c, 
		    twopc_thread_t::coord_thread_kind,
		    const gtid_t &dtid,
		    int num_threads,
		    const server_handle_t *spec_list,
		    int timeout,
		    bool prepare_only=false
		    );
    // for coord_reply_handler and subord_commit_handler
    NORET 	coord_thread_t(coordinator *c, 
		    twopc_thread_t::coord_thread_kind
		    );

    // for coord_recovery_handler: on_the_fly indicates
    // how the thread will be reaped
    NORET 	coord_thread_t(coordinator *c, 
		    twopc_thread_t::coord_thread_kind,
		    const gtid_t &dtid, bool on_the_fly
		    );
	
    NORET 	~coord_thread_t();

    NORET	W_FASTNEW_CLASS_DECL(coord_thread_t);
	
    void       	vtable_collect(vtable_info_t& t) ;
    void 	run() ;

protected:
    void 	recover(); // for PN

    rc_t 	died(server_handle_t &s); //died, mapping may be invalid
    rc_t 	recovered(server_handle_t &s);  // recovered, mapping is up-to-date

    void	got_vote(message_t &m, server_handle_t &srvaddr);
    void	got_ack(message_t &m, server_handle_t &srvaddr);
    void 	set_coord_state(int threadnum, int seq=0) ;
    rc_t	resolve(const gtid_t &d, bool abort_it = false);

private:
		// resets sequence# as a side effect:
    void 	set_state(coord_state s, bool flush=false)  {
		   w_assert3(_mutex.is_mine());
		   _entry->set_state(s);
		   if(flush) _entry->put_state();
		   _sequence = 0;
		}
    coord_state state() const {
		   w_assert3(_mutex.is_mine());
		   return _entry->state();

		}
    int		awaiting() const {  return _awaiting; }
    int		died() const {  return _died; }
    int		recovered() const {  return _recovered; }

private:
    coordinator*	_coord;
    gtid_t		_tid;
    int			_sequence; 
    log_entry*		_entry;
    int			_awaiting; // some subordinate died
    int			_died; // # subordinate died
    int			_recovered; // # subordinates recovered
    bool                _stop_when_prepared;
};


/*<std-footer incl-file-exclusion='COORD_H'>  -- do not edit anything below this line -- */

#endif          /*</std-footer>*/

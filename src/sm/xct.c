/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

#define DBGTHRD(arg) DBG(<<" th."<<me()->id << " " arg)
/*
 *  $Id: xct.c,v 1.122 1996/07/01 22:07:55 nhall Exp $
 */
#define SM_SOURCE
#define XCT_C

#ifdef __GNUG__
#pragma implementation "xct.h"
#endif

#include <new.h>
#include "sm_int.h"
#include <srvid_t.h>
#include "sdesc.h"
#include "lock_x.h"
#include "remote_s.h"
#include "remote_client.h"
#ifdef __GNUG__
#include <iomanip.h>
#endif
#include <new.h>
#include <crash.h>

#ifdef __GNUG__
template class w_list_t<xct_t>;
template class w_list_i<xct_t>;
template class w_list_t<xct_dependent_t>;
template class w_list_i<xct_dependent_t>;
template class w_list_t<master_xct_proxy_t>;
template class w_list_i<master_xct_proxy_t>;
template class w_keyed_list_t<xct_t, tid_t>;
template class w_descend_list_t<xct_t, tid_t>;
template class w_list_const_i<lock_request_t>;
#ifdef MULTI_SERVER
template class w_list_t<pid_hash_t>;
template class w_list_i<pid_hash_t>;
template class w_hash_t<pid_hash_t, lpid_t>;
#endif /* MULTI_SERVER */
#endif

#define LOGTRACE(x)	DBG(x)
#define DBGX(arg) DBG(<<" th."<<me()->id << " " << "tid." << _tid  arg)


#ifdef DEBUG
extern "C" void debugflags(const char *);
void
debugflags(const char *a) 
{
   _debug.setflags(a);
}
#endif

/*********************************************************************
 *
 *  The xct list is sorted for easy access to the oldest and
 *  youngest transaction. All instantiated xct_t objects are
 *  in the list.
 *
 *********************************************************************/
smutex_t			xct_t::_xlist_mutex("_xctlist");
w_descend_list_t<xct_t, tid_t>	xct_t::_xlist(offsetof(xct_t, _tid), 
					      offsetof(xct_t, _xlink));

/*********************************************************************
 *
 *  _nxt_tid is used to generate unique transaction id
 *  _1thread_name is the name of the mutex protecting the xct_t from
 *  	multi-thread access
 *
 *********************************************************************/
tid_t 				xct_t::_nxt_tid = tid_t::null;
const char* 			xct_t::_1thread_name = "1th";

/*********************************************************************
 *
 *  Xcts are dynamically allocated. Use fastnew facility to 
 *  allocate many at a time.
 *
 *********************************************************************/
W_FASTNEW_STATIC_DECL(xct_t, 32);



/*********************************************************************
 *
 *  Print out tid and status
 *
 *********************************************************************/
ostream&
operator<<(ostream&		o, const xct_t&	x)
{
    o << x._tid << " " << x.status()
	<< " " << x.get_lock_level_const()
	<< " " << x.timeout_c()
    ;
    if(x.lock_info()) {
	 o << *x.lock_info();
    }
    return o << endl;
}



/*********************************************************************
 *
 *  int
 *  xct_t::cleanup()
 *
 *  Abort all active transactions and clean up environment.
 *  Return # prepared transactions left (not disposed-of)
 *
 *********************************************************************/
int
xct_t::cleanup(bool dispose_prepared)
{
    bool	changed_list;
    int		nprepared = 0;
    xct_t* 	xd;
    do {
	/*
	 *  We cannot delete an xct while iterating. Use a loop
	 *  to iterate and delete one xct for each iteration.
	 */
	xct_i i;
	changed_list = false;
	if ((xd = i.next())) switch(xd->state()) {
	case xct_active: {
		me()->attach_xct(xd);
		/*
		 *  We usually want to shutdown cleanly. For debugging
		 *  purposes, it is sometimes desirable to simply quit.
		 *
		 *  NB:  if a vas has multiple threads running on behalf
		 *  of a tx at this point, it's going to run into trouble.
		 */
		if (shutdown_clean) {
		    W_COERCE( xd->abort() );
		} else {
		    W_COERCE( xd->dispose() );
		}
		delete xd;
		changed_list = true;
	    } 
	    break;

	case xct_ended: {
		DBG(<< xd->tid() <<"deleting " << " w/ state=" << xd->state() );
		delete xd;
		changed_list = true;
	    }
	    break;

	case xct_prepared: {
		if(dispose_prepared) {
		    me()->attach_xct(xd);
		    W_COERCE( xd->dispose() );
		    delete xd;
		    changed_list = true;
		} else {
		    DBG(<< xd->tid() <<"keep -- prepared ");
		    nprepared++;
		}
	    } 
	    break;

	default: {
		DBG(<< xd->tid() <<"skipping " << " w/ state=" << xd->state() );
	    }
	    break;
	
	}

    } while (xd && changed_list);
    return nprepared;
}




/*********************************************************************
 *
 *  xct_t::num_active_xcts()
 *
 *  Return the number of active transactions (equivalent to the
 *  size of _xlist.
 *
 *********************************************************************/
w_base_t::uint4_t
xct_t::num_active_xcts()
{
    w_base_t::uint4_t num;
    W_COERCE(_xlist_mutex.acquire());
    num = _xlist.num_members();
    _xlist_mutex.release();
    return  num;
}



/*********************************************************************
 *
 *  xct_t::look_up(tid)
 *
 *  Find the record for tid and return it. If not found, return 0.
 *
 *********************************************************************/
xct_t* 
xct_t::look_up(const tid_t& tid)
{
    xct_t* xd;
    xct_i iter;

    while ((xd = iter.next())) {
	if (xd->_tid == tid) {
	    return xd;
	}
    }
    return 0;
}


/*********************************************************************
 *
 *  xct_t::oldest_tid()
 *
 *  Return the tid of the oldest active xct.
 *
 *********************************************************************/
tid_t
xct_t::oldest_tid()
{
    W_COERCE(_xlist_mutex.acquire());
    xct_t* xd = _xlist.last();
    _xlist_mutex.release();
    return xd ? xd->_tid : _nxt_tid;
}


#ifdef NOTDEF
/*********************************************************************
 *
 *  xct_t::min_first_lsn()
 *
 *  Compute and return the minimum first_lsn of all active xcts.
 *
 *********************************************************************/
lsn_t
xct_t::min_first_lsn() 
{
    W_COERCE(_xlist_mutex.acquire());
    w_list_i<xct_t> i(_xlist);
    lsn_t lsn = lsn_t::max;
    xct_t* xd;
    while ((xd = i.next()))  {
	if (xd->_first_lsn)  {
	    if (xd->_first_lsn < lsn)  lsn = xd->_first_lsn;
	}
    }
    _xlist_mutex.release();
    return lsn;
}
#endif
    

/*********************************************************************
 *
 *  xct_t::xct_t(timeout)
 *
 *  Begin a transaction. The transaction id is assigned automatically,
 *  and the xct record is inserted into _xlist.
 *
 *********************************************************************/
xct_t::xct_t(long timeout, type_t type) 
    :   
	_threads_attached(0),
	_waiters("mpl_xct"),
	_tid(_nxt_tid.incr()), 
	_lock_level(cc_alg),
	_timeout(timeout), 
	_1thread(_1thread_name), 
	_state(xct_active), 
	_vote(vote_bad), 
	_global_tid(0),
	_read_only(false),
	// _first_lsn, _last_lsn, _undo_nxt, _xlink,
	_lock_cache_enable(true),
	_dependent_list(offsetof(xct_dependent_t, _link)),
#ifdef MULTI_SERVER
	_master_proxy_list(master_xct_proxy_t::link_offset()),
	_master_site(0), 
	_callback_proxy(0), 
	_callback(0),
	_dirty_pages(0),
	_EX_locked_files(0),
#endif
	_lock_info(0),
	_log_state(ON),
	_last_log(0),
	_log_buf(0),
	// _last_mod_page,
	_last_mod_thread(0),
	_log_bytes_fwd(0),
	_log_bytes_bwd(0),
	_alloced_page(false),
	_freed_page(false),
	_in_compensated_op(0)
{
    // TODO - put something meaningful here when
    // server-server 2PC is in place 
    memset(&_coord_handle, '\0', sizeof(_coord_handle));

    _log_buf = new logrec_t;
#ifdef PURIFY
    memset(_log_buf, '\0', sizeof(logrec_t));
#endif

    if (!_log_buf)  W_FATAL(eOUTOFMEMORY);

#ifdef MULTI_SERVER
    if(comm_m::use_comm()) {
	if (type == t_remote || type == t_master) {
	    _callback = new callback_op_t;
	    if (!_callback) W_FATAL(eOUTOFMEMORY);
	}

	if (type == t_master) {
	    _dirty_pages = new w_hash_t<pid_hash_t, lpid_t>(128,
			offsetof(pid_hash_t, pid), offsetof(pid_hash_t, link));
            if (!_dirty_pages) W_FATAL(eOUTOFMEMORY);

	    _EX_locked_files = new file_EX_lock_cache_t;
	    if (!_EX_locked_files) W_FATAL(eOUTOFMEMORY);
        }
    }
#endif

    _lock_info = new xct_lock_info_t(type);
    if (! _lock_info)  W_FATAL(eOUTOFMEMORY);

    if (timeout == WAIT_SPECIFIED_BY_THREAD) {
		// override in this case
		_timeout = me()->lock_timeout();
    }

    w_assert3(_timeout >= 0 || _timeout == WAIT_FOREVER);

    W_COERCE(_xlist_mutex.acquire());
    _xlist.put_in_order(this);
    _xlist_mutex.release();
    
    me()->attach_xct(this);
    
    incr_begin_cnt();
}


/*********************************************************************
 *
 *   xct_t::xct_t(tid, state, last_lsn, undo_nxt, timeout)
 *
 *   Manually begin a specific transaction with a specific
 *   tid. This form mainly used by the restart recovery routines
 *
 *********************************************************************/
xct_t::xct_t(const tid_t& t, state_t s, const lsn_t& last_lsn,
	     const lsn_t& undo_nxt, long timeout) 
    :    
	_threads_attached(0),
	_waiters("mpl_xct"),
	_tid(t),
	_lock_level(cc_alg),
	_timeout(timeout), 
	_1thread(_1thread_name),
	_state(s), 
	_vote(vote_bad), 
	_global_tid(0),
	_read_only(false),
	// _first_lsn, 
	_last_lsn(last_lsn),
	_undo_nxt(undo_nxt),
	//_xlink,
	_lock_cache_enable(true),
	_dependent_list(offsetof(xct_dependent_t, _link)),
#ifdef MULTI_SERVER
	_master_proxy_list(master_xct_proxy_t::link_offset()),
	_master_site(0), 
	_callback_proxy(0), 
	_callback(0),
	_dirty_pages(0),
	_EX_locked_files(0),
#endif
	_lock_info(0),
	_log_state(ON),
	_last_log(0),
	_log_buf(0),
	// _last_mod_page
	_last_mod_thread(0),
	_log_bytes_fwd(0),
	_log_bytes_bwd(0),
	_alloced_page(false),
	_freed_page(false),
	_in_compensated_op(0)
{

    // TODO - put something meaningful here when
    // server-server 2PC is in place 
    memset(&_coord_handle, '\0', sizeof(_coord_handle));

    if (_tid > _nxt_tid)   _nxt_tid = _tid;

    _log_buf = new logrec_t;
    if (!_log_buf)  W_FATAL(eOUTOFMEMORY);

    _lock_info = new xct_lock_info_t(t_master);
    if (! _lock_info)  W_FATAL(eOUTOFMEMORY);

    if (timeout == WAIT_SPECIFIED_BY_THREAD) {
	    _timeout = me()->lock_timeout();
    }
    w_assert3(_timeout >= 0 || _timeout == WAIT_FOREVER);

    _lock_level = cc_alg;

    W_COERCE(_xlist_mutex.acquire());
    _xlist.put_in_order(this);
    _xlist_mutex.release();
    
    // sm.tcb()->xct = 0;
    w_assert1(me()->xct() == 0);
    
#ifdef DEBUG
    W_COERCE(_xlist_mutex.acquire());
    {
	// make sure that _xlist is in order
	w_list_i<xct_t> i(_xlist);
	tid_t t = tid_t::null;
	xct_t* xd;
	while ((xd = i.next()))  {
	    w_assert1(t < xd->_tid);
	}
	w_assert1(t <= _nxt_tid);
    }
    _xlist_mutex.release();
#endif /*DEBUG*/

}


/*********************************************************************
 *
 *  xct_t::~xct_t()
 *
 *  Clean up and free up memory used by the transaction. The 
 *  transaction has normally ended (committed or aborted) 
 *  when this routine is called.
 *
 *********************************************************************/
xct_t::~xct_t()
{
    FUNC(xct_t::~xct_t);
    DBGX( << " ended" );

    w_assert3(_state == xct_ended);
    w_assert3(_in_compensated_op==0);

    if (shutdown_clean)  {
	w_assert1(me()->xct() == 0);
#ifdef MULTI_SERVER
	if(comm_m::use_comm()) {
	    w_assert1(_callback_proxy == 0 && _master_site == 0);
	}
#endif
    }

    W_COERCE(_xlist_mutex.acquire());
    _xlink.detach();
    _xlist_mutex.release();

    while (_dependent_list.pop());

    if (_log_buf)  delete _log_buf;
#ifdef MULTI_SERVER
    if (_callback) delete _callback;
    if (_dirty_pages) delete _dirty_pages;
    if (_EX_locked_files) delete _EX_locked_files;
#endif
    if (_lock_info) delete _lock_info;
    if (_global_tid) delete _global_tid;

    // clean up what's stored in the thread
    me()->no_xct();
}

/*********************************************************************
 *
 *  bool xct_t::is_extern2pc()
 *
 *  return true iff this tx is participating
 *  in an external 2-phase commit protocol, 
 *  which is effected by calling enter2pc() on this
 *
 *********************************************************************/
bool			
xct_t::is_extern2pc() 
const
{
    // true if is a thread of global tx
    return _global_tid != 0;
}

/*********************************************************************
 *
 *  xct_t::set_coordinator(...)
 *  xct_t::get_coordinator(...)
 *
 *  get and set the coordinator handle
 *  NOT IMPLEMENTED -- it's just junk getting
 *  logged for the moment.  When full peer
 *  servers is implemented, the coord_t type will
 *  have to be expanded to include an internet address
 *
 *********************************************************************/
void
xct_t::set_coordinator(const coord_t &h) 
{
    DBG(<<"set_coord for tid " << _tid
	<< " handle is " << h);
    _coord_handle = h;
}

const smlevel_0::coord_handle_t &
xct_t::get_coordinator() const
{
    return _coord_handle;
}

/*********************************************************************
 *
 *  xct_t::change_state(new_state)
 *
 *  Change the status of the transaction to new_state. All 
 *  dependents are informed of the change.
 *
 *********************************************************************/
void
xct_t::change_state(state_t new_state)
{
#ifndef MULTI_SERVER
    w_assert3(one_thread_attached());
#endif

    w_assert3(_state != new_state);
    state_t old_state = _state;
    _state = new_state;

    w_list_i<xct_dependent_t> i(_dependent_list);
    xct_dependent_t* d;
    while ((d = i.next()))  {
	d->xct_state_changed(old_state, new_state);
    }
}


/*********************************************************************
 *
 *  xct_t::add_dependent(d)
 *
 *  Add a dependent to the dependent list of the transaction.
 *
 *********************************************************************/
rc_t
xct_t::add_dependent(xct_dependent_t* dependent)
{
    // PROTECT
    acquire_1thread_mutex();
    w_assert3(dependent->_link.member_of() == 0);
    
    _dependent_list.push(dependent);
    release_1thread_mutex();
    return RCOK;
}

/*********************************************************************
 *
 *  xct_t::find_dependent(d)
 *
 *  Return true iff a given dependent(ptr) is in the transaction's
 *  list.   This must cleanly return false (rather than crashing) 
 *  if d is a garbage pointer, so it cannot dereference d
 *  Used by value-added servers.
 *
 *********************************************************************/
bool
xct_t::find_dependent(xct_dependent_t* ptr)
{
    // PROTECT
    xct_dependent_t	*d;
    acquire_1thread_mutex();
    w_list_i<xct_dependent_t> 		iter(_dependent_list);
    while((d=iter.next())) {
	if(d == ptr) {
	    release_1thread_mutex();
	    return true;
	}
    }
    release_1thread_mutex();
    return false;
}


/*********************************************************************
 *
 *  xct_t::prepare()
 *
 *  Enter prepare state. For 2 phase commit protocol.
 *  Set vote_abort or vote_commit if any
 *  updates were done, else return vote_readonly
 *
 *  We are called here if we are participating in external 2pc,
 *  OR we're just committing 
 *
 *  This does NOT do the commit
 *
 *********************************************************************/
rc_t 
xct_t::prepare()
{
    // This is to be applied ONLY to the local thread.
    // Distribution of prepare requests is handled in the
    // ss_m layer (except in xct_auto_abort_t transactions)

    // w_assert3(one_thread_attached());
    if( _threads_attached > 1 && !is_remote_proxy()) {
	return RC(eTWOTRANS);
    }
    _flush_logbuf();
    w_assert1(_state == xct_active);

    // default unless all works ok
    _vote = vote_abort;

#ifdef DEBUG
    // temporary
    if(_debug.flag_on("dumplocks",__FILE__)) {
	(void) dump_locks(cerr);
    }
#endif

    _read_only = (_first_lsn == lsn_t::null);

    if(_read_only) {
	_vote = vote_readonly;
	// No need to log prepare record
#ifdef DEBUG
	// This is really a bogus assumption,
	// since a tx could have explicitly
	// forced an EX lock and then never
	// updated anything.  We'll leave it
	// in until we can run all the scripts.
	// The question is: should the lock 
	// be held until the tx is resolved,
	// even though no updates were done???
	// 
	// THIS IS A SIZZLING QUESTION.  Needs to be resolved
	{
	    int	total_EX, total_IX, total_SIX, num_extents;
	    W_DO(lock_info()->get_lock_totals(total_EX, total_IX, total_SIX, num_extents));
	    w_assert3(total_EX == 0);
	}
#endif
	// If commit is done in the readonly case,
	// it's done by ss_m::_prepare_xct(), NOT HERE

	change_state(xct_prepared);
	return RCOK;
    }

    ///////////////////////////////////////////////////////////
    // NOT read only
    ///////////////////////////////////////////////////////////

    if(is_extern2pc() || is_distributed()) {
	DBG(<<"logging prepare because e2pc=" << is_extern2pc()
		<<" distrib=" << is_distributed());
	W_DO(log_prepared());

    } else {
	// Not distributed -- no need to log prepare
    }

    /******************************************
    // Don't set the state until after the
    // log records are written, so that
    // if a checkpoint is going on right now,
    // it'll not log this tx in the checkpoint
    // while we are logging it.
    ******************************************/

    change_state(xct_prepared);

    _vote = vote_commit;
    return RCOK;
}

/*********************************************************************
 * xct_t::log_prepared(bool in_chkpt)
 *  
 * log a prepared tx 
 * (fuzzy, so to speak -- can be intermixed with other records)
 * 
 * 
 * called from xct_t::prepare() when tx requests prepare,
 * also called during checkpoint, to checkpoint already prepared
 * transactions
 * When called from checkpoint, the argument should be true, false o.w.
 *
 *********************************************************************/

rc_t
xct_t::log_prepared(bool in_chkpt)
{
    FUNC(xct_t::log_prepared);
    w_assert1(_state == in_chkpt?xct_prepared:xct_active);

    w_rc_t rc;

    if( !in_chkpt)  {
	// grab the mutex that serializes prepares & chkpts
	smlevel_3::chkpt->chkpt_mutex_acquire();
    }


    CRASHTEST("prepare.unfinished.0");

    int total_EX, total_IX, total_SIX, num_extents;
    rc = log_xct_prepare_st(_global_tid, _coord_handle);
    if (rc) { RC_AUGMENT(rc); goto done; }

    CRASHTEST("prepare.unfinished.1");
    {
	lockid_t	*space_l=0;
	lock_mode_t	*space_m=0;

	rc = lock_info()->
	    get_lock_totals(total_EX, total_IX, total_SIX, num_extents);
	if (rc) { RC_AUGMENT(rc); goto done; }

	/*
	 * We will not get here if this is a read-only
	 * transaction -- according to _read_only, above
	*/

	/*
	 *  Figure out how to package the locks
	 *  If they all fit in one record, do that.
	 *  If there are lots of some kind of lock (most
	 *  likely EX in that case), split those off and
	 *  write them in a record of uniform lock mode.
	 */  
	int i;

	/*
	 * NB: for now, we assume that ONLY the EX locks
	 * have to be acquired, and all the rest are 
	 * acquired by virtue of the hierarchy.
	 * ***************** except for extent locks -- they aren't
	 * ***************** in the hierarchy.
	 *
	 * If this changes (e.g. any code uses dir_m::access(..,.., true)
	 * for some non-EX lock mode, we have to figure out how
	 * to locate those locks *not* acquired by virtue of an
	 * EX lock acquisition, and log those too.
	 *
	 * We have the mechanism here to do the logging,
	 * but we don't have the mechanism to separate the
	 * extraneous IX locks, say, from those that DO have
	 * to be logged.
	 */

#ifdef LOG_ALL_MODES

	i = total_EX + total_IX + total_SIX + num_extents;
#else
	i = total_EX + num_extents;
#endif

#ifdef LOG_ALL_MODES
	if(i < prepare_all_lock_t::max_locks_logged) {
	    /*
	    // EX, IX, SIX 
	    // we can fit them *all* in one record
	    */
	    space_l = new lockid_t[i];
	    space_m = new lock_mode_t[i];

	    rc = lock_info()-> get_locks(NL, i, space_l, space_m);
	    if (rc) { RC_AUGMENT(rc); goto done; }

	    CRASHTEST("prepare.unfinished.2");

	    rc = log_xct_prepare_alk( i, space_l, space_m);
	    if (rc) { RC_AUGMENT(rc); goto done; }

	} 
#else
	if(i < prepare_lock_t::max_locks_logged) {
	    /*
	    // EX ONLY
	    // we can fit them *all* in one record
	    */
	    space_l = new lockid_t[i];

	    rc = lock_info()-> get_locks(EX, i, space_l);
	    if (rc) { RC_AUGMENT(rc); goto done; }

	    CRASHTEST("prepare.unfinished.2");

	    rc = log_xct_prepare_lk( i, EX, space_l);
	    if (rc) { RC_AUGMENT(rc); delete[] space_l; goto done; }
	    delete[] space_l;
	} 
#endif /* LOG_ALL_MODES */

	else {
	    // can't fit them *all* in one record
	    // so first we log the EX locks only,
	    // the rest in one or more records

	    /* EX only */
	    i = total_EX;
	    space_l = new lockid_t[i];

	    rc = lock_info()-> get_locks(EX, i, space_l);
	    if (rc) { RC_AUGMENT(rc); goto done; }

	    // Use as many records as needed for EX locks:
	    //
	    // i = number to be recorded in next log record
	    // j = number left to be recorded altogether
	    // k = offset into space_l array
	    //
	    i = prepare_lock_t::max_locks_logged;
	    int j=total_EX, k=0;
	    while(i < total_EX) {
		rc = log_xct_prepare_lk(prepare_lock_t::max_locks_logged, EX, &space_l[k]);
		if (rc) { RC_AUGMENT(rc); goto done; }
		i += prepare_lock_t::max_locks_logged;
		k += prepare_lock_t::max_locks_logged;
		j -= prepare_lock_t::max_locks_logged;
	    }
	    CRASHTEST("prepare.unfinished.2");
	    // log what's left of the EX locks (that's in j)

	    rc = log_xct_prepare_lk(j, EX, &space_l[k]);
	    if (rc) { RC_AUGMENT(rc); goto done; }
#ifdef LOG_ALL_MODES

	    // We don't have the mechanism in place for this --
	    // for figuring out which of the IX, SIX locks
	    // to log
	    w_assert1(0);
#endif
	}

#define LOG_EXTENT_LOCKS
#ifdef LOG_EXTENT_LOCKS
	{

	/* Now log the extent locks */
	i = num_extents;
	space_l = new lockid_t[i];
	space_m = new lock_mode_t[i];
	rc = lock_info()-> get_locks(NL, i, space_l, space_m, true);
	if (rc) { RC_AUGMENT(rc); goto done; }
	if(i < prepare_lock_t::max_locks_logged) {
	    CRASHTEST("prepare.unfinished.2");
	    rc = log_xct_prepare_alk( i, space_l, space_m);
	    if (rc) { RC_AUGMENT(rc); goto done; }
	} else {
	    assert(0);
	}
	}
#endif /* LOG_EXTENT_LOCKS */

	if(space_l) delete[] space_l;
	if(space_m) delete[] space_m;
    }

    CRASHTEST("prepare.unfinished.3");

    rc = log_xct_prepare_fi(total_EX, total_IX, total_SIX, num_extents);
    if (rc) { RC_AUGMENT(rc); goto done; }

done:
    // We have to force the log record to the log
    // If we're not in a chkpt, we also have to make
    // it durable
    _flush_logbuf(!in_chkpt);

    if( !in_chkpt)  {
	// free the mutex that serializes prepares & chkpts
	smlevel_3::chkpt->chkpt_mutex_release();
    }
    return rc;
}


/*********************************************************************
 *
 *  xct_t::_commit(flags)
 *
 *  Commit the transaction. If flag t_lazy, log is not synced.
 *  If flag t_chain, a new transaction is instantiated inside
 *  this one, and inherits all its locks.
 *
 *********************************************************************/
rc_t
xct_t::_commit(uint4_t flags)
{
    if(is_extern2pc()) {
	w_assert1(_state == xct_prepared);
    } else {
	w_assert1(_state == xct_active || _state == xct_prepared);
    };
    CRASHTEST("commit.1");
    _flush_logbuf();
    
    change_state(flags & t_chain ? xct_chaining : xct_committing);

    change_state(xct_ended);

    CRASHTEST("commit.2");

    if (_last_lsn)  {
	/*
	 *  If xct generated some log, write a synchronous
	 *  Xct End Record.
	 * 
	 *  Logging a commit must be serialized with logging
	 *  prepares (done by chkpt).
	 */
	bool do_release = false;
	if( _threads_attached > 1) {

	    // wait for the checkpoint to finish
	    smlevel_3::chkpt->chkpt_mutex_acquire();
	    do_release = false;

	    if( _threads_attached > 1) {
		smlevel_3::chkpt->chkpt_mutex_release();
		return RC(eTWOTRANS);
	    }
	}
	W_DO( log_xct_end() );
	CRASHTEST("commit.3");
	if(do_release) {
	    smlevel_3::chkpt->chkpt_mutex_release();
	}

	if (!(flags & t_lazy) /* && !_read_only */)  {
	    if (log) {
		_flush_logbuf(true);
	    }
	    // W_COERCE(log->flush_all());	/* sync log */
	}
    }

    /*
     *  Free all locks. Do not free locks if chaining.
     */
    if (! (flags & t_chain))  {
	W_COERCE( lm->unlock_duration(t_long, TRUE) )
    }

    me()->detach_xct(this);	// no transaction for this thread
    incr_commit_cnt();

    /*
     *  Xct is now committed
     */

    if (flags & t_chain)  {
	/*
	 *  Start a new xct in place
	 */
        _xlink.detach();
        (tid_t&)_tid = _nxt_tid.incr();
        _first_lsn = _last_lsn = _undo_nxt = lsn_t::null;
        _last_log = 0;
        _log_state = ON;
        _lock_cache_enable = true;

	W_COERCE(_xlist_mutex.acquire());
        _xlist.put_in_order(this);
	_xlist_mutex.release();

	_alloced_page = false;
	_freed_page = false;

	// should already be out of compensated operation
	w_assert3( _in_compensated_op==0 );

        me()->attach_xct(this);
        incr_begin_cnt();
        change_state(xct_active);
    }

    if (_freed_page) {
	// this transaction allocated pages, so the abort will
	// free them, invalidating the io managers cache.
	io->invalidate_free_page_cache();
    }

    return RCOK;
}


/*********************************************************************
 *
 *  xct_t::abort()
 *
 *  Abort the transaction by calling rollback().
 *
 *********************************************************************/
rc_t
xct_t::abort()
{
    // w_assert3(one_thread_attached());
    if( _threads_attached > 1) {
	return RC(eTWOTRANS);
    }
    _flush_logbuf();

    w_assert1(_state == xct_active || _state == xct_prepared);

    change_state(xct_aborting);

    W_DO( rollback(lsn_t::null) );

    change_state(xct_ended);

    if (_last_lsn) {
	/*
	 *  If xct generated some log, write a 
	 *  Xct End Record. 
	 *  We flush because if this was a prepared
	 *  transaction, it really must be synchronous 
	 */
	W_DO( log_xct_end() );
	_flush_logbuf(true);
    }

    /*
     *  Free all locks 
     */
    W_COERCE( lm->unlock_duration(t_long, TRUE) )

    me()->detach_xct(this);	// no transaction for this thread
    incr_abort_cnt();

    DBGX(<< "Ratio:"
	<< " bFwd: " << _log_bytes_fwd << " bBwd: " << _log_bytes_bwd
	<< " ratio b/f:" << (((float)_log_bytes_bwd)/_log_bytes_fwd)
    );
    return RCOK;
}


/*********************************************************************
 *
 *  xct_t::enter2pc(...)
 *
 *  Mark this tx as a thread of a global tx (participating in EXTERNAL
 *  2PC)
 *
 *********************************************************************/
rc_t 
xct_t::enter2pc(const gtid_t &g)
{
    if( _threads_attached > 1) {
	return RC(eTWOTRANS);
    }
    w_assert1(_state == xct_active);

    if(is_extern2pc()) {
	return RC(eEXTERN2PCTHREAD);
    }
    _global_tid = new gtid_t;
    if(!_global_tid) {
	W_FATAL(eOUTOFMEMORY);
    }
    DBG(<<"ente2pc for tid " << _tid 
	<< " global tid is " << g);
    *_global_tid = g;

    return RCOK;
}

/*********************************************************************
 *
 *  xct_t::recover2pc(...)
 *
 *  Locate a prepared tx with this global tid
 *
 *********************************************************************/

rc_t 
xct_t::recover2pc(const gtid_t &g,
	bool	/*mayblock*/,
	xct_t	*&xd)
{
    w_list_i<xct_t> i(_xlist);
    while ((xd = i.next()))  {
	if( xd->status() == xct_prepared ) {
	    if(xd->_global_tid &&
		*(xd->_global_tid) == g) {
		// found
		// TODO  try to reach the coordinator
		return RCOK;
	    }
	}
    }
    return RC(eNOSUCHPTRANS);
}

/*********************************************************************
 *
 *  xct_t::save_point(lsn)
 *
 *  Generate and return a save point in "lsn".
 *
 *********************************************************************/
rc_t
xct_t::save_point(lsn_t& lsn)
{
    // cannot do this with >1 thread attached
    w_assert3(one_thread_attached());

    _flush_logbuf();
    lsn = _last_lsn;
    return RCOK;
}


/*********************************************************************
 *
 *  xct_t::dispose()
 *
 *  Make the transaction disappear.
 *  This is only for simulating crashes.  It violates
 *  all tx semantics.
 *
 *********************************************************************/
rc_t
xct_t::dispose()
{
    w_assert3(one_thread_attached());
    _flush_logbuf();
    W_COERCE( lm->unlock_duration(t_long, TRUE) );
    _state = xct_ended; // unclean!
    me()->detach_xct(this);
    return RCOK;
}


/*********************************************************************
 *
 *  xct_t::flush_logbuf()
 *
 *  public version of _flush_logbuf()
 *
 *********************************************************************/
void
xct_t::flush_logbuf()
{
    /*
    // let only the thread that did the update
    // do the flushing
    */
#ifdef DEBUG
    if( _threads_attached < 2) {
        w_assert3(_threads_attached == 1);
        w_assert3(_last_mod_thread == 0 || _last_mod_thread == me());
    }
    if(_last_mod_thread == me()) {
        // we had better be able to acquire this mutex
        w_assert3(_1thread.is_mine() || ! _1thread.is_locked());
    }
#endif

    if(_last_mod_thread == me()) {
	acquire_1thread_mutex();
	_flush_logbuf();
	release_1thread_mutex();
    }
}

/*********************************************************************
 *
 *  xct_t::_flush_logbuf(bool sync=false)
 *
 *  Write the log record buffered and update lsn pointers.
 *  If "sync" is true, force it to disk also.
 *
 *********************************************************************/
void
xct_t::_flush_logbuf( bool sync )
{
    // ASSUMES ALREADY PROTECTED BY MUTEX

#ifndef MULTI_SERVER
    w_assert3( _1thread.is_mine() || one_thread_attached());
#endif
    if (_last_log)  {
	DBGTHRD ( << " xct_t::_flush_logbuf " << sync );
	_last_log->fill_xct_attr(_tid, _last_lsn);

	//
	// debugging prints a * if this record was written
	// during rollback
	//
	DBGX( << "logging" << ((char *)(status()==xct_aborting)?"*":" ")
		<< " lsn:" << log->curr_lsn() 
		<< " rec:" << *_last_log 
		<< " size:" << _last_log->length()  
		<< " prevlsn:" << _last_log->prev() 
		);

	if (status()==xct_aborting) {
	    _log_bytes_bwd += _last_log-> length();
	} else {
	    _log_bytes_fwd += _last_log-> length();
	}
	/*
	 *  This log insert must succeed... the operation has already
	 *  been performed. If the log cannot be inserted, the database
	 *  will be corrupted beyond repair.
	 */

	if (_last_log->pid().vol().is_remote()) {
#ifdef MULTI_SERVER
	    w_assert3(comm_m::use_comm());

	    lsn_t last_remote_lsn;
	    W_COERCE(rm->log_insert(*_last_log, last_remote_lsn));
	    _last_log = 0;

	    if (_last_mod_page)  {
		w_assert3(_last_mod_thread == me());
		_last_mod_page.set_lsn(last_remote_lsn);
		_last_mod_page.unfix_dirty();
                _last_mod_thread  = 0;
	    }
#else
	    W_FATAL(eINTERNAL);
#endif
	} else {
	    W_COERCE( log->insert(*_last_log, &_last_lsn) );
	    if ( ! _first_lsn)  
	        _first_lsn = _last_lsn;
	
	    _undo_nxt = (_last_log->is_cpsn() ? _last_log->undo_nxt() :
		     _last_lsn);
	    _last_log = 0;

	    if (_last_mod_page)  {
                w_assert3(_last_mod_thread == me());
	        _last_mod_page.set_lsn(_last_lsn);
	        _last_mod_page.unfix_dirty();
		_last_mod_thread  = 0;
	    }
        }
	if( sync ) {
	    W_COERCE( log->flush_all() );
	}
    }
}


/*********************************************************************
 *
 *  xct_t::get_logbuf(ret)
 *  xct_t::give_logbuf(ret)
 *
 *  Flush the logbuf and return it in "ret" for use. Caller must call
 *  give_logbuf(ret) to free it after use.
 *  Leaves the 1thread_mutex() acquired
 *
 *  These are used in the log_stub.i functions
 *  and ONLY there.  THE ERROR RETURN (running out of log space)
 *  IS PREDICATED ON THAT -- in that it's expected that in the case of
 *  a normal  return (no error), give_logbuf will be called, but in
 *  the error case (out of log space), it will not, and so we must
 *  release the mutex in get_logbuf.
 *  
 *
 *********************************************************************/
rc_t 
xct_t::get_logbuf(logrec_t*& ret)
{
    // PROTECT
    ret = 0;
    acquire_1thread_mutex();
    _flush_logbuf();

#define FUDGE sizeof(logrec_t)

    if (_state == xct_active)  {
	// for now, assume that the ratio is 1:1 for rollback 
	w_assert3(_log_bytes_bwd == 0);

	if( log->space_left() - FUDGE < _log_bytes_fwd ) {
	    DBGX(<< "Out of log space by space_to_abort calculation" );
	    release_1thread_mutex();

	    ss_m::errlog->clog <<error_prio 
		<< smlevel_0::stats << flushl;

	    ss_m::errlog->clog <<error_prio 
	    << _tid << " " << _last_lsn << " " << _first_lsn 

	    << " left= " << log->space_left() 
	    << " fwd=" << _log_bytes_fwd
	    << " bwd=" << _log_bytes_bwd
	    << flushl;

	    return RC(eOUTOFLOGSPACE);
	}
    }
    ret = _last_log = _log_buf;
    w_assert3(_1thread.is_mine());
    return RCOK;
}


void 
xct_t::give_logbuf(logrec_t* l)
{
    FUNC(xct_t::give_logbuf);
    DBG(<<"_last_log contains: "   << *l );
	
    // ALREADY PROTECTED from get_logbuf() call
    w_assert3(_1thread.is_mine());

    w_assert1(l == _last_log);
    w_assert1(!_last_mod_page);
    w_assert3(_last_mod_thread == 0);

    if (l->pid())  {
	W_COERCE(_last_mod_page.fix(l->_pid, (page_p::tag_t) l->tag(),
						LATCH_EX, TMP_NOFLAG));
	_last_mod_thread = me();
    }
    w_assert3(_1thread.is_mine());
    release_1thread_mutex();
}

/*********************************************************************
 *
 *  xct_t::start_crit() and xct_t::stop_crit()
 *
 *  start and stop critical sections vis-a-vis compensated operations
 *
 *********************************************************************/
void
xct_t::start_crit()
{
    // PROTECT
    acquire_1thread_mutex();
    _in_compensated_op ++;
    DBGX(    
	    << " START CRIT " 
	    << " in compensated op==" << _in_compensated_op
    );
}
void
xct_t::stop_crit()
{
    FUNC(xct_t::stop_crit);

    // w_assert3(_1thread.is_mine());
    DBGX(    
	    << " STOP CRIT " 
	    << " in compensated op==" << _in_compensated_op
    );
    // Flush so that if _last_mod_page is fixed,
    // it will no longer be so after this
    // compensation is written (if called from compensate())
    // or the error is handled (if called in case of error).
    // That should take care of most cases in which
    // a manager (e.g., io manager) has a mutex associated
    // with it, and fixed _last_mod_page as a side
    // effect.  Must free these latches/mutexes in the
    // correct order.

    w_assert3(_in_compensated_op>0);

    // UN-PROTECT 
    _in_compensated_op -- ;

    DBGX(    
	<< " out compensated op=" << _in_compensated_op
    );
    if(_in_compensated_op == 0) {

	// don't flush unless we have popped back
	// to the last compensate() of the bunch

	// Now see if this last item was supposed to be
	// compensated:
	if(_anchor != lsn_t::null) {
	   if(_last_log) {
	       if ( _last_log->is_cpsn()) {
		    DBG(<<"already compensated");
		    w_assert3(_anchor == _last_log->undo_nxt());
	       } else {
		   DBG(<<"SETTING anchor:" << _anchor);
		   _last_log->set_clr(_anchor);
	       }
	   } else {
	       DBG(<<"no _last_log:" << _anchor);
	    }
	}

	_anchor = lsn_t::null;

	_flush_logbuf();

    }
    release_1thread_mutex();
}

/*********************************************************************
 *
 *  xct_t::anchor()
 *
 *  Return a log anchor (begin a top level action).
 *
 *********************************************************************/
const lsn_t& 
xct_t::anchor()
{
    // PROTECT
    start_crit();

    // flush might change _last_lsn
    _flush_logbuf();

    if(_in_compensated_op == 1) {
	// the above start_crit() was the outermost call.
	// _anchor is set to null when _in_compensated_op goes to 0
	w_assert3(_anchor == lsn_t::null);
        _anchor = _last_lsn;
	DBGX(    << " anchor =" << _anchor);
    }
    DBGX(    << " anchor returns " << _last_lsn );

    return _last_lsn;
}


/*********************************************************************
 *
 *  xct_t::compensate_undo(lsn)
 *
 *  compensation during undo is handled slightly differently--
 *  the gist of it is the same, but the assertions differ, and
 *  we have to acquire the mutex first
 *********************************************************************/
void 
xct_t::compensate_undo(const lsn_t& lsn)
{
    DBGX(    << " compensate_undo (" << lsn << ") -- status=" << status());

    acquire_1thread_mutex(); 
    w_assert3(_in_compensated_op);
    // w_assert3(status() == xct_aborting); it's active if in sm::rollback_work
    if ( (! _last_log) || _last_log->is_undoable_clr()) {
	/*
	// force it to write a compensation-only record
	// either because there's no record on which to 
	// piggy-back the compensation, or because the record
	// that's there is an undoable/compensation and will be
	// undone (and we *really* want to compensate around it)
	*/

	W_COERCE(log_compensate(lsn));
    } else {
	_last_log->set_clr(lsn);
    }
    release_1thread_mutex();
}
/*********************************************************************
 *
 *  xct_t::compensate(lsn, bool undoable)
 *
 *  Generate a compensation log record to compensate actions 
 *  started at "lsn" (commit a top level action).
 *  Generates a new log record only if it has to do so.
 *
 *  Special case of undoable compensation records is handled by the
 *  boolean argument.
 *
 *********************************************************************/
void 
xct_t::compensate(const lsn_t& lsn, bool undoable)
{
    DBGX(    << " compensate(" << lsn << ") -- status=" << status());

    // acquire_1thread_mutex(); should already be mine
    w_assert3(_1thread.is_mine());
    if ( !_last_log ) {
	w_assert3((smlevel_1::log == 0) || !undoable);

	// old way:
	// w_assert1(_last_lsn == lsn);   // no log generated before here
	// new way: write a compensation record
	DBG(<<"Writing compensate log record" );
	W_COERCE(log_compensate(lsn));
    } else {
	if(undoable) {
	    _last_log->set_undoable_clr(lsn);
	} else {
	    _last_log->set_clr(lsn);
	}
    }
    stop_crit();
}


#ifdef MULTI_SERVER

/*********************************************************************
 * xct_t::set_dirty(const lpid_t& pid)
 *
 * Have the xact remember that it has dirtied the "pid" page.
 * This is done for remote pages only.
 *
 *********************************************************************/
void
xct_t::set_dirty(const page_p& page, int idx)
{
    // for now...
    w_assert3(page.tag()==page_p::t_file_p || page.tag()==page_p::t_lgdata_p
					   || page.tag()==page_p::t_lgindex_p);

    const lpid_t& pid = page.pid();

    if (page.tag() == page_p::t_file_p && !_EX_locked_files->lookup(pid)) {
	return;
    }

    pid_hash_t* pg = _dirty_pages->lookup(pid);
    if (!pg) {
	pg = new pid_hash_t(pid);
	_dirty_pages->push(pg);
    }
#ifdef OBJECT_CC
    if (idx >= 0 && cc_alg == t_cc_record && page.tag() == page_p::t_file_p) {
	if (pg->recmap != 0 && idx < pg->nslots) {
	    ;
	} else if (pg->recmap != 0) {
	    u_char* newmap = new u_char[(page.nslots()-1) / 8 + 1];
	    memcpy(newmap, pg->recmap, (pg->nslots-1) / 8 + 1);
	    w_assert3(sizeof(pg->recmap) == (pg->nslots-1) / 8 + 1);
	    delete [] pg->recmap;
	    pg->recmap = newmap;
	    pg->nslots = page.nslots();
	} else {
	    u_char* newmap = new u_char[(page.nslots()-1) / 8 + 1];
	    pg->recmap = newmap;
	    pg->nslots = page.nslots();
	}
	bm_set(pg->recmap, idx);
    }
#endif /* OBJECT_CC */

    // TODO: If the dirty_pages table grows too big, then scan the buffer pool
    //       for pages registered in this table but not currently cached.
    //       These pages can be removed from the dirty_pages table.
}
#endif /* MULTI_SERVER */


/*********************************************************************
 *
 *  xct_t::rollback(savept)
 *
 *  Rollback transaction up to "savept".
 *
 *********************************************************************/
rc_t
xct_t::rollback(lsn_t save_pt)
{
    FUNC(xct_t::rollback);
    w_assert3(one_thread_attached());
    w_rc_t	rc;
    logrec_t* 	buf =0;

    // MUST PROTECT anyway, since this generates compensations
    acquire_1thread_mutex();

    w_assert3(_in_compensated_op==0);
    w_assert3(_anchor == lsn_t::null);

    DBGX( << " in compensated op");
    _in_compensated_op++;

    _flush_logbuf();
    lsn_t nxt = _undo_nxt;

    LOGTRACE( << "abort begins at " << nxt);

    // if(!log) { return RC(eNOABORT); }
    if(!log) { 
	ss_m::errlog->clog  <<
	"Cannot roll back with logging turned off. " 
	<< flushl; 
    }

    bool released;

    while (save_pt < nxt)  {
	rc =  log->fetch(nxt, buf);
	// WE HAVE THE LOG_M MUTEX
	released = false;

	logrec_t& r = *buf;
	if(rc==RC(eEOF)) {
	    DBG(<< " fetch returns EOF" );
	    log->release();
	    goto done;
	}
	w_assert3(!r.is_skip());

	if (r.is_undo()) {
	    /*
	     *  Undo action of r.
	     */
	    LOGTRACE( << setiosflags(ios::right) << nxt
		      << resetiosflags(ios::right) << " U: " << r 
		      << " ... " );

#ifdef DEBUG
	    u_int	 bbwd = _log_bytes_bwd;
	    u_int	 bfwd = _log_bytes_fwd;
#endif
	    lpid_t pid = r.pid();
	    page_p page;

	    /*
	     * ok - might have to undo this one -- make a copy
	     * of it and free the log manager for other
	     * threads to use; also have to free it because
	     * during redo we might try to reacquire it and we
	     * can't acquire it twice.
	     * 
	     * Also, we have to release the mutex before we
	     * try to fix a page.
	     */

	    logrec_t 	copy = r;
	    log->release();
	    released = true;

	    if (! copy.is_logical()) {
		rc = page.fix(pid, page_p::t_any_p, LATCH_EX, TMP_NOFLAG);
		if(rc) {
		    goto done;
		}
		w_assert3(page.pid() == pid);
	    }


	    copy.undo(page ? &page : 0);

#ifdef DEBUG
	    bbwd = _log_bytes_bwd - bbwd;
	    bfwd = _log_bytes_fwd - bfwd;
	    if(bbwd  > copy.length()) {
		  LOGTRACE(<< " len=" << copy.length() << " B=" << bbwd );
	    }
	    /*
	    if((bfwd !=0) || (bbwd !=0)) {
		  LOGTRACE( << " undone" << " F=" << bfwd << " B=" << bbwd );
	    }
	    */
#endif
	    if(copy.is_cpsn()) {
		LOGTRACE( << " compensating to" << copy.undo_nxt() );
		nxt = copy.undo_nxt();
	    } else {
		LOGTRACE( << " undoing to" << copy.prev() );
		nxt = copy.prev();
	    }

	} else  if (r.is_cpsn())  {
	    LOGTRACE( << setiosflags(ios::right) << nxt
		      << resetiosflags(ios::right) << " U: " << r 
		      << " compensating to" << r.undo_nxt() );
	    nxt = r.undo_nxt();

	} else {
	    LOGTRACE( << setiosflags(ios::right) << nxt
	      << resetiosflags(ios::right) << " U: " << r 
		      << " skipping to " << r.prev());
	    nxt = r.prev();
	}
	if (! released ) {
	    // not released yet
	    log->release();
	}
    }
    _undo_nxt = nxt;

    /*
     *  The sdesc cache must be cleared, because rollback may
     *  have caused conversions from multi-page stores to single
     *  page stores.
     */
    if(sdesc_cache()) {
	sdesc_cache()->remove_all();
    }

    if (_alloced_page) {
	// this transaction allocated pages, so the abort will
	// free them, invalidating the io managers cache.
	io->invalidate_free_page_cache();
    }

done:

    _flush_logbuf();
    DBGX( << " out compensated op");
    _in_compensated_op --;
    w_assert3(_anchor == lsn_t::null);
    release_1thread_mutex();

    if(save_pt != lsn_t::null) {
	smlevel_0::stats.rollback_savept_cnt++;
    }

    return rc;
}


//
// TODO: protect all access to 
// xct for multiple threads
//


bool			
xct_t::is_local() const 
{
    // is_local means it didn't spread from HERE,
    // that doesn't mean that this isn't a distributed
    // tx.  It returns true for a server thread of
    // a distributed tx.
#ifdef MULTI_SERVER
    return _master_proxy_list.is_empty();
#else
    // stub for now
    return false;
#endif
}

bool			
xct_t::is_distributed() const 
{
#ifdef MULTI_SERVER
    // is_master_proxy() means this is the client 
    // is_local() means it has not spread from here
    // If either one is false, this is a distributed tx 
    return     !is_master_proxy() 
	    || !is_local();
#else
    return false;
#endif
}

#ifndef MULTI_SERVER
bool			
xct_t::is_remote_proxy() const 
{
    return false;
}
#endif

#ifdef MULTI_SERVER
// Functions related to distributed transactions

bool			
xct_t::is_master_proxy() const 
{
    return _master_site == 0 && _callback_proxy == 0;
}

bool			
xct_t::is_remote_proxy() const 
{
    return _master_site != 0;
}

bool			
xct_t::is_cb_proxy() const 
{
    return _callback_proxy != 0;
}

w_list_t<master_xct_proxy_t>& 
xct_t::master_proxy_list() 
{
    return _master_proxy_list;
}

remote_xct_proxy_t*		
xct_t::master_site() const 
{
    return _master_site;
}

void			
xct_t::set_master_site(remote_xct_proxy_t* r) 
{
    _master_site = r;
}

inline srvid_t
xct_t::master_site_id() const
{
    return _master_site->server();
}

callback_xct_proxy_t*	
xct_t::cb_proxy() const 
{
    return _callback_proxy; 
}

void			
xct_t::set_cb_proxy(callback_xct_proxy_t* cb) 
{
    _callback_proxy = cb;
}

callback_op_t*		
xct_t::callback()  const
{ 
    return _callback; 
}

w_hash_t<pid_hash_t, lpid_t>& 
xct_t::dirty_pages() const 
{ 
    return *_dirty_pages; 
}

#endif /* MULTI_SERVER */

void                        
xct_t::set_alloced() 
{
    //TODO: PROTECT
    _alloced_page = true;
}
void                        
xct_t::set_freed() 
{	
    acquire_1thread_mutex();
    _freed_page = true;
    release_1thread_mutex();
}


/*
 *  NB: the idea here is that you cannot change
 *  the timeout for the tx if other threads are 
 *  running in this tx.  By seeing that only one
 *  thread runs in this tx during set_timeout, we
 *  can use the const (non-mutex-acquiring form)
 *  of xct_t::timeout() in the lock manager, and
 *  we don't have to acquire the mutex here either.
 */
void			
xct_t::set_timeout(long t) 
{ 
    // acquire_1thread_mutex();
    w_assert3(one_thread_attached());
    _timeout = t; 
    // release_1thread_mutex();
}


smlevel_0::concurrency_t		
xct_t::get_lock_level()  
{ 
    smlevel_0::concurrency_t l;
    acquire_1thread_mutex();
    l =  _lock_level; 
    release_1thread_mutex();
    return l;
}

void		   	
xct_t::lock_level(concurrency_t l) 
{
    acquire_1thread_mutex();
    w_assert3(l != t_cc_record || cc_alg == t_cc_record);
    _lock_level = l;
    release_1thread_mutex();
}

int 
xct_t::num_threads() 
{
    int result;
    acquire_1thread_mutex();
    result = _threads_attached;
    release_1thread_mutex();
    return result;
}

int 
xct_t::attach_thread() 
{
    me()->new_xct();
    acquire_1thread_mutex();
    if(++_threads_attached > 1) {
	if(_state == xct_prepared) {

	    // special case of checkpoint being
	    // taken for a prepared tx
	    w_assert1(_threads_attached == 2); // at most

	    /////////////////////////////////////////////////
	    // NB: this check is not safe for preemptive threads
	    // w_assert3(smlevel_3::chkpt->taking()==true);
		// and in any case, smlevel_3::chkpt is not known here
	    /////////////////////////////////////////////////

	} else {
#if defined(NOT_PREEMPTIVE) && !defined(MULTI_SERVER)
	    cerr 
		<< "Multi-threaded transactions are not supported."
		<< "Server configured with -DNOT_PREEMPTIVE." << endl;
	    w_assert3( 0 );
#endif
	}
	smlevel_0::stats.mpl_attach_cnt++;
    } else {
	w_assert3( ! _waiters.is_hot());
    }
    release_1thread_mutex();
    return _threads_attached;
}

int 
xct_t::detach_thread() 
{
    acquire_1thread_mutex();
    -- _threads_attached;
    release_1thread_mutex();
    me()->no_xct();
    return _threads_attached;
}

w_rc_t
xct_t::lockblock(long timeout)
{
    acquire_1thread_mutex();
    DBGTHRD(<<"blocking on condn variable");
    w_rc_t rc = _waiters.wait(_1thread, timeout);
    DBGTHRD(<<"not blocked on cond'n variable");
    release_1thread_mutex();
    if(rc) {
	return RC_AUGMENT(rc);
    } else {
	return rc;
    }
}

void
xct_t::lockunblock()
{
    acquire_1thread_mutex();
    DBGTHRD(<<"signalling waiters on cond'n variable");
    _waiters.broadcast();
    DBGTHRD(<<"signalling cond'n variable done");
    release_1thread_mutex();
}

bool
xct_t::one_thread_attached() const
{
    if( _threads_attached > 1) {
	cerr    << "Fatal VAS or SSM error:" << endl
		<< "Only one thread allowed in this operation at any time." << endl
		<< _threads_attached << " threads are attached to xct " << _tid <<endl;
	return false;
    }
    return true;
}

void			
xct_t::assert_1thread_mutex_free() const
{
    if(_1thread.is_locked()) {
	w_assert3( ! _1thread.is_mine());
	DBGX(<<"some (other) thread holds 1thread mutex");
    }
}

void
xct_t::acquire_1thread_mutex()
{
    // should be mine IFF in compensated operation
    if(_1thread.is_mine()) {
    	// w_assert3(_in_compensated_op>0);
	return;
    }
    W_COERCE(_1thread.acquire());
    DBGX(    << " acquired xct mutex");
}

void
xct_t::release_1thread_mutex()
{
    w_assert3(_1thread.is_mine());

    DBGX( << " release xct mutex");
    if( _in_compensated_op==0 ) {
	// If we updated something and we're going
	// to release the mutex, we must flush the
	// log if another thread could possibly want
	// to grab it.  The only reason we don't just
	// automatically _flush_logbuf() in give_logbuf()
	// is that we would just as soon hang onto the 
	// log record if we're in a compensated operation.
	if(_last_mod_thread == me()) {
	    _flush_logbuf();
	}
	_1thread.release();
    } else {
	DBGX( << " in compensated operation: can't release xct mutex");
    }
}

rc_t
xct_t::commit(bool lazy)
{
    // w_assert3(one_thread_attached());
    // removed because a checkpoint could
    // be going on right now.... see comments
    // in log_prepared and chkpt.c

    return _commit(t_regular | (lazy ? t_lazy : t_regular));
}


rc_t
xct_t::chain(bool lazy)
{
    w_assert3(one_thread_attached());
    return _commit(t_chain | (lazy ? t_lazy : t_chain));
}



bool
xct_t::set_lock_cache_enable(bool enable)
{
    //PROTECT
    acquire_1thread_mutex();
    _lock_cache_enable = enable;
    release_1thread_mutex();
    return _lock_cache_enable;
}

sdesc_cache_t*  	
xct_t::new_sdesc_cache_t()
{
    sdesc_cache_t*  	_sdesc_cache = new sdesc_cache_t;
    if (!_sdesc_cache) W_FATAL(eOUTOFMEMORY);
    return _sdesc_cache;
}

void  		
xct_t::delete_sdesc_cache_t(sdesc_cache_t *x)
{
    delete x;
}
    

lockid_t*  	
xct_t::new_lock_hierarchy()
{
    lockid_t*  	l = new lockid_t [lockid_t::NUMLEVELS];
    if (!l) W_FATAL(eOUTOFMEMORY);
    return l;
}
    

ostream &			
xct_t::dump_locks(ostream &out) const
{
    return lock_info()->dump_locks(out);
}

rc_t
xct_lock_info_t::get_lock_totals(
    int			&total_EX,
    int			&total_IX,
    int			&total_SIX,
    int			&total_extent // of type EX, IX, or SIX
) const
{
    const lock_request_t 	*req;
    const lock_head_t 		*lh;
    int				i;

    total_EX=0;
    total_IX=0;
    total_SIX=0;
    total_extent = 0;

    // Start only with t_long locks
    for(i=0; i< lock_base_t::NUM_DURATIONS; i++) {
	w_list_const_i<lock_request_t> iter(list[i]);
	while ((req = iter.next())) {
	    ////////////////////////////////////////////
	    //w_assert3(req->xd == xct());
	    // xct() is null when we're doing this during a checkpoint
	    ////////////////////////////////////////////
	    w_assert3(req->status() == t_granted);
	    lh = req->get_lock_head();
	    if(lh->name.lspace() == lockid_t::t_extent) {
		// keep track of *all* extent locks
		++total_extent;
	    } else if(req->mode == EX) {
		++total_EX;
	    } else if (req->mode == IX) {
		++total_IX;
	    } else if (req->mode == SIX) {
		++total_SIX;
	    }
	}
    }
    return RCOK;
}

/*
 * xct_lock_info_t::get_locks(mode, numslots, space_l, space_m, bool)
 * 
 *  Grab all NON-SH locks: EX, IX, SIX, U (if mode==NL)
 *  or grab all locks of given mode (if mode != NL)
 *  
 *  numslots indicates the sizes of the arrays space_l and space_m,
 *  where the results are written.  Caller allocs/frees space_*.
 *
 *  If bool arg is true, it gets *only* extent locks that are EX,IX, or
 *   SIX; if bool is  false, only the locks of the given mode are 
 *   gathered, regardless of their granularity
 */

#ifndef W_DEBUG
#define numslots /* numslots not used */
#endif

rc_t
xct_lock_info_t::get_locks(
    lock_mode_t		mode,
    int			numslots,
    lockid_t *		space_l,
    lock_mode_t *	space_m,
    bool                extents // == false; true means get only extent locks
) const
#undef numslots
{
    const lock_request_t 	*req;
    const lock_head_t 		*lh=0;
    int				i;

    if(extents) mode = NL;

    // copy the non-share locks  (if mode == NL)
    //       or those with mode "mode" (if mode != NL)
    // count only long-held locks

    int j=0;
    for(i= t_long; i< lock_base_t::NUM_DURATIONS; i++) {
	w_list_const_i<lock_request_t> iter(list[i]);
	while ((req = iter.next())) {
	    ////////////////////////////////////////////
	    // w_assert3(req->xd == xct());
	    // doesn't work during a checkpoint since
	    // xct isn't attached to that thread
	    ////////////////////////////////////////////
	    w_assert3(req->status() == t_granted);

	    if(mode == NL) {
		//
		// order:  NL, IS, IX, SH, SIX, UD, EX
		// If an UD lock hasn't been converted by the
		// time of the prepare, well, it won't! So
		// all we have to consider are the even-valued
		// locks: IX, SIX, EX
		//
		w_assert3( ((IX|SIX|EX)  & 0x1) == 0);
		w_assert3( ((NL|IS|SH|UD)  & 0x1) != 0);

		// keep track of the lock if:
		// extents && it's an extent lock OR
		// !extents && it's an EX,IX, or SIX lock

		bool wantit = false;
		lh = req->get_lock_head();
		if(extents) {
		    if(lh->name.lspace() == lockid_t::t_extent) wantit = true;
		} else {
		    if((req->mode & 0x1) == 0)  wantit = true;
		}
		if(wantit) {
		    space_m[j] = req->mode;
		    space_l[j] = lh->name;
		    j++;
		} 
	    } else {
		if(req->mode == mode) {
		    lh = req->get_lock_head();
		    // don't bother stashing the (known) mode
		    space_l[j++] = lh->name;
		} 
	    }
	}
    }
    w_assert3(numslots == j);
    return RCOK;
}



rc_t			
xct_t::check_lock_totals(int nex, int nix, int nsix, int nextents) const
{
    int	num_EX, num_IX, num_SIX, num_extents;
    W_DO(lock_info()->get_lock_totals( num_EX, num_IX, num_SIX, num_extents));
    if( nex != num_EX || nix != num_IX || nsix != num_SIX) {
#ifdef DEBUG
	lm->dump();
#endif
	w_assert1(nex == num_EX);
	// IX and SIX are the same for this purpose,
	// but whereas what was SH+IX==SIX when the
	// prepare record was written, will be only
	// IX when acquired implicitly as a side effect
	// of acquiring the EX locks.
	w_assert1(nix + nsix == num_IX + num_SIX );
	w_assert1(nextents == num_extents);
    }
    return RCOK;
}

rc_t			
xct_t::obtain_locks(lock_mode_t mode, int num, const lockid_t *locks)
{
    int  i;
    for (i=0; i<num; i++) {
#ifndef MULTI_SERVER
	W_COERCE(lm->lock(locks[i], mode, t_long, WAIT_IMMEDIATE));
#else
	W_COERCE(lm->lock(locks[i], mode, t_long, WAIT_IMMEDIATE, TRUE));
#endif
    }
    return RCOK;
}

rc_t			
xct_t::obtain_one_lock(lock_mode_t mode, const lockid_t &lock)
{
#ifndef MULTI_SERVER
    W_COERCE(lm->lock(lock, mode, t_long, WAIT_IMMEDIATE));
#else
    W_COERCE(lm->lock(lock, mode, t_long, WAIT_IMMEDIATE, TRUE));
#endif
    return RCOK;
}

ostream &			
xct_lock_info_t::dump_locks(ostream &out) const
{
    const lock_request_t 	*req;
    const lock_head_t 		*lh;
    int						i;
    for(i=0; i< lock_base_t::NUM_DURATIONS; i++) {
	out << "***Duration " << i <<endl;

	w_list_const_i<lock_request_t> iter(list[i]);
	while ((req = iter.next())) {
	    w_assert3(req->xd == xct());
	    lh = req->get_lock_head();
	    out << "Lock: " << lh->name 
		<< " Mode: " << req->mode 
		<< " State: " << req->status() <<endl;
	}
    }
    return out;
}

#ifdef MULTI_SERVER 
void			
xct_t::EX_locked_files_insert(const lockid_t& id)
{
    w_assert3(comm_m::use_comm());
    // PROTECT
    // (could cause you to block in the lock manager)
    acquire_1thread_mutex();
    _EX_locked_files->insert(id);
    release_1thread_mutex();
}
#endif /* MULTI_SERVER */


/*<std-header orig-src='shore'>

 $Id: log.cpp,v 1.124 1999/08/06 19:53:45 bolo Exp $

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
#define LOG_C
#ifdef __GNUG__
#   pragma implementation
#endif

#include <sm_int_1.h>
#include <srv_log.h>
#include "logdef_gen.cpp"
#include "crash.h"


rc_t	log_m::new_log_m(log_m	*&the_log,
			 const char *path,
			 fileoff_t max_logsz,
			 int rdbufsize,
			 int wrbufsize,
			 char  *shmbase,
			 bool  reformat)
{
	log_m	*log;

	log = new log_m(path, max_logsz, rdbufsize, wrbufsize,
			shmbase, reformat);
	if (!log)
		return RC(fcOUTOFMEMORY);

	the_log = log;
	return RCOK;
}


NORET
log_m::log_m(
	const char *path,
	fileoff_t max_logsz,
	int   rdbufsize,
	int   wrbufsize,
	char  *shmbase,
	bool  reformat
    ) : 
    log_base(shmbase),
   _mutex("log_m"),
   _peer(0),
   _countdown(0), 
   _countdown_expired(0)
{
    /*
     *  make sure logging parameters are something reasonable
     */
    w_assert1(smlevel_0::max_openlog > 0 &&
	      (max_logsz == 0 || max_logsz >= 64*1024 &&
	      smlevel_0::chkpt_displacement >= 64*1024));



    _shared->_curr_lsn =
    _shared->_append_lsn =
    _shared->_durable_lsn = // set_durable(...)
    _shared->_master_lsn =
    _shared->_old_master_lsn = lsn_t::null;

    lsn_t	starting = srv_log::first_lsn(1);
    _shared->_min_chkpt_rec_lsn = starting;

    DBGTHRD(<< "_shared->_min_chkpt_rec_lsn = " 
	<<  _shared->_min_chkpt_rec_lsn);

    _shared->_space_available = 0;
    _shared->_max_logsz = max_logsz; // max size of a partition

    skip_log *s = new skip_log;
    _shared->_maxLogDataSize = max_logsz - s->length();
    delete s;

    /*
     * mimic argv[]
     */
    char arg1[100];
  // TODO remove  char arg2[100];

    ostrstream s1(arg1, sizeof(arg1));
    s1 << path << '\0';

/*
    ostrstream s2(arg2, sizeof(arg2));
    s2 << _shmem_seg.id() << '\0';
*/

    DBGTHRD(<< "_shared->_min_chkpt_rec_lsn = " 
	<<  _shared->_min_chkpt_rec_lsn);

    //_peer = srv_log::new_log_m(s1.str(), s2.str(), reformat);
    w_rc_t	e;
    srv_log	*slog;
    e = srv_log::new_log_m(slog, s1.str(),  
			   rdbufsize, wrbufsize, shmbase,
			   reformat);

    if (e == RCOK)
	    _peer = slog;
    
    DBGTHRD(<< "_shared->_min_chkpt_rec_lsn = " 
	<<  _shared->_min_chkpt_rec_lsn << "\0" );

    W_COERCE(e);
    w_assert3(curr_lsn() != lsn_t::null);
}

NORET
log_m::~log_m() 
{
    // TODO: replace with RPC
    if(_peer) {
	delete _peer;
	_peer = 0;
    }
}

/*********************************************************************
 *
 *  log_m::wait(nbytes, sem, timeout)
 *
 *  Block current thread on sem until nbytes have been written 
 *  (in which case log_m will signal sem) or timeout expires.
 *  Other threads might also signal sem.
 *
 *********************************************************************/
rc_t
log_m::wait(fileoff_t nbytes, sevsem_t& sem, timeout_in_ms timeout)
{
    // checkpoint could have been taken by explicit command
    // (e.g. sm checkpoint) before the countdown expired,
    // in which case _countdown_expired is non-null.  We assert
    // here that there is only *one* event semaphore used for 
    // this purpose.

    w_assert3(_countdown_expired == 0
	|| _countdown_expired == &sem);

    _countdown = nbytes;
    _countdown_expired = &sem;
    return _countdown_expired->wait(timeout);
}

/**********************************************************************
 * RPC log functions:
 **********************************************************************/

inline void
log_m::release_var_mutex()
{
#ifndef NOT_PREEMPTIVE
    _var_mutex.release();
#endif /*NOT_PREEMPTIVE*/
}

void                
log_m::acquire_var_mutex()
{
#ifndef NOT_PREEMPTIVE
    if(_var_mutex.is_locked() && !_var_mutex.is_mine()) {
	INC_TSTAT(await_log_monitor_var);
    }
    W_COERCE(_var_mutex.acquire());
#endif
}

void                
log_m::acquire_mutex()
{
    if(_mutex.is_locked() && !_mutex.is_mine()) {
	INC_TSTAT(await_log_monitor);
    }
    W_COERCE(_mutex.acquire());
}

void                
log_m::set_master(
    const lsn_t&                lsn, 
    const lsn_t&                min_rec_lsn,
    const lsn_t&                min_xct_lsn)
{
    w_assert1(_peer);
    acquire_mutex();
    _peer->set_master(lsn, min_rec_lsn, min_xct_lsn);
    _mutex.release();
}

rc_t                
log_m::insert(logrec_t& r, lsn_t* ret)
{
    w_assert1(_peer);

    w_assert1(smlevel_0::operating_mode != smlevel_0::t_in_analysis);

    FUNC(log_m::insert)
    if (r.length() > sizeof(r)) {
	// the log record is longer than a log record!
	W_FATAL(fcINTERNAL);
    }
    DBGTHRD( << 
	"Insert tx." << r 
	<< " size: " << r.length() << " prevlsn: " << r.prev() );
 
    acquire_var_mutex();
    if (_countdown_expired && _countdown)  {
	/*
	 *  some thread asked for countdown
	 */
	if (_countdown > (int) r.length()) {
	    _countdown -= r.length();
	} else  {
	    W_IGNORE( _countdown_expired->post() );
	    _countdown_expired = 0;
	    _countdown = 0;
	    release_var_mutex();
	    me()->yield();
	    acquire_var_mutex();
	}
    }
    release_var_mutex();

    acquire_mutex();
    rc_t rc = _peer->insert(r, ret);
    // RC could be eOUTOFLOGSPACE
    _mutex.release();

    if(!rc) {
	acquire_var_mutex();

	if(xct()) {
	    INC_TSTAT(xlog_records_generated);
	    ADD_TSTAT(xlog_bytes_generated, r.length());
#ifdef W_TRACE
	    base_stat_t tmp = GET_STAT(log_bytes_active);
	    tmp += r.length();
	    DBGTHRD(<<"log_bytes_active is now " << tmp
		<< " after adding " << r.length()
		<< " for xct " << xct()->tid()
		);
#endif

	    ADD_STAT(log_bytes_active, r.length());

	} else {
	    INC_STAT(log_records_generated);
	    ADD_STAT(log_bytes_generated, r.length());
	}
	release_var_mutex();
    }
    return rc;
}


rc_t                
log_m::compensate(const lsn_t& rec, const lsn_t& undo)
{
    w_assert1(_peer);

    w_assert1(smlevel_0::operating_mode != smlevel_0::t_in_analysis);

    FUNC(log_m::compensate)
    DBGTHRD( << 
	"Compensate in record #" << rec 
	<< " to: " << undo );
 
    acquire_mutex();
    rc_t rc = _peer->compensate(rec, undo);
    _mutex.release();

    return rc;
}

rc_t                        
log_m::fetch(
    lsn_t&                      lsn,
    logrec_t*&                  rec,
    lsn_t*                      nxt)
{
    FUNC(log_m::fetch);
    acquire_mutex();
    w_assert1(_peer);
    rc_t rc = _peer->fetch(lsn, rec, nxt);
    // has to be released explicitly
    // _mutex.release();
    return rc;
}

void                        
log_m::release()
{
    FUNC(log_m::release);
    _mutex.release();
}

rc_t                
log_m::flush(const lsn_t& lsn)
{
    acquire_var_mutex();

    w_assert1(_peer);
    FUNC(log_m::flush);
    DBGTHRD(<<" flushing to lsn " << lsn);
    rc_t rc;

    if(lsn >= _shared->_durable_lsn) {
	INC_STAT(log_sync_cnt);
	release_var_mutex();
	acquire_mutex();
	rc =  _peer->flush(lsn);
	_mutex.release();
    } else {
	INC_STAT(log_dup_sync_cnt);
	release_var_mutex();
    }
    return rc;
}

rc_t                
log_m::scavenge(
    const lsn_t&                min_rec_lsn,
    const lsn_t&                min_xct_lsn)
{
    w_assert1(_peer);
    acquire_mutex();
    rc_t rc =  _peer->scavenge(min_rec_lsn, min_xct_lsn);
    _mutex.release();
    return rc;
}
/*********************************************************************
 *
 *  log_m::shm_needed(int n)
 *
 *  Return the amount of shared memory needed (in bytes)
 *  for the given value of the sm_logbufsize option
 *
 *  This *should* be a function of the kind of log
 *  we're going to construct, but since this is called long before
 *  we've discovered what kind of log it will be, we cannot manager that.
 *
 *********************************************************************/
int
log_m::shm_needed(int n)
{
    return (int) (n * 2) + CHKPT_META_BUF;
}

log_m::fileoff_t log_m::limit() const
{
	/* XXX this is a truly disgusting cast.  The log stuff just
	   isn't put together well enough to do it right. */
	return ((srv_log*)_peer)->limit();
}

void 		        
log_base::compute_space() { }
void 		        
log_m::compute_space()
{
   _peer->compute_space();
}

void 		        
log_base::check_wal(const lsn_t &/*ll*/) { }

void 		        
log_m::check_wal(const lsn_t &ll) 
{
   _peer->check_wal(ll);
}


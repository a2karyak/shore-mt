#define TRYTHIS
#define TRYTHIS2 0x2
#define TRYTHIS3 0x4
#define TRYTHIS4 0x8
#define TRYTHIS5 0x10
#define TRYTHIS6 0x1

/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

/*
 *  $Id: bf.c,v 1.153 1996/07/01 22:07:11 nhall Exp $
 */

#define SM_SOURCE
#define BF_C

#ifdef __GNUG__
#pragma implementation "bf.h"
#endif

#include <sm_int.h>
#include "bf_core.h"
#ifdef MULTI_SERVER
#include <auto_release.h>
#include "callback.h"
#include "lock_remote.h"
#include "remote_client.h"
#endif

#ifdef __GNUC__
template class w_auto_delete_array_t<lpid_t>;

#if defined(OBJECT_CC) && defined(MULTI_SERVER)
template class w_list_t<cb_race_t>;
template class w_list_i<cb_race_t>;
template class w_hash_t<cb_race_t, rid_t>;
#endif

#endif

#define DBGTHRD(arg) DBG(<<" th."<<me()->id << " " arg)

int bf_m::_dirty_threshhold = 20; // changed in constructor for bf_m
int bf_m::_ndirty = 0;
int bf_m::_strategy = 0;


/*********************************************************************
 *
 *  cmp_lpid(x, y)
 *
 *  Used for qsort(). Compare two lpids x and y ... disregard
 *  store id in comparison.
 *
 *********************************************************************/
static int
cmp_lpid(const void* x, const void* y)
{
    register const lpid_t* p1 = (lpid_t*) x;
    register const lpid_t* p2 = (lpid_t*) y;
    if (p1->vol() - p2->vol())
        return p1->vol() - p2->vol();
#ifdef SOLARIS2
    return (p1->page > p2->page ? 1 :
            p2->page > p1->page ? -1 :
            0);
#else
    return (p1->page > p2->page ? 1 :
            p1->page < p2->page ? -1 :
            0);
#endif
}


/*********************************************************************
 *
 *  class bf_filter_t
 *
 *  Abstract base class 
 *********************************************************************/
class bf_filter_t : public smlevel_0 {
public:
    virtual bool                is_good(const bfcb_t&) const = 0;
};


/*********************************************************************
 *  
 *  Filters for use with bf_m::_scan()
 *
 *********************************************************************/
class bf_filter_store_t : public bf_filter_t {
public:
    NORET                       bf_filter_store_t(const stid_t& stid);
    bool                        is_good(const bfcb_t& p) const;
private:
    stid_t                      _stid;
};

class bf_filter_vol_t : public bf_filter_t {
public:
    NORET                       bf_filter_vol_t(const vid_t&);
    bool                        is_good(const bfcb_t& p) const;
private:
    vid_t                       _vid;
};

class bf_filter_none_t : public bf_filter_t {
public:
    NORET			bf_filter_none_t();
    bool                        is_good(const bfcb_t& p) const;
};

class bf_filter_lsn_t : public bf_filter_t {
public:
    NORET			bf_filter_lsn_t(const lsn_t& lsn);
    bool                        is_good(const bfcb_t& p) const;
private:
    lsn_t			_lsn;
};

class bf_filter_sweep_t : public bf_filter_t {
public:
    NORET			bf_filter_sweep_t( int sweep );
    bool                        is_good(const bfcb_t& p) const;
private:
    enum 			{
		_threshhold = 8
    };
    int				_sweep;
};


/*********************************************************************
 *
 *  class bf_cleaner_thread_t
 *
 *  Thread that flushes multiple dirty buffer pool pages in 
 *  sorted order. See run() method for more on algorithm.
 *  The thread runs at highest (t_time_critical) priority, but
 *  since it would be doing lots of I/O, it would yield very
 *  frequently.e
 *
 *  Once started, the thread goes into an infinite loop that
 *  	1. wait for an activate signal
 *  	2. get all dirty pids in the buffer pool
 *	3. sort the dirty pids
 *	4. call bf_m::_clean_buf with the pids
 *	5. goto 1
 *
 *********************************************************************/

class bf_cleaner_thread_t : public smthread_t {
public:
    NORET			bf_cleaner_thread_t();
    NORET			~bf_cleaner_thread_t()  {};

    void			activate();
    void			retire();
    virtual void		run();
private:
    bool			_is_running;
    bool			_retire;
    smutex_t			_mutex;
    scond_t			_activate;

    // disabled
    NORET			bf_cleaner_thread_t(
	const bf_cleaner_thread_t&);
    bf_cleaner_thread_t&	operator=(const bf_cleaner_thread_t&);
};


/*********************************************************************
 *
 *  bf_cleaner_thread_t::bf_cleaner_thread_t()
 *
 *********************************************************************/
NORET
bf_cleaner_thread_t::bf_cleaner_thread_t()
    : smthread_t(t_time_critical, false, false, "bf_cleaner"),
      _is_running(false),
      _retire(false),
      _mutex("bf_cleaner"),
      _activate("bf_cleaner")
{
    _is_running = true;
}


/*********************************************************************
 *
 *  bf_cleaner_thread_t::activate()
 *
 *  Signal the cleaner thread to wake up and do work.
 *
 *********************************************************************/
void
bf_cleaner_thread_t::activate()
{
    _activate.signal();
}


/*********************************************************************
 *
 *  bf_cleaner_thread_t::retire()
 *
 *  Signal the cleaner thread to wake up and exit
 *
 *********************************************************************/
void
bf_cleaner_thread_t::retire()
{
    _retire = true;
    while (_is_running)  {
        activate();
        rc_t rc = wait(1000);
        if (rc)  {
            if (rc.err_num() == stTIMEOUT)
                continue;
            W_COERCE(rc);
        }
    }
}


/*********************************************************************
 *
 *  bf_m::npages()
 *
 *  Return the size of the buffer pool in pages.
 *
 *********************************************************************/
inline int
bf_m::npages()
{
    return _core->_num_bufs;
}


/*********************************************************************
 *
 *  bf_cleaner_thread_t::run()
 *
 *  Body of cleaner thread. Repeatedly wait for wakeup signal,
 *  get array of dirty pages from bf, and call bf_m::_clean_buf()
 *  to flush the pages.
 *
 *********************************************************************/
void
bf_cleaner_thread_t::run()
{
    FUNC(bf_cleaner_thread_t::run);
    if (_retire)  {
        _is_running = false;
        return;
    }
    lpid_t* pids = new lpid_t[bf_m::npages()];
    if (! pids)  {
        DBG( << " cleaner " << id << " died ... insufficient memory" << endl );
        _is_running = false;
        return;
    }
    w_auto_delete_array_t<lpid_t> auto_del(pids);

#ifdef DEBUG
    // for use with DBG()
    sthread_base_t::id_t id = me()->id;
#endif
    DBG( << " cleaner " << id << " activated" << endl );

    int ntimes = 0;

    for ( ;; )  {
	bf_m::_incr_sweeps();
	bf_m:: _ndirty = 0;

        /*
         *  Wait for wakeup signal
         */
        if (_retire) break;
	W_COERCE(_mutex.acquire());
        W_IGNORE( _activate.wait(_mutex, 30000/*30sec timeout*/) );
	_mutex.release();
        if (_retire) break;

        /*
         * Sync the log in case any lazy transactions are pending.
         * Plus this is more efficient then small syncs when
         * pages are flushed below.
         */
        if (smlevel_0::log) { // log manager is running
            DBG(<< "flushing log");
	    bf_m::_incr_log_flush_all();
            W_IGNORE(smlevel_0::log->flush_all());
        }

	/*
	 *  Get list of dirty pids
	 */
	int count = 0;
	{
	    bf_filter_sweep_t filter(ntimes);
	    for (int i = 0; i < bf_core_m::_num_bufs; i++)  {

		/* 
		 * use the refbit as an indicator of how hot it is
		 */
		if ((bf_core_m::_buftab[i].hot = bf_core_m::_buftab[i].refbit))
			bf_core_m::_buftab[i].hot--;

		if ( filter.is_good(bf_core_m::_buftab[i])) {
		    if (! bf_core_m::_buftab[i].pid.is_remote()) {
			pids[count++] = bf_core_m::_buftab[i].pid;
		    }
		}
	    }
	}
        if (_retire)  break;

        ++ntimes;
        W_COERCE( bf_m::_clean_buf(8, count, pids, 0, &_retire) );
    }
    DBG( << " cleaner " << id << " retired" << endl
         << "\tswept " << ntimes << " times " << endl );
    DBG( << endl );
    _is_running = false;
}


/*********************************************************************
 *
 *  bf_m class static variables
 *
 *	_cleaner_thread : background thread to write dirty bf pages 
 *
 *	_core		: bf core essentially rsrc_m that manages bfcb_t
 *
 *********************************************************************/
bf_cleaner_thread_t*		bf_m::_cleaner_thread = 0;
bf_core_m*		 	bf_m::_core = 0;

#if defined(OBJECT_CC) && defined(MULTI_SERVER)
w_hash_t<cb_race_t, rid_t>*	bf_m::race_tab = 0;
#endif


/*********************************************************************
 *
 *  bf_m::bf_m(max, extra, stratgy)
 *
 *  Constructor. Allocates shared memory and internal structures to
 *  manage "max" number of frames.
 *
 *  stratgy is a mask
 *
 *********************************************************************/
bf_m::bf_m(uint4 max, char *bp, uint4 stratgy)
{
#ifdef DEBUG
#define BF_M_NAME "bf"
#else
#define BF_M_NAME 0
#endif

    _core = new bf_core_m(max, bp, stratgy, BF_M_NAME);
    if (! _core) W_FATAL(eOUTOFMEMORY);

#if defined(OBJECT_CC) && defined(MULTI_SERVER)
    race_tab = new w_hash_t<cb_race_t, rid_t>(97, offsetof(cb_race_t, rid),
                                                  offsetof(cb_race_t, link));
    if (! race_tab) W_FATAL(eOUTOFMEMORY);
#endif

    W_COERCE( disable_background_flushing() );
    W_COERCE( enable_background_flushing() );

    _strategy = stratgy;
    _dirty_threshhold = npages()/4;

}


/*********************************************************************
 *
 *  bf_m::~bf_m()
 *
 *  Clean up
 *
 *********************************************************************/
bf_m::~bf_m()
{
    W_COERCE( disable_background_flushing() );

    /*
     *  clean up frames
     */
    if (_core)  {
        W_COERCE( (shutdown_clean ? force_all(TRUE) : discard_all()) );
        delete _core;
        _core = 0;
    }
}


/*********************************************************************
 *
 *  bf_m::shm_needed(int npages)
 *
 *  Return the amount of shared memory needed (in bytes)
 *  for the given number of pages.
 *
 *********************************************************************/
int
bf_m::shm_needed(int n)
{
    return (int) sizeof(page_s) * n;
}

/*********************************************************************
 *
 *  bf_m::total_fix()
 *
 *********************************************************************/
inline int
bf_m::total_fix()
{
    return _core->_total_fix;
}


inline void bf_m::mutex_acquire()
{
    MUTEX_ACQUIRE(_core->_mutex);
}


inline void bf_m::mutex_release()
{
    MUTEX_RELEASE(_core->_mutex);
}


/*********************************************************************
 *
 *  bf_m::get_cb(page)
 *
 *  Given a frame, compute and return a pointer to its
 *  buffer control block.
 *
 *********************************************************************/
inline bfcb_t* bf_m::get_cb(const page_s* p)
{
    int idx = p - bf_core_m::_bufpool;
    return (idx<0 || idx>=bf_core_m::_num_bufs) ? 0 : bf_core_m::_buftab + idx;
}


/*********************************************************************
 *
 *  bf_m::is_bf_page(const page_s* p)
 *
 *********************************************************************/
bool
bf_m::is_bf_page(const page_s* p)
{
    return (p - _core->_bufpool >= 0) && (p - _core->_bufpool < _core->_num_bufs
);
}


/*********************************************************************
 *
 *  bf_m::has_frame(pid)
 *
 *  Return TRUE if the "pid" page has a bf frame assigned to it, i.e.
 *  the page is either cached or in-transit-in.
 *  FALSE otherwise.
 * 
 *  Used for peer servers (callback.c, remote.c)
 *
 *********************************************************************/
bool
bf_m::has_frame(const lpid_t& pid)
{
    bfcb_t* b;
    return _core->has_frame(pid, b);
}


/*********************************************************************
 * bf_m::is_cached(b)
 **********************************************************************/
bool
bf_m::is_cached(const bfcb_t* b)
{
    return _core->_in_htab(b);
}


#if defined(OBJECT_CC) && defined(MULTI_SERVER)

/*********************************************************************
 * bf_m::is_partially_cached(page)
 **********************************************************************/
bool
bf_m::is_partially_cached(const page_s* page)
{
    bfcb_t* b = get_cb(page);

#ifndef NOT_PREEMPTIVE
    W_COERCE(b->recmap_mutex.acquire());
    auto_release_t<smutex_t> dummy(b->recmap_mutex);
#endif

    for (int i = 0; i < b->frame->nslots; i++) {
        if (page->slot[-i].offset >= 0 && bm_is_clr(b->recmap, i)) return TRUE;
    }

    if (bm_is_clr(b->recmap, smlevel_0::max_recs-1)) return TRUE;

    return FALSE;
}
#endif /* OBJECT_CC && MULTI_SERVER */


/*********************************************************************
 *
 *  bf_m::fix(ret_page, pid, mode, no_read, ignore_store_id)
 *
 *  Fix a frame for "pid" in buffer pool in latch "mode". "No_read"
 *  indicates whether bf_m should read the page from disk if it is
 *  not cached. "Ignore_store_id" indicates whether the store ID
 *  on the page can be trusted to match pid.store; usually it can,
 *  but if not, then passing in true avoids an extra assert check.
 *  The frame is returned in "ret_page".
 *
 *********************************************************************/
rc_t
bf_m::fix(
    page_s*&            ret_page,
    const lpid_t&       pid,
    uint2               tag,            // page_t::tag_t
    latch_mode_t        mode,
    bool                no_read,
    bool                ignore_store_id)
{
    FUNC(bf_m::fix);
    DBGTHRD( << "about to fix " << pid );
    ret_page = 0;

    bool 	found, is_new;
    bfcb_t* 	b;
    rc_t	rc;

#ifdef TRYTHIS
#ifdef NOTDEF
    if(_strategy & TRYTHIS2) 
#endif
    {
	rc = _core->find(b, pid, mode);
	if(!rc) {
	    // latch already acquired
	    goto success;
	}
	// if no clean pages, await a clean one
	// to grab -- try this only once
	if(_ndirty == npages()) {
	    if(_cleaner_thread) {
		_incr_await_clean();
		_cleaner_thread->activate();
		me()->yield();
	    }
	}
    }
#endif

#ifdef MULTI_SERVER
    if (pid.is_remote()) {
        w_assert3(tag == page_p::t_btree_p || tag == page_p::t_file_p ||
                  tag == page_p::t_lgdata_p || tag == page_p::t_lgindex_p);
        if (tag != page_p::t_file_p && mode == LATCH_EX)
            W_FATAL(eNOTIMPLEMENTED);
    }
#endif

    rc = _core->grab(b, pid, found, is_new, mode, WAIT_FOREVER);
    if (rc) {
#ifdef MULTI_SERVER
        if (rc.err_num() == eINTRANSIT) {
            w_assert3(!found && pid.is_remote());

            rc_t rc = get_page(pid, b, tag, no_read, ignore_store_id,
                                                        TRUE, TRUE, is_new);
            if (rc) {
                _core->publish(b, rc.is_error());       // publish with error
                return rc.reset();
            }

            w_assert3(((lpid_t)b->pid) == pid);
            w_assert3(b->dirty == false);
            w_assert3(b->rec_lsn == lsn_t::null);

            _core->publish(b); // successful
            goto success;
        } else {
#endif
            return rc.reset();
#ifdef MULTI_SERVER
        }
#endif
    }

    w_assert1(b);

    if (found /* && (pid == b->frame->pid)*/)  {
        /*
         * Page was found.
         * Note: The bf_core_m uses the bfpid_t class derived from lpid_t.
         *       So, it does not check if pid.stid == b->pid.stid. This
         *       is why we check for (pid == b->frame->pid) in the if above.
	 * neh: took out the extra check. A page is relative to a volume,
	 *      so checking volume id + page id is enough.
         */
        DBG( << "found page " << pid );
        /*
         *  set store flag if volume is local (otherwise, server set it)
         */
        if (no_read && !pid.is_remote()) {
            W_DO( io->get_store_flags(pid.stid(), b->frame->store_flags));
        }
        b->pid = pid;

    } else if (found) {

        w_assert3(!b->old_pid_valid && !is_new);

        if (b->dirty)  {
            if (_cleaner_thread)
                _cleaner_thread->activate(); // wake up cleaner thread
            W_COERCE(_replace_out(b));
        } else {
	   _incr_replaced(false);
	}

        // set store flag if volume is local (otherwise, server set it)
        if (!pid.is_remote()) {
            W_DO(io->get_store_flags(pid.stid(), b->frame->store_flags));
        }

        b->pid = pid;
        w_assert3(b->dirty == false);           // dirty flag and rec_lsn are
        w_assert3(b->rec_lsn == lsn_t::null);   // cleared inside ::_replace_out

    } else {
        rc_t rc;


#ifdef DEBUG
        DBG( << "not found " << pid );
        if (!is_new) {
            if (b->old_pid.is_remote()) {
                TRACE(352, "remote page replaced: pid: " << b->old_pid <<
                    " new page: " <<pid<<(b->dirty ? "victim is dirty" : ""));
            } else {
                TRACE(353, "local page replaced: pid: " << b->old_pid <<
                    " new page: " <<pid<<(b->dirty ? "victim is dirty" : ""));
            }
        }
        if (b->old_pid_valid)   w_assert3(!is_new);
        if (!is_new)            w_assert3(b->old_pid_valid);
#endif

#ifdef MULTI_SERVER
        if (!is_new && b->old_pid.is_remote())
            W_COERCE(cbm->purge_page(b->old_pid));
#endif /* MULTI_SERVER */

        if (! is_new)  {
            /*
             *  b is a replacement. Clean up and publish_partial()
             *  to inform bf_core_m that the old page has been flushed out.
             */
            if (b->dirty)  {
                if (_cleaner_thread)
                    _cleaner_thread->activate(); // wake up cleaner thread
                W_COERCE(_replace_out(b));
            } else {
	       _incr_replaced(false);
	    }
            _core->publish_partial(b);
        }

        /*
         *  Read the page and update store_flags. If error occurs,
         *  call _core->publish( .. error) with error flag to inform
         *  bf_core_m that the frame is bad.
         */
        if (rc = get_page(pid, b, tag, no_read, ignore_store_id,
                                                FALSE, TRUE, is_new)) {
            _core->publish(b, rc.is_error());
            return rc.reset();
        }
        DBG( << "got " << pid );

        // set store flag if volume is local (otherwise, server set it)
        if (!pid.is_remote()) {
            if (rc = io->get_store_flags(pid.stid(), b->frame->store_flags)) {
                _core->publish(b, rc.is_error());
                return rc.reset();
            }
        }

        b->pid = pid;
        w_assert3(b->dirty == false);           // dirty flag and rec_lsn are
        w_assert3(b->rec_lsn == lsn_t::null);   // cleared inside ::_replace_out

        _core->publish(b);              // successful
    }

#if defined(TRYTHIS) || defined(MULTI_SERVER)
success:
#endif

    if (log && 
	mode == LATCH_EX && 
	b->rec_lsn == lsn_t::null &&
	(!pid.is_remote())) {

        // Intent to modify. Page would be dirty later than this lsn.
        b->rec_lsn = log->curr_lsn();
    }

    _core->_total_fix++;
    _incr_fixes();
    ret_page = b->frame;
    return RCOK;
}


#if defined(MULTI_SERVER) && defined(OBJECT_CC)

/*********************************************************************
 *
 * bf_m::fix_record(page, rid, mode)
 *
 *********************************************************************/
rc_t
bf_m::fix_record(
    page_s*&            ret_page,
    const rid_t&        rid,
    latch_mode_t        mode)
{
    ret_page = 0;
    bool found, is_new;
    bfcb_t* b;
    const lpid_t& pid = rid.pid;
    uint2 tag = page_p::t_file_p;

    w_assert1(rid.slot < smlevel_0::max_recs-1);
    w_assert3(pid.is_remote());

    /*
     * Pin a frame for the page without latching it
     */
    rc_t rc = _core->grab(b, pid, found, is_new, LATCH_NL, WAIT_FOREVER);

    if (rc && rc.err_num() != eINTRANSIT) return rc.reset();
    w_assert1(b);

    if (found) {
        w_assert3(!rc);
        w_assert3(pid == b->frame->pid);
        /*
         * Page is found in buffer pool (in hash tab). Now look for the record.
         */
        MUTEX_ACQUIRE(b->recmap_mutex);

        if (bm_is_set(b->recmap, rid.slot)) {
            /*
             * The record is available. Get the latch on the page and return.
             * Notice that releasing the recmap_mutex before asking for the
             * latch is safe because the "rid" record is already locked, so
             * its availability cannot change.
             */
            MUTEX_RELEASE(b->recmap_mutex);
            W_COERCE(b->latch.acquire(mode));
            goto success;

        } else {
            /*
             * The record is unavailable, so read the page from the server.
             * Note that the page should be already registered in the server's
             * copy table, so we say to the server not to look for read-purge
             * race, and as a result, we don't need to mark this as a parallel
             * read, even if it is.
             */
            MUTEX_RELEASE(b->recmap_mutex);
            rc_t rc = get_page(rid, b, tag, FALSE, FALSE, FALSE, FALSE,is_new);

	    if (rc) {
		_core->publish(b, rc.is_error());
		return rc.reset();
	    }
            W_COERCE(b->latch.acquire(mode));
            goto success;
        }

    } else {
        /*
         * Page is not found.
         */
        if (rc) {
            w_assert3(rc.err_num() == eINTRANSIT);
            /*
             * The page is in-transit-in.
             * Send a read req to the server marking it as a parallel read
             * and asking for the read-purge race to be checked.
             */
            rc_t rc = get_page(rid, b, tag, FALSE, FALSE, TRUE, TRUE, is_new);

            if (rc) {
                _core->publish(b, rc.is_error());
                return rc.reset();
            }
            W_COERCE(b->latch.acquire(mode));
            _core->publish(b); // successful
            goto success;

        } else {

#ifdef DEBUG
            if (!is_new) {
                if (b->old_pid.is_remote()) {
                    TRACE(352, "remote page replaced: pid: " << b->old_pid <<
                        " new page: "<<pid<<(b->dirty ? "victim is dirty":""));
                } else {
                    TRACE(353, "local page replaced: pid: " << b->old_pid <<
                        " new page: "<<pid<<(b->dirty ? "victim is dirty":""));
                }
            }
            if (b->old_pid_valid)   w_assert3(!is_new);
            if (!is_new)  w_assert3(b->old_pid_valid);
#endif

            if (!is_new && b->old_pid.is_remote())
                W_COERCE(cbm->purge_page(b->old_pid));

            if (! is_new)  {
                if (b->dirty)  {
                    if (_cleaner_thread) _cleaner_thread->activate();
                    W_COERCE(_replace_out(b));
                } else {
		   _incr_replaced(false);
		}
                _core->publish_partial(b);
            }

            rc_t rc = get_page(rid, b, tag, FALSE, FALSE, FALSE, TRUE, is_new);

            if (rc) {
                _core->publish(b, rc.is_error());
                return rc.reset();
            }
            w_assert3(((lpid_t) b->pid) == pid);
            w_assert3(b->dirty == false);        // dirty flag and rec_lsn are
            w_assert3(b->rec_lsn == lsn_t::null); // cleared inside _replace_out

            W_COERCE(b->latch.acquire(mode));
            _core->publish(b);
        }
    }

success:
    _core->_total_fix++;
    _incr_fixes();
    ret_page = b->frame;
    return RCOK;
}
#endif /* MULTI_SERVER  && OBJECT_CC */


/*********************************************************************
 *
 *  bf_m::fix_if_cached(ret_page, pid, mode)
 *
 *  If a frame for "pid" exists in the buffer pool, then fix the
 *  frame in "mode" and return a pointer to it in "ret_page".
 *  Otherwise, return eNOTFOUND error.
 *
 *********************************************************************/
rc_t
bf_m::fix_if_cached(
    page_s*&            ret_page,
    const lpid_t&       pid,
    latch_mode_t        mode)
{
    ret_page = 0;
    bfcb_t* b;
    // Note: no "refbit" argument is given to find() below; so, the refbit of
    //       the frame found (if any) will not change. However, the refbit of
    //       the frame will most probably be set during unfix() because the
    //       default value for the "refbit" argument of unfix() is 1.
    W_DO( _core->find(b, pid, mode) );

    w_assert3(pid == b->frame->pid);

    if (log && mode == LATCH_EX && b->rec_lsn == lsn_t::null &&
                                        (! pid.is_remote()))  {
        /*
         *  intent to modify
         *  page would be dirty later than this lsn
         */
        b->rec_lsn = log->curr_lsn();
    }

    _core->_total_fix++;
    _incr_fixes();
    ret_page = b->frame;
    return RCOK;
}


/*********************************************************************
 *
 *  bf_m::refix(buf, mode)
 *
 *  Fix "buf" again in "mode".
 *
 *********************************************************************/
void
bf_m::refix(const page_s* buf, latch_mode_t mode)
{
    bfcb_t* b = get_cb(buf);
    w_assert1(b && b->frame == buf);
    _core->pin(b, mode);
    _core->_total_fix++;
    _incr_fixes();
}


/*********************************************************************
 *
 *  bf_m::upgrade_latch_if_not_block(buf, would_block)
 *
 *  Upgrade the latch on buf, only if it would not block.
 *  Set would_block to true if upgrade would block
 *
 *********************************************************************/
void
bf_m::upgrade_latch_if_not_block(const page_s* buf, bool& would_block)
{
    bfcb_t* b = get_cb(buf);
    w_assert1(b && b->frame == buf);
    _core->upgrade_latch_if_not_block(b, would_block);
}



/*********************************************************************
 *
 *  bf_m::unfix(buf, dirty, refbit)
 *
 *  Unfix the buffer "buf". If "dirty" is true, set the dirty bit.
 *  "Refbit" is a page-replacement policy hint to rsrc_m. It indicates
 *  the importance of keeping the page in the buffer pool. If a page
 *  is marked with a 0 refbit, rsrc_m will mark it as the next
 *  replacement candidate.
 *
 *********************************************************************/
void
bf_m::unfix(const page_s*& buf, bool dirty, int refbit)
{
    FUNC(bf_m::unfix);
    bool    kick_cleaner = false;
    bfcb_t* b = get_cb(buf);
    w_assert1(b && b->frame == buf);

    if (dirty)  {
	if(! b->dirty)  {
	    b->dirty = true;
	    if(++_ndirty > _dirty_threshhold) {
		kick_cleaner = true;
	    }
	}
#ifdef TRYTHIS
	if (
#ifdef NOTDEF
// NB: TRYTHIS4 seems to be worth keeping in all the time... ???
// or so it seems at latest reckoning
	(_strategy & TRYTHIS4) &&
#endif /* NOTDEF */
	    // If we're unfixing a dirty page and it's almost full
	    // (this is for the case of creations, forexample)
	    // kick it out asap
	   (_ndirty <= _dirty_threshhold &&
		buf->space.nfree() < (page_s::data_sz >> 3)) ) {
		kick_cleaner = true;
	}
#endif
    } else {
        if (! b->dirty) b->rec_lsn = lsn_t::null;
    }

    DBGTHRD( << "about to unfix " << b->pid );

    _core->unpin(b, refbit);
    _core->_total_fix--;
    _incr_unfixes();
    buf = 0;
    if(kick_cleaner && _cleaner_thread) {
	_cleaner_thread->activate();
	me()->yield();
    }
}


#if defined(OBJECT_CC) && defined(MULTI_SERVER)

/*********************************************************************
 *
 * bf_m::discard_record(const rid_t& rid, bool dummy)
 *
 *********************************************************************/
rc_t
bf_m::discard_record(const rid_t& rid, bool aborting)
{
    bfcb_t* b;
    rc_t rc = _core->find(b, rid.pid, LATCH_NL);

    if (!rc || rc.err_num() == eINTRANSIT) {

	MUTEX_ACQUIRE(b->recmap_mutex);

        if (!rc) {
	    w_assert3(rid.pid == b->frame->pid);
	    if (bm_is_clr(b->recmap, rid.slot) && !aborting)
		smlevel_0::stats.false_cb_cnt++;
            bm_clr(b->recmap, rid.slot);
  	} else {
	    w_assert3(b->remote_io);
	    w_assert3(bm_is_clr(b->recmap, rid.slot));
	    if (!aborting) smlevel_0::stats.false_cb_cnt++;
	}

	if (b->remote_io && !aborting) {
	    cb_race_t* race = bf->race_tab->lookup(rid);
	    if (!race) {
		race = new cb_race_t;
	        race->rid = rid;
	        race_tab->push(race);
	    }
	}

        MUTEX_RELEASE(b->recmap_mutex);
	_core->unpin(b, 0, FALSE);

    } else {
	return rc.reset();
    }
    return RCOK;
}


rc_t
bf_m::discard_many_records(const lpid_t& pid, u_char* dirty_map)
{
    bfcb_t* b;
    rc_t rc = _core->find(b, pid, LATCH_NL);

    if (!rc || rc.err_num() == eINTRANSIT) {

	MUTEX_ACQUIRE(b->recmap_mutex);

	if (!rc) {
	    w_assert3(pid == b->frame->pid);

	    for (int i = 0; i < b->frame->nslots; i++)
	        if (bm_is_set(dirty_map, i)) bm_clr(b->recmap, i);
	} else {
	    w_assert3(b->remote_io);
	    w_assert3(bm_num_set(b->recmap, smlevel_0::max_recs) == 0);
	}

	MUTEX_RELEASE(b->recmap_mutex);
	_core->unpin(b);

    } else {
	return rc.reset();
    }
    return RCOK;
}

#endif /* OBJECT_CC && MULTI_SERVER */


#ifdef MULTI_SERVER
/*********************************************************************
 *
 *  bf_m::discard_page(pid)
 *
 *  Remove page "pid" from the buffer pool. Note that the page
 *  is not flushed even if it is dirty.
 *
 *********************************************************************/
rc_t
bf_m::discard_page(const lpid_t& pid)
{
    TRACE(351, "bf_m: purging page: pid: " << pid);

    bfcb_t* b;
    W_DO(_core->find(b, pid, LATCH_EX));
    w_assert3(pid == b->frame->pid);

    return _discard_pinned_page(b, pid);
}

rc_t
bf_m::_discard_pinned_page( bfcb_t *b, const lpid_t& pid )
{
    FUNC(_discard_pinned_page);

    w_assert1(_core->pin_cnt(b) == 1); 

#ifdef DEBUG
    // for use with assert below
    bfcb_t* p = b;
#endif

    // BUGBUG: some other thread can pin the page (w/o latching it) at this
    // point. _mutex should be acquired before find(). In fact , instead of
    // calling find(), we should only pin the page and then check that pin_cnt
    // is 1. (bug is present if preemptive threads only.)

    MUTEX_ACQUIRE(_core->_mutex);

    if (_core->remove(b))  {
        /* ignore */;
    } else {
        if (pid.is_remote())
            W_COERCE(cbm->purge_page(pid));
        w_assert3(p->pid == lpid_t::null);
    }

    MUTEX_RELEASE(_core->_mutex);

    return RCOK;
}
#endif /* MULTI_SERVER */

#ifdef NOTDEF
/*********************************************************************
 *
 *  bf_m::discard_pinned_page(page)
 *
 *  Remove page "pid" from the buffer pool. Note that the page
 *  is not flushed even if it is dirty.
 *
 *********************************************************************/
rc_t
bf_m::discard_pinned_page(page_p *page)
{
    TRACE(351, "bf_m: purging pinned page: " << page->pid());

    bfcb_t* b;
    W_DO(_core->find(b, page->pid(), LATCH_EX));
    b = get_cb(&page->persistent_part());

    w_assert3(page->pid() == b->frame->pid);

    w_assert1(_core->pin_cnt(b) == 1); 
    return _discard_pinned_page(b, page->pid());
}
#endif /*NOTDEF*/


/*********************************************************************
 *
 *  bf_m::_clean_buf(mincontig, count, pids, timeout, retire_flag)
 *
 *  Sort pids array (of "count" elements) and write out those pages
 *  in the buffer pool. Retire_flag points to a bool flag that any
 *  one can set to cause _clean_buf() to return prematurely.
 *
 *********************************************************************/
#if !defined(TRYTHISNOT)
#define mincontig /*mincontig not used*/
#endif
rc_t
bf_m::_clean_buf(
    int			mincontig,
    int			count,
    lpid_t		pids[],
    int4_t		timeout,
    bool*		retire_flag)
#undef mincontig
{
    /*
     *  sort the pids so we can write them out in sorted order
     */
    qsort(pids, count, sizeof(lpid_t), cmp_lpid);

    /*
     *  1st iteration --- no latch wait, write as many as we
     *  possibly can. 
     */
    int skipped = 0;
    lpid_t* p = pids;
    latch_mode_t lmode = LATCH_SH;

    int i;

#ifdef TRYTHISNOT
    if((_strategy & TRYTHIS3)==0) {
	// If strategy 3 is off, do the original thing
	mincontig = 1;
    }
    // If strategy 3 is on, for the bf_cleaner thread, we
    // only look at contiguous chunks of at least 8 pages
    for (i = count; i >= mincontig; )  
#else
    for (i = count; i; )  
#endif
    {

	if (retire_flag && *retire_flag)   return RCOK;

	bfcb_t* bfcb[8];
	bfcb_t** bp = bfcb;
	lpid_t prev_pid;
	/*
	 *  loop to find consecutive pages 
	 */
	int j;
	for (j = (i < 8) ? i : 8; j; p++, bp++, j--)  {

	    // 0 timeout, do not wait for fixed pages
	    // 0 ref_bit
	    rc_t rc = _core->find(*bp, *p, lmode, 0, 0); 
	    if (rc)  {
		++skipped;
		break;
	    }

	    if (!(*bp)->dirty || 
		(bp != bfcb && (p->page != prev_pid.page + 1 ||
				/* ignore store id */
				p->vol() != prev_pid.vol())))  {
		_core->unpin(*bp);
		*bp = 0;
		++skipped;
		break;
	    } 

	    prev_pid = *p;
	    *p = lpid_t::null;	// mark as processed
	}

	if (j)  {
	    /*
	     *  Some non-consecutive page in the array caused us
	     *  to break out of the loop.
	     */
	    --i;		// for progress
	    ++p;
	}

	/*
	 *  pages between bfcb and bp are consecutive
	 */
	if (bp != bfcb)  {
	    /* 
	     *  assert that pages between bfcb and bp are consecutive
	     */
	   {
		/*
		uint extra = bp - bfcb - 1;
		w_assert3(extra == (*(bp-1))->pid.page - bfcb[0]->pid.page);
		  ss_m::errlog->clog << info_prio 
		    << " cleaner[1]: writing page(s) " 
		    << bfcb[0]->pid.page  << " + " << extra << flushl;
		*/
	    }

	    /*
	     *  write out chunk of consecutive pages
	     */
	    W_DO( _write_out(bfcb, bp - bfcb) );

	    for (bfcb_t** t = bfcb; t != bp; t++)  {
		_core->unpin(*t);
	    }

	    /*
	     *  bp - bfcb pages have been written
	     */
	    i -= (bp - bfcb);
	}
    }

    /*
     *  2nd iteration --- write out pages we missed the first
     *  time round. For this iteration, however, be more persistent
     *  to ensure that hot pages are written out.
     *
     *  NB: pages were skipped only if they were fixed by another
     *  thread or they weren't in contiguous with the rest of the
     *  chunk.
     */
    p = pids;
    for (i = count; i && skipped; p++, i--)  {

	if (! p->page) continue; // already processed

	if (retire_flag && *retire_flag)   return RCOK;

	--skipped;

	bfcb_t* b;
	// wait as long as needed
	rc_t rc = _core->find(b, *p, LATCH_SH, 0, timeout); 
	if (!rc)  {
	    if (b->dirty)  {
		  /*
		  ss_m::errlog->clog << info_prio 
		    << " cleaner[2]: writing page " << b->pid.page << flushl;
		  */
		W_DO( _write_out(&b, 1) );
	    }
	    _core->unpin(b);
	}
    }
    return RCOK;
}


/*********************************************************************
 * 
 *  bf_m::activate_background_flushing()
 *
 *********************************************************************/
void
bf_m::activate_background_flushing()
{
    if (_cleaner_thread) 
	_cleaner_thread->activate(); // wake up cleaner thread
    else 
	W_IGNORE(force_all());	// simulate cleaner thread in flushing
}


/*********************************************************************
 *
 *  bf_m::_write_out(ba, cnt)
 *
 *  Write out frames belonging to the array ba[cnt].
 *  Note: all pages in the ba array belong to the same volume.
 *
 *********************************************************************/
rc_t
bf_m::_write_out(bfcb_t** ba, int cnt)
{
    /*
     *  Chop off head and tail that are not dirty; first, the head
     */
    int i;
    for (i = 0; i < cnt && !ba[i]->dirty; i++);
    ba += i;
    cnt -= i;
    if (cnt==0) return RCOK;	/* no dirty pages */
    /* 
     *  now, the tail
     */
    for (i = cnt - 1; i && !ba[i]->dirty; i--);
    if (i < 0)  return RCOK;
    cnt = i + 1;

    page_s* out_going[8];
    w_assert1(cnt > 0 && cnt <= 8);

    /* 
     * we'll compute the highest lsn of the bunch
     * of pages, and do one log flush to that lsn
     */
    lsn_t  highest = lsn_t::null;
    for (i = 0; i < cnt; i++)  {

	// w_assert1(ba[i]->dirty);
	/*
	 *  if recovery option enabled, dirty frame must have valid
	 *  lsn unless it corresponds to a temporary page
	 */
	if (log)  {
	    lsn_t lsn = ba[i]->frame->lsn1;
	    if (lsn) {
		if (ba[i]->pid.is_remote()) {
#ifdef MULTI_SERVER
		    W_COERCE(rm->flush_log(ba[i]->pid, lsn));
#else
		    W_FATAL(eINTERNAL);
#endif
		} else {
		    if(lsn > highest) {
			highest = lsn;
		    }
		}
	    } else {
		w_assert1(ba[i]->frame->store_flags & page_p::st_tmp);
	    }
	}
	if(highest > lsn_t::null) {
	    _incr_log_flush_lsn();
	    W_COERCE( log->flush(highest) );
	}

#ifdef UNDEF
	/*
	 *  we may want this optimization
	 */
	if (ba[i]->frame->tag == t_file_p)  {
	    if (ba[i]->frame->page_flags & page_p::t_virgin)  {
	    /* genenerate log record for the newly allocated file page */
	    }
	}
#endif

	out_going[i] = ba[i]->frame;
        w_assert1(ba[i]->pid == ba[i]->frame->pid);
    }

    if (! ba[0]->pid.is_remote()) {
	io->write_many_pages(out_going, cnt);
	_incr_page_write(cnt, true); // in background
    }

#ifdef DEBUG
    {
	lsn_t now_highest;
	for (i = 0; i < cnt; i++)  {
	    lsn_t lsn = ba[i]->frame->lsn1;
	    if(lsn > now_highest) {
		now_highest = lsn;
	    }
	}
	if(now_highest > highest) {
	    if( log && now_highest) log->check_wal(now_highest);
	}
    }
#endif

    /*
     *  Buffer is not dirty anymore. Clear dirty flag and
     *  recovery lsn
     */
    for (i = 0; i < cnt; i++)  {
	ba[i]->dirty = false;
	ba[i]->rec_lsn = lsn_t::null;
    }

    return RCOK;
}


/*********************************************************************
 *
 *  bf_m::_replace_out(b)
 *
 * Called from bf_m::fix to write out a dirty victim page.
 * The bf_m::_write_out method is not suitable for this purpuse because
 * it uses the "pid" field of the frame control block (bfcb_t) whereas
 * during replacement we want to write out the page specified by the
 * "old_pid" field.
 *********************************************************************/
rc_t
bf_m::_replace_out(bfcb_t* b)
{
    _incr_replaced(true);

    if (log)  {
        lsn_t lsn = b->frame->lsn1;
        if (lsn) {
            if (b->old_pid.is_remote()) {
#ifdef MULTI_SERVER
                W_COERCE(rm->flush_log(b->old_pid, lsn));
#else
		W_FATAL(eINTERNAL);
#endif
            } else {
		_incr_log_flush_lsn();
                W_COERCE(log->flush(lsn));
            }
        } else {
            w_assert1(b->frame->store_flags & page_p::st_tmp);
        }
    }

    if (! b->old_pid.is_remote()) {
        io->write_many_pages(&b->frame, 1);
	_incr_page_write(1, false); // for replacement
    }

    b->dirty = false;
    b->rec_lsn = lsn_t::null;

    return RCOK;
}


/*********************************************************************
 *
 *  bf_m::get_page(pid, b, no_read, ignore_store_id, is_new)
 *
 *  Initialize the frame pointed by "b' with the page 
 *  identified by "pid". If "no_read" is true, do not read 
 *  page from disk; just initialize its header. 
 *
 *********************************************************************/
#ifndef DEBUG
#define ignore_store_id /* ignore_store_id not used */
#endif
rc_t
bf_m::get_page(
    const lockid_t& 	name, 
    bfcb_t* 		b, 
    uint2		ptag,
    bool 		no_read, 
    bool 		ignore_store_id,
    bool		parallel_read,	// possible for remote pages only
    bool		detect_race,
    bool                is_new)
#undef ignore_store_id
{
    const lpid_t& pid = *(lpid_t*)name.name();

    w_assert3(pid == b->pid);

    if (! no_read)  {
	if (pid.is_remote()) {
#ifdef MULTI_SERVER
	    bool purged = rlm->purged_flag(pid);
	    w_assert3(ptag == page_p::t_file_p || purged == FALSE);
	    W_DO(rm->read_page(name, b, ptag,
			(!purged && detect_race), parallel_read, is_new));

	    // byte swap the page and do any other fixups needed
	    //   (pass volume id since vid needs to be changed for pages
	    //    not stored on a volume on this server)
	    // COERCE(_byte_swap_page_func(pid.vol, buf));

	    b->frame->page_flags |= page_p::t_remote;
	    b->frame->ntoh(pid.vol());
#else
	    W_FATAL(eINTERNAL);
	    // keep compiler quiet about unused parameters
	    if (parallel_read || detect_race || is_new) {}
#endif /* MULTI_SERVER */

	} else {

#ifdef MULTI_SERVER
#ifdef USE_HATESEND
	   if (!is_new && pid.is_remote())
	      W_DO(rm->send_page(pid, *b->frame));
#endif /* USE_HATESEND */	   
#endif /* MULTI_SERVER */	   
	    W_DO( io->read_page(pid, *b->frame) );
	}
    }

    if (! no_read)  {
	// clear virgin flag, and set written flag
	b->frame->page_flags &= ~page_p::t_virgin;
	b->frame->page_flags |= page_p::t_written;

	/*
	 * NOTE: that the store ID may not be correct during
	 * redo-recovery in the case where a page has been
	 * deallocated and reused.  This can arise because the page
	 * will have a new store ID.  If this case should arise, the
	 * commented our assert should be used instead since it does
	 * not check the store number of the page.
	 * Also, if the page LSN is 0 then the page is
	 * new and should have a page ID of 0.
	 */
	if (b->frame->lsn1 == lsn_t::null)  {
	    w_assert3(b->frame->pid.page == 0);
	} else {
	    w_assert3(pid.page == b->frame->pid.page &&
		      pid.vol() == b->frame->pid.vol());

#ifdef DEBUG
	    if( pid.store() != b->frame->pid.store() ) {
		w_assert3(ignore_store_id);
	    }
#endif
	    w_assert3(ignore_store_id ||
		      pid.store() == b->frame->pid.store());
	}
    }

#ifdef DEBUG
    if (pid.is_remote()) {
	int tag = b->frame->tag;
        w_assert3(tag == page_p::t_btree_p || tag == page_p::t_file_p ||
		  tag == page_p::t_lgdata_p || tag == page_p::t_lgindex_p);
    }
#endif /* DEBUG */

    return RCOK;
}




/*********************************************************************
 * 
 *  bf_m::enable_background_flushing()
 *
 *  Spawn cleaner thread.
 *
 *********************************************************************/
rc_t
bf_m::enable_background_flushing()
{
    if (_cleaner_thread)
	    return RCOK;

    bool bad;

    if (!option_t::str_to_bool(_backgroundflush->value(), bad)) {
	    // background flushing is turned off
	    return RCOK;
    }

    _cleaner_thread = new bf_cleaner_thread_t();
    if (! _cleaner_thread)
	    return RC(eOUTOFMEMORY);

    rc_t e = _cleaner_thread->fork();
    if (e != RCOK) {
	    delete _cleaner_thread;
	    _cleaner_thread = 0;
	    return e;
    }

    return RCOK;
}



/*********************************************************************
 *
 *  bf_m::disable_background_flushing()
 *
 *  Kill cleaner thread.
 *
 *********************************************************************/
rc_t
bf_m::disable_background_flushing()
{
    if (_cleaner_thread)  {
	_cleaner_thread->retire();
	delete _cleaner_thread;
	_cleaner_thread = 0;
    }
    return RCOK;
}


/*********************************************************************
 *
 *  bf_m::get_rec_lsn(start, count, pid, rec_lsn, ret)
 *
 *  Get recovery lsn of "count" frames in the buffer pool
 *  starting at index "start". The pids and rec_lsns are returned
 *  in "pid" and "rec_lsn" respectively. The number of frames
 *  with rec_lsn (number of pids and rec_lsns filled in) is 
 *  returned in "ret".
 *
 *********************************************************************/
rc_t
bf_m::get_rec_lsn(int start, int count, lpid_t pid[], lsn_t rec_lsn[],
		  int& ret)
{
    ret = 0;
    w_assert3(start >= 0 && count > 0);
    w_assert3(start + count <= _core->_num_bufs);

    int i;
    for (i = 0; i < count && start < _core->_num_bufs; start++)  {
	if (_core->_buftab[start].dirty && _core->_buftab[start].pid.page &&
				(! _core->_buftab[start].pid.is_remote())) {
	    pid[i] = _core->_buftab[start].pid;
	    rec_lsn[i] = _core->_buftab[start].rec_lsn;
	    w_assert3(rec_lsn[i] != lsn_t::null);
	    i++;
	}
    }
    ret = i;

    return RCOK;
}


/*********************************************************************
 *
 *  bf_m::min_rec_lsn()
 *
 *  Return the minimum recovery lsn of all pages in the buffer pool.
 *
 *********************************************************************/
lsn_t
bf_m::min_rec_lsn()
{
    lsn_t lsn = lsn_t::max;
    for (int i = 0; i < _core->_num_bufs; i++)  {
	if (_core->_buftab[i].dirty && _core->_buftab[i].pid.page &&
					_core->_buftab[i].rec_lsn < lsn)
	    lsn = _core->_buftab[i].rec_lsn;
    }
    return lsn;
}


/*********************************************************************
 *
 *  bf_m::dump()
 *
 *  Print to stdout content of buffer pool (for debugging)
 *
 *********************************************************************/
void 
bf_m::dump()
{
    _core->dump(cout);
}


/*********************************************************************
 *
 *  bf_m::discard_all()
 *
 *  Discard all pages in the buffer pool.
 *
 *********************************************************************/
rc_t
bf_m::discard_all()
{
    return _scan(bf_filter_none_t(), false, true);
}


/*********************************************************************
 *
 *  bf_m::discard_store(stid)
 *
 *  Discard all pages belonging to stid.
 *
 *********************************************************************/
rc_t
bf_m::discard_store(stid_t stid)
{
    return _scan(bf_filter_store_t(stid), false, true);
}


/*********************************************************************
 *
 *  bf_m::discard_volume(vid)
 *
 *  Discard all pages originating from volume vid.
 *
 *********************************************************************/
rc_t
bf_m::discard_volume(vid_t vid)
{
    return _scan(bf_filter_vol_t(vid), false, true);
}


/*********************************************************************
 *
 *  bf_m::force_all(bool flush)
 *
 *  Force (write out) all pages in the buffer pool. If "flush"
 *  is true, then invalidate the pages as well.
 *
 *********************************************************************/
rc_t
bf_m::force_all(bool flush)
{
    return _scan(bf_filter_none_t(), true, flush);
}



/*********************************************************************
 *
 *  bf_m::force_store(stid, flush)
 *
 *  Force (write out) all pages belonging to stid. If "flush"
 *  is true, then invalidate the pages as well.
 *
 *********************************************************************/
rc_t
bf_m::force_store(stid_t stid, bool flush)
{
    return _scan(bf_filter_store_t(stid), true, flush);
}



/*********************************************************************
 *
 *  bf_m::force_volume(vid, flush)
 *
 *  Force (write out) all pages originating from vlume vid.
 *  If "flush" is true, then invalidate the pages as well.
 *
 *********************************************************************/
rc_t
bf_m::force_volume(vid_t vid, bool flush)
{
    return _scan(bf_filter_vol_t(vid), true, flush);
}


/*********************************************************************
 *
 *  bf_m::force_until_lsn(lsn, flush)
 *
 *  Flush (write out) all pages whose rec_lsn is less than 
 *  or equal to "lsn". If "flush" is true, then invalidate the
 *  pages as well.
 * 
 *********************************************************************/
rc_t
bf_m::force_until_lsn(const lsn_t& lsn, bool flush)
{
    // cout << "bf_m::force_until_lsn" << endl;
    return _scan(bf_filter_lsn_t(lsn), true, flush);
}



/*********************************************************************
 *
 *  bf_m::_scan(filter, write_dirty, discard)
 *
 *  Scan and filter all pages in the buffer pool. For successful
 *  candicates (those that passed filter test):
 *     1. if frame is dirty and "write_dirty" is true, then write it out
 *     2. if "discard" is true then invalidate the frame
 *
 *********************************************************************/
rc_t
bf_m::_scan(const bf_filter_t& filter, bool write_dirty, bool discard)
{
    if(write_dirty) {
        /*
         * Sync the log. This is now necessary because
	 * the log is buffered.
         * Plus this is more efficient then small syncs when
         * pages are flushed below.
         */
        if (smlevel_0::log) { // log manager is running
            DBG(<< "flushing log");
	    bf_m::_incr_log_flush_all();
            W_IGNORE(smlevel_0::log->flush_all());
        }
    }
    /*
     *  First, try to call _clean_buf() which is optimized for writes.
     */
    lpid_t* pids = 0;
    if (write_dirty && (pids = new lpid_t[_core->_num_bufs]))  {
	w_auto_delete_array_t<lpid_t> auto_del(pids);
	/*
	 *  Write_dirty, and there is enough memory for pid array...
	 *  Fill pid array with qualify candidate, and call 
	 *  _clean_buf().
	 */
	int count = 0;
	for (int i = 0; i < _core->_num_bufs; i++)  {
	    if (filter.is_good(bf_core_m::_buftab[i]) &&
		_core->_buftab[i].dirty) {

		pids[count++] = _core->_buftab[i].pid;
	    }
	}
	W_DO( _clean_buf(1, count, pids, WAIT_FOREVER, 0) );

	if (! discard)   {
	    return RCOK;	// done 
	}
	/*
	 *  else, fall thru 
	 */
    } 

    /*
     *  We need EX latch to discard, SH latch to write.
     */
    latch_mode_t mode = discard ? LATCH_EX : LATCH_SH;
    
    /*
     *  Go over buffer pool. For each good candidate, fix in 
     *  mode, and force and/or flush the page.
     */
    for (int i = 0; i < _core->_num_bufs; i++)  {
	if (filter.is_good(_core->_buftab[i])) {
	    bfcb_t* b;
	    if (_core->find(b, _core->_buftab[i].pid, mode))  
		continue;
	    if (write_dirty && b->dirty)  {
		W_DO( _write_out(&b, 1) ); 
	    }
	    if (discard)  {
		MUTEX_ACQUIRE(_core->_mutex);
		bfcb_t* tmp = b;
#ifdef MULTI_SERVER
		lpid_t pid = b->pid;
#endif /* MULTI_SERVER */
		if (_core->remove(tmp))  {
		    /* ignore */ ;
		} else {
#ifdef MULTI_SERVER
		    if (pid.is_remote())
			W_COERCE(cbm->purge_page(pid));
#endif /* MULTI_SERVER */
		    w_assert3(b->pid == lpid_t::null);
		}
		MUTEX_RELEASE(_core->_mutex);
	    }
	}
    }
    return RCOK;
}


/*********************************************************************
 *
 *  bf_m::force_page(pid, flush)
 *
 *  If page "pid" is cached,
 *	1. if it is dirty, write it out to the disk
 *  	2. if flush is TRUE, invalidate the frame
 *
 *********************************************************************/
rc_t
bf_m::force_page(const lpid_t& pid, bool flush)
{
    bfcb_t* b;
    W_DO( _core->find(b, pid, flush ? LATCH_EX : LATCH_SH) );
    w_assert3(pid == b->frame->pid && pid == b->pid);
    if (b->dirty) _write_out(&b, 1);
    if (flush)  {
#ifdef DEBUG
	// for use with assert below
	bfcb_t* p = b;
#endif
	MUTEX_ACQUIRE(_core->_mutex);
	if (_core->remove(b))  {
	    /* ignore */;
	} else {
#ifdef MULTI_SERVER
	    if (pid.is_remote())
		W_COERCE(cbm->purge_page(pid));
#endif /* MULTI_SERVER */
	    w_assert3(p->pid == lpid_t::null);
	}
	MUTEX_RELEASE(_core->_mutex);
    } else {
	_core->unpin(b);
    }

    return RCOK;
}


/*********************************************************************
 *
 *  bf_m::set_dirty(buf)
 *
 *  Mark the buffer page "buf" dirty.
 *
 *********************************************************************/
rc_t
bf_m::set_dirty(const page_s* buf)
{
    bfcb_t* b = get_cb(buf);
    if (!b)  {
	// buf is probably something on the stack
	return RCOK;
    }
    w_assert1(b->frame == buf);
    if( !b->dirty ) {
	b->dirty = true;
	if(++_ndirty > _dirty_threshhold) {
	    if(_cleaner_thread) {
		_cleaner_thread->activate();
		me()->yield();
	    }
	}
    }
    return RCOK;
}

/*********************************************************************
 *
 *  bf_m::is_dirty(buf)
 *
 *  For debugging
 *
 *********************************************************************/
bool
bf_m::is_dirty(const page_s* buf)
{
    bfcb_t* b = get_cb(buf);
    w_assert3(b); 
    w_assert3(b->frame == buf);
    return b->dirty;
}


/*********************************************************************
*
* bf_m::set_clean(const lpid_t& pid)
*
* Mark the "pid" buffer page clean.
* This function is called during commit time to un-dirty cached remote
* pages. The pages are assumed W-locked by the committing xact, so no
* latch is acquired to clean them.
*
*********************************************************************/
void
bf_m::set_clean(const lpid_t& pid)
{
    w_assert3(pid.is_remote());
    MUTEX_ACQUIRE(_core->_mutex);
    bfcb_t* b;
    if (_core->has_frame(pid, b)) {
	// the page cannot be in-transit-in
	w_assert1(b && b->frame->pid == pid);
	b->dirty = false;
	w_assert3(b->rec_lsn == lsn_t::null);
    }
    MUTEX_RELEASE(_core->_mutex);
}

/*********************************************************************
 *
 *  bf_m::snapshot(ndirty, nclean, nfree, nfixed)
 *
 *  Return statistics of buffer usage. 
 *
 *********************************************************************/
void
bf_m::snapshot(
    u_int& ndirty, 
    u_int& nclean,
    u_int& nfree, 
    u_int& nfixed)
{
    nfixed = nfree = ndirty = nclean = 0;
    for (int i = 0; i < _core->_num_bufs; i++) { 
	if (_core->_buftab[i].pid.page)  {
	    _core->_buftab[i].dirty ? ++ndirty : ++nclean;
	}
    }
    _core->snapshot(nfixed, nfree);

    /* 
     *  assertion cannot be maintained ... need to lock up
     *  the whole rsrc_m/bf_m for consistent results.
     *
     *  w_assert3(nfree == _num_bufs - ndirty - nclean);
     */
}



/*********************************************************************
 *  
 *  Filters
 *
 *********************************************************************/
bf_filter_store_t::bf_filter_store_t(const stid_t& stid)
    : _stid(stid)
{
}

bool
bf_filter_store_t::is_good(const bfcb_t& p) const
{
    return p.pid.page && (p.pid.stid() == _stid);
}

NORET
bf_filter_vol_t::bf_filter_vol_t(const vid_t& vid)
    : _vid(vid)
{
}

bool
bf_filter_vol_t::is_good(const bfcb_t& p) const
{
    return p.pid.page && (p.pid.vol() == _vid);
}

NORET
bf_filter_none_t::bf_filter_none_t()
{
}

bool
bf_filter_none_t::is_good(const bfcb_t& p) const
{
    return p.pid != lpid_t::null;
}

NORET
bf_filter_sweep_t::bf_filter_sweep_t(int sweep)
    : _sweep((sweep+1) % _threshhold)
{
}

bool
bf_filter_sweep_t::is_good(const bfcb_t& p) const
{
    if( ! p.pid.page)  return false;

#ifdef TRYTHIS
// NB: TRYTHIS5 seems to be worth keeping in all the time... ???
// or so it seems at latest reckoning

    if(
#ifdef NOTDEF
    (bf_m::_strategy & TRYTHIS5) && 
#endif /* NOTDEF */
	p.hot) {
	// skip hot pages even if they are dirty
	// but every "_threshhold" sweeps,
	// we'll include the dirty hot pages
	//
	return _sweep==0?p.dirty:false; 
    }
#endif
    return p.dirty;
}

NORET
bf_filter_lsn_t::bf_filter_lsn_t(const lsn_t& lsn) 
    : _lsn(lsn)
{
}


bool
bf_filter_lsn_t::is_good(const bfcb_t& p) const
{
    // NEH: replaced the following with some asserts
    // and a simpler computation of return value
    // return p.pid.page && p.rec_lsn  && (p.rec_lsn <= _lsn);
    //
#ifdef DEBUG
    if( ! p.pid.page ) {
	w_assert3(! p.rec_lsn );
    }

#ifdef notdef
    // Not a valid assertion:  fix() 
    // sets rec_lsn before it sets dirty
    // (if getting an exclusive lock
    if( p.rec_lsn ) {
	w_assert3(p.dirty);
    } else {
	w_assert3(!p.dirty);
    }
#endif /*notdef*/
#endif /*DEBUG*/

    return p.rec_lsn  && (p.rec_lsn <= _lsn);
}


// 
// ********************************************************************
// 		NOTES ON CLOCK SCHEDULING ALGORITHM
// ********************************************************************
//  
//  	1. Advance clock ptr
// 	2. Test & clear ref bit
// 	3. if (ref bit was set) goto 1
// 	4. if (dirty) 
// 		4.1 schedule page for clearning
// 	5. replace page
// 		
// ********************************************************************


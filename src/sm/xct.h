/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

/*
 *  $Id: xct.h,v 1.87 1996/06/25 14:45:58 nhall Exp $
 */
#ifndef XCT_H
#define XCT_H

class sdesc_cache_t;

// defined in remote.h
class master_xct_proxy_t;
class remote_xct_proxy_t;
class callback_xct_proxy_t;
class callback_op_t;

class logrec_t;
class xct_t;

#ifdef __GNUG__
#pragma interface
#endif

#ifdef MULTI_SERVER

/*
 * pid_hash_t is used for entries into a hash table that registers the pages
 * dirtied by a transaction.
 */
struct pid_hash_t {
    NORET	pid_hash_t(const lpid_t& p): pid(p)
#ifdef OBJECT_CC
    		, nslots(0), recmap(0)
#endif
		{}

    NORET	~pid_hash_t() {
#ifdef OBJECT_CC
	if (recmap) delete [] recmap;
#endif
	link.detach();
    }

    w_link_t	link;
    lpid_t	pid;
#ifdef OBJECT_CC
    int		nslots;
    u_char*	recmap;
#endif
};

#endif /* MULTI_SERVER */


class file_EX_lock_cache_t {
public:
    NORET	file_EX_lock_cache_t();
    NORET	~file_EX_lock_cache_t();

    bool	lookup(const lockid_t& pid);
    void	insert(const lockid_t& id);

private:
    lockid_t*	fids;
    int		curr_num;
    int         max_num;	// size of the fids array;

};


inline
file_EX_lock_cache_t::file_EX_lock_cache_t(): fids(0), curr_num(0), max_num(10)
{
    fids = new lockid_t[10];
    if (!fids) W_FATAL(eOUTOFMEMORY);
}

inline
file_EX_lock_cache_t::~file_EX_lock_cache_t()
{
    delete [] fids;
}

inline bool
file_EX_lock_cache_t::lookup(const lockid_t& pid)
{
    w_assert3(pid.lspace() == lockid_t::t_page);
    if (curr_num > 0) {
        lockid_t fid = pid;
        lockid_t vid = pid;
        fid.truncate(lockid_t::t_store);
        vid.truncate(lockid_t::t_vol);

        for (int i = 0; i < curr_num; i++) {
            if (fids[i] == fid || fids[i] == vid) return TRUE;
        }
    }
    return FALSE;
}

inline void
file_EX_lock_cache_t::insert(const lockid_t& id)
{
    int i;
    for (i = 0; i < curr_num; i++) {
        if (fids[i] == id) return;
    }
    if (i < max_num) {
	fids[i] = id;
    } else {
	lockid_t* tmp = new lockid_t[2 * max_num];
	if (!tmp) W_FATAL(eOUTOFMEMORY);
	for (i = 0; i < max_num; i++) tmp[i] = fids[i];
	delete [] fids;
	fids = tmp;
	fids[i] = id;
	max_num = 2 * max_num;
    }
    curr_num++;
}

class xct_t : public smlevel_1 {
    friend class xct_log_switch_t;
    friend class xct_i;
    friend class restart_m;
    enum commit_t { t_regular = 0, t_lazy = 1, t_chain = 2 };
public:
    typedef xct_state_t 	state_t;
    typedef smlevel_0::coord_handle_t 	coord_t;

    enum type_t { t_master, t_remote, t_callback };

    state_t			state() const;
    vote_t			vote() const;
    bool			is_extern2pc() const;
    rc_t			enter2pc(const gtid_t &g);

    const smlevel_0::coord_handle_t& 	get_coordinator()const; 
    void 			set_coordinator(const coord_t &); 

    static rc_t			recover2pc(const gtid_t &g,
					bool mayblock, xct_t *&);  

    void			acquire_1thread_mutex();
    void			release_1thread_mutex();
    void			assert_1thread_mutex_free() const;
    bool			one_thread_attached() const;   // assertion
    void                        change_state(state_t new_state);
    const lsn_t&		last_lsn() const;
    const lsn_t&		first_lsn() const;
    const lsn_t&		undo_nxt() const;
    const logrec_t*		last_log() const;

    static xct_t*		look_up(const tid_t& tid);
    static tid_t 		oldest_tid();	// with min tid value
    static tid_t 		youngest_tid();	// with max tid value
    static uint4_t		num_active_xcts();
    // static lsn_t		min_first_lsn();

    NORET			xct_t(
	long			    timeout = WAIT_SPECIFIED_BY_THREAD,
	type_t			    type = t_master);
    NORET			xct_t(
	const tid_t& 		    tid, 
	state_t 		    s, 
	const lsn_t&		    last_lsn,
	const lsn_t& 		    undo_nxt,
	long			    timeout = WAIT_SPECIFIED_BY_THREAD);
    NORET			xct_t(const logrec_t& r);
    NORET			~xct_t();

    static int 			cleanup(bool dispose_prepared=false); 
				 // returns # prepared txs not disposed-of

    rc_t			prepare();
    rc_t			log_prepared(bool in_chkpt=false);
    rc_t			commit(bool lazy = false);
    rc_t			rollback(lsn_t save_pt);
    rc_t			save_point(lsn_t& lsn);
    rc_t			chain(bool lazy = false);
    rc_t			abort();
    rc_t			dispose();

    rc_t			add_dependent(xct_dependent_t* dependent);
    bool			find_dependent(xct_dependent_t* dependent);

    void 			start_crit();
    void 			stop_crit();
    const lsn_t& 		anchor();
    void 			compensate(const lsn_t&, bool undoable = false);
    void 			compensate_undo(const lsn_t&);

    NORET			operator const void*() const;

    /*
     *	logging functions -- used in logdef.i
     */
    bool			is_log_on() const;
    rc_t			get_logbuf(logrec_t*&);
    void			give_logbuf(logrec_t*);
 private: // disabled for now
    // void			invalidate_logbuf();
 public:

    void			flush_logbuf();
    state_t			status() const;
    ostream &			dump_locks(ostream &) const;
    rc_t			check_lock_totals(int nex, int nix, int nsix, int ) const;
    rc_t			obtain_locks(lock_mode_t mode, int nlks, const lockid_t *l); 
    rc_t			obtain_one_lock(lock_mode_t mode, const lockid_t &l); 


    /////////////////////////////////////////////////////////////////
    // used internallly only:
    void                        set_alloced();
    void                        set_freed();
    bool			in_nested_compensated_op();  // needed by exent-allocation
    /////////////////////////////////////////////////////////////////

    concurrency_t		get_lock_level_const() const; // const but not safe
    concurrency_t		get_lock_level(); // non-const because it acquires mutex 
    void		   	lock_level(concurrency_t l);

    /////////////////////////////////////////////////////////////////
    // non-const because it acquires mutex:
    // removed, now that the lock mgrs use the const,inline-d form
    // long			timeout(); 

    // does not acquire the mutex :
    inline
    long			timeout_c() const { 
				    return  _timeout; 
				}
    void			set_timeout(long t); 

    /////////////////////////////////////////////////////////////////
    // turn on(enable=TRUE) or  off/(enable=FALSE) the lock cache 
    // return previous state.
    bool 			set_lock_cache_enable(bool enable);

    // return whether lock cache is enabled
    bool 			lock_cache_enabled() const;

    /////////////////////////////////////////////////////////////////
    // use faster new/delete
    NORET			W_FASTNEW_CLASS_DECL;
    void*			operator new(size_t, void* p)  {
	return p;
    }

    rc_t			_commit(uint4_t flags);
    static void 		xct_stats(
	u_long& 		    begins,
	u_long& 		    commits,
	u_long& 		    aborts,
	bool 			    reset);
    bool			is_local() const;
    bool			is_distributed() const;
    bool			is_remote_proxy() const;

#ifdef MULTI_SERVER
    // Functions related to distributed transactions
    bool			is_master_proxy() const;
    bool			is_cb_proxy() const; 
    w_list_t<master_xct_proxy_t>& master_proxy_list(); 
    srvid_t			master_site_id() const;
    remote_xct_proxy_t*		master_site() const;
    void			set_master_site(remote_xct_proxy_t* r);
    callback_xct_proxy_t*	cb_proxy() const;
    void			set_cb_proxy(callback_xct_proxy_t* cb);
    callback_op_t*		callback() const;
    w_hash_t<pid_hash_t, lpid_t>& dirty_pages() const;
    void                        set_dirty(const page_p& page, int idx = -1);
    void			EX_locked_files_insert(const lockid_t& id);

#endif /* MULTI_SERVER */

    friend ostream& operator<<(ostream&, const xct_t&);

    tid_t 			tid() const { return _tid; }	//safe because it's const

    inline
    lockid_t*  			lock_info_hierarchy() const {
				    return me()->lock_hierarchy();
				}

    inline 
    xct_lock_info_t*  		lock_info() const {
				    return _lock_info;
				}

    inline
    sdesc_cache_t*    		sdesc_cache() const {
				    return me()->sdesc_cache();
				}

    /////////////////////////////////////////////////////////////////
    // the following is put here because smthread doesn't know about the structures
    // and we have changed these to be a per-thread structures.
    static lockid_t*  		new_lock_hierarchy();
    static void  		delete_lock_hierarchy(lockid_t *);

    static sdesc_cache_t*  	new_sdesc_cache_t();
    static void  		delete_sdesc_cache_t(sdesc_cache_t *);
    /////////////////////////////////////////////////////////////////

    // NB: TO BE USED ONLY BY SMTHREAD functions!!!
    int				attach_thread(); // returns # attached upon return
    int				detach_thread(); // returns # attached upon return

    // NB: TO BE USED ONLY BY LOCK MANAGER 
    w_rc_t			lockblock(long timeout);// await other thread's wake-up in lm
    void			lockunblock(); 	 // inform other waiters
    int				num_threads(); 	 // 

private:
    void			_flush_logbuf(bool sync=false);

private: // all data members private
				// to be manipulated only by smthread funcs
    int				_threads_attached; 
    scond_t			_waiters; // for preserving deadlock detector's assumptions
					
    const tid_t 		_tid;

    concurrency_t		_lock_level;
    long			_timeout; // default timeout value for lock reqs

    // the 1thread mutex is used to ensure that only one thread
    // is in the SSM on behalf of a transaction.
    smutex_t			_1thread;
    static const char*		_1thread_name; // name of mutex

    state_t 			_state;
    vote_t 			_vote;
    gtid_t *			_global_tid; // null if not participating
    coord_t 			_coord_handle; // ignored for now
    bool			_read_only;
    lsn_t			_first_lsn;
    lsn_t			_last_lsn;
    lsn_t			_undo_nxt;
    w_link_t			_xlink;
    bool			_lock_cache_enable;

    // list of dependents
    w_list_t<xct_dependent_t>	_dependent_list;

#ifdef MULTI_SERVER

    // list of servers this xct has spread to
    w_list_t<master_xct_proxy_t> _master_proxy_list;

    // Site which is master for this transaction
    // (_master_site is also a member of _remote_proxy_list)
    remote_xct_proxy_t*		 _master_site;

    // non-null if this xact is running as a callback proxy
    callback_xct_proxy_t*	_callback_proxy;

    callback_op_t*              _callback;     // outstanding callback op.

    // list of sites we are serving as a remote xct for
    // TODO: Support serving as remote xct for >1 site
    //w_list_t<remote_xct_proxy_t> _remote_proxy_list;

    /*
     * Remote pages dirtied by this xact.
     */
    w_hash_t<pid_hash_t, lpid_t>* _dirty_pages;

    /*
     * Remote files and volumes that have been EX-locked by this xact;
     * used for redo-at-server-purposes
     */
    file_EX_lock_cache_t*	_EX_locked_files;
#endif /* MULTI_SERVER */

    /*
     *  lock request stuff
     */
    xct_lock_info_t*		_lock_info;

    /*
     *  log_m related
     */
    switch_t			_log_state;	// flag indicates log ON/OFF
    logrec_t*			_last_log;	// last log generated by xct
    logrec_t*			_log_buf;
    page_p 			_last_mod_page; // fix page affected
						// by _last_log so bf
						// cannot flush it out
						// without first writing _last_log
    smthread_t* 		_last_mod_thread; // the thread that
						// modified _last_mod_page
						// and therefore must be
						// the thread to flush the
						// _last_log and the page out
						// (in order to preserve
						// assumptions in the buffer
						// manager -- thread T cannot
						// unfix a page fixed by S

    /*
     * For figuring the amount of log bytes needed to roll back
     */
    u_int			_log_bytes_fwd; // bytes written during forward 
						    // progress
    u_int			_log_bytes_bwd; // bytes written during rollback


    /*
       This flag is set whenever a page/store is free'd/alloced.  If set
       the transaction will call io_m::invalidate_free_page_cache()
       indicating that the io_m must invalidate it's cache of last
       extents in files with free pages.  In the future it might be wise
       to extend this to hold info about which stores were involved so
       that only specific store entries need be removed from the cache.
     */
     bool			_alloced_page;
     bool			_freed_page;
     int			_in_compensated_op; 
		// in the midst of a compensated operation
		// use an int because they can be nested.
     lsn_t			_anchor;
		// the anchor for the outermost compensated op


    /*
     *	static part
     */

    // list of all transactions instances
    static smutex_t			  _xlist_mutex;
    static w_descend_list_t<xct_t, tid_t> _xlist;

    ///////////////////////////////////////////////////////////////////////
    // These functions are required so that ss_m::dismount_dev can dismount
    // devices only while there are no active xcts.
    //
public:
    static rc_t AcquireXlistMutex()		{return _xlist_mutex.acquire();};
    static void ReleaseXlistMutex()		{_xlist_mutex.release();};
    static uint4_t NumberOfTransactions()	{return _xlist.num_members();};

private:

    static tid_t 		_nxt_tid;// Not safe for pre-emptive threads

    /*
     * Transaction stats
     */
    static void 		incr_begin_cnt()   {
	smlevel_0::stats.begin_xct_cnt++;
    }
    static void 		incr_commit_cnt()   {
	smlevel_0::stats.commit_xct_cnt++;
    }
    static void 		incr_abort_cnt()   {
	smlevel_0::stats.abort_xct_cnt++;
    }

    // disabled
    NORET			xct_t(const xct_t&);
    xct_t& 			operator=(const xct_t&);
};


inline 
bool			
xct_t::in_nested_compensated_op()
{
    return (_in_compensated_op > 1);
}

inline
xct_t::state_t
xct_t::state() const
{
    return _state;
}


inline
bool
operator>(const xct_t& x1, const xct_t& x2)
{
    return x1.tid() > x2.tid();
}


class xct_i  {
public:
    // NB: still not safe, since this does not
    // lock down the list for the entire iteration.
    xct_t* curr()  {
	    xct_t *x;
	    W_COERCE(xct_t::_xlist_mutex.acquire());
	    x = unsafe_iterator->curr();
	    xct_t::_xlist_mutex.release();
	    return x;
	}
    xct_t* next()  {
	    xct_t *x;
	    W_COERCE(xct_t::_xlist_mutex.acquire());
	    x = unsafe_iterator->next();
	    xct_t::_xlist_mutex.release();
	    return x;
	}
    xct_i()  {
		unsafe_iterator = new
			w_list_i<xct_t>(xct_t::_xlist);
	}
    ~xct_i() { delete unsafe_iterator; }
private:
    w_list_i<xct_t> *unsafe_iterator;

    // disabled
    xct_i(const xct_i&);
    xct_i& operator=(const xct_i&);
};
    

class xct_log_switch_t : public smlevel_0 {
    switch_t old_state;
public:
    xct_log_switch_t(switch_t s) {
	if (xct())  {
	    old_state = xct()->_log_state;
	    xct()->_log_state = s;
	}
    }
    ~xct_log_switch_t()  {
	if (xct())   xct()->_log_state = old_state;
    }
};


// For use in sm functions that don't allow
// active xct when entered.  These are functions that
// apply to local volumes only.
class xct_auto_abort_t : public smlevel_1 {
public:
    xct_auto_abort_t(xct_t* xct) : _xct(xct) {}
    ~xct_auto_abort_t() {
	switch(_xct->state()) {
	case smlevel_1::xct_ended:
	    // do nothing
	    break;
	case smlevel_1::xct_active:
	    W_COERCE(_xct->abort());
	    break;
	default:
	    W_FATAL(eINTERNAL);
	}
    }
    rc_t commit() {
	// These are only for local txs
	// W_DO(_xct->prepare());
	w_assert3(!_xct->is_distributed());
	W_DO(_xct->commit());
	return RCOK;
    }
    rc_t abort() {W_DO(_xct->abort()); return RCOK;}

private:
    xct_t*	_xct;
};

/*
 * Use X_DO inside compensated operations
 */
#define X_DO(x)             \
{                           \
    w_rc_t __e = (x);       \
    if (__e) {			\
	w_assert3(xct());	\
	xct()->stop_crit();	\
	return RC_AUGMENT(__e); \
    } \
}


inline
tid_t
xct_t::youngest_tid()
{
    return _nxt_tid;
}

inline
bool
xct_t::lock_cache_enabled() const
{
    return _lock_cache_enable;
}

inline
vote_t
xct_t::vote() const
{
    return _vote;
}

inline
xct_t::state_t
xct_t::status() const
{
    return _state;
}

inline
const lsn_t&
xct_t::last_lsn() const
{
    return _last_lsn;
}

inline
const lsn_t&
xct_t::first_lsn() const
{
    return _first_lsn;
}

inline
const lsn_t&
xct_t::undo_nxt() const
{
    return _undo_nxt;
}

inline
const logrec_t*
xct_t::last_log() const
{
    return _last_log;
}


inline
bool xct_t::is_log_on() const
{
    return (_log_state == ON && log) ? true : false;
}

inline
xct_t::operator const void*() const
{
    return _state == xct_stale ? 0 : (void*) this;
}


inline
void  		
xct_t::delete_lock_hierarchy(lockid_t *x)
{
    delete[] x;
}

inline
smlevel_0::concurrency_t		
xct_t::get_lock_level_const()  const
{ 
    return _lock_level; 
}
#endif /* XCT_H */

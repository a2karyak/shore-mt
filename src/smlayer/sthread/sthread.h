/*<std-header orig-src='shore' incl-file-exclusion='STHREAD_H'>

 $Id: sthread.h,v 1.187 2002/02/13 22:06:11 bolo Exp $

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

#ifndef STHREAD_H
#define STHREAD_H

#include "w_defines.h"

/*  -- do not edit anything above this line --   </std-header>*/

/*
 *   NewThreads is Copyright 1992, 1993, 1994, 1995, 1996, 1997 by:
 *
 *	Josef Burger	<bolo@cs.wisc.edu>
 *	Dylan McNamee	<dylan@cse.ogi.edu>
 *      Ed Felten       <felten@cs.princeton.edu>
 *
 *   All Rights Reserved.
 *
 *   NewThreads may be freely used as long as credit is given
 *   to the above authors and the above copyright is maintained.
 */

/*
 * The base thread functionality of Shore Threads is derived
 * from the NewThreads implementation wrapped up as c++ objects.
 */

#ifndef W_H
#include <w.h>
#endif

#include <w_timer.h>

class sthread_t;
class smthread_t;

#include <signal.h>
#include <setjmp.h>

/* The setjmp() / longjmp() used should NOT restore the signal mask.
   If sigsetjmp() / siglongjmp() is available, use them, as they
   guarantee control of the save/restore of the signal mask */

#if defined(SUNOS41) || defined(SOLARIS2) || defined(Ultrix42) || defined(HPUX8) || defined(AIX32) || defined(AIX41) || defined(Linux) || defined(__NetBSD__) || defined(OSF1)
#define POSIX_SETJMP
#endif

#ifdef POSIX_SETJMP
#define	thread_setjmp(j)	sigsetjmp(j,0)
#define	thread_longjmp(j,i)	siglongjmp(j,i)
typedef	sigjmp_buf	thread_jmp_buf;
#else
#define	thread_setjmp(j)	_setjmp(j)

#ifdef _WINDOWS
#define	thread_longjmp(j,i)	longjmp(j,i)
#else
#define	thread_longjmp(j,i)	_longjmp(j,i)
#endif /* !_WINDOWS */

typedef	jmp_buf		thread_jmp_buf;
#endif /* POSIX_SETJMP */

#if defined(AIX32) || defined(AIX41)
extern "C" {
#include <sys/select.h>
}
#endif

typedef w_list_t<sthread_t>		sthread_list_t;

#ifdef __GNUC__
#pragma interface
#endif

// If it is defined by the system includes, lose it.  We should
// change our WAIT_ enums to not overlap the posix WAIT_ namespace
// for wait
#if defined(WAIT_ANY)
#undef WAIT_ANY
#endif

#include <sdisk.h>

#ifdef STHREAD_IO_DISKRW
class sdisk_handler_t;
#endif

class vtable_info_t;
class vtable_info_array_t;

class smutex_t;
class scond_t;
class sevsem_t;

struct sthread_core_t;

struct sthread_exception_t;

class	svcport_t;

/*
 * Normally these typedefs would be defined in the sthread_t class,
 * but due to bugs in gcc 2.5 we had to move them outside.
 * TODO: Fix for later g++ releases
 */
class ready_q_t;

class sthread_base_t : public w_base_t {
public:
    typedef uint4_t id_t;

    /* XXX this is really something for the SM, not the threads package;
       only WAIT_IMMEDIATE should ever make it to the threads package. */
    enum {
	WAIT_IMMEDIATE 	= 0, 
		// sthreads package recognizes 2 WAIT_* values:
		// == WAIT_IMMEDIATE
		// and != WAIT_IMMEDIATE.
		// If it's not WAIT_IMMEDIATE, it's assumed to be
		// a positive integer (milliseconds) used for the
		// select timeout.
		//
		// All other WAIT_* values other than WAIT_IMMEDIATE
		// are handled by sm layer.
		// 
		// The user of the thread (e.g., sm) had better
		// convert timeout that are negative values (WAIT_* below)
		// to something >= 0 before calling block().
		//
		//
	WAIT_FOREVER 	= -1,
	WAIT_ANY 	= -2,
	WAIT_ALL 	= -3,
	WAIT_SPECIFIED_BY_THREAD 	= -4, // used by lock manager
	WAIT_SPECIFIED_BY_XCT = -5 // used by lock manager
    };
    /* XXX int would also work, sized type not necessary */
    typedef int4_t timeout_in_ms;

    static const w_error_t::info_t 	error_info[];
	static void  init_errorcodes();

#include "st_error_enum_gen.h"

    enum {
	stOS = fcOS,
	stINTERNAL = fcINTERNAL,
	stNOTIMPLEMENTED = fcNOTIMPLEMENTED
    };

    /* import sdisk base */
    typedef sdisk_base_t::fileoff_t	fileoff_t;
    typedef sdisk_base_t::filestat_t	filestat_t;
    typedef sdisk_base_t::disk_geometry_t	disk_geometry_t;
    typedef sdisk_base_t::iovec_t	iovec_t;

    static const fileoff_t		fileoff_max;

    /* XXX magic number */
    enum { iovec_max = 8 };

    enum {
	OPEN_RDWR = sdisk_base_t::OPEN_RDWR,
	OPEN_RDONLY = sdisk_base_t::OPEN_RDONLY,
	OPEN_WRONLY = sdisk_base_t::OPEN_WRONLY,

	OPEN_SYNC = sdisk_base_t::OPEN_SYNC,
	OPEN_TRUNC = sdisk_base_t::OPEN_TRUNC,
	OPEN_CREATE = sdisk_base_t::OPEN_CREATE,
	OPEN_EXCL = sdisk_base_t::OPEN_EXCL,
	OPEN_APPEND = sdisk_base_t::OPEN_APPEND,
	OPEN_RAW = sdisk_base_t::OPEN_RAW
    };
    enum {
	SEEK_AT_SET = sdisk_base_t::SEEK_AT_SET,
	SEEK_AT_CUR = sdisk_base_t::SEEK_AT_CUR,
	SEEK_AT_END = sdisk_base_t::SEEK_AT_END
    };

    /* Additional open flags which shouldn't collide with sdisk flags,
       the top nibble is left for sthread flags. */
    enum {
	OPEN_LOCAL   = 0x10000000,	// do I/O locally
	OPEN_KEEP    = 0x20000000,	// keep fd open
	OPEN_FAST    = 0x40000000, 	// open for fastpath I/O
	OPEN_STHREAD = 0x70000000	// internal, sthread-specific mask
    };
};


class sthread_name_t {
public:
	enum { NAME_ARRAY = 64 };

	char		_name[NAME_ARRAY];

	sthread_name_t();
	~sthread_name_t();

	void rename(const char *n1, const char *n2=0, const char *n3=0);

	W_FASTNEW_CLASS_PTR_DECL(sthread_name_t);
};


class sthread_named_base_t: public sthread_base_t, public w_own_heap_base_t  {
public:
    NORET			sthread_named_base_t(
	const char*		    n1 = 0,
	const char*		    n2 = 0,
	const char*		    n3 = 0);
    NORET			~sthread_named_base_t();
    
    void			rename(
	const char*		    n1,
	const char*		    n2 = 0,
	const char*		    n3 = 0);

    const char*			name() const;
    void			unname();

private:
    sthread_name_t*		_name;
};

inline NORET
sthread_named_base_t::sthread_named_base_t(
    const char*		n1,
    const char*		n2,
    const char*		n3)
	: _name(0)
{
    if (n1) rename(n1, n2, n3);

}

inline const char*
sthread_named_base_t::name() const
{
    return _name ? _name->_name : ""; 
}

class sthread_main_t;
class sthread_idle_t;
#if defined(NEW_IO) && defined(_WIN32)
class win32_event_handler_t;
class win32_event_t;
#else
class sfile_handler_t;
class sfile_hdl_base_t;
#endif

class ThreadFunc
{
    public:
	virtual void operator()(const sthread_t& thread) = 0;
	virtual NORET ~ThreadFunc() {}
};


#include "strace.h"
class _strace_t;

/*
 *  Thread Structure
 */
class sthread_t : public sthread_named_base_t  {
    friend class sthread_init_t;
    friend class sthread_priority_list_t;
    friend class sthread_idle_t;
    friend class sthread_main_t;
    friend class sthread_timer_t;
    /* For access to block() and unblock() */
    friend class smutex_t;
    friend class scond_t;
    friend class diskport_t;
    friend class sdisk_handler_t;
    /* For access to I/O stats */
    friend class IOmonitor;
    
public:

    enum status_t {
	t_defunct,	// thread has terminated
	t_virgin,	// thread hasn't started yet	
	t_ready,	// thread is ready to run
	t_running,	// when me() is this thread 
	t_blocked,      // thread is blocked on something
	t_boot		// system boot
    };
    static const char *status_strings[];

    enum priority_t {
	t_time_critical = 3,
	t_regular	= 2,
	t_fixed_low	= 1,
	t_idle_time 	= 0,
	max_priority    = t_time_critical,
	min_priority    = t_idle_time
    };
    static const char *priority_strings[];

    /* Default stack size for a thread */
    enum { default_stack = 64*1024 };

    /*
     *  Class member variables
     */
    void* 			user;	// user can use this 

    const id_t 			id;

    int				trace_level;	// for debugging

private:
    static w_rc_t		block(
	int4_t 			    timeout = WAIT_FOREVER,
	sthread_list_t*		    list = 0,
	const char* const	    caller = 0,
	const void *		    id = 0);
    w_rc_t			unblock(const w_rc_t& rc = *(w_rc_t*)0);

public:
    virtual void		_dump(ostream &) const; // to be over-ridden

    // these traverse all threads
    static void			dump(const char *, ostream &);
    static void			dump(ostream &);

    static void			dump_io(ostream &);
    static void			dump_event(ostream &);

    static void                 dump_stats(ostream &);
    static void                 reset_stats();

    static void			dump_stats_io(ostream &);
    static void			reset_stats_io();

    virtual void		vtable_collect(vtable_info_t &); // to be over-ridden
    static int 			collect(vtable_info_array_t &v);

    static void			find_stack(void *address);
    static void			for_each_thread(ThreadFunc& f);

    /* request stack overflow check, die on error. */
    static void			check_all_stacks(const char *file = "",
    						 int line = 0);
    bool			isStackOK(const char *file = "", int line = 0);

    /* Recursion, etc stack depth estimator */
    bool			isStackFrameOK(unsigned size = 0);

    w_rc_t			set_priority(priority_t priority);
    priority_t			priority() const;
    status_t			status() const;
    w_rc_t			set_use_float(int);
    void			push_resource_alloc(const char* n,
						    const void *id = 0,
						    bool is_latch = false);
    void			pop_resource_alloc(const void *id = 0);

    /*
     * per-thread heap management
     */
    static sthread_t* 		id2thread( w_heapid_t );
    w_heapid_t 			per_thread_alloc(size_t amt, bool is_free);
    void 			per_thread_alloc_stats(size_t& amt, 
					    size_t& allocations,
					    size_t& hiwat) const;

    /*
     *  Concurrent I/O ops
     */
    static char*		set_bufsize(unsigned size);
    static w_rc_t		set_bufsize(unsigned size, char *&start);

    static w_rc_t		open(
	const char*		    path,
	int			    flags,
	int			    mode,
	int&			    fd);
    static w_rc_t		close(int fd);
    static w_rc_t		read(
	int 			    fd,
	void*	 		    buf,
	int 			    n);
    static w_rc_t		write(
	int 			    fd, 
	const void* 		    buf, 
	int 			    n);
    static w_rc_t		readv(
	int 			    fd, 
	const iovec_t* 		    iov,
	size_t			    iovcnt);
    static w_rc_t		writev(
	int 			    fd,
	const iovec_t*	    	    iov,
	size_t 			    iovcnt);
    static w_rc_t		pread(int fd, void *buf, int n, fileoff_t pos);
    static w_rc_t		pwrite(int fd, const void *buf, int n,
				       fileoff_t pos);
    static w_rc_t		lseek(
	int			    fd,
	fileoff_t		    offset,
	int			    whence,
	fileoff_t&		    ret);
    /* returns an error if the seek doesn't match its destination */
    static w_rc_t		lseek(
	int			    fd,
	fileoff_t	    	    offset,
	int			    whence);
    static w_rc_t		fsync(int fd);
    static w_rc_t		ftruncate(int fd, fileoff_t sz);
    static w_rc_t		fstat(int fd, filestat_t &sb);
    static w_rc_t		fgetfile(int fd, void *);
    static w_rc_t		fgetgeometry(int fd, disk_geometry_t &dg);
    static w_rc_t		fisraw(int fd, bool &raw);


    /*
     *  Misc
     */
    static sthread_t*		me()  { return _me; }

    /* XXX  sleep, fork, and wait exit overlap the unix version. */

    // sleep for timeout milliseconds
    void			sleep(timeout_in_ms timeout = WAIT_IMMEDIATE,
				      const char *reason = 0);
    void			wakeup();

    // wait for a thread to finish running
    w_rc_t			wait(timeout_in_ms timeout = WAIT_FOREVER);

    // start a thread
    w_rc_t			fork();

    // exit the current thread
    static void 		end();
    static void			exit() { end(); }

    // give up the processor
    static void			yield(bool doselect=false);

    static void			set_diskrw_name(const char* diskrw);

    ostream			&print(ostream &) const;

    unsigned			stack_size() const;

    // anyone can wait and delete a thread
    virtual			~sthread_t();

#if defined(NEW_IO) && defined(_WIN32)
    static w_rc_t		io_start(win32_event_t &);
    static w_rc_t		io_finish(win32_event_t &);
#else
    static w_rc_t		io_start(sfile_hdl_base_t &);
    static w_rc_t		io_finish(sfile_hdl_base_t &);
    static bool			io_active(int fd);
#endif

    // function to do runtime up-cast to smthread_t
    // return 0 if the sthread is not derrived from sm_thread_t.
    // should be removed when RTTI is supported
    virtual smthread_t*		dynamic_cast_to_smthread();
    virtual const smthread_t*	dynamic_cast_to_const_smthread() const;

protected:
    sthread_t(priority_t	priority = t_regular,
	      const char	*name = 0,
	      unsigned		stack_size = default_stack);

    virtual void		run() = 0;

private:

    friend class pipe_handler;
    friend class ready_q_t;

    static ready_q_t*		_ready_q;	// ready queue

    /* start offset of sthread FDs, to differentiate from system FDs */
    enum { fd_base = 4000 };

    // thread resource tracing
    strace_t	_trace;
    _strace_t	*_blocked;	// resource thread is blocked for

    sevsem_t*			_terminate;	// posted when thread ends

    sthread_core_t		*_core;		// registers, stack, etc
    status_t			_status;	// thread status
    priority_t			_priority; 	// thread priority
    thread_jmp_buf		_exit_addr;	// longjmp to end thread
    w_rc_t			_rc;		// used in block/unblock

    w_link_t			_link;		// used by smutex etc

    w_link_t			_class_link;	// used in _class_list
    static sthread_list_t*	_class_list;

    sthread_t*			_ready_next;	// used in ready_q_t
    sthread_timer_t*		_tevent;	// used in sleep/wakeup

    sthread_exception_t		*_exception;	// exception context
    /* XXX alignment probs in derived thread classes.  Sigh */
    fill4			_ex_fill;

    /* Per-thread heap management */
    size_t			_bytes_allocated;
    size_t			_allocations;    
    size_t			_high_water_mark;    

    /* I/O subsystem */
#ifndef STHREAD_FDS_STATIC
	static	sdisk_t		**_disks;
	static	unsigned	open_max;
#else
	enum { open_max = 20 };
	static	sdisk_t		*_disks[open_max];
#endif
	static	unsigned	open_count;

    static	bool		_do_fastpath;

    /* in-thread startup and shutdown */ 
    static void			__start(void *arg_thread);
    void			_start();
    void			_reset_cpu();


    /* system initialization and shutdown */
    static w_rc_t		startup();
    static w_rc_t		startup_events();
    static w_rc_t		startup_io();
    static w_rc_t		shutdown();
    static w_rc_t		shutdown_io();

    static stime_t		boot_time;

#ifdef _WINDOWS
   typedef unsigned int sigset_t;
#else
    static w_rc_t		setup_signals(sigset_t &lo_spl,
					      sigset_t &hi_spl);
#endif

    static void 		_caught_signal(int);
    static void 		_caught_signal_io(int);
    static void 		_ctxswitch(status_t s); 
    static void			_polldisk();

    static sthread_t*		_me;		// the running thread
    static sthread_t*		_idle_thread;
    static sthread_t*		_main_thread; 
    static uint4_t		_next_id;	// unique id generator
    static bool			_stack_checks;

    static w_timerqueue_t	_event_q;
#if defined(NEW_IO) && defined(_WIN32)
    static win32_event_handler_t	*_io_events;
#else
    static sfile_handler_t	*_io_events;
#endif
    static bool			in_cs;

#ifdef STHREAD_IO_DISKRW
    static sdisk_handler_t	*sdisk_handler;    
#else
    static char			*disk_buffer;
#endif

    static const char*		_diskrw;	// name of diskrw executable
    static	int		_io_in_progress;

    /* Control for idle thread dynamically changing priority */
    static int			_idle_priority_phase;
    static int			_idle_priority_push;
    static int			_idle_priority_max;
};


extern ostream &operator<<(ostream &o, const sthread_t &t);

void print_timeout(ostream& o, const sthread_base_t::timeout_in_ms timeout);


class sthread_main_t : public sthread_t  {
    friend class sthread_t;
	
protected:
    NORET			sthread_main_t();
    virtual void		run();
};

class sthread_idle_t : public sthread_t {
private:
    friend class sthread_t;

protected:
    NORET			sthread_idle_t();
    virtual void		run();
};

class sthread_priority_list_t 
    : public w_descend_list_t<sthread_t, sthread_t::priority_t>  {
public:
    NORET			sthread_priority_list_t(); 
	
    NORET			~sthread_priority_list_t()   {};
private:
    // disabled
    NORET			sthread_priority_list_t(
	const sthread_priority_list_t&);
    sthread_priority_list_t&	operator=(const sthread_priority_list_t&);
};

/*
 *  Mutual Exclusion
 */
class smutex_t : public sthread_named_base_t {
public:
    NORET			smutex_t(const char* name = 0);
    NORET			~smutex_t();
#ifdef W_DEBUG
    w_rc_t			acquire(int4_t timeout = WAIT_FOREVER);
#else
    w_rc_t			acquire(int4_t timeout);
    w_rc_t			acquire();
#endif
    void			release();
    bool			is_mine() const;
    bool			is_locked() const;

private:
    sthread_t*			holder;	// owner of the mutex
    sthread_priority_list_t	waiters;

    // disabled
    NORET			smutex_t(const smutex_t&);
    smutex_t&			operator=(const smutex_t&);
};

#ifndef NOT_PREEMPTIVE
#define MUTEX_ACQUIRE(mutex)    W_COERCE((mutex).acquire());
#define MUTEX_RELEASE(mutex)    (mutex).release();
#define MUTEX_IS_MINE(mutex)    (mutex).is_mine()
#else
#define MUTEX_ACQUIRE(mutex)
#define MUTEX_RELEASE(mutex)
#define MUTEX_IS_MINE(mutex)    1
#endif

/*
 *  Condition Variable
 */
class scond_t : public sthread_named_base_t  {
public:
    NORET			scond_t(const char* name = 0);
    NORET			~scond_t();

    w_rc_t			wait(
	smutex_t& 		    m, 
	timeout_in_ms		    timeout = WAIT_FOREVER);

    void 			signal();
    void 			broadcast();
    bool			is_hot() const;

private:
    sthread_priority_list_t	_waiters;

    // disabled
    NORET			scond_t(const scond_t&);
    scond_t&			operator=(const scond_t&);
};

/*
 *  event semaphore
 */
class sevsem_t : public sthread_base_t {
public:
    NORET			sevsem_t(
	int 			    is_post = 0,
	const char* 		    name = 0);
    NORET			~sevsem_t();

    w_rc_t			post();
    w_rc_t			reset(int* pcnt = 0);
    w_rc_t			wait(timeout_in_ms timeout = WAIT_FOREVER);
    void 			query(int& pcnt);
    
    void	 		setname(
	const char* 		    n1, 
	const char* 		    n2 = 0);
private:
    smutex_t 			_mutex;
    scond_t 			_cond;
    uint4_t			_post_cnt;

    // disabled
    NORET			sevsem_t(const sevsem_t&);
    sevsem_t&			operator=(const sevsem_t&);
};

inline bool
smutex_t::is_mine() const
{ 
    return holder == sthread_t::me(); 
}

inline bool
smutex_t::is_locked() const
{
    return holder != 0;
}

/*
 *  Internal --- responsible for initialization
 */
class sthread_init_t : public sthread_base_t {
public:
    NORET			sthread_init_t();
    NORET			~sthread_init_t();
    static w_base_t::int8_t 	max_os_file_size;
private:
    static uint4_t 		count;
};

//////////////////////////////////////////////////////
// In sthread_core.cpp sthread_init_t must be the 2nd
// thing initialized; in every other file it
// must be the first.
//////////////////////////////////////////////////////
#ifndef STHREAD_CORE_C
static sthread_init_t sthread_init;
#endif /*STHREAD_CORE_C*/


inline sthread_t::priority_t
sthread_t::priority() const
{
    return _priority;
}

inline sthread_t::status_t
sthread_t::status() const
{
    return _status;
}

inline bool
scond_t::is_hot() const 
{
    return ! _waiters.is_empty();
}

inline void
sevsem_t::setname(const char* n1, const char* n2)
{
    _mutex.rename("s:m:", n1, n2);
    _cond.rename("s:c:", n1, n2);
}


inline    void	sthread_t::push_resource_alloc(const char* n,
					       const void *id,
					       bool is_latch)
{
	_trace.push(n, id, is_latch);
}
				
inline	void	sthread_t::pop_resource_alloc(const void *id)
{
	_trace.pop(id);
}

/*<std-footer incl-file-exclusion='STHREAD_H'>  -- do not edit anything below this line -- */

#endif          /*</std-footer>*/

/*<std-header orig-src='shore'>

 $Id: sthread.cpp,v 1.308 2001/06/25 20:01:08 bolo Exp $

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

#define STHREAD_C

#define W_INCL_LIST
#define W_INCL_SHMEM
#define W_INCL_TIMER
#include <w.h>

#include <w_debug.h>
#include <w_stream.h>
#include <w_signal.h>
#include <stdlib.h>
#ifndef _WINDOWS
#include <unistd.h>
#endif
#include <string.h>

#ifdef _WINDOWS
#include <time.h>
#else
#include <sys/time.h>
#endif

#ifndef _WINDOWS
#include <sys/wait.h>
#endif
#include <new.h>

#include <sys/stat.h>
#include <w_rusage.h>

#if defined(AIX32) || defined(AIX41)
static void hack_aix_signals();
#endif

#ifndef _WINDOWS
/* XXX In general signal handling needs to be improved.  Until then,
   we do the best we can to ignore it. */ 
static	void	hack_signals();
#endif

#ifdef __GNUC__
#pragma implementation "sthread.h"
#endif


#include <w_statistics.h>

#include <unix_stats.h>

#include "sthread.h"
#include "sthread_stats.h"
#include "spin.h"
#include "ready_q.h"

/* Choose an appropriate thread core.  These will be classes  ...
   one of these days. */
#if !defined(STHREAD_CORE_PTHREAD) && !defined(STHREAD_CORE_WIN32)
#define	STHREAD_CORE_STD
#endif

#if defined(STHREAD_CORE_PTHREAD)
#include "stcore_pthread.h"
#elif defined(STHREAD_CORE_WIN32)
#include "stcore_win32.h"
#elif defined(STHREAD_CORE_STD)
#include "stcore.h"
#else
#error "no thread core selected"
#endif

#ifdef PURIFY
#include <purify.h>
#endif

#ifdef EXPLICIT_TEMPLATE
template class w_list_t<sthread_t>;
template class w_list_i<sthread_t>;
template class w_descend_list_t<sthread_t, sthread_t::priority_t>;
template class w_keyed_list_t<sthread_t, sthread_t::priority_t>;
#endif

#ifdef _WINDOWS
extern w_rc_t init_winsock();
#endif


//////////////////////////////////////////////
// YOU MUST MAKE  
// W_FASTNEW_STATIC_DECL
// the first thing in the file so that it 
// gets constructed before any threads do!
//
//////////////////////////////////////////////
W_FASTNEW_STATIC_PTR_DECL(sthread_name_t);

class sthread_stats SthreadStats;

const
#include "st_einfo_gen.h"

extern "C" void dumpthreads();
extern "C" void threadstats();

bool sthread_t::isStackOK(const char *file, int line)
{
	if (!_stack_checks)	/* incase called by user code */
		return true;

	sthread_core_t *from_core = this->_core;

	bool ok;
	ok = sthread_core_stack_ok(from_core, (this==me()) ? 2:0);

	if (!ok)
		cerr << "*** Stack corruption at " 
			<< file << ":" << line 
			<< " in thread " << me()->id << " ***" << endl
			<< *this << endl;

	return ok;
}


/* check all threads */
void sthread_t::check_all_stacks(const char *file, int line)
{
	if (!_stack_checks)	/* incase called by user code */
		return;

	w_list_i<sthread_t> i(*_class_list);
	unsigned	corrupt = 0;

	while (i.next())  {
		if (! i.curr()->isStackOK(file, line))
			corrupt++;
	}

	if (corrupt > 0) {
		cerr << "sthread_t::check_all: " << corrupt 
		<< " thread stacks, dieing" << endl;
		W_FATAL(fcINTERNAL);
	}
}


/* Give an estimate if the stack will overflow if a function with
   a stack frame of the requested size is called. */

bool	sthread_t::isStackFrameOK(unsigned size)
{
	/* Choose a default stack frame allocation based upon random
	   factors not discussed here. */
	if (size == 0)
		size = 1024;

#if defined(STHREAD_CORE_STD)
	return sthread_core_stack_frame_ok(_core, size, _me->_core == _core);
#else
	return true;
#endif
}


/*********************************************************************
 *
 *  class sthread_timer_t
 *
 *  Times out threads that are blocked on a timeout.
 *
 *********************************************************************/

class sthread_timer_t : public w_timer_t, public sthread_base_t {
public:
	sthread_timer_t(const stime_t &when, sthread_t &t)
		: w_timer_t(when), _thread(t)    {}

	void		trigger();

private:
	sthread_t	&_thread;
};


void sthread_timer_t::trigger()
{
	if (_thread.status() == sthread_t::t_blocked)  {
		W_COERCE( _thread.unblock(RC(stTIMEOUT)) );
	}
}


/*********************************************************************
 *
 *  Class static variable intialization
 *
 *********************************************************************/


sthread_t* 		sthread_t::_me;
sthread_t* 		sthread_t::_idle_thread = 0;
sthread_t*		sthread_t::_main_thread = 0;
w_base_t::uint4_t	sthread_t::_next_id = 0;
ready_q_t*		sthread_t::_ready_q = 0;
sthread_list_t*		sthread_t::_class_list = 0;

#ifdef W_DEBUG
bool			sthread_t::_stack_checks = true;
#else
bool			sthread_t::_stack_checks = false;
#endif

/*
  The "idle" thread is really more than a "run when nothing else is"
  thread.  It is also responsible for ALL event generation in the
  system, including IO completions (diskrw), descriptor I/O ready
  (sfile), and timeouts (stimer).  If high priority threads are
  always scheduled, events which they are waiting upon may never be
  generated.

  To combat this problem, the context switcher will automagically
  increase the priority of the idle thread, which guarantees that it
  will eventually have a chance to run.  Once the idle thread starts,
  it will reduce its' priority to idle_time, so that it doesn't
  consume thread time continuously.

  The maximum priority which the idle thread will climb to is
  'idle_priority_max'.  'idle_priority_push' and 'idle_priority_phase'
  implement a mechanism to boost the priority of the idle thread by a
  priority level after every 'idle_priority_push' context switches.  */

int	sthread_t::_idle_priority_phase = 0;
int	sthread_t::_idle_priority_push = 10;
int	sthread_t::_idle_priority_max = max_priority;


#if defined(W_DEBUG) && defined(_WINDOWS)

#ifdef extreme_problems
// This function is used for debugging to verify that signals are blocked.
static int sthread_signals_are_blocked()
{
    sigset_t checkset;

    sigprocmask(SIG_SETMASK, 0, &checkset);
    return sigismember(&checkset, SIGUSR2);
}
#endif	

#if !defined(HPUX8) && !defined(Irix)
#define	HAVE_SYS_SIGLIST
#endif

#ifdef HAVE_SYS_SIGLIST
/* funny names? */
#ifdef SOLARIS2
#define	sys_siglist	_sys_siglist
#endif

/* create a prototype if necessary */
#if !defined(SOLARIS2) && !defined(Linux)
extern const char *sys_siglist[];
#endif

#ifndef _WINDOWS
void print_sigmask()
{
	int		i;
	sigset_t	mask;
	bool		blocked = false;
	
	sigprocmask(SIG_BLOCK, 0, &mask);
	for (i = 0; i < NSIG; i++) {
		if (sigismember(&mask, i)) {
			blocked = true;
			break;
		}
	}
	if (blocked) {
		cout << "blocked signals:";
		for (i = 1; i < NSIG; i++)
			if (sigismember(&mask, i)) {
#ifdef HAVE_SYS_SIGLIST
				cout << " '" << sys_siglist[i] <<  "'";
#else
				cout.form(" 'signal_%d'", i); 
#endif
			}
	}
	else
		cout << "no signals blocked";
	cout << endl;
}
#endif /* !_WINDOWS */
#endif /* HAVE_SYS_SIGLIST */
#endif


/*
 *  sthread_t::_ctxswitch(status)
 *
 *  Set the status of the current thread and switch to the
 *  next ready thread.
 */

void sthread_t::_ctxswitch(status_t s)
{
    sthread_t *doner = _me;		/* giving up CPU */
    sthread_t *taker = _ready_q->get();	/* taking up CPU */
    sthread_t *dieing = 0;

    ++SthreadStats.ctxsw;

    /*
       Don't bother changing the priority of the idle thread if it
       has just run, or is just about to run.
       1) It has/will run recently.
       2) It won't be in the ready_q, and the change_priority() will fail.
     */  
    if (_idle_thread
	&&  doner != _idle_thread && taker != _idle_thread
	&&  _idle_priority_phase++ > _idle_priority_push) {

	int old_pri = _idle_thread->_priority;
	int new_pri = old_pri + 1;

	if (old_pri > _idle_priority_max)
		new_pri = old_pri;
	else if	(new_pri > _idle_priority_max)
		new_pri = _idle_priority_max;

	if (new_pri != old_pri) {
#if 0
		W_FORM(cout)("== Change priority of idle thread from %d to %d ==\n",
			  _idle_thread->_priority, new_pri); 
#endif
		_ready_q->change_priority(_idle_thread, (priority_t) new_pri);
	}
	_idle_priority_phase = 0;
    }

    /* If there are no other threads in the ready queue, schedule
       the current thread.  If the current thread is exiting, this
       must be undone.  Another problem occurs if the thread is not
       runnable.  This isn't checked for! XXX */
    if (!taker)
	taker = doner;

    switch (s) {
    case t_defunct:
	/* The current thread is exiting.  There MUST be another thread to
	   switch to */
	w_assert1(taker != _me);	

	if (_me) {
	    /* XXX if a context switch occurs here, threads break */
	    W_COERCE( _me->_terminate->post() );
	}

	dieing = _me;
	break;

    case t_ready:
	/* Insert the current thread into the ready queue. */
	doner->_status = s;
	if (taker != doner) _ready_q->put(doner);
	break;

    case t_blocked:
	/* Set status. Don't insert thread into ready queue. */
	doner->_status = s;
	break;

    case t_boot:
	/* bootstrap the main thread */
	w_assert1(taker);
	w_assert1(doner == 0);
	w_assert1(taker->_status == t_ready);
	/* Try to make the main thread have the common cpu modes */
	taker->_reset_cpu();
	doner = taker;
	break;

    default:
	W_FATAL(stINTERNAL);
    }

#if STHREAD_STACK_CHECK > 1 || defined(W_DEBUG)
    if (_stack_checks) {
	if (sthread_t::_next_id > 2)
		check_all_stacks(__FILE__, __LINE__);
    }
#endif


#if defined(STHREAD_STACK_CHECK)  || defined(W_DEBUG)
    /* Check for stack overflow at thread switch or yield.  If you
       want really wicked fast performance, you could turn this
       off.  This is always the FAST stack check, a more
       comprehensive job is done by check_all if requested. */
    if (doner && !sthread_core_stack_ok(doner->_core, 1)) {
	    cerr << "*** Stack overflow at thread switch from" 
		<< endl << *doner << endl;
	    W_FATAL(fcINTERNAL);
    }
#endif

    /*
     *  Context switch --- change identity
     */
    (_me = taker)->_status = t_running;

    if (doner != taker)  {
	    sthread_core_switch(doner->_core, taker->_core);
    }
    
    /* Current thread resumes after context switch -- 
     * this means that _me has changed back since we set it.
     *
     * NB: this assertion fails if we are calling
     * thread switch from gdb from an arbitrary place, and
     * we don't switch back before we "continue", 
     * because it didn't come through this function.
     */
    w_assert3(_me == doner && _me->_status == t_running);
}


stime_t	sthread_t::boot_time = stime_t::now();


/*
 * sthread_t::startup()
 *
 * Initialize system threads from cold-iron.  The main thread will run
 * on the stack startup() is called on.
 */

w_rc_t	sthread_t::startup()
{
	_ready_q = new ready_q_t;
	if (_ready_q == 0)
		W_FATAL(fcOUTOFMEMORY);

	_class_list = new sthread_list_t(offsetof(sthread_t, _class_link));
	if (_class_list == 0)
		W_FATAL(fcOUTOFMEMORY);

	const char *s = getenv("STHREAD_STACK_CHECK");
	if (s && *s)
		_stack_checks = (atoi(s) != 0);

	W_COERCE(startup_events());

	W_COERCE(startup_io());

	/*
	 * Boot the main thread onto the current (system) stack.
	 */
	sthread_main_t *main = new sthread_main_t;
	if (!main)
		W_FATAL(fcOUTOFMEMORY);
	W_COERCE( main->fork() );
	_ctxswitch(t_boot);
	if (_me != main)
		W_FATAL(stINTERNAL);
	_main_thread = main;

#if defined(PURIFY)
	/* The main thread is different from all other threads. */
	purify_name_thread(me()->name());
#endif	

	/*
	 *  Bring the idle thread to life.
	 */
	sthread_t *idle = new sthread_idle_t;
	if (!idle)
		W_FATAL(fcOUTOFMEMORY);
	W_COERCE( idle->fork() );
	_idle_thread = idle;

	yield();	// force the idle thread to start
#ifndef _WINDOWS
	w_assert3(in_cs);
#endif

	return RCOK;
}

/*
 * sthread_t::shutdown()
 *
 * Shutdown the thread system.  Must be called from the context of
 * the main thread.
 *
 * XXX notyet
 */

w_rc_t sthread_t::shutdown()
{
	if (_me != _main_thread) {
		cerr << "sthread_t::shutdown(): not main thread!" << endl;
		return RC(stINTERNAL);
	}

	/* wait for other threads */

	/* shutdown disk i/o */
	W_COERCE(shutdown_io());

	_me = 0;
	return RCOK;
}


/*
 *  sthread_main_t::sthread_main_t()
 *
 * Create the main thread.  It is a placeholder for the
 * thread which uses the system stack.
 */

sthread_main_t::sthread_main_t()
: sthread_t(t_regular, "main_thread", 0)
{
}


/*
 *  sthread_main_t::run()
 *
 *  This is never called.  It's an artifact of the thread architecture.
 */

void sthread_main_t::run()
{
}


/*
 *  sthread_t::_caught_signal(sig)
 *
 *  Handle signals.
 */
 
void sthread_t::_caught_signal(int sig)
{
    // cerr << "_caught_signal " << dec << sig << endl;

    /* cure for a bogus gcc-2.7.2 warning message */
    sig = sig;

#if 0
    _caught_signal_io(sig);
#endif
   
    w_assert3(!in_cs);
    /* nothing more process */
}


/*
 *  sthread_t::set_use_float(flag)
 *
 *  Current thread do not make floating point calculations if
 *  flag is TRUE. FALSE otherwise.
 * 
 *  Optimization. We do not need to save some floating point 
 *  registers when context switching.
 */

w_rc_t sthread_t::set_use_float(int flag)
{
	sthread_core_set_use_float(_core, flag ? 1 : 0);
	return RCOK;
}



/*
 *  sthread_t::set_priority(priority)
 *
 *  Sets the priority of current thread.  The thread must not
 *  be in the ready Q.
 */

w_rc_t sthread_t::set_priority(priority_t priority)
{
	_priority = priority;

	// in this statement, <= is used to keep gcc -Wall quiet
	if (_priority <= min_priority) _priority = min_priority;
	if (_priority > max_priority) _priority = max_priority;

	if (_status == t_ready)  {
		cerr << "sthread_t::set_priority()  :- "
			<< "cannot change priority of ready thread" << endl;
		W_FATAL(stINTERNAL);
	}

	return RCOK;
}



/*
 *  sthread_t::sleep(timeout)
 *
 *  Sleep for timeout milliseconds.
 */

void sthread_t::sleep(timeout_in_ms timeout, const char *reason)
{
	reason = (reason && *reason) ? reason : "sleep";

	W_IGNORE(block(timeout,  0, reason, this));
}

/*
 *  sthread_t::wakeup()
 *
 *  Cancel sleep 
 */

void sthread_t::wakeup()
{
    if(_tevent) _tevent->trigger();
}


/*
 *  Wait for this thread to end. This method returns when this thread
 *  end or when timeout expires.
 */  

w_rc_t
sthread_t::wait(timeout_in_ms timeout)
{
	/* A thread that hasn't been forked can't be wait()ed for.
	   It's not a thread until it has been fork()ed. */
	if (_status == t_virgin)
		return RC(stOS);

	/*
	 *  Wait on _terminate sevsem. This sevsem is posted when 
	 *  the thread ends.
	 */
	return _terminate->wait(timeout);
}


/*
 * sthread_t::fork()  
 *
 * Turn the "chunk of memory" into a real-live thread.
 */

w_rc_t	sthread_t::fork()
{
	/* can only fork a new thread */
	if (_status != t_virgin)
		return RC(stOS);

	_status = t_ready;

	/* Add us to the list of threads */
	_class_list->append(this);

	/* go go gadget ... schedule it */
	_ready_q->put(this);

#ifdef notyet	/* XXX need special case for system boot */
	/* And give it a chance to start running */
	yield();
#endif

	return RCOK;
}


/*
 *  sthread_t::sthread_t(priority, name)
 *
 *  Create a thread.  Until it starts running, a created thread
 *  is just a memory object.
 */

sthread_t::sthread_t(priority_t		pr,
		     const char 	*nm,
		     unsigned		stack_size)
: sthread_named_base_t(nm),
  user(0),
  id(_next_id++),
#ifdef W_TRACE
  trace_level(_debug.trace_level()),
#else
  trace_level(0),
#endif
  _blocked(0),
  _terminate(new sevsem_t(0, "terminate")),
  _core(0),
  _status(t_virgin),
  _priority(pr), 
  _ready_next(0),
  _tevent(0),
  _bytes_allocated(0), _allocations(0), _high_water_mark(0)
{
	if (!_terminate)
		W_FATAL(fcOUTOFMEMORY);

	_core = new sthread_core_t;
	if (!_core)
		W_FATAL(fcOUTOFMEMORY);
	_core->thread = (void *)this;
	
	/*
	 *  Set a valid priority level
	 */
	if (_priority > max_priority) 
		_priority = max_priority;
	else if (_priority <= min_priority) 
		_priority = min_priority;
	
	/*
	 *  Initialize the core.
	 */
	if (sthread_core_init(_core, __start, this, stack_size) == -1) {
		cerr << "sthread_t: cannot initialize thread core" << endl;
		W_FATAL(stINTERNAL);
	}
}



/*
 *  sthread_t::~sthread_t()
 *
 *  Destructor. Thread must have already exited before its object
 *  is destroyed.
 */

sthread_t::~sthread_t()
{
	/* Valid states for destroying a thread are ...
	   1) It was never started
	   2) It ended.
	   3) There is some braindamage which imply that blocked threads
	   can be deleted.  This is sick and wrong, and it
	   can cause race conditions.  It is enables for compabilitiy,
	   and hopefully the warning messages will tell you if
	   something is wrong. */
	w_assert1(_status == t_virgin
		  || _status == t_defunct
		  || _status == t_blocked);

	/* XXX make sure the thread isn't on the ready Q, or
	   it isn't on a blocked list */

	if (_ready_next) {
		W_FORM2(cerr,("sthread_t(%#lx): \"%s\": destroying a thread on the ready queue!",
			  (long)this, name()));
	}

	if (_link.member_of()) {
		W_FORM2(cerr,("sthread_t(%#lx): \"%s\": destroying a thread on a list!",
			  (long)this, name()));
	}



	if (_blocked) {
		W_FORM2(cerr, ("sthread_t(%#lx): \"%s\": destroying a blocked thread!",
			  (long)this, name()));
		cerr << *_blocked << endl;

		_trace.release(_blocked);
		_blocked = 0;
	}

	sthread_core_exit(_core);

	delete _core;
	_core = 0;

	if (_rc)  {/*nothing*/;}
	delete _terminate;
}


/* A springboard from "C" function + argument into an object */
void	sthread_t::__start(void *arg)
{
	sthread_t &t = *(sthread_t *)arg;
	t._start();
}


/*
 *  sthread_t::_start(t)
 *
 *  All threads start and end here.
 */

void sthread_t::_start()
{
	w_assert1(_me == this);

	/* the context switch should have changed the status ... */
	w_assert3(_status == t_running);

#if defined(PURIFY)
	/* threads should be named in the constructor, not
	   in run() ... this is mostly useless if that happens */
	purify_name_thread(name());
#endif

	/* Setup whatever machine dependent modes we want all threads
	   to have beyond the default provided by the core switcher. */
	_reset_cpu();

	/* Save an exit jmpbuf and call run().  Either run() returns
	   or sthread_t::end() longjmp()'s here to terminate the thread */
	if (thread_setjmp(_exit_addr) == 0) {
		/* do not save sigmask */
		w_assert1(_me == this);
#ifdef STHREAD_CXX_EXCEPTION
		/* Provide a "backstop" exception handler to catch uncaught
		   exceptions in the thread.  This prevents them from going
		   into never-never land. */
		try {
			run();
		}
		catch (...) {
			cerr << endl
			     << "sthread_t(id = " << id << "  name = " << name()
			     << "): run() threw an exception."
#if defined(W_DEBUG)
			     << endl << *this
#endif
			     << endl
			     << endl;
		}
#else
		run();
#endif
	}

	/* Returned from run(). Current thread is ending. */
	w_assert3(_me == this);
	_status = t_defunct;
	_link.detach();
	_class_link.detach();

	/* An ordinary thread is ending, kill deschedule ourself
	   and run any ready thread */
	w_assert3(this == _me);
	_ctxswitch(t_defunct);

	W_FATAL(stINTERNAL);	// never reached
}



/*********************************************************************
 *
 *  sthread_t::block(timeout, list, caller)
 *
 *  Block the current thread and puts it on list. 
 *
 *********************************************************************/
w_rc_t
sthread_t::block(
    timeout_in_ms	timeout,
    sthread_list_t*	list,		// list for thread after blocking
    const char* const	caller,		// for debugging only
    const void		*id)
{

    /*
     *  Sanity checks
     */
    w_assert3(_me->_link.member_of() == 0); // not in other list
    w_assert1(timeout != WAIT_IMMEDIATE);   // not 0 timeout

    /*
     *  Save caller name
     */
    _me->_blocked = _me->_trace.get(caller, id);

    /*
     *  Put on list
     */
    if (list)  {
	list->put_in_order(_me);
    }

    /*
     *  See if timeout is < 10; 10 is too small for some
     *  slow machine.
     *  remember timeout could be < 0(WAIT_FOREVER, etc)
     */
    if (timeout > WAIT_IMMEDIATE && timeout < 10)  {
	timeout = 10;        // make it 10
    }

    /*
     *  Allocate the timer on the stack.
     *  XXX This is a hack to avoid the cost
     *  of the constructor, which WAS magic, but no longer is.
     */
    char buf[ SIZEOF(sthread_timer_t) ];
    sthread_timer_t*		tevent = _me->_tevent;

    /*
       * WAIT_IMMEDIATE, WAIT_FOREVER are not times, and
       should be treated differently than time-outs.
       Does WAIT_IMMEDIATE imply a context switch?

       * What exactly do relative timeouts mean?
     */

    if (timeout >= 10)  {
	    stime_t when(stime_t::now() + stime_t::msec(timeout));

	    tevent = _me->_tevent = new (buf) sthread_timer_t(when, *_me);
	    _event_q.schedule(*tevent);
    }
    
    /*
     *  Give up CPU
     */
    _ctxswitch(t_blocked);

    /*
     *  Resume
     */
    if ( (tevent = _me->_tevent) )  {
	    if (!tevent->fired()) {
		    /* Someone called sthread_t::wakeup() before
		     * timer went off 
		     */
		    _event_q.cancel(*tevent);
	    }
	    tevent->sthread_timer_t::~sthread_timer_t();

	    // when buf goes out of scope soon, taking *_me->_tevent
	    _me->_tevent = 0;
    }

    return _me->_rc;
}



/*********************************************************************
 *
 *  sthread_t::unblock(rc)
 *
 *  Unblock the thread with an error in rc.
 *
 *********************************************************************/
w_rc_t
sthread_t::unblock(const w_rc_t& rc)
{
    w_assert3(_me != this);
    w_assert3(_status == t_blocked);

    _link.detach();
    _status = t_ready;

    if (_blocked) {
	    _trace.release(_blocked);
	    _blocked = 0;
    }

    /*
     *  Save rc (will be returned by block())
     */
    if (_rc)  {;}
    if (&rc) 
	_rc = rc;
    else
	_rc = RCOK;

    /*
     *  Thread is again ready.
     */
    _ready_q->put(this);

    return RCOK;
}



/*********************************************************************
 *
 *  sthread_t::yield()
 *  if do_select==true, we'll allow a select w/ 0 timeout
 *  to occur if the ready queue is empty
 *
 *  Give up CPU. Maintain ready status.
 *
 *********************************************************************/
void sthread_t::yield(bool do_select) // default is false
{
    w_assert3(_me->_status == t_running);

    if(do_select) {
	// we put the idle thread on the ready 
	// queue ahead of us
	w_assert3(_idle_thread != 0);
	_ready_q->change_priority(_idle_thread, _me->_priority);
    }
    _ctxswitch(t_ready);
    return;
}


/*
 *  sthread_t::end()
 *
 *  End thread prematurely (vis-a-vis naturally by returning 
 *  from run()). 
 */
void sthread_t::end()
{
	/* The main thread is special; for now, end()ing it is
	   equivalent to process termination.  Once shutdown() is
	   implemented, this could be wait for all other threads to
	   terminate before exiting. */
	if (_me->id == 0) {
		cout << "sthread_t::end(): process exit()ing via"
			<< " main thread end()ing." << endl;
		::exit(0);
	}
	
	/* return to cleanup code in start() */
	thread_longjmp(_me->_exit_addr, -1);
}


/*********************************************************************
 *
 *  sthread_t::push_resource_alloc(const char* n, void *id, bool isLatch)
 *  sthread_t::pop_resource_alloc(void *id)
 *  sthread_t::dump(str, o)
 *
 *  For debugging.
 *
 *********************************************************************/

#if 0
/* this left here in case a non-inline version is needed */

void
sthread_t::push_resource_alloc(const char* n, void *id, bool isLatch)
{
	_trace.push(n, id, isLatch);
}

void
sthread_t::pop_resource_alloc(void *id)
{
	_trace.pop(id);
}

#endif


/* print all threads */
void sthread_t::dump(const char *str, ostream &o)
{
	if (str)
		o << str << ": " << endl;

	dump(o);
}

void sthread_t::dump(ostream &o)
{
	w_list_i<sthread_t> i(*_class_list);

	while (i.next())  {
		o << "******* ";
		if (_me == i.curr())
			o << " --->ME<---- ";
		o << endl;

		i.curr()->_dump(o);
	}
}


/* XXX individual thread dump function... obsoleted by print method */
void sthread_t::_dump(ostream &o) const
{
	o << *this << endl;
}

/* XXX it is not a bug that you can sometime see >100% cpu utilization.
   Don't even think about hacking something to change it.  The %CPU
   is an *estimate* developed by statistics gathered by the process,
   not something solid given by the kernel. */

static void print_time(ostream &o, const sinterval_t &real,
		       const sinterval_t &user, const sinterval_t &kernel)
{
	sinterval_t	total(user + kernel);
	double	pcpu = ((double)total / (double)real) * 100.0;
	double 	pcpu2 = ((double)user / (double)real) * 100.0;

	o << "\t" << "real: " << real
		<< endl;
	o << "\tcpu:"
		<< "  kernel: " << kernel
		<< "  user: " << user
		<< "  total: " << total
		<< endl;
	o << "\t%CPU:"
		<< " " << setprecision(3) << pcpu
		<< "  %user: " << setprecision(2) << pcpu2
		<< endl;
}

void sthread_t::dump_stats(ostream &o)
{
	o << SthreadStats;
	dump_stats_io(o);

	/* To be moved somewhere else once I put some other infrastructure
	   into place.  Live with it in the meantime, the output is really
	   useful for observing ad-hoc system performance. */
#ifdef _WIN32
	/* DISGUSTING */
	FILETIME	times[4];
	timeval	tv[2];
	bool	ok;
	ok = GetProcessTimes(GetCurrentProcess(), times, times+1, times+2, times+3);
	if (!ok) {
		w_rc_t e = RC(fcWIN32);
		cerr << "GetProcessTimes():" << endl << e << endl;
		return;
	}
	stime_t	now(stime_t::now());

	extern void filetime2timeval(FILETIME &, timeval &);

	filetime2timeval(times[2], tv[0]);
	filetime2timeval(times[3], tv[1]);
	sinterval_t	kernel(tv[0]);
	sinterval_t	user(tv[1]);
	sinterval_t	real(now - boot_time);
	sinterval_t	total(user + kernel);
	sinterval_t	idle(SthreadStats.idle_time);
#else
	struct	rusage	ru;
	int	n;

	stime_t	now(stime_t::now());
	n = getrusage(RUSAGE_SELF, &ru);
	if (n == -1) {
		w_rc_t	e = RC(fcOS);
		cerr << "getrusage() fails:" << endl << e << endl;
		return;
	}

	sinterval_t	real(now - boot_time);
	sinterval_t	kernel(ru.ru_stime);
	sinterval_t	user(ru.ru_utime);
#endif

	/* Try to provide some modicum of recent cpu use. This will eventually
	   move into the class, once a "thread handler" arrives to take
	   care of it. */
	static	sinterval_t	last_real;
	static	sinterval_t	last_kernel;
	static	sinterval_t	last_user;
	static	bool last_valid = false;

	o << "TIME:" << endl;
	print_time(o, real, user, kernel);
	if (last_valid) {
		sinterval_t	r(real - last_real);
		sinterval_t	u(user - last_user);
		sinterval_t	k(kernel - last_kernel);
		o << "RECENT:" << endl;
		print_time(o, r, u, k);
	}
	else
		last_valid = true;

	last_kernel = kernel;
	last_user = user;
	last_real = real;

	o << endl;
}

void sthread_t::reset_stats()
{
	SthreadStats.clear();
	reset_stats_io();
}


const char *sthread_t::status_strings[] = {
	"defunct",
	"virgin",
	"ready",
	"running",
	"blocked",
	"boot"
};

const char *sthread_t::priority_strings[]= {
	"idle_time",
	"fixed_low",
	"regular",
	"time_critical"
};


ostream& operator<<(ostream &o, const sthread_t &t)
{
	return t.print(o);
}


/*
 *  sthread_t::print(stream)
 *
 *  Print thread status to an stream
 */
ostream &sthread_t::print(ostream &o) const
{
	o << "thread id = " << id ;

	if (name()) {
		o << ", name = " << name();
	};

	o
	<< ", addr = " <<  (void *) this
	<< ", core = " <<  (void *) _core << endl;
	o
	<< "priority = " << sthread_t::priority_strings[priority()]
	<< ", status = " << sthread_t::status_strings[status()];
	o << endl;	      

	o << *_core << endl;

	if (user)
		o << "user = " << user << endl;
	
	if (status() == t_blocked && _blocked)
		o << "blocked on: " << *_blocked;
	
	_trace.print(o);
	
	if (status() != t_defunct  &&  !sthread_core_stack_ok(_core, 0))
		cerr << "***  warning:  Thread stack overflow  ***" << endl;
	
	if(_high_water_mark != 0) {
	    o << "heap: " 
		<< _bytes_allocated << " bytes allocated in "
		<< _allocations << " calls " 
		<< (_allocations * w_fastnew_t::overhead() ) << 
			" bytes overhead" 
		<< " high water mark=" << _high_water_mark
		<<endl;
	}
	return o;
}



/*********************************************************************
 *
 *  smutex_t::for_each_thread(ThreadFunc& f)
 *
 *  For each thread in the system call the function object f.
 *
 *********************************************************************/
void sthread_t::for_each_thread(ThreadFunc& f)
{
    w_list_i<sthread_t> i(*_class_list);

    while (i.next())  {
	f(*i.curr());
    }
}

void print_timeout(ostream& o, const sthread_base_t::timeout_in_ms timeout)
{
    if (timeout > 0)  {
	o << timeout;
    }  else if (timeout >= -5)  {
	static const char* names[] = {"WAIT_IMMEDIATE",
				      "WAIT_FOREVER",
				      "WAIT_ANY",
				      "WAIT_ALL",
				      "WAIT_SPECIFIED_BY_THREAD",
				      "WAIT_SPECIFIED_BY_XCT"};
	o << names[-timeout];
    }  else  {
	o << "UNKNOWN_TIMEOUT_VALUE(" << timeout << ")";
    }
}



/*********************************************************************
 *
 *  smutex_t::smutex_t(name)
 *
 *  Construct a mutex. Name is used for debugging.
 *
 *********************************************************************/
smutex_t::smutex_t(const char* nm)
    : holder(0), waiters()
{
    if (nm) rename(nm?"m:":0, nm);
}



/*********************************************************************
 *
 *  smutex_t::~smutex_t()
 *
 *  Destroy the mutex. There should not be any holder.
 *
 *********************************************************************/
smutex_t::~smutex_t()
{
    w_assert1(!holder);
}



/*********************************************************************
 *
 *  smutex_t::acquire(timeout)
 *
 *  Acquire the mutex. Block and wait for up to timeout milliseconds
 *  if some other thread is holding the mutex.
 *
 *********************************************************************/
w_rc_t 
smutex_t::acquire(int4_t timeout)
{
    // FUNC(smutex_t::acquire);
    sthread_t* self = sthread_t::me();


    w_rc_t ret;
    if (! holder) {
	/*
	 *  No holder. Grab it.
	 */
	w_assert3(waiters.num_members() == 0);
	holder = self;
    } else {
	/*
	 *  Some thread holding this mutex. Block and wait.
	 */
	if (self == holder)  {
	    cerr << "fatal error -- deadlock while acquiring mutex";
	    if (name()) {
		cerr << " (" << name() << ")";
	    }
	    cerr << endl;
	    W_FATAL(stINTERNAL);
	}

	if (timeout == WAIT_IMMEDIATE)
	    ret = RC(stTIMEOUT);
	else  {
	    DBGTHRD(<<"block on mutex " << this);
	    SthreadStats.mutex_wait++;
	    ret = sthread_t::block(timeout, &waiters, name(), this);
	}
    }
#if defined(W_DEBUG) || defined(SHORE_TRACE)
    if (! ret.is_error()) {
	self->push_resource_alloc(name(), this);
    }
#endif 
    return ret;
}

#ifndef W_DEBUG
/* Fast path version of acquire(). */

w_rc_t smutex_t::acquire()
{
    if (holder) {
	if (sthread_t::me() == holder)  {
	    cerr << "fatal error -- deadlock while acquiring mutex";
	    if (name()) {
		cerr << " (" << name() << ")";
	    }
	    cerr << endl;
	    W_FATAL(stINTERNAL);
	}

	SthreadStats.mutex_wait++;
	w_rc_t e = sthread_t::block(WAIT_FOREVER, &waiters, name(), this);
#if defined(SHORE_TRACE)
	if (e == RCOK)
		sthread_t::me()->push_resource_alloc(name(), this);
#endif
	return e;
    } else {
	holder = sthread_t::me();
#if defined(SHORE_TRACE)
	sthread_t::me()->push_resource_alloc(name(), this);
#endif
	return RCOK;
    }
}
#endif /* !W_DEBUG */




/*********************************************************************
 *
 *  smutex_t::release()
 *
 *  Release the mutex. If there are waiters, then wake up one.
 *
 *********************************************************************/
void
smutex_t::release()
{
    w_assert3(holder == sthread_t::me());


#if defined(W_DEBUG) || defined(SHORE_TRACE) 
    holder->pop_resource_alloc(this);
#endif 
   
    holder = waiters.pop();
    if (holder) {
	W_COERCE( holder->unblock() );
    }
}


/*********************************************************************
 *
 *  scond_t::scond_t(name)
 *
 *  Construct a conditional variable. "Name" is used for debugging.
 *
 *********************************************************************/
scond_t::scond_t(const char* nm)
: _waiters()
{
    if (nm) rename(nm?"c:":0, nm);
}


/*
 *  scond_t::~scond_t()
 *
 *  Destroy a condition variable.
 */
scond_t::~scond_t()
{
}


/*
 *  scond_t::wait(mutex, timeout)
 *
 *  Wait for a condition. Current thread release mutex and wait
 *  up to timeout milliseconds for the condition to fire. When
 *  the thread wakes up, it re-acquires the mutex before returning.
 *  
 */
w_rc_t
scond_t::wait(smutex_t& m, timeout_in_ms timeout)
{
    w_rc_t rc;

    if (timeout == WAIT_IMMEDIATE)  {
	rc = RC(stTIMEOUT);
    } else {
	m.release();
	rc = sthread_t::block(timeout, &_waiters, name(), this);
	W_COERCE(m.acquire());

	SthreadStats.scond_wait++;
    }
    
    return rc;
}


/*
 *  scond_t::signal()
 *
 *  Wake up one waiter of the condition variable.
 */
void scond_t::signal()
{
    sthread_t* t;
    t = _waiters.pop();
    if (t)  {
	W_COERCE( t->unblock() );
    }
}


/*
 *  scond_t::broadcast()
 *
 *  Wake up all waiters of the condition variable.
 */
void scond_t::broadcast()
{
    sthread_t* t;
    while ((t = _waiters.pop()) != 0)  {
	W_COERCE( t->unblock() );
    }
}



/*********************************************************************
 *
 *  sevsem_t::sevsem_t(is_post, name)
 *
 *  Construct an event semaphore. If "is_post" is true, semaphore
 *  is immediately posted. "Name" is used for debugging.
 *
 *********************************************************************/
sevsem_t::sevsem_t(int is_post, const char* name)
: _mutex(name), _cond(name), _post_cnt(0)
{
    if (is_post) _post_cnt = 1;
}


/*********************************************************************
 *
 *  sevsem_t::~sevsem_t()
 *
 *  Destroy an event semaphore. There should be no waiters.
 *
 *********************************************************************/
sevsem_t::~sevsem_t()
{
    if ((_mutex.acquire(WAIT_IMMEDIATE) != RCOK) || 
	_cond.is_hot())  {
	cerr << "sevsem_t::~sevsem_t:  fatal error --- semaphore busy\n";
	W_FATAL(stINTERNAL);
    }

    _mutex.release();
} 



/*********************************************************************
 *
 *  sevsem_t::post()
 *
 *  Post an event semaphore. All waiters are awakened and the posted
 *  state is saved until the next reset()---all subsequent wait()
 *  calls will succeed and return immediately until the next reset().
 *
 *  Return stSEMPOSTED if semaphore is already posted when post()
 *  is called.
 *
 *********************************************************************/
w_rc_t
sevsem_t::post()
{
    W_COERCE( _mutex.acquire() );
    if (_post_cnt++)  {
	_cond.broadcast();
	_mutex.release();
	return RC(stSEMPOSTED);
    }
    _cond.broadcast();
    _mutex.release();
    return RCOK;
}



/*********************************************************************
 *
 *  sevsem_t::reset(pcnt)
 *
 *  Save the semaphore post count in pcnt and reset the post count.
 *  Return stSEMALREADYRESET if post count is 0 when reset() is called.
 *
 *********************************************************************/
w_rc_t
sevsem_t::reset(int* pcnt)
{
    W_COERCE( _mutex.acquire() );
    if (_post_cnt) {
	if (pcnt) *pcnt = CAST(int,_post_cnt);
	_post_cnt = 0;
	_mutex.release();
	return RCOK;
    } 
    _mutex.release();
    return RC(stSEMALREADYRESET);
}


/*********************************************************************
 *
 *  sevsem_t::wait(timeout)
 *
 *  Wait up to timeout milliseconds for an event to occur.
 *
 *********************************************************************/
w_rc_t
sevsem_t::wait(timeout_in_ms timeout)
{
    W_COERCE( _mutex.acquire() );
    if (_post_cnt == 0)  {
	w_rc_t rc = _cond.wait(_mutex, timeout);
	SthreadStats.sevsem_wait++;
	_mutex.release();
	return rc.reset();
    }
    _mutex.release();
    return RCOK;
}



/*********************************************************************
 *
 *  sevsem_t::query(pcnt)
 *
 *  Return the post count in pcnt.
 *
 *********************************************************************/
void
sevsem_t::query(int& pcnt)
{
    pcnt = CAST(int, _post_cnt);
}


sthread_name_t::sthread_name_t() {
    memset(_name, '\0', sizeof(_name));
}

sthread_name_t::~sthread_name_t() {
}

void
sthread_name_t::rename(
    // can't have n2 or n3 without n1
    // can have n1,0,n3 or n1,n2,0
    const char*		n1,
    const char*		n2,
    const char*		n3)
{
	const int sz = sizeof(_name) - 1;
	size_t len = 0;
	_name[0] = '\0';
	if (n1)  {
#ifdef W_DEBUG
		len = strlen(n1);
		if(n2) len += strlen(n2);
		if(n3) len += strlen(n3);
		len++;
		if(len>sizeof(_name)) {
			cerr << "WARNING-- name too long for sthread_named_t: "
				<< n1 << n2 << n3;
		}
#endif

		// only copy as much as will fit
		strncpy(_name, n1, sz);
		len = strlen(_name);
		if (n2 && (int)len < sz)  {
			strncat(_name, n2, sz - len);
			len = strlen(_name);
			if (n3 && (int)len < sz)
				strncat(_name, n3, sz - len);
		}

		_name[sz] = '\0';
	}
}

void
sthread_named_base_t::unname() { 
	if(_name) { 
		delete _name; _name=0; 
	} 
}

void
sthread_named_base_t::rename(
    // can't have n2 or n3 without n1
    // can have n1,0,n3 or n1,n2,0
    const char*		n1,
    const char*		n2,
    const char*		n3)
{
    // NB: instead of delete/realloc, just wipe out 
    // the old name and re-use the space if we
    // have a new non-null name  
    // unname();
    if(n1) {
	if(_name) {
	    _name->rename(0,0,0);
	    _name->rename(n1,n2,n3);
	} else {
	    _name = new sthread_name_t;
	    _name->rename(n1,n2,n3);
	}
    } else {
	unname();
    }
}

sthread_named_base_t::~sthread_named_base_t()
{
	unname();
}



/*********************************************************************
 *
 *  sthread_priority_list_t::sthread_priority_list_t()
 *
 *  Construct an sthread list sorted by priority level.
 *
 *********************************************************************/
NORET
sthread_priority_list_t::sthread_priority_list_t()
    : w_descend_list_t<sthread_t, sthread_t::priority_t> (
	offsetof(sthread_t, _priority),
	offsetof(sthread_t, _link))  
{
}


// if you really are a sthread_t return 0
smthread_t* sthread_t::dynamic_cast_to_smthread()
{
	return 0;
}


const smthread_t* sthread_t::dynamic_cast_to_const_smthread() const
{
	return 0;
}


/*********************************************************************
 *
 *  dumpthreads()
 *  For debugging, but it's got to be
 *  present in servers compiled without debugging.  
 *
 *********************************************************************/
void dumpthreads()
{
	sthread_t::dump("dumpthreads()", cerr);
	sthread_t::dump_event(cerr);
	sthread_t::dump_io(cerr);

#if 0
	// Took out temporarily in order to circumvent
	// some gcc/purify problem (with printing doubles)
	// (Probably an alignment problem.)
	threadstats();
#endif
}

void threadstats()
{
	sthread_t::dump_stats(cerr);
}


/* If a write to a bogus pipe/socket occurs, we get a SIGPIPE, which
   normally ends the process.  This is bad because it kills the
   process!  The system call will return the proper "can't be used
   anymore" error, and so the error is detectable.  Ignoring the
   signal ensures that we don't need to die from problems like
   that. */

#ifndef _WINDOWS
static	void	hack_signals()
{
	struct	sigaction	sa;
	int	kr;

	kr = sigaction(SIGPIPE, 0, &sa);
	if (kr == -1) {
		w_rc_t	e = RC(sthread_base_t::stOS);
		cerr << "Warning: can't ignore SIGPIPE:" << endl << e << endl;
		return;
	}
	sa.sa_handler = W_POSIX_HANDLER SIG_IGN;
	kr = sigaction(SIGPIPE, &sa, 0);
	if (kr == -1) {
		w_rc_t	e = RC(sthread_base_t::stOS);
		cerr << "Warning: can't ignore SIGPIPE:" << endl << e << endl;
		return;
	}
	
}
#endif


#if defined(AIX32) || defined(AIX41)
/* Change signal masks so core dumps occur */
/* XXX other signal handlers may need flag changes */

static int signals_to_hack[] = {
	SIGSEGV, SIGILL, SIGBUS,
	SIGABRT, SIGEMT,
	0 };

static void hack_aix_signals()
{
	struct sigaction sa;
	int	i;
	int	kr;
	int	sig;

	for (i = 0; sig = signals_to_hack[i]; i++) {
		kr = sigaction(sig, 0, &sa);
		if (kr == -1) {
			w_rc_t	e = RC(sthread_base_t::stOS);
			cerr.form("sigaction(%d, FETCH):\n", sig);
			cerr << e << endl;
			continue;
		}

		sa.sa_flags |= SA_FULLDUMP;

		kr = sigaction(sig, &sa, 0);
		if (kr == -1) {
			w_rc_t e = RC(sthread_base_t::stOS);
			cerr.form("sigaction(%d, SET): AIX core hack:\n",
				  sig);
			cerr << e << endl;
		}
	}
}
#endif /* AIX signal hacks */

#if defined(W_DEBUG) && defined(STHREAD_CORE_STD)
/*
   Search all thread stacks s for the target address
 */
extern bool address_in_stack(sthread_core_t &core, void *address);

void sthread_t::find_stack(void *addr)
{
	w_list_i<sthread_t> i(*sthread_t::_class_list);
	
	while (i.next())  {
		if (address_in_stack(*(i.curr()->_core), addr)) {
			cout << "*** address " << addr << " found in ..."
			     << endl;
			i.curr()->print(cout);
		}
	}
}

extern "C" void find_thread_stack();

void find_thread_stack(void *addr)
{
	sthread_t::find_stack(addr);
}
#endif


/*********************************************************************
 *
 * Class sthread_init_t
 *
 *********************************************************************/


w_base_t::uint4_t	sthread_init_t::count = 0;

w_base_t::int8_t 	sthread_init_t::max_os_file_size;



/*
 * Default win32 exception handler.
 */

#ifdef _WIN32
LONG WINAPI
__ef(LPEXCEPTION_POINTERS p) 
{
    DWORD flags = p->ExceptionRecord->ExceptionFlags;
    W_FORM2(cerr, ("***** NT EXCEPTION (%#lx, flags=%#lx): %d params\n",
	p->ExceptionRecord->ExceptionCode,
	flags,
	p->ExceptionRecord->NumberParameters ));

    int i;
    for (i = 0; i< (int)(p->ExceptionRecord->NumberParameters); i++) {
	W_FORM2(cerr, ("infor[%d]=%#lx\n",
	    i,
	    (unsigned int)( p->ExceptionRecord->ExceptionInformation[i])));
    }


    switch(p->ExceptionRecord->ExceptionCode) {
    case EXCEPTION_ACCESS_VIOLATION:
	W_FORM2(cerr, 
	    ("***** access violation : %s of info[0] occurred at info[1]\n",
	    (char *)((p->ExceptionRecord->ExceptionInformation[0]==0)? "read":
	    (p->ExceptionRecord->ExceptionInformation[0]==1)? "write": 
	    "unknown")));
	break;

    case EXCEPTION_PRIV_INSTRUCTION:
	cerr << "***** priv instruction " << endl;
	break;
    case EXCEPTION_SINGLE_STEP:
	cerr << "***** single step " << endl;
	break;
    case EXCEPTION_STACK_OVERFLOW:
	cerr << "***** stack overflow " << endl;
	break;
    case EXCEPTION_ILLEGAL_INSTRUCTION:
	cerr << "***** illegal instruction " << endl;
	break;
    case EXCEPTION_INVALID_DISPOSITION:
	cerr << "***** invalid disposition " << endl;
	break;
    case EXCEPTION_IN_PAGE_ERROR:
	cerr << "***** in page error " << endl;
	break;
    case EXCEPTION_INT_DIVIDE_BY_ZERO:
	cerr << "***** integer divide by zero " << endl;
	break;
    case EXCEPTION_INT_OVERFLOW:
	cerr << "***** integer overflow " << endl;
	break;
    case EXCEPTION_ARRAY_BOUNDS_EXCEEDED:
	cerr << "***** array bounds " << endl;
	break;
    case EXCEPTION_FLT_DENORMAL_OPERAND:
    case EXCEPTION_FLT_DIVIDE_BY_ZERO:
    case EXCEPTION_FLT_INEXACT_RESULT:
    case EXCEPTION_FLT_INVALID_OPERATION:
    case EXCEPTION_FLT_OVERFLOW:
    case EXCEPTION_FLT_UNDERFLOW:
    case EXCEPTION_FLT_STACK_CHECK:
	cerr << "***** floating point exception" << endl;
	break;
    case EXCEPTION_NONCONTINUABLE_EXCEPTION:
	cerr << "***** noncontinuable exception" << endl;
	flags = EXCEPTION_NONCONTINUABLE; //for now
    default:
	break;
    }
    dumpthreads();

    flags = EXCEPTION_NONCONTINUABLE; //for now

    switch(flags) {
    case EXCEPTION_NONCONTINUABLE:
	break;
    default:
	return EXCEPTION_CONTINUE_EXECUTION; // i.e., continue
    }
    return EXCEPTION_EXECUTE_HANDLER; // i.e., terminate
}
#endif /* _WINDOWS */


static	void	hack_large_file_stuff(w_base_t::int8_t &max_os_file_size)
{
    /*
     * Get limits on file sizes imposed by the operating 
     * system and shell.
     */

#if !defined(_WIN32)
#if defined(SOLARIS2) && defined(LARGEFILE_AWARE)
#define	os_getrlimit(l,r)	getrlimit64(l,r)
#define	os_setrlimit(l,r)	setrlimit64(l,r)
typedef	struct rlimit64	os_rlimit_t;
#else
#define	os_getrlimit(l,r)	getrlimit(l,r)
#define	os_setrlimit(l,r)	setrlimit(l,r)
typedef struct rlimit	os_rlimit_t;
#endif

	os_rlimit_t	r;
	int		n;

	n = os_getrlimit(RLIMIT_FSIZE, &r);
	if (n == -1) {
		w_rc_t e = RC(fcOS);
		cerr << "getrlimit(RLIMIT_FSIZE):" << endl << e << endl;
		W_COERCE(e);
	}
	if (r.rlim_cur < r.rlim_max) {
		r.rlim_cur = r.rlim_max;
		n = os_setrlimit(RLIMIT_FSIZE, &r);
		if (n == -1) {
			w_rc_t e = RC(fcOS);
			cerr << "setrlimit(RLIMIT_FSIZE, " << r.rlim_cur
				<< "):" << endl << e << endl;
		    	cerr << e << endl;
			W_FATAL(fcINTERNAL);
		}
	}
	max_os_file_size = w_base_t::int8_t(r.rlim_cur);
	/*
	 * Unfortunately, sometimes this comes out
	 * negative, since r.rlim_cur is unsigned and
	 * fileoff_t is signed (sigh).
	 */
	if (max_os_file_size < 0) { 
		max_os_file_size = w_base_t::uint8_t(r.rlim_cur) >> 1;
		w_assert1( max_os_file_size > 0);
	}
#else
	/* 
	 * resource limits not yet implemented : we're stuck
	 * with small files for the time being
	 */
#ifdef LARGEFILE_AWARE
	max_os_file_size = w_base_t::int8_max;
#else
	max_os_file_size = w_base_t::int4_max;
#endif
//	max_os_file_size = fileoff_max;
#endif
}

/* XXX this doesn't work, neither does the one in sdisk, because
   the constructor order isn't guaranteed.  The only important
   use before main() runs is the one right above here. */
const sthread_base_t::fileoff_t sthread_base_t::fileoff_max = sdisk_t::fileoff_max;


/*********************************************************************
 *
 *  sthread_init_t::sthread_init_t()
 *
 *  Initialize the sthread environment. The first time this method
 *  is called, it sets up the environment and returns with the
 *  identity of main_thread.
 *
 *********************************************************************/
sthread_init_t::sthread_init_t()
{
	if (++count == 1) {

		hack_large_file_stuff(max_os_file_size);


#ifdef _WIN32
		bool do_exceptions = false;
#ifdef STHREAD_WIN32_EXCEPTION
		do_exceptions = true;
#else
		char *s = getenv("STHREAD_WIN32_EXCEPTION");
		do_exceptions = (s && *s && atoi(s));
#endif /*STHREAD_WIN32_EXCEPTION*/
		if (do_exceptions) {
			LPTOP_LEVEL_EXCEPTION_FILTER res =
				SetUnhandledExceptionFilter(__ef);
	}
#endif /* _WIN32 */

		/*
		 *  Register error codes.
		 */
		if (! w_error_t::insert(
			"Threads Package",
			error_info, 
			sizeof(error_info) / sizeof(error_info[0])))   {
		    
		    cerr << "sthread_init_t::sthread_init_t: "
			 << " cannot register error code" << endl;

		    W_FATAL(stINTERNAL);
		}

#if defined(AIX31) || defined(AIX41)
		hack_aix_signals();
#endif

#ifndef _WIN32
		hack_signals();
#endif

		/*
		 * There is a chance that the static _w_fastnew member 
		 * not been constructed yet.
		 */
		W_FASTNEW_STATIC_PTR_DECL_INIT(sthread_name_t, 100);
#ifndef NO_FASTNEW
		if (sthread_name_t::_w_fastnew == 0)
			W_FATAL(fcOUTOFMEMORY);
#endif

		W_COERCE(sthread_t::startup());

#ifdef _WIN32
		w_rc_t e = init_winsock();
		if (e != RCOK) {
			cerr << "Can't init winsock, stuff may be broken:"
				<< endl << e << endl;
		}
#endif

	}
}



/*********************************************************************
 *
 *  sthread_init_t::~sthread_init_t()
 *
 *  Destructor. Does not do much.
 *
 *********************************************************************/
NORET
sthread_init_t::~sthread_init_t()
{
    w_assert1(count > 0);
    if (--count == 0)  {

	// sthread_t::_class_list.pop();
	// delete sthread_t::_class_list;
	// delete sthread_t::_ready_q;

	W_COERCE(sthread_t::shutdown());
    }
}


/*<std-header orig-src='shore'>

 $Id: event.cpp,v 1.20 2002/01/25 00:15:18 bolo Exp $

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

#include <w_statistics.h>

#include <unix_stats.h>

#define	EVENT_C
#include "sthread.h"
#include "sthread_stats.h"
#include "spin.h"
#include "ready_q.h"

/*
 * Choose an event handler.
 *
 * So far, Solaris is the only OS we use that has a reasonable poll.
 * Linux supports poll, but it is a hack on top of select in most
 * existing linux systems, if it is present at all.
 * It also lacks all the features of a real poll, so we don't configure
 * it into the system.
 */

#if defined(NEW_IO) && defined(_WIN32)
#include <win32_events.h>
#else
#include <sfile_handler.h>
#ifndef _WIN32
#if defined(SOLARIS2) 
#define OS_HAS_POLL
#define OS_HAS_SELECT
/* Solaris implements select() with poll().  Bolo's implementation
   is better. */
#define OS_PREFERS_POLL
#elif defined(HPUX8)
#define	OS_HAS_POLL
#define	OS_HAS_SELECT
/* just try poll */
#define	OS_PREFERS_POLL
#else	/* the default */
#define OS_HAS_SELECT
#endif

#ifdef OS_HAS_POLL
#include <sfile_handler_poll.h>
#endif

#ifdef OS_HAS_SELECT
#include <sfile_handler_select.h>
#endif

#endif	/*WIN32*/
#endif	/*NEW_IO*/

#include <sdisk.h>

#if !(defined(_WIN32) && defined(NEW_IO_ONLY))
#define	HACKED_SCHED
#endif
#ifdef HACKED_SCHED
/* Hack sthread scheduling to poll the diskrw queue by hand */
#include "diskrw.h"
#include <sdisk_diskrw.h>
#endif

extern class sthread_stats SthreadStats;


bool			sthread_t::in_cs = false;
w_timerqueue_t		sthread_t::_event_q;
#if defined(NEW_IO) && defined(_WIN32)
win32_event_handler_t	*sthread_t::_io_events;
#else
sfile_handler_t		*sthread_t::_io_events;
#endif


w_rc_t	sthread_t::startup_events()
{
        /* OS event specific */
#if defined(_WIN32) && defined(NEW_IO)
	_io_events = new win32_event_handler_t;
#elif defined(_WIN32)
	_io_events = new sfile_handler_t;
#else
	const char	*s;

	s = getenv("STHREAD_EVENTS");
	if (!s)
		s = "";
   
	switch (s[0]) {
#ifdef OS_HAS_POLL 
	case 'p':
	case 'P':
		_io_events = new sfile_handler_poll_t;
		break;
#endif
#ifdef OS_HAS_SELECT
	case 's':
	case 'S':
		_io_events = new sfile_handler_select_t;
		break;
#endif 
	case 'D':
	default:
	case 'd':
#ifdef OS_PREFERS_POLL
		_io_events = new sfile_handler_poll_t;
#else
		_io_events = new sfile_handler_select_t;
#endif
		break;
	}
#endif

	return _io_events ? RCOK : RC(fcOUTOFMEMORY);
}

#ifndef _WINDOWS
/*
 * sthread_t::setup_signals()
 *
 * Initialize signal handlers and set priority level / signal mask.
 */

w_rc_t	sthread_t::setup_signals(sigset_t &lo_spl, sigset_t &hi_spl)
{
	sigset_t nset, oset;
	sigemptyset(&oset);
	sigemptyset(&nset);

	/* POSIX way -- expects handler to take no parameters */
	
	struct sigaction sact;
	sact.sa_handler = _caught_signal;
	sact.sa_mask = nset;
	sact.sa_flags = 0;

#if 0
	sigaction(SIGINT, &sact, 0);  // catch cntrl-C
#endif
	
	if (sigprocmask(SIG_BLOCK, &nset, &oset) == -1) {
		w_rc_t	e = RC(stOS);
		cerr << "sigprocmask(SIG_BLOCK):" << endl << e << endl;
		W_COERCE(e);
	}

	lo_spl = oset;	// lo priority ... interrupts allowed
	hi_spl = nset;	// hi priority ... no interrupts
	in_cs = true;

	return RCOK;
}

#endif /* _WINDOWS */

/*
 * The idle thread <insert description from somewhere else>
 *
 * The thread is created at normal priority so system
 * startup will allow the idle thread cpu time for its setup
 * before other threads are run.
 */

sthread_idle_t::sthread_idle_t()
: sthread_t(t_regular, "idle_thread")
{
}


/*
 *  sthread_idle_t::run()
 *
 *  Body of idle thread.
 */
void 
sthread_idle_t::run()
{
	
    unsigned	idle_timeout = 60;
    char		*s;
    s = getenv("STHREAD_IDLE_TIMEOUT");
    if (s && *s && atoi(s) >= 0)
	idle_timeout = atoi(s);

    /* Wakeup this often when nothing is happening */
    const stime_t DEFAULT_TIMEOUT(stime_t::sec(idle_timeout));
    const stime_t IMMEDIATE_TIMEOUT;			// 0 seconds

#ifndef _WINDOWS
    sigset_t lo_spl, hi_spl;
    W_COERCE(setup_signals(lo_spl, hi_spl));
#endif

    /*
     *  Idle thread only runs at minimum priority
     */
    W_COERCE( _me->set_priority(t_idle_time) );

    /*
     *  Main loop
     */
    for (;;)	{
	bool	timeout_immediate = false;

#ifdef HACKED_SCHED
	/*
	 *  Check if any disk I/O is done
	 */

	if (sdisk_handler)
		sdisk_handler->polldisk_check();
#endif

	yield();
	SthreadStats.idle_yield_return++;

#ifdef HACKED_SCHED
	if (sdisk_handler && sdisk_handler->pending())
		sdisk_handler->polldisk();
#endif

	if (_priority > t_idle_time)  {
	    // we might wake up to find that our priority was changed
	    timeout_immediate = true;

	    /* XXX ??? reduce priority AFTER scheduling I/O
	       completions, to allow idle thread as I/O tasker to
	       complete first. */

	    // set it back
	    W_COERCE( _me->set_priority(t_idle_time) );
	}

#ifdef HACKED_SCHED
	if (sdisk_handler) {
		/* If some I/O has finished do not go to sleep.  */
		if (sdisk_handler->pending())
			continue;
		sdisk_handler->ensure_sleep();
//	    svcport->sleep = 1;
	}
#endif

	/*
	 * Determine the scheduling delay, if any.
	 */
	stime_t	then;		// when the next event occurs
	stime_t	before;		// time before waiting
	stime_t	after;		// time after waiting
	stime_t	timeout;	// selected timeout	

	bool events_pending = false;		// timer events have occured
	bool have_before = false;		// lazy time retrieval
	bool events_scheduled = _event_q.nextEvent(then);

	if (timeout_immediate)
		timeout = IMMEDIATE_TIMEOUT;
	else if (! _ready_q->is_empty())
		timeout = IMMEDIATE_TIMEOUT;
	else if (events_scheduled) {
		// lazy system call
		before = stime_t::now();
		have_before = true;
		if (then <= before) {
			events_pending = true;
			timeout = IMMEDIATE_TIMEOUT;
		}
		else
			timeout = then - before;
	}
	else
		timeout = DEFAULT_TIMEOUT;
	
	/*
	 *  Wait for external events or interrupts
	 */
	static unsigned continuous_idle = 0;

	{
	    w_rc_t rc = _io_events->prepare(timeout, false);
	    if (rc) {
		cerr << rc << endl;
		W_COERCE(rc);
	    }
	}

	/* allow interrupts */
	in_cs = false;	/* system call can trigger a signal */
#ifndef _WINDOWS
	int kr;
	kr = sigprocmask(SIG_SETMASK, &lo_spl, 0);
	if (kr == -1) {
		w_rc_t	e = RC(sthread_base_t::stOS);
		cerr << "sigprocmask(SIG_SETMASK)" << endl << e << endl;
		W_COERCE(e);
	}
#endif


	// lazy system calls :-)
	if (!have_before)
		before = stime_t::now();

	w_rc_t rc = _io_events->wait();

	after = stime_t::now();
	
#ifndef _WINDOWS	// POTENTIAL BUGBUG
	/* block interrupts */
	kr = sigprocmask(SIG_SETMASK, &hi_spl, 0);
	if (kr == -1) {
		w_rc_t	e = RC(sthread_base_t::stOS);
		cerr << "sigprocmask(SIG_SETMASK)" << endl << e << endl;
		W_COERCE(e);
	}
#endif
	in_cs = true;	/* only now is signal delivery blocked */

	SthreadStats.idle_time += (float) (after - before);

	/*
	 * Dispatch any events which are pending, from a menu
	 * of file, and timer events
	 */
	
	/* file events */
	if (rc == RCOK) {
		_io_events->dispatch(after);
	}

	/* timer events */
	// reevaluate if needed
	if (!events_pending && events_scheduled)
		events_pending = (then <= after);

	if (events_pending)
		_event_q.run(after /* + stime_t::msec(20) ??? */ );

	if (rc)  {
		if (rc.err_num() == stTIMEOUT)  {
			SthreadStats.idle++;

			/* If we have been idled for too long, print
			 status to stdout for debugging.  */

			if (++continuous_idle == 40) {
				continuous_idle = 0;
			}
		} else {
			continuous_idle = 0;
                }
	}
    }
}


/* Attach and detach IO event handlers */
#if defined(NEW_IO) && defined(_WIN32)
w_rc_t	sthread_t::io_start(win32_event_t &event)
{
	if (!_io_events)
		return RC(stINTERNAL);
	return _io_events->start(event);
}

w_rc_t	sthread_t::io_finish(win32_event_t &event)
{
	if (!_io_events)
		return RC(stINTERNAL);
	_io_events->stop(event);
	return RCOK;
}

#else
w_rc_t	sthread_t::io_start(sfile_hdl_base_t &event)
{
	if (!_io_events)
		return RC(stINTERNAL);
	return _io_events->start(event);
}

w_rc_t	sthread_t::io_finish(sfile_hdl_base_t &event)
{
	if (!_io_events)
		return RC(stINTERNAL);
	_io_events->stop(event);
	return RCOK;
}

bool	sthread_t::io_active(int fd)
{
	return _io_events ? _io_events->is_active(fd) : false;
}

#endif

void	sthread_t::dump_event(ostream &s)
{
	if (_io_events)
		_io_events->print(s);

	if (_event_q.isEmpty())
		s << "No scheduled events." << endl;
	else {
		s << "Scheduled events:" << endl;
                _event_q.print(s);
		s << endl;
	}
}

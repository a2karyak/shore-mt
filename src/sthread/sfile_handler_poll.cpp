/*<std-header orig-src='shore'>

 $Id: sfile_handler_poll.cpp,v 1.9 1999/06/07 19:06:06 kupsch Exp $

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
 *   NewThreads is Copyright 1992, 1993, 1994, 1995, 1996, 1997, 1998 by:
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

#include <w_strstream.h>
#include <w_workaround.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include <poll.h>

#ifdef Linux
/* some aliases that aren't defined in linux poll.h */
#	ifndef POLLRDNORM
#	define  POLLRDNORM POLLIN
#	endif

/* XXX this interpretation is questionable! */
#	ifndef POLLRDBAND
#	define  POLLRDBAND POLLPRI
#	endif

#	ifndef POLLWRNORM
#	define  POLLWRNORM POLLOUT
#	endif

/* Not supported */
#	ifndef POLLWRBAND
#	define  POLLWRBAND  00
#	endif

#endif /* Linux */

#ifdef __GNUC__
#pragma implementation "sfile_handler_poll.h"
#endif

#define W_INCL_LIST
#include <w.h>
#include <w_debug.h>

#include <sthread.h>
#include <sfile_handler.h>
#include <sfile_handler_poll.h>

#include <w_statistics.h>
#include <unix_stats.h>
#include "sthread_stats.h"
extern class sthread_stats SthreadStats;

/* pool sleeps for infinite time */
#ifndef INFTIME
#define	INFTIME	(-1)
#endif

#define FD_NONE	(-1)	/* invalid unix file descriptor */

#if defined(DEBUG)
static int gotenv;	// hold over from sthreads integration
#endif

extern ostream	&operator<<(ostream &,  const pollfd &);
extern ostream	&print_poll_events(ostream &, short);

/*
 * File "Handlers" and "Handles".
 *
 * A means of generating thread wakeup events for thread's
 * awaiting I/O on non-blocking devices.
 */


sfile_handler_poll_t::sfile_handler_poll_t()
: map(0),
  max_map(0),
  fds(0),
  num_fds(0),
  max_fds(0),
  active_fds(0),
  direction(0),
  rc(0),
  wc(0),
  ec(0),
  timeout(0)
{
	_hits[0] = _hits[1] = _hits[2] = 0;
}


sfile_handler_poll_t::~sfile_handler_poll_t()
{
	if (map) {
		delete [] map;
		map = 0;
		max_map = 0;
	}
	if (fds) {
		delete [] fds;
		fds = 0;
		max_fds = num_fds = 0;
	}
}


/*
 *  sfile_handler_poll_t::enable()
 *
 * Allow this sfile to receive events.
 */

w_rc_t sfile_handler_poll_t::start(sfile_hdl_base_t &hdl)
{
	W_DO(_start(hdl));

	/* always keep enough map entries allocated to map all FDs. */
	if ((unsigned)hdl.fd >= max_map) {
		unsigned	new_max = hdl.fd + 10;	/* XXX magick */
		fd_status	*new_map = new fd_status[new_max];
		if (!new_map)
			return RC(fcOUTOFMEMORY);

		/* retain existing entries */
		for (unsigned i = 0; i < max_map; i++)
			new_map[i] = map[i];		    

		fd_status	*tmp = map;
		map = new_map;
		max_map = new_max;
		delete [] tmp;
	}
	map[hdl.fd].refs++;

	/* keep track of unique file descriptors */
	if (map[hdl.fd].refs == 1)
		num_fds++;

	/* always keep enough poll entries allocated */
	if (num_fds > max_fds) {
		unsigned	new_max = max_fds + 10;	/* XXX magick */
		struct pollfd	*new_fds = new struct pollfd[new_max];
		if (!new_fds)
			return RC(fcOUTOFMEMORY);

		/* fds are filled on demand, no need to copy the contents */
		struct pollfd	*tmp = fds;
		fds = new_fds;
		max_fds = new_max;
		delete [] tmp;
	}

	return RCOK;
}


/*
 *  sfile_handler_poll_t::disable()
 *
 *  Stop dispatching events for a sfile.
 */

void sfile_handler_poll_t::stop(sfile_hdl_base_t &hdl)
{
	w_rc_t	e(_stop(hdl));
	if (e)
		return;

	map[hdl.fd].refs--;
	if (map[hdl.fd].refs == 0)
		num_fds--;
}


void sfile_handler_poll_t::enable(sfile_hdl_base_t &hdl)
{
	if (hdl._mode & rd)
		rc++;
	if (hdl._mode & wr)
		wc++;
	if (hdl._mode & ex)
		ec++;
	map[hdl.fd].active |= hdl._mode;
}


void sfile_handler_poll_t::disable(sfile_hdl_base_t &hdl)
{
	if (hdl._mode & rd)
		rc--;
	if (hdl._mode & wr)
		wc--;
	if (hdl._mode & ex)
		ec--;
	/* XXX this causes problems if >1 handle has the same request
	   for the file descriptor.  That shouldn't be a problem,
	   since that would cause irrational behavior.  In that case,
	   we need to track counts of rd,wr,ex instead of just flags. */
	map[hdl.fd].active &= ~hdl._mode;
}
	


/*
 *  sfile_handler_poll_t::prepare(timeout)
 *
 *  Prepare the sfile to wait on file events for timeout milliseconds.
 */

w_rc_t sfile_handler_poll_t::prepare(const stime_t &tout, bool forever)
{
	if (forever)
		timeout = INFTIME;    
	else if (tout == stime_t(0))	/* poll */
		timeout = 0;
	else {			/* sleep */	
		timeout = tout.msecs();
		/* round up the sleep interval to the millisecond;
		   this is stupid; poll() should take a timeval
		   like everything else. */
		if (timeout == 0)
			timeout = 1;
	}

	/* always be ready to have all file descriptors active */
	/* a snazzier implementation would be to cache
	   the poll list */

	unsigned	next_slot = 0;
	/* XXX could maintain a list of active descriptors to speed
	   sparse event generation */
	for (unsigned i = 0; i < max_map; i++) {
		fd_status	&f(map[i]);
		if (!f.active) {
			f.slot = -1;    
			continue;
		}
		f.slot = next_slot++;
		fds[f.slot].fd = i;
		fds[f.slot].events = 0;
		fds[f.slot].revents = 0;
		/* generate poll event flags */
		short	poll_event = 0;
		if (f.active & rd)
			poll_event |= POLLRDNORM;
		if (f.active & wr)
			poll_event |= POLLWRNORM;
		/* XXX this interpretation is questionable; it may
		   not be supportable. */
		if (f.active & ex)
			poll_event |= POLLRDBAND;
		    
		fds[f.slot].events |= poll_event;
	}
	active_fds = next_slot;
	ready_fds = 0;

#if defined(DEBUG)
	if (gotenv > 1) {
		gotenv = 0;
		cerr << "poll():" << endl;
		for (unsigned i = 0; i < active_fds; i++)
			W_FORM(cerr)(" %d:%s%s%s",
				     fds[i].fd,
				     (fds[i].events & POLLRDNORM) ? "R" : "",
				     (fds[i].events & POLLWRNORM) ? "W" : "",
				     (fds[i].events & POLLRDBAND) ? "X" : "");

		cerr << "\ttimeout= ";
		if (timeout > 0)
			cerr << timeout;
		else if (timeout == 0)
			cerr << " (POLL)";
		else
			cerr << "INDEFINITE";
		cerr << endl << endl;
	}
	if (active_fds == 0) {
		w_assert1(timeout >= 0);
	}
#else
	w_assert1(active_fds || timeout >= 0);
#endif

#ifdef EXPENSIVE_STATS
	/* Account for these here, rather than in wait(), so timing
	   of event waits isn't compromised.  Could move this segment
	   to the wait() method if 100% accuracy is important (for
	   the case where prepare is called, but the wait doens't
	   happen. */

	stats_wait(active_fds);
#endif

	return RCOK;
}


/*
 *  sfile_handler_poll_t::wait()
 *
 *  Wait for any file events or for interrupts to occur.
 */
w_rc_t sfile_handler_poll_t::wait()
{
	SthreadStats.selects++;

	int n = ::poll(fds, active_fds, timeout);
	int poll_errno = errno;
	
	switch (n)  {
	case -1:
		if (poll_errno != EINTR)  {
			cerr << "poll():" << endl
				<< RC2(fcOS, poll_errno) << endl;
		}
		else {
			// cerr << "EINTR " << endl;
			SthreadStats.eintrs++;
		}
		return RC2(sthread_t::stOS, poll_errno);
		break;

	case 0:
		return RC(sthread_base_t::stTIMEOUT);
		break;

	default:
		SthreadStats.selfound++;
		ready_fds = n;
#if 1
		/* yell about any unforseen events that occur */
		for (unsigned i = 0; i < active_fds; i++) {
			if (!(fds[i].revents & (POLLERR|POLLHUP|POLLNVAL)))
				continue;
			W_FORM(cerr)("poll: slot %d, fd %d: ", i, fds[i].fd);
			print_poll_events(cerr, fds[i].revents);
			cerr << endl;
		}
#endif
		break;
	}

	return RCOK;
}


/*
 *  sfile_handler_poll_t::dispatch()
 *
 *  Dispatch select() events to individual file handles.
 *
 *  Events are processed in order of priority.  At current, only two
 *  priorities, "read"==0 and "write"==1 are supported, rather crudely
 */

void sfile_handler_poll_t::dispatch(const stime_t &at)
{
	/* any events to dispatch? */
	if (!ready_fds)
		return;

#ifdef EXPENSIVE_STATS
	/* Account for these here, rather than in wait(), so timing
	   of event waits isn't compromised.  Could move this segment
	   to the wait() method if 100% accuracy is important (for
	   the case where wait is called, but dispatch isn't. */

	stats_found(ready_fds);
#endif

	last_event = at;

	direction = (direction == 0) ? 1 : 0;
	
	sfile_hdl_base_t *p;

	w_list_i<sfile_hdl_base_t> handles(_list, direction);

	bool active[3];

	while ((p = handles.next()))  {
		/* an iterator across priority would do better */
		if (!p->enabled() || p->priority() != 0)
			continue;

		/* XXXX turn HUP, ERR, etc into read and exception events
		   too.  XXX complain about bad FDs POLLNVAL;
		   what to do about errors? */

		struct pollfd &poll(fds[map[p->fd].slot]);

		/* XXX how about poll.revents & (poll.events | other_stuff)? */
		
		active[0] = (p->_mode & rd) && (poll.revents & (POLLRDNORM |POLLERR |POLLHUP));
		active[1] = (p->_mode & wr) && (poll.revents & (POLLWRNORM |POLLERR | POLLHUP));
		active[2] = (p->_mode & ex) && (poll.revents & POLLRDBAND);

		if (active[0] || active[1] || active[2]) {
			p->dispatch(at, active[0], active[1], active[2]);
			_hits[0] += (active[0] != 0);
			_hits[1] += (active[1] != 0);
			_hits[2] += (active[2] != 0);
		}
	}
	
	for (handles.reset(_list);  (p = handles.next()); )  {
		if (!p->enabled() || p->priority() == 0)
			continue;

		struct pollfd &poll(fds[map[p->fd].slot]);
		
		active[0] = (p->_mode & rd) && (poll.revents & (POLLRDNORM |POLLERR |POLLHUP));
		active[1] = (p->_mode & wr) && (poll.revents & (POLLWRNORM |POLLERR | POLLHUP));
		active[2] = (p->_mode & ex) && (poll.revents & POLLRDBAND);

		if (active[0] || active[1] || active[2]) {
			p->dispatch(at, active[0], active[1], active[2]);
			_hits[0] += (active[0] != 0);
			_hits[1] += (active[1] != 0);
			_hits[2] += (active[2] != 0);
		}
	}
}


bool	sfile_handler_poll_t::probe(sfile_hdl_base_t &hdl)
{
	struct	pollfd fd;
	int	n;

	if (hdl._mode == 0)
		cout << "PROBE without a mode" << endl;
	if (hdl.fd < 0)
		cout << "PROBE with bad fd " << hdl.fd << endl;

	fd.fd = hdl.fd;
	fd.events = 0;
	if (hdl._mode & rd)
		fd.events |= POLLRDNORM;
	if (hdl._mode & wr)
		fd.events |= POLLWRNORM;
	/* XXX this interpretation is questionable; it may
	   not be supportable. */
	if (hdl._mode & ex)
		fd.events |= POLLRDBAND;
	fd.revents = 0;

	n = ::poll(&fd, 1, 0);
	if (n == -1) {
		w_rc_t	e = RC(fcOS);
		cerr << "poll_handler: probe fails:" << endl << e << endl;
	}
	else if (fd.revents & POLLNVAL)
		W_FORM(cerr)("poll_handler: probe invalid fd %d.\n", fd.fd);

	if (fd.revents & (POLLERR|POLLHUP|POLLNVAL)) {
		W_FORM(cerr)("poll_probe: fd %d: ", fd.fd);
		print_poll_events(cerr, fd.revents);
		cerr << endl;
	}

	/* Without error returns, NO I/O with a ERR or HUP means that an I/O
	   is not possible. */
	if (((fd.events & fd.revents) == 0) &&
	    (fd.revents & (POLLERR | POLLHUP)))
		return false;

	/* XXX don't give errors or hup to exception-only? */
	return (fd.events & fd.revents);
}


/*
 *  sfile_handler_poll_t::is_active(fd)
 *
 *  Return true if there is an active file handler for fd.
 */

bool sfile_handler_poll_t::is_active(int fd)
{
	w_list_i<sfile_hdl_base_t> i(_list);
	sfile_hdl_base_t *p; 

	while ((p = i.next()))  {
		if (p->fd == fd)
			break;
	}

	return p != 0;
}


ostream &sfile_handler_poll_t::print(ostream &o)
{
	o << "POLL EVENTS:";

	if (_hits[0])
		W_FORM(o)(" %d read", _hits[0]);
	if (_hits[1])
		W_FORM(o)(" %d write", _hits[1]);
	if (_hits[2])
		W_FORM(o)(" %d except", _hits[2]);

	o << " timeout: ";
	if (timeout == -1)
		o << "indefinite";
	else if (timeout == 0)
		o << "poll";
	else
		o << timeout << " msec";

	if (_hits[0] || _hits[1] || _hits[2]) {
		sinterval_t	delta(stime_t::now() - last_event);
		o << ", last event " << delta << " seconds ago";
	}

	o << endl;
			
	w_list_i<sfile_hdl_base_t> i(_list);
	sfile_hdl_base_t* f;

	while ((f = i.next()))
		o << *f << endl;    

	return o;
}



ostream	&print_poll_events(ostream &s, short events)
{
	if (events & POLLIN)
		s << " in";
	if (events & POLLRDNORM)
		s << " rdnorm";
	if (events & POLLRDBAND)
		s << " rdband";
#if POLLOUT != POLLWRNORM
	if (events & POLLOUT)
		s << " out";
#endif
	if (events & POLLWRNORM)
		s << " wrnorm";
	if (events & POLLWRBAND)
		s << " wrband";
	if (events & POLLERR)
		s << " err";
	if (events & POLLHUP)
		s << " hup";
	if (events & POLLNVAL)
		s << " invalid";

	return s;
}


ostream	&operator<<(ostream &s,  const pollfd &poll)
{
	s << "poll(" << poll.fd;
	if (poll.events) {
		s << " events:";
		print_poll_events(s, poll.events);
	}
	if (poll.revents) {
		s << " revents:";
		print_poll_events(s, poll.revents);
	}
	return s << ")";
}

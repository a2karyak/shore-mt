/*<std-header orig-src='shore'>

 $Id: sfile_handler_select.cpp,v 1.10 2001/09/18 22:09:57 bolo Exp $

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
#include <w_signal.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#ifdef __GNUC__
#pragma implementation "sfile_handler_select.h"
#endif

#define W_INCL_LIST
#include <w.h>
#include <w_debug.h>

#include <sthread.h>
#include <sfile_handler.h>
#include <sfile_handler_select.h>

#include <w_statistics.h>
#include <unix_stats.h>
#include "sthread_stats.h"
extern class sthread_stats SthreadStats;


#define FD_NONE	-1	/* invalid unix file descriptor */

#if !defined(AIX32) && !defined(AIX41)

extern "C" int select(int, fd_set *, fd_set *, fd_set *, struct timeval *);

#endif


#ifdef FD_SETSIZE
static	const	int	maxfds = FD_SETSIZE;
#else
/* XXX hack ... the first thing to need this sets it.  Could
   use a constructor initializer, but it might not happen
   early enough.  Ugly race conditions anyway you look at it. */
static	const	int	maxfds = 0;
#endif

/*
 *  operator<<(o, fd_set)
 *
 *  Print an fd_set.
 */

ostream& operator<<(ostream& o, const fd_set &s) 
{
#ifndef FD_SETSIZE
	/* XXX hack */
	if (maxfds == 0)
		maxfds = sysconf(_SC_OPEN_MAX);
#endif
	for (int n = 0; n < maxfds; n++)  {
		if (FD_ISSET(n, &s))  {
			o << ' ' << n;
		}
	}
	return o;
}

#ifdef DEBUG
/*********************************************************************
 *
 *  check for all-zero mask
 *
 *  For debugging.
 *
 *********************************************************************/

bool
fd_allzero(const fd_set *s)
{
#ifndef FD_SETSIZE
	/* XXX hack */
	if (maxfds == 0)
		maxfds = sysconf(_SC_OPEN_MAX);
#endif

	// return true if s is null ptr or it
	// points to an empty mask
	if (s==0)
		return true;

	for (int n = 0; n < maxfds; n++)  {
		if (FD_ISSET(n, s))
			return false;
	}
	return true;
}

#endif /*DEBUG*/

#ifdef DEBUG
static int gotenv;	// hold over from sthreads integration
#endif


/*
 * File "Handlers" and "Handles".
 *
 * A means of generating thread wakeup events for thread's
 * awaiting I/O on non-blocking devices.
 */


sfile_handler_select_t::sfile_handler_select_t()
: direction(0),
  rc(0),
  wc(0),
  ec(0),
  dtbsz(0),
  tvp(0)
{
	memset(masks, '\0', sizeof(masks));
	memset(ready, '\0', sizeof(ready));

	any[0] = any[1] = any[2] = 0;
	_hits[0] = _hits[1] = _hits[2] = 0;

	tval.tv_sec = tval.tv_usec = 0;
}


/*
 *  sfile_handler_select_t::enable()
 *
 * Allow this sfile to receive events.
 */

w_rc_t sfile_handler_select_t::start(sfile_hdl_base_t &hdl)
{
	W_DO(_start(hdl));    

	if (hdl.fd >= dtbsz)
		dtbsz = hdl.fd + 1;

	return RCOK;
}


/*
 *  sfile_handler_select_t::disable()
 *
 *  Stop dispatching events for a sfile.
 */

void sfile_handler_select_t::stop(sfile_hdl_base_t &hdl)
{
	w_rc_t	e(_stop(hdl));
	if (e)
		return;    

#if 0
	/* XXX what if there is another event on the same descriptor ? */
	if (hdl.fd == dtbsz - 1)
		--dtbsz;
#endif
}


void sfile_handler_select_t::enable(sfile_hdl_base_t &hdl)
{
	if (hdl._mode & rd) {
		rc++;
		FD_SET(hdl.fd, &masks[0]);
	}
	if (hdl._mode & wr) {
		wc++;
		FD_SET(hdl.fd, &masks[1]);
	}
	if (hdl._mode & ex) {
		ec++;
		FD_SET(hdl.fd, &masks[2]);
	}
}


void sfile_handler_select_t::disable(sfile_hdl_base_t &hdl)
{
	if (hdl._mode & rd) {
		rc--;
		FD_CLR(hdl.fd, &masks[0]);
	}
	if (hdl._mode & wr) {
		wc--;
		FD_CLR(hdl.fd, &masks[1]);
	}
	if (hdl._mode & ex) {
		ec--;
		FD_CLR(hdl.fd, &masks[2]);
	}
}
	


/*
 *  sfile_handler_select_t::prepare(timeout)
 *
 *  Prepare the sfile to wait on file events for timeout milliseconds.
 */

w_rc_t sfile_handler_select_t::prepare(const stime_t &timeout, bool forever)
{
	if (forever)
		tvp = 0;
	else {
		tval = timeout;
		tvp = &tval;
	}

	fd_set *rset = rc ? &ready[0] : 0;
	fd_set *wset = wc ? &ready[1] : 0;
	fd_set *eset = ec ? &ready[2] : 0;

	if (rset || wset || eset)
		memcpy(ready, masks, sizeof(masks));
	w_assert1(dtbsz <= maxfds);

#ifdef DEBUG
	if (gotenv > 1) {
		gotenv = 0;
		cerr << "select():" << endl;
		if (rset)
			cerr << "\tread_set:" << *rset << endl; 
		if (wset)
			cerr << "\twrite_set:" << *wset << endl;
		if (eset)
			cerr << "\texcept_set:" << *eset << endl;

		cerr << "\ttimeout= ";
		if (tvp)
			cerr << timeout;
		else
			cerr << "INDEFINITE";

		if (timeout == stime_t(0))
			cerr << " (POLL)";

		cerr << endl << endl;
	}
	if (fd_allzero(rset) && fd_allzero(wset) && fd_allzero(eset)) {
		w_assert1(tvp!=0);
	}
#else
	w_assert1(rset || wset || eset || tvp);
#endif

	any[0] = rset;
	any[1] = wset;
	any[2] = eset;

#ifdef EXPENSIVE_STATS
	/* Account here, rather than in wait() to disrupt timing less */
	/* These stats are slightly different from the poll stats,
	   but consistent with the select model in that each bitmap is
	   it's own event.  Poll stuff could be adjusted to keep
	   track of other events on each fd */
	stats_wait(rc+wc+ec);
#endif

	return RCOK;
}


/*
 *  sfile_handler_select_t::wait()
 *
 *  Wait for any file events or for interrupts to occur.
 */
w_rc_t sfile_handler_select_t::wait()
{
	fd_set *rset = any[0];
	fd_set *wset = any[1];
	fd_set *eset = any[2];

	SthreadStats.selects++;

	int n = select(dtbsz, rset, wset, eset, tvp);
	int select_errno = errno;
	
	switch (n)  {
	case -1:
		if (select_errno != EINTR)  {
			cerr << "select():" << RC2(fcOS, select_errno) << endl;
			if (rset)
				cerr << "\tread_set:" << *rset << endl;
			if (wset)
				cerr << "\twrite_set:" << *wset << endl;
			if (eset)
				cerr << "\texcept_set:" << *eset << endl;
			cerr << endl;
		}
		else {
			// cerr << "EINTR " << endl;
			SthreadStats.eintrs++;
		}
		return RC2(sthread_t::stOS, select_errno);
		break;

	case 0:
		return RC(sthread_base_t::stTIMEOUT);
		break;

	default:
		SthreadStats.selfound++;
#ifdef EXPENSIVE_STATS
		/* XXX Could move this to dispatch() if # ready is tracked */
		stats_found(n);
#endif

		break;
	}

	return RCOK;
}


/*
 *  sfile_handler_select_t::dispatch()
 *
 *  Dispatch select() events to individual file handles.
 *
 *  Events are processed in order of priority.  At current, only two
 *  priorities, "read"==0 and "write"==1 are supported, rather crudely
 */

void sfile_handler_select_t::dispatch(const stime_t &at)
{
	/* any events to dispatch? */
	if (!(any[0] || any[1] || any[2]))
		return;

	last_event = at;

	direction = (direction == 0) ? 1 : 0;
	
	sfile_hdl_base_t *p;

	w_list_i<sfile_hdl_base_t> i(_list, direction);

	bool active[3];

	while ((p = i.next()))  {
		/* an iterator across priority would do better */
		if (!p->enabled() || p->priority() != 0)
			continue;

		active[0] = any[0] && (p->_mode & rd) && FD_ISSET(p->fd, ready+0);
		active[1] = any[1] && (p->_mode & wr) && FD_ISSET(p->fd, ready+1);
		active[2] = any[2] && (p->_mode & ex) && FD_ISSET(p->fd, ready+2);

		if (active[0] || active[1] || active[2]) {
			p->dispatch(at, active[0], active[1], active[2]);
			_hits[0] += (active[0] != 0);
			_hits[1] += (active[1] != 0);
			_hits[2] += (active[2] != 0);
		}
	}
	
	for (i.reset(_list);  (p = i.next()); )  {
		if (!p->enabled() || p->priority() != 1)
			continue;

		active[0] = any[0] && (p->_mode & rd) && FD_ISSET(p->fd, ready+0);
		active[1] = any[1] && (p->_mode & wr) && FD_ISSET(p->fd, ready+1);
		active[2] = any[2] && (p->_mode & ex) && FD_ISSET(p->fd, ready+2);

		if (active[0] || active[1] || active[2]) {
			p->dispatch(at, active[0], active[1], active[2]);
			_hits[0] += (active[0] != 0);
			_hits[1] += (active[1] != 0);
			_hits[2] += (active[2] != 0);
		}
	}
}


/* XXX This breaks if more than one event is received by
   the handle; need to add seperate masks. */

bool	sfile_handler_select_t::probe(sfile_hdl_base_t &hdl)
{
	int	n;
	fd_set	set;
	fd_set	*setp[3];
	struct	timeval tv;

	FD_ZERO(&set);
	FD_SET(hdl.fd, &set);
	tv.tv_sec = 0;
	tv.tv_usec = 0;

	setp[0] = (hdl._mode & rd) ? &set : 0;
	setp[1] = (hdl._mode & wr) ? &set : 0;
	setp[2] = (hdl._mode & ex) ? &set : 0;

	n = select(hdl.fd+1, setp[0], setp[1], setp[2], &tv);
	if (n == -1)
		cerr << "select_handler: probe fails:" << endl
		     << RC(fcOS) << endl; 

	return (n == 1);
}


/*
 *  sfile_handler_select_t::is_active(fd)
 *
 *  Return true if there is an active file handler for fd.
 */

bool sfile_handler_select_t::is_active(int fd)
{
	w_list_i<sfile_hdl_base_t> i(_list);
	sfile_hdl_base_t *p; 

	while ((p = i.next()))  {
		if (p->fd == fd)
			break;
	}

	return p != 0;
}


ostream &sfile_handler_select_t::print(ostream &o)
{
	o << "SELECT EVENTS:";
	if (_hits[0])
		W_FORM(o)(" %d read", _hits[0]);
	if (_hits[1])
		W_FORM(o)(" %d write", _hits[1]);
	if (_hits[2])
		W_FORM(o)(" %d except", _hits[2]);
	if (tvp)
		o << " last_timeout: " << sinterval_t(tval);

	if (_hits[0] || _hits[1] || _hits[2]) {
		sinterval_t	delta(stime_t::now() - last_event);
		o << ", last event " << delta << " seconds ago";
	}

	o << endl;
			
	w_list_i<sfile_hdl_base_t> i(_list);
	sfile_hdl_base_t *f;

	while ((f = i.next()))
		o << *f << endl;    

	return o;
}

/*<std-header orig-src='shore'>

 $Id: sfile_handler.cpp,v 1.21 1999/06/07 19:06:05 kupsch Exp $

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

#ifdef __GNUC__
#pragma implementation "sfile_handler.h"
#endif

#define W_INCL_LIST
#include <w.h>
#include <w_strstream.h>
#include <sthread.h>

#include <sfile_handler.h>

#include <w_statistics.h>
#include <unix_stats.h>
#include "sthread_stats.h"
extern class sthread_stats SthreadStats;

#ifdef EXPLICIT_TEMPLATE
template class w_list_t<sfile_hdl_base_t>;
template class w_list_i<sfile_hdl_base_t>;
#endif

#define	FD_NONE	(-1)	/* an invalid file descriptor value */


/*
 * File "Handlers" and "Handles".
 *
 * A means of generating thread wakeup events for thread's
 * awaiting I/O on non-blocking devices.
 */


sfile_handler_t::sfile_handler_t()
: _list(offsetof(sfile_hdl_base_t, _link))
{
}

/*
 *  sfile_handler_t::_start()
 *
 *  Add the file handle to the list of handles controlled by
 *  this handler.
 */

w_rc_t sfile_handler_t::_start(sfile_hdl_base_t &hdl)
{
	if (hdl.fd == FD_NONE)
		return RC(sthread_t::stBADFD);

	// already enabled?
	void *owner = hdl._link.member_of();
	if (owner) {
		/* enabled by a different handler?! */
		if (owner != &_list)
			return RC(sthread_t::stINUSE);
		return RCOK;
	}

	_list.append(&hdl);
	hdl._owner = this;
	
	return RCOK;
}


/*
 *  sfile_handler_t::_stop()
 *
 *  Remove the file handle from the list of handles controlled
 *  by this handler.
 */

w_rc_t	sfile_handler_t::_stop(sfile_hdl_base_t &hdl)
{
	void	*owner = hdl._link.member_of();
	if (!owner)
		return RC(sthread_t::stBADFD);	/*XXX overloaded*/
	else if (owner != &_list) {
		cerr.form("sfile_handler_t(%#lx): handle %#lx doesn't belong to me!\n",
			  (long) this, (long) &hdl);
		return RC(sthread_t::stINUSE);
	}

	hdl.disable();
	hdl._link.detach();
	hdl._owner = 0;

	return RCOK;
}


/*
 * Common accounting of events waited for and found.  Otherwise the
 * code is duplicated into each implementation.
 *
 * XXX This would be faster if stats allowed arrays.
 */

void	sfile_handler_t::stats_wait(unsigned waiting)
{
	switch (waiting) {
	case 0:
		SthreadStats.selw0++;
		break;
	case 1:
		SthreadStats.selw1++;
		break;
	case 2:
		SthreadStats.selw2++;
		break;
	case 3:
		SthreadStats.selw3++;
		break;
	case 4:
		SthreadStats.selw4++;
		break;
	default:
		SthreadStats.selwX++;
		break;
	}
}

void	sfile_handler_t::stats_found(unsigned found)
{
	switch (found) {
	case 1:
		SthreadStats.self1++;
		break;
	case 2:
		SthreadStats.self2++;
		break;
	case 3:
		SthreadStats.self3++;
		break;
	case 4:
		SthreadStats.self4++;
		break;
	default:
		SthreadStats.selfX++;
		break;
	}
}

ostream &operator<<(ostream &o, sfile_handler_t &h)
{
	return h.print(o);
}



/*
 *  sfile_hdl_base_t::sfile_hdl_base_t(fd, mask)
 */

sfile_hdl_base_t::sfile_hdl_base_t(int f, int m)
: fd(f),
  _mode(m),
  _enabled(false),
  _hits(0)
{
}


/*
 *  sfile_hdl_base_t::~sfile_hdl_base_t()
 */

sfile_hdl_base_t::~sfile_hdl_base_t()
{
	if (_link.member_of()) {
		W_FORM(cerr)("sfile_hdl_base_t(%#lx): destructed in place (%#lx) !!!\n",
			     (long)this, (long)_link.member_of());
		/* XXX how about printing the handler ?? */
		/* XXX try removing it from the list? */
	}
}


/*
 * Allow changes to the file descriptor if the handle isn't in-use.
 */

w_rc_t	sfile_hdl_base_t::change(int new_fd)
{
	if (enabled() || running())
		return RC(sthread_t::stINUSE);
	fd = new_fd;
	return RCOK;
}


bool sfile_hdl_base_t::running() const
{
	return (_link.member_of() != 0);
}


bool sfile_hdl_base_t::enabled() const
{
	return _enabled;
}


void sfile_hdl_base_t::enable()
{
	if (enabled())
		return;
	if (!running())
		return;

	_owner->enable(*this);
	_enabled = true;
}


void sfile_hdl_base_t::disable()
{
	if (!enabled())
		return;
	if (!running())
		return;

	_owner->disable(*this);
	_enabled = false;
}

/* Revector this to sthread for compatability */
bool sfile_hdl_base_t::is_active(int fd)
{
	return sthread_t::io_active(fd);
}

/*
 *  sfile_hdl_base_t::dispatch()
 *
 *  Execute the appropriate callbacks for the sfile event.
 */

void sfile_hdl_base_t::dispatch(const stime_t &at,
				bool read, bool write, bool except)
{
	_hits++;
	_last_event = at;

	if (read && (_mode & rd))
		read_ready();

	if (write && (_mode & wr))
		write_ready();

	if (except && (_mode & ex))
		exception_ready();
}


bool	sfile_hdl_base_t::probe()
{
	return _owner ? _owner->probe(*this) : false;
}


ostream &sfile_hdl_base_t::print(ostream &s) const
{
	W_FORM(s)("sfile(%#lx, fd %d, %s%s%s, %d hits", 
		  (long)this,
		  fd,
		  ((_mode & rd) ? (enabled() ? " READ" : " read") : ""),
		  ((_mode & wr) ? (enabled() ? " WRITE" : " write") : ""),
		  ((_mode & ex) ? (enabled() ? " EXCEPT" : " except") : ""),
		  _hits);
	if (_hits) {
		sinterval_t	delta(stime_t::now() - _last_event);
	    	s << ", " << delta << " seconds ago";
	}
	return s << ')';
}


ostream &operator<<(ostream &o, sfile_hdl_base_t &h)
{
	return h.print(o);
}


sfile_safe_hdl_t::sfile_safe_hdl_t(int fd, int mode)
: sfile_hdl_base_t(fd, mode),
  _shutdown(false)
{
	char buf[20];
	ostrstream s(buf, sizeof(buf));
	s << fd << "_";
	if (_mode & rd)
		s << "R";
	if (_mode & wr)
		s << "W";
	if (_mode & ex)
		s << "X";
	s << ")" << ends;
	buf[sizeof(buf)-1] = '\0';
	sevsem.setname("sfile(", buf);
}


sfile_safe_hdl_t::~sfile_safe_hdl_t()
{
} 


w_rc_t	sfile_safe_hdl_t::change(int new_fd)
{
	W_DO(sfile_hdl_base_t::change(new_fd));

	char buf[40];
	ostrstream s(buf, sizeof(buf));
	s << fd << "_";
	if (_mode & rd)
		s << "R";
	if (_mode & wr)
		s << "W";
	if (_mode & ex)
		s << "X";
	s << ")" << ends;
	buf[sizeof(buf)-1] = '\0';
	sevsem.setname("sfile(", buf);

	return RCOK;
}

w_rc_t 
sfile_safe_hdl_t::wait(long timeout)
{
	if (is_down())  {
		return RC(sthread_t::stBADFILEHDL);
	} 

	// XXX overloaded error code?
	if (!running())
		return RC(sthread_t::stBADFILEHDL);

	enable();

	W_DO( sevsem.wait(timeout) );

	if (is_down())
		return RC(sthread_t::stBADFILEHDL);

	W_IGNORE( sevsem.reset() );

	return RCOK;
}

/*
 *  sfile_safe_hdl_t::shutdown()
 *
 *  Shutdown this file handler. If a thread is waiting on it, 
 *  wake it up and tell it the bad news.
 */

void
sfile_safe_hdl_t::shutdown()
{
	_shutdown = true;
	W_IGNORE(sevsem.post());
}

void 
sfile_safe_hdl_t::read_ready()
{
	W_IGNORE(sevsem.post());
	disable();
}

void 
sfile_safe_hdl_t::write_ready()
{
	W_IGNORE(sevsem.post());
	disable();
}

void 
sfile_safe_hdl_t::exception_ready()
{
	W_IGNORE(sevsem.post());
	disable();
}



sfile_compat_hdl_t::sfile_compat_hdl_t(int fd, int mode)
: sfile_safe_hdl_t(fd, mode)
{
	w_assert3(fd >= 0);
	w_assert3(mode == rd || mode == wr || mode == ex);

	W_COERCE(sthread_t::io_start(*this));
}

sfile_compat_hdl_t::~sfile_compat_hdl_t()
{
	W_COERCE(sthread_t::io_finish(*this));
}

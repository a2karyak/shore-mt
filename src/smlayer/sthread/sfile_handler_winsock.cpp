/*<std-header orig-src='shore'>

 $Id: sfile_handler_winsock.cpp,v 1.22 2000/02/01 23:57:54 bolo Exp $

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

#include <w.h>
#include <w_workaround.h>
#include <w_stream.h>
#include <w_strstream.h>
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

#include <os_fcntl.h>

#ifndef _WINDOWS
#include <sys/wait.h>
#include <new.h>
#endif

#ifdef __GNUC__
#pragma implementation "sfile_handler.h"
#endif

#define W_INCL_LIST
#include <w.h>
#include <w_debug.h>
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


#define FD_NONE	-1	/* invalid unix file descriptor */
#if defined(_WINDOWS) && defined(notdef)
#undef FD_ISSET
#define FD_ISSET(a,b) __fd_is_set(a,b)
extern "C" int __fd_is_set(SOCKET fd, const fd_set FAR *set) ;
int __fd_is_set(SOCKET fd, const fd_set FAR *set) 
{
    u_int i;
    for (i=0; i< set->fd_count; i++) {
	if( set->fd_array[i] == fd) {
	    return 1;
	}
    }
    return 0;
}
#endif

#ifdef HPUX8
inline int select(int nfds, fd_set* rfds, fd_set* wfds, fd_set* efds,
	      struct timeval* t)
{
    return select(nfds, (int*) rfds, (int*) wfds, (int*) efds, t);
}
#else

#if !defined(AIX32) && !defined(AIX41) && !defined (_WINDOWS)
extern "C" int select(int, fd_set *, fd_set *, fd_set *, struct timeval *);
#endif

#endif /*HPUX8*/

/*
 *  operator<<(o, fd_set)
 *
 *  Print an fd_set.
 */

ostream& operator<<(ostream& o, const fd_set &s) 
{
#ifdef FD_SETSIZE
	int	maxfds = FD_SETSIZE;
#else
	static int maxfds = 0;
	if (maxfds == 0) {
		maxfds = sysconf(_SC_OPEN_MAX);
	}
#endif /* FD_SETSIZE */
	o << "maxfds=" << maxfds << "; ";

#ifdef _WINDOWS
	o << "fd_count=" << s.fd_count << " ";
	for (unsigned int n = 0; n < s.fd_count; n++)  {
		o << " " << s.fd_array[n];
	}
#else
	for (int n = 0; n < maxfds; n++)  {
		if (FD_ISSET(n, &s))  {
			o << " " << n;
		}
	}
#endif /*_WINDOWS */
	return o;
}

/*********************************************************************
 *
 *  check for all-zero mask, or for null ptr to mask
 *
 *  Not just for debugging anymore.
 *
 *********************************************************************/

bool
fd_allzero(const fd_set *s)
{
	if(s == 0) {
		return true;
	}

#ifdef _WINDOWS
	if(s->fd_count == 0) {
		return true;
	} else {
		return false;
	}
#else
	DBGTHRD(<<"");
#ifdef FD_SETSIZE
	int	maxfds = FD_SETSIZE;
#else
	static long maxfds = 0;
	if (maxfds == 0) {
		maxfds = sysconf(_SC_OPEN_MAX);
	}
#endif /* FD_SETSIZE */

	// return true if s is null ptr or it
	// points to an empty mask
	if (s==0) {
		DBGTHRD(<<"");
		return true;
	}

	for (int n = 0; n < maxfds; n++)  {
		if (FD_ISSET(n, s)) {
			DBGTHRD(<<"");
			return false;
		}
	}
	DBGTHRD(<<"");
	return true;

#endif /*_WINDOWS*/
}

#ifdef W_DEBUG
static int gotenv = 0;	// hold over from sthreads integration
#endif


/*
 * File "Handlers" and "Handles".
 *
 * A means of generating thread wakeup events for thread's
 * awaiting I/O on non-blocking devices.
 */


sfile_handler_t::sfile_handler_t()
    : _list(offsetof(sfile_hdl_base_t, _link))
{
	memset(masks, '\0', sizeof(masks));// FD_ZERO(masks);
	memset(ready, '\0', sizeof(ready));// FD_ZERO(ready);

	any[0] = any[1] = any[2] = 0;

	direction = false;
	rc = wc = ec = 0;

	dtbsz = 0;

	tvp = 0;
	tval.tv_sec = tval.tv_usec = 0;
}


/*
 *  sfile_handler_t::enable()
 *
 * Allow this sfile to receive events.
 */

w_rc_t sfile_handler_t::start(sfile_hdl_base_t &hdl)
{
	DBG(<<"sfile_hdl_t::enable(" << hdl << ")");

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
	
	if (hdl.fd >= dtbsz)
		dtbsz = hdl.fd + 1;

	return RCOK;
}


/*
 *  sfile_handler_t::disable()
 *
 *  Stop dispatching events for a sfile.
 */

void sfile_handler_t::stop(sfile_hdl_base_t &hdl)
{
	DBGTHRD(<<"sfile_hdl_base_t::disable(" << hdl << ")");

	void	*owner = hdl._link.member_of();
	if (!owner)
		return;
	else if (owner != &_list) {
		W_FORM2(cerr,("sfile_handler_t(%#lx): handle %#lx doesn't belong to me!\n",
			  (long) this, (long) &hdl));
		return;
	}

	hdl.disable();
	hdl._link.detach();
	hdl._owner = 0;

#if 0
	/* XXX what if there is another event on the same descriptor ? */
	if (hdl.fd == dtbsz - 1)
		--dtbsz;
#endif
}


void sfile_handler_t::enable(sfile_hdl_base_t &hdl)
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


void sfile_handler_t::disable(sfile_hdl_base_t &hdl)
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
 *  sfile_handler_t::prepare(timeout)
 *
 *  Prepare the sfile to wait on file events for timeout milliseconds.
 */
#ifdef _WINDOWS
static SOCKET __dummyfd = INVALID_SOCKET;
#endif

w_rc_t sfile_handler_t::prepare(const stime_t &timeout, bool forever)
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

	if (rset || wset || eset) {
		memcpy(ready, masks, sizeof(masks));
	}
#ifndef _WINDOWS
	w_assert1(dtbsz <= FD_SETSIZE);
#endif

#ifdef W_DEBUG
	if (gotenv > 1) {
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
		if ((float) timeout==0.0)
			cerr << " (POLL)";
		cerr << endl << endl;
	}
	if (fd_allzero(rset) && fd_allzero(wset) && fd_allzero(eset)) {
	    w_assert1(tvp!=0);
	}
#else
	w_assert1(rset || wset || eset || tvp);
#endif
#ifdef _WINDOWS
	if (fd_allzero(rset) && fd_allzero(wset) && fd_allzero(eset)) {
	    w_assert1(tvp!=0);
	    /* Grot: put in a dummy socket */
	    if(__dummyfd == INVALID_SOCKET) {
			__dummyfd = socket(AF_INET, SOCK_STREAM, 0);
			DBGTHRD(<<"__dummyfd=" << __dummyfd
			<< " INVALID_SOCKET = " << INVALID_SOCKET);
			w_assert3(__dummyfd != INVALID_SOCKET);
	    }
	    if(!eset) eset = &ready[2];
	    w_assert3(eset);
	    if(! FD_ISSET(__dummyfd, eset)) {
			FD_SET(__dummyfd, eset);
			w_assert3(!fd_allzero(eset));
	    }
	    w_assert3(!fd_allzero(eset));
	}
#endif

	any[0] = rset;
	any[1] = wset;
	any[2] = eset;

#ifdef W_DEBUG
	if (gotenv > 1) {
		if (rset)
			cerr << "\t*any[0]:" << *rset << endl; 
		if (wset)
			cerr << "\t*any[1]:" << *wset << endl;
		if (eset)
			cerr << "\t*any[2]:" << *eset << endl;

		cerr << endl << endl;
	}
#endif
#ifdef EXPENSIVE_STATS
	/* XXXX lots cheaper if stats allowed arrays */
	switch (rc+wc+ec) {
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
#endif

	return RCOK;
}


/*
 *  sfile_handler_t::wait()
 *
 *  Wait for any file events or for interrupts to occur.
 */
w_rc_t sfile_handler_t::wait()
{
	fd_set *rset = any[0];
	fd_set *wset = any[1];
	fd_set *eset = any[2];

	SthreadStats.selects++;

	DBGTHRD(<<"select: dtbsz=" << dtbsz
		<< " Rset=" << rset
		<< " Wset=" << wset
		<< " Eset=" << eset
		<< " tvp =" << tvp
	);
	if(tvp) {
	    DBGTHRD(<<"select: timeout is:" << tvp->tv_sec << "." << tvp->tv_usec);
	}

	int n = select(dtbsz, rset, wset, eset, tvp);
#ifdef _WINDOWS
	DWORD select_errno = WSAGetLastError();
#else
	int select_errno = errno;
#endif

#ifdef _WINDOWS
	if (!fd_allzero(eset)) {
	    /* Grot: remove dummy socket */
		if(__dummyfd != INVALID_SOCKET) {
			w_assert3(eset);
			if(FD_ISSET(__dummyfd, eset)) {
				FD_CLR(__dummyfd, eset);
			}
			w_assert3(!FD_ISSET(__dummyfd, eset));
	    }
	}
#endif
	
	switch (n)  {
	case -1:
#ifdef _WINDOWS
	    if (select_errno == WSAENOTSOCK) return RC(sthread_t::stOS);
	    if (select_errno == WSAEINVAL) {
		if ((tvp==0)||(tvp->tv_usec==0 && tvp->tv_sec==0)) {
		    DBG(<<"Converted error.");
		    select_errno = WSAEINTR;
		} else {
		    if(tvp) {
			cerr <<"select: timeout is:" 
			    << tvp->tv_sec
			    << "." << tvp->tv_usec
			<< endl;
		    } else {
			cerr <<"select: null timeout ptr"  <<endl;
		    }
		}
	    }
	    if (select_errno != WSAEINTR) {
		    w_rc_t e= RC2(fcWIN32, select_errno);
#else
	    if (select_errno != EINTR)  {
		    w_rc_t e = RC2(fcOS, select_errno);
#endif	
		    cerr << "select():" << endl << e << endl;
		    if (rset)
			    cerr << "\tread_set:" << *rset << endl;
		    if (wset)
			    cerr << "\twrite_set:" << *wset << endl;
		    if (eset)
			    cerr << "\texcept_set:" << *eset << endl;
		    cerr << endl;
		    cerr << "file handlers are ..." << endl;
		    cerr << *this << endl;
	    } /* } */ else {
		    cerr << "EINTR " << endl;
		    SthreadStats.eintrs++;
	    }
	    return RC(sthread_t::stOS);
	    break;

	case 0:
	    return RC(sthread_base_t::stTIMEOUT);
	    break;

	default:
	    SthreadStats.selfound++;
#ifdef EXPENSIVE_STATS
	    /* XXXX lots cheaper if stats allowed arrays */
	    switch (n) {
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
#endif
	    break;
	}

	return RCOK;
}


/*
 *  sfile_handler_t::dispatch()
 *
 *  Dispatch select() events to individual file handles.
 *
 *  Events are processed in order of priority.  At current, only two
 *  priorities, "read"==0 and "write"==1 are supported, rather crudely
 */

void sfile_handler_t::dispatch(const stime_t &)
{
	DBGTHRD(<<"sfile_hdl_base_t::dispatch");

	/* any events to dispatch? */
	if (!(any[0] || any[1] || any[2]))
		return;

	direction = !direction;
	
	sfile_hdl_base_t *p;

	w_list_i<sfile_hdl_base_t> i(_list, direction);

	bool active[3];

	while ((p = i.next()))  {
		/* an iterator across priority would do better */
		DBGTHRD("dispatch: p->priority=" << p->priority());
		if (p->priority() != 0)
			continue;

		active[0] = any[0] && FD_ISSET(p->fd, ready+0);
		active[1] = any[1] && FD_ISSET(p->fd, ready+1);
		active[2] = any[2] && FD_ISSET(p->fd, ready+2);

		if (active[0] || active[1] || active[2]) {
			DBGTHRD("dispatch: p->fd=" << p->fd
			    << " : " << active[0]
			    << " : " << active[1]
			    << " : " << active[2]
			);
			p->dispatch(active[0], active[1], active[2]);
		}
	}
	
	for (i.reset(_list);  (p = i.next()); )  {
		DBGTHRD("dispatch: p->priority=" << p->priority());
		if (p->priority() != 1)
			continue;

		active[0] = any[0] && FD_ISSET(p->fd, ready+0);
		active[1] = any[1] && FD_ISSET(p->fd, ready+1);
		active[2] = any[2] && FD_ISSET(p->fd, ready+2);

		if (active[0] || active[1] || active[2]) {
			DBGTHRD("dispatch: p->fd=" << p->fd
			    << " : " << active[0]
			    << " : " << active[1]
			    << " : " << active[2]
			);
			p->dispatch(active[0], active[1], active[2]);
                }
	}
}



/*
 *  sfile_handler_t::is_active(fd)
 *
 *  Return true if there is an active file handler for fd.
 */

bool sfile_handler_t::is_active(file_descriptor fd)
{
	w_list_i<sfile_hdl_base_t> i(_list);
	sfile_hdl_base_t *p; 

	while ((p = i.next()))  {
		if (p->fd == fd)
			break;
	}

	return p != 0;
}


ostream &sfile_handler_t::print(ostream &o)
{
	w_list_i<sfile_hdl_base_t> i(_list);
	sfile_hdl_base_t* f = i.next();

	if (f)  {
		o << "waiting on FILE EVENTS:" << endl;
		do {
			f->print(o);
		} while ((f = i.next()));
	}

	return o;
}

ostream &operator<<(ostream &o, sfile_handler_t &h)
{
	return h.print(o);
}



/*
 *  sfile_hdl_base_t::sfile_hdl_base_t(fd, mask)
 */

sfile_hdl_base_t::sfile_hdl_base_t(file_descriptor f, int m)
: fd(f),
  _mode(m),
  _enabled(false)
{
}    


/*
 *  sfile_hdl_base_t::~sfile_hdl_base_t()
 */

sfile_hdl_base_t::~sfile_hdl_base_t()
{
	if (_link.member_of()) {
		W_FORM2(cerr,("sfile_hdl_base_t(%#lx): destructed in place (%#lx) !!!\n",
			     (long)this, (long)_link.member_of()));
		/* XXX how about printing the handler ?? */
		/* XXX try removing it from the list? */
	}
}


/*
 * Allow changes to the file descriptor if the handle isn't in-use.
 */

w_rc_t	sfile_hdl_base_t::change(file_descriptor new_fd)
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
bool sfile_hdl_base_t::is_active(file_descriptor fd)
{
	return sthread_t::io_active(fd);
}

/*
 *  sfile_hdl_base_t::dispatch()
 *
 *  Execute the appropriate callbacks for the sfile event.
 */

void sfile_hdl_base_t::dispatch(bool read, bool write, bool except)
{
    if (read && (_mode & rd))
	    read_ready();

    if (write && (_mode & wr))
	    write_ready();

    if (except && (_mode & ex))
	    exception_ready();
}


ostream &sfile_hdl_base_t::print(ostream &s) const
{
	W_FORM2(s,("sfile=%#lx [%sed] fd=%d mask=%s%s%s", 
	       (long)this,
	       enabled() ? "enabl" : "disabl",
	       fd,
	       (_mode & rd) ? " rd" : "",
	       (_mode & wr) ? " wr" : "",
	       (_mode & ex) ? " ex" : ""));

#if 0
	if (enabled()) {
		s.form("ready[%s%s%s ]  masks[%s%s%s ]",
	       (_mode & rd) && FD_ISSET(fd, &ready[0]) ? " rd" : "",
	       (_mode & wr) && FD_ISSET(fd, &ready[1]) ? " wr" : "",
	       (_mode & ex) && FD_ISSET(fd, &ready[2]) ? " ex" : "",
	       (_mode & rd) && FD_ISSET(fd, &masks[0]) ? " rd" : "",
	       (_mode & wr) && FD_ISSET(fd, &masks[1]) ? " wr" : "",
	       (_mode & ex) && FD_ISSET(fd, &masks[2]) ? " ex" : ""
	       );
	}
#endif

	return s << '\n';
}


ostream &operator<<(ostream &o, sfile_hdl_base_t &h)
{
	return h.print(o);
}


sfile_safe_hdl_t::sfile_safe_hdl_t(file_descriptor fd, int mode)
: sfile_hdl_base_t(fd, mode),
  _shutdown(false)
{
	char buf[20];
	ostrstream s(buf, sizeof(buf));
	s << fd << ends;
	sevsem.setname("sfile ", buf);
}


sfile_safe_hdl_t::~sfile_safe_hdl_t()
{
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



sfile_compat_hdl_t::sfile_compat_hdl_t(file_descriptor fd, int mode)
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


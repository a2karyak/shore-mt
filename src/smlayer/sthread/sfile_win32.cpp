/*<std-header orig-src='shore'>

 $Id: sfile_win32.cpp,v 1.8 2002/01/05 18:17:16 bolo Exp $

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

#include <memory.h>
#include <string.h>

#include <w.h>
#include <w_rc.h>

#include <sthread.h>
#include <sfile_win32.h>

#include <w_winsock.h>

/*
 * Convert the socket to the underling win32 handle.
 */
#define	sock2h(sock)	((HANDLE) (sock))

#if defined(SFILE_DEBUG) || defined(SFILE_DEBUG_IO)
static ostream &operator<<(ostream &o, const struct sockaddr &addr);
#endif


sfile_t::sfile_t(SOCKET handle, int mode)
: _handle(INVALID_SOCKET),
  _mode(CLOSED)
{
	w_rc_t		e;
	unsigned	i;

	for (i = 0; i < 3; i++) {
		memset(_async + i, '\0', sizeof(OVERLAPPED));
		_async[i].hEvent = INVALID_HANDLE_VALUE;
	}

	if (handle != INVALID_SOCKET) {
		e = _open(handle, convert_mode(mode));
		W_COERCE(e);
	}
}

sfile_t::~sfile_t()
{
	if (_handle != INVALID_SOCKET)
		W_FORM(cerr)("~sfile_t(%#lx): warning: open file %#x\n",
			  (long)this, _handle);

	W_IGNORE(close());		// bye-bye
}


int sfile_t::convert_mode(int unixmode)
{
	int	open_mode = 0;
	switch (unixmode) {
	case OPEN_RDONLY:
		open_mode = READING;
		break;
	case OPEN_WRONLY:
		open_mode = WRITING;
		break;
	case OPEN_RDWR:
		open_mode = READ_WRITE;
		break;
	}
	return open_mode;
}

/*
   XXX almost all of the non-blocking ops need to be
   fixed to return EnotOpen (or whaterver) if their is no FD!

   Only selection of blocking / nonblocking works now
 */

w_rc_t sfile_t::setFlag(int the_flag, bool set_them)
{
	int	r;
	unsigned long	flags;	/* XXX DWORD? */
	w_rc_t	e;

	if (the_flag != FIONBIO)
		return RC(fcNOTIMPLEMENTED);

	flags = set_them ? 1 : 0;

	r = ::ioctlsocket(_handle, the_flag, &flags);

	return MAKERC(r == SOCKET_ERROR, fcWIN32);
}


/* XXX overall error handling of NT failures */
w_rc_t	sfile_t::wait(int which, long timeout)
{
	w_rc_t	e;
	bool	ok;
	int	n;

	/* XXX don't need error checks in internal vectors */
	if (_mode == CLOSED)
		return RC(sfCLOSED);

	/* XXX check for compatabile modes? */

	if (which == for_read || which == for_write)
		return _event[which].wait(timeout);

	ok = ResetEvent(_async[2].hEvent);
	if (!ok)
		return RC(fcWIN32);

	/* allow generation of the appropriate event */

	int	flag = which == for_accept ? FD_ACCEPT : FD_CONNECT;
	n = WSAEventSelect(_handle, _async[2].hEvent, flag);
	if (n == SOCKET_ERROR)
		return RC(fcWIN32);

	e = _event[2].wait(timeout);

	/* Cancel event generation.  Also prevents accepted
	   socket from inheriting the FD_ACCEPT event. */

	n = WSAEventSelect(_handle, _async[2].hEvent, 0);
	if (n == SOCKET_ERROR) {
#if 1
		/* This is having random failure problems; the system
		   works with the error.  In the meantime, it will
		   output the error message so this more information can
		   be collected.  I think I know what the problem is,
		   but don't have time this week. */
#if defined(W_DEBUG) || defined(SFILE_DEBUG)
		w_rc_t	e2 = RC(fcWIN32);
		cerr << "sfile_t::wait(" << _handle << "):"
			<< " Ignoring reset event error on "
			<< (which == for_accept ? "FD_ACCEPT" : "FD_CONNECT")
			<< ":" << endl << e2 << endl;
#endif
#else
		return RC(fcWIN32);
#endif
	}

	return e;
}

w_rc_t	sfile_t::_open(SOCKET handle, int mode)
{
	w_rc_t	e;
	HANDLE	h;
	SECURITY_ATTRIBUTES sa;

	if (_mode != CLOSED)
		return RC(sfOPEN);

	/* create a security attribute that inherits whatever is in use */
	sa.nLength = sizeof(sa);
	sa.lpSecurityDescriptor = 0;
	sa.bInheritHandle = TRUE;

	/* XXX should track socket mode.
	   0 = closed
	   1 = socket() either accepting or connecting
	   2 = socket() read/write
	   */

	/* All errors are fatal for now, instead of cleaning up
	   It may be easier to create all the NT objects with
	   object creating, so cleanup isn't as messy. */

	/* XXX The events are manual reset to allow the use
	   of the wait==true in GetOverlappedResults().  OTW the
	   event is reset by WaitFor...() and further Waits
	   for the event will wait for the *next* occurence of
	   the requested event!  The winsock or win32 I/O handlers
	   will reset the event flags for us, so we don't need
	   to reset them manually all the time.  Need to add
	   a loop around GetOverlappedResults() incase the I/O
	   is not truely done in that case (because wait flag
	   must be false) */

	if (mode & READING) {
		h = CreateEvent(&sa, TRUE, FALSE, 0);
		if (h == NULL)
			return RC(fcWIN32);
		_async[0].hEvent = h;
		/* XXX cleanup */
		W_COERCE(_event[0].change(h));
		W_COERCE(sthread_t::io_start(_event[0]));
	}

	if (mode & WRITING) {
		h = CreateEvent(&sa, TRUE, FALSE, 0);
		if (h == NULL)
			W_FATAL(fcWIN32);

		_async[1].hEvent = h;
		W_COERCE(_event[1].change(h));
		W_COERCE(sthread_t::io_start(_event[1]));
	}

	/* XXX why is this manual reset.  The OverlappedResults
	   problem doesn't exist here.  Leave this manual reset
	   for now. */
	h = CreateEvent(&sa, TRUE, FALSE, 0);
	if (h == NULL)
		W_FATAL(fcWIN32);
	_async[2].hEvent = h;
	W_COERCE(_event[2].change(h));
	W_COERCE(sthread_t::io_start(_event[2]));

	_mode = mode;
	_handle = handle;

#if 0
	W_COERCE(setFlag(FIONBIO));
#else
	e = setFlag(FIONBIO);
 	if (e) {
		cerr << "setflag(FIONBIO) fails:" << endl << e << endl;
		W_COERCE(e);
	}
#endif

	return RCOK;
}

w_rc_t	sfile_t::cancel()
{
	if (_mode == CLOSED)
		return RC(sfCLOSED);

#ifdef SFILE_DEBUG
	W_FORM(cout)("sfile_t(handle=%#x)::cancel()\n", _handle);
#endif
	if (_mode & READING)
		_event[0].shutdown();
	if (_mode & WRITING)
		_event[1].shutdown();

	_event[2].shutdown();

	return RCOK;
}

w_rc_t	sfile_t::fdopen(SOCKET handle, int mode)
{
	return _open(handle, convert_mode(mode));
}

w_rc_t	sfile_t::socket(int domain, int type, int protocol)
{
	SOCKET	s;
	w_rc_t	e;

	if (_mode != CLOSED)
		return RC(sfOPEN);

	s = ::socket(domain, type, protocol);
	if (s == INVALID_SOCKET)
		return RC(fcWIN32);

	e = _open(s, READ_WRITE);
	if (e != RCOK)
		::closesocket(s);

	return e;
}

w_rc_t	sfile_t::close()
{
	int	i;

	if (_mode == CLOSED)
		return RC(sfCLOSED);

#ifdef SFILE_DEBUG
	W_FORM(cout)("sfile_t(handle=%#x)::close()\n", _handle);
#endif

	if (_mode & READING) {
		_event[0].shutdown();
		W_COERCE(sthread_t::io_finish(_event[0]));
	}
	if (_mode & WRITING) {
		_event[1].shutdown();
		W_COERCE(sthread_t::io_finish(_event[1]));
	}

	_event[2].shutdown();
	W_COERCE(sthread_t::io_finish(_event[2]));

	for (i = 0; i < 3; i++) {
		CloseHandle(_async[i].hEvent);
		_async[i].hEvent = INVALID_HANDLE_VALUE;
	}

	closesocket(_handle);

	_handle = INVALID_SOCKET;
	_mode = CLOSED;

	return RCOK;
}

w_rc_t	sfile_t::recv(void *where, int size, int _flags, int &numbytes)
{
	long unsigned	n = 0;
	bool	ok;
	int	err;
	const	int	me = for_read;
	w_rc_t	e;
	WSABUF	vec;
	unsigned long	flags = _flags;

	if (_mode == CLOSED)
		return RC(sfCLOSED);
	if (!(_mode & READING))
		return RC(sfWRONGMODE);

	vec.len = size;
	vec.buf = (char *) where;

	err = WSARecv(_handle, &vec, 1, &n, &flags, _async+me, 0);
	if (err == SOCKET_ERROR) {
		err = GetLastError();
		if (err == ERROR_NETNAME_DELETED) {
#ifdef SFILE_DEBUG
			W_FORM(cerr)("sfile_t(%#lx, mode=%d, handle %#lx): read netname deleted\n",
				(long) this, _mode, (long)_handle);
#endif
			return RC(sthread_t::stBADFILEHDL);
		}
		else if (err != ERROR_IO_PENDING) {
#ifdef SFILE_DEBUG
			e = RC2(fcWIN32, err);
			W_FORM(cerr)("sfile_t(%#lx, mode=%d): Unexpected WSARecv(%#lx, %#lx, %u) %d read %#x flags error:\n",
				(long)this, _mode,
				(long)_handle, (long)where, size, n, flags);
			cerr << e << endl;
			return e;
#else
			return RC2(fcWIN32, err);
#endif
		}

		e = wait(me);
		if (e && e.err_num() == sthread_t::stBADFILEHDL)
			return e;
		else
			W_COERCE(e);

		ok = WSAGetOverlappedResult(_handle, _async+me, &n, TRUE, &flags);
		if (!ok) {
			err = GetLastError();
			if (err == ERROR_NETNAME_DELETED)
				return RC(sthread_t::stBADFILEHDL);

			return RC2(fcWIN32, err);
		}
	}

	if (n == 0) {
		numbytes = 0;
		e = RC(sfEOF);
	}
	else
		numbytes = n;

	return e;
}

w_rc_t	sfile_t::send(const void *where, int size, int _flags, int &numbytes)
{
	long unsigned n = 0;
	bool	ok;
	int	err;
	const	int	me = for_write;
	w_rc_t	e;
	WSABUF	vec;
	unsigned long flags = _flags;

	if (_mode == CLOSED)
		return RC(sfCLOSED);
	if (!(_mode & WRITING))
		return RC(sfWRONGMODE);

	vec.len = size;
	vec.buf = (char *) where;

	err = WSASend(_handle, &vec, 1, &n, flags, _async+me, 0);
	if (err == SOCKET_ERROR) {
		err = GetLastError();
		if (err == ERROR_NETNAME_DELETED) {
#ifdef SFILE_DEBUG
			W_FORM(cerr)("sfile_t(%#lx, mode=%d, handle %#lx): write netname deleted\n",
				(long) this, _mode, (long)_handle);
			return RC(sthread_t::stBADFILEHDL);
#endif
		}
		else if (err != ERROR_IO_PENDING) {
#ifdef SFILE_DEBUG
			e = RC(fcWIN32, err);
			W_FORM(cerr)("sfile_t(%#lx, mode=%d): Unexpected WSASend(%#lx, %#lx, %u) %d written %#x flag serror:\n",
				(long)this, _mode,
				(long)_handle, (long)where, size, n, flags);
			cerr << e << endl;
			return e;
#else
			return RC2(fcWIN32, err);
#endif
		}

		e = wait(me);
		if (e && e.err_num() == sthread_t::stBADFILEHDL)
			return e;
		else
			W_COERCE(e);

		ok = WSAGetOverlappedResult(_handle, _async+me, &n, TRUE, &flags);
		if (!ok) {
			err = GetLastError();
			if (err == ERROR_NETNAME_DELETED)
				return RC(sthread_t::stBADFILEHDL);

			return RC2(fcWIN32, err);
		}
	}

	if (n == 0)
		e = RC(sfEOF);
	numbytes = n;

	return e;
}

/*
 * Have accept just wait for an accept event without
 * issuing an accept.
 */
// #define ACCEPT_WAIT_FIRST


w_rc_t	sfile_t::accept(struct sockaddr *a, int *l, sfile_t &acceptor)
{
	w_rc_t	e;
	SOCKET	s;
#ifndef ACCEPT_WAIT_FIRST
	DWORD	err;
#endif

#ifdef SFILE_DEBUG
	W_FORM(cout)("sfile(handle=%#x)::accept()\n", _handle);
#endif

	if (_mode == CLOSED)
		return RC(sfCLOSED);

	if (acceptor.isOpen())
		return RC(sfOPEN);

#ifdef ACCEPT_WAIT_FIRST
	e = wait(for_accept);
	if (e && e.err_num() == sthread_t::stBADFILEHDL)
		return e;
	else
		W_COERCE(e);

#ifdef SFILE_DEBUG
	W_FORM(cout)("sfile(handle=%#x)::accept() ... reaccept\n", _handle);
#endif
	s = ::accept(_handle, a, l);
	if (s == INVALID_SOCKET)
		return RC(fcWIN32);
#else
	s = ::accept(_handle, a, l);
	if (s == INVALID_SOCKET) {
		err = GetLastError();
		if (err != WSAEWOULDBLOCK)
			return RC2(fcWIN32, err);

#ifdef SFILE_DEBUG
		W_FORM(cout)("sfile(handle=%#x)::accept() ... async\n", _handle);
#endif

		e = wait(for_accept);
		if (e && e.err_num() == sthread_t::stBADFILEHDL)
			return e;
		else
			W_COERCE(e);

#ifdef SFILE_DEBUG
		W_FORM(cout)("sfile(handle=%#x)::accept() ... reaccept\n", _handle);
#endif
		s = ::accept(_handle, a, l);
		if (s == INVALID_SOCKET)
			return RC(fcWIN32);
	}
#endif


#ifdef SFILE_DEBUG
	W_FORM(cout)("sfile(handle=%#x)::accept() new=%#x addr=",
		_handle, s);
	cout << *a << endl;
#endif

	e = acceptor.fdopen(s, OPEN_RDWR);
	if (e != RCOK)
		::closesocket(s);

	return e;
}


w_rc_t	sfile_t::connect(struct sockaddr *name, int size)
{
	int	err;
	int	n;
	w_rc_t	e;

#ifdef SFILE_DEBUG
	W_FORM(cout)("sfile(handle=%#x)::connect(", _handle);
	cout << *name << ")" << endl;
#endif
	if (_mode == CLOSED)
		return RC(sfCLOSED);

	n = ::connect(_handle, name, size);
	if (n == SOCKET_ERROR) {
		err = GetLastError();
		if (err != WSAEWOULDBLOCK)
			return RC2(fcWIN32, err);

#ifdef SFILE_DEBUG
		W_FORM(cout)("sfile(handle=%#x)::connect() ... async\n", _handle);
#endif

		e = wait(for_connect);
		if (e && e.err_num() == sthread_t::stBADFILEHDL)
			return e;
		else
			W_COERCE(e);

#ifdef SFILE_DEBUG
		W_FORM(cout)("sfile(handle=%#x)::connect() ... reconnect\n",
			     _handle);
#endif

#if 0
		n = ::connect(_handle, name, size);
		if (n == SOCKET_ERROR) {
			err = GetLastError();
			if (err == WSAEALREADY)
				cout << "socket still connecting???" << endl;
			if (err == WSAEISCONN)
				cout << "socket already connected" << endl;
			return RC2(fcWIN32, err);
		}
#endif
	}

	return RCOK;
}

w_rc_t	sfile_t::bind(struct sockaddr *name, int size)
{
	int	n;

#ifdef SFILE_DEBUG
	W_FORM(cout)("sfile(handle=%#x)::bind(", _handle);
	cout << *name << ")" << endl;
#endif
	if (_mode == CLOSED)
		return RC(sfCLOSED);

	n = ::bind(_handle, name, size);
	if (n == SOCKET_ERROR)
		return RC(fcWIN32);

	return RCOK;
}

w_rc_t	sfile_t::getpeername(struct sockaddr *name, int *size)
{
	int	n;

	if (_mode == CLOSED)
		return RC(sfCLOSED);

	n = ::getpeername(_handle, name, size);
	if (n == SOCKET_ERROR)
		return RC(fcWIN32);

#ifdef SFILE_DEBUG
	W_FORM(cout)("sfile(handle=%#x)::getpeername() == ", _handle);
	cout << *name << endl;
#endif

	return RCOK;
}

w_rc_t	sfile_t::getsockname(struct sockaddr *name, int *size)
{
	int	n;

	if (_mode == CLOSED)
		return RC(sfCLOSED);

	n = ::getsockname(_handle, name, size);
	if (n == SOCKET_ERROR)
		return RC(fcWIN32);

#ifdef SFILE_DEBUG
	W_FORM(cout)("sfile(handle=%#x)::getsockname() == ", _handle);
	cout << *name << endl;
#endif

	return RCOK;
}

w_rc_t	sfile_t::listen(int q)
{
	int n;

	if (_mode == CLOSED)
		return RC(sfCLOSED);

#ifdef SFILE_DEBUG
	W_FORM(cout)("sfile(handle=%#x)::listen(%d)\n", _handle, q);
#endif

	n = ::listen(_handle, q);
	if (n == SOCKET_ERROR)
		return RC(fcWIN32);

	return RCOK;
}

w_rc_t	sfile_t::setsockopt(int level, int optname, char *optval, int optlen)
{
	int n;

	if (_mode == CLOSED)
		return RC(sfCLOSED);

	n = ::setsockopt(_handle, level, optname, optval, optlen);
	if (n == SOCKET_ERROR)
		return RC(fcWIN32);
	return RCOK;
}

w_rc_t	sfile_t::getsockopt(int level, int optname, char *optval, int *optlen)
{
	int n;

	if (_mode == CLOSED)
		return RC(sfCLOSED);

	n = ::getsockopt(_handle, level, optname, optval, optlen);
	if (n == SOCKET_ERROR)
		return RC(fcWIN32);
	return RCOK;
}


w_rc_t	sfile_t::recvfrom(void *where, int size, int _flags,
			  struct sockaddr *from, int *fromlen,
			  int &numbytes)
{
	long unsigned	n = 0;
	bool	ok;
	int	err;
	const	int	me = for_read;
	w_rc_t	e;
	WSABUF	vec;
	unsigned long	flags = _flags;

	if (_mode == CLOSED)
		return RC(sfCLOSED);
	if (!(_mode & READING))
		return RC(sfWRONGMODE);

	vec.len = size;
	vec.buf = (char *) where;

	err = WSARecvFrom(_handle, &vec, 1, &n, &flags,
			  from, fromlen, _async+me, 0);
	if (err == SOCKET_ERROR) {
		err = GetLastError();
		if (err == ERROR_NETNAME_DELETED) {
#ifdef SFILE_DEBUG
			W_FORM(cerr)("sfile_t(%#lx, mode=%d, handle %#lx): read netname deleted\n",
				(long) this, _mode, (long)_handle);
#endif
			return RC(sthread_t::stBADFILEHDL);
		}
		else if (err != ERROR_IO_PENDING) {
#ifdef SFILE_DEBUG
			e = RC2(fcWIN32, err);
			W_FORM(cerr)("sfile_t(%#lx, mode=%d): Unexpected WSARecvFrom(%#lx, %#lx, %u) %d read %#x flags error:\n",
				(long)this, _mode,
				(long)_handle, (long)where, size, n, flags);
			cerr << e << endl;
			return e;
#else
			return RC2(fcWIN32, err);
#endif
		}

		e = wait(me);
		if (e && e.err_num() == sthread_t::stBADFILEHDL)
			return e;
		else
			W_COERCE(e);

		ok = WSAGetOverlappedResult(_handle, _async+me, &n, TRUE, &flags);
		if (!ok) {
			err = GetLastError();
			if (err == ERROR_NETNAME_DELETED)
				return RC(sthread_t::stBADFILEHDL);

			return RC2(fcWIN32, err);
		}
	}

	if (n == 0) {
		numbytes = 0;
		e = RC(sfEOF);
	}
	else
		numbytes = n;

	return e;
}


/* XXX duplicated code from send() */
w_rc_t	sfile_t::sendto(const void *where, int size, int _flags,
			const struct sockaddr *to, int tolen,
			int &numbytes)
{
	long unsigned n = 0;
	bool	ok;
	int	err;
	const	int	me = for_write;
	w_rc_t	e;
	WSABUF	vec;
	unsigned long flags = _flags;

	if (_mode == CLOSED)
		return RC(sfCLOSED);
	if (!(_mode & WRITING))
		return RC(sfWRONGMODE);

	vec.len = size;
	vec.buf = (char *) where;

	err = WSASendTo(_handle, &vec, 1, &n, flags, to, tolen, _async+me, 0);
	if (err == SOCKET_ERROR) {
		err = GetLastError();
		if (err == ERROR_NETNAME_DELETED) {
#ifdef SFILE_DEBUG
			W_FORM(cerr)("sfile_t(%#lx, mode=%d, handle %#lx): write netname deleted\n",
				(long) this, _mode, (long)_handle);
			return RC(sthread_t::stBADFILEHDL);
#endif
		}
		else if (err != ERROR_IO_PENDING) {
#ifdef SFILE_DEBUG
			e = RC2(fcWIN32, err);
			W_FORM(cerr)("sfile_t(%#lx, mode=%d): Unexpected WSASendTo(%#lx, %#lx, %u) %d written %#x flag serror:\n",
				(long)this, _mode,
				(long)_handle, (long)where, size, n, flags);
			cerr << e << endl;
			return e;
#else
			return RC2(fcWIN32, err);
#endif
		}

		e = wait(me);
		if (e && e.err_num() == sthread_t::stBADFILEHDL)
			return e;
		else
			W_COERCE(e);

		ok = WSAGetOverlappedResult(_handle, _async+me, &n, TRUE, &flags);
		if (!ok) {
			err = GetLastError();
			if (err == ERROR_NETNAME_DELETED)
				return RC(sthread_t::stBADFILEHDL);

			return RC2(fcWIN32, err);
		}
	}

	if (n == 0)
		e = RC(sfEOF);
	numbytes = n;

	return e;
}


w_rc_t	sfile_t::gethostname(char *buf, int buflen)
{
	int n;

	n = ::gethostname(buf, buflen);
	if (n == -1)
		return RC(fcWIN32);
	buf[buflen-1] = '\0';

	return RCOK;
}


ostream &sfile_t::print(ostream &s) const
{
	W_FORM(s)("sfile_t(handle=%#x, mode=%d)", _handle, _mode);
	return s;
}

ostream &operator<<(ostream &s, const sfile_t &sfile)
{
	return sfile.print(s);
}


#if defined(SFILE_DEBUG) || defined(SFILE_DEBUG_IO)

static ostream &operator<<(ostream &o, const struct sockaddr &addr)
{
	switch (addr.sa_family) {
	case AF_INET: {
		struct sockaddr_in *in = (struct sockaddr_in *) &addr;
		W_FORM(o)("sockaddr_in(addr=%#lx, port=%d)",
		       ntohl(in->sin_addr.s_addr), ntohs(in->sin_port));
		break;
	}
	default:
		W_FORM(o)("sockaddr(sa_family=%d)", addr.sa_family);
		break;
	}
	return o;
}

#endif /*DEBUG*/

/*<std-header orig-src='shore'>

 $Id: sfile.cpp,v 1.22 2001/09/18 22:09:57 bolo Exp $

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

#include <memory.h>
#ifdef _WINDOWS
#include <string.h>
#else
#include <strings.h>
#endif
#include <w.h>
#include <sthread.h>

#include <sfile.h>
#ifdef _WIN32	/* Extreme hack */
#define sfOS	fcWIN32
#endif

#include <unix_error.h>

extern "C" {
#ifdef _WINDOWS
#include <time.h>
#else
#include <sys/time.h>
#endif

#include <os_fcntl.h>

#ifdef _WINDOWS
#ifdef OLD_WINSOCK
#include <winsock.h>
#else
#include <winsock2.h>
#endif
#else
#include <sys/socket.h>
#endif

#ifdef SOLARIS2
#define USE_UTSNAME
#endif
#ifdef USE_UTSNAME
/* Use uname() to determine the hostname.  Gethostname() is the default. */ 
#include <sys/utsname.h>
#endif

#ifdef AIX41
#undef accept
#undef getsockname
#undef getpeername
#undef recvfrom
#endif

#ifdef _WINDOWS
int close(int x) 
{
	return closesocket(x);
}

int read(int x, void *y, int z)
{
	return recv(x, (char*) y, z, 0);
}

int write(int x, const void *y, int z)
{
	return send(x, (char*)y, z, 0);
}
#endif

#ifndef _WINDOWS

#if !defined(SOLARIS2) && !defined(HPUX8) && !defined(AIX41) && !defined(Linux) && !defined(__NetBSD__) && !defined(OSF1)
	extern int socket(int, int, int);
	extern int bind(int, void *, int);
	extern int getsockname(int, struct sockaddr *, int *);
	extern int listen(int, int);
	extern int accept(int, struct sockaddr *, int *);
	extern int connect(int, struct sockaddr *, int);
	extern int getpeername(int, struct sockaddr *, int *);
	extern int setsockopt(int, int, int, char *, int);
	extern int getsockopt(int, int, int, char *, int *);
	extern int recvfrom(int, void *, int, int, struct sockaddr *, int *);
	extern int send(int, void *, int, int);
	extern int gethostname(char *, int);
#endif



#if !defined(AIX32) && !defined(AIX41)
	extern int select(int, fd_set *, fd_set *, fd_set *, struct timeval *);
#endif
#endif

	/* for FD_ZERO */
#define	bzero(a,c)	memset(a, '\0', c);

#ifdef AIX41
	/* HUH???  AIX is braindead */
	inline	int accept(int fd, struct sockaddr *ad , int *sz) {
		return naccept(fd, ad, sz);
	}
	inline	int getpeername(int fd, struct sockaddr *ad, int *sz) {
		return ngetpeername(fd, ad, sz);
	}
	inline	int getsockname(int fd, struct sockaddr *ad, int *sz) {
		return ngetsockname(fd, ad, sz);
	}
	inline	int recvfrom(int fd, void *ptr, int sz, int fl,
			 struct sockaddr *ad, int *asz) {
		return recvfrom(fd, ptr, sz, fl, ad, asz);
	}
#endif /* AIX41 kludge */
}


/* XXX this may be a BSD .vs. POSIX argument; some machines, such as
   Decstations, can return either, depending upon the process mode.
   There is a bug in the ultrix kernels, which in some cases
   it will return the POSIX style error irrespective of the mode
   of the process.  I've fixed it on the UW ultrix boxes, but
   it still exists in the normal ultrix distributions.  If you
   are running on one of those boxes, the ultrix hack below
   should catch it. */

#ifdef Ultrix42
#define	ERR_WOULDBLOCK(err)	((err) == EGAGIN || (err) == EWOULDBLOCK)
#endif
#ifdef _WIN32
/* XXX corrupt the stylized usage a bit, but gets rid of fragments,
   hmmm need to check if errno is set to WSAGetLastError anyway */
#define	ERR_WOULDBLOCK(err)	(WSAGetLastError() == WSAEWOULDBLOCK)
#endif
#ifndef ERR_WOULDBLOCK
#ifdef EWOULDBLOCK
#define	ERR_WOULDBLOCK(err)	((err) == EWOULDBLOCK)
#else
#define	ERR_WOULDBLOCK(err)	((err) == EAGAIN)
#endif
#endif	

/* Some operating systems have broken the traditional mostly "integer" 
   arguments to system calls.  For these, we have a hack.  Once
   all the I/O stuff switches to use size_t, the reverse will
   probably become true. */
#if defined(Linux)	/* XXX AIX may want this too */
#define	SFILE_HACK_SIZE_T(int_ptr)	((unsigned int *)(int_ptr))
#else
#define	SFILE_HACK_SIZE_T(int_ptr)	(int_ptr)
#endif

#if defined(DEBUG_SFILE) || defined(DEBUG_SFILE_IO)
static ostream &operator<<(ostream &o, const struct sockaddr &addr);
#endif

/*
   XXX almost all of the non-blocking ops need to be
   fixed to return EnotOpen (or whaterver) if their is no FD!
 */  
   
	
void sfile_t::setFlag(int the_flag, bool set_them)
{
#ifndef _WINDOWS  // for VC++, we ignore this function, potential BUG!!
	int	r;
	int	flags;
	w_rc_t	e;

	flags = ::fcntl(_fd, F_GETFL, 0);
	if (flags == -1) {
		e = RC(sfOS);
		W_FORM(cerr)("fcntl(%d, F_GETFL)\n", _fd);
		cerr << e << endl;
		return;
	}

	if (set_them)
		flags |= the_flag;
	else
		flags &= ~the_flag;

	r = ::fcntl(_fd, F_SETFL, flags);
	if (r == -1) {
		e = RC(sfOS);
		W_FORM(cerr)("fcntl(%d, F_SETFL, %#x)\n", _fd, the_flag);
		cerr << e << endl;
	}
#endif
}

sfile_t::sfile_t(int fd, int mode)
: read_port(fd, sfile_hdl_base_t::rd),
  write_port(fd, sfile_hdl_base_t::wr)
{
	w_rc_t	e;

	_mode = CLOSED;
	_fd = fd_none;

	if (fd != fd_none) {
		e = _open(fd, convert_mode(mode));
		/*XXX No way to return errors from a constructor. */
		W_COERCE(e);
	}
}

sfile_t::~sfile_t()
{
	if (_fd != fd_none)
		W_FORM(cerr)("~sfile_t(%#lx): warning: open file %d\n",
			  (long)this, _fd);
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
   In a system with real threads, the wait_read()  is best
   done with a select and a timeout.
   This will allow another thread to request that the "waiting"
   thread terminate.  When the select expires, the thread
   will do so.  (aka a mechanism must exist for unblocking from
   the sytem call)
 */  
   
   
w_rc_t	sfile_t::wait_read(long timeout)
{
	w_rc_t	e;

	if (_mode == CLOSED)
		return RC(sfCLOSED);
	if (!(_mode & READING))		/* XXX no reading */
		return RC(sfWRONGMODE);

	/* Fast path wait -- no thread switch if data is ready! */  
	static	int	delay  = -1;

	/* is data available now? */
	if (delay  == -1) {
		/* XXX do at sfile creation to avoid
		   synchronization problems! */
		char	*s = getenv("SFILE_SELECT_READ");
		delay = (s && *s) ? atoi(s) : 0;
	}

	if (delay >= 0) {
		/* XXX error handling from probe? */
		bool	ok = read_port.probe();
		if (ok)
			return RCOK;
	}

	e = read_port.wait(timeout);

	if (e != RCOK) {
		switch (e.err_num()) {
		case sthread_t::stBADFILEHDL:
		case sthread_t::stTIMEOUT:
			break;
		default:
			cerr << "sfile_t: read_port.wait:" << endl
				<< e << endl;
			break;
		}
	}

	return e;
}

w_rc_t	sfile_t::wait_write(long timeout)
{
	w_rc_t	e;
	
	if (_mode == CLOSED)
		return RC(sfCLOSED);	/* really want not open */
	if (!(_mode & WRITING))		/* XXX no writing */
		return RC(sfWRONGMODE);

	/* Fast path wait -- no thwrite switch if data is writey! */  
	static	int	delay  = -1;

	/* is data available now? */
	if (delay  == -1) {
		/* XXX setup at object creation to avoid sync. problems */
		char	*s = getenv("SFILE_SELECT_WRITE");
		delay = (s && *s) ? atoi(s) : 0;
	}
	if (delay >= 0) {
		/* XXX error handling from probe? */
		bool	ok = write_port.probe();
		if (ok)
			return RCOK;
	}

	e = write_port.wait(timeout);

	if (e != RCOK) {
		switch (e.err_num()) {
		case sthread_t::stBADFILEHDL:
		case sthread_t::stTIMEOUT:
			break;
		default:
			cerr << "sfile_t: write_port.wait:" << endl
				<< e << endl;
			break;
		}
	}

	return e;
}


w_rc_t	sfile_t::_open(int fd, int mode)
{
	w_rc_t	e;

	if (_mode != CLOSED)
		return RC(sfOPEN);	/* really want already open! */

	if (mode & READING) {
		W_COERCE(read_port.change(fd));
		e = sthread_t::io_start(read_port);
		if (e != RCOK)
			return e;
	}

	if (mode & WRITING) {
		// W_FORM(cout)("sfile_t::_open(fd=%d) write port\n", fd);
		W_COERCE(write_port.change(fd));
		e = sthread_t::io_start(write_port);
		if (e != RCOK && (mode & READING)) {
			W_COERCE(sthread_t::io_finish(read_port));
			return e;
		}
	}

	_mode = mode;
	_fd = fd;

#if 0
	int	n;
	char	*s;
	s = getenv("SFILE_NDELAY");
	n = (s && *s) ? atoi(s) : 1;
	if (n)
		setFlag(O_NDELAY);
#else
#ifdef _WINDOWS
	unsigned long param = 1;
	if (ioctlsocket(_fd, FIONBIO, &param) == -1) {
		e = RC(fcOS);
		W_FORM(cerr)("fcntl(%d, F_GETFL)\n", _fd);
		cerr << e << endl;
		return e;
	} 
#else
	setFlag(O_NDELAY);	// XXX error reporting
#endif
#endif

	return RCOK;
}

w_rc_t	sfile_t::cancel()
{
	if (_mode == CLOSED)
		return RC(sfCLOSED);	/* really want not open */

#ifdef DEBUG_SFILE
	W_FORM(cout)("sfile_t::cancel() fd=%d\n", _fd);
#endif
	if (_mode & READING)
		read_port.shutdown();
	if (_mode & WRITING)
		write_port.shutdown();

	return RCOK;
}

w_rc_t	sfile_t::fdopen(int fd, int mode)
{
	return _open(fd, convert_mode(mode));
}

w_rc_t	sfile_t::socket(int domain, int type, int protocol)
{
	int	n;
	w_rc_t	e;

	if (_mode != CLOSED)
		return RC(sfOPEN);	/* really want already open! */

	n = ::socket(domain, type, protocol);
	if (n == -1)
		return RC(sfOS);

	e = _open(n, READ_WRITE);
	if (e != RCOK)
		::close(n);

	return e;
}

w_rc_t	sfile_t::close()
{
	if (_mode == CLOSED)  
		return RC(sfCLOSED);	/* really want not open */

#ifdef DEBUG_SFILE
	W_FORM(cout)("sfile_t::close() fd=%d\n", _fd);
#endif

	if (_mode & READING) {
		read_port.shutdown();
		W_COERCE(sthread_t::io_finish(read_port));
	}
	if (_mode & WRITING) {
		write_port.shutdown();
		W_COERCE(sthread_t::io_finish(write_port));
	}

	::close(_fd);
	_fd = fd_none;
	_mode = CLOSED;

	return RCOK;
}


w_rc_t	sfile_t::shutdown(int how)
{
	if (_mode == CLOSED)
		return RC(sfCLOSED);

#ifdef DEBUG_SFILE
	W_FORM(cout)("sfile_t::shutdown() fd=%d how=%d\n", _fd, how);
#endif

	int	n;

	n = ::shutdown(_fd, how);
	if (n == -1)
		return RC(fcOS);
	return RCOK;
}


/* Read with partial read semantics */
w_rc_t	sfile_t::read(void *where, int size, int &numbytes)
{
	w_rc_t	e;
	int	n;

#ifdef DEBUG_SFILE_IO
	W_FORM(cout)("sfile(fd=%d)::read(%#lx, %d)\n", _fd, (long)where, size);
#endif
#if 1
	for (;;) {
		e = wait_read();
		if (e != RCOK) {
			numbytes = -1;
			return e;
		}

#ifdef DEBUG_SFILE_IO
		W_FORM(cout)("sfile(fd=%d)::read(%#lx, %d) GOING\n",
			     _fd, (long)where, size);
#endif
		n = ::read(_fd, where, size);
		if (n == -1  &&  ERR_WOULDBLOCK(errno))
			continue;

		break;
	}

#else
	e = wait_read();
	if (e != RCOK) {
		numbytes = -1;
		return e;
	}

	n = ::read(_fd, where, size);
#endif
	if (n == -1) {
		numbytes = -1;
		e = RC(sfOS);
	}
	else if (n == 0) {
		e = RC(sfEOF);
		numbytes = 0;
	}
	else
		numbytes = n;

	return e;
}


/* XXX act like a reliable or partial writer ??? */
w_rc_t	sfile_t::write(const void *where, int size, int &numbytes)
{
	int	n;
	w_rc_t	e;

#ifdef DEBUG_SFILE_IO
	W_FORM(cout)("sfile(fd=%d)::write(%#lx, %d)\n", _fd, (long)where, size);
#endif
#if 1
	for (;;) {
		e = wait_write();
		if (e != RCOK) {
			numbytes = -1;
			return e;
		}

#ifdef DEBUG_SFILE_IO
		W_FORM(cout)("sfile(fd=%d)::write(%#lx, %d) GOING\n",
			       _fd, (long)where, size);
#endif
		n = ::write(_fd, where, size);
		if (n == -1  &&  ERR_WOULDBLOCK(errno))
			continue;

		break;
	}
#else
#if 0
	e = wait_write();
	if (e != RCOK) {
		numbytes = -1;
		return e;
	}
#endif

	n = ::write(_fd, where, size);
#endif

	if (n == -1 && errno != EPIPE) {
		e = RC(sfOS);
		numbytes = -1;
	}
	else if (n == 0 || n == -1) {
		/* treat EPIPE as eof; close enough */
		e = RC(sfEOF);
		numbytes = 0;
	}
	else {
		e = RCOK;
		numbytes = n;
	}

	return e;
}

w_rc_t	sfile_t::accept(struct sockaddr *a, int *l, sfile_t &acceptor)
{
	w_rc_t	e;
	int	n;

#ifdef DEBUG_SFILE
	W_FORM(cout)("sfile(fd=%d)::accept()\n", _fd);
#endif	

	if (_mode == CLOSED)
		return RC(sfCLOSED);	/* really want not open */

	if (acceptor.isOpen())
		return RC(sfOPEN);	/* XXX poor choice */

	e = wait_read();
	if (e != RCOK)
		return e;

	n = ::accept(_fd, a, SFILE_HACK_SIZE_T(l));
	if (n == -1)
		e = RC(sfOS);
	else {
		e = acceptor.fdopen(n, OPEN_RDWR);
		if (e != RCOK)
			::close(n);

	}
#ifdef DEBUG_SFILE
	W_FORM(cout)("sfile(fd=%d)::accept() addr=", _fd);
	cout << *a << endl;
#endif

	return e;
}


w_rc_t	sfile_t::connect(struct sockaddr *name, int size)
{
	int	n;
	w_rc_t	e;

#ifdef DEBUG_SFILE
	W_FORM(cout)("sfile(fd=%d)::connect(", _fd);
	cout << *name << ")" << endl;
#endif	
	if (_mode == CLOSED)
		return RC(sfCLOSED);	/* really want not open */

	n = ::connect(_fd, name, size);
#ifdef _WINDOWS
	if (n == -1  &&  (WSAGetLastError() == WSAEINPROGRESS))
#else
	if (n == -1 && errno != EINPROGRESS)
#endif
		return RC(sfOS);
	else if (n == 0)
		return RCOK;

#ifdef DEBUG_SFILE
	W_FORM(cout)("sfile(fd=%d)::connect() ... waiting for completion\n", _fd);
#endif	
#ifdef _WINDOWS
	DWORD lastErr = 0;
#endif
	for (;;) {
		e = wait_write();
		if (e != RCOK)
			return e;

		n = ::connect(_fd, name, size);
#ifdef _WINDOWS
		if (n == -1 && ((lastErr = WSAGetLastError()) == WSAEALREADY || (lastErr == WSAEINVAL)))
#else
		if (n == -1 && errno == EALREADY)
#endif
			continue;

		break;
	}
#ifdef _WINDOWS
	if (n == -1  &&  (lastErr != WSAEISCONN))
#else
	if (n == -1  &&  errno != EISCONN)
#endif
		return RC(sfOS);

	return RCOK;
}

w_rc_t	sfile_t::bind(struct sockaddr *name, int size)
{
	int	n;

#ifdef DEBUG_SFILE
	W_FORM(cout)("sfile(fd=%d)::bind(", _fd);
	cout << *name << ")" << endl;
#endif	
	n = ::bind(_fd, name, size);
	if (n == -1)
		return RC(sfOS);

	return RCOK;
}

w_rc_t	sfile_t::getpeername(struct sockaddr *name, int *size)
{
	int	n;
	
	n = ::getpeername(_fd, name, SFILE_HACK_SIZE_T(size));
	if (n == -1)
		return RC(sfOS);

#ifdef DEBUG_SFILE
	W_FORM(cout)("sfile(fd=%d)::getpeername() == ", _fd);
	cout << *name << endl;
#endif	

	return RCOK;
}

w_rc_t	sfile_t::getsockname(struct sockaddr *name, int *size)
{
	int	n;
	
	n = ::getsockname(_fd, name, SFILE_HACK_SIZE_T(size));
	if (n == -1)
		return RC(sfOS);

#ifdef DEBUG_SFILE
	W_FORM(cout)("sfile(fd=%d)::getsockname() == ", _fd);
	cout << *name << endl;
#endif	

	return RCOK;
}

w_rc_t	sfile_t::listen(int q)
{
	int n;

#ifdef DEBUG_SFILE
	W_FORM(cout)("sfile(fd=%d)::listen(%d)\n", _fd, q);
#endif

	n = ::listen(_fd, q);
	if (n == -1)
		return RC(sfOS);
	return RCOK;
}

w_rc_t	sfile_t::setsockopt(int level, int optname, char *optval, int optlen)
{
	int n;

	n = ::setsockopt(_fd, level, optname, optval, optlen);
	if (n == -1)
		return RC(sfOS);
	return RCOK;
}

w_rc_t	sfile_t::getsockopt(int level, int optname, char *optval, int *optlen)
{
	int n;

	n = ::getsockopt(_fd, level, optname, optval,
		SFILE_HACK_SIZE_T(optlen));
	if (n == -1)
		return RC(sfOS);
	return RCOK;
}

w_rc_t	sfile_t::recvfrom(void *where, int size, int flags,
			  struct sockaddr *from, int *fromlen,
			  int &numbytes)
{
	w_rc_t	e;
	int	n;

	for (;;) {
		e = wait_read();
		if (e != RCOK) {
			numbytes = -1;
			return e;
		}

		n = ::recvfrom(_fd, (char*)where, size, flags, from,
			SFILE_HACK_SIZE_T(fromlen));
		if (n == -1  &&  ERR_WOULDBLOCK(errno))
			continue;

		break;
	}

	if (n == -1) {
		numbytes = -1;
		e = RC(sfOS);
	}
	else if (n == 0) {
		numbytes = 0;
		e = RC(sfEOF);
	}
	else {
		numbytes = n;
		e = RCOK;
	}

	return e;
}

w_rc_t	sfile_t::recv(void *where, int size, int flags, int &numbytes)
{
	w_rc_t	e;
	int	n;

	for (;;) {
		e = wait_read();
		if (e != RCOK) {
			numbytes = -1;
			return e;
		}

		n = ::recv(_fd, (char*)where, size, flags);
		if (n == -1  &&  ERR_WOULDBLOCK(errno))
			continue;

		break;
	}

	if (n == -1) {
		numbytes = -1;
		e = RC(sfOS);
	}
	else if (n == 0) {
		numbytes = 0;
		e = RC(sfEOF);
	}
	else {
		numbytes = n;
		e = RCOK;
	}

	return e;
}

w_rc_t	sfile_t::sendto(const void *where, int size, int flags,
			const struct sockaddr *to, int  tolen,
			int &numbytes)
{
	int	n;
	w_rc_t	e;

	for (;;) {
		e = wait_write();
		if (e != RCOK) {
			numbytes = -1;
			return e;
		}

		n = ::sendto(_fd, (char *)where, size, flags, to, tolen);
		if (n == -1  &&  ERR_WOULDBLOCK(errno))
			continue;
		break;
	}

	if (n == -1 && errno != EPIPE) {
		numbytes = -1;
		e = RC(sfOS);
	}
	else if (n == 0 || n == -1) {
		/* treat EPIPE as eof; close enough */
		e = RC(sfEOF);
		numbytes = 0;
	}
	else {
		numbytes = n;
		e = RCOK;
	}

	return e;
}


w_rc_t	sfile_t::send(const void *where, int size, int flags, int &numbytes)
{
	int	n;
	w_rc_t	e;

	for (;;) {
		e = wait_write();
		if (e != RCOK) {
			numbytes = -1;
			return e;
		}

		n = ::send(_fd, (char *)where, size, flags);
		if (n == -1  &&  ERR_WOULDBLOCK(errno))
			continue;
		break;
	}

	if (n == -1 && errno != EPIPE) {
		numbytes = -1;
		e = RC(sfOS);
	}
	else if (n == 0 || n == -1) {
		/* treat EPIPE as eof; close enough */
		e = RC(sfEOF);
		numbytes = 0;
	}
	else {
		numbytes = n;
		e = RCOK;
	}

	return e;
}

w_rc_t	sfile_t::gethostname(char *buf, int buflen)
{
	int	n;

#ifdef USE_UTSNAME
	struct utsname uts;
	n = uname(&uts);
	if (n == -1)
		return RC(fcOS);
	strncpy(buf, uts.nodename, buflen);
#else
	n = ::gethostname(buf, buflen);
	if (n == -1)
		return RC(fcOS);
#endif
	buf[buflen-1] = '\0';
	
	return RCOK;
}


ostream &sfile_t::print(ostream &s) const
{
	W_FORM(s)("sfile_t(fd=%d, mode=%d)", _fd, _mode);
	return s;
}

ostream &operator<<(ostream &s, const sfile_t &sfile)
{
	return sfile.print(s);
}


#if defined(DEBUG_SFILE) || defined(DEBUG_SFILE_IO)
#include <sys/un.h>
#include <netinet/in.h>

static ostream &operator<<(ostream &o, const struct sockaddr &addr)
{
	switch (addr.sa_family) {
	case AF_INET: {
		struct sockaddr_in *in = (struct sockaddr_in *) &addr;
		W_FORM(o)("sockaddr_in(addr=%#lx  port=%d)",
			  ntohl(in->sin_addr.s_addr), ntohs(in->sin_port));
		break;
	}
	case AF_UNIX: {
		struct sockaddr_un *un = (struct sockaddr_un *) &addr;
		W_FORM(o)("sockaddr_un(%s)", un->sun_path);
		break;
	}
	default:
		W_FORM(o)("sockaddr(sa_family=%d)", addr.sa_family);
		break;
	}
	return o;
}

	
#endif /*DEBUG*/


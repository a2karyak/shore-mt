/*<std-header orig-src='shore' incl-file-exclusion='SFILE_H'>

 $Id: sfile.h,v 1.18 2007/05/18 21:53:43 nhall Exp $

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

#ifndef SFILE_H
#define SFILE_H

#include "w_defines.h"

/*  -- do not edit anything above this line --   </std-header>*/

#if defined(_WIN32) && defined(NEW_IO)
#include <sfile_win32.h>
#else

#include <sfile_handler.h>

#if defined(AIX41) && !defined(__xlC__)
#undef getsockname
#undef accept
#undef getpeername
#undef recvfrom
#endif

extern "C" {
#if defined(_WIN32)
#include <w_winsock.h>
#else
#ifdef HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif
#ifdef HAVE_NETINET_IN_H
#include <netinet/in.h>
#else 
#error Error expecting <netinet/in.h>
#endif
#endif
}

#if defined(AIX41)
#undef getsockname
#undef accept
#undef getpeername
#undef recvfrom
#endif

#ifndef IPPORT_ANY
#define	IPPORT_ANY	0
#endif

class sfile_t {
private:
	enum { periodic_wakeup = 2000 };	// milli-seconds
	enum { fd_none = -1 };
	/* inner mode bits; bitwise mode select */
	enum { CLOSED=0, READING=1, WRITING=2, READ_WRITE=3 };
public:
	/* user-visible posix-like I/O mode */
	enum { OPEN_RDONLY=0, OPEN_WRONLY=1, OPEN_RDWR=2 };

private:
	int	_fd;
	int	_mode;

	sfile_safe_hdl_t	read_port;
	sfile_safe_hdl_t	write_port;

	void	setFlag(int flag, bool set_them = true);

	w_rc_t	wait_read(long timeout = sthread_base_t::WAIT_FOREVER);
	w_rc_t	wait_write(long timeout = sthread_base_t::WAIT_FOREVER);

	w_rc_t	_open(int fd, int mode);

	int	convert_mode(int posix_mode);

public:
	sfile_t(int fd = fd_none, int mode = OPEN_RDONLY);
	~sfile_t();

	w_rc_t	fdopen(int, int mode = OPEN_RDONLY);
	w_rc_t	close();

	w_rc_t	read(void *where, int size, int &numbytes);
	w_rc_t	write(const void *where, int size, int &numbytes);

#ifdef notyet
	/* not certain about reliable I/O semantics at the moment */
	w_rc_t	read(void *where, int size);
	w_rc_t	write(const void *where, int size);
#endif

	w_rc_t	socket(int, int, int);
	w_rc_t	accept(struct sockaddr *, int *, sfile_t &);
	w_rc_t	connect(struct sockaddr *, int);
	w_rc_t	bind(struct sockaddr *, int);
	w_rc_t	getpeername(struct sockaddr *, int *);
	w_rc_t	getsockname(struct sockaddr *, int *);
	w_rc_t	listen(int);
	w_rc_t	setsockopt(int, int, char *, int);
	w_rc_t	getsockopt(int, int, char *, int *);
	w_rc_t	shutdown(int);

	w_rc_t	recvfrom(void *, int, int, struct sockaddr *, int *, int &);
	w_rc_t	sendto(const void *, int, int, const struct sockaddr *, int, int &);

	w_rc_t	send(const void *, int, int, int &);
	w_rc_t  recv(void *, int, int, int &);

	int	fd() const { return _fd; };
	bool	isOpen() const { return  _mode != CLOSED; }
	w_rc_t	cancel();

	static	w_rc_t	gethostname(char *, int);

	ostream	&print(ostream &) const;

	/* steal some error codes without stealing using an
	   entire error system.  This will be integrated
	   with sthread eventually. */
	enum {
		sfEOF = sthread_base_t::stEOF,
		sfOS = sthread_base_t::stOS,
		sfCLOSED = fcEMPTY,
		sfOPEN = fcFULL,
		sfWRONGMODE = fcMIXED
	};

private:	/* disabled for now */
	sfile_t(const sfile_t &);
	sfile_t	operator=(const sfile_t &);
};


extern ostream &operator<<(ostream &, const sfile_t &);

#endif	/*NEW_IO*/

/*<std-footer incl-file-exclusion='SFILE_H'>  -- do not edit anything below this line -- */

#endif          /*</std-footer>*/

/*<std-header orig-src='shore' incl-file-exclusion='SFILE_WIN32_H'>

 $Id: sfile_win32.h,v 1.6 1999/06/07 19:06:07 kupsch Exp $

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

#ifndef SFILE_WIN32_H
#define SFILE_WIN32_H

#include "w_defines.h"

/*  -- do not edit anything above this line --   </std-header>*/

/*
 *   NewThreads is Copyright 1992, 1993, 1994, 1995, 1996, 1997, 1998 by:
 *
 *	Josef Burger	<bolo@cs.wisc.edu>
 *      Dylan McNamee   <dylan@cse.ogi.edu>
 *      Ed Felten       <felten@cs.princeton.edu>
 *
 *   All Rights Reserved.
 *
 *   NewThreads may be freely used as long as credit is given
 *   to the above authors and the above copyright is maintained.
 */
#include <windows.h>
#include <win32_events.h>

#ifndef IPPORT_ANY
#define	IPPORT_ANY	0
#endif

class sfile_t {
private:
	/* inner mode bits; bitwise mode select */
	enum { CLOSED=0, READING=1, WRITING=2, READ_WRITE=3 };
public:
	/* user-visible posix-like I/O mode */
	enum { OPEN_RDONLY=0, OPEN_WRONLY=1, OPEN_RDWR=2 };

private:
	SOCKET	_handle;
	int	_mode;		/* read/write mode */

	enum {
		for_read = 0,
		for_write = 1,
		for_accept = 2,
		for_connect = 3
	};

	// [0] == read, [1] == write, [2] == socket
	OVERLAPPED		_async[3];
	win32_safe_event_t	_event[3];

	w_rc_t	setFlag(int flag, bool set_them = true);

	w_rc_t	wait(int which, long timeout = sthread_base_t::WAIT_FOREVER);

	w_rc_t	_open(SOCKET handle, int mode);

	int	convert_mode(int posix_mode);

public:
	sfile_t(SOCKET handle = INVALID_SOCKET, int mode = OPEN_RDONLY);
	~sfile_t();

	w_rc_t	fdopen(SOCKET h, int mode = OPEN_RDONLY);
	w_rc_t	close();

	w_rc_t	read(void *where, int size, int &numbytes) {
		return recv(where, size, 0, numbytes);
	}
	w_rc_t	write(const void *where, int size, int &numbytes) {
		return send(where, size, 0, numbytes);
	}

	w_rc_t	socket(int, int, int);
	w_rc_t	accept(struct sockaddr *, int *, sfile_t &);
	w_rc_t	connect(struct sockaddr *, int);
	w_rc_t	bind(struct sockaddr *, int);
	w_rc_t	getpeername(struct sockaddr *, int *);
	w_rc_t	getsockname(struct sockaddr *, int *);
	w_rc_t	listen(int);
	w_rc_t	setsockopt(int, int, char *, int);
	w_rc_t	getsockopt(int, int, char *, int *);

	w_rc_t	recvfrom(void *, int, int, struct sockaddr *, int *, int &);
	w_rc_t	sendto(const void *, int, int, const struct sockaddr *, int , int &);

	w_rc_t	recv(void *, int, int, int &);
	w_rc_t	send(const void *, int, int, int &);

	bool	isOpen() const { return  _mode != CLOSED; }
	w_rc_t	cancel();

	int	fd() const { return _handle; }

	static	w_rc_t	gethostname(char *, int);

	ostream	&print(ostream &) const;
		
	/* Overload some existing error codes until we get our own */
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

/*<std-footer incl-file-exclusion='SFILE_WIN32_H'>  -- do not edit anything below this line -- */

#endif          /*</std-footer>*/

/*<std-header orig-src='shore' incl-file-exclusion='SDISK_UNIX_H'>

 $Id: sdisk_unix.h,v 1.8 1999/06/07 19:06:04 kupsch Exp $

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

#ifndef SDISK_UNIX_H
#define SDISK_UNIX_H

#include "w_defines.h"

/*  -- do not edit anything above this line --   </std-header>*/

/*
 *   NewThreads I/O is Copyright 1995, 1996, 1997, 1998 by:
 *
 *	Josef Burger	<bolo@cs.wisc.edu>
 *
 *   All Rights Reserved.
 *
 *   NewThreads I/O may be freely used as long as credit is given
 *   to the above author(s) and the above copyright is maintained.
 */

class sdisk_unix_t : public sdisk_t {
	int	_fd;

	enum	{ FD_NONE = -1 };

#ifdef SDISK_DISKRW_HACK
protected:
#endif

	sdisk_unix_t() : _fd(FD_NONE) { }

public:
	static	w_rc_t	make(const char *name,
			     int flags, int mode,
			     sdisk_t *&disk);
	~sdisk_unix_t();

	w_rc_t	open(const char *name, int flags, int mode);
	w_rc_t	close();

	w_rc_t	read(void *buf, int count, int &done);
	w_rc_t	write(const void *buf, int count, int &done);

#ifndef _WIN32
	/* Win32 lacks the *v() variants.  Use the generic 
	   implementation in sdisk_t */
	w_rc_t	readv(const iovec_t *iov, int iovcnt, int &done);
	w_rc_t	writev(const iovec_t *iov, int iovcnt, int &done);
#endif

#ifdef SOLARIS2
	w_rc_t	pread(void *buf, int count, fileoff_t pos, int &done);
	w_rc_t	pwrite(const void *buf, int count, fileoff_t pos, int &done);
#endif

	w_rc_t	seek(fileoff_t pos, int origin, fileoff_t &newpos);

	w_rc_t	truncate(fileoff_t size);

	w_rc_t	sync();

	w_rc_t	stat(filestat_t &st);

#if defined(SOLARIS2) || defined(SUNOS41)
	w_rc_t	getGeometry(disk_geometry_t &dg);
#endif

	/* convert sdisk mode+flags to unix mode+flags */
	static	int	convert_flags(int);
};

/*<std-footer incl-file-exclusion='SDISK_UNIX_H'>  -- do not edit anything below this line -- */

#endif          /*</std-footer>*/

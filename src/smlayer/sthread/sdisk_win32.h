/*<std-header orig-src='shore' incl-file-exclusion='SDISK_WIN32_H'>

 $Id: sdisk_win32.h,v 1.8 1999/06/07 19:06:04 kupsch Exp $

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

#ifndef SDISK_WIN32_H
#define SDISK_WIN32_H

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
class sdisk_win32_t : public sdisk_t {
protected:
	HANDLE	_handle;
	bool	_isDevice;
	filestat_t	_default_stat;

	sdisk_win32_t() : _handle(INVALID_HANDLE_VALUE), _isDevice(false) { }

protected:

	/* Convert sdisk flags to appropriate win32 flags */
	static	DWORD	convert_access(int flags);
	static	DWORD	convert_share(int flags);
	static	DWORD	convert_create(int flags);
	static	DWORD	convert_attributes(int flags);

	w_rc_t	_open(const char *name,
		      DWORD acc, DWORD shar,
		      DWORD create, DWORD attr);


	/* XXX this stuff is hacky, will be fixed soon. */
	static	inline	void	getOffset(LONG *lw, const fileoff_t pos) {
#ifdef LARGEFILE_AWARE
		const LONG *lwp = (const LONG *) &pos;
		lw[0] = lwp[0];
		lw[1] = lwp[1];
#else
		lw[0] = pos;
		lw[1] = 0;
#endif
	}
	static	inline	void	getOffset(OVERLAPPED &ov, fileoff_t pos) {
#ifdef LARGEFILE_AWARE
		const	LONG	*lwp = (const LONG *) &pos;
		ov.Offset     = lwp[0];
		ov.OffsetHigh = lwp[1];
#else
		ov.Offset = pos;
		ov.OffsetHigh = 0;
#endif
	}
	static	inline	void	setOffset(fileoff_t &pos, const LONG *lw) {
#ifdef LARGEFILE_AWARE
		LONG	*lwp = (LONG *) &pos;
		lwp[0] = lw[0];
		lwp[1] = lw[1];
#else
		pos = lw[0];
#endif
	}
	static	inline	void	setOffset(fileoff_t &pos, LONG h, LONG l) {
#ifdef LARGEFILE_AWARE
		LONG	*lwp = (LONG *) &pos;
		lwp[0] = l;
		lwp[1] = h;
#else
		pos = l;
#endif
	}
	static	inline	void	setOffset(fileoff_t &pos, const LARGE_INTEGER &li) {
#ifdef LARGEFILE_AWARE
		LONG	*lwp = (LONG *) &pos;
		lwp[0] = li.LowPart;
		lwp[1] = li.HighPart;
#else
		pos = li.LowPart;
#endif
	}

public:
	static	w_rc_t	make(const char *name,
			     int flags, int mode,
			     sdisk_t *&disk);
	~sdisk_win32_t();

	w_rc_t	open(const char *name, int flags, int mode);
	w_rc_t	close();

	w_rc_t	read(void *buf, int count, int &done);
	w_rc_t	write(const void *buf, int count, int &done);

	w_rc_t	pread(void *buf, int count, fileoff_t pos, int &done);
	w_rc_t	pwrite(const void *buf, int count, fileoff_t pos, int &done);

	w_rc_t	seek(fileoff_t pos, int origin, fileoff_t &newpos);
	w_rc_t	truncate(fileoff_t size);

	w_rc_t	sync();

	w_rc_t	stat(filestat_t &st);

	w_rc_t	getGeometry(disk_geometry_t &dg);
};

/*<std-footer incl-file-exclusion='SDISK_WIN32_H'>  -- do not edit anything below this line -- */

#endif          /*</std-footer>*/

/*<std-header orig-src='shore' incl-file-exclusion='SDISK_WIN32_ASYNC_H'>

 $Id: sdisk_win32_async.h,v 1.11 2000/06/01 21:32:39 bolo Exp $

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

#ifndef SDISK_WIN32_ASYNC_H
#define SDISK_WIN32_ASYNC_H

#include "w_defines.h"

/*  -- do not edit anything above this line --   </std-header>*/

/*
 *   NewThreads I/O is Copyright 1995, 1996, 1997, 1998, 1999, 2000 by:
 *
 *	Josef Burger	<bolo@cs.wisc.edu>
 *
 *   All Rights Reserved.
 *
 *   NewThreads I/O may be freely used as long as credit is given
 *   to the above author(s) and the above copyright is maintained.
 */



class sdisk_win32_async_t : public sdisk_win32_t {

	/* XXX eliminating the compiled-in limits is on the to-do list */
#ifdef SDISK_WIN32_ASYNC_MAX
	enum { MAXasync = SDISK_WIN32_ASYNC_MAX };
#else
	enum { MAXasync = 8 };
#endif
	unsigned		_num_inuse;
	unsigned		_num_concurrent[MAXasync+1];
	unsigned		_max_concurrent;
	unsigned		_num_waited;
	unsigned		_num_waiting;

	const	unsigned	maxAsync;

	scond_t			available;

	/* XXX could chain entries so they don't need to be searched */
	bool			_inuse[MAXasync];

	/* XXX these may be able to be moved to a struct.  However, there
	   was some reason they were NOT placed into one.  Until I can
	   remember why, this will stay as parallel arrays. */
	OVERLAPPED		_async[MAXasync];
	win32_safe_event_t	_event[MAXasync];
	smutex_t		lock;

#ifdef SDISK_WIN32_ASYNC_ASYNC
	struct SyncThread {
		HANDLE		handle;		/* to sync */
		HANDLE		thread;
#ifdef _MT
		unsigned	threadId;
		static unsigned	__stdcall	_start(void *);
#else
		DWORD		threadId;
		static DWORD	__stdcall	_start(void *);
#endif
		HANDLE		request;	/* a sync */
		HANDLE		_done;		/* sync complete */
		win32_safe_event_t	done;
		bool		abort;		/* done */
		DWORD		_err;		/* error from "last" op XXX */

		DWORD		run();
		w_rc_t		start(HANDLE h);
		w_rc_t		stop();
		w_rc_t		sync();

		SyncThread();
	};
	SyncThread	syncThread;
	/* XXX this protection is needed in case two threads try
	   sync()ing the sdisk at the same time.  It is extra overhead,
	   but better safe than sorry.  If it is guaranteed that this
	   won't happen (at the application level) the additional sync.
	   stuff could be made conditional at compile and/or runtime. */
	scond_t		syncAvailable;
	bool		_use_async;
	bool		sync_in_progress;
	unsigned	sync_waiters;
	unsigned	_sync_waited;	/* stat */
#endif

	/* XXX may be able to use offset in overlapped to store seek pos */
	fileoff_t	_pos;	// current seek position 

	w_rc_t		wait(unsigned slot,
			     long timeout = sthread_t::WAIT_FOREVER);

	w_rc_t		_read(unsigned slot, void *buf, int count,
			      DWORD &done, bool &fast);
	w_rc_t		_write(unsigned slot, const void *buf, int count,
			       DWORD &done, bool &fast);

	inline	w_rc_t		allocate_slot(unsigned &slot);
	inline	w_rc_t		release_slot(unsigned slot);

	unsigned	io_fast[3];
	unsigned	io_total[3];
	double		io_time[3];

	sdisk_win32_async_t();
	sdisk_win32_async_t(const char *name, int flags, int mode);

public:
	static	w_rc_t	make(const char *name,
			     int flags, int mode,
			     sdisk_t *&disk);
	~sdisk_win32_async_t();

	w_rc_t	open(const char *name, int flags, int mode);
	w_rc_t	close();

	w_rc_t	read(void *buf, int count, int &done);
	w_rc_t	write(const void *buf, int count, int &done);

	w_rc_t	readv(const iovec_t *iov, int iovcnt, int &done);
	w_rc_t	writev(const iovec_t *iov, int iovcnt, int &done);

	w_rc_t	pread(void *buf, int count, fileoff_t pos, int &done);
	w_rc_t	pwrite(const void *buf, int count, fileoff_t pos, int &done);

	w_rc_t	seek(fileoff_t pos, int origin, fileoff_t &newpos);

	w_rc_t	truncate(fileoff_t size);

	w_rc_t	sync();
};

/*<std-footer incl-file-exclusion='SDISK_WIN32_ASYNC_H'>  -- do not edit anything below this line -- */

#endif          /*</std-footer>*/

/*<std-header orig-src='shore'>

 $Id: sdisk_win32_async.cpp,v 1.11 2000/06/01 21:32:39 bolo Exp $

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
 *   NewThreads I/O is Copyright 1995, 1996, 1997, 1998, 1999, 2000 by:
 *
 *	Josef Burger	<bolo@cs.wisc.edu>
 *
 *   All Rights Reserved.
 *
 *   NewThreads I/O may be freely used as long as credit is given
 *   to the above author(s) and the above copyright is maintained.
 */

#ifdef __GNUC__
#pragma implementation "sdisk_win32_async.h"
#endif

#include <w.h>
#include <sdisk.h>
#include <sthread.h>

#include <w_statistics.h>
#include <sthread_stats.h>
extern class sthread_stats SthreadStats;

#include <windows.h>
#include <sdisk_win32.h>
#include <win32_events.h>
#include <sdisk_win32_async.h>


const	int	stBADFD = sthread_base_t::stBADFD;
const	int	stINVAL = sthread_base_t::stINVAL;
const	int	stINTERNAL = sthread_base_t::stINTERNAL;

#define	BROKEN_IO_HACK
#ifdef BROKEN_IO_HACK
/*
 * The way that the Storage manager does multiple I/O to the
 * same file descriptor is really broken.  It only works with
 * diskrw, and one particular diskrw thread switch example.
 * It breaks horribly if real threads are used, or if anything
 * (such as the mutex in this class) is used to change
 * the order of I/O and context switches.
 *
 * BROKEN_IO_HACK will grab the file position from the class
 * *BEFORE* locking the class for multiple I/Os.  This means
 * that the seek pointer that the I/O starts with is the
 * same as the seek offset that occured immediateley before
 * it.  This will not work with real threads.
 *
 * In other words, this hack makes this "simulate" diskrw I/O
 * until the underlying problem can be fixed.
 */
#endif

/*
 * Stuff in here is in a real mess at the moment.  There
 * are so many dependencies in the SM about "how" I/O happens
 * that only something that completely mimics the synchronization
 * semantics of diskrw will work correctly.  This is really
 * screwing up the async I/O system.  And slowing it down.
 */

// #define	SDISK_VERBOSE


static unsigned setMaxAsync(unsigned numAsync, unsigned maxAsync)
{
	const	char *s = getenv("SDISK_WIN32_ASYNC_MAX");
	if (s && *s) {
		int	n = atoi(s);
		if (n <= 0)
			n = 1;
		numAsync = n;
	}
	if (numAsync > maxAsync)
		numAsync = maxAsync;
	
	return numAsync;
}


sdisk_win32_async_t::sdisk_win32_async_t()
: _pos(0),
  _num_inuse(0),
  _num_waited(0),
  _num_waiting(0),
  _max_concurrent(0),
  maxAsync(setMaxAsync(1, MAXasync))
#ifdef SDISK_WIN32_ASYNC_ASYNC
  ,
  _use_async(true),
  sync_waiters(0),
  sync_in_progress(false),
  _sync_waited(0)
#endif
{
	unsigned	i;

	for (i = 0; i < maxAsync; i++) {
		_inuse[i] = false;
		_num_concurrent[i] = 0;
		_async[i].hEvent = INVALID_HANDLE_VALUE;
	}

	for (i = 0; i < 3; i++) {
		io_total[i] = 0;
		io_fast[i] = 0;
		io_time[i] = 0.0;
	}

	lock.rename("m:", "sdisk");
	available.rename("c:", "sdisk");

#ifdef SDISK_WIN32_ASYNC_ASYNC
	const char *s = getenv("SDISK_WIN32_ASYNC_ASYNC");
	if (s && *s)
		_use_async = atoi(s);
	/* XXX rename sync, should have a rename() for this class */
#endif
}


sdisk_win32_async_t::~sdisk_win32_async_t()
{
	if (_handle != INVALID_HANDLE_VALUE)
		W_COERCE(close());
}


w_rc_t	sdisk_win32_async_t::make(const char *name, int flags, int mode,
				  sdisk_t *&disk)
{
	sdisk_win32_async_t	*ud;
	w_rc_t		e;

	disk = 0;	/* default value*/
	
	ud = new sdisk_win32_async_t;
	if (!ud)
		return RC(fcOUTOFMEMORY);

	e = ud->open(name, flags, mode);
	if (e != RCOK) {
		delete ud;
		return e;
	}

	disk = ud;
	return RCOK;
}


w_rc_t	sdisk_win32_async_t::open(const char *name, int flags, int mode)
{
	/* XXX duplicates sdisk_win32 code */
	DWORD	access_mode;
	DWORD	share_mode;
	DWORD	create_mode;
	DWORD	attributes;

	access_mode = convert_access(flags);

	share_mode = convert_share(flags);

	create_mode = convert_create(flags);

	attributes = convert_attributes(flags) | FILE_FLAG_OVERLAPPED;

	w_rc_t	e;

	e = _open(name, access_mode, share_mode, create_mode, attributes);
	if (e != RCOK)
		return e;

	unsigned	i;
	HANDLE		h;

	for (i = 0; i < maxAsync; i++) {
		/* manual reset, not set */
		h = CreateEvent(0, TRUE, FALSE, 0);
		/* XXX cleanup on error, or have less slots */
		if (h == INVALID_HANDLE_VALUE)
			W_FATAL(fcWIN32);


		/* XXX error handling */
		_async[i].hEvent = h;
		W_COERCE(_event[i].change(_async[i].hEvent));
		W_COERCE(sthread_t::io_start(_event[i]));

		_inuse[i] = false;
		_num_concurrent[i] = 0;
	}

#ifdef SDISK_WIN32_ASYNC_ASYNC
	/* XXX If the sync thread can't be started it isn't the end
	   of the world.  We could always fallback */
	
	if (_use_async)
		W_COERCE(syncThread.start(_handle));
#endif

	_num_inuse = 0;
	_num_waiting = 0;
	_num_waited = 0;
//	_num_concurrent = 0;
	_max_concurrent = 0;
	_pos = 0;
	for (i = 0; i < 3; i++) {
		io_total[i] = 0;
		io_fast[i] = 0;
		io_time[i] = 0.0;
	}

	const char *s = strrchr(name, '/');
	if (s && s[1])
		s = s+1;
	else
		s = name;

	lock.rename("m:", "sdisk:", s);
	available.rename("c:", "sdisk:", s);
	
	return RCOK;
}


w_rc_t	sdisk_win32_async_t::close()
{
	w_rc_t	e = sdisk_win32_t::close();
	if (e != RCOK)
		return e;

	/* XXX should wait for any active I/O requests to complete to
	   be failsafe.  However, it is a complete usage error if the
	   user tries anything like that. */

	unsigned	i;
#ifdef SDISK_VERBOSE
	cout << "async:";
	if (io_total[0]) {
		cout << " read[" << io_total[0];
		if (io_fast[0])
			cout << ", fast=" << io_fast[0];
		cout << "]";
	}
	if (io_total[1]) {
		cout << " write[" << io_total[1];
		if (io_fast[1])
			cout << ", fast=" << io_fast[1];
		cout << "]";
	}
	if (io_total[2]) {
		cout << " sync[" << io_total[2] << "]";
#ifdef SDISK_WIN32_ASYNC_ASYNC
		if (_sync_waited)
			cout << " (" << _sync_waited << " waited)";
#endif
	}

	for (i = 0; i < 3; i++) {
		if (io_time[i] > 0) {
			if (i == 0)
				cout << " io_read=";
			else if (i == 1)
				cout << " io_write=";
			else
				cout << " io_sync=";
			W_FORM(cout)("%f", io_time[i]);
		}
	}

	if (_num_waited)
		cout << " #waited=" << _num_waited;
#if 1
	if (_max_concurrent) {
		cout << " #cc_io:";
		for (i = 0; i < maxAsync; i++)
			if (_num_concurrent[i])
				cout << " " << i+1 << ":" << _num_concurrent[i];
	}
#else
	if (_num_concurrent)
		cout << " #cc_io=" << _num_concurrent;
#endif
	if (_max_concurrent)
		cout << " #max_cc=" << _max_concurrent;
	if (maxAsync > 1)
		cout << " maxAsync=" << maxAsync;

	cout << endl;
#endif

	/* XXX error handling */

#ifdef SDISK_WIN32_ASYNC_ASYNC
	if (_use_async)
		W_COERCE(syncThread.stop());
#endif

	for (i = 0; i < maxAsync; i++) {
		W_COERCE(sthread_t::io_finish(_event[i]));
		W_COERCE(_event[i].change(INVALID_HANDLE_VALUE));

		CloseHandle(_async[i].hEvent);	/* XXX lost eror */
		_async[i].hEvent = INVALID_HANDLE_VALUE;
	}

	lock.rename("m:", "sdisk:");
	available.rename("c:", "sdisk:");

	return RCOK;
}


w_rc_t	sdisk_win32_async_t::wait(unsigned slot, long timeout)
{
	return _event[slot].wait(timeout);
}


/* Unlocked I/O to encapsulate async activity */
w_rc_t	sdisk_win32_async_t::_read(unsigned slot, void *buf, int count,
				   DWORD &done, bool &fast)
{
	bool	ok;
	DWORD	n;
	DWORD	err;

	done = 0;
	fast = false;

#ifdef	EXPENSIVE_STATS
	stime_t	start_time = stime_t::now();
#endif

	ok = ReadFile(_handle, buf, count, &n, _async + slot);
	if (!ok) {
		err = GetLastError();
		if (err != ERROR_IO_PENDING)
			return RC2(fcWIN32, err);

		/* XXX shutdown? */
		W_COERCE(wait(slot));

		ok = GetOverlappedResult(_handle, _async+slot, &n, TRUE);
		if (!ok)
			return RC(fcWIN32);

#ifdef EXPENSIVE_STATS
		SthreadStats.io_time += (float) (stime_t::now() - start_time);
#endif
	}
	else {
#ifdef EXPENSIVE_STATS
		double	delta =  stime_t::now() - start_time;
		SthreadStats.io_time += (float) delta;
		SthreadStats.idle_time += (float) delta;
		io_time[0] += delta;
#endif
		fast = true;
		io_fast[0]++;
	}
	io_total[0]++;

	done = n;

	return RCOK;
}


w_rc_t	sdisk_win32_async_t::_write(unsigned slot, const void *buf, int count,
				    DWORD &done, bool &fast)
{
	bool	ok;
	DWORD	n;
	DWORD	err;

	done = 0;
	fast = false;

#ifdef	EXPENSIVE_STATS
	stime_t	start_time = stime_t::now();
#endif

	ok = WriteFile(_handle, buf, count, &n, _async + slot);
	if (!ok) {
		err = GetLastError();
		if (err != ERROR_IO_PENDING)
			return RC2(fcWIN32, err);

		/* XXX shutdown? */
		W_COERCE(wait(slot));

		ok = GetOverlappedResult(_handle, _async+slot, &n, TRUE);
		if (!ok)
			return RC(fcWIN32);

#ifdef EXPENSIVE_STATS
		SthreadStats.io_time += (float) (stime_t::now() - start_time);
#endif
	}
	else {
#ifdef EXPENSIVE_STATS
		double	delta =  stime_t::now() - start_time;
		SthreadStats.io_time += (float) delta;
		SthreadStats.idle_time += (float) delta;
		io_time[1] += delta;
#endif
		fast = true;
		io_fast[1]++;
	}
	io_total[1]++;

	done = n;

	return RCOK;
}


inline w_rc_t	sdisk_win32_async_t::allocate_slot(unsigned &slot)
{
	W_DO(lock.acquire());

#if 0 && defined(SDISK_WIN32_ASYNC_ASYNC)
	if (sync_in_progress)	/* XXX */
		SthreadStats.cc_sync_io++;
#endif

	/* abbreviated lower-overhead lock/release protocol for 1 slot case. */
	if (maxAsync == 1) {
		slot = 0;
		_inuse[slot] = true;
		_num_inuse = 1;
		return RCOK;
	}

	if (_num_inuse >= maxAsync) {
		_num_waited++;
		_num_waiting++;
		while (_num_inuse >= maxAsync)
			W_COERCE(available.wait(lock));
		_num_waiting--;
	}

	unsigned	i;
	for (i = 0; i < maxAsync; i++)
		if (!_inuse[i])
			break;
	if (i == maxAsync) {
		lock.release();
		return RC(stINTERNAL);
	}

	_num_inuse++;
	_inuse[i] = true;

	if (_num_inuse > 1) {
#if 1
		_num_concurrent[_num_inuse-1]++;
#else
		_num_concurrent++;
#endif
		if (_num_inuse > _max_concurrent)
			_max_concurrent = _num_inuse;
	}

	lock.release();

	slot = i;
	return RCOK;
}

inline w_rc_t	sdisk_win32_async_t::release_slot(unsigned slot)
{
	/* abbreviated lower-overhead lock/release protocol for 1 slot case. */
	if (maxAsync == 1) {
		w_assert3(slot == 0);
		_inuse[slot] = false;
		_num_inuse = 0;
		lock.release();
		return RCOK;
	}

	W_DO(lock.acquire());

	w_assert3(slot < maxAsync && _inuse[slot]);

	_num_inuse--;
	_inuse[slot] = false;

	if (_num_waiting)
		available.signal();

	lock.release();
	return RCOK;
}


w_rc_t	sdisk_win32_async_t::read(void *buf, int count, int &done)
{
	if (_handle == INVALID_HANDLE_VALUE)
		return RC(stBADFD);

#ifdef BROKEN_IO_HACK
	fileoff_t	io_pos = _pos;
#endif

	unsigned	slot;
	W_COERCE(allocate_slot(slot));

	bool	ok;
	DWORD	n;
	DWORD	err;
	bool	fast = false;

#ifdef BROKEN_IO_HACK
	getOffset(_async[slot], io_pos);
#else
	getOffset(_async[slot], _pos);
#endif

	SthreadStats.diskrw_io++;
#ifdef	EXPENSIVE_STATS
	stime_t	start_time = stime_t::now();
#endif

	ok = ReadFile(_handle, buf, count, &n, _async + slot);
	if (!ok) {
		err = GetLastError();
		if (err != ERROR_IO_PENDING) {
			W_COERCE(release_slot(slot));
			return RC2(fcWIN32, err);
		}

		/* XXX shutdown? */
		W_COERCE(wait(slot));

		ok = GetOverlappedResult(_handle, _async+slot, &n, TRUE);
		if (!ok) {
			W_COERCE(release_slot(slot));
			return RC(fcWIN32);
		}
#ifdef EXPENSIVE_STATS
		SthreadStats.io_time += (float) (stime_t::now() - start_time);
#endif
	}
	else {
#ifdef EXPENSIVE_STATS
		double	delta =  stime_t::now() - start_time;
		SthreadStats.io_time += (float) delta;
		SthreadStats.idle_time += (float) delta;
		io_time[0] += delta;
#endif
		fast = true;
		io_fast[0]++;
	}

#ifdef BROKEN_IO_HACK
	_pos = io_pos + n;
#else
	_pos += n;
#endif

	W_COERCE(release_slot(slot));

	io_total[0]++;

	done = n;

	if (fast)
		sthread_t::yield();

	return RCOK;
}


w_rc_t	sdisk_win32_async_t::write(const void *buf, int count, int &done)
{
	if (_handle == INVALID_HANDLE_VALUE)
		return RC(stBADFD);

#ifdef BROKEN_IO_HACK
	fileoff_t	io_pos = _pos;
#endif

	unsigned	slot;
	W_COERCE(allocate_slot(slot));

	bool	ok;
	DWORD	n;
	DWORD	err;
	bool	fast = false;

#ifdef BROKEN_IO_HACK
	getOffset(_async[slot], io_pos);
#else
	getOffset(_async[slot], _pos);
#endif

	SthreadStats.diskrw_io++;
#ifdef	EXPENSIVE_STATS
	stime_t	start_time = stime_t::now();
#endif

	ok = WriteFile(_handle, buf, count, &n, _async+slot);
	if (!ok) {
		err = GetLastError();
		if (err != ERROR_IO_PENDING) {
			W_COERCE(release_slot(slot));
			return RC2(fcWIN32, err);
		}

		/* XXX shutdown? */
		W_COERCE(wait(slot));

		ok = GetOverlappedResult(_handle, _async+slot, &n, TRUE);
		if (!ok) {
			W_COERCE(release_slot(slot));
			return RC(fcWIN32);
		}
#ifdef EXPENSIVE_STATS
		SthreadStats.io_time += (float) (stime_t::now() - start_time);
#endif
	}
	else {
#ifdef EXPENSIVE_STATS
		double	delta =  stime_t::now() - start_time;
		SthreadStats.io_time += (float) delta;
		SthreadStats.idle_time += (float) delta;
		io_time[1] += delta;
#endif
		fast = true;
		io_fast[1]++;
	}

#ifdef BROKEN_IO_HACK
	_pos = io_pos + n;
#else
	_pos += n;
#endif

	io_total[1]++;

	W_COERCE(release_slot(slot));

	done = n;

	if (fast)
		sthread_t::yield();

	return RCOK;
}


w_rc_t	sdisk_win32_async_t::readv(const iovec_t *iov, int iovcnt, int &done)
{
	if (_handle == INVALID_HANDLE_VALUE)
		return RC(stBADFD);

	fileoff_t	io_pos = _pos;

	unsigned	slot;
	W_COERCE(allocate_slot(slot));

	DWORD	n;
	DWORD	total = 0;
	int	v;
	w_rc_t	e;
	bool	fast = true;
	bool	this_fast;

	SthreadStats.diskrw_io++;

	for (v = 0; v < iovcnt; v++) {
		getOffset(_async[slot], io_pos);
		e = _read(slot, iov[v].iov_base, iov[v].iov_len, n, this_fast);
		if (e != RCOK)
			break;
		total += n;
		io_pos += n;
		fast = fast && this_fast;
	}

	_pos = io_pos;

	W_COERCE(release_slot(slot));

	done = total;

	if (fast)
		sthread_t::yield();

	return e;
}


w_rc_t	sdisk_win32_async_t::writev(const iovec_t *iov, int iovcnt, int &done)
{
	if (_handle == INVALID_HANDLE_VALUE)
		return RC(stBADFD);

	/* XXX broken io hack semantics overlap writev, since all the
	   vectors are written sequentially, and it is OK.  This takes
	   care of BROKEN_IO_HACK, and works correctly with or without
	   that option enabled. */
	fileoff_t	io_pos = _pos;

	/* Coalesce the iovec_t to minimize user<>kernel transitions.
	   This could be controlled by a flag IFF some applications
	   didn't work without an I/O per iov segment.  This is not
	   something to worry about, but I have seen "hints" that vector
	   I/O on SOME OSes may have odd semantics doing I/O to SOME
	   devices (such as records on a tape drive). */

	/* XXX once diskrw uses sdisk, coupling problem goes away */
	iovec_t	newVec[sthread_t::iovec_max]; /* XXX coupling bad */
	int	newCnt = 0;
	if (iovcnt > 1) 
		vcoalesce(iov, iovcnt, newVec, newCnt);
	if (newCnt && newCnt < iovcnt) {
		SthreadStats.writev_coalesce++;
#ifdef STHREAD_writev_coal_count
		SthreadStats.writev_coal_count += iovcnt - newCnt;
#endif
		iov = newVec;
		iovcnt = newCnt;
	}
		
	unsigned	slot;
	W_COERCE(allocate_slot(slot));

	DWORD	n;
	DWORD	total = 0;
	int	v;
	w_rc_t	e;
	bool	fast = true;
	bool	this_fast;

	SthreadStats.diskrw_io++;

	for (v = 0; v < iovcnt; v++) {
		getOffset(_async[slot], io_pos);
		e = _write(slot, iov[v].iov_base, iov[v].iov_len, n, this_fast);
		if (e != RCOK)
			break;
		total += n;
		io_pos += n;
		fast = fast && this_fast;
	}

	_pos = io_pos;

	W_COERCE(release_slot(slot));

	done = total;

	if (fast)
		sthread_t::yield();

	return e;
}


w_rc_t	sdisk_win32_async_t::pread(void *buf, int count, fileoff_t pos,
				   int &done)
{
	if (_handle == INVALID_HANDLE_VALUE)
		return RC(stBADFD);

	if (pos < 0)
		return RC(stINVAL);

	unsigned	slot;
	W_COERCE(allocate_slot(slot));

	bool	ok;
	DWORD	n;
	DWORD	err;
	bool	fast = false;

	getOffset(_async[slot], pos);

	SthreadStats.diskrw_io++;
#ifdef	EXPENSIVE_STATS
	stime_t	start_time = stime_t::now();
#endif

	ok = ReadFile(_handle, buf, count, &n, _async + slot);
	if (!ok) {
		err = GetLastError();
		if (err != ERROR_IO_PENDING) {
			W_COERCE(release_slot(slot));
			return RC2(fcWIN32, err);
		}

		/* XXX shutdown? */
		W_COERCE(wait(slot));

		ok = GetOverlappedResult(_handle, _async+slot, &n, TRUE);
		if (!ok) {
			W_COERCE(release_slot(slot));
			return RC(fcWIN32);
		}
#ifdef EXPENSIVE_STATS
		SthreadStats.io_time += (float) (stime_t::now() - start_time);
#endif
	}
	else {
#ifdef EXPENSIVE_STATS
		double	delta =  stime_t::now() - start_time;
		SthreadStats.io_time += (float) delta;
		SthreadStats.idle_time += (float) delta;
		io_time[0] += delta;
#endif
		fast = true;
		io_fast[0]++;
	}

	io_total[0]++;
	W_COERCE(release_slot(slot));

	done = n;

	if (fast)
		sthread_t::yield();

	return RCOK;
}


w_rc_t	sdisk_win32_async_t::pwrite(const void *buf, int count, 
				    fileoff_t pos, int &done)
{
	if (_handle == INVALID_HANDLE_VALUE)
		return RC(stBADFD);

	if (pos < 0)
		return RC(stINVAL);

	unsigned	slot;
	W_COERCE(allocate_slot(slot));

	bool	ok;
	DWORD	n;
	DWORD	err;
	bool	fast = false;

	getOffset(_async[slot], pos);

	SthreadStats.diskrw_io++;
#ifdef	EXPENSIVE_STATS
	stime_t	start_time = stime_t::now();
#endif

	ok = WriteFile(_handle, buf, count, &n, _async+slot);
	if (!ok) {
		err = GetLastError();
		if (err != ERROR_IO_PENDING) {
			W_COERCE(release_slot(slot));
			return RC2(fcWIN32, err);
		}

		/* XXX shutdown? */
		W_COERCE(wait(slot));

		ok = GetOverlappedResult(_handle, _async+slot, &n, TRUE);
		if (!ok) {
			W_COERCE(release_slot(slot));
			return RC(fcWIN32);
		}
#ifdef EXPENSIVE_STATS
		SthreadStats.io_time += (float) (stime_t::now() - start_time);
#endif
	}
	else {
#ifdef EXPENSIVE_STATS
		double	delta =  stime_t::now() - start_time;
		SthreadStats.io_time += (float) delta;
		SthreadStats.idle_time += (float) delta;
		io_time[1] += delta;
#endif
		fast = true;
		io_fast[1]++;
	}

	io_total[1]++;
	W_COERCE(release_slot(slot));

	done = n;

	if (fast)
		sthread_t::yield();

	return RCOK;
}


w_rc_t	sdisk_win32_async_t::seek(fileoff_t pos, int origin, fileoff_t &newpos)
{
	if (_handle == INVALID_HANDLE_VALUE)
		return RC(stBADFD);

#ifdef notyet
	LONG		size[2];
	fileoff_t	p;
	DWORD		n;
#endif

	switch (origin) {
	case SEEK_AT_SET:
		if (pos < 0)
			return RC(stINVAL);
		_pos = pos;
		break;
	case SEEK_AT_CUR:
		_pos += pos;
		break;
	case SEEK_AT_END:
#ifdef notyet
		/* 32bits for now */
		n = GetFileSize(_handle, size+1);
		if (l == -1)
			return RC(fcWIN32);
		size[0] = n;

		setOffset(_pos, size);
		_pos += pos;
		break;
#else
		cerr << "sdisk_win32_async_t::seek():"
			<< " SEEK_AT_END not supported" << endl;
		/*FALLTHROUGH*/
#endif
	default:
		return RC(stINVAL);
		break;
	}
		
	newpos = _pos;
	return RCOK;
}


/* XXX improve seek pointer error handling */
w_rc_t	sdisk_win32_async_t::truncate(fileoff_t size)
{
	if (_handle == INVALID_HANDLE_VALUE)
		return RC(stBADFD);

	w_rc_t	e;

	W_COERCE(lock.acquire());

	e = sdisk_win32_t::truncate(size);

	if (e == RCOK) {
		/* reset the seek pointer to keep it valid */
		if (_pos >= size)
			_pos = size;
	}

	lock.release();
	
	return e;
}

/* XXX sync not supported except for a file mode.
   Could have a sync which returns an error if the file
   wasn't opened for file synchronization */

w_rc_t	sdisk_win32_async_t::sync()
{
	if (_handle == INVALID_HANDLE_VALUE)
		return RC(stBADFD);

	w_rc_t	e;

#ifdef SDISK_WIN32_ASYNC_ASYNC
	/* XXX stats ... just io_time I think? */

#ifdef EXPENSIVE_STATS
	stime_t	start_time = stime_t::now();
#endif

	if (_use_async) {
		SthreadStats.diskrw_io++;

		/* This code has nothing to do with the sync thread itself,
		   it just prevents multiple sync's from occuring at once
		   and hosing the sync thread / error return interface.
		   If multiple syncs occur at once they should be queued,
		   so sync's can be batched, which eliminates user<->kernel
		   transitions. */

		W_COERCE(lock.acquire());
		bool	waited = false;
#if 0
		if (_num_inuse)
			SthreadStats.cc_sync_io++;
#endif
		if (sync_in_progress) {
			waited = true;
			sync_waiters++;
		}
		while (sync_in_progress) {
			W_COERCE(syncAvailable.wait(lock));
		}

		sync_in_progress = true;
		if (waited) {
			sync_waiters--;
			// XXX stats update need not be locked
			_sync_waited++;
#if 0
			SthreadStats.cc_sync++;
#endif
		}
		lock.release();
	
		e = syncThread.sync();

		W_COERCE(lock.acquire());
		sync_in_progress = false;
		/* XXX convoy versus reality, grouped sync waits, etc */
		if (sync_waiters)
			syncAvailable.signal();
		lock.release();
	}
	else {
		SthreadStats.local_io++;
		bool	ok;
		ok = FlushFileBuffers(_handle);
		if (!ok) {
			DWORD	err = GetLastError();
			if (err != ERROR_ACCESS_DENIED && err != ERROR_INVALID_FUNCTION)
				e = RC2(fcWIN32, err);
		}
	}

#ifdef EXPENSIVE_STATS
	double	delta = stime_t::now() - start_time;
	io_time[2] += delta;
	io_total[2]++;
	SthreadStats.io_time += (float) delta;
#endif
#else	/* SDISK_WIN32_ASYNC_ASYNC */

#if 1
#ifdef EXPENSIVE_STATS
	SthreadStats.local_io++;
	stime_t	start_time = stime_t::now();
#endif

	bool	ok;
	ok = FlushFileBuffers(_handle);
	if (!ok) {
		DWORD	err = GetLastError();
		if (err != ERROR_ACCESS_DENIED && err != ERROR_INVALID_FUNCTION)
			e = RC2(fcWIN32, err);
	}

#ifdef EXPENSIVE_STATS
	double	delta = stime_t::now() - start_time;
	io_time[2] += delta;
	io_total[2]++;
	SthreadStats.idle_time += (float) delta;
	SthreadStats.io_time += (float) delta;
#endif
#else
	e = sdisk_win32_t::sync();
#endif

	sthread_t::yield();
#endif	/* SDISK_WIN32_ASYNC_ASYNC */
	
	return e;
}


#ifdef SDISK_WIN32_ASYNC_ASYNC

/* XXX If the syncThread was new/deleted, state transitions become much
   easier and cleaner. */

sdisk_win32_async_t::SyncThread::SyncThread()
: handle(INVALID_HANDLE_VALUE),
  thread(INVALID_HANDLE_VALUE),
  threadId(0),
  request(INVALID_HANDLE_VALUE),
  _done(INVALID_HANDLE_VALUE),
  abort(false),
  _err(0)
{
}

w_rc_t	sdisk_win32_async_t::SyncThread::start(HANDLE flush_handle)
{
	handle = flush_handle;
	abort = false;
	_err = 0;

	HANDLE	h;

	/* auto reset, not set */
	h = CreateEvent(0, FALSE, FALSE, 0);
	if (h == INVALID_HANDLE_VALUE)
		W_FATAL(fcWIN32);
	request = h;

	/* XXX I forget, does this need to be manual reset to be reliable? */
	/* auto reset, not set */
	h = CreateEvent(0, FALSE, FALSE, 0);
	if (h == INVALID_HANDLE_VALUE)
		W_FATAL(fcWIN32);
	_done = h;

	W_COERCE(done.change(_done));
	W_COERCE(sthread_t::io_start(done));

#ifdef _MT
	h = (HANDLE) _beginthreadex(NULL, 0,
				    _start, this,
				    0,
				    &threadId);
#else
	h = CreateThread(NULL, 0,
			 _start, this,
			 0,
			 &threadId);
#endif
	if (h == INVALID_HANDLE_VALUE)
		W_FATAL(fcWIN32);
	thread = h;

	return RCOK;
}

w_rc_t	sdisk_win32_async_t::SyncThread::stop()
{
	abort = true;
	handle = INVALID_HANDLE_VALUE;

	bool	ok;
	ok = SetEvent(request);
	if (!ok)
		W_FATAL(fcWIN32);
	
	/* XXX could setup a wait event on the thread */
	DWORD	err;
	err = WaitForSingleObject(thread, INFINITE);
	if (err != WAIT_OBJECT_0)
		W_FATAL(fcWIN32);
	CloseHandle(thread);
	thread = INVALID_HANDLE_VALUE;

	W_COERCE(sthread_t::io_finish(done));
	W_COERCE(done.change(INVALID_HANDLE_VALUE));

	CloseHandle(_done);
	_done = INVALID_HANDLE_VALUE;

	CloseHandle(request);
	request = INVALID_HANDLE_VALUE;

	return RCOK;
}


w_rc_t	sdisk_win32_async_t::SyncThread::sync()
{
	/* XXX should synchronize to prevent multiple requests from
	   trampling, also to batch syncs */

	bool	ok;

	ok = SetEvent(request);
	if (!ok)
		W_FATAL(fcWIN32);
	
	W_COERCE(done.wait(sthread_t::WAIT_FOREVER));

	return _err ? RC2(fcWIN32, _err) : RCOK;
}


DWORD	sdisk_win32_async_t::SyncThread::run()
{
//	cout << "SyncThread: running" << endl;
	for (;;) {
		DWORD	err;
		bool	ok;

		err = WaitForSingleObject(request, INFINITE);

		/* XXX error handling */
		if (err == WAIT_FAILED)
			W_FATAL(fcWIN32);
		else if (err == WAIT_TIMEOUT)
			W_FATAL(fcWIN32);
		else if (err != WAIT_OBJECT_0)
			W_FATAL(fcWIN32);

		if (abort) {
			_err = 0;
			break;
		}

		/* XXX error propogation isn't exactly great, since
		   it isn't guaranteed to by synchronized.  Probably
		   need to put a mutex around sync requests to
		   prevent problems anyway, and that would allow
		   batch syncing of I/Os too */
		_err = 0;

		ok = FlushFileBuffers(handle);
		if (!ok) {
			err = GetLastError();
			if (err != ERROR_ACCESS_DENIED
			    && err != ERROR_INVALID_FUNCTION)
				_err = err;
		}

		ok = SetEvent(_done);
		if (!ok)
			W_FATAL(fcWIN32);
	}
//	cout << "SyncThread: exiting" << endl;

	return 0;
}

#ifdef _MT
#define	THREAD_RETURNS	unsigned
#else
#define	THREAD_RETURNS	DWORD
#endif

/* XXX SB SyncThread methods */

THREAD_RETURNS	__stdcall	sdisk_win32_async_t::SyncThread::_start(void *arg)
{
	SyncThread	&sd = *(SyncThread *)arg;
	return sd.run();
}

#endif

/*<std-header orig-src='shore'>

 $Id: sdisk_win32_async.cpp,v 1.7 1999/06/07 19:06:04 kupsch Exp $

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
 *   NewThreads I/O is Copyright 1995, 1996, 1997, 1998 by:
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


sdisk_win32_async_t::sdisk_win32_async_t()
: _pos(0)
{
	unsigned	i;

	for (i = 0; i < maxAsync; i++) {
		_async[i].hEvent = INVALID_HANDLE_VALUE;
	}

	for (i = 0; i < 3; i++) {
		io_total[i] = 0;
		io_fast[i] = 0;
		io_time[i] = 0.0;
	}
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
	}

	_pos = 0;
	for (i = 0; i < 3; i++) {
		io_total[i] = 0;
		io_fast[i] = 0;
		io_time[i] = 0.0;
	}
	
	return RCOK;
}


w_rc_t	sdisk_win32_async_t::close()
{
	w_rc_t	e = sdisk_win32_t::close();
	if (e != RCOK)
		return e;

	unsigned	i;
#if 0
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
	if (io_total[2])
		cout << " sync[" << io_total[2] << "]";

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

	cout << endl;
#endif

	/* XXX error handling */
	for (i = 0; i < maxAsync; i++) {
		W_COERCE(sthread_t::io_finish(_event[i]));
		W_COERCE(_event[i].change(INVALID_HANDLE_VALUE));

		CloseHandle(_async[i].hEvent);	/* XXX lost eror */
		_async[i].hEvent = INVALID_HANDLE_VALUE;
	}

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
	slot = 0;
	return lock.acquire();
}

inline w_rc_t	sdisk_win32_async_t::release_slot(unsigned slot)
{
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


#ifdef BROKEN_IO_HACK
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
#endif /* BROKEN_IO_HACK */


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

	W_COERCE(lock.acquire());

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

	lock.release();

	sthread_t::yield();
	
	return e;
}

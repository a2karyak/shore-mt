/*<std-header orig-src='shore'>

 $Id: io.cpp,v 1.30 2000/02/21 23:22:20 bolo Exp $

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
 *   NewThreads is Copyright 1992, 1993, 1994, 1995, 1996, 1997 by:
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

#define	IO_C

#define W_INCL_LIST
#define W_INCL_SHMEM
#define W_INCL_TIMER
#include <w.h>

#include <w_debug.h>
#include <w_stream.h>
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

#ifndef _WINDOWS
#include <sys/wait.h>
#endif
#include <new.h>

#include <sys/stat.h>
#include <w_rusage.h>

#include <w_statistics.h>

#include <unix_stats.h>

#include "sthread.h"
#include "sthread_stats.h"
#include "spin.h"
#include "ready_q.h"

#if defined(_WIN32) && defined(NEW_IO)
#include <win32_events.h>
#else
#include <sfile_handler.h>
#endif

#include <sdisk.h>
#if defined(STHREAD_IO_UNIX)
#include <sdisk_unix.h>
#endif
#if defined(STHREAD_IO_DISKRW)
#include <diskrw.h>
#include <sdisk_diskrw.h>
#endif
#if defined(STHREAD_IO_WIN32)
#include <sdisk_win32.h>
#include <sdisk_win32_async.h>
#endif


extern class sthread_stats SthreadStats;


/* I/O system related sthread statics */
bool			sthread_t::_do_fastpath = false;
const char* 		sthread_t::_diskrw = "diskrw";
int			sthread_t::_io_in_progress = 0;
#ifdef STHREAD_IO_DISKRW
sdisk_handler_t		*sthread_t::sdisk_handler = 0;
#else
char			*sthread_t::disk_buffer = 0;
#endif
#ifndef STHREAD_FDS_STATIC
sdisk_t			**sthread_t::_disks = 0;
unsigned		sthread_t::open_max = 0;
#else
sdisk_t			*sthread_t::_disks[sthread_t::open_max];
#endif
unsigned		sthread_t::open_count = 0;

#if defined(STHREAD_IO_WIN32) && !defined(NEW_IO_ONLY)
static	bool		io_local_unix = false;
static	bool		io_async_unix = false;
#endif


/* XXX not in class because it isn't defined.  Could be allocated
   at sthread initialization.  Can wait for fix of sync. variables.
   to be thread independent. */
static			smutex_t	diskIO;


w_rc_t	_caught_signal_io(int )
{
#ifdef notyet
	if (! (sig == SIGINT && _dummy_disk != 0))
		return RCOK;

	// cntrl-C, so signal dummy disk process so it can free memory
	cerr << "ctrl-C detected -- killing disk processes" 
		<< endl << flush;

	// signal dummy diskrw process so it can kill the others
	// and free shared memory.
	if (kill(_dummy_disk, SIGINT) == -1) {
		cerr << 
		"error: cannot kill disk process that cleans up shared memory(" 
		 << _dummy_disk << ')' << endl
		 << RC(stOS) << endl;
		cerr << "You might have to clean up shared memory segments by hand."
		<< endl;
		cerr << "See ipcs(1) and ipcrm(1)."<<endl;
		_exit(1);
	} else {
		cerr << "ctrl-C detected -- exiting" << endl;
		_exit(0);
	}
#endif
	return RCOK;
}


w_rc_t	sthread_t::startup_io()
{
	char	*s;

	s = getenv("STHREAD_IO_FASTPATH");
	if (s && *s)
		_do_fastpath = atoi(s);

#if defined(STHREAD_IO_WIN32) && !defined(NEW_IO_ONLY)
	s = getenv("STHREAD_IO_LOCAL");
	if (!s)
		s = "";
	switch (s[0]) {
	case 'u':
	case 'U':
	case 'd':
	case 'D':
		io_local_unix = true;
		break;

	case 'w':
	case 'W':
	default:
		io_local_unix = false;
		break;
	}
	

	s = getenv("STHREAD_IO_ASYNC");
	if (!s)
		s = "";
	switch (s[0]) {
	case 'u':
	case 'U':
	case 'd':
	case 'D':
		io_async_unix = true;
		break;

	case 'w':
	case 'W':
	default:
		io_async_unix = false;
		break;
	}
	
#endif

	return RCOK;
}

w_rc_t	sthread_t::shutdown_io()
{
	/* shutdown disk i/o */

#ifdef notyet
	/* Clean up and exit. */
	/* XXX needs work */
	if (shmem_seg.base()) {
		kill(_dummy_disk, SIGHUP);
		for (int i = 0; i < open_max; i++)  {
			if (diskport[i].pid && diskport[i].pid != -1)
				kill(diskport[i].pid, SIGHUP);
		}
		if (svcport) {
			svcport->~svcport_t();
			svcport = 0;
		}
		shmem_seg.destroy();
	}
	unname();	// clean up to avoid assertion in fastnew
#endif

	return RCOK;
}


/*
 * Allocate and return a memory buffer for I/O to be done
 * in.  This is needed because some I/O mechanisms require
 * a dedicated memory area that can have I/O done to/from it.
 *
 * The diskrw mechanism used for traditional Unix I/O needs this.
 *
 * Other implementations may not need the dedicated segment,
 * and memory is just allocated in that case.
 *
 * If the segment size is zero, the segment is deallocated.
 */

w_rc_t sthread_t::set_bufsize(unsigned size, char *&buf_start)
{
#ifdef STHREAD_IO_DISKRW
	if (sdisk_handler && size == 0) {
		w_rc_t	e;
		/* XXX error handling */
		e = sdisk_handler->shutdown();
		if (e != RCOK)
			return e;
		delete sdisk_handler;
		sdisk_handler = 0;
		return RCOK;
	}
	else if (sdisk_handler) {
		cerr << "Can't re-allocate disk buffer" << endl;
		return RC(stINTERNAL);
	}

	w_rc_t	e;

	buf_start = 0;

	e = sdisk_handler_t::make(size, _diskrw, sdisk_handler);
	if (e != RCOK)
		return e;

	buf_start = (char *)sdisk_handler->ioBuffer();

	return RCOK;
#else
	/* Without DISKRW, just allocate/free a chunk of memory */
	if (disk_buffer && size == 0) {
		delete [] disk_buffer;
		disk_buffer = 0;
		return RCOK;
	}
	else if (disk_buffer) {
		cerr << "Can't re-allocate disk buffer without disabling"
			<< endl;
		return RC(stINTERNAL);
	}

	buf_start = 0;

	disk_buffer = new char[size];
	if (!disk_buffer)
		return RC(fcOUTOFMEMORY);

	buf_start = disk_buffer;
	return RCOK;
#endif
}


char *sthread_t::set_bufsize(unsigned size)
{
	w_rc_t	e;
	char	*start;

	e = set_bufsize(size, start);

	if (e != RCOK) {
		cerr << "Hidden Failure: set_bufsize(" << size << "):"
			<< endl << e << endl;
		return 0;
	}

	/* compatability on free */
	if (size == 0)
		start = 0;

	return start;
}


w_rc_t
sthread_t::open(const char* path, int flags, int mode, int &ret_fd)
{
	w_rc_t	e;
	sdisk_t	*dp;

	/* default return value */
	ret_fd = -1;

	bool	open_local = (flags & OPEN_LOCAL);
#ifdef SDISK_DISKRW_HACK
	bool	open_keep = (flags & OPEN_KEEP);
#endif
#ifdef notyet
	bool	open_fast = (flags & OPEN_FAST);
#endif

	/* Remove sthread-specific open flags */
	flags &= ~OPEN_STHREAD;

#ifdef SDISK_DISKRW_HACK
	if (open_keep)
		flags |= sdisk_t::OPEN_KEEP_HACK;
#endif

	W_COERCE(diskIO.acquire());

	if (open_count >= open_max) {
#ifndef STHREAD_FDS_STATIC
		/* reallocate file table */
		/* XXXX open slop, or some better algorithm */
		unsigned	new_max = open_max + 64;
		sdisk_t	**new_disks = new sdisk_t *[new_max];
		/* XXX could generate chained error or duplicate existing */
		if (!new_disks) {
			diskIO.release();
			return RC(fcOUTOFMEMORY);
		}
		unsigned	disk;
		for (disk = 0; disk < open_count; disk++)
			new_disks[disk] = _disks[disk];
		for (; disk < new_max; disk++)
			new_disks[disk] = 0;
		sdisk_t	**tmp = _disks;
		_disks = new_disks;
		open_max = new_max;
		delete [] tmp;
#else
		diskIO.release();
		return RC(stTOOMANYOPEN);
#endif
	}

	/* XXX incredibly slow when #fds large */
	unsigned	disk;
	for (disk = 0; disk < open_max; disk++)
		if (!_disks[disk])
			break;
	if (disk == open_max) {
		diskIO.release();
		return RC(stINTERNAL);	/* XXX or toomanyopen */
	}

	/* XXX can allow sim. open by locking lower levels, put dummy
		pointer in array, unlocking here, opening, etc */

	if (open_local) {
#if defined(STHREAD_IO_WIN32) && !defined(NEW_IO_ONLY)
		if (io_local_unix)
			e = sdisk_unix_t::make(path, flags, mode, dp);
		else
			e = sdisk_win32_t::make(path, flags, mode, dp);
#elif defined(STHREAD_IO_WIN32)
		e = sdisk_win32_t::make(path, flags, mode, dp);
#elif defined(STHREAD_IO_UNIX)
		e = sdisk_unix_t::make(path, flags, mode, dp);
#else
		cerr << "LOCAL I/O NOT CONFIGURED" << endl;
		e = RC(stNOTIMPLEMENTED);
#endif
	}
	else {
#if defined(STHREAD_IO_WIN32) && !defined(NEW_IO_ONLY)
		if (io_async_unix) {
			if (sdisk_handler)
				e = sdisk_diskrw_t::make(*sdisk_handler, path, flags, mode, dp);
			else {
				cerr << "Set_bufsize needed for diskrw I/O" << endl;
				e = RC(stINTERNAL);
			}
		}
		else
			e = sdisk_win32_async_t::make(path, flags, mode, dp);
#elif defined(STHREAD_IO_WIN32)
		e = sdisk_win32_async_t::make(path, flags, mode, dp);
#elif defined(STHREAD_IO_DISKRW)
		if (sdisk_handler)
			e = sdisk_diskrw_t::make(*sdisk_handler, path, flags, mode, dp);
		else {
			cerr << "Set_bufsize needed for diskrw I/O" << endl;
			e = RC(stINTERNAL);
		}
#else
		cerr << "ASYNC I/O NOT CONFIGURED" << endl;
		e = RC(stNOTIMPLEMENTED);
#endif
	}


#ifdef notyet
	if (open_fastpath) {
		/* open both local and remote disks, install fastpath module */
		e = sdisk_fastpath_t::make(path, flags, mode, dp);
	}
#endif

	if (e != RCOK) {
		diskIO.release();
		return e;
	}

	_disks[disk] = dp;
	open_count++;
	diskIO.release();

	ret_fd = fd_base + disk;
	
	return RCOK;
}



/*
 *  sthread_t::close(fd)
 *
 *  Close a file previously opened with sthread_t::open(). Kill the
 *  diskrw process for this file.
 */

w_rc_t sthread_t::close(int fd)
{
	fd -= fd_base;
	if (fd < 0 || fd >= (int)open_max || !_disks[fd])
		return RC(stBADFD);

	w_rc_t	e;

	// sync before close
	e = _disks[fd]->sync();
	if (e != RCOK)
		return e;

	e = _disks[fd]->close();
	if (e != RCOK)
		return e;

	W_COERCE(diskIO.acquire());
	sdisk_t	*togo = _disks[fd];
	_disks[fd] = 0;
	open_count--;
	diskIO.release();

	delete togo;
	
	return e;
}

/*
 *  sthread_t::write(fd, buf, n)
 *  sthread_t::writev(fd, iov, iovcnt)
 *  sthread_t::read(fd, buf, n)
 *  sthread_t::readv(fd, iov, iovcnt)
 *  sthread_t::fsync(fd)
 *  sthread_t::ftruncate(fd, len)
 *
 *  Perform I/O.
 *
 *  XXX Currently I/O operations that don't have a complete character
 *  count return with a "SHORTIO" error.  In the future,
 *  there should be two forms of read and
 *  write operations.  The current style which returns errors
 *  on "Short I/O", and a new version which can return a character
 *  count, or "Short I/O" if a character count can't be
 *  determined.
 *
 *  XXX various un-const casts are included below.  Some of them
 *  will be undone when cleanup hits.  Others should be
 *  propogated outward to the method declarations in sthread.h
 *  to match what the underlying OSs may guarantee.
 */

/* XXX use scoped class to make automagic I/O monitor */
class IOmonitor {
	bool	nothing;
public:
	inline	IOmonitor(int &counter);
	inline	~IOmonitor();
};

inline IOmonitor::IOmonitor(int &counter)
: nothing(false)
{
	counter++;

	SthreadStats.io++;
	sthread_t::_io_in_progress++;

	if (sthread_t::_io_in_progress > 1) {
		SthreadStats.ccio++;
#ifdef EXPENSIVE_STATS
		switch (sthread_t::_io_in_progress) {
		case 2:
			SthreadStats.ccio2++;
			break;
		case 3:
			SthreadStats.ccio3++;
			break;
		case 4:
			SthreadStats.ccio4++;
			break;
		}
#endif
	}
}


inline	IOmonitor::~IOmonitor()
{
	sthread_t::_io_in_progress--;
}


w_rc_t	sthread_t::read(int fd, void* buf, int n)
{
	fd -= fd_base;
	if (fd < 0 || fd >= (int)open_max || !_disks[fd]) 
		return RC(stBADFD);

	IOmonitor monitor(SthreadStats.read);

	int	done = 0;
	w_rc_t	e;

	e = _disks[fd]->read(buf, n, done);
	if (e == RCOK && done != n)
		e = RC(stSHORTIO);

	return e;
}


w_rc_t	sthread_t::write(int fd, const void* buf, int n)
{
	fd -= fd_base;
	if (fd < 0 || fd >= (int)open_max || !_disks[fd]) 
		return RC(stBADFD);

	IOmonitor	monitor(SthreadStats.write);

	int	done = 0;
	w_rc_t	e;

	e = _disks[fd]->write(buf, n, done);
	if (e == RCOK && done != n)
		e = RC(stSHORTIO);

	return e;
}


w_rc_t	sthread_t::readv(int fd, const iovec_t *iov, size_t iovcnt)
{
	fd -= fd_base;
	if (fd < 0 || fd >= (int)open_max || !_disks[fd]) 
		return RC(stBADFD);

	IOmonitor	monitor(SthreadStats.read);

	int	done = 0;
	int	total = 0;
	w_rc_t	e;

	total = sdisk_t::vsize(iov, iovcnt);

	e = _disks[fd]->readv(iov, iovcnt, done);
	if (e == RCOK && done != total)
		e = RC(stSHORTIO);

	return e;
}


w_rc_t	sthread_t::writev(int fd, const iovec_t *iov, size_t iovcnt)
{
	fd -= fd_base;
	if (fd < 0 || fd >= (int)open_max || !_disks[fd]) 
		return RC(stBADFD);

	IOmonitor	monitor(SthreadStats.write);

	int	done = 0;
	int	total = 0;
	w_rc_t	e;

	total = sdisk_t::vsize(iov, iovcnt);

	e = _disks[fd]->writev(iov, iovcnt, done);
	if (e == RCOK && done != total)
		e = RC(stSHORTIO);

	return e;
}


w_rc_t	sthread_t::pread(int fd, void *buf, int n, fileoff_t pos)
{
	fd -= fd_base;
	if (fd < 0 || fd >= (int)open_max || !_disks[fd]) 
		return RC(stBADFD);

	IOmonitor	monitor(SthreadStats.read);

	int	done = 0;
	w_rc_t	e;

	e = _disks[fd]->pread(buf, n, pos, done);
	if (e == RCOK && done != n)
		e = RC(stSHORTIO);

	return e;
}


w_rc_t	sthread_t::pwrite(int fd, const void *buf, int n, fileoff_t pos)
{
	fd -= fd_base;
	if (fd < 0 || fd >= (int)open_max || !_disks[fd]) 
		return RC(stBADFD);

	IOmonitor	monitor(SthreadStats.write);

	int	done = 0;
	w_rc_t	e;

	e = _disks[fd]->pwrite(buf, n, pos, done);
	if (e == RCOK && done != n)
		e = RC(stSHORTIO);

	return e;
}


w_rc_t	sthread_t::fsync(int fd)
{
	fd -= fd_base;
	if (fd < 0 || fd >= (int)open_max || !_disks[fd]) 
		return RC(stBADFD);

	w_rc_t		e;
	IOmonitor	monitor(SthreadStats.sync);

	e = _disks[fd]->sync();

	return e;
}

w_rc_t	sthread_t::ftruncate(int fd, fileoff_t n)
{
	fd -= fd_base;
	if (fd < 0 || fd >= (int)open_max || !_disks[fd]) 
		return RC(stBADFD);

	w_rc_t		e;
	IOmonitor	monitor(SthreadStats.truncate);

	e =  _disks[fd]->truncate(n);

	return e;
}


w_rc_t sthread_t::lseek(int fd, fileoff_t pos, int whence, fileoff_t& ret)
{
	fd -= fd_base;
	if (fd < 0 || fd >= (int)open_max || !_disks[fd]) 
		return RC(stBADFD);

	w_rc_t	e;

	e = _disks[fd]->seek(pos, whence, ret);

	return e;
}


w_rc_t sthread_t::lseek(int fd, fileoff_t offset, int whence)
{
	fileoff_t	dest;
	w_rc_t		e;

	e = sthread_t::lseek(fd, offset, whence, dest);
	if (e == RCOK && whence == SEEK_AT_SET && dest != offset)
		e = RC(stSHORTSEEK);

	return e;
}


w_rc_t	sthread_t::fstat(int fd, filestat_t &st)
{
	fd -= fd_base;
	if (fd < 0 || fd >= (int)open_max || !_disks[fd]) 
		return RC(stBADFD);

	w_rc_t	e;

	e = _disks[fd]->stat(st);

	return e;
}


w_rc_t	sthread_t::fgetgeometry(int fd, disk_geometry_t &dg)
{
	fd -= fd_base;
	if (fd < 0 || fd >= (int)open_max || !_disks[fd]) 
		return RC(stBADFD);

	w_rc_t	e;

	e = _disks[fd]->getGeometry(dg);

	return e;
}


w_rc_t	sthread_t::fisraw(int fd, bool &isRaw)
{
	filestat_t	st;

	isRaw = false;		/* default value */

	W_DO(fstat(fd, st));	/* takes care of errors */

	isRaw = st.is_device && st.is_raw_device;
	return RCOK;
}


void	sthread_t::dump_io(ostream &s)
{
	s << "I/O:";
	s << " open_max=" << int(open_max);
	s << " open_count=" << open_count;
	s << " in_progress=" << _io_in_progress;
	s << " diskrw=" << (_diskrw ? _diskrw : "<none>");
	s << endl;
#ifdef STHREAD_IO_DISKRW
	if (sdisk_handler)
		sdisk_handler->print(s);
	else
		s << "Disk R/W not enabled";
#endif
}


void	sthread_t::dump_stats_io(ostream &)
{
	/* The shmc stats are sort of lame at this point */
#if 0 && !defined(EXCLUDE_SHMCSTATS)
	s << ShmcStats << endl;
#endif
}


void	sthread_t::reset_stats_io()
{
#if 0
	ShmcStats.clear();
#endif
}


void	sthread_t::set_diskrw_name(const char *name)
{
	_diskrw = name;
#ifdef STHREAD_IO_DISKRW
	if (sdisk_handler)
		sdisk_handler->set_diskrw_name(name);
#endif
}


extern "C" void dump_diskrw()
{
	sthread_t::dump_io(cout);
	cout << flush;
}

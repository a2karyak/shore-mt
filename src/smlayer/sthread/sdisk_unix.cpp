/*<std-header orig-src='shore'>

 $Id: sdisk_unix.cpp,v 1.12 1999/06/07 19:06:04 kupsch Exp $

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

#include <w.h>
#include <sthread.h>
#include <sdisk.h>
#include <sdisk_unix.h>

#ifdef EXPENSIVE_STATS
#include <stime.h>
#endif

#include <w_statistics.h>
#include <sthread_stats.h>
extern class sthread_stats SthreadStats;

#ifdef _WIN32
#include <io.h>
#else
#include <unistd.h>
#endif
#include <fcntl.h>
#include <errno.h>
#include <sys/stat.h>

#ifndef _WIN32
#include <sys/uio.h>
#endif

#if !defined(AIX41) && !defined(SOLARIS2) && !defined(_WIN32)
extern "C" {
	extern int writev(int, const struct iovec *, int);
	extern int readv(int, const struct iovec *, int);
}
#endif

#include <os_interface.h>


const int stBADFD = sthread_base_t::stBADFD;
const int stINVAL = sthread_base_t::stINVAL;


int	sdisk_unix_t::convert_flags(int sflags)
{
	int	flags = 0;

	/* 1 of n */
	switch (modeBits(sflags)) {
	case OPEN_RDWR:
		flags |= O_RDWR;
		break;
	case OPEN_WRONLY:
		flags |= O_WRONLY;
		break;
	case OPEN_RDONLY:
		flags |= O_RDONLY;
		break;
	}

	/* m of n */
	/* could make a data driven flag conversion, :-) */
	if (hasOption(sflags, OPEN_CREATE))
		flags |= O_CREAT;
	if (hasOption(sflags, OPEN_TRUNC))
		flags |= O_TRUNC;
	if (hasOption(sflags, OPEN_EXCL))
		flags |= O_EXCL;
#ifdef O_SYNC
	if (hasOption(sflags, OPEN_SYNC))
		flags |= O_SYNC;
#endif
	if (hasOption(sflags, OPEN_APPEND))
		flags |= O_APPEND;

#if defined(O_BINARY)
	/* NewThreads I/O, just like Unix, I/O, is always byte-for-byte */
	flags |= O_BINARY;
#endif

	return flags;
}


sdisk_unix_t::~sdisk_unix_t()
{
	if (_fd != FD_NONE)
		W_COERCE(close());
}


w_rc_t	sdisk_unix_t::make(const char *name, int flags, int mode,
			   sdisk_t *&disk)
{
	sdisk_unix_t	*ud;
	w_rc_t		e;

	disk = 0;	/* default value*/
	
	ud = new sdisk_unix_t;
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


w_rc_t	sdisk_unix_t::open(const char *name, int flags, int mode)
{
	if (_fd != FD_NONE)
		return RC(stBADFD);	/* XXX in use */

	_fd = ::os_open(name, convert_flags(flags), mode);
	if (_fd == -1)
		return RC(fcOS);

	return RCOK;
}

w_rc_t	sdisk_unix_t::close()
{
	if (_fd == FD_NONE)
		return RC(stBADFD);	/* XXX closed */

	int	n;

	n = ::os_close(_fd);
	if (n == -1)
		return RC(fcOS);

	_fd = FD_NONE;
	return RCOK;
}


/* An I/O monitor for local I/O which will time and yield as needed */
class IOmonitor_local {
#ifdef EXPENSIVE_STATS
	stime_t	start_time;
#else
	bool	unused;
#endif
public:
	inline IOmonitor_local(int &counter);
	inline ~IOmonitor_local();
};

inline	IOmonitor_local::IOmonitor_local(int &counter) :
#ifdef EXPENSIVE_STATS
	start_time(stime_t::now())
#else
	unused(false)
#endif
{
	counter++;
}

inline	IOmonitor_local::~IOmonitor_local()
{
#ifdef EXPENSIVE_STATS
	double	delta = stime_t::now() - start_time;

	SthreadStats.io_time += (float) delta;

	/* It's idle time because no threads can run */
	SthreadStats.idle_time += (float) delta;
#endif

	/* XXX this is disgusting.  The threads package should yield. */
	sthread_t::yield();
}


w_rc_t	sdisk_unix_t::read(void *buf, int count, int &done)
{
	if (_fd == FD_NONE)
		return RC(stBADFD);

	int	n;

	IOmonitor_local	monitor(SthreadStats.local_io);

	n = ::os_read(_fd, buf, count);
	if (n == -1)
		return RC(fcOS);

	done = n;

	return RCOK;
}

w_rc_t	sdisk_unix_t::write(const void *buf, int count, int &done)
{
	if (_fd == FD_NONE)
		return RC(stBADFD);

	int	n;

	IOmonitor_local	monitor(SthreadStats.local_io);

	n = ::os_write(_fd, buf, count);
	if (n == -1)
		return RC(fcOS);

	done = n;

	return RCOK;
}

#ifndef _WIN32
w_rc_t	sdisk_unix_t::readv(const iovec_t *iov, int iovcnt, int &done)
{
	if (_fd == FD_NONE)
		return RC(stBADFD);

	int	n;

	IOmonitor_local	monitor(SthreadStats.local_io);

	/* XXX if iovec_t doesn't match iovec, need to translate */
	n = ::os_readv(_fd, (const struct iovec *)iov, iovcnt);
	if (n == -1)
		return RC(fcOS);

	done = n;

	return RCOK;
}

w_rc_t	sdisk_unix_t::writev(const iovec_t *iov, int iovcnt, int &done)
{
	if (_fd == FD_NONE)
		return RC(stBADFD);

	int	n;

	IOmonitor_local	monitor(SthreadStats.local_io);

	/* XXX if iovec_t doesn't match iovec, need to translate */
	n = ::os_writev(_fd, (const struct iovec *)iov, iovcnt);
	if (n == -1)
		return RC(fcOS);

	done = n;

	return RCOK;
}
#endif

#ifdef SOLARIS2
w_rc_t	sdisk_unix_t::pread(void *buf, int count, fileoff_t pos, int &done)
{
	if (_fd == FD_NONE)
		return RC(stBADFD);

	int	n;

	IOmonitor_local	monitor(SthreadStats.local_io);

	n = ::os_pread(_fd, buf, count, pos);
	if (n == -1)
		return RC(fcOS);

	done = n;

	return RCOK;
}


w_rc_t	sdisk_unix_t::pwrite(const void *buf, int count, fileoff_t pos,
			    int &done)
{
	if (_fd == FD_NONE)
		return RC(stBADFD);

	int	n;

	IOmonitor_local	monitor(SthreadStats.local_io);

	n = ::os_pwrite(_fd, buf, count, pos);
	if (n == -1)
		return RC(fcOS);

	done = n;

	return RCOK;
}
#endif

w_rc_t	sdisk_unix_t::seek(fileoff_t pos, int origin, fileoff_t &newpos)
{
	if (_fd == FD_NONE)
		return RC(stBADFD);

	switch (origin) {
	case SEEK_AT_SET:
		origin = SEEK_SET;
		break;
	case SEEK_AT_CUR:
		origin = SEEK_CUR;
		break;
	case SEEK_AT_END:
		origin = SEEK_END;
		break;
	}

#ifdef notyet	/* XXX mismatched off_ts */
	/* Try to do something sane if the file positions requested
	   by the user go out of range of those that the system
	   can deal with, or vice-versa. */
	off_t	o;
	off_t	l;

	if (pos < int4_min || pos > int4_max)
		return RC(stBADADDR);	/* XXX offset out of range */
#else
	fileoff_t	l=0;
	l = ::os_lseek(_fd, pos, origin);
#endif
	if (l == -1)
		return RC(fcOS);

	newpos = l;

	return RCOK;
}

w_rc_t	sdisk_unix_t::truncate(fileoff_t size)
{
	if (_fd == FD_NONE)
		return RC(stBADFD);

	int	n;

	IOmonitor_local	monitor(SthreadStats.local_io);

#if defined(_WIN32) && defined(LARGEFILE_AWARE)
	if (size > w_base_t::int4_max) {
		cerr << "Warning: LARGEFILE truncate not supported: size="
			<< size << endl;
		return RC(stINVAL);
	}
#endif
	n = ::os_ftruncate(_fd, size);

	return (n == -1) ? RC(fcOS) : RCOK;
}

w_rc_t	sdisk_unix_t::sync()
{
	if (_fd == FD_NONE)
		return RC(stBADFD);

	int	n;

	IOmonitor_local	monitor(SthreadStats.local_io);

	n = os_fsync(_fd);

	/* fsync's to r/o files and devices can fail ok */
	if (n == -1 && (errno == EBADF || errno == EINVAL))
		n = 0;

	return (n == -1) ? RC(fcOS) : RCOK;
}


w_rc_t	sdisk_unix_t::stat(filestat_t &st)
{
	if (_fd == FD_NONE)
		return RC(stBADFD);

	os_stat_t	sys;
	int		n;

	n = os_fstat(_fd, &sys);
	if (n == -1)
		return RC(fcOS);

	st.st_size = sys.st_size;
#ifdef _WIN32
	st.st_block_size = 512;	/* XXX */
#else
	st.st_block_size = sys.st_blksize;
#endif

	st.st_device_id = sys.st_dev;
	st.st_file_id = sys.st_ino;

	int mode = (sys.st_mode & S_IFMT);
	st.is_file = (mode == S_IFREG);
	st.is_dir = (mode == S_IFDIR);
#ifdef S_IFBLK
	st.is_device = (mode == S_IFBLK);
#else
	st.is_device = false;
#endif
	st.is_device = st.is_device || (mode == S_IFCHR);
	st.is_raw_device = (mode == S_IFCHR);

#ifdef _WIN32
	/* Device and File information from Win32 POSix subsystem
	   is bogus.  If possible, use the native Win32 facilities to 
	   extract that info. */
	LONG	l;

	l = _get_osfhandle(_fd);
	if (l != -1) {
		BY_HANDLE_FILE_INFORMATION	info;
		bool	ok;

		ok = GetFileInformationByHandle((HANDLE) l, &info);
		if (ok) {
			st.st_device_id = info.dwVolumeSerialNumber;
#ifdef LARGEFILE_AWARE
			st.st_file_id = ((fileoff_t)info.nFileIndexHigh << 32)
				| info.nFileIndexLow;
#else
			st.st_file_id = info.nFileIndexLow;
#endif
		}
	}
#endif

	return RCOK;
}

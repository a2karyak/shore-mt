/*<std-header orig-src='shore' incl-file-exclusion='SDISK_H'>

 $Id: sdisk.h,v 1.22 2000/04/12 17:05:52 bolo Exp $

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

#ifndef SDISK_H
#define SDISK_H

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

#if !(defined(_WIN32) && defined(NEW_IO_ONLY))
/* XXX a hack to let diskrw work until it is finished */
#ifndef SDISK_DISKRW_HACK
#define	SDISK_DISKRW_HACK
#endif
#ifndef STHREAD_IO_DISKRW
#define STHREAD_IO_DISKRW
#endif
#ifndef STHREAD_IO_UNIX
#define	STHREAD_IO_UNIX
#endif
#endif

#if defined(_WIN32) && defined(NEW_IO) && !defined(STHREAD_IO_WIN32)
#define STHREAD_IO_WIN32
#endif

class sdisk_base_t {
public:

	struct iovec_t {
		void	*iov_base;
		int	iov_len;

		iovec_t(void *base = 0, int len = 0) 
			: iov_base(base), iov_len(len) { }
	};

	/* Don't use off_t, cause we may want the system off_t */
#if /*defined(_WIN32) &&*/ !defined(LARGEFILE_AWARE)
	/* XXX if !LARGEFILE, should choose on per operating system
	   to pick the native size. */
	typedef w_base_t::int4_t fileoff_t;
#else
	typedef w_base_t::int8_t fileoff_t;
#endif
	static	const	fileoff_t	fileoff_max;

	/* XXX actually disk geometry isn't needed ... just the
	   blocksize and size of a raw partition */
	struct disk_geometry_t {
		fileoff_t	blocks;		// # of blocks on device
		size_t	block_size;	// # bytes in each block
		
		/* this may be an approximation */
		size_t	cylinders;
		size_t	tracks;
		size_t	sectors;

		size_t	label_size;	// # bytes in disk label

		disk_geometry_t() : blocks(0), block_size(0),
			cylinders(0), tracks(0), sectors(0),
			label_size(0) { }
	};

	struct	filestat_t {
		fileoff_t	st_size;
		fileoff_t	st_file_id;
		unsigned	st_device_id;
		unsigned	st_block_size;
		bool		is_file;
		bool		is_dir;
		bool		is_device;
		bool		is_raw_device;

		filestat_t() : st_size(0),
			st_file_id(0), st_device_id(0), st_block_size(0),
			is_file(false), is_dir(false), 
			is_device(false), is_raw_device(false) { }
	};

	/* posix-compatabile file modes, namespace contortion */
	enum {
		/* open modes ... 1 of n */
		OPEN_RDONLY=0,
		OPEN_WRONLY=0x1,
		OPEN_RDWR=0x2,
		MODE_FLAGS=0x3,		// internal

		/* open options ... m of n */
		OPEN_TRUNC=0x10,
		OPEN_EXCL=0x20,
		OPEN_CREATE=0x40,
		OPEN_SYNC=0x80,
		OPEN_APPEND=0x100,
		OPEN_RAW=0x200,
#ifdef SDISK_DISKRW_HACK
		OPEN_KEEP_HACK = 0x01000000,	// a hack until diskrw is fixed
		OPTION_FLAGS   = 0x010003f0
#else
		OPTION_FLAGS=0x3f0	// internal
#endif
	};

	/* seek modes; contortions to avoid namespace problems */
	enum {
		SEEK_AT_SET=0,		// absolute
		SEEK_AT_CUR=1,		// from current position
		SEEK_AT_END=2		// from end-of-file 
	};

	/* utility functions */
	static	int	vsize(const iovec_t *iov, int iovcnt);
	static	int	vcoalesce(const iovec_t *iov, int iovcnt,
				  iovec_t *nv, int &newcnt);
};


/* sdisk is an interface class which isn't useful by itself */ 

class sdisk_t : public sdisk_base_t {
protected:
	sdisk_t() { }

	/* methods for lookint at open flags to extract I/O mode and options */
	static	int	modeBits(int mode);
	static	bool	hasMode(int mode, int wanted);
	static	bool	hasOption(int mode, int wanted);

public:
	virtual	~sdisk_t() { }

	virtual w_rc_t	open(const char *name, int flags, int mode) = 0;
	virtual	w_rc_t	close() = 0;

	virtual	w_rc_t	read(void *buf, int count, int &done) = 0;
	virtual	w_rc_t	write(const void *buf, int count, int &done) = 0;

	virtual	w_rc_t	readv(const iovec_t *iov, int ioc, int &done);
	virtual w_rc_t	writev(const iovec_t *iov, int ioc, int &done);

	virtual	w_rc_t	pread(void *buf, int count, fileoff_t pos, int &done);
	virtual	w_rc_t	pwrite(const void *buf, int count,
			       fileoff_t pos, int &done);

	virtual w_rc_t	seek(fileoff_t pos, int origin, fileoff_t &newpos) = 0;

	virtual w_rc_t	truncate(fileoff_t size) = 0;
	virtual w_rc_t	sync();

	virtual	w_rc_t	stat(filestat_t &stat);
	virtual	w_rc_t	getGeometry(disk_geometry_t &geom);
};

/* This remains for diskrw. */
extern w_rc_t sdisk_getgeometry(int fd, sdisk_t::disk_geometry_t &dg);

/*<std-footer incl-file-exclusion='SDISK_H'>  -- do not edit anything below this line -- */

#endif          /*</std-footer>*/

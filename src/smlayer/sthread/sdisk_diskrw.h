/*<std-header orig-src='shore' incl-file-exclusion='SDISK_DISKRW_H'>

 $Id: sdisk_diskrw.h,v 1.9 2000/02/22 21:25:15 bolo Exp $

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

#ifndef SDISK_DISKRW_H
#define SDISK_DISKRW_H

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

/* XXXX Yes, this horribly pollutes the namespace.  It will eventually
   be fixed better, and it is only visible to things that need
   to instantiated diskrw sdisk's, not generic sdisk code */

class sdisk_handler_t;

#if defined(_WIN32) && defined(NEW_IO)
class pipe_handler : public win32_event_t {
	sdisk_handler_t	&handler;

public:
	pipe_handler(HANDLE h, sdisk_handler_t &hr)
		: win32_event_t(h), handler(hr) { }
	~pipe_handler() { }

	void	ready();
};
#else
class pipe_handler : public sfile_hdl_base_t {
	sdisk_handler_t	&handler;

public:
	pipe_handler(int fd, sdisk_handler_t &h)
		: sfile_hdl_base_t(fd, rd), handler(h) { }
	~pipe_handler() { }

	void	read_ready();
	void	write_ready() { }
	void	exception_ready() { }
};
#endif


/* XXX maybe need a sdisk_base_t class */
class sdisk_handler_t {
    friend class sdisk_diskrw_t;

public:
	typedef		sdisk_t::fileoff_t fileoff_t;
	typedef		sdisk_t::iovec_t iovec_t;
	enum { iovec_max = max_diskv };

#ifdef SDISK_DISKRW_OPEN_MAX
	enum { default_open_max = SDISK_DISKRW_OPEN_MAX };
#else
	enum { default_open_max = 20 };
#endif

private:
	const char	*_diskrw;	// name of diskrw executable
	pid_t		_dummy_disk;

	w_shmem_t	shmem_seg;
	void		*_iomem;

	diskport_t	*diskport;
	unsigned	diskport_cnt;
	svcport_t 	*svcport;

	unsigned	open_max;

	smutex_t	forkdisk;

#if defined(_WIN32) && defined(NEW_IO)
	HANDLE		master_chan[2];
#else
	int		master_chan[2];
#endif

	pipe_handler	chan_event;


	/* Interfaces for sdisk_diskrw_t to utilize the shared segment. */
	w_rc_t		open(const char *path, int flags, int mode,
			     unsigned &slot);

	w_rc_t		close(unsigned slot);

	w_rc_t		io(unsigned slot, int op,
			   const iovec_t *iov, int iovcnt,
			   fileoff_t other=0);

	w_rc_t		seek(unsigned slot, fileoff_t offset,
			     int whence, fileoff_t &newpos);

	/* map a our-memory iovec_t into a shared-memory diskv_t */
	w_rc_t		map_iovec(const iovec_t *iov, int cnt,
				  diskv_t *diskv, int &total);

	w_rc_t		init_shared(unsigned size);
	w_rc_t		finish_shared();

	sdisk_handler_t(const char *diskrw_name = "diskrw");

public:
	static w_rc_t	make(unsigned size, const char *diskrw,
			     sdisk_handler_t *&hp);
	~sdisk_handler_t();

	w_rc_t		shutdown();

	void		*ioBuffer();

	void		set_diskrw_name(const char* diskrw);

	void		polldisk();
	void		polldisk_check();
	bool		pending();
	void		ensure_sleep();

	/* XXX not const for now */
	ostream		&print(ostream &);
};

#ifdef SDISK_DISKRW_HACK
#include "sdisk_unix.h"
#endif

class sdisk_diskrw_t : public
#ifdef SDISK_DISKRW_HACK
sdisk_unix_t
#else
sdisk_t 
#endif
{
	sdisk_handler_t	&handler;
	int		slot;	// channel # of diskrw, -1 for closed

	sdisk_diskrw_t(sdisk_handler_t &m);

	w_rc_t		io(int op,
			   const iovec_t *, int cnt,
			   fileoff_t other=0);

public:
	static	w_rc_t	make(sdisk_handler_t &master,
			     const char *name,
			     int flags, int mode,
			     sdisk_t *&disk);
	~sdisk_diskrw_t();

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


/*<std-footer incl-file-exclusion='SDISK_DISKRW_H'>  -- do not edit anything below this line -- */

#endif          /*</std-footer>*/

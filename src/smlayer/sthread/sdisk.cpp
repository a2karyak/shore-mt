/*<std-header orig-src='shore'>

 $Id: sdisk.cpp,v 1.8 2000/04/12 17:05:54 bolo Exp $

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
#include <sdisk.h>

#include <sthread.h>	/* XXX for error codes */


#if defined(_WIN32)
const sdisk_base_t::fileoff_t	sdisk_base_t::fileoff_max = w_base_t::int4_max;
#else
const sdisk_base_t::fileoff_t	sdisk_base_t::fileoff_max = w_base_t::int8_max;
#endif

const	int	stSHORTSEEK = sthread_base_t::stSHORTSEEK;


int	sdisk_base_t::vsize(const iovec_t *iov, int iovcnt)
{
	int	total = 0;

	for (int i = 0; i < iovcnt; i++)
		total += iov[i].iov_len;

	return total;
}

// XXX Assumes newVec is iovcnt long, since it might not compress
int	sdisk_base_t::vcoalesce(const iovec_t *iov, int iovcnt,
				iovec_t *newVec, int &newcnt)
{
	char		*tail;
	const iovec_t	*lastVec = iov + iovcnt;
	iovec_t		*newFirst = newVec;
	int		totalSize = 0;

	/* prime it with the first chunk */
	*newVec = *iov++;
	tail = (char *) newVec->iov_base  + newVec->iov_len;
	totalSize += newVec->iov_len;

	for (; iov < lastVec; iov++) {
		if (tail == (char *)iov->iov_base) {
			newVec->iov_len += iov->iov_len;
			tail += iov->iov_len;
		}
		else {
			*++newVec = *iov;
			tail = (char *) newVec->iov_base + newVec->iov_len;
		}
		totalSize += iov->iov_len;
	}
	newVec++;

#if 0
	int	newCount = newVec - newFirst;
	if (newCount < iovcnt)
		cout << "Coalesce " << iovcnt << " to " << newCount << endl;
#endif

	/* If it can't be coalesced, who cares, else the count */
	newcnt = (newVec - newFirst == iovcnt) ? 0 : newVec - newFirst;

	return totalSize;
}

int	sdisk_t::modeBits(int mode)
{
	return mode & MODE_FLAGS;
}


/* open mode is a 1-of-n choice */
bool	sdisk_t::hasMode(int mode, int wanted)
{
	mode = modeBits(mode);
	return mode == wanted;
}

/* options are one-of-many; true if all wanted are found */
bool	sdisk_t::hasOption(int mode, int wanted)
{
	mode &= OPTION_FLAGS;
	return (mode & wanted) == wanted;
}


/* Emulate vector I/O operations on thos boxes which don't
   support it. Either an error or a short transfer will
   abort the I/O and return the transfer count. */

w_rc_t	sdisk_t::readv(const iovec_t *iov, int iovcnt, int &transfered)
{
	int	total = 0;
	int	done = 0;
	int	n;
	int	i;
	w_rc_t	e;

	total = vsize(iov, iovcnt);

	for (i = 0; i < iovcnt; i++) {
		e = read(iov[i].iov_base, iov[i].iov_len, n);
		if (e)
			break;
		done += n;
		if (n != iov[i].iov_len)
			break;
	}

	transfered = done;

	return e;
}


w_rc_t	sdisk_t::writev(const iovec_t *iov, int iovcnt, int &transfered)
{
	int	total = 0;
	int	done = 0;
	int	n;
	int	i;
	w_rc_t	e;

	total = vsize(iov, iovcnt);

	for (i = 0; i < iovcnt; i++) {
		e = write(iov[i].iov_base, iov[i].iov_len, n);
		if (e)
			break;
		done += n;
		if (n != iov[i].iov_len)
			break;
	}

	transfered = done;

	return e;
}

/* Emulate positioned I/O operations on thos boxes which don't
   support it.  It isn't "race safe", that is a difficult problem
   to deal with.  Pread/Pwrite don't move the file pointer, but
   that is difficult to do if the OS doens't support them.  We 
   seek to where the I/O is supposed to be, execute it, and
   then restore the old position.  */

w_rc_t	sdisk_t::pread(void *buf, int size, fileoff_t pos,
		       int &transfered)
{
	fileoff_t	was;
	fileoff_t	newpos;
	int		n;
	w_rc_t		e;
	w_rc_t		es;

	W_DO(seek(0, SEEK_AT_CUR, was));
	/* Only move if it needs to */
	if (pos != was) {
		W_DO(seek(pos, SEEK_AT_SET, newpos));
		if (newpos != pos) {
			es = seek(was, SEEK_AT_SET, newpos);
			if (es != RCOK)
				cerr << "Warning: pread reposition failed!"
					<< endl << e << endl;
			return RC(stSHORTSEEK);
		}
	}

	e = read(buf, size, n);

	es = seek(was, SEEK_AT_SET, newpos);
	/* XXX should a reposition error make the I/O fail? */
	if (es != RCOK || newpos != was)
		cerr << "Warning: pread reposition failed!"
			<< endl << e << endl;

	transfered = n;

	return e;
}

w_rc_t	sdisk_t::pwrite(const void *buf, int size, fileoff_t pos,
		       int &transfered)
{
	fileoff_t	was;
	fileoff_t	newpos;
	int		n;
	w_rc_t		e;
	w_rc_t		es;

	W_DO(seek(0, SEEK_AT_CUR, was));
	/* Only move if it needs to */
	if (pos != was) {
		W_DO(seek(pos, SEEK_AT_SET, newpos));
		if (newpos != pos) {
			es = seek(was, SEEK_AT_SET, newpos);
			if (es != RCOK)
				cerr << "Warning: pwrite reposition failed!"
					<< endl << e << endl;
			return RC(stSHORTSEEK);
		}
	}

	e = write(buf, size, n);

	es = seek(was, SEEK_AT_SET, newpos);
	/* XXX should a reposition error make the I/O fail? */
	if (es != RCOK || newpos != was)
		cerr << "Warning: pwrite reposition failed!"
			<< endl << e << endl;

	transfered = n;

	return e;
}


/* a no-op file-sync if the underlying implementation doesn't support it. */
w_rc_t	sdisk_t::sync()
{
	return RCOK;
}


w_rc_t	sdisk_t::stat(filestat_t &)
{
	return RC(fcNOTIMPLEMENTED);
}


w_rc_t	sdisk_t::getGeometry(disk_geometry_t &)
{
	return RC(fcNOTIMPLEMENTED);
}

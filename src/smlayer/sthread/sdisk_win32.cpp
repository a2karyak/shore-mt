/*<std-header orig-src='shore'>

 $Id: sdisk_win32.cpp,v 1.8 2001/06/09 17:29:58 bolo Exp $

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
#include <sthread.h>

#include <w_statistics.h>
#include <sthread_stats.h>
extern class sthread_stats SthreadStats;

#include <windows.h>
#include <sdisk_win32.h>

#include <winioctl.h>


const	int	stINVAL = sthread_base_t::stINVAL;
const	int	stBADFD = sthread_base_t::stBADFD;

/* Return value of SetFilePointer upon failure. */
#define	SETFP_FAILED	(-1)
#ifndef NULL_HANDLE_VALUE
#define	NULL_HANDLE_VALUE	(0)
#endif


/* map flags into an NT access mode */
DWORD	sdisk_win32_t::convert_access(int flags)
{
	DWORD	access_mode = 0;

	switch (modeBits(flags)) {
	case OPEN_RDWR:
		access_mode = GENERIC_READ | GENERIC_WRITE;
		break;
	case OPEN_WRONLY:
		access_mode = GENERIC_WRITE;
		break;
	case OPEN_RDONLY:
		access_mode = GENERIC_READ;
		break;
	}

	return access_mode;
}


DWORD	sdisk_win32_t::convert_share(int W_UNUSED(flags))
{
	DWORD	share_mode = 0;

	/* chose the most lenient sharing mode for now */
	share_mode = FILE_SHARE_WRITE | FILE_SHARE_READ;

	return share_mode;
}


/* map flags into an NT "how to open the file" mode */
DWORD	sdisk_win32_t::convert_create(int flags)
{
	DWORD	create_mode = 0;

	if (hasOption(flags, OPEN_CREATE|OPEN_EXCL))
		create_mode = CREATE_NEW;
	else if (hasOption(flags, OPEN_CREATE|OPEN_TRUNC))
		create_mode = CREATE_ALWAYS;
	else if (hasOption(flags, OPEN_CREATE))
		create_mode = OPEN_ALWAYS;
	else if (hasOption(flags, OPEN_TRUNC))
		create_mode = TRUNCATE_EXISTING;
	else
		create_mode = OPEN_EXISTING;

	return create_mode;
}


DWORD	sdisk_win32_t::convert_attributes(int flags)
{
	DWORD	attributes = FILE_ATTRIBUTE_NORMAL;

	/* file attributes XXX do better */
	if (hasOption(flags, OPEN_RAW))
		attributes |= FILE_FLAG_NO_BUFFERING;
	if (hasOption(flags, OPEN_SYNC))
		attributes |= FILE_FLAG_WRITE_THROUGH;

	/* XXX FILE_FLAG_NO_BUFFERING for devices ? */

#if 0
	/* Act as much like unix as possible */
	attributes |= FILE_FLAG_POSIX_SEMANTICS;
#endif

	return attributes;
}


/* XXX need to update sthread idle stats IFF no per-thread I/O */

sdisk_win32_t::~sdisk_win32_t()
{
	if (_handle != INVALID_HANDLE_VALUE)
		W_COERCE(close());
}


w_rc_t	sdisk_win32_t::make(const char *name, int flags, int mode,
			   sdisk_t *&disk)
{
	sdisk_win32_t	*ud;
	w_rc_t		e;

	disk = 0;	/* default value*/

	ud = new sdisk_win32_t;
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

ostream &operator<<(ostream &o, const LARGE_INTEGER &li)
{
#if 1
	w_base_t::int8_t	*x = (w_base_t::int8_t *) &li;
	return o << *x;
#else
	return o << '(' << li.HighPart << '.' << li.LowPart << ')';
#endif
}

/* A hack for an ostream operator */
struct FILESYS_GEOMETRY {
	DWORD	SectorsPerCluster;
	DWORD	BytesPerSector;
	DWORD	FreeClusters;
	DWORD	Clusters;
};

ostream &operator<<(ostream &o, const FILESYS_GEOMETRY &dg)
{
	o << "\tSectors Per Cluster: " << dg.SectorsPerCluster << endl;
	o << "\tBytes Per Sector: " << dg.BytesPerSector << endl;
	o << "\tFreeClusters: " << dg.FreeClusters << endl;
	o << "\tClusters: " << dg.Clusters << endl;
	return o;
}

ostream &operator<<(ostream &o, const DISK_GEOMETRY &dg)
{
	o << "DISK_GEOMETRY:" << endl;
	o << "\tCylinders: " << dg.Cylinders << endl;
	o << "\tMediaType: " << dg.MediaType << endl;
	o << "\tTracksPerCylinder: " << dg.TracksPerCylinder << endl;
	o << "\tSectorsPerTrack: " << dg.SectorsPerTrack << endl;
	o << "\tBytesPerSector: " << dg.BytesPerSector << endl;

	return o;
}


ostream &operator<<(ostream &o, const PARTITION_INFORMATION &pi)
{
	o << "PARTITION_INFORMATION:" << endl;
	o << "\tStartingOffset: " << pi.StartingOffset << endl;
	o << "\tPartitionLength: " << pi.PartitionLength << endl;
	o << "\tHiddenSectors: " << pi.HiddenSectors << endl;
	o << "\tPartitionNumber: " << pi.PartitionNumber << endl;
	o << "\tPartitionType: " << (int)pi.PartitionType << endl;
	o << "\tBootIndicator: " << (int)pi.BootIndicator << endl;
	o << "\tRecognizedPartition: " << (int)pi.RecognizedPartition << endl;
	o << "\tRewritePartition: " << (int)pi.RewritePartition << endl;
	return o;
}



w_rc_t	sdisk_win32_t::_open(const char *name,
			     DWORD access_mode, DWORD share_mode,
			     DWORD create_mode, DWORD attributes)
{
	if (_handle != INVALID_HANDLE_VALUE)
		return RC(stBADFD);	/* XXX already open */

	SECURITY_ATTRIBUTES	sa;

	/* create a security attribute that inherits whatever is in use */
	sa.nLength = sizeof(sa);
	sa.lpSecurityDescriptor = 0;
	sa.bInheritHandle = TRUE;

	char	device_name[20];
	char	drive_letter = '\0';
	if (strlen(name) == 2 && name[1] == ':' && isalpha(name[0])) {
		/* Munge the path for a partition open */
		ostrstream	s(device_name, sizeof(device_name));

		drive_letter = tolower(name[0]);

		s << '\\' << '\\' << '.' << '\\' << drive_letter << ':' << ends;

		create_mode = OPEN_EXISTING;

		name = device_name;

		_isDevice = true;
	}


	HANDLE f;
	f = CreateFile(name, access_mode, share_mode, &sa, create_mode,
		       attributes, NULL_HANDLE_VALUE);
	if (f == INVALID_HANDLE_VALUE)
		return RC(fcWIN32);

	_handle = f;

	if (!_isDevice) {
		char	root_buf[4];
		char	*root;

		if (strlen(name) > 2 && name[1] == ':' && isalpha(name[0])) {
			root_buf[0] = name[0];
			root_buf[1] = ':';
			root_buf[2] = '\\';
			root_buf[3] = '\0';
			root = root_buf;
		}
		else
			root = 0;

		FILESYS_GEOMETRY	dg;
		bool	ok;

		ok = GetDiskFreeSpace(root,
			&dg.SectorsPerCluster,
			&dg.BytesPerSector,
			&dg.FreeClusters,
			&dg.Clusters);

		/* Default values for filestat_t fields that aren't
		   retrieved by normal means. */
		if (ok) {
			_default_stat.st_block_size = dg.BytesPerSector;
		}
		else {
			_default_stat.st_block_size = 512;	/* XXX */
#ifdef W_DEBUG
			w_rc_t	e = RC(fcWIN32);
			cerr << "GetDiskFreeSpace(" << (root ? root : "")
				<< "):" << endl << e << endl;
#endif
		}
	}
	else {
		/* Provide a goofy ID of the ascii char of the drive letter */
		_default_stat.st_device_id = drive_letter;
	}

	return RCOK;
}


w_rc_t	sdisk_win32_t::open(const char *name, int flags, int W_UNUSED(mode))
{
	DWORD	access_mode;
	DWORD	share_mode;
	DWORD	create_mode;
	DWORD	attributes;

	access_mode = convert_access(flags);

	share_mode = convert_share(flags);

	create_mode = convert_create(flags);

	attributes = convert_attributes(flags);

	return _open(name, access_mode, share_mode, create_mode, attributes);
}


w_rc_t	sdisk_win32_t::close()
{
	if (_handle == INVALID_HANDLE_VALUE)
		return	RC(stBADFD);	/* already closed */

	bool	ok;

	ok = CloseHandle(_handle);
	if (!ok)
		return RC(fcWIN32);
	_handle = INVALID_HANDLE_VALUE;
	return RCOK;
}


/* XXX reuse local IO monitor! */
class IOmonitor_win32 {
#ifdef	EXPENSIVE_STATS
	stime_t		start_time;
#else
	bool		unused;
#endif

public:

	inline IOmonitor_win32(int &counter);
	inline ~IOmonitor_win32();
};


inline	IOmonitor_win32::IOmonitor_win32(int &counter)
#ifdef EXPENSIVE_STATS
: start_time(stime_t::now())
#else
: unused(false)
#endif
{
	counter++;
}


inline	IOmonitor_win32::~IOmonitor_win32()
{
#ifdef EXPENSIVE_STATS
	double	delta = stime_t::now() - start_time;

	SthreadStats.io_time += (float) delta;

	SthreadStats.idle_time += (float) delta;
#endif
	sthread_t::yield();
}



w_rc_t	sdisk_win32_t::read(void *buf, int count, int &done)
{
	if (_handle == INVALID_HANDLE_VALUE)
		return RC(stBADFD);	/* XXX not open */

	bool	ok;
	DWORD	n;

	IOmonitor_win32	monitor(SthreadStats.local_io);

	ok = ReadFile(_handle, buf, count, &n, 0);
	if (!ok)
		return RC(fcWIN32);

	done = n;

	return RCOK;
}


w_rc_t	sdisk_win32_t::write(const void *buf, int count, int &done)
{
	if (_handle == INVALID_HANDLE_VALUE)
		return RC(stBADFD);	/* XXX not open */

	bool	ok;
	DWORD	n;

	IOmonitor_win32	monitor(SthreadStats.local_io);

	ok = WriteFile(_handle, buf, count, &n, 0);
	if (!ok)
		return RC(fcWIN32);

	done = n;

	return RCOK;
}


w_rc_t	sdisk_win32_t::pread(void *buf, int count, fileoff_t pos, int &done)
{
	if (_handle == INVALID_HANDLE_VALUE)
		return RC(stBADFD);	/* XXX not open */

	if (pos < 0)
		return RC(stINVAL);

	bool	ok;
	DWORD	n;
	OVERLAPPED	ov;

	getOffset(ov, pos);
	ov.hEvent = 0;

	IOmonitor_win32	monitor(SthreadStats.local_io);

	ok = ReadFile(_handle, buf, count, &n, &ov);
	if (!ok)
		return RC(fcWIN32);

	done = n;

	return RCOK;
}


w_rc_t	sdisk_win32_t::pwrite(const void *buf, int count, 
			      fileoff_t pos, int &done)
{
	if (_handle == INVALID_HANDLE_VALUE)
		return RC(stBADFD);	/* XXX not open */

	if (pos < 0)
		return RC(stINVAL);

	bool	ok;
	DWORD	n;
	OVERLAPPED	ov;

	getOffset(ov, pos);
	ov.hEvent = 0;

	IOmonitor_win32	monitor(SthreadStats.local_io);

	ok = WriteFile(_handle, buf, count, &n, &ov);
	if (!ok)
		return RC(fcWIN32);

	done = n;

	return RCOK;
}



w_rc_t	sdisk_win32_t::seek(fileoff_t pos, int origin, fileoff_t &newpos)
{
	if (_handle == INVALID_HANDLE_VALUE)
		return RC(stBADFD);	/* XXX not open */

	int		method; 

	switch (origin) {
	case SEEK_AT_SET:
		method = FILE_BEGIN;
		if (pos < 0)
			return RC(stINVAL);
		break;
	case SEEK_AT_CUR:
		method = FILE_CURRENT;
		break;
	case SEEK_AT_END:
		method = FILE_END;
		break;
	default:
		/* XXX return invalid seek mode */
		return RC(stINVAL);
		break;
	}

	LONG	to[2];
	DWORD	n;
	getOffset(to, pos);

#ifdef LARGEFILE_AWARE
	DWORD	err;
	n = SetFilePointer(_handle, to[0], to+1, method);
	if (n == SETFP_FAILED && (err = GetLastError()) != NO_ERROR)
		return RC2(fcWIN32, err);
#else
	n = SetFilePointer(_handle, to[0], 0, method);
	if (n == SETFP_FAILED)
		return RC(fcWIN32);
	to[1] = 0;
#endif

	to[0] = n;
	setOffset(newpos, to);

	return RCOK;
}


/* XXX improve seek pointer error handling */
w_rc_t	sdisk_win32_t::truncate(fileoff_t size)
{
	if (_handle == INVALID_HANDLE_VALUE)
		return RC(stBADFD);	/* XXX not open */

	if (size < 0)
		return RC(stINVAL);

	IOmonitor_win32	monitor(SthreadStats.local_io);

	DWORD	n;
	LONG	there[2];

	getOffset(there, size);

#ifdef LARGEFILE_AWARE
	DWORD	err;
	LONG	here[2];
	here[0] = here[1] = 0;


	/* Where are we? */
	n = SetFilePointer(_handle, here[0], here+1, FILE_CURRENT);
	if (n == SETFP_FAILED && ((err = GetLastError()) != NO_ERROR))
		return RC2(fcWIN32, err);
	here[0] = n;

	n = SetFilePointer(_handle, there[0], there+1, FILE_BEGIN);
	if (n == SETFP_FAILED && ((err = GetLastError()) != NO_ERROR))
		return RC2(fcWIN32, err);
	there[0] = n;

	bool	ok;
	ok = SetEndOfFile(_handle);
	if (!ok)	/* XXX re-seek */
		return RC(fcWIN32);

	/* reset the seek pointer to keep it valid; if it is
	   no longer valid, it is already at the new eof */

	fileoff_t	_here;
	setOffset(_here, here);
	if (_here < size) {
		n = SetFilePointer(_handle, here[0], here+1, FILE_BEGIN);
		if (n == SETFP_FAILED && (err = GetLastError()) != NO_ERROR)
			return RC2(fcWIN32, err);
	}
#else
	n = SetFilePointer(_handle, 0, 0, FILE_CURRENT);
	if (n == SETFP_FAILED)
		return RC(fcWIN32);
	LONG	here = n;

	n = SetFilePointer(_handle, there[0], 0, FILE_BEGIN);
	if (n == SETFP_FAILED)
		return RC(fcWIN32);

	bool	ok;
	ok = SetEndOfFile(_handle);
	if (!ok)	/* XXX re-seek */
		return RC(fcWIN32);

	if (here < size) {
		n = SetFilePointer(_handle, here, 0, FILE_BEGIN);
		if (n == SETFP_FAILED)
			return RC(fcWIN32);
	}
#endif

	return RCOK;
}

/* XXX sync not supported except for a file mode.
   Could have a sync which returns an error if the file
   wasn't opened for file synchronization */

w_rc_t	sdisk_win32_t::sync()
{
	if (_handle == INVALID_HANDLE_VALUE)
		return RC(stBADFD);

	IOmonitor_win32	monitor(SthreadStats.local_io);

	/* XXX track mode, don't bother on R/O files */

	bool	ok;
	ok = FlushFileBuffers(_handle);

	if (!ok) {
		/* Is it a read-only file, or a device? */
		DWORD	n = GetLastError();
		if (n != ERROR_ACCESS_DENIED && n != ERROR_INVALID_FUNCTION)
			return RC2(fcWIN32, n);
	}

	return RCOK;


}


w_rc_t	sdisk_win32_t::stat(filestat_t &st)
{
	if (_handle == INVALID_HANDLE_VALUE)
		return RC(stBADFD);

	/* Can't stat a device, build it by hand */
	if (_isDevice) {
		st.st_size = 0;
		st.st_block_size = 0;	/* XXX check unix */
		st.st_device_id = _default_stat.st_device_id;
		st.st_file_id = 0;	/* XXX */
		st.is_file = false;
		st.is_dir = false;
		st.is_device = true;
		st.is_raw_device = true;

		return RCOK;
	}

	bool	ok;

	BY_HANDLE_FILE_INFORMATION	info;
	ok = GetFileInformationByHandle(_handle, &info);
	if (!ok)
		return RC(fcWIN32);

	DWORD	type;
	type = GetFileType(_handle);

	setOffset(st.st_size, info.nFileSizeHigh, info.nFileSizeLow);

	st.st_block_size = _default_stat.st_block_size;

	/* XXX dependent upon file_id being a fileoff_t */
	setOffset(st.st_file_id, info.nFileIndexHigh, info.nFileIndexLow);

	st.st_device_id = info.dwVolumeSerialNumber;
	st.is_file = (type == FILE_TYPE_DISK);
	st.is_dir = (info.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY);
	st.is_device = false;		/* XXX */
	st.is_raw_device = false;	/* XXX */

	return RCOK;
}


w_rc_t	sdisk_win32_t::getGeometry(disk_geometry_t &_dg)
{
	if (_handle == INVALID_HANDLE_VALUE)
		return RC(stBADFD);

	if (!_isDevice)
		return RC(stINVAL);

	DWORD		n;
	bool		ok;

	DISK_GEOMETRY	dg;

	ok = DeviceIoControl(_handle,
		IOCTL_DISK_GET_DRIVE_GEOMETRY,
		0, 0,
		&dg, sizeof(dg),
		&n,
		0);
	if (!ok)
		return RC(fcWIN32);

	/* Only partitions supported for now, so this isn't bad */

	/* XXX This is a hack for NT 4.0 wanting to write 
	   more bytes than a PARTITION_INFORMATION contains! */
#define	PI_HACK
#ifdef PI_HACK
	struct	__PI__ {
		PARTITION_INFORMATION	_pi;
		DWORD			_mystery;
	} _pi;
#define	pi	_pi._pi
#else
	PARTITION_INFORMATION	pi;
#define	_pi	pi
#endif

	ok = DeviceIoControl(_handle,
		IOCTL_DISK_GET_PARTITION_INFO,
		0, 0,
		&_pi, sizeof(_pi),	/* part of PI_HACK */
		&n,
		0);
	if (!ok)
		return RC(fcWIN32);

#ifdef notyet_for_raw_disk
	setOffset(_dg.blocks, dg.Cylinders);
	_dg.blocks *= dg.TracksPerCylinder * dg.SectorsPerTrack;
#else
	setOffset(_dg.blocks, pi.PartitionLength);
	_dg.blocks /= dg.BytesPerSector;

#if !defined(LARGEFILE_AWARE)
	/* XXX really should check >2GB */
	if (pi.PartitionLength.HighPart) {
		cerr << "WARNING: getGeometry: PartitionLength > 4GB: "
			<< pi.PartitionLength << endl;
		_dg.blocks = w_base_t::uint4_max / dg.BytesPerSector;
	}
#endif
#endif

	_dg.block_size = dg.BytesPerSector;

	_dg.sectors = dg.SectorsPerTrack;
	_dg.tracks = dg.TracksPerCylinder;

#ifdef notyet_for_raw_disk
	/* XXX really should check >2GB */
	if (dg.Cylinders.HighPart) {
		cerr << "WARNING: getGeometry: Cylinders >4GB: " 
			<< dg.Cylinders << endl;
		_dg.cylinders = w_base_t::uint4_max;
	}
	else
		_dg.cylinders = dg.Cylinders.LowPart;
#else
	/* Number of full cylinders in the partition.  If you want to
	   use all the blocks, use the block count, otw the geometry. */
	fileoff_t	cyl;
	cyl = _dg.blocks / (_dg.tracks * _dg.sectors);
	if (cyl > (fileoff_t) w_base_t::uint4_max) {
		cerr << "WARNING: getGeometry: Cylinders > 4GB: "
			<< cyl << endl;
		cyl = w_base_t::uint4_max;
	}

	_dg.cylinders = (size_t) cyl;
#endif

	_dg.label_size = 0;	/* XXX differs for raw disk */

	return RCOK;
}

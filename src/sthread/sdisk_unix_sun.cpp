/*<std-header orig-src='shore'>

 $Id: sdisk_unix_sun.cpp,v 1.18 2007/05/18 21:53:43 nhall Exp $

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

#include <w.h>
#include <sdisk.h>
#include <sdisk_unix.h>

#include <iostream>
#include <os_fcntl.h>
#include <sys/stat.h>

#include <st_error_enum_gen.h>	/* XXX */

#if defined(SOLARIS2)
#include <sys/dkio.h>
#include <sys/vtoc.h>
#if HAVE_SYS_PARAM_H
#include <sys/param.h>
#endif
#define	HAVE_GEOMETRY
#endif

#if defined(SUNOS41)
// we should get dk_map from <sun/dkio.h> but its definition is not c++ friendly
struct dk_map {
	daddr_t dkl_cylno;      /* starting cylinder */
	daddr_t dkl_nblk;       /* number of 512-byte blocks */
};
#include <sun/dkio.h>
#if HAVE_SYS_PARAM_H
#include <sys/param.h>
#endif
#define	HAVE_GEOMETRY
#endif

#if !defined(SOLARIS2) && !defined(AIX41)
extern "C" int ioctl(int, int, ...);
#endif

#include <os_interface.h>


w_rc_t	sdisk_getgeometry(int fd, sdisk_t::disk_geometry_t &dg)
{
	w_rc_t	e;
	int	n;
	os_stat_t	st;

	n = ::os_fstat(fd, &st);
	if (n == -1)
		return RC(fcOS);

	/* only raw disks have a geometry */
	if ((st.st_mode & S_IFMT) != S_IFCHR)
		return RC(stINVALIDIO);

#if defined(SOLARIS2)
	// use the size of the raw partition for the log
	struct dk_cinfo		cinfo;
	struct dk_allmap	allmap;
	struct vtoc		vtoc;

#ifdef PURIFY_ZERO
	memset(&cinfo, '\0', sizeof(cinfo));
	memset(&allmap, '\0', sizeof(allmap));
	memset(&vtoc, '\0', sizeof(vtoc));
#endif
	
	n = ioctl(fd, DKIOCINFO, &cinfo);
	if (n == -1) {
		e = RC(fcOS);
		cerr << "ioctl(DKIOCINFO):" << endl << e << endl;
	        return e;
	}
	
	n = ioctl(fd, DKIOCGAPART, &allmap);
	if (n == -1) {
		e = RC(fcOS);
		cerr << "ioctl(DKIOCGAPART):" << endl << e << endl;
		return e;
	}

	n = ioctl(fd, DKIOCGVTOC, &vtoc);
	if (n == -1) {
		e = RC(fcOS);
		cerr << "ioctl(DKIOCGVTOC):" << endl << e << endl;
		return e;
	}
	
	dg.block_size = vtoc.v_sectorsz;
	dg.blocks = allmap.dka_map[cinfo.dki_partition].dkl_nblk;

#elif defined(SUNOS41)

	struct dk_info	info;
	struct dk_map	map;

#ifdef PURIFY_ZERO
	memset(&info, '\0', sizeof(info));
	memset(&map, '\0', sizeof(map));
#endif

	n = ioctl(fd, DKIOCINFO, &info);
	if (n == -1) {
		e = RC(fcOS);
		cerr << "ioctl(DKIOCINFO):" << endl << e << endl;
		return e;
	}

	if (info.dki_ctype != DKC_SCSI_CCS) {
		e = RC(fcINTERNAL);
		cerr << "non-scsi drives not supported at user-level"
			<< endl << e << endl;
		return e;
	}
	
	n = ioctl(fd, DKIOCGPART, &map);
	if (n == -1) {
		e = RC(fcOS);
		cerr << "ioctl(DKIOCGAPART):" << endl << e << endl;
		return e;
	}
	
	dg.block_size = DEV_BSIZE;
	dg.blocks = map.dkl_nblk;
#endif

#if defined(SUNOS41) || defined(SOLARIS2) 
	struct dk_geom      geom;

#ifdef PURIFY_ZERO
	memset(&geom, '\0', sizeof(geom));
#endif

	n = ioctl(fd, DKIOCGGEOM, &geom);
	if (n == -1) {
		e = RC(fcOS);
		cerr << "ioctl(DKIOCGGEOM):" << endl << e << endl;
		return e;
	}

	dg.sectors = geom.dkg_nsect;
	dg.tracks = geom.dkg_nhead;
	dg.cylinders = geom.dkg_ncyl;	// only valid for 'c' or 'd'

#ifdef DK_LABEL_SIZE
	dg.label_size = DK_LABEL_SIZE;
#else
	dg.label_size = 512;	/* XXX check for SunOS */
#endif
#endif

#ifdef HAVE_GEOMETRY
	/* No way to find #cylinders in a partition; calculate it by hand.
	   Assume (valid for most everything) that partitions are
	   cylinder aligned. */
	dg.cylinders = dg.blocks / (dg.sectors * dg.tracks);
#else
	e = RC(fcNOTIMPLEMENTED);
	cerr << "fgetgeometry(): only implemented on SunOS and Solaris"
		<< endl << e << endl;
	dg.cylinders = 0;
	dg.tracks = 0;
	dg.sectors = 0;
	dg.block_size = 0;
	dg.blocks = 0;
	dg.label_size = 0;
#endif
	return e;
}


#if defined(SOLARIS2) || defined(SUNOS41)
w_rc_t	sdisk_unix_t::getGeometry(disk_geometry_t &dg)
{
	if (_fd == FD_NONE)
		return RC(stBADFD);

	return sdisk_getgeometry(_fd, dg);
}
#endif

/*<std-header orig-src='shore'>

 $Id: init_winsock.cpp,v 1.7 2000/01/14 00:14:09 bolo Exp $

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

#include <w_debug.h>
#include <w_stream.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <w_winsock.h>
#include <windows.h>
#include <io.h>
#include <process.h>
#include <w_rusage.h>


w_rc_t	init_winsock()
{
	int	err;
	WORD	wVersionRequested;
	WSADATA wsaData;

	wVersionRequested = MAKEWORD(2, 0);
	err = WSAStartup(wVersionRequested, &wsaData);
	if (err != 0) {
		w_rc_t	e = RC2(fcWIN32, err);
		cerr << "WSAStartup(version 1.1):" 
			<< endl << e << endl;
		return e;
	}

	/* Confirm that the Windows Sockets DLL supports 1.1.*/
	/* Note that if the DLL supports versions greater */
	/* than 1.1 in addition to 1.1, it will still return */
	/* 1.1 in wVersion since that is the version we */
	/* requested. */
	
#if 0
	W_FORM(cerr)("winsock version %u, %u\n",
				LOBYTE(wsaData.wVersion),
				HIBYTE(wsaData.wVersion));
#endif
	if ( wsaData.wVersion != wVersionRequested ) {
		WSACleanup();
		W_FORM2(cerr, ("Winsock version (%d, %d) not compatible!\n",
				LOBYTE(wsaData.wVersion),
				HIBYTE(wsaData.wVersion)));
		return RC(fcINTERNAL);;
	}

	return RCOK;
}

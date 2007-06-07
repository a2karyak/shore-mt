/*<std-header orig-src='shore'>

 $Id: w_wsa.cpp,v 1.8 1999/06/07 19:06:18 kupsch Exp $

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

#ifdef _WINDOWS
#include <w_debug.h>
#include <w_stream.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <w_rusage.h>

#include <w_wsa.h>


wsa_init::~wsa_init()
{
    WSACleanup();
}

wsa_init::wsa_init()
	: _error(false)
{
    WORD wVersionRequested;
    WSADATA wsaData;
    int err;
    wVersionRequested = MAKEWORD(1, 1);
    err = WSAStartup(wVersionRequested, &wsaData);
    if (err != 0) {
	cerr << "Couldn't find a usable WINSOCK.DLL" << endl;
        _error = true;
	return;
    }
    /* Confirm that the Windows Sockets DLL supports 1.1.*/
    /* Note that if the DLL supports versions greater */
    /* than 1.1 in addition to 1.1, it will still return */
    /* 1.1 in wVersion since that is the version we */
    /* requested. */

    if ( LOBYTE( wsaData.wVersion ) != 1 || HIBYTE( wsaData.wVersion ) != 1 ) {
	cerr << "Couldn't find a usable WINSOCK.DLL" << endl;
	WSACleanup();
        _error = true;
	return; 
    }
}

#endif /* _WINDOWS */


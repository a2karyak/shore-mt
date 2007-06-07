/*<std-header orig-src='shore'>

 $Id: win32_error.cpp,v 1.10 2000/01/14 00:16:48 bolo Exp $

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
 * NT error system interface is Copyright 1996, 1998 by:
 *
 *	Josef T. Burger		<bolo@cs.wisc.edu>
 *
 * All rights reserved.
 *
 * This software may be freely used as long as credit is given
 * to the author and this copyright is maintained in the source.
 */

/* An interface to windows-NT error numbers and strings. */


#include <w_windows.h>

#include <win32_error.h>

#include <w_stream.h>


/*
 * Define this if winsock error strings are missing from the
 * system error tables.  In that case, we output the error messages
 * grabbed from the include file.
 */
#define NO_WINSOCK_MESSAGES
#ifdef NO_WINSOCK_MESSAGES
static const char *find_winsock_error(int err);
#endif


int last_win32_error()
{
	return GetLastError();
}


void format_win32_error(int err, char *buf, int bufsize)
{
	DWORD	len;

	len = FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM,
			    0,
			    err,
			    MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
			    buf,
			    bufsize,
			    0);

	if (len > 0) {	/* Erase junk at the end of string */
		if (buf[len-1] == '\n' && buf[len-2] == '\r') {
			len -= 2;
			buf[len] = '\0';
		}
	}

#ifdef NO_WINSOCK_MESSAGES
	if (len == 0 && err >= WSABASEERR) {
		const char *s = find_winsock_error(err);
		if (s) {
			strncpy(buf, s, bufsize);
			len = 1;	/* XXX */
		}
	}
#endif

	if (len == 0) {	/* Fallback if an error message is not found. */
		ostrstream s(buf, bufsize);
		s << "<error " << err << ">" << ends;
	}

	buf[bufsize-1] = '\0';
}


#ifdef NO_WINSOCK_MESSAGES
/* Hacky solution for WinSock error messages for now. */
struct my_error {
	int		err;
	const char	*message;
};

#ifndef NUMBEROF
#define	NUMBEROF(x)	(sizeof(x) / sizeof(x[0]))
#endif


int	my_error_compare(const void *_l, const void *_r)
{
	const my_error &l = *(my_error *) _l;
	const my_error &r = *(my_error *) _r;

	return l.err - r.err;
}



static my_error winsock_errors[] = {
{ WSAEINTR, "WSAEINTR" },
{ WSAEBADF, "WSAEBADF" },
{ WSAEACCES, "WSAEACCES" },
{ WSAEFAULT, "WSAEFAULT" },
{ WSAEINVAL, "WSAEINVAL" },
{ WSAEMFILE, "WSAEMFILE" },
{ WSAEWOULDBLOCK, "WSAEWOULDBLOCK" },
{ WSAEINPROGRESS, "WSAEINPROGRESS" },
{ WSAEALREADY, "WSAEALREADY" },
{ WSAENOTSOCK, "WSAENOTSOCK" },
{ WSAEDESTADDRREQ, "WSAEDESTADDRREQ" },
{ WSAEMSGSIZE, "WSAEMSGSIZE" },
{ WSAEPROTOTYPE, "WSAEPROTOTYPE" },
{ WSAENOPROTOOPT, "WSAENOPROTOOPT" },
{ WSAEPROTONOSUPPORT, "WSAEPROTONOSUPPORT" },
{ WSAESOCKTNOSUPPORT, "WSAESOCKTNOSUPPORT" },
{ WSAEOPNOTSUPP, "WSAEOPNOTSUPP" },
{ WSAEPFNOSUPPORT, "WSAEPFNOSUPPORT" },
{ WSAEAFNOSUPPORT, "WSAEAFNOSUPPORT" },
{ WSAEADDRINUSE, "WSAEADDRINUSE" },
{ WSAEADDRNOTAVAIL, "WSAEADDRNOTAVAIL" },
{ WSAENETDOWN, "WSAENETDOWN" },
{ WSAENETUNREACH, "WSAENETUNREACH" },
{ WSAENETRESET, "WSAENETRESET" },
{ WSAECONNABORTED, "WSAECONNABORTED" },
{ WSAECONNRESET, "WSAECONNRESET" },
{ WSAENOBUFS, "WSAENOBUFS" },
{ WSAEISCONN, "WSAEISCONN" },
{ WSAENOTCONN, "WSAENOTCONN" },
{ WSAESHUTDOWN, "WSAESHUTDOWN" },
{ WSAETOOMANYREFS, "WSAETOOMANYREFS" },
{ WSAETIMEDOUT, "WSAETIMEDOUT" },
{ WSAECONNREFUSED, "WSAECONNREFUSED" },
{ WSAELOOP, "WSAELOOP" },
{ WSAENAMETOOLONG, "WSAENAMETOOLONG" },
{ WSAEHOSTDOWN, "WSAEHOSTDOWN" },
{ WSAEHOSTUNREACH, "WSAEHOSTUNREACH" },
{ WSAENOTEMPTY, "WSAENOTEMPTY" },
{ WSAEPROCLIM, "WSAEPROCLIM" },
{ WSAEUSERS, "WSAEUSERS" },
{ WSAEDQUOT, "WSAEDQUOT" },
{ WSAESTALE, "WSAESTALE" },
{ WSAEREMOTE, "WSAEREMOTE" },
{ WSASYSNOTREADY, "WSASYSNOTREADY" },
{ WSAVERNOTSUPPORTED, "WSAVERNOTSUPPORTED" },
{ WSANOTINITIALISED, "WSANOTINITIALISED" },
{ WSAEDISCON, "WSAEDISCON" },
#ifdef WSAENOMORE
{ WSAENOMORE, "WSAENOMORE" },
{ WSAECANCELLED, "WSAECANCELLED" },
{ WSAEINVALIDPROCTABLE, "WSAEINVALIDPROCTABLE" },
{ WSAEINVALIDPROVIDER, "WSAEINVALIDPROVIDER" },
{ WSAEPROVIDERFAILEDINIT, "WSAEPROVIDERFAILEDINIT" },
{ WSASYSCALLFAILURE, "WSASYSCALLFAILURE" },
{ WSASERVICE_NOT_FOUND, "WSASERVICE_NOT_FOUND" },
{ WSATYPE_NOT_FOUND, "WSATYPE_NOT_FOUND" },
{ WSA_E_NO_MORE, "WSA_E_NO_MORE" },
{ WSA_E_CANCELLED, "WSA_E_CANCELLED" },
{ WSAEREFUSED, "WSAEREFUSED" }
#endif
};

static int num_winsock_errors = NUMBEROF(winsock_errors);

static const char *find_winsock_error(int err)
{
	my_error	find_me;
	my_error	*it;

	find_me.err = err;
	find_me.message = "bogus";
	
	it = (my_error *) bsearch(&find_me,
				  winsock_errors, num_winsock_errors,
				  sizeof(my_error),
				  &my_error_compare);

	return it ? it->message : 0;
}

#endif /* NO_WINSOCK_MESSAGES */

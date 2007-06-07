/*<std-header orig-src='shore'>

 $Id: unix_error.cpp,v 1.8 2007/05/18 21:38:24 nhall Exp $

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


/* An interface to Unix/Posix error numbers and strings. */

#include <unix_error.h>

#include <cstring>


int last_unix_error()
{
	return errno;
}

void format_unix_error(int err, char *buf, int bufsize)
{
#ifdef HAVE_STRERROR
	char	*s = strerror(err);
#else
	char	*s = "No strerror function. Cannot format unix error.";
#endif
	strncpy(buf, s, bufsize);
	buf[bufsize-1] = '\0';
}

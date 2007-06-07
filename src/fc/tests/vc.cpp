/*<std-header orig-src='shore'>

 $Id: vc.cpp,v 1.8 1999/06/07 19:03:06 kupsch Exp $

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

#include <w_stream.h>
#include <stddef.h>
#include <w.h>
#include <w_vector.h>

#ifdef _WINDOWS
/*
 * vc.cpp - a place to put tests of things that happen in 
 * visual c++ (or NT, for that matter)
 */
#include <fcntl.h>

bool
fd_allzero(const fd_set *s)
{
    if(s == 0) {
	    return true;
    }

#ifdef _WINDOWS
    if(s->fd_count == 0) {
	    return true;
    } else {
	    return false;
    }
#endif
}

int
main()
{
    int testnum=1;
    {
	/* dereference a zero ptr */
	cout <<  "Test " << testnum << " : Dereference a null ptr " <<endl;
	char *p =0;
	char junk = *p;
    }
    cout <<  "Test " << testnum++ << " done." << endl;

    {
	/* dereference a ptr to page 0 */
	cout <<  "Test " << testnum << " Dereference a ptr to page 0." << endl;
	struct {
		int i;
		int j;
	} *p = 0;
	int  junk = p->j;
    }
    cout <<  "Test " << testnum++ << " done." << endl;

    {
	/* dereference a null ptr to fd_set */
	cout <<  "Test " << testnum << " Dereference a 0 ptr to fd_set." << endl;
	fd_set *p = 0;
	bool b = fd_allzero(p);
    }
    cout <<  "Test " << testnum++ << " done." << endl;
    return 0;
}
#else 

int
main()
{
    cout <<  "No-op on Unix\n" << endl;
}
#endif /* _WINDOWS */


/*<std-header orig-src='shore'>

 $Id: dtid_t.cpp,v 1.14 2006/03/14 05:31:26 bolo Exp $

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

#define SM_SOURCE
#define DTID_T_C

#include "dtid_t.h"
#include <stime.h>

#include <w_strstream.h>

#ifdef USE_COORD

uint4_t DTID_T::unique() {
    // TODO: make this unique
    return 0x42424242; // "BBBB"
}
void
DTID_T::update() 
{
	relative ++;

	stime_t stamp = stime_t::now();
	w_ostrstream s(date, sizeof(date));
	stamp.ctime(s);
	s << endl << ends;
	/* paranoia .. Ensure it is null terminated. */
	date[sizeof(date)-1] = '\0';
}

NORET 	
DTID_T::DTID_T() 
{
    memset(nulls, '\0', sizeof(nulls));
    location =  unique();
    relative =  0x41414140; // "AAAA"
    update();
}
#endif /* USE_COORD */


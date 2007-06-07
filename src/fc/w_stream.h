/*<std-header orig-src='shore' incl-file-exclusion='W_STREAM_H'>

 $Id: w_stream.h,v 1.8 2004/10/05 23:19:55 bolo Exp $

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

#ifndef W_STREAM_H
#define W_STREAM_H

#include "w_defines.h"

/*  -- do not edit anything above this line --   </std-header>*/

#ifdef _WINDOWS

/*
 * NB: _WINDOWS has 2 implementation of the iostream libraries;
 * they don't mix.  You have to choose 1 or the other.  I don't yet
 * know how you tell what version is used in any given DLL that you
 * are using.
 */

#	define  _OLD_IOSTREAM_LIBRARIES  1

#	ifdef _OLD_IOSTREAM_LIBRARIES
	/*
	 * If you want the OLD iostream libraries:
	 */
#	include <fstream.h>
#	include <iomanip.h>
#	include <ios.h>
#	include <iostream.h>
#	include <istream.h>
#	include <ostream.h>
#	include <streamb.h>
#	include <strstrea.h>  /* DISGUSTING */

#	else
	/*
	 * If you want the NEW iostream libraries:
         * WARNING: compile with -GX
	 */
#	include <fstream>
#	include <iomanip>
#	include <ios>
#	include <iostream>
#	include <istream>
#	include <ostream>
#	include <streambuf>
#	include <strstream>

    using namespace std;

#	endif /* _OLD_IOSTREAM_LIBRARIES */
#else
#	include <fstream>
#	include <iomanip>
#	include <iostream>
#	include <w_strstream.h>
#endif

/*<std-footer incl-file-exclusion='W_STREAM_H'>  -- do not edit anything below this line -- */

#endif          /*</std-footer>*/

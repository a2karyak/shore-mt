/*<std-header orig-src='shore' incl-file-exclusion='W_FORM_H'>

 $Id: w_form.h,v 1.2 2006/03/25 23:30:27 bolo Exp $

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

#ifndef W_FORM_H
#define W_FORM_H

#include "w_defines.h"

/*  -- do not edit anything above this line --   </std-header>*/


/*
 *   This software is Copyright 1989, 1991, 1992, 1993, 1994, 1998,
 *                              2003, 2004 by:
 *
 *	Josef Burger	<bolo@cs.wisc.edu>
 *
 *   All Rights Reserved.
 *
 *   This software may be freely used as long as credit is given
 *   to the author and this copyright is maintained.
 */

#if 0
// Just use the system ... if it is available

#include "stream.h"

#else

extern const char *hex(int, int=0);
extern const char *hex(unsigned, int=0);
extern const char *hex(long, int=0);
extern const char *hex(unsigned long, int=0);

extern const char *dec(int, int=0);
extern const char *dec(unsigned, int=0);
extern const char *dec(long, int=0);
extern const char *dec(unsigned long, int=0);

extern const char *oct(int, int=0);
extern const char *oct(unsigned, int=0);
extern const char *oct(long, int=0);
extern const char *oct(unsigned long, int=0);

#endif

/*<std-footer incl-file-exclusion='W_FORM_H'>  -- do not edit anything below this line -- */

#endif          /*</std-footer>*/

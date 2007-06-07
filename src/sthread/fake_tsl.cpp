/*<std-header orig-src='shore'>

 $Id: fake_tsl.cpp,v 1.11 1999/06/07 19:05:59 kupsch Exp $

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
 * The Testandset package is Copyright 1993, 1994, 1995, 1997 by:
 *
 *	Josef Burger	<bolo@cs.wisc.edu>
 *
 * All rights reserved.
 *
 * The Testandset package may be freely used as long as credit is given
 * to the author and the above copyright is maintained. 
 */


extern "C" {
#include "tsl.h"
}

unsigned tsl(tslcb_t *it, int val)
{
	unsigned	old;
	if (!val)
		val = 1;
	old = it->lock;
	if (!old)
		it->lock = val;
	return	 old;
}

void tsl_init(tslcb_t *it)
{
	it->lock = 0;
}

void tsl_release(tslcb_t *it)
{
	it->lock = 0;
}

unsigned tsl_examine(tslcb_t *it)
{
	return it->lock;
}


/*<std-header orig-src='shore'>

 $Id: cq1.cpp,v 1.17 2000/01/07 07:17:04 bolo Exp $

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

#define W_INCL_CIRCULAR_QUEUE
#include <w.h>

#ifdef EXPLICIT_TEMPLATE
template class w_cirqueue_t<int>;
#endif


int main()
{
    int qbuf[10];
    w_cirqueue_t<int> q(qbuf, 10);

    w_assert1(q.is_empty());

    int i;
    for (i = 0; i < 10; i++)  {
	W_COERCE( q.put(i) );
    }

    w_assert1(q.is_full());

    w_assert1( q.put(i) );

    for (i = 0; i < 10; i++)  {
	int j;
	W_COERCE( q.get(j));
	w_assert1(j == i);
    }

    w_assert1(q.is_empty());
    w_assert1( q.get() );

    for (i = 0; i < 3; i++)  {
	W_COERCE( q.put(i) );
    }

    for (i = 0; i < 2; i++)  {
	W_COERCE( q.put(i) );
    }

    for (i = 0; i < 3; i++)  {
	W_COERCE( q.get());
    }

    w_assert1(! q.is_empty());
    w_assert1(q.cardinality() == 2);

    for (i = 2; i < 10; i++)  {
	W_COERCE(q.put(i));
    }

    w_assert1(q.is_full());
    w_assert1( q.put(i) );

    for (i = 0; i < 10; i++)  {
	int j;
	W_COERCE( q.get(j));
	w_assert1(j == i);
    }

    w_assert1( q.get() );

    return 0;
}


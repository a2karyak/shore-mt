/*<std-header orig-src='shore'>

 $Id: stats.cpp,v 1.14 1999/06/07 19:03:06 kupsch Exp $

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
#include <time.h>
#endif

#include <w.h>
#include <unix_stats.h>


int
main()
{
    unix_stats U;
    U.start();

    double f= 3.14159;

    int i;
    for(i=0; i < 10000000; i++) {
	f *= 3.0/2.8;
    }

    U.stop(1); // 1 iteration

    cout << "Unix stats :" <<endl;
    cout << "float f=" << float(f) << endl;
    cout << U << endl << endl;
    cout << flush;

    return 0;
}


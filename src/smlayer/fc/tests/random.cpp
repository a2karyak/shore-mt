/*<std-header orig-src='shore'>

 $Id: random.cpp,v 1.10 1999/06/07 19:03:05 kupsch Exp $

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
#include <w_random.h>

int
main(int argc, char *argv[])
{
    if(argc > 1) {
	int seed;
	if( (seed = atoi(argv[1])) != 0) {
	    random_generator::generator.srand(seed);
	    cout << random_generator::generator.lrand() <<endl ;
	    return 0;
	}
    }
    int i;
    for(i=1; i<25; i++) {
	cerr << "i= "<< i << " ";
	cerr << random_generator::generator.lrand() ;
	cerr <<endl;
    }
    for(i=1; i<25; i++) {
	cerr << "i= "<< i << " ";
	W_FORM2(cerr, ("%10.10f ",random_generator::generator.drand()) );
	cerr <<endl;
    }
    for(i=1; i<25; i++) {
	cerr << "i= "<< i << " ";
	W_FORM2(cerr, ("%d (0->25) ",random_generator::generator.mrand_choice(0,25)) );
	cerr <<endl;
    }
    cerr << "done." << endl;

    return 0;
}


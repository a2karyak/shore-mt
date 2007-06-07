/*<std-header orig-src='shore'>

 $Id: rc.cpp,v 1.24 2006/01/29 18:09:01 bolo Exp $

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
#include <cstddef>
#include <w.h>

w_rc_t testing()
{
    w_rc_t rc = RC(fcOS);
	RC_AUGMENT(rc);
	RC_PUSH(rc, fcINTERNAL);
	RC_AUGMENT(rc);
	RC_PUSH(rc, fcFULL);
	RC_AUGMENT(rc);
	RC_PUSH(rc, fcEMPTY);
	RC_AUGMENT(rc);
	RC_PUSH(rc, fcNOTFOUND);
	RC_AUGMENT(rc);
    if (rc)  {; }

    return rc;
}

w_rc_t testing_ok()
{
    return RCOK;
}

int main()
{
    cout << "Expect two 'error not checked' message" << endl;
    {
        w_rc_t rc = testing();
    }

    {
	testing_ok();
    }

    if(testing_ok() != RCOK) {
	cout << "FAILURE: This should never happen!" << endl;
    }

    cout << "Expect 3 forms of the string of errors" << endl;
    {
        w_rc_t rc = testing();
	{
	    //////////////////////////////////////////////////// 
	    // this gets you to the integer values, one at a time
	    //////////////////////////////////////////////////// 
	    cout << "*************************************" << endl;
	    w_rc_i iter(rc);
	    cout << endl << "1 : List of error numbers:" << endl;
	    for(w_base_t::int4_t e = iter.next_errnum();
		    e!=0; e = iter.next_errnum()) {
		cout << "error = " << e << endl;
	    }
	    cout << "End list of error numbers:" << endl;
	}
	{
	    //////////////////////////////////////////////////// 
	    // this gets you to the w_error_t structures, one
	    // at a time.  If you print each one, though, you
	    // get it PLUS everything attached to it
	    //////////////////////////////////////////////////// 
	    w_rc_i iter(rc);
	    cout << "*************************************" << endl;
	    cout << endl << "2 : List of w_error_t:" << endl;
	    for(w_error_t *e = iter.next();
		    e; 
		    e = iter.next()) {
		cout << "error = " << *e << endl;
	    }
	    cout << "End list of w_error_t:" << endl;
	}
	{
	    cout << "*************************************" << endl;
	    cout << endl << "3 : print the rc:" << endl;
	    cout << "error = " << rc << endl;
	    cout << "End print the rc:" << endl;
	}
    }
    return 0;
}


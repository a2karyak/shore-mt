/*<std-header orig-src='shore'>

 $Id: vector.cpp,v 1.10 2000/01/07 07:17:05 bolo Exp $

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

typedef w_vector_t<float> floatvec;
#ifdef EXPLICIT_TEMPLATE
template class w_vector_t<float>;
#endif

ostream & 
operator<<(ostream &o, const floatvec &v)
{
    unsigned i;
    for(i=0; i< v.vectorSize(); i++) {
	o << i 
		<< "[" 
		<< v[i] 
		<< "] "; 
    }
    return o;
}

int main()
{
    floatvec v(3);

    v[0] = 1.0f;
    v[1] = 10.01f;
    v[2] = 100.001f;

    floatvec w(3);

    w[0] = 2.0f;
    w[1] = 20.02f;
    w[2] = 200.002f;

    floatvec x(v);

    cout << "w = " << w << endl;
    cout << "v = " << v << endl;

    x += w;
    cout << "(x=v) + w = " << x << endl;

    x -= w;
    cout << "(x - w) = " << x << endl;
    x -= v;
    cout << "(x - v) = " << x << endl;

    x = v;
    x -= w;
    cout << "(x=v - w) = " << x << endl;

    cout << "x > w = " << (bool)(x>w) << endl;
    cout << "x < w = " << (bool)(x<w) << endl;
    cout << "x == w = " << (bool)(x==w) << endl;
    cout << "x != w = " << (bool)(x!=w) << endl;

    return 0;
}


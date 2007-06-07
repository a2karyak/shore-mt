/*<std-header orig-src='shore'>

 $Id: list1.cpp,v 1.24 2006/01/29 18:09:01 bolo Exp $

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

struct elem1_t {
    int 	i;
    w_link_t	link;
};

ostream& operator<<(ostream& o, const elem1_t& e)
{
    return o << e.i;
}

int main()
{
    w_list_t<elem1_t> l(W_LIST_ARG(elem1_t, link));

    elem1_t array[10];
    
    int i;
    for (i = 0; i < 10; i++)  {
	array[i].i = i;
	l.push(&array[i]);
    }

    cout << l << endl;

    for (i = 0; i < 10; i++)  {
#ifdef W_DEBUG
	elem1_t* p = l.pop();
	w_assert3(p);
	w_assert3(p->i == 9 - i);
#else
	(void) l.pop();
#endif
    }

    w_assert3(l.pop() == 0);

    for (i = 0; i < 10; i++)  {
	l.append(&array[i]);
    }

    for (i = 0; i < 10; i++)  {
#ifdef W_DEBUG
	elem1_t* p = l.chop();
#else
	(void) l.chop();
#endif
	w_assert3(p);
	w_assert3(p->i == 9 - i);
    }
    w_assert3(l.chop() == 0);

    return 0;
}

#ifdef __BORLANDC__
#pragma option -Jgd
#include <w_list.cpp>
typedef w_list_t<elem1_t> w_list_t_elem1_t_dummy;
#endif /*__BORLANDC__*/


#ifdef __GNUG__
// force instatiation of this function.
template ostream& operator<<(ostream& o, const w_list_t<elem1_t>& l);
#endif

#ifdef EXPLICIT_TEMPLATE
template class w_list_t<elem1_t>;
template class w_list_i<elem1_t>;
#endif


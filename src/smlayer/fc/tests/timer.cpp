/*<std-header orig-src='shore'>

 $Id: timer.cpp,v 1.14 2000/01/07 07:17:05 bolo Exp $

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

#include <w_timer.h>

w_timerqueue_t events;

class mytimer : public w_timer_t {
public:
	void	trigger() {
		cout << "mytimer ... " << *this << endl;
	}
};

mytimer	one, two, three, four, five;

// extern "C" unsigned long time(unsigned long *);

#ifdef _WINDOWS
#define SLEEP(x) Sleep(x*1000)
#else
#define SLEEP(x) sleep(x)
#endif

int main()
{
	cout << events << endl;

	stime_t	now(stime_t::now());

	one.reset(now + (stime_t)10.0);
	two.reset(now + (stime_t)2.0);
	three.reset(now + (stime_t)15.0);
	four.reset(now + (stime_t)10.0);
	five.reset(now + (stime_t)7.0);
	
	events.schedule(one);
	events.schedule(two);
	events.schedule(three);
	events.schedule(four);
	events.schedule(five);

	cout << "it is now " << now << endl;
	cout << "it is now " << (double)now << ", double" << endl;
	cout << events << endl;

	stime_t	then;
	while (events.nextEvent(then)) {
		cout << "next in " << (sinterval_t)(then - now)
			<< ", sinterval_t";
		cout << " or " << (double)(then - now)
			<< ", double";
#if 0
		/* gcc 2.7.2 can't decide on a conversion */
		cout << " or " << (unsigned long)(then - now)
			<< ", unsigned long";
#endif
		cout << endl;

#ifdef stupid 
		while (now < then) {
			SLEEP(10);
			now = stime_t::now();
		}
#else
		SLEEP(((int)(double)(then - now)));
		now = then;
#endif

		cout << "Running events" << endl;
		events.run(now);
		cout << events << endl;
	}

	cout << "Finis" << endl << events << endl; 

	return 0;
}


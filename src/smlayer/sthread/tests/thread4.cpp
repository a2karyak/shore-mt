/*<std-header orig-src='shore'>

 $Id: thread4.cpp,v 1.21 1999/10/24 17:45:57 bolo Exp $

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
#include <stdlib.h>
#include <time.h>
#include <memory.h>

#include <w.h>
#include <sthread.h>

class timer_thread_t : public sthread_t {
public:
	timer_thread_t(unsigned ms);

protected:
	virtual void run();

private:
	unsigned _ms;
};

unsigned default_timeout[] = { 
	4000, 5000, 1000, 6000, 4500, 4400, 4300, 4200, 4100, 9000
};

bool	verbose = false;


int main(int argc, char **argv)
{
	int		timeouts;
	unsigned	*timeout;

	if (argc > 1 && strcmp(argv[1], "-v") == 0) {
		verbose = true;
		argc--;
		argv++;
	}
	if (argc > 1) {
		timeouts = argc - 1;
		timeout = new unsigned[timeouts];
		for (int i = 1; i < argc; i++)
			timeout[i-1] = atoi(argv[i]);
	}
	else {
		timeouts = sizeof(default_timeout) /sizeof(default_timeout[0]);
		timeout = default_timeout;
	}

	timer_thread_t **timer_thread = new timer_thread_t *[timeouts];

	int i;
	for (i = 0; i < timeouts; i++)  {
		timer_thread[i] = new timer_thread_t(timeout[i]);
		w_assert1(timer_thread[i]);
		W_COERCE(timer_thread[i]->fork());
	}

	for (i = 0; i < timeouts; i++)  {
		W_COERCE( timer_thread[i]->wait());
		delete timer_thread[i];
	}

	delete [] timer_thread;
	if (timeout != default_timeout)
		delete [] timeout;

	if (verbose)
		sthread_t::dump_stats(cout);

	return 0;
}

    

timer_thread_t::timer_thread_t(unsigned ms)
    : sthread_t(t_regular, 0, 0, "timer_thread"), _ms(ms)
{
}

void timer_thread_t::run()
{
	stime_t	start, stop;

	cout << "[" << setw(2) << setfill('0') << id 
		<< "] going to sleep for " << _ms << "ms" << endl;

	if (verbose)
		start = stime_t::now();

	sthread_t::sleep(_ms);

	if (verbose)
		stop = stime_t::now();
	
	cout << "[" << setw(2) << setfill('0') << id 
		<< "] woke up after " << _ms << "ms";
	if (verbose)
		cout << "; measured " << (sinterval_t)(stop-start)
		<< " seconds.";
	cout << endl;
}


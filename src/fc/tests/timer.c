/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994,5, 6  Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */
#include <iostream.h>
#include <assert.h>
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

mytimer	one, two, three, four;

// extern "C" unsigned long time(unsigned long *);

main()
{
	cout << events << endl;

	stime_t	now(stime_t::now());

	one.reset(now + 30.0);
	two.reset(now + 30.0);
	three.reset(now + 10.0);
	four.reset(now + 40.0);
	
	events.schedule(one);
	events.schedule(two);
	events.schedule(three);
	events.schedule(four);

	cout << "it is now " << now << endl;
	cout << "it is now " << (double)now << ", double" << endl;
	cout << events << endl;

	cout << "one in " << (double)(one.when() - now)
		<< ", double" << endl;
	cout << "one in " << (unsigned long)(one.when() - now)
		<< ", unsigned long" << endl;

	stime_t	then;
	while (events.nextEvent(then)) {
#ifdef stupid 
		while (now < then) {
			sleep(10);
			now = stime_t::now();
		}
#else
		sleep((then - now));
		now = then;
#endif

		cout << "Running events" << endl;
		events.run(now);
		cout << events << endl;
	}

	cout << "Finis" << endl << events << endl; 

	return 0;
}

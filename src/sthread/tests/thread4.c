/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

/*
 *  $Id: thread4.c,v 1.10 1996/03/13 20:23:53 bolo Exp $
 */
#include <iostream.h>
#include <iomanip.h>
#include <strstream.h>
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

const N = 10;
unsigned timeout[N] = { 
    4000, 5000, 1000, 6000, 4500, 4400, 4300, 4200, 4100, 9000 };

//const N = 4;
//unsigned timeout[N] = { 400, 500, 100, 600 };

main()
{
    timer_thread_t* timer_thread[N];

    int i;
    for (i = 0; i < N; i++)  {
	timer_thread[i] = new timer_thread_t(timeout[i]);
	w_assert1(timer_thread[i]);
	W_COERCE(timer_thread[i]->fork());
    }

    for (i = 0; i < N; i++)  {
	W_COERCE( timer_thread[i]->wait());
	delete timer_thread[i];
    }

    return 0;
}

    

timer_thread_t::timer_thread_t(unsigned ms)
    : sthread_t(t_regular), _ms(ms)
{
    rename("timer thread");
}

void timer_thread_t::run()
{
    cout << "[" << setw(2) << setfill('0') << id 
	 << "] going to sleep for " << _ms << "ms" << endl;
    sthread_t::sleep(_ms);
    cout << "[" << setw(2) << setfill('0') << id 
	 << "] woke up after " << _ms << "ms" << endl;
}


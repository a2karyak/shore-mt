/*<std-header orig-src='shore' incl-file-exclusion='W_TIMER_H'>

 $Id: w_timer.h,v 1.20 2000/02/01 23:37:12 bolo Exp $

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

#ifndef W_TIMER_H
#define W_TIMER_H

#include "w_defines.h"

/*  -- do not edit anything above this line --   </std-header>*/

#include <stime.h>

class ostream;

#ifdef __GNUG__
#pragma interface
#endif



class w_timerqueue_t;

class w_timer_t {
    friend class w_timerqueue_t;
private:
	w_timer_t	*_next;
	w_timer_t	*_prev;
	w_timerqueue_t	*_queue;

	stime_t		_when;
	bool		_fired;

	// function to execute when the event is triggered
	virtual void	trigger() = 0;

	// function to execute if the event is discarded.
	virtual void	destroy() { }

	// {,de}install me! 
	void	attach(w_timerqueue_t &to, w_timer_t &after);
	void	detach();

public:
	w_timer_t(const stime_t &when = stime_t::sec(0));
	virtual ~w_timer_t();

	/* play it again, sam */
	void	reset(const stime_t &when);

	const stime_t	&when() const {
		return _when;
	}

	bool	fired() const { return _fired; }

	virtual	ostream	&print(ostream &s) const;

private:
	// disabled
	w_timer_t(const w_timer_t &);
	w_timer_t	&operator=(const w_timer_t &);
};


class w_null_timer_t : public w_timer_t {
private:
	void	trigger() { }
	void	destroy() { }

public:
	w_null_timer_t() : w_timer_t(stime_t::sec(0)) { }
};


class w_timerqueue_t {
private:
	w_null_timer_t	_queue;

public:
	w_timerqueue_t();
	~w_timerqueue_t();

	// Combination atomic !isEmpty() + nextTime() 
	bool	nextEvent(stime_t &when) const;

	// any events ?
	bool	isEmpty() const;

	// time of next event, 0 if none
	stime_t	nextTime() const;

	// it is 'now'.  Run any timers which have expired.
	void	run(const stime_t &now);
	
	// schedule and cancel events
	void	schedule(w_timer_t &event);
	void	cancel(w_timer_t &event);

	ostream	&print(ostream &) const;

private:
	// disabled
	w_timerqueue_t(const w_timerqueue_t &);
	w_timerqueue_t	&operator=(const w_timerqueue_t &);
};


inline ostream &operator<<(ostream &s, const w_timer_t &t)
{
	return t.print(s);
}


inline ostream &operator<<(ostream &s, const w_timerqueue_t &q)
{
	return q.print(s);
}

/*<std-footer incl-file-exclusion='W_TIMER_H'>  -- do not edit anything below this line -- */

#endif          /*</std-footer>*/

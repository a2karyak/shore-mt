/*<std-header orig-src='shore' incl-file-exclusion='WIN32_EVENTS_H'>

 $Id: win32_events.h,v 1.13 2001/06/05 03:48:39 bolo Exp $

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

#ifndef WIN32_EVENTS_H
#define WIN32_EVENTS_H

#include "w_defines.h"

/*  -- do not edit anything above this line --   </std-header>*/

/*
 *   NewThreads is Copyright 1992, 1993, 1994, 1995, 1996, 1997, 1998 by:
 *
 *	Josef Burger	<bolo@cs.wisc.edu>
 *      Dylan McNamee   <dylan@cse.ogi.edu>
 *      Ed Felten       <felten@cs.princeton.edu>
 *
 *   All Rights Reserved.
 *
 *   NewThreads may be freely used as long as credit is given
 *   to the above authors and the above copyright is maintained.
 */

#ifdef __GNUC__
#pragma interface
#endif

class win32_event_t;
class win32_event_handler_t;

class win32_event_handler_t {
public:
	win32_event_handler_t();
	~win32_event_handler_t();

	// Wait for sfile events
	w_rc_t		prepare(const stime_t &timeout,
				bool no_timeout);

	w_rc_t		wait();
	void		dispatch(const stime_t &at);
	bool		probe(win32_event_t &);

	/* configure / deconfigure handles which can generate events */
	w_rc_t		start(win32_event_t &);
	void		stop(win32_event_t &);

	/* enable / disable events for a configured handle */
	void		enable(win32_event_t &);
	void		disable(win32_event_t &);

	ostream		&print(ostream &s);

private:
	enum {	/* granularity of memory allocations, # events */
		alloc_chunk = 40
	};
	enum { max_mux = 8 };
	enum { max_fired_per_mux = 4 };
	enum { max_fired = max_fired_per_mux * max_mux };

private:

	w_list_t<win32_event_t> _list;

	HANDLE		*ready;
	win32_event_t	**map;		// map[i] == event for ready[i]
	unsigned	max_ready;	// slots in ready
	unsigned	num_ready;	// #events that are enabled

	unsigned	num_started;	// #events that can enable/disable
	unsigned	num_enabled;	// #events enabled

	unsigned	num_fired;
	win32_event_t	*which_fired[max_fired];

	unsigned	_hits;
	stime_t		_last_event;

	w_rc_t		resize(unsigned howmany);

	DWORD		_timeout;	// milli-seconds
	bool		_timeout_forever;

	static w_rc_t	harvest(HANDLE *, win32_event_t **,
				unsigned count, unsigned threshold,
				win32_event_t ** fired, unsigned max_fired,
				unsigned &ret_num_fired);

	/* XXX temporary controls for multiple event harvesting */
	unsigned	use_rewait;
	unsigned	event_debug;

	unsigned	use_mux;
	bool		force_mux;
	bool		using_mux;	/* for this prepare/wait/dispatch */

	struct eventMux;
	friend eventMux;	/* for max_fired */

	/* Combination running info and startup info for mux threads */
	struct eventMux {
		/* Startup info */
		win32_event_handler_t	&handler;
		const unsigned		which_mux;

		/* Runtime info */
		HANDLE	thread;
#if defined(_MT) && !defined(WIN32_THREADS_ONLY)
		unsigned	thread_id;
#else
		DWORD		thread_id;
#endif
		HANDLE	wakeup;		// start mux waiting
		HANDLE	gen;		// mux generates event
		HANDLE	stopwait;	// stop waiting

		enum muxOp { MUXnone, MUXdone, MUXwait };

		muxOp		op;
		HANDLE		*start;		// starting event to wait on
		unsigned	start_index;
		unsigned	count;		// #entries to wait on
		win32_event_t	**map;

		enum { muxEvents = 63, muxEventsSize = 64 };

		HANDLE		ready[muxEventsSize];

		enum { max_fired = max_fired_per_mux };

		unsigned	num_fired;	// # entries which fired
		win32_event_t	*which_fired[max_fired];

		eventMux(win32_event_handler_t &h, unsigned wm=0)
			: handler(h), which_mux(wm),
			thread(INVALID_HANDLE_VALUE), thread_id(0),
			wakeup(INVALID_HANDLE_VALUE),
			gen(INVALID_HANDLE_VALUE),
			stopwait(INVALID_HANDLE_VALUE),
			op(MUXnone),
			start(0), start_index(0), count(0), map(0),
			num_fired(0)
			{ }
		~eventMux();

		static	w_rc_t	newEventMux(eventMux *&mux,
					    win32_event_handler_t &h,
					    unsigned which = 0);
		
		w_rc_t	_startup();

		unsigned	unmap_events(win32_event_t **which);

		/* called by owner to control the mux */
		w_rc_t	fork();
		w_rc_t	stop();
		w_rc_t	request(muxOp op);

		/* internal stuff to handle wait requests */
		void	prepare();
		bool	wait();

		/* internal startup gizmos */
		/* Putting the thread start function in here is a pain,
		   but if not done it needs to be made a friend, and it is
		   still difficult to hide it. */
#if defined(_MT) && !defined(WIN32_THREADS_ONLY)
		static unsigned	__stdcall	__start(void *);
#else
		static DWORD	__stdcall	__start(void *);
#endif
		void		run();

		ostream		&print(ostream &) const;
	};

	eventMux	*muxes[max_mux];
	unsigned	num_muxes;	// number activated
	unsigned	num_ready_muxes;	// number used for this wait
	HANDLE		ready_mux[max_mux];

	w_rc_t		muxwait();
	w_rc_t		resize_mux(unsigned howmany);

	w_rc_t		start_mux_thread(const unsigned whichMux);
	w_rc_t		stop_mux_thread(const unsigned whichMux);

	w_rc_t		distribute_events();
};


class win32_event_t {
	friend class win32_event_handler_t;

public:
	win32_event_t(HANDLE handle = INVALID_HANDLE_VALUE);
    	virtual	~win32_event_t();

	w_rc_t			change(HANDLE handle = INVALID_HANDLE_VALUE);

	HANDLE			handle;

	virtual	void		ready() = 0;

	bool			probe();

	ostream			&print(ostream &) const;
	
	bool			running() const;
	bool			enabled() const;
	void			enable();
	void			disable();

private:
	w_link_t		_link;
	bool			_enabled;
	win32_event_handler_t	*_owner;
	int			_slot;

	unsigned		_hits;
	stime_t			_last_event;

	void			dispatch(const stime_t &at);

	// disabled
	win32_event_t(const win32_event_t&);
	win32_event_t&	operator=(const win32_event_t&);
};


class win32_safe_event_t : public win32_event_t {
public:
	win32_safe_event_t(HANDLE handle = INVALID_HANDLE_VALUE);
	~win32_safe_event_t();

	w_rc_t			change(HANDLE handle = INVALID_HANDLE_VALUE);

	w_rc_t		wait(long timeout);
	void		shutdown();
	bool		is_down()  { return _shutdown; }

protected:
	void		ready();

private:
	bool		_shutdown;
#ifdef notyet
	bool		_waiting;
	smutex_t	lock;
	scond_t		done;
#else
	sevsem_t	sevsem;
#endif

	// disabled
	win32_safe_event_t(const win32_safe_event_t&);
	win32_safe_event_t	&operator=(const win32_safe_event_t&);
};

/*
 * What compatability you ask?  Actually it is similar to the
 * Unix compatability events.  This event_t automagically installs
 * itself into the system event queue, deinstall itself, sets
 * itself running, etc.  
 *
 * All the things that need to be done for trivial event handling.
 *
 * Just as with the sfile variant of the same name, it must
 * be instantiated with the handle that it will use.
 */

class win32_compat_event_t : public win32_safe_event_t {
public:
	win32_compat_event_t(HANDLE handle);
	~win32_compat_event_t();

};

extern ostream &operator<<(ostream &o, win32_event_handler_t &h);
extern ostream &operator<<(ostream &o, win32_event_t &h);

/*<std-footer incl-file-exclusion='WIN32_EVENTS_H'>  -- do not edit anything below this line -- */

#endif          /*</std-footer>*/

/*<std-header orig-src='shore'>

 $Id: win32_events.cpp,v 1.18 2001/06/05 03:48:40 bolo Exp $

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

/*
 *   NewThreads is Copyright 1992, 1993, 1994, 1995, 1996, 1997, 1998 by:
 *
 *	Josef Burger	<bolo@cs.wisc.edu>
 *	Dylan McNamee	<dylan@cse.ogi.edu>
 *      Ed Felten       <felten@cs.princeton.edu>
 *
 *   All Rights Reserved.
 *
 *   NewThreads may be freely used as long as credit is given
 *   to the above authors and the above copyright is maintained.
 */

#include <w_strstream.h>
#include <w_workaround.h>
#include <stdlib.h>

#ifdef __GNUC__
#pragma implementation "win32_events.h"
#endif

#define W_INCL_LIST
#include <w.h>

#include <sthread.h>
#include <win32_events.h>

#include <w_statistics.h>
#include "sthread_stats.h"
extern class sthread_stats SthreadStats;

#if defined(_MT) && !defined(WIN32_THREADS_ONLY)
#include <process.h>
#endif

#ifdef EXPLICIT_TEMPLATE
template class w_list_t<win32_event_t>;
template class w_list_i<win32_event_t>;
#endif


/*
 * Win32 Event "Handlers" and "Handles".
 *
 * A means of generating thread wakeup events for thread's
 * awaiting event generation from Win32 gadgets.
 */


win32_event_handler_t::win32_event_handler_t()
: _list(offsetof(win32_event_t, _link)),
  ready(0),
  map(0),
  max_ready(0),
  num_ready(0),
  num_started(0),
  num_enabled(0),
  num_fired(0),
  _hits(0),
  _timeout(0),
  _timeout_forever(false),
  use_rewait(0),
  event_debug(0),
  use_mux(0),
  force_mux(false),
  using_mux(false),
  num_muxes(0),
  num_ready_muxes(0)
{
#if 1
	/* Look again for any other events if there are any events remaining.
	   If the multiplexor is in use, it isn't as aggresive as this at the 
	   moment. */
	use_rewait = 1;

	/* The multiplexor will turn on if the #events requires it. */
	use_mux = eventMux::muxEvents;
#endif

	const char	*s;
	int		n;

	s = getenv("WIN32_EVENT_REWAIT");
	if (s && *s) {
		n = atoi(s);
		if (n < 0)
			n = 0;
		use_rewait = n;
	}

	s = getenv("WIN32_EVENT_DEBUG");
	if (s && *s)
		event_debug = atoi(s);

	s = getenv("WIN32_EVENT_MUX");
	if (s && *s) {
		n = atoi(s);
		if (n < 0) {
			force_mux = true;
			n = -n;
		}
		if (n > eventMux::muxEvents)
			n = eventMux::muxEvents;
		use_mux = n;
	}

	unsigned	i;
	for (i = 0; i < max_mux; i++)
		muxes[i] = 0;
}


win32_event_handler_t::~win32_event_handler_t()
{
	/* XXX what if anything is left started or running??? */
	/* XXX what about muxes */

	if (map) {
		delete [] map;
		map = 0;
	}
	if (ready) {
		delete [] ready;
		ready = 0;
	}
}


w_rc_t	win32_event_handler_t::resize(unsigned howmany)
{
	unsigned	i;
	HANDLE		*h;
	win32_event_t	**v;

	/* XXX could do dynamic resizing down; for now
	 just grow max_ready */
	if (howmany <= max_ready)
		return RCOK;

	h = new HANDLE[howmany];
	if (!h)
		return RC(fcOUTOFMEMORY);

	v = new win32_event_t *[howmany];
	if (!v) {
		delete [] h;
		return RC(fcOUTOFMEMORY);
	}

	for (i = 0; i < max_ready; i++) {
		h[i] = ready[i];
		v[i] = map[i];
	}

	for (; i < howmany; i++) {
		h[i] = INVALID_HANDLE_VALUE;
		v[i] = 0;
	}

	win32_event_t	**vtmp = map;
	HANDLE		*htmp = ready;

	map = v;
	ready = h;
	max_ready = howmany;

	delete [] vtmp;
	delete [] htmp;

	return RCOK;
}


/*
 * Attach an event to the event handler.
 */

w_rc_t win32_event_handler_t::start(win32_event_t &hdl)
{
	if (hdl.handle == INVALID_HANDLE_VALUE)
		return RC(sthread_t::stBADFD);

	// already enabled?
	void *owner = hdl._link.member_of();
	if (owner) {
		/* enabled by a different handler?! */
		if (owner != &_list)
			return RC(sthread_t::stINUSE);
		return RCOK;
	}
#ifdef CACHE_READY
	/* Don't do caching yet */
	/* need more slots? */
	if (num_started == max_ready) {
		w_rc_t	e;
		e = resize(max_ready + alloc_chunk);
		if (e)
			return e;
	}
#else
	/* Always keep enough resource around for all
	   handles to generate events */
	if (num_ready == max_ready) {
		w_rc_t	e;
		e = resize(max_ready + alloc_chunk);
		if (e)
			return e;
	}
#endif
	num_started++;
	num_ready++;

	_list.append(&hdl);
	hdl._owner = this;

	return RCOK;
}


/*
 * Detach an event from the event handler.
 */

void win32_event_handler_t::stop(win32_event_t &hdl)
{
	void	*owner = hdl._link.member_of();
	if (!owner)
		return;
	else if (owner != &_list) {
		W_FORM(cerr)("win32_event_handler_t(%#lx): handle %#lx doesn't belong to me!\n",
			  (long) this, (long) &hdl);
		return;
	}

	hdl.disable();
	hdl._link.detach();
	hdl._owner = 0;

	num_started--;
	num_ready--;
}

/*
 * ENABLE at attached event to receive events
 */

void win32_event_handler_t::enable(win32_event_t &hdl)
{
#ifdef CACHE_READY
	if (hdl._slot != -1)
		return;

	ready[num_ready] = hdl.handle;
	map[num_ready] = &hdl;
	hdl._slot = num_ready;

	num_ready++;
#else
	if (hdl.enabled())
		return;
	num_enabled++;
#endif
}


/*
 * DISABLE an attached event to receive events
 */

void win32_event_handler_t::disable(win32_event_t &hdl)
{
#ifdef CACHE_READY
	int	slot = hdl._slot;
	if (slot == -1)
		return;

	hdl._slot = -1;

	if (slot != num_ready - 1) {
		/* swap the last element into the vacated slot */
		ready[slot] = ready[num_ready-1];
		map[slot] = map[num_ready-1];
		map[slot]->_slot = slot;
	}

	ready[num_ready-1] = INVALID_HANDLE_VALUE;
	map[num_ready-1] = 0;

	num_ready--;
#else
	if (!hdl.enabled())
		return;
	num_enabled--;
#endif
}
	


/*
 *  win32_event_handler_t::prepare(timeout)
 *
 *  Prepare the sfile to wait on file events for timeout milliseconds.
 */

w_rc_t win32_event_handler_t::prepare(const stime_t &timeout, bool forever)
{
	_timeout_forever = forever;
	if (_timeout_forever)
		_timeout = INFINITE;
	else if (timeout == stime_t(0))
		_timeout = 0;
	else {
		_timeout = timeout.msecs();

		/* round up the sleep interval to the minimal sleep
		   interval allowed by the Win32 interface. */
		if (_timeout == 0)
			_timeout = 1;
	}

	
#ifndef CACHE_READY
	w_list_i<win32_event_t> i(_list);
	win32_event_t *p;

	unsigned num_waiting = 0;

	w_assert1(num_enabled <= max_ready);

	while ((p = i.next())) {
		win32_event_t	&hdl = *p;
		if (!hdl.enabled())
			continue;

		ready[num_waiting] = hdl.handle;
		map[num_waiting] = &hdl;
		num_waiting++;
	}


	if (num_waiting != num_enabled) {
		cerr << "** warn: " << num_waiting << " waiters, expect "
			<< num_enabled << " enabled" << endl;
		W_FATAL(fcINTERNAL);
	}
#endif

	num_fired = 0;
	using_mux = false;

	if (use_mux == 0 && num_waiting > MAXIMUM_WAIT_OBJECTS) {
		cerr << "Warning: Maximum Wait Objects of "
			<< MAXIMUM_WAIT_OBJECTS 
			<< " exceeded, waiting for " << num_waiting
			<< "." << endl;
	}

	/* Dynamically enable use of the MUX when number of events is high. */
	if (force_mux || (use_mux && num_enabled > MAXIMUM_WAIT_OBJECTS))
		using_mux = true;

	/* seperate from above to seperate command from control */
	if (using_mux) {
		w_rc_t	e = distribute_events();
		if (e != RCOK)
			return e;
	}
		

#ifdef EXPENSIVE_STATS
	/* XXXX lots cheaper if stats allowed arrays */
	switch (num_waiting) {
	case 0:
		SthreadStats.selw0++;
		break;
	case 1:
		SthreadStats.selw1++;
		break;
	case 2:
		SthreadStats.selw2++;
		break;
	case 3:
		SthreadStats.selw3++;
		break;
	case 4:
		SthreadStats.selw4++;
		break;
	default:
		SthreadStats.selwX++;
		break;
	}
#endif

	return RCOK;
}


/*
 *  win32_event_handler_t::wait()
 *
 *  Wait for any file events or for interrupts to occur.
 */
w_rc_t win32_event_handler_t::wait()
{
	DWORD	w;
	int	bw;

	SthreadStats.selects++;

	/* Win32 doesn't handle 0 objects with a timeout correctly */
	if (num_enabled == 0) {
		Sleep(_timeout);
		return RC(sthread_base_t::stTIMEOUT);
	}

	if (using_mux)
		return muxwait();

	w = WaitForMultipleObjects(num_enabled,
				   ready,
				   FALSE,
				   _timeout);

	if (w == WAIT_FAILED)
		return RC(fcWIN32);

	if (w == WAIT_TIMEOUT)
		return RC(sthread_base_t::stTIMEOUT);

	/* For now we treat abandonment as firing.  For long term issues,
	   it might be good to remember abandoned waits, or add an 
	   exception dispatch handler. */

	if (w >= WAIT_ABANDONED_0 && w < WAIT_ABANDONED_0 + num_enabled)
		bw = WAIT_ABANDONED_0;
	else if (w >= WAIT_OBJECT_0 && w < WAIT_OBJECT_0 + num_enabled)
		bw = WAIT_OBJECT_0;
	else {
		w_rc_t	e = RC(fcWIN32);
		cerr << "** Unexpected WaitForMultipleObjects code " <<
			w << ":" << endl;
		return e;
	}

	unsigned	last_fired = w - bw;
	w_assert3(last_fired < num_enabled);

	which_fired[num_fired++] = map[last_fired];

	unsigned	threshold = use_rewait;
	unsigned	remaining = num_enabled - 1;

	if (num_fired < max_fired && remaining && remaining > threshold) {
		/* Compact the event list and map, if needed */
		if (last_fired != remaining) {
			ready[last_fired] = ready[remaining];
			map[last_fired] = map[remaining];
		}

		w_rc_t		e;
		unsigned	additional = 0;
		e = harvest(ready, map, remaining, threshold,
			    which_fired+1, max_fired-1, additional);
		W_IGNORE(e);
		num_fired += additional;
	}

	SthreadStats.selfound++;

	return RCOK;
}

w_rc_t	win32_event_handler_t::muxwait()
{
	bool		ok;
	DWORD		w = 0;
	int		bw = 0;
	unsigned	i;

	for (i = 0; i < num_ready_muxes; i++)
		W_COERCE(muxes[i]->request(eventMux::MUXwait));

	w = WaitForMultipleObjects(num_ready_muxes, ready_mux,
				   FALSE, _timeout);

	int	wakened = -1;

	/* See if a valid wakeup occured */
	if (w >= WAIT_ABANDONED_0 && w < WAIT_ABANDONED_0 + num_ready_muxes)
		wakened = w - WAIT_ABANDONED_0;
	else if (w >= WAIT_OBJECT_0 && w < WAIT_OBJECT_0 + num_ready_muxes)
		wakened = w - WAIT_OBJECT_0;

	/* XXX error handling needs to be fixed here */

	/* XXX could do background waiting if we added all the SYNC.
	   to add/drop a waiter. */

	/* Tell each mux that didn't find anything to stop waiting. */
	/* XXX but what if they did find something and we haven't seen
		the event yet.  That screws up the protocol. */
	for (i = 0; i < num_ready_muxes; i++) {
		/* Don't bother it if it woke us up */
		if (wakened != -1 && i == (unsigned) wakened)
			continue;

		ok = SetEvent(muxes[i]->stopwait);
		if (!ok) {
			w_rc_t	e = RC(fcWIN32);
			cerr << "SetEvent():" << endl << e << endl;
			W_COERCE(e);
		}
	}

	/* And now wait for everyone else to be done */
	unsigned	remaining = num_ready_muxes;
	if (wakened != -1) {
		if ((unsigned)wakened != remaining-1)
			ready_mux[wakened] = ready_mux[remaining-1];
		remaining--;
	}

	if (remaining) {
//		cout << "waiting for " << remaining << " muxes." << endl;
		DWORD	n;
		/* wait for all mux objects not signalled to signal */
		n = WaitForMultipleObjects(remaining, ready_mux,
					   TRUE, INFINITE);
		if (!((n >= WAIT_OBJECT_0 && n < WAIT_OBJECT_0 + remaining)
			|| (n >= WAIT_ABANDONED_0 && n < WAIT_ABANDONED_0+remaining))) {
			w_rc_t	e = RC(fcWIN32);
			cerr << "WaitForSingleObject():" << endl << e << endl;
			W_COERCE(e);
		}
		remaining--;
	}

	if (w == WAIT_FAILED)
		return RC(fcWIN32);

	if (w == WAIT_TIMEOUT)
		return RC(sthread_base_t::stTIMEOUT);

	for (i = 0; i < num_ready_muxes; i++) {
		unsigned	fired;
		if (num_fired + muxes[i]->num_fired > max_fired) {
			cerr << "TOO MANY EVENTS FIRED" 
				<< " max_fired " << max_fired
				<< " num_fired " << num_fired
				<< " mux[" << i << "].num_fired " << muxes[i]->num_fired
				<< endl;
			W_FATAL(fcINTERNAL);
		}
		fired = muxes[i]->unmap_events(which_fired + num_fired);
		num_fired += fired;
	}

	SthreadStats.selfound++;

	return RCOK;
}


/*
 *  Dispatch events to individual requesting handles.
 *
 *  Events are processed in order of priority.  At current, only two
 *  priorities, "read"==0 and "write"==1 are supported, rather crudely
 */

void win32_event_handler_t::dispatch(const stime_t &at)
{
	/* any events to dispatch? */
	if (num_fired == 0)
		return;

#if 1
	if (event_debug && num_fired >= event_debug) {
		cout << "WIN32: " << num_fired 
			<< " of " << num_enabled << " events:";
		for (unsigned i = 0; i < num_fired; i++)
			cout << ' ' << (int)which_fired[i]->handle;

		cout << endl;
	}
#endif

#ifdef EXPENSIVE_STATS
	/* XXXX lots cheaper if stats allowed arrays */
	switch (num_fired) {
	case 1:
		SthreadStats.self1++;
		break;
	case 2:
		SthreadStats.self2++;
		break;
	case 3:
		SthreadStats.self3++;
		break;
	case 4:
		SthreadStats.self4++;
		break;
	default:
		SthreadStats.selfX++;
		break;
	}
#endif

	_hits++;
	_last_event = at;

	for (unsigned i = 0; i < num_fired; i++)
		which_fired[i]->dispatch(at);
}


bool	win32_event_handler_t::probe(win32_event_t &hdl)
{
	DWORD	what;
	bool	huh = false;
	w_rc_t	e;

	if (hdl.handle == INVALID_HANDLE_VALUE) {
		cerr << "PROBE with invalid handle " << hdl.handle << endl;
		return huh;
	}

	what = WaitForSingleObject(hdl.handle, 0);

	switch (what) {
	case WAIT_FAILED:
		return RC(fcWIN32);
		break;
	case WAIT_ABANDONED:
		cerr << "probe:: WaitForSingleObject(): abandonded" << endl;
		/* normal semantics */
		/*FALLTHROUGH*/
	case WAIT_OBJECT_0:
		huh = true;
		break;
	case WAIT_TIMEOUT:
		cerr << "probe:: WaitForSingleObject(): timeout" << endl;
		break;
	default:
		e = RC(fcWIN32);
		cerr << "probe:: WaitForSingleObject():" << endl
			<< e << endl;
		break;
	}

	return huh;
}

/*
 * Look for any other events that may be ready in the
 * specified ready list.  The ready list and the map is mulilated
 * as events are found to make harvest()'s work easier.
 */
w_rc_t win32_event_handler_t::harvest(HANDLE *ready, win32_event_t **map,
				      unsigned count, unsigned threshold,
				      win32_event_t **which_fired,
				      unsigned max_fired,
				      unsigned	&ret_num_fired)
{
	unsigned	num_fired = 0;
	unsigned	last_fired = 0;	/* XXX invalid until set */
	bool		try_again = true;
	unsigned	remaining = count;
	DWORD		w;
	w_rc_t		e;

	ret_num_fired = 0;

	while (num_fired < max_fired && try_again &&
		remaining && remaining >= threshold) {

		/* Compact the event list and map, if needed */
		if (num_fired > 0 && last_fired != remaining) {
			ready[last_fired] = ready[remaining];
			map[last_fired] = map[remaining];
		}

		w = WaitForMultipleObjects(remaining, ready, FALSE, 0);
		if (w == WAIT_FAILED) {
			e = RC(fcWIN32);
			cerr << "Ignoring secondary wait failure:"
				<< endl << e << endl;
			try_again = false;
		}
		else if (w == WAIT_TIMEOUT)
			try_again = false;
		else if (w >= WAIT_OBJECT_0 && w < WAIT_OBJECT_0+remaining) {
			last_fired = w - WAIT_OBJECT_0;
			which_fired[num_fired++] = map[last_fired];
		}
		else if (w >= WAIT_ABANDONED_0 && w < WAIT_ABANDONED_0+remaining) {
			last_fired = w - WAIT_ABANDONED_0;
			which_fired[num_fired++] = map[last_fired];
		}
		else {
			e = RC(fcWIN32);
			cerr << "** Unexpected WaitForMultipleObjects code " <<
				w << ":" << endl << e << endl;
			cout << "WAIT_OBJECT_0 " << WAIT_OBJECT_0
				<< " WAIT_ABANDONED_0 " << WAIT_ABANDONED_0
				<< " remaining " << remaining << endl;
			try_again = false;
		}
			
		/* Doesn't matter if goes west, try_again supersedes */
		remaining--;
	}
	ret_num_fired = num_fired;
	return	e;
}

w_rc_t	win32_event_handler_t::distribute_events()
{
	/* XXX figure out how many muxes are running, or control that */
	if (num_enabled == 0 || use_mux == 0) {
		/* XXX or don't need mux events */
		num_ready_muxes = 0;
		return RCOK;
	}

	/* XXX must do a better job of controlling muxes */
	unsigned	howmany = (num_enabled + use_mux-1) / use_mux;
	if (howmany > max_mux)
		howmany = max_mux;
		
	/* XXX if can't resize, use what is available, if none, die */
	if (howmany > num_muxes)
		W_COERCE(resize_mux(howmany));

	unsigned	needed = (num_enabled + use_mux -1) / use_mux;
	unsigned	permux = use_mux;
	if (needed > max_mux) {
#if 0
		cout << "rehash mux count=" << needed << ", " << permux
			<< " /mux";
#endif
		permux = (num_enabled + max_mux -1) / max_mux;
		needed = (num_enabled + permux -1) / permux;
#if 0
		cout << " TO " << needed << ", " << permux << "/mux" << endl;
#endif
	}
	unsigned	i;
	unsigned	remaining = num_enabled;
	unsigned	next = 0;

	w_assert1(needed <= max_mux);

	/* XXX if the division is really bizzare, must carry-over
	   entries so that everything fits.  Need to keep better track
	   of everything here.   Think about it */

	for (i = 0; i < needed; i++) {
		if (!muxes[i])
			W_FATAL(fcINTERNAL);
		eventMux	&mux = *muxes[i];

		unsigned	howmany = (remaining < permux) ?
						remaining : permux;

		if (remaining == 0)
			cout << "Warning: mux " << i << " remaining " << remaining << endl;

		mux.start = ready + next;
		mux.count = howmany;
		mux.start_index = next;
		mux.map = map + next;

		next += howmany;
		remaining -= howmany;

		ready_mux[i] = mux.gen;
	}
	if (remaining) {
		cout << "Give " << remaining << " extra events to " 
			<< needed-1 << endl;
		eventMux	&mux = *muxes[needed-1];
		mux.count += remaining;
	}
	num_ready_muxes = needed;

	return RCOK;
}


w_rc_t	win32_event_handler_t::resize_mux(unsigned howmany)
{
	if (howmany > max_mux)
		return RC(fcINTERNAL);

	if (howmany <= num_muxes)
		return RCOK;

	unsigned	mux;
	for (mux = num_muxes; mux < howmany; mux++)
		if (!muxes[mux])
			W_COERCE(start_mux_thread(mux));
	
	num_muxes = howmany;

	return RCOK;
}


w_rc_t	win32_event_handler_t::start_mux_thread(const unsigned which_mux)
{
	if (which_mux >= max_mux)
		return RC(fcINTERNAL);

#if 0
	cerr << "start_mux_thread(" << which_mux << ") ... ";
#endif

	eventMux	*muxp;
	w_rc_t		e;

	e = eventMux::newEventMux(muxp, *this, which_mux);
	if (e != RCOK)
		return e;

	muxes[which_mux] = muxp;
	eventMux	&mux = *muxp;

	W_COERCE(mux.fork());

#if 0
	cerr << "MUX thread " << mux.which_mux << " started" << endl;
#endif

	return RCOK;
}



w_rc_t	win32_event_handler_t::stop_mux_thread(const unsigned which_mux)
{
	if (which_mux >= max_mux)
		return RC(fcINTERNAL);

	eventMux	&mux = *muxes[which_mux];

	W_COERCE(mux.stop());

	delete &mux;
	muxes[which_mux] = 0;

	return RCOK;
}

unsigned win32_event_handler_t::eventMux::unmap_events(win32_event_t **which)
{
	for (unsigned i = 0; i < num_fired; i++)
		which[i] = which_fired[i];

	return num_fired;
}


/* Prepare our set of events for querying */
void	win32_event_handler_t::eventMux::prepare()
{
	/* Wait on all the events we are supposed to. */
	if (count == 0)
		cout << "mux count 0!" << endl;

	memcpy(ready, start, count * sizeof(HANDLE));

	/* Add our wakeup event to stop waiting */
	ready[count] = stopwait;

	num_fired = 0;
}


bool	win32_event_handler_t::eventMux::wait()
{
	DWORD	w;
	bool	ok;
	w_rc_t	e;

	w = WaitForSingleObject(wakeup, INFINITE);
	if (w != WAIT_OBJECT_0) {
		e = RC(fcWIN32);
		cerr << "Win32 event mux: WaitForSingleObject():"
			<< endl << e << endl;
		W_COERCE(e);
	}

	if (op != MUXwait)
		cerr << "MUX thread " << which_mux << " wakeup, op="
			<< (int) op << endl;

#if 0
	static unsigned calls[10];
	if (calls[which_mux] && (calls[which_mux] % 10) == 0)
		cerr << "MUX thread " << which_mux << " wakeup, op="
			<< (int) op << " call=" << calls[which_mux] << endl;
	calls[which_mux]++;
#endif

	/* XXX the op isn't reset until return, so that print methods
	   can say what the mux is doing. */

	if (op == MUXdone) {
		/* master waiting for thread exit as ACK signal */
		op = MUXnone;
		return false;
	}

	prepare();


	w = WaitForMultipleObjects(count+1, ready, FALSE, INFINITE);
	switch (w) {
	case WAIT_FAILED:
		/* XXX could propogate failure to handler */
		e = RC(fcWIN32);
		cerr << "Win32 event mux: WaitForMultipleObjects():"
			<< endl << e << endl;
		W_COERCE(e);
		break;

	case WAIT_TIMEOUT:
		/* reset the op */
		op = MUXnone;

		/* Tell the handler we are done. */
		ok = ReleaseSemaphore(gen, 1, NULL);
		if (!ok) {
			e = RC(fcWIN32);
			cerr << "Win32 event mux: release semaphore fails:"
				<< endl << e << endl;
			W_COERCE(e);
		}

		return true;
		break;
	}

	unsigned	bw;

	/* Treat abandonment as firing; maybe add an exception handler? */
	if (w >= WAIT_ABANDONED_0 && w < WAIT_ABANDONED_0 + count+1)
		bw = WAIT_ABANDONED_0;
	else if (w >= WAIT_OBJECT_0 && w < WAIT_OBJECT_0 + count+1)
		bw = WAIT_OBJECT_0;
	else {
		e = RC(fcWIN32);
		cerr << "** Unexpected WaitForMultipleObjects code "
			<< w << ":" << endl << e << endl;
		W_COERCE(e);
	}

	unsigned	which = w - bw;

	/* If ours, master is telling us to stop waiting, nothing happened */
	if (which != count) {
		unsigned	last_fired = w - bw;
		unsigned	remaining = count-1;

		which_fired[num_fired++] = map[last_fired];

		if (remaining > 0 && num_fired < max_fired) {
			/* Compact the event list and map, if needed */
			if (last_fired != remaining) {
				ready[last_fired] = ready[remaining];
				map[last_fired] = map[remaining];
			}

			unsigned	additional = 0;

			w_rc_t	e;
			e = harvest(ready, map, remaining, 1/*threshold*/,
				    which_fired+1, max_fired-1, additional);
			W_COERCE(e);	/* error due to RC allocation */
			num_fired += additional;
		}
	}

	/* reset the op */
	op = MUXnone;

	/* Tell the master we are done. */
	ok = ReleaseSemaphore(gen, 1, NULL);
	if (!ok) {
		e = RC(fcWIN32);
		cerr << "Win32 event mux: release semaphore fails:"
			<< endl << e << endl;
		W_COERCE(e);
	}

	return true;
}

w_rc_t	win32_event_handler_t::eventMux::request(muxOp requestedOp)
{
	bool	ok;

	ok = ResetEvent(stopwait);
	if (!ok) {
		w_rc_t e = RC(fcWIN32);
		cerr << "Win32 event mux: ResetEvent():"
			<< endl << e << endl;
		return e;
	}

	op = requestedOp;

	ok = ReleaseSemaphore(wakeup, 1, NULL);
	if (!ok) {
		w_rc_t	e = RC(fcWIN32);
		cerr << "ReleaseSemaphore():" << endl << e << endl;
		return e;
	}

	return RCOK;
}

w_rc_t	win32_event_handler_t::eventMux::_startup()
{
	/* XXX all this junk belongs to the mux */
	/* XXX magic value 8, needs research, perhaps only 1 or 2? */

	wakeup = CreateSemaphore(NULL, 0, 8, NULL);
	if (wakeup == INVALID_HANDLE_VALUE) {
		w_rc_t	e = RC(fcWIN32);
		cerr << "CreateSemaphore():" << endl << e << endl;
		W_COERCE(e);
	}

	gen = CreateSemaphore(NULL, 0, 8, NULL);
	if (gen == INVALID_HANDLE_VALUE) {
		w_rc_t	e = RC(fcWIN32);
		cerr << "CreateSemaphore():" << endl << e << endl;
		W_COERCE(e);
	}

	/* Can't use semaphore because is statistical, not guaranteed */
	/* Auto reset, Initially not set */

	stopwait = CreateEvent(0, true, false, 0);
	if (stopwait == INVALID_HANDLE_VALUE) {
		w_rc_t	e = RC(fcWIN32);
		cerr << "CreateEvent():" << endl << e << endl;
		W_COERCE(e);
	}

	return RCOK;
}


win32_event_handler_t::eventMux::~eventMux()
{
	/* XXX error returns */
	if (stopwait != INVALID_HANDLE_VALUE) {
		CloseHandle(stopwait);
		stopwait = INVALID_HANDLE_VALUE;
	}
	if (gen != INVALID_HANDLE_VALUE) {
		CloseHandle(gen);
		gen = INVALID_HANDLE_VALUE;
	}
	if (wakeup != INVALID_HANDLE_VALUE) {
		CloseHandle(wakeup);
		wakeup = INVALID_HANDLE_VALUE;
	}
}



w_rc_t	win32_event_handler_t::eventMux::newEventMux(
					eventMux		*&_mux,
					win32_event_handler_t	&h,
					const unsigned		which)
{
	_mux = 0;

	eventMux	*mux;

	mux = new eventMux(h, which);
	if (!mux)
		return RC(fcOUTOFMEMORY);

	w_rc_t	e;

	e = mux->_startup();

	if (e != RCOK)
		delete mux;
	else
		_mux = mux;
	
	return e;
}



void	win32_event_handler_t::eventMux::run()
{
	while (wait())
		;
}


/* Static to transition system to objects */
#if defined(_MT) && !defined(WIN32_THREADS_ONLY)
unsigned
#else
DWORD
#endif
__stdcall	win32_event_handler_t::eventMux::__start(void *_arg)
{
	eventMux	&mux = *(eventMux *) _arg;
	
	mux.run();

	return 0;
}


w_rc_t	win32_event_handler_t::eventMux::fork()
{
	if (thread != INVALID_HANDLE_VALUE) {
		cerr << "eventMux already started" << endl;
		return RC(fcINTERNAL);
	}

#if defined(_MT) && !defined(WIN32_THREADS_ONLY)
	thread = (HANDLE) _beginthreadex(NULL, 0,
		__start, this,
		0, &thread_id);
#else
	thread = CreateThread(NULL, 0,
		__start, this,
		0, &thread_id);
#endif
	if (thread == INVALID_HANDLE_VALUE) {
		w_rc_t	e = RC(fcWIN32);
		cerr << "CreateThread():" << endl << e << endl;
		/* XXX shouldn't be fatal if a fallback is available */
		return e;
	}

	return RCOK;
}

w_rc_t	win32_event_handler_t::eventMux::stop()
{
	if (thread == INVALID_HANDLE_VALUE) {
		cerr << "eventMux stopped" << endl;
		return RC(fcINTERNAL);
	}

	DWORD		what;

	W_COERCE(request(MUXdone));
	
	/* XXX could schedule this as a callback or something, but
	   because we are the idle thread, idling ourself isn't an
	   option. */
	what = WaitForSingleObject(thread, INFINITE);
	if (what != WAIT_OBJECT_0) {
		w_rc_t	e = RC(fcWIN32);
		cerr << "WaitForSingleObject():" << endl << e << endl;
		return e;
	}

	CloseHandle(thread);
	thread = INVALID_HANDLE_VALUE;


	return RCOK;
}


#if 0
/*
 *  win32_event_handler_t::is_active(fd)
 *
 *  Return true if there is an active file handler for fd.
 */

bool win32_event_handler_t::is_active(HANDLE handle)
{
	w_list_i<win32_event_t> i(_list);
	win32_event_t *p; 

	while ((p = i.next()))  {
		if (p->handle == handle)
			break;
	}

	return p != 0;
}
#endif


ostream &win32_event_handler_t::print(ostream &o)
{
	o << "WIN32 EVENTS:" << endl;
	if (_hits)
		o << _hits << " events";

	if (num_muxes)
		o << " " << num_muxes << " muxes";

	if (num_started)
		o << " " << num_started << " started";
	if (num_enabled)
		o << " " << num_enabled << " enabled";

        o << " timeout: ";
	if (_timeout_forever)
		o << "indefinite";
	else if (_timeout == 0)
		o << "poll";
	else
		o << _timeout << " msecs";

	if (_hits) {
		sinterval_t     delta(stime_t::now() - _last_event);
		o << ", last event " << delta << " seconds ago";
	}

	o << endl;
	
	w_list_i<win32_event_t> i(_list);
	win32_event_t *f;

	while ((f = i.next()))
		o << *f << endl;

	return o;
}

ostream &operator<<(ostream &o, win32_event_handler_t &h)
{
	return h.print(o);
}



win32_event_t::win32_event_t(HANDLE ahandle)
: handle(ahandle),
  _enabled(false),
  _slot(-1),
  _hits(0)
{
}    


win32_event_t::~win32_event_t()
{
	if (_link.member_of()) {
		W_FORM(cerr)("win32_event_t(%#lx): destructed in place (%#lx) !!!\n",
			     (long)this, (long)_link.member_of());
		/* XXX how about printing the handler ?? */
		/* XXX try removing it from the list? */
	}
}


/*
 * Allow changes to the file descriptor if the handle isn't in-use.
 */

w_rc_t	win32_event_t::change(HANDLE new_handle)
{
	if (enabled() || running())
		return RC(sthread_t::stINUSE);
	handle = new_handle;
	return RCOK;
}


bool win32_event_t::running() const
{
	return (_link.member_of() != 0);
}


bool win32_event_t::enabled() const
{
	return _enabled;
}


void win32_event_t::enable()
{
	if (enabled())
		return;
	if (!running())
		return;

	_owner->enable(*this);
	_enabled = true;
}


void win32_event_t::disable()
{
	if (!enabled())
		return;
	if (!running())
		return;

	_owner->disable(*this);
	_enabled = false;
}


void win32_event_t::dispatch(const stime_t &at)
{
	_hits++;
	_last_event = at;
	ready();
}


bool	win32_event_t::probe()
{
	return _owner ? _owner->probe(*this) : false;
}


ostream &win32_event_t::print(ostream &s) const
{
	W_FORM(s)("sfile(%#lx handle %#lx, [%sed], %d hits",
		(long)this,
		(long)handle,
		enabled() ? "enabl" : "disabl",
		_hits);
	if (_hits) {
		sinterval_t     delta(stime_t::now() - _last_event);
		s << ", " << delta << " seconds ago";
	}
	return s << ')';
}

ostream &operator<<(ostream &o, win32_event_t &h)
{
	return h.print(o);
}


win32_safe_event_t::win32_safe_event_t(HANDLE handle)
: win32_event_t(handle),
  _shutdown(false)
#ifdef notyet
  , _waiting(false)
#endif
{
	char	buf[20];
	ostrstream	s(buf, sizeof(buf));
	W_FORM(s)("sfile(%#x)", handle);
	s << ends;
#ifndef notyet
	sevsem.setname(buf);
#endif
}


w_rc_t	win32_safe_event_t::change(HANDLE new_handle)
{
	W_DO(win32_event_t::change(new_handle));

	char	buf[20];
	ostrstream	s(buf, sizeof(buf));
	W_FORM(s)("sfile(%#x)", new_handle);
	s << ends;
#ifndef notyet
	sevsem.setname(buf);
#endif

	return RCOK;
}


win32_safe_event_t::~win32_safe_event_t()
{
#ifdef notyet
	/* XXXX hmmmm */
	shutdown();
#endif
} 


w_rc_t 
win32_safe_event_t::wait(long timeout)
{
	if (is_down())
		return RC(sthread_t::stBADFILEHDL);

	// XXX overloaded error code?
	if (!running())
		return RC(sthread_t::stBADFILEHDL);


#ifdef notyet
	/* XXX timeout not implemented correctly */

	W_COERCE(lock.acquire());
	_waiting = true;
	enable();
	while (_waiting)
		W_COERCE(done.wait(lock, timeout ));
	lock.release();

	if (is_down())
		return RC(sthread_t::stBADFILEHDL);
#else
	enable();
	W_DO(sevsem.wait(timeout));

	if (is_down())
		return RC(sthread_t::stBADFILEHDL);

	W_IGNORE(sevsem.reset());
#endif


	return RCOK;
}

/*
 *  win32_safe_event_t::shutdown()
 *
 *  Shutdown this file handler. If a thread is waiting on it, 
 *  wake it up and tell it the bad news.
 */

void win32_safe_event_t::shutdown()
{
#ifdef notyet
	W_COERCE(lock.acquire());
	disable();
	_shutdown = true;
	if (_waiting) {
		_waiting = false;
		done.signal();
	}
	lock.release();
#else
	_shutdown = true;
	W_IGNORE(sevsem.post());
#endif
}

void win32_safe_event_t::ready()
{
#ifdef notyet
	W_COERCE(lock.acquire());
	disable();
	w_assert3(_waiting);
	_waiting = false;
	done.signal();
	lock.release();
#else
	W_IGNORE(sevsem.post());
	disable();
#endif
}


win32_compat_event_t::win32_compat_event_t(HANDLE handle)
: win32_safe_event_t(handle)
{
	w_assert3(handle != INVALID_HANDLE_VALUE);

	W_COERCE(sthread_t::io_start(*this));
}


win32_compat_event_t::~win32_compat_event_t()
{
	W_COERCE(sthread_t::io_finish(*this));
}

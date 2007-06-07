/*<std-header orig-src='shore' incl-file-exclusion='SFILE_HANDLER_H'>

 $Id: sfile_handler.h,v 1.14 2004/10/05 23:19:59 bolo Exp $

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

#ifndef SFILE_HANDLER_H
#define SFILE_HANDLER_H

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
#ifdef _WIN32
#include <sfile_handler_winsock.h>
#else

#ifndef W_H
#include <w.h>
#endif

#include <iosfwd>

#ifdef __GNUC__
#pragma interface
#endif

class sfile_common_t {
public:
	enum { rd = 1, wr = 2, ex = 4 };
};	

class sfile_hdl_base_t;

/* This is an interface class for whatever implementation of
   file handler one needs to use.  The one thing it does
   implement is a list of all file handles known to the handler. */

class sfile_handler_t : public sfile_common_t {
public:
	virtual		~sfile_handler_t() { }

	// Wait for sfile events
	virtual	w_rc_t		prepare(const stime_t &timeout,
					bool no_timeout) = 0;
	virtual	w_rc_t		wait() = 0;
	virtual	void		dispatch(const stime_t &at) = 0;

    	/* A handle requests a low-cost "quick check" of its
	   activity; this allows delay of context switch
	   when data should be immediately available. */
	virtual	bool		probe(sfile_hdl_base_t &) = 0;

	/* Is there a handle active for this descriptor? */
	virtual	bool		is_active(int fd) = 0;

	/* configure / deconfigure handles which can generate events */
	virtual	w_rc_t		start(sfile_hdl_base_t &) = 0;
	virtual	void		stop(sfile_hdl_base_t &) = 0;

	/* enable / disable events for a configured handle */
	virtual	void		enable(sfile_hdl_base_t &) = 0;
	virtual	void		disable(sfile_hdl_base_t &) = 0;

   	/* print isn't const because the iterator doesn't do const */ 
	virtual	ostream		&print(ostream &s) = 0;

protected:
	sfile_handler_t();

	w_list_t<sfile_hdl_base_t> _list;

	w_rc_t	_start(sfile_hdl_base_t &);
	w_rc_t	_stop(sfile_hdl_base_t &);

	/* common statistics gathering */
	static	void	stats_wait(unsigned n);
	static	void	stats_found(unsigned n);
};


class sfile_hdl_base_t : public sfile_common_t {
	friend class sfile_handler_t;

public:
	sfile_hdl_base_t(int fd, int mask);
    	virtual	~sfile_hdl_base_t();

	w_rc_t			change(int fd);
	
	int			fd;
	const int		_mode;

	virtual void		read_ready()	{};
	virtual void 		write_ready()	{};
	virtual void 		exception_ready()	{};

	/* Priority is currently used to help prevent write blocks
	   when a read could happen deadlock. */
	virtual int		priority() { return _mode & wr ? 1 : 0; }

	ostream			&print(ostream &) const;

	/* Is this handle attached to a handler? */
	bool			running() const;

	/* Is this handle enabled to receive events from the handler? */ 
	bool			enabled() const;

    	/* event notification on/off */
	void			enable();
	void			disable();

	/* quick-check for an event */
	bool			probe();

	/* Event[s] for the handle occured; propogate them to a handler. */
	void			dispatch(const stime_t &at,
					 bool r, bool w, bool e);

	/* XXX revectored for compatability */
	static	bool		is_active(int fd);

private:
	w_link_t		_link;
	bool			_enabled;
	sfile_handler_t		*_owner;
	unsigned		_hits;
	stime_t			_last_event;
    
	// disabled
	NORET			sfile_hdl_base_t(const sfile_hdl_base_t&);
	sfile_hdl_base_t&	operator=(const sfile_hdl_base_t&);
};


/* A "safe handle" provides a wrapper on the event mechanism so
   that a thread can wait on the handle for an event to occur. */

class sfile_safe_hdl_t : public sfile_hdl_base_t {
public:
	sfile_safe_hdl_t(int fd, int mode);
	~sfile_safe_hdl_t();

	w_rc_t		change(int fd);
	
	w_rc_t		wait(long timeout);
	void		shutdown();
	bool		is_down()  { return _shutdown; }

protected:
	void		read_ready();
	void		write_ready();
	void		exception_ready();

private:
	bool		_shutdown;
	sevsem_t	sevsem;

	// disabled
	sfile_safe_hdl_t(const sfile_safe_hdl_t&);
	sfile_safe_hdl_t	&operator=(const sfile_safe_hdl_t&);
};


/* Provide backwards compatability for code that is too
   time consuming to change at the moment.  This will eventually
   go away.

   Compatability?  This is a an sfile which will automagically
   start itself at creation and stop itself when destructed.
   ... just like the old one.
 */

class sfile_compat_hdl_t : public sfile_safe_hdl_t {
public:
	sfile_compat_hdl_t(int fd, int mode);
	~sfile_compat_hdl_t();
};


class sfile_read_hdl_t : public sfile_compat_hdl_t {
public:
	sfile_read_hdl_t(int fd) : sfile_compat_hdl_t(fd, rd) { };
};


class sfile_write_hdl_t : public sfile_compat_hdl_t {
public:
	sfile_write_hdl_t(int fd) : sfile_compat_hdl_t(fd, wr) { };
};


extern ostream &operator<<(ostream &o, sfile_handler_t &h);
extern ostream &operator<<(ostream &o, sfile_hdl_base_t &h);

#endif


/*<std-footer incl-file-exclusion='SFILE_HANDLER_H'>  -- do not edit anything below this line -- */

#endif          /*</std-footer>*/

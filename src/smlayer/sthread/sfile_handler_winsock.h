/*<std-header orig-src='shore' incl-file-exclusion='SFILE_HANDLER_WINSOCK_H'>

 $Id: sfile_handler_winsock.h,v 1.11 1999/06/07 19:06:07 kupsch Exp $

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

#ifndef SFILE_HANDLER_WINSOCK_H
#define SFILE_HANDLER_WINSOCK_H

#include "w_defines.h"

/*  -- do not edit anything above this line --   </std-header>*/

#ifdef __GNUC__
#pragma interface
#endif


class sfile_common_t {
public:
#ifdef _WINDOWS
	typedef unsigned int file_descriptor;
#else
	typedef int file_descriptor;
#endif
	enum { rd = 1, wr = 2, ex = 4 };
};	

class sfile_hdl_base_t;


class sfile_hdl_base_t : public w_vbase_t, public sfile_common_t {
	friend class sfile_handler_t;

public:
	sfile_hdl_base_t(file_descriptor fd, int mask);
    	~sfile_hdl_base_t();

	w_rc_t			change(file_descriptor fd);
	
	file_descriptor 	fd;

	virtual void		read_ready()	{};
	virtual void 		write_ready()	{};
	virtual void 		exception_ready()	{};

	virtual int		priority() { return _mode & wr ? 1 : 0; }

	ostream			&print(ostream &) const;
	
	bool			running() const;
	bool			enabled() const;
	void			enable();
	void			disable();

	void			dispatch(bool r, bool w, bool e);

	// A placeholder to let code compile with both old
	// and new event generators.
	bool		probe() const { return false; }

	/* XXX revectored for compatability */
	static	bool		is_active(file_descriptor fd);

private:
	w_link_t		_link;
	const int		_mode;
	bool			_enabled;
	sfile_handler_t		*_owner;

	// disabled
	NORET			sfile_hdl_base_t(const sfile_hdl_base_t&);
	sfile_hdl_base_t&	operator=(const sfile_hdl_base_t&);
};

class sfile_handler_t : public sfile_common_t {
public:
	sfile_handler_t();

	// Wait for sfile events
	w_rc_t		prepare(const stime_t &timeout,
				bool no_timeout);

	w_rc_t		wait();
	void		dispatch(const stime_t &);

	bool		is_active(file_descriptor fd);

	/* configure / deconfigure handles which can generate events */
	w_rc_t		start(sfile_hdl_base_t &);
	void		stop(sfile_hdl_base_t &);

	/* enable / disable events for a configured handle */
	void		enable(sfile_hdl_base_t &);
	void		disable(sfile_hdl_base_t &);

	ostream		&print(ostream &s);

private:
	w_list_t<sfile_hdl_base_t> _list;

	fd_set		masks[3];
	fd_set		ready[3];
	fd_set		*any[3];

	bool		direction;	
	int		rc, wc, ec;
	file_descriptor	dtbsz;

	struct timeval	tval;
	struct timeval	*tvp;
};


class sfile_safe_hdl_t : public sfile_hdl_base_t {
public:
	sfile_safe_hdl_t(file_descriptor fd, int mode);
	~sfile_safe_hdl_t();

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
	sfile_compat_hdl_t(file_descriptor fd, int mode);
	~sfile_compat_hdl_t();
};


class sfile_read_hdl_t : public sfile_compat_hdl_t {
public:
	sfile_read_hdl_t(file_descriptor fd) : sfile_compat_hdl_t(fd, rd) { };
};


class sfile_write_hdl_t : public sfile_compat_hdl_t {
public:
	sfile_write_hdl_t(file_descriptor fd) : sfile_compat_hdl_t(fd, wr) { };
};


extern ostream &operator<<(ostream &o, sfile_handler_t &h);
extern ostream &operator<<(ostream &o, sfile_hdl_base_t &h);

/*<std-footer incl-file-exclusion='SFILE_HANDLER_WINSOCK_H'>  -- do not edit anything below this line -- */

#endif          /*</std-footer>*/

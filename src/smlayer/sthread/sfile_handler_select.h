/*<std-header orig-src='shore' incl-file-exclusion='SFILE_HANDLER_SELECT_H'>

 $Id: sfile_handler_select.h,v 1.7 1999/06/07 19:06:07 kupsch Exp $

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

#ifndef SFILE_HANDLER_SELECT_H
#define SFILE_HANDLER_SELECT_H

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

class sfile_handler_select_t : public sfile_handler_t {
public:
	sfile_handler_select_t();

	// Wait for sfile events
	w_rc_t		prepare(const stime_t &timeout,
				bool no_timeout);

	w_rc_t		wait();
	void		dispatch(const stime_t &at);

	bool		probe(sfile_hdl_base_t &);

	bool		is_active(int fd);

	/* configure / deconfigure handles which can generate events */
	w_rc_t		start(sfile_hdl_base_t &);
	void		stop(sfile_hdl_base_t &);

	/* enable / disable events for a configured handle */
	void		enable(sfile_hdl_base_t &);
	void		disable(sfile_hdl_base_t &);

	ostream		&print(ostream &s);

private:
	fd_set		masks[3];
	fd_set		ready[3];
	fd_set		*any[3];

	unsigned	_hits[3];

	int		direction;	
	int		rc, wc, ec;
	int		dtbsz;

	struct timeval	tval;
	struct timeval	*tvp;

	stime_t		last_event;
};

/*<std-footer incl-file-exclusion='SFILE_HANDLER_SELECT_H'>  -- do not edit anything below this line -- */

#endif          /*</std-footer>*/

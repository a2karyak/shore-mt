/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

/*
 *  $Id: restart.h,v 1.13 1996/03/19 23:14:38 nhall Exp $
 */
#ifndef RESTART_H
#define RESTART_H

class dirty_pages_tab_t;

#ifdef __GNUG__
#pragma interface
#endif

#include <restart_s.h>

class restart_m : public smlevel_3 {
public:
    NORET			restart_m()	{};
    NORET			~restart_m()	{};

    static void 		recover(lsn_t master);

private:
    static void 		analysis_pass(
	lsn_t 			    master,
	dirty_pages_tab_t& 	    ptab, 
	lsn_t& 			    redo_lsn
	);

    static void 		redo_pass(
	lsn_t 			    redo_lsn, 
	const lsn_t 	            &highest,  /* for debugging */
	dirty_pages_tab_t& 	    ptab);

    static void 		undo_pass();
};

#endif /*RESTART_H*/

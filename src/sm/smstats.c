/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

/*
 *  $Header: /p/shore/shore_cvs/src/sm/smstats.c,v 1.4 1995/04/24 19:38:09 zwilling Exp $
 */
#include <copyright.h>
#include <w_statistics.h>
#include "smstats.h"
#include "sm_stats_info_t_op.i"

// the strings:
const char *sm_stats_info_t ::stat_names[] = {
#include "sm_stats_info_t_msg.i"
};

// see sm.c for void sm_stats_info_t::compute()

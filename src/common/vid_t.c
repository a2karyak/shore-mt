/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

/*
 *  $Header: /p/shore/shore_cvs/src/common/vid_t.c,v 1.6 1995/04/24 19:29:01 zwilling Exp $
 */

#define VID_T_C

#ifdef __GNUC__
#pragma implementation
#endif

#include <stdlib.h>
#include <sys/types.h>
#include <stream.h>
#include "w_base.h"
#include "basics.h"
#include "vid_t.h"

const vid_t vid_t::null;

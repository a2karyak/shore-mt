/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

#ifdef __GNUG__
#if defined(AIX41) || defined(AIX32)
#pragma implementation "aix_tid_t.h"
#else
#pragma implementation "tid_t.h"
#endif
#endif

#include <ostream.h>
#include "basics.h"
#include "tid_t.h"

const tid_t tid_t::null(0, 0);
const tid_t tid_t::max(tid_t::hwm, tid_t::hwm);

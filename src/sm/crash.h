/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994,95,96 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */
#ifndef _CRASH_H_
#define _CRASH_H_
#ifdef DEBUG
extern "C" {
    void crashtest(log_m *, const char *c, const char *file, int line) ;
};

#define CRASHTEST(x) crashtest(log,x,__FILE__,__LINE__)


#else /* DEBUG */

#define CRASHTEST(x)

#endif /*DEBUG*/
#endif /*_CRASH_H_*/

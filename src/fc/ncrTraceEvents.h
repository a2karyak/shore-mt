/*<std-header orig-src='shore' incl-file-exclusion='NCRTRACEEVENTS_H'>

 $Id: ncrTraceEvents.h,v 1.12 1999/08/25 01:09:31 kupsch Exp $

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

#ifndef NCRTRACEEVENTS_H
#define NCRTRACEEVENTS_H

#include "w_defines.h"

/*  -- do not edit anything above this line --   </std-header>*/


#ifdef USE_EXTERNAL_TRACE_EVENTS

#include "piextern.h"
#include "tormsg_gen.h"

#define TOR_TRACE(x) do {x;} while (0)
#define TOR_EVENT(x) do {x;} while (0)
#define PARA_EVENT(x)

void
ncr_trace(
    const char*			message,
    const char*			file,
    unsigned int		line,
    EventContext::Categories	category = EventContext::basic,
    const char*			func = ""
);

void
ncr_event1(
    const char*			message,
    const char*			file,
    unsigned int		line,
    EventCode			eCode,
    const char*			func = ""
);

void
ncr_crash(
    const char*			message,
    const char*			file,
    unsigned int		line,
    EventCode			eCode = torUnknownFatalError,
    const char*			func = ""
);

void 
ncr_kill_nomem(void);

#ifdef W_COERCE
#undef W_COERCE
#define W_COERCE(x)					\
do {							\
    w_rc_t e__ = (x);					\
    if (e__)  {						\
	RC_AUGMENT(e__);				\
	ostrstream os;					\
	os << e__;					\
	os << ends;					\
	ncr_crash(os.str(), __FILE__, __LINE__, torSmFatalError);	\
	delete [] os.str();				\
    }							\
} while (0)
#endif

#ifdef W_PRINT_ASSERT
#undef W_PRINT_ASSERT
#define W_PRINT_ASSERT(m)				\
do {							\
    ostrstream os;					\
    os m << ends;					\
    ncr_event1(os.str(), __FILE__, __LINE__, torWAssert);	\
    delete[] os.str()					\
}  while (0)
#endif

#else

#define TOR_TRACE(x)
#define TOR_EVENT_CONTEXT(x)
#define TOR_EVENT_MEMBER_CONTEXT(x)
#define TOR_EVENT(x)
#define PARA_EVENT(x) do {*ERROR_LOG x << endl;} while (0)

#endif


/*<std-footer incl-file-exclusion='NCRTRACEEVENTS_H'>  -- do not edit anything below this line -- */

#endif          /*</std-footer>*/

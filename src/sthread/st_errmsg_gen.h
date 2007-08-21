#ifndef ST_ERRMSG_GEN_H
#define ST_ERRMSG_GEN_H

/* DO NOT EDIT --- generated by ../../tools/errors.pl from st_error.dat
                   on Tue Aug 21 15:07:56 2007 

<std-header orig-src='shore' genfile='true'>

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
static char* st_errmsg[] = {
/* stTIMEOUT                 */ "Timed out waiting for resource",
/* stSEMBUSY                 */ "Semaphore busy",
/* stSEMPOSTED               */ "Semaphore already posted",
/* stSEMALREADYRESET         */ "Semaphore was already reset",
/* stNOTHREAD                */ "No active thread",
/* stBADFD                   */ "Bad file descriptor for I/O operation, seek, or close",
/* stBADIOVCNT               */ "Bad count for io vector (too high)",
/* stTOOMANYOPEN             */ "Too many open files ",
/* stBADADDR                 */ "Bad offset in shared memory segment",
/* stBADFILEHDL              */ "Attempt to wait on file handle that is down",
/* stINVAL                   */ "Invalid argument to thread I/O operation",
/* stINUSE                   */ "Resource in use",
/* stSHORTIO                 */ "Short I/O",
/* stSHORTSEEK               */ "Short Seek",
/* stINVALIDIO               */ "Invalid I/O operation",
/* stEOF                     */ "End of File",
	"dummy error code"
};

const st_msg_size = 15;

#endif

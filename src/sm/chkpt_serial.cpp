/*<std-header orig-src='shore'>

 $Id: chkpt_serial.cpp,v 1.7 1999/06/07 19:03:58 kupsch Exp $

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

#define SM_SOURCE
#define CHKPT_SERIAL_C

#ifdef __GNUG__
#   pragma implementation
#endif


#include <sm_int_0.h>
#include <chkpt_serial.h>


/*********************************************************************
 *
 *  Fuzzy checkpoints and prepares cannot be inter-mixed in the log.
 *  This mutex is for serializing them.
 *
 *********************************************************************/
smutex_t                      chkpt_serial_m::_chkpt_mutex("_xct_chkpt");

void
chkpt_serial_m::chkpt_mutex_release()
{
    FUNC(chkpt_m::chkpt_mutex_release);
    DBGTHRD(<<"done logging a prepare");
    _chkpt_mutex.release();
}

void
chkpt_serial_m::chkpt_mutex_acquire()
{
    FUNC(chkpt_m::chkpt_mutex_acquire);
    DBGTHRD(<<"wanting to log a prepare...");
    W_COERCE(_chkpt_mutex.acquire());
    DBGTHRD(<<"may now log a prepare...");
}


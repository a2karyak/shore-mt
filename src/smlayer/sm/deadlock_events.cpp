/*<std-header orig-src='shore'>

 $Id: deadlock_events.cpp,v 1.17 2000/02/02 03:57:28 bolo Exp $

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
#include <sm_int_1.h>

#include <w_strstream.h>

#ifdef EXPLICIT_TEMPLATE
template class w_list_t<GtidElem>;
#endif

W_FASTNEW_STATIC_DECL(GtidElem, 64);

void
DeadlockEventPrinterBase::PrintLocalDeadlockDetected(
	ostream&		os,
	XctWaitsForLockList&	waitsForList,
	const xct_t*		current,
	const xct_t*		victim
)
{
    os	<< "LOCAL DEADLOCK DETECTED:\n"
    	<< " CYCLE:\n";
    bool isFirstElem = true;
    XctWaitsForLockElem* elem; 
    while ((elem = waitsForList.pop()))  {
        char tidBuffer[80];
        ostrstream tidStream(tidBuffer, 80);
        tidStream << elem->xct->tid() << ends;
        W_FORM2(os, ("  %7s%15s waits for ", isFirstElem ? "" : "held by", tidBuffer));
	os << elem->lockName << '\n';
        isFirstElem = false;
        delete elem;
    }
    os	<< " XCT WHICH CAUSED CYCLE:  " << current->tid() << '\n'
	<< " SELECTED VICTIM:         " << victim->tid() << '\n';
}

void
DeadlockEventPrinterBase::PrintKillingGlobalXct(ostream& os, const xct_t* xct, const lockid_t& lockid)
{
    os	<< "GLOBAL DEADLOCK DETECTED:\n"
	<< " KILLING local xct " << xct->tid() << " (global id " << *xct->gtid()
	<< " was holding lock " << lockid << '\n';
}

void
DeadlockEventPrinterBase::PrintGlobalDeadlockDetected(ostream& os, GtidList& list)
{
    os	<< "GLOBAL DEADLOCK DETECTED:\n"
	<< " PARTICIPANT LIST:\n";
    GtidElem* gtidElem;
    while ((gtidElem = list.pop()))  {
	os << "  " << gtidElem->gtid << '\n';
	delete gtidElem;
    }
}

void
DeadlockEventPrinterBase::PrintGlobalDeadlockVictimSelected(ostream& os, const gtid_t& gtid)
{
    os	<< "GLOBAL DEADLOCK DETECTED:\n"
	<< " SELECTING VICTIM: " << gtid << '\n';
}

void
DeadlockEventPrinter::LocalDeadlockDetected(XctWaitsForLockList& waitsForList, const xct_t* current, const xct_t* victim)
{
    PrintLocalDeadlockDetected(out, waitsForList, current, victim);
}

void
DeadlockEventPrinter::KillingGlobalXct(const xct_t* xct, const lockid_t& lockid)
{
    PrintKillingGlobalXct(out, xct, lockid);
}

void
DeadlockEventPrinter::GlobalDeadlockDetected(GtidList& list)
{
    PrintGlobalDeadlockDetected(out, list);
}

void
DeadlockEventPrinter::GlobalDeadlockVictimSelected(const gtid_t& gtid)
{
    PrintGlobalDeadlockVictimSelected(out, gtid);
}

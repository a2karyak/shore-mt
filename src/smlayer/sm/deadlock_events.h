/*<std-header orig-src='shore' incl-file-exclusion='DEADLOCK_EVENTS_H'>

 $Id: deadlock_events.h,v 1.13 1999/06/07 19:04:00 kupsch Exp $

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

#ifndef DEADLOCK_EVENTS_H
#define DEADLOCK_EVENTS_H

#include "w_defines.h"

/*  -- do not edit anything above this line --   </std-header>*/

#ifdef __GNUG__
#pragma interface
#endif

class XctWaitsForLockElem
{
    public:
	const xct_t*	xct;
	lockid_t	lockName;
	w_link_t	_link;

			XctWaitsForLockElem(const xct_t* theXct, const lockid_t& name);
			~XctWaitsForLockElem();
	static uint4_t	link_offset();

	W_FASTNEW_CLASS_DECL(XctWaitsForLockElem);
};

inline
XctWaitsForLockElem::XctWaitsForLockElem(const xct_t* theXct, const lockid_t& name)
:
    xct(theXct),
    lockName(name)
{
}

inline
XctWaitsForLockElem::~XctWaitsForLockElem()
{
    if (_link.member_of() != 0)  {
	_link.detach();
    }
}

inline uint4_t
XctWaitsForLockElem::link_offset()
{
    return offsetof(XctWaitsForLockElem, _link);
}

typedef w_list_t<XctWaitsForLockElem> XctWaitsForLockList;
typedef w_list_i<XctWaitsForLockElem> XctWaitsForLockListIter;

class GtidElem  {
    public:
	gtid_t			    gtid;
	w_link_t		    _link;

	NORET			    GtidElem(gtid_t g)
					: gtid(g)
					{};
	NORET			    ~GtidElem()
					{
					    if (_link.member_of() != NULL)
						_link.detach();
					};
	static uint4_t		    link_offset()
					{
					    return offsetof(GtidElem, _link);
					};
	W_FASTNEW_CLASS_DECL(GtidElem);
};

typedef w_list_t<GtidElem> GtidList;
typedef w_list_i<GtidElem> GtidListIter;

class DeadlockEventCallback
{
    public:
	virtual void LocalDeadlockDetected(
				XctWaitsForLockList&	waitsForList,
				const xct_t*		current,
				const xct_t*		victim) = 0;
	virtual void KillingGlobalXct(const xct_t* xct, const lockid_t& lockid) = 0;
	virtual void GlobalDeadlockDetected(GtidList& list) = 0;
	virtual void GlobalDeadlockVictimSelected(const gtid_t& gtid) = 0;
	virtual ~DeadlockEventCallback() {};
};


class DeadlockEventPrinterBase : public DeadlockEventCallback
{
    protected:
	void		PrintLocalDeadlockDetected(
				ostream&		os,
				XctWaitsForLockList&	waitsForList,
				const xct_t*		current,
				const xct_t*		victim);
	void		PrintKillingGlobalXct(
				ostream&		os,
				const xct_t*		xct,
				const lockid_t&		lockid);
	void		PrintGlobalDeadlockDetected(ostream& os, GtidList& list);
	void		PrintGlobalDeadlockVictimSelected(ostream& os, const gtid_t& gtid);
};


class DeadlockEventPrinter : public DeadlockEventPrinterBase
{
    public:
			DeadlockEventPrinter(ostream& o);
	void		LocalDeadlockDetected(					// see sm.cpp
				XctWaitsForLockList&	waitsForList,
				const xct_t*		current,
				const xct_t*		victim);
	void		KillingGlobalXct(
				const xct_t*		xct,
				const lockid_t&		lockid);
	void		GlobalDeadlockDetected(GtidList& list);
	void		GlobalDeadlockVictimSelected(const gtid_t& gtid);

    private:
	ostream&	out;
};


inline
DeadlockEventPrinter::DeadlockEventPrinter(ostream& o)
:
    out(o)
{
}


/*<std-footer incl-file-exclusion='DEADLOCK_EVENTS_H'>  -- do not edit anything below this line -- */

#endif          /*</std-footer>*/

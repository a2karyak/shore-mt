/*<std-header orig-src='shore'>

 $Id: participant2.cpp,v 1.12 1999/11/10 20:04:06 bolo Exp $

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

/*
 *  Stuff common to both coordinator and subordnates.
 */

#define SM_SOURCE
#define COORD_C /* yes - COORD_C */


#include <sm_int_0.h>
#ifdef USE_COORD
#include <coord.h>

rc_t
participant::register_ep(
    CommSystem *	_comm,
    NameService *	_ns,
    const char *	uniquename,
    Endpoint &		ep
)
{
    rc_t 	rc;

    w_assert1(_ns);
    w_assert1(_comm);

    W_IGNORE(_ns->cancel((char *)uniquename));
    W_DO(_comm->make_endpoint(ep));

    if(smlevel_0::errlog->getloglevel() >= log_info) {
	smlevel_0::errlog->clog <<info_prio 
	    <<"REGISTERING " << uniquename <<" <--> "
	    << ep
	    << flushl;
    }

    W_DO(_ns->enter((char *)uniquename, ep));

    return RCOK;
}

rc_t
participant::un_register_ep(
    NameService *	_ns,
    const char *	uniquename
)
{
    rc_t 	rc;

    w_assert1(_ns);

    W_IGNORE(_ns->cancel((char *)uniquename));

    if(smlevel_0::errlog->getloglevel() >= log_info) {
	smlevel_0::errlog->clog <<info_prio 
	    <<"DEREGISTERED " << uniquename <<" <--> "
	    << flushl;
    }
    return RCOK;
}
#endif /* USE_COORD */


/*<std-header orig-src='shore' incl-file-exclusion='MAPPINGS_H'>

 $Id: mappings.h,v 1.19 1999/06/07 19:04:17 kupsch Exp $

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

#ifndef MAPPINGS_H
#define MAPPINGS_H

#include "w_defines.h"

/*  -- do not edit anything above this line --   </std-header>*/

#ifdef USE_COORD

/*
 * This file describes two abstract classes that must be implemented
 * by a value-added server (VAS) that uses the Coordinator and Subordinate
 * classes for two-phase commit (2PC).
 *
 * Both classes implement 1:1 mappings: 
 * class name_ep_map 
 *    maps  names (opaque values) to Endpoints (a class in the object-comm
 *    package), and Endpoints to names.  This mapping is used by the
 *    Coordinator and by Subordinates.  It is the responsibility of the
 *    VAS to keep this mapping current:  if a subordinate VAS crashes
 *    and resumes with a different endpoint, the architecture of the
 *    VAS must provide for the mapping to be brought up-to-date.
 *
 * class tid_gtid_map
 *    maps a local transaction ID (tid) to a global transaction ID (gtid)
 *    (an opaque value), and a gtid to a tid.
 *    This is used by Subordinates to locate a local transaction to which
 *    to apply a command containing a global tid.   
 *
 * A coordinating VAS (master) must, in some sense, "know" what
 *    subordinate servers are involved in a distributed transaction,
 *    but the Coordinator and Subordinate classes to not require any
 *    such mapping to be maintained by the VAS.  
 */


#ifndef TID_T_H
#include <tid_t.h>
#endif
#ifndef STHREAD_H
#include <sthread.h>
#endif
#ifndef _SCOMM_HH_
#include <scomm.h>
#endif
#ifndef __NS_CLIENT__
#include <ns_client.h>
#endif


#define VIRTUAL(x) virtual x = 0;

class name_ep_map {
public: 
    virtual NORET  	~name_ep_map() {};
    virtual rc_t  	name2endpoint(const server_handle_t &, Endpoint &)=0;
     /*
      *	NB: regarding reference counts on endpoints:
      *          In order to avoid race conditions, it's necessary 
      *          for the implementation of this class to call ep.acquire()
      *          in name2endpoint(), and the caller must do ep.release().
      */          
    virtual rc_t  	endpoint2name(const Endpoint &, server_handle_t &)=0;

    /*
     * NB: both methods must return RC(nsNOTFOUND) if entry not found,
     * may return RC(scDEAD) if ep known to be dead.
     */
};


class tid_gtid_map {
public: 
    virtual NORET ~tid_gtid_map() {};
    virtual rc_t  add(const gtid_t &, const tid_t &)=0;
    virtual rc_t  remove(const gtid_t &, const tid_t &)=0;
    virtual rc_t  gtid2tid(const gtid_t &, tid_t &)=0;
    /*
     * NB: must return RC(fcNOTFOUND) if entry not found
     * (yes this is different from the above mapping)
     */

};

#endif

/*<std-footer incl-file-exclusion='MAPPINGS_H'>  -- do not edit anything below this line -- */

#endif          /*</std-footer>*/

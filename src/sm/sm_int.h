/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

#ifndef SM_INT_H
#define SM_INT_H

#define SM_LEVEL -1

#if defined(BF_C) || defined(COMM_C) || defined(LATCH_C) || defined(CACHE_C)
#    undef SM_LEVEL
#    define SM_LEVEL 0
#endif /*BF_C || defined(COMM_C) */

#if defined(LOCK_C) || defined(LOCK_CORE_C) || defined(LOCK_REMOTE_C) || defined(VOL_C) || defined(SMLOCK_C) || defined(PAGE_C) || defined(LOG_C) || defined(LOGSTUB_C) || defined(SMTHREAD_C) || defined(CALLBACK_C)
#    undef SM_LEVEL
#    define SM_LEVEL 1
#endif /*LOCK_C || VOL_C || XCT_C || SMLOCK_C || PAGE_C*/

#if defined(KEYED_C) || defined(ZKEYED_C) || defined(BTREE_C) || defined(FILE_C) || defined(RTREE_C) || defined(RDTREE_C) || defined(LOGREC_C) || defined(LGREC_C) 
#    undef SM_LEVEL
#    define SM_LEVEL 2
#endif /*BTREE_C || FILE_C || RTREE_C*/

#if defined(CHKPT_C) || defined(DIR_C) || defined(RESTART_C) || defined(LID_C) || defined(REMOTE_CLIENT_C) || defined(IO_C) || defined(XCT_C)
#    undef SM_LEVEL
#    define SM_LEVEL 3
#endif /* CHKPT_C || DIR_C || RESTART_C || LID_C IO_C */

#if defined(PIN_C) || defined(SCAN_C) || defined(SM_C) || defined(SMFILE_C) || defined(SMINDEX_C) || defined(SORT_C) || defined(REMOTE_C) || defined(TEST_C)
#    undef SM_LEVEL
#    define SM_LEVEL 4
#endif

#include <sysdefs.h>
#include <basics.h>
#include <debug.h>
#include <sthread.h>
#include <vec_t.h>
#include <zvec_t.h>
#include <latch.h>
#include <rsrc.h>
#if defined(MULTI_SERVER) && (defined(COMM_C) || defined(REMOTE_C) || defined(REMOTE_CLIENT_C) || defined(SM_C) || defined(XCT_C) || defined(CALLBACK_C) || defined(LOCK_CORE_C) || defined(TEST_C) || defined(LOCK_REMOTE_C))
#    include "w.h"
#    include "sthread.h"
#    include "scomm.hh"
#    include "ns_client.hh"  // NameServer
#endif /* MULTI_SERVER */
#include <lid_t.h>
#include <sm_s.h>
#include <sm_base.h>
#include <smthread.h>
#include <tid_t.h>
#include <bitmap.h>
#include "smstats.h"


#if (SM_LEVEL >= 0) 
#    include <bf.h>
#    include <io.h>
#    include <page.h>
#    include <log.h>
#    include <srvid_t.h>
#    include <comm.h>
#endif

#if (SM_LEVEL >= 1)
#    include <lock.h>
#    include <lock_remote.h>
#    include <xct.h>
#    include <logrec.h>
#    ifdef VOL_C
#        include <vol.h>
#    endif
#endif

#if (SM_LEVEL >= 2)
#    include <sdesc.h>
#    ifdef BTREE_C
#	define RTREE_H
#	define RDTREE_H
#    endif
#    ifdef RTREE_C
#	define BTREE_H
#	define RDTREE_H
#    endif
#    ifdef RDTREE_C
#	define BTREE_H
#    endif
#    if defined(FILE_C) || defined(SMFILE_C)
#	define BTREE_H
#	define RTREE_H
#	define RDTREE_H
#    endif
#    include <btree.h>
#    include <nbox.h>
#    include <rtree.h>
#    ifdef USE_RDTREE
#        include <setarray.h>
#        include <rdtree.h>
#    endif /* USE_RDTREE */
#    include <file.h>
#    if defined(FILE_C) || defined(LGREC_C) || defined(PIN_C) || defined(SM_C) || defined(SMFILE_C) || defined(SORT_C)
#        include <lgrec.h>
#    endif
#endif

#if (SM_LEVEL >= 3)
#    if defined(CHKPT_C) || defined (SM_C) || defined(IO_C) || defined(XCT_C)
#        include <chkpt.h>
#    endif
#    include <dir.h>
#    include <restart.h>
#    if defined(LID_C) || defined(REMOTE_CLIENT_C) || (SM_LEVEL > 3)
#        include <lid.h>
#    endif
#endif

#if (SM_LEVEL >= 4)
#    include <sm.h>
#    if defined(REMOTE_C) || defined(SM_C) || defined(TEST_C)
#       include <srvid_t.h>
#       include <remote_s.h>
#    	include <remote.h>
#    endif
#    if defined(PIN_C) || defined(SCAN_C) || defined(SM_C)
#    	include <pin.h>
#    endif 
#    if defined(SM_C)
#	include <xct.h>
#    endif
#    if defined(SCAN_C)
#    	include <scan.h>
#    endif 
#    include <sort.h>
#    include <prologue.h>
#endif

#ifdef DEBUG
#define SMSCRIPT(x) \
	scriptlog->clog << info_prio << "sm " x << flushl;
#define RES_SMSCRIPT(x)\
	scriptlog->clog << info_prio << "set res [sm " x << "]" << flushl;\
			scriptlog->clog << info_prio << "verbose $res" << flushl;
#else
#define SMSCRIPT(x) 
#define RES_SMSCRIPT(x) 
#endif

#endif /*SM_INT_H*/


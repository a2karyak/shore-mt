/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

/*
 *  $Id: sm_vas.h,v 1.17 1995/10/06 03:40:17 zwilling Exp $
 */
#ifndef SM_VAS_H
#define SM_VAS_H

/*
 * sm_vas.h is the include file that all value added servers should
 * to get access to the Shore Server storage manager interface.
 */
#include <stddef.h>
#include <iostream.h>
#include <strstream.h>

#ifndef	 RPC_HDR
#include "w.h"
#include "option.h"
#include "basics.h"
#include "lid_t.h"
#include "vec_t.h"
#include "zvec_t.h"
#include "tid_t.h"
#endif 	/* RPC_HDR */

#include "sm_s.h"
#include "sm_base.h"
#include "smthread.h"
#include "file_s.h"
#include "sm.h"
#include "pin.h"
#include "scan.h"
#include "sort.h"
#include "nbox.h"

#endif /* SM_VAS_H */

/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

/*
 *  $Header: /p/shore/shore_cvs/src/vas/server/sizes.C,v 1.12 1995/04/24 19:47:29 zwilling Exp $
 */
#include <copyright.h>

#include "debug.h"
#include "mount_m.h"
#include "Anonymous.h"
#include "Symlink.h"
#include "Xref.h"
#include "cltab.h"
#include "vaserr.h"

main()
{
#define S(x) cout << #x << ": " << sizeof(x) << endl;
	S(svas_base);
	S(svas_server);
	S(client_t);
	S(string_t);
	S(mount_m);
	S(mount_info);
//	S(mem_info);
//	S(mem_m);
	S(cltab_t);
	S(Anonymous);
	S(Object);
	S(Pool);
	S(Registered);
	S(Directory);
	S(Xref);
	S(Symlink);
//	S(Module);
	S(SysProps);
	S(RegProps);
	S(AnonProps);
	S(reg_sysprops);
	S(anon_sysprops);
	S(sm_du_stats_t);
}

int OpenMax;
ss_m	*Sm = NULL;


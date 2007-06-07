/*<std-header orig-src='shore'>

 $Id: ssh_error.cpp,v 1.13 1999/06/07 19:05:02 kupsch Exp $

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

#include <w.h>
#include "e_error_def_gen.h"
#include "st_error_def_gen.h"
#include "opt_error_def_gen.h"
#include "fc_error_def_gen.h"

#ifdef USE_COORD
#include "sc_error_def_gen.h"
#include "ns_error_def_gen.h"
#endif

#include "e_einfo_bakw_gen.h"
#include "opt_einfo_bakw_gen.h"
#include "st_einfo_bakw_gen.h"
#include "fc_einfo_bakw_gen.h"
#ifdef USE_COORD
#include "sc_einfo_bakw_gen.h"
#include "ns_einfo_bakw_gen.h"
#endif
#include <w_debug.h>
#include "ssh_error.h"

const char *
ssh_err_msg(const char *str)
{
    FUNC(ssh_err_msg);

    return w_error_t::error_string(ssh_err_code(str));
}

unsigned int
ssh_err_code(const char *x)
{
    FUNC(ssh_err_code);
    w_error_info_t  *v;
    int             j;


#define LOOK(a,b,c) \
    v = (a);\
    j = (b);\
    while( (v != 0) && j++ <= (c) ) {\
	    if(strcmp(v->errstr,x)==0) { \
		    return  v->err_num;\
	    }\
	    v++;\
    }
    LOOK(e_error_info_bakw,E_ERRMIN,E_ERRMAX);
    LOOK(st_error_info_bakw,ST_ERRMIN,ST_ERRMAX);
    LOOK(opt_error_info_bakw,OPT_ERRMIN,OPT_ERRMAX);
    LOOK(fc_error_info_bakw,FC_ERRMIN,FC_ERRMAX);

#undef LOOK
    return fcNOSUCHERROR;
}

// returns error name given error code
// return false if the error code is not in SVAS_* or OS_*
const char *
ssh_err_name(unsigned int x)
{
    FUNC(ssh_err_name);

    DBG(<<"ssh_err_name for " << x);

    w_error_info_t  *v;
    int j;

#define LOOK(a,b,c) \
    v = (a);\
    j = (b);\
    while( (v != 0) && j++ <= (c) ) {\
	    if(x == v->err_num) {\
		    return  v->errstr;\
	    }\
	    v++;\
    }
    
    LOOK(e_error_info_bakw,E_ERRMIN,E_ERRMAX);
    LOOK(st_error_info_bakw,ST_ERRMIN,ST_ERRMAX);
    LOOK(opt_error_info_bakw,OPT_ERRMIN,OPT_ERRMAX);
    LOOK(fc_error_info_bakw,FC_ERRMIN,FC_ERRMAX);
#ifdef USE_COORD
    LOOK(sc_error_info_bakw,SC_ERRMIN,SC_ERRMAX);
    LOOK(ns_error_info_bakw,NS_ERRMIN,NS_ERRMAX);
#endif

#undef LOOK

    return "bad error value";
}


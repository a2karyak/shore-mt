/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

#ifdef __GNUG__
#pragma implementation "sm_s.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <iostream.h>
#include <stddef.h>
#include <limits.h>
#include "sm_app.h"

const stid_t stid_t::null;
const rid_t rid_t::null;

const lpid_t lpid_t::bof;
const lpid_t lpid_t::eof;
const lpid_t lpid_t::null;

const lsn_t lsn_t::null(0, 0);
const lsn_t lsn_t::max(lsn_t::hwm, lsn_t::hwm);

int pull_in_sm_export()
{
    return eNOERROR;
}


w_rc_t key_type_s::parse_key_type(
    const char* 	s,
    uint4& 		count,
    key_type_s		kc[])
{
    istrstream is(s, strlen(s));

    uint i;
    for (i = 0; i < count; i++)  {
	kc[i].variable = 0;
	if (! (is >> kc[i].type))  {
	    if (is.eof())  break;
	    return RC(smlevel_0::eBADKEYTYPESTR);
	}
	if (is.peek() == '*')  {
	    if (! (is >> kc[i].variable))
		return RC(smlevel_0::eBADKEYTYPESTR);
	}
	if (! (is >> kc[i].length))
	    return RC(smlevel_0::eBADKEYTYPESTR);
	if (kc[i].length > key_type_s::max_len)
	    return RC(smlevel_0::eBADKEYTYPESTR);

	switch (kc[i].type)  {
	case key_type_s::i:
	case key_type_s::u:
	    if (kc[i].length != 2 && kc[i].length != 4 || kc[i].variable)
		return RC(smlevel_0::eBADKEYTYPESTR);
	    break;
	case key_type_s::f:
	    if (kc[i].length != 4 && kc[i].length != 8 || kc[i].variable)
		return RC(smlevel_0::eBADKEYTYPESTR);
	    break;
	case key_type_s::b:
	    break;
	default:
	    return RC(smlevel_0::eBADKEYTYPESTR);
        }
    }
    count = i;

    for (i = 0; i < count; i++)  {
	if (kc[i].variable && i != count - 1)
	    return RC(smlevel_0::eBADKEYTYPESTR);
    }
    
    return RCOK;
}

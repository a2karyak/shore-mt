/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

/*
 *  $Id: keyed.c,v 1.17 1995/04/24 19:35:51 zwilling Exp $
 */
#define SM_SOURCE
#define KEYED_C

#ifdef __GNUG__
#   pragma implementation
#endif

#include <sm_int.h>
#include <keyed.h>

MAKEPAGECODE(keyed_p, page_p);

void keyed_p::ntoh()
{
    /* nothing to do */
}

/*--------------------------------------------------------------*
 *  keyed_p::shift()						*
 *--------------------------------------------------------------*/
rc_t
keyed_p::shift(int idx, keyed_p* rsib)
{
    w_assert1(idx >= 0 && idx < nrecs());

    int n = nrecs() - idx;
    vec_t* tp = new vec_t[n];
    w_assert1(tp);
    
    for (int i = 0; i < n; i++) {
	tp[i].put(page_p::tuple_addr(1 + idx + i),
		  page_p::tuple_size(1 + idx + i));
    }
    
    /*
     *  insert all of tp into slot 1 of rsib 
     *  (slot 0 reserved for header)
     */
    rc_t rc = rsib->insert_expand(1, n, tp);
    if (! rc)  {
	rc = remove_compress(1 + idx, n);
    }

    delete[] tp;

    return rc.reset();
}

rc_t
keyed_p::set_hdr(const cvec_t& data)
{
    W_DO( page_p::overwrite(0, 0, data) );
    return RCOK;
}

/*--------------------------------------------------------------*
 *  keyed_p::insert()						*
 *	Insert a <key, el> pair into a particular slot of a	*
 *	keyed page. 						*
 *--------------------------------------------------------------*/
rc_t
keyed_p::insert(
    const cvec_t& key,		// the key to be inserted
    const cvec_t& el,		// the element associated with key
    int slot,			// the interesting slot
    shpid_t child)
{
    keyrec_t::hdr_s hdr;
    hdr.klen = key.size();
    hdr.elen = el.size();
    hdr.child = child;

    vec_t vec;
    vec.put(&hdr, sizeof(hdr)).put(key).put(el);
    
    W_DO( page_p::insert_expand(slot + 1, 1, &vec) );

    return RCOK;
}

/*--------------------------------------------------------------*
 *  keyed_p::remove()						*
 *--------------------------------------------------------------*/
rc_t
keyed_p::remove(int slot)
{
    W_DO( page_p::remove_compress(slot + 1, 1) );
    return RCOK;
}

/*--------------------------------------------------------------*
 *  keyed_p::format()						*
 *--------------------------------------------------------------*/
rc_t
keyed_p::format(const lpid_t& /*pid*/, tag_t /*tag*/, uint4_t /*flags*/)
{
    /* 
     *  keyed_p is never instantiated individually. it is meant to
     *  be inherited.
     */
    w_assert1(eINTERNAL);
    return RCOK;
}

rc_t
keyed_p::format(const lpid_t& pid, tag_t tag,
		uint4_t flags, const
		cvec_t& hdr)
{
    W_DO( page_p::format(pid, tag, flags) );
    W_COERCE( page_p::insert_expand(0, 1, &hdr) );
    return RCOK;
}

/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

/*
 *  $Id: zkeyed.c,v 1.23 1995/09/15 03:44:27 zwilling Exp $
 */
#define SM_SOURCE
#define ZKEYED_C
#include <sm_int.h>
#ifdef __GNUG__
#   pragma implementation
#endif

#include <zkeyed.h>


void zkeyed_p::ntoh()
{
    /* nothing to do */
}

MAKEPAGECODE(zkeyed_p, page_p);



/*********************************************************************
 *
 *  zkeyed_p::shift(idx, rsib)
 *
 *  Shift all entries starting at "idx" to first entry of page "rsib".
 *
 *********************************************************************/
rc_t
zkeyed_p::shift(int idx, zkeyed_p* rsib)
{
    w_assert1(idx >= 0 && idx < nrecs());

    int n = nrecs() - idx;
    rc_t rc;
    for (int i = 0; i < n && (! rc); ) {
	vec_t tp[20];
	int j;
	for (j = 0; j < 20 && i < n; j++, i++)  {
	    tp[j].put(page_p::tuple_addr(1 + idx + i),
		      page_p::tuple_size(1 + idx + i));
	}
	rc = rsib->insert_expand(1 + i - j, j, tp);
    }
    if (! rc)  {
	rc = remove_compress(1 + idx, n);
    }

    return rc.reset();

#ifdef undef
    /* OLD VERSION  */
    int n = nrecs() - idx;
    vec_t* tp = new vec_t[n];
    w_assert1(tp);
    w_auto_delete_array_t<vec_t> auto_del(tp);
    
    for (int i = 0; i < n; i++) {
	tp[i].put(page_p::tuple_addr(1 + idx + i),
		  page_p::tuple_size(1 + idx + i));
    }
    
    /*
     *  Insert all of tp into slot 1 of rsib (slot 0 reserved for header)
     */
    rc_t rc = rsib->insert_expand(1, n, tp);
    if (! rc)  {
	rc = remove_compress(1 + idx, n);
    }

    return rc.reset();
#endif /*undef*/
}


/*********************************************************************
 *
 *  zkeyed_p::set_hdr(data)
 *
 *  Set the page header to "data".
 *
 *********************************************************************/
rc_t
zkeyed_p::set_hdr(const cvec_t& data)
{
    W_DO( page_p::overwrite(0, 0, data) );
    return RCOK;
}



/*********************************************************************
 *
 *  zkeyed_p::insert(key, aux, slot)
 *
 *  Insert a <key, aux> pair into a particular slot of a
 *  keyed page.
 *
 *********************************************************************/
rc_t
zkeyed_p::insert(
    const cvec_t& 	key,
    const cvec_t& 	aux,
    int 		slot)
{
    vec_t v;
    int4 klen = key.size();
    v.put(&klen, sizeof(klen)).put(key).put(aux);
    W_DO( page_p::insert_expand(slot + 1, 1, &v) );
    return RCOK;
}




/*********************************************************************
 *
 *  zkeyed_p::remove(slot)
 *
 *********************************************************************/
rc_t
zkeyed_p::remove(int slot)
{
    W_DO( page_p::remove_compress(slot + 1, 1) );
    return RCOK;
}



/*********************************************************************
 *
 *  zkeyed_p::format(pid, tag, flags)
 *
 *  Should never be called (overriden by derived class).
 *
 *********************************************************************/
rc_t
zkeyed_p::format(const lpid_t& /*pid*/, tag_t /*tag*/, uint4_t /*flags*/)
{
    /* 
     *  zkeyed_p is never instantiated individually. it is meant to
     *  be inherited.
     */
    w_assert1(eINTERNAL);
    return RCOK;
}



/*********************************************************************
 *
 *  zkeyed_p::format(pid, tag, flags, hdr)
 *
 *  Format a page with header "hdr". 
 *
 *********************************************************************/
rc_t
zkeyed_p::format(
    const lpid_t& 	pid, 
    tag_t 		tag, 
    uint4_t 		page_flags,
    const cvec_t& 	hdr)
{
    W_DO( page_p::format(pid, tag, page_flags) );
    W_COERCE( page_p::insert_expand(0, 1, &hdr) );
    return RCOK;
}

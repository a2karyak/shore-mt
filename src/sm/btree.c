/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

/*
 *  $Id: btree.c,v 1.221 1996/07/09 20:41:10 nhall Exp $
 */
#define SM_SOURCE
#define BTREE_C

#ifdef __GNUG__
#   pragma implementation "btree.h"
#   pragma implementation "btree_p.h"
#endif

#include "sm_int.h"
#include "btree_p.h"
#include "sort.h"
#include "sm_du_stats.h"
#include <debug.h>
#include <crash.h>

#ifdef __GNUG__
template class w_auto_delete_array_t<record_t *>; 
#endif

#ifdef DEBUG
/* change these at will */
#	define print_sanity false
#	define supertest (_debug.flag_on("supertest",__FILE__))
#	define print_traverse (_debug.flag_on("print_traverse",__FILE__))
#	define print_wholetree (_debug.flag_on("print_wholetree",__FILE__))
#	define print_remove false
#	define print_propagate (_debug.flag_on("print_propagate",__FILE__))

#else
/* don't change these */
#	define print_sanity false
#	define print_wholetreee false
#	define print_traverse false
#	define print_remove false
#	define print_propagate false
#endif

/*********************************************************************
 *
 *  Potential Latch Deadlock.
 *
 *  Scenerio: a tree has leaf pages P1, P2.
 *	1. T1 latch P1(EX) and needs to split P1
 *	2. T2 latch P2(EX) and need to unlink(destroy) P2
 *  	3. T1 allocates P3 and tries to latch P2 to update link
 *	4. T2 tries to latch P1
 *	5. T1 and T2 deadlocked.
 *
 *  Solution:
 *	if T2 cannot get latch on P2 siblings, it should unlatch
 *	P2 and retry (might involve retraverse).
 *
 *  Followup:
 *	Solution Applied.
 *
 *********************************************************************/

/*********************************************************************
 *
 *  class btsink_t
 *
 *  Manages bulk load of a btree. 
 *
 *  User calls put() to add <key,el> into the sink, and then
 *  call map_to_root() to map the current root to the original 
 *  root page of the btree, and finalize the bulkload.
 *
 *  CC Note: 
 *	Btree must be locked EX.
 *  Recovery Note: 
 *	Logging is turned off during insertion into the btree.
 *	However, as soon as a page fills up, a physical 
 *	page image log is generated for the page.
 *
 *********************************************************************/
class btsink_t : private btree_m {
public:
    NORET	btsink_t(const lpid_t& root, rc_t& rc);
    NORET	~btsink_t() {};
    
    rc_t	put(const cvec_t& key, const cvec_t& el);
    rc_t	map_to_root();

    uint2	height()	{ return _height; }
    uint4	num_pages()	{ return _num_pages; }
    uint4	leaf_pages()	{ return _leaf_pages; }
private:
    uint2	_height;	// height of the tree
    uint4	_num_pages;	// total # of pages
    uint4	_leaf_pages;	// total # of leaf pages

    lpid_t	_root;		// root of the btree
    btree_p	_page[20];	// a stack of pages (path) from root
				// to leaf
    int		_slot[20];	// current slot in each page of the path
    shpid_t	_left_most[20];	// id of left most page in each level
    int 	_top;		// top of the stack

    rc_t	_add_page(int i, shpid_t pid0);
};

smsize_t			
btree_m::max_entry_size() {
    return btree_p::max_entry_size;
}

/*********************************************************************
 *
 *  btree_m::_search(page, key, elem, found, slot)
 *
 *  Search page for <key, elem> and return status in found and slot.
 *
 *********************************************************************/
inline rc_t
btree_m::_search(
    const btree_p&	page,
    const cvec_t&	key,
    const cvec_t&	elem,
    bool&		found,
    int&		ret_slot)
{
    if (key.is_neg_inf())  {
	found = FALSE, ret_slot = 0;
    } else if (key.is_pos_inf()) {
	found = FALSE, ret_slot = page.nrecs();
    } else {
	rc_t rc = page.search(key, elem, found, ret_slot);
	if (rc)  return rc.reset();
    }
    return RCOK;
}

/*********************************************************************
 *
 *  btree_m::create(stpgid, root)
 *
 *  Create a btree. Return the root page id in root.
 *
 *********************************************************************/
rc_t
btree_m::create(
    stpgid_t 		stpgid,		// I-  store id of new btree
    lpid_t& 		root)		// O-  root of new btree
{
    lsn_t anchor;
    xct_t* xd = xct();

    if (xd)  anchor = xd->anchor();

    /*
     *  if this btree is not stored in a 1-page store, then
     *  we need to allocate a root.
     */
    if (stpgid.is_stid()) {
	X_DO( io->alloc_pages(stpgid.stid(), lpid_t::eof, 1, &root) );
    } else {
	root = stpgid.lpid;
    }

    btree_p page;
    X_DO( page.fix(root, LATCH_EX, page.t_virgin) );
    X_DO( page.set_hdr(root.page, 1, 0, 0) );

    
    if (xd)  {
	CRASHTEST("toplevel.btree.1");
	xd->compensate(anchor);
    }

    return RCOK;
}




/*********************************************************************
 *
 *  btree_m::is_empty(root, ret)
 *
 *  Return true in ret if btree at root is empty. False otherwise.
 *
 *********************************************************************/
rc_t
btree_m::is_empty(
    const lpid_t&	root,	// I-  root of btree
    bool& 		ret)	// O-  TRUE if btree is empty
{
    key_type_s kc(key_type_s::b, 0, 4);
    cursor_t cursor;
    W_DO( fetch_init(cursor, root, 1, &kc, false, t_cc_none,
		     cvec_t::neg_inf, cvec_t::neg_inf,
		     ge,
		     le, cvec_t::pos_inf));
    W_DO( fetch(cursor) );
    ret = (cursor.key() == 0);
    return RCOK;
}



/*********************************************************************
 *
 *  btree_m::purge(root, check_empty)
 *
 *  Remove all pages of a btree except the root. "Check_empty" 
 *  indicates whether to check if the tree is empty before purging.
 #  NOT USED 
 *
 *********************************************************************/
rc_t
btree_m::purge(
    const lpid_t& 	root,		// I-  root of btree
    bool		check_empty)
{
    if (check_empty)  {
	bool flag;
	W_DO( is_empty(root, flag) );
	if (! flag)  {
	    return RC(eNDXNOTEMPTY);
	}
    }

    lsn_t anchor;
    xct_t* xd = xct();
    w_assert3(xd);
    anchor = xd->anchor();

    lpid_t pid;
    X_DO( io->first_page(root.stid(), pid) );
    while (pid.page)  {
	/*
	 *  save current pid, get next pid, free current pid.
	 */
	lpid_t cur = pid;
	rc_t rc = io->next_page(pid);
	if (cur.page != root.page)  {
	    X_DO( io->free_page(cur) );
	}
	if (rc)  {
	    if (rc.err_num() != eEOF)  {
		xd->stop_crit();
		return RC_AUGMENT(rc);
	    }
	    break;
	}
    }

    btree_p page;
    X_DO( page.fix(root, LATCH_EX) );
    X_DO( page.set_hdr(root.page, 1, 0, 0) );

    CRASHTEST("toplevel.btree.2");
    xd->compensate(anchor);
    
    W_COERCE( log_btree_purge(page) );

    return RCOK;
}

#ifdef UNDEF


static int
elm_cmp(const void* k1, const void* k2)
{
    const record_t* r1 = (const record_t*)k1;
    const record_t* r2 = (const record_t*)k2;
    vec_t key(r1->body(), (int) r1->body_size());

    return ( key.cmp(r2->body(), (int) r2->body_size()) );
}
#endif /*UNDEF*/




/*********************************************************************
 *
 *  btree_m::_handle_dup_keys(sink, slot, prevp, curp, count, r, pid)
 *
 *
 *********************************************************************/
rc_t
btree_m::_handle_dup_keys(
    btsink_t*           sink,           // IO- load stack
    int&                slot,           // IO- current slot
    file_p*             prevp,          // I-  previous page
    file_p*             curp,           // I-  current page
    int&                count,          // O-  number of duplicated keys
    record_t*&  	r,              // O-  last record
    lpid_t&             pid,            // IO- last pid
    int			nkc,
    const key_type_s*	kc)
{
    count = 0;
    int max_rec_cnt = 500;

    record_t** recs = new record_t* [max_rec_cnt];
    if (!recs)  { return RC(eOUTOFMEMORY); }
    w_auto_delete_array_t<record_t*> auto_del_recs(recs);

    bool eod = false, eof = false;
    register i;

    if (slot==1) {
        // previous rec is on the previous page
        W_COERCE( prevp->get_rec(prevp->num_slots()-1, r) );
    } else {
        W_COERCE( curp->get_rec(slot-1, r) );
    }
    recs[count++] = r;

    W_COERCE( curp->get_rec(slot, r) );
    recs[count++] = r;

    int s = slot;
    while ((s = curp->next_slot(s)))  {
        W_COERCE( curp->get_rec(s, r) );
        if (r->hdr_size() == recs[0]->hdr_size() &&
            !memcmp(r->hdr(), recs[0]->hdr(), r->hdr_size())) {
            if (r->body_size() == recs[0]->body_size() &&
                !memcmp(r->body(), recs[0]->body(), (int)r->body_size())) {
                return RC(eDUPLICATE);
            }
            if (count == max_rec_cnt) {
                max_rec_cnt *= 2;
                record_t** tmp = new record_t* [max_rec_cnt];
		if (!tmp)  { return RC(eOUTOFMEMORY); }
                memcpy(tmp, recs, count*sizeof(recs));
                delete [] recs;
                recs = tmp;
		auto_del_recs.set(recs);
            }
            recs[count++] = r;
        } else {
            eod = true;
            break;
        }
    }

    int page_cnt = 0, max_page_cnt = 100;
    file_p* pages = new file_p[max_page_cnt];
    if (!pages)  { return RC(eOUTOFMEMORY); }
    w_auto_delete_array_t<file_p> auto_del_pages(pages);

    if (!eod)  {
        W_DO( fi->next_page(pid, eof) );
    }

    while (!eof && !eod) {
        W_DO( pages[page_cnt].fix(pid, LATCH_SH) );
	s = 0;
	while ((s = pages[page_cnt].next_slot(s)))  {
            W_COERCE( pages[page_cnt].get_rec(s, r) );

            if (r->hdr_size() == recs[0]->hdr_size() &&
               !memcmp(r->hdr(), recs[0]->hdr(), r->hdr_size())) {
                if (r->body_size() == recs[0]->body_size() &&
		    !memcmp(r->body(), recs[0]->body(), (int)r->body_size())) {
                    return RC(eDUPLICATE);
                }
                if (count==max_rec_cnt) {
                    max_rec_cnt *= 2;
                    record_t** tmp = new record_t* [max_rec_cnt];
		    if (!tmp)  { return RC(eOUTOFMEMORY); }
                    memcpy(tmp, recs, count*sizeof(recs));
                    delete [] recs;
                    recs = tmp;
		    auto_del_recs.set(recs);
                }
                recs[count++] = r;
            } else {
                eod = true;
                break;
            }
        }
        page_cnt++;
        if (!eod) {
            W_DO( fi->next_page(pid, eof) );
            if (page_cnt >= max_page_cnt) {
		// BUGBUG: panic, too many duplicate key entries
		W_FATAL(eINTERNAL);
	    }
        }
    }

    // sort the recs : use selection sort
    for (i = 0; i < count - 1; i++) {
        for (register j = i + 1; j < count; j++) {
            vec_t el(recs[i]->body(), (int)recs[i]->body_size());
            if (el.cmp(recs[j]->body(), (int)recs[j]->body_size()) > 0) {
                record_t* tmp = recs[i];
                recs[i] = recs[j];
                recs[j] = tmp;
            }
        }
    }

//  qsort(recs, count, sizeof(void*), elm_cmp);

    vec_t key(recs[0]->hdr(), recs[0]->hdr_size());
    for (i = 0; i < count; i++) {
        cvec_t el(recs[i]->body(), (int)recs[i]->body_size());
	cvec_t* real_key;
	_scramble_key(real_key, key, nkc, kc);
        W_DO( sink->put(*real_key, el) );
    }

    if (page_cnt>0) {
        *curp = pages[page_cnt-1];
    }
    slot = s;

    if (eof) pid = lpid_t::null;

    return RCOK;
}


/*********************************************************************
 *
 *  btree_m::bulk_load(root, src, unique, cc, stats)
 *
 *  Bulk load a btree at root using records from store src.
 *  The key and element of each entry is stored in the header and
 *  body part, respectively, of records from src store. 
 *  NOTE: src records must be sorted in ascending key order.
 *
 *  Statistics regarding the bulkload is returned in stats.
 *
 *********************************************************************/
rc_t
btree_m::bulk_load(
    const lpid_t&	root,		// I-  root of btree
    stid_t		src,		// I-  store containing new records
    int			nkc,
    const key_type_s*	kc,
    bool		unique,		// I-  TRUE if btree is unique
    concurrency_t	cc_unused,	// I-  concurrency control mechanism
    btree_stats_t&	stats)		// O-  index stats
{
    w_assert1(kc && nkc > 0);

    // keep compiler quiet about unused parameters
    if (cc_unused) {}

    /*
     *  Set up statistics gathering
     */
    stats.clear();
    base_stat_t uni_cnt = 0;
    base_stat_t cnt = 0;

    /*
     *  Btree must be empty for bulkload.
     */
    W_DO( purge(root, true) );
	
    /*
     *  Create a sink for bulkload
     */
    rc_t rc;
    btsink_t sink(root, rc);
    if (rc) return RC_AUGMENT(rc);

    /*
     *  Go thru the src file page by page
     */
    int i = 0;		// toggle
    file_p page[2];		// page[i] is current page

    const record_t* pr = 0;	// previous record
    lpid_t pid;
    bool eof = false;
    bool skip_last = false;
    for (rc = fi->first_page(src, pid);
	 !rc.is_error() && !eof;
	  rc = fi->next_page(pid, eof))     {
	/*
	 *  for each page ...
	 */
	W_DO( page[i].fix(pid, LATCH_SH) );

	int s = page[i].next_slot(0);
	if (! s)  {
	    /*
	     *  do nothing. skip over empty page, so do not toggle
	     */
	    continue;
	} 
	for ( ; s; s = page[i].next_slot(s))  {
	    /*
	     *  for each slot in page ...
	     */
	    record_t* r;
	    W_COERCE( page[i].get_rec(s, r) );

	    if (pr) {
		cvec_t key(pr->hdr(), pr->hdr_size());
		cvec_t el(pr->body(), (int)pr->body_size());

		/*
		 *  check uniqueness and sort order
		 */
		if (key.cmp(r->hdr(), r->hdr_size()))  {
		    /*
		     *  key of r is greater than the previous key
		     */
		    ++cnt;
		    cvec_t* real_key = 0;
		    _scramble_key(real_key, key, nkc, kc);
		    W_DO( sink.put(*real_key, el) );
		    skip_last = false;
		} else {
		    /*
		     *  key of r is equal to the previous key
		     */
		    if (unique) {
			return RC(eDUPLICATE);
		    }

		    /*
		     * temporary hack for duplicated keys:
		     *  sort the elem in order before loading
		     */
		    int dup_cnt;
		    W_DO( _handle_dup_keys(&sink, s, &page[1-i],
					   &page[i], dup_cnt, r, pid,
					   nkc, kc) );
		    cnt += dup_cnt;
		    eof = (pid==lpid_t::null);
		    skip_last = eof ? true : false;
		} 

		++uni_cnt;
	    }

	    if (page[1-i])  page[1-i].unfix();
	    pr = r;

	    if (!s) break;
	}
	i = 1 - i;	// toggle i
	if (eof) break;
    }

    if (rc)  {
	return rc.reset();
    }

    if (!skip_last) {
	cvec_t key(pr->hdr(), pr->hdr_size());
	cvec_t el(pr->body(), (int)pr->body_size());
	cvec_t* real_key;
	_scramble_key(real_key, key, nkc, kc);
	W_DO( sink.put(*real_key, el) );
	++uni_cnt;
	++cnt;
    }

    W_DO( sink.map_to_root() );

    stats.level_cnt = sink.height();
    stats.leaf_pg_cnt = sink.leaf_pages();
    stats.int_pg_cnt = sink.num_pages() - stats.leaf_pg_cnt;

    stats.leaf_pg.unique_cnt = uni_cnt;
    stats.leaf_pg.entry_cnt = cnt;

    return RCOK;
}


/*********************************************************************
 *
 *  btree_m::bulk_load(root, sorted_stream, unique, cc, stats)
 *
 *  Bulk load a btree at root using records from sorted_stream.
 *  Statistics regarding the bulkload is returned in stats.
 *
 *********************************************************************/
rc_t
btree_m::bulk_load(
    const lpid_t&	root,		// I-  root of btree
    sort_stream_i&	sorted_stream,	// IO - sorted stream	
    int			nkc,
    const key_type_s*	kc,
    bool		unique,		// I-  TRUE if btree is unique
    concurrency_t	cc_unused,	// I-  concurrency control
    btree_stats_t&	stats)		// O-  index stats
{
    w_assert1(kc && nkc > 0);

    // keep compiler quiet about unused parameters
    if (cc_unused) {}

    /*
     *  Set up statistics gathering
     */
    stats.clear();
    base_stat_t uni_cnt = 0;
    base_stat_t cnt = 0;

    /*
     *  Btree must be empty for bulkload
     */
    W_DO( purge(root, true) );

    /*
     *  Create a sink for bulkload
     */
    rc_t rc;
    btsink_t sink(root, rc);
    if (rc) return RC_AUGMENT(rc);

    /*
     *  Allocate space for storing prev keys
     */
    char* tmp = new char[page_s::data_sz];
    if (! tmp)  {
	return RC(eOUTOFMEMORY);
    }
    w_auto_delete_array_t<char> auto_del_tmp(tmp);
    vec_t prev_key(tmp, page_s::data_sz);

    /*
     *  Go thru the sorted stream
     */
    bool pr = false;	// flag for prev key
    bool eof = false;

    vec_t key, el;
    W_DO ( sorted_stream.get_next(key, el, eof) );

    while (!eof) {
	++cnt;

	if (! pr) {
	    ++uni_cnt;
	    pr = TRUE;
	    prev_key.copy_from(key);
	} else {
	    // check unique
	    if (key.cmp(prev_key))  {
		++uni_cnt;
		prev_key.copy_from(key);
	    } else {
		if (unique) {
		    return RC(eDUPLICATE);
		}
		// BUGBUG: need to sort elems for duplicate keys
		return RC(eNOTIMPLEMENTED);
	    }
	}

	cvec_t* real_key;
	_scramble_key(real_key, key, nkc, kc);
	W_DO( sink.put(*real_key, el) ); 
	key.reset();
	el.reset();
	W_DO ( sorted_stream.get_next(key, el, eof) );
    }

    W_DO( sink.map_to_root() );
	
    stats.level_cnt = sink.height();
    stats.leaf_pg_cnt = sink.leaf_pages();
    stats.int_pg_cnt = sink.num_pages() - stats.leaf_pg_cnt;

    sorted_stream.finish();

    stats.leaf_pg.unique_cnt = uni_cnt;
    stats.leaf_pg.entry_cnt = cnt;

    return RCOK;
}



/*********************************************************************
 *
 *  btree_m::print_key_str(root)
 *
 *  Print the btree (for debugging only)
 *
 *********************************************************************/
void btree_m::print_key_str(const lpid_t& root, bool print_elem)
{
    lpid_t nxtpid, pid0;
    nxtpid = pid0 = root;
    {
	btree_p page;
	W_COERCE( page.fix(root, LATCH_SH) );

	for (int i = 0; i < 5 - page.level(); i++) {
	    cout << '\t';
	}
	cout 
	     << (page.is_smo() ? "*" : " ")
	     << (page.is_phantom() ? "P" : " ")
	     << " "
	     << "LEVEL " << page.level() 
	     << ", page " << page.pid().page 
	     << ", prev " << page.prev()
	     << ", next " << page.next()
	     << ", nrec " << page.nrecs()
	     << endl;
	page._print_key_str(print_elem);
	cout << flush;
	if (page.next())  {
	    nxtpid.page = page.next();
	}

	if ( ! page.prev() && page.pid0())  {
	    pid0.page = page.pid0();
	}
    }
    if (nxtpid != root)  {
	print_key_str(nxtpid,print_elem);
    }
    if (pid0 != root) {
	print_key_str(pid0,print_elem);
    }
}




/*********************************************************************
 *
 *  btree_m::print_key_uint4(root)
 *
 *  Print the btree (for debugging only)
 *
 *********************************************************************/
void btree_m::print_key_uint4(const lpid_t& root)
{
    lpid_t nxtpid, pid0;
    nxtpid = pid0 = root;
    {
	btree_p page;
	W_COERCE( page.fix(root, LATCH_SH) );

	for (int i = 0; i < 5 - page.level(); i++) {
	    cout << '\t';
	}
	cout << (page.is_smo() ? "* " : "  ")
	     << "LEVEL " << page.level() 
	     << ", page " << page.pid().page 
	     << ", prev " << page.prev()
	     << ", next " << page.next()
	     << ", nrec " << page.nrecs()
	     << endl;
	page.print_key_uint4();
	cout << flush;
	if (page.next())  {
	    nxtpid.page = page.next();
	}

	if ( ! page.prev() && page.pid0())  {
	    pid0.page = page.pid0();
	}
    }
    if (nxtpid != root)  {
	print_key_uint4(nxtpid);
    }
    if (pid0 != root) {
	print_key_uint4(pid0);
    }
}

void btree_m::print_key_uint2(const lpid_t& root)
{
    lpid_t nxtpid, pid0;
    nxtpid = pid0 = root;
    {
	btree_p page;
	W_COERCE( page.fix(root, LATCH_SH) );

	for (int i = 0; i < 5 - page.level(); i++) {
	    cout << '\t';
	}
	cout << (page.is_smo() ? "* " : "  ")
	     << "LEVEL " << page.level() 
	     << ", page " << page.pid().page 
	     << ", prev " << page.prev()
	     << ", next " << page.next()
	     << ", nrec " << page.nrecs()
	     << endl;
	page.print_key_uint2();
	cout << flush;
	if (page.next())  {
	    nxtpid.page = page.next();
	}

	if ( ! page.prev() && page.pid0())  {
	    pid0.page = page.pid0();
	}
    }
    if (nxtpid != root)  {
	print_key_uint2(nxtpid);
    }
    if (pid0 != root) {
	print_key_uint2(pid0);
    }
}




/*********************************************************************
 *
 *  mk_kvl(kvl, ...)
 *
 *  Make key value lock given <key, el> or a btree record, and 
 *  return it in kvl.
 *
 *  NOTE: if btree is non-unique, KVL lock is <key, el>
 *	  if btree is unique, KVL lock is <key>.
 *
 *********************************************************************/
inline void 
mk_kvl(kvl_t& kvl, stid_t stid, bool unique, 
       const cvec_t& key, const cvec_t& el = cvec_t::neg_inf)
{
    if (unique)
	kvl.set(stid, key);
    else {
	w_assert3(&el != &cvec_t::neg_inf);
	kvl.set(stid, key, el);
    }
}

inline void 
mk_kvl(kvl_t& kvl, stid_t stid, bool unique, 
       const btrec_t& rec)
{
    if (unique)
	kvl.set(stid, rec.key());
    else {
	kvl.set(stid, rec.key(), rec.elem());
    }
}




/*********************************************************************
 *
 *  btree_m::_check_duplicate(key, leaf, slot, kvl)
 *
 *  Check for duplicate key in leaf. Slot is the index returned 
 *  by btree_m::_traverse(), i.e. the position where key should
 *  be in leaf if it is in the leaf (or if it is to be inserted
 *  into leaf). 
 *  Returns TRUE if duplicate found, FALSE otherwise.
 *
 *********************************************************************/
bool
btree_m::_check_duplicate(
    const cvec_t& 	key, 
    btree_p& 		leaf, 
    int 		slot,
    kvl_t* 		kvl)
{
    stid_t stid = leaf.root().stid();
    if (kvl)  mk_kvl(*kvl, stid, TRUE, key);

    /*
     *  Check slot
     */
    if (slot < leaf.nrecs())  {
	btrec_t r(leaf, slot);
	if (key == r.key())  {
	    return TRUE;
	}
    }
    
    /* 
     *  Check neighbor slot to the left
     */
    if (slot > 0) {
	btrec_t r(leaf, slot-1);
	if (key == r.key())  {
	    return TRUE;
	}
    } else if (leaf.prev())  {
	w_assert3(slot == 0);
	lpid_t lpid = leaf.root();

	btree_p lsib;
	lpid.page = leaf.prev();
	while (lpid.page)  {

	    // "following" a link here means fixing the page
	    smlevel_0::stats.bt_links++;
	    W_COERCE( lsib.fix(lpid, LATCH_SH) );
	    if (lsib.nrecs() > 0)  {
		btrec_t r(lsib, lsib.nrecs() - 1);
		if (key == r.key())  {
		    return TRUE;
		}
		break;
	    }
	    lpid.page = lsib.prev();
	}
    }

    /*
     *  Check neighbor slot to the right
     */
    if (slot < leaf.nrecs() - 1) {
	btrec_t r(leaf, slot+1);
	if (key == r.key())  {
	    return TRUE;
	}
    } else if (leaf.next())  {
	// slot >= leaf.nrecs() - 1
	lpid_t rpid = leaf.root();

	btree_p rsib;
	rpid.page = leaf.next();
	while (rpid.page)  {

	    // "following" a link here means fixing the page
	    smlevel_0::stats.bt_links++;

	    W_COERCE( rsib.fix(rpid, LATCH_SH) );
	    if (rsib.nrecs() > 0) {
		btrec_t r(rsib, 0);
		if (key == r.key())  {
		    return TRUE;
		}
		break;
	    }
	    rpid.page = rsib.next();
	}
    }

    return FALSE;
}

/*********************************************************************
 *
 *  btree_m::insert(root, unique, cc, key, el, split_factor)
 *
 *  Insert <key, el> into the btree. Split_factor specifies 
 *  percentage factor in spliting (if it occurs):
 *	60 means split 60/40 with 60% in left and 40% in right page
 *  A normal value for "split_factor" is 50. However, if I know
 *  in advance that I am going insert a lot of sorted entries,
 *  a high split factor would result in a more condensed btree.
 *
 *********************************************************************/
rc_t
btree_m::insert(
    const lpid_t&	root,		// I-  root of btree
    int			nkc,
    const key_type_s*	kc,
    bool		unique,		// I-  TRUE if tree is unique
    concurrency_t	cc,		// I-  concurrency control 
    const cvec_t&	key,		// I-  which key
    const cvec_t&	el,		// I-  which element
    int 		split_factor)	// I-  tune split in %
{
    w_assert1(kc && nkc > 0);
#ifdef DEBUG
    DBG(<<"");
    if(print_sanity) {
	cout << "BEFORE INSERT"<<endl;
	btree_m::print_key_str(root,false);
    }
#endif

    if(key.size() + el.size() > btree_p::max_entry_size) {
	return RC(eRECWONTFIT);
    }

    cvec_t* real_key;
    _scramble_key(real_key, key, nkc, kc);

    return _insert(root, unique, cc, *real_key, el, split_factor);
}


rc_t
btree_m::_insert(
    const lpid_t&	root,		// I-  root of btree
    bool		unique,		// I-  TRUE if tree is unique
    concurrency_t	cc,		// I-  concurrency control 
    const cvec_t&	key,		// I-  which key
    const cvec_t&	el,		// I-  which element
    int 		split_factor)	// I-  tune split in %
{
    btree_p leaf;
    int slot;
    bool found;
    stid_t stid = root.stid();


    do  {
	/*
	 *  Traverse to locate leaf and slot
	 */
	W_DO( _traverse(root, key, el, found, LATCH_EX, leaf, slot) );
	DBG(<<"found = " << found << " leaf=" << leaf.pid() << " slot=" << slot);
	w_assert3(leaf.is_leaf());

	kvl_t kvl;
	if (cc == t_cc_kvl)  mk_kvl(kvl, stid, unique, key, el);

	if (found)  {
	    if (cc == t_cc_kvl)  {
		if (lm->lock(kvl, SH,
			     t_long, WAIT_IMMEDIATE))  {
		    /*
		     *  Another xct is holding a lock ... 
		     *  unfix leaf, wait, and retry
		     */
		    leaf.unfix();
		    W_DO(lm->lock(kvl, SH));
		    continue;
		}
	    }
	    return RC(eDUPLICATE);
	}
	
	if (unique)  {
	    kvl_t dup;
	    if (_check_duplicate(key, leaf, slot, 
				 (cc == t_cc_kvl) ? &dup : 0))  {
		if (cc == t_cc_kvl)  {
		    if (lm->lock(dup, SH, 
				 t_long, WAIT_IMMEDIATE))  {
			/*
			 *  Another xct is holding the lock ...
			 *  unfix leaf, wait, and retry
			 */
		        leaf.unfix();
		        W_DO( lm->lock(dup, SH) );
		        continue;
		    }
		}
		return RC(eDUPLICATE);
	    }
	}
	
	if (cc != t_cc_kvl)
	    break;

	/*
	 *  Obtain KVL lock
	 */
	kvl_t nxt_kvl;
	if (slot < leaf.nrecs())  {
	    /* higher key exists */
	    btrec_t r(leaf, slot);
	    mk_kvl(nxt_kvl, stid, unique, r);
	} else {
	    if (! leaf.next())  {
		nxt_kvl.set(stid, kvl_t::eof);
	    } else {
		lpid_t sib_pid = root;
		sib_pid.page = leaf.next();

		btree_p sib;
		do  {
		    // "following" a link here means fixing the page
		    smlevel_0::stats.bt_links++;

		    W_DO( sib.fix(sib_pid, LATCH_SH) );
		} while (sib.nrecs() == 0 && (sib_pid.page = sib.next()));

		if (sib.nrecs())  {
		    btrec_t r(sib, 0);
		    mk_kvl(nxt_kvl, stid, unique, r);
		} else {
		    nxt_kvl.set(stid, kvl_t::eof);
		}
	    }
	}

	if (lm->lock(nxt_kvl, IX,
		     t_instant, WAIT_IMMEDIATE))  {
	    leaf.unfix();
	    W_DO( lm->lock(nxt_kvl, IX, t_instant));
	    continue;
	}

	/*
	 *  BUGBUG: 
	 *    Right now lock_m does not provide facility to query 
	 *    lock mode of a particular lock held by a particular xct.
	 *    
	 *    Before this is available, take the most conservative
	 *    approach and acquire EX lock. In the future, 
	 *    when lock_m implements better query mechanism, 
	 *    we should implement the full algorithm:
	 *
	 *    if nxt_kvl already locked in X, S or SIX by current xct then
	 *     lmode = X else lmode = IX
	 */
	lock_mode_t lmode;
	lmode = EX;
	if (lm->lock(kvl, lmode, t_long, WAIT_IMMEDIATE))  {
	    leaf.unfix();
	    W_DO( lm->lock(kvl, lmode, t_long) );
	    continue;
	}

	break;
	
    } while (1);
    

    /*
     *  Insert <key, el> into leaf, with logical logging.
     */
    rc_t rc;
    {
	/*
	 *  Turn off log for physical insert into leaf
	 */
	xct_log_switch_t toggle(OFF);
	DBG(<<"insert in leaf " << leaf.pid() <<  " at slot " << slot);
	rc = leaf.insert(key, el, slot);
	if (rc) {
	    if (rc.err_num() != eRECWONTFIT) {
		return RC_AUGMENT(rc);
	    }
	    DBG(<<"leaf is full -- split");

	    /*
	     *  Leaf is full, turn on log for split operation
	     */
	    xct_log_switch_t toggle(ON); 
	    btree_p rsib;
	    int addition = key.size() + el.size() + 2;
	    bool left_heavy;
	    W_DO( _split_page(leaf, rsib, left_heavy,
			    slot, addition, split_factor) );
	    if (! left_heavy)  {
		leaf = rsib;
	    }

	    {
		/*
		 *  Turn off log for physical insert into leaf (as before)
		 */
		xct_log_switch_t toggle(OFF);
		DBG(<<" do the insert into page " << leaf.pid());
		W_COERCE( leaf.insert(key, el, slot) );
	    }
	    CRASHTEST("btree.insert.1");
	}
    }
    CRASHTEST("btree.insert.2");


    /*
     *  Log is on here. Log a logical insert.
     */
    rc = log_btree_insert(leaf, slot, key, el, unique);
    if (rc) {
	/*
	 *  Failed writing log. Manually undo physical insert.
	 */
	xct_log_switch_t toggle(OFF);
	W_COERCE( leaf.remove(slot) );
	return rc.reset();
    }

#ifdef DEBUG
    DBG(<<"");
    if(print_sanity) {
	cout << "AFTER INSERT"<<endl;
	btree_m::print_key_str(root,false);
    }
#endif
    return RCOK;
}




/*********************************************************************
 *
 *  btree_m::remove_key(root, unique, cc, key, num_removed)
 *
 *  Remove all occurences of "key" in the btree, and return
 *  the number of entries removed in "num_removed".
 *
 *********************************************************************/
rc_t
btree_m::remove_key(
    const lpid_t&	root,	// root of btree
    int			nkc,
    const key_type_s*	kc,
    bool		unique, // TRUE if btree is unique
    concurrency_t	cc,	// concurrency control
    const cvec_t&	key,	// which key
    int&		num_removed)
{
    w_assert1(kc && nkc > 0);

    num_removed = 0;

    /*
     *  We do this the dumb way ... optimization needed if this
     *  proves to be a bottleneck.
     */
    while (1)  {
	/*
	 *  scan for key
	 */
	cursor_t cursor;
	W_DO( fetch_init(cursor, root, nkc, kc, 
		     unique, cc, key, cvec_t::neg_inf,
		     ge, 
		     le, key));
	W_DO( fetch(cursor) );
	if (!cursor.key()) {
	    /*
	     *  no more occurence of key ... done! 
	     */
	    break;
	}
	/*
	 *  call btree_m::_remove() 
	 */
	W_DO( remove(root, nkc, kc, unique, cc, key, 
		     cvec_t(cursor.elem(), cursor.elen())) );
	++num_removed;

	if (unique) break;
    }
    if (num_removed == 0)  {
	return RC(eNOTFOUND);
    }

    return RCOK;
}



/*********************************************************************
 *
 *  btree_m::remove(root, unique, cc, key, el)
 *
 *  Remove <key, el> from the btree.
 *
 *********************************************************************/
rc_t
btree_m::remove(
    const lpid_t&	root,	// root of btree
    int			nkc,
    const key_type_s*	kc,
    bool		unique, // TRUE if btree is unique
    concurrency_t	cc,	// concurrency control
    const cvec_t&	key,	// which key
    const cvec_t&	el)	// which el
{
    w_assert1(kc && nkc > 0);

    cvec_t* real_key;
    _scramble_key(real_key, key, nkc, kc);

    return _remove(root, unique, cc, *real_key, el);
}


rc_t
btree_m::_remove(
    const lpid_t&	root,	// root of btree
    bool		unique, // TRUE if btree is unique
    concurrency_t	cc,	// concurrency control
    const cvec_t&	key,	// which key
    const cvec_t&	el)	// which el
{
    FUNC(btree_m::_remove);
    bool found;
    btree_p leaf;
    int slot;
    stid_t stid = root.stid();

    DBG(<<"_remove:");
#ifdef DEBUG
    if(print_remove) {
	cout << "BEFORE _remove" <<endl;
	btree_m::print_key_str(root,false);
    }
#endif

    do {
	DBG(<<"_remove.do:");
	/*
	 *  Traverse to locate leaf and slot
	 */
	W_DO( _traverse(root, key, el, found, LATCH_EX, leaf, slot) );
	DBG(<<"found = " << found << " leaf=" << leaf.pid() << " slot=" << slot);
	if (! found)  {
	    return RC(eNOTFOUND);
	}

	if (cc != t_cc_kvl)  break;

	/*
	 *  KVL lock for this key value
	 */
	kvl_t kvl;
	mk_kvl(kvl, stid, unique, key, el);

	/*
	 *  KVL lock for next key value
	 */
	kvl_t nxt_kvl;
	if (slot < leaf.nrecs() - 1)  {
	    /* higher key exists */
	    btrec_t r(leaf, slot + 1);
	    mk_kvl(nxt_kvl, stid, unique, r);
	} else {
	    if (! leaf.next())  {
		nxt_kvl.set(stid, kvl_t::eof);
	    } else {
		lpid_t sib_pid = root;
		sib_pid.page = leaf.next();
		btree_p sib;
		do {
		    // "following" a link here means fixing the page
		    smlevel_0::stats.bt_links++;

		    W_DO( sib.fix(sib_pid, LATCH_SH) );
		} while (sib.nrecs() == 0 && (sib_pid.page = sib.next()));

		if (sib.nrecs())  {
		    btrec_t r(sib, 0);
		    mk_kvl(nxt_kvl, stid, unique, r);
		} else {
		    nxt_kvl.set(stid, kvl_t::eof);
		}
	    }
	}

	/*
	 *  Lock next key value in EX
	 */
	if (lm->lock(nxt_kvl, EX, t_long, WAIT_IMMEDIATE))  {
	    leaf.unfix();
	    W_DO( lm->lock(nxt_kvl, EX) );
	    continue;		/* retry */
	}

	/*
	 *  Lock this key value in EX
	 *  The lock duration can be t_instant if this is a unique
	 *  index or this is definitely the last instance of this key
	 *  otherwise t_long must be used.
	 */
	lock_duration_t duration;
	if (unique) {
	    duration = t_instant;
	} else {
	    duration = t_long;
	}
	if (lm->lock(kvl, EX, duration, WAIT_IMMEDIATE))  {
	    leaf.unfix();
	    W_DO( lm->lock(kvl, EX, duration) );
	    continue;		/* retry */
	}
	
	break;

    } while (1);
	
    /*
     *  Turn off logging and perform physical remove.
     */
    {
	xct_log_switch_t toggle(OFF);
	DBG(<<" leaf: " << leaf.pid() << " removing slot " << slot);
	W_DO( leaf.remove(slot) );
    }

    CRASHTEST("btree.remove.1");
    /*
     *  Log is on here. Log a logical remove.
     */
    rc_t rc = log_btree_remove(leaf, slot, key, el, unique);
    if (rc)  {
	/*
	 *  Failed writing log. Manually undo physical remove.
	 */
	xct_log_switch_t toggle(OFF);
	W_COERCE( leaf.insert(key, el, slot) );
	return rc.reset();
    }

    CRASHTEST("btree.remove.3");
    /*
     *  Remove empty page
     */
    bool unlinked_page = false;
    while (leaf.nrecs() == 0)  {
	DBG(<<" unlinking page: leaf "  << leaf.pid()
	    << " nrecs " << leaf.nrecs()
	    << " is_phantom " << leaf.is_phantom()
	);
	CRASHTEST("btree.remove.2");
	w_rc_t rc = leaf.unlink();
	unlinked_page = true;
	if (rc)  {
	    if (rc.err_num() != ePAGECHANGED)  return rc.reset();
	    if (! leaf.is_phantom()) 
		continue; // retry 
	}
	break;
    }

    if (unlinked_page) {
	DBG(<<" retraverse: " );
	// a page was unlinked, so re-travers in order to remove it
	CRASHTEST("btree.remove.4");
	W_COERCE( _traverse(root, key, el, found, LATCH_EX, leaf, slot) );
	DBG(<<"found = " << found << " leaf=" << leaf.pid() << " slot=" << slot);
	w_assert3(!found);
    }

    return RCOK;
}




/*********************************************************************
 *
 *  btree_m::lookup(root, unique, cc, key_to_find, el, elen, found)
 *
 *  Find key in btree. If found, copy up to elen bytes of the 
 *  entry element into el. 
 *
 *********************************************************************/
rc_t
btree_m::lookup(
    const lpid_t& 	root,	// I-  root of btree
    int			nkc,
    const key_type_s*	kc,
    bool		unique, // I-  TRUE if btree is unique
    concurrency_t	cc,	// I-  concurrency control
    const cvec_t& 	key,	// I-  key we want to find
    void* 		el,	// I-  buffer to put el found
    smsize_t& 		elen,	// IO- size of el
    bool& 		found)	// O-  TRUE if key is found
{
    w_assert1(kc && nkc > 0);

    cvec_t* real_key;
    _scramble_key(real_key, key, nkc, kc);

    W_DO( _lookup(root, unique, cc, *real_key, el, elen, found));
    return RCOK;
}


rc_t
btree_m::lookup_prev(
    const lpid_t& 	root,	// I-  root of btree
    int			nkc,
    const key_type_s*	kc,
    bool		unique, // I-  TRUE if btree is unique
    concurrency_t	cc,	// I-  concurrency control
    const cvec_t& 	keyp,	// I-  find previous key for key
    bool& 		found,	// O-  TRUE if a previous key is found
    void* 		key_prev,	// I- is set to
					//    nearest one less than key
    smsize_t& 		key_prev_len)	// IO- size of key_prev
{
    w_assert1(key_prev && kc && nkc > 0);

    cvec_t* real_key;
    _scramble_key(real_key, keyp, nkc, kc);
    cvec_t& key = *real_key;

    stid_t stid = root.stid();

    /*
     * The following block of code is based on a similar block
     * in _lookup()
     */
    {
	found = false;
	btree_p p1, p2;
	btree_p* child = &p1;	// child points to p1 or p2
	int slot;
	cvec_t null;

	/*
	 *  Walk down the tree
	 */
	W_DO( _traverse(root, key, null, found, LATCH_SH, p1, slot) );
	DBG(<<"found = " << found << " p1=" << p1.pid() << " slot=" << slot);

	// the previous key is in the previous slot
	if (slot > p1.nrecs()) {
	    slot = p1.nrecs();
	}
	slot--;

	/*
	 *  Get a handle on the record and its KVL
	 */
	kvl_t	kvl;
	btrec_t	rec;
	lpid_t  pid = root;  // eventually holds pid containing key 
	if (slot >= 0 )  {
	    // the previous slot is on page p1
	    rec.set(p1, slot);
	    if (cc == t_cc_kvl) mk_kvl(kvl, stid, unique, rec);
	    pid = p1.pid();
	} else {
	    // the previous slot is on the nearest, non-empty, 
	    // previous page
	    slot = 0;
	    pid.page = p1.prev();
	    p1.unfix(); // unfix current page to avoid latch deadlock
	    for (; pid.page; pid.page = p2.prev()) {

		// "following" a link here means fixing the page
		smlevel_0::stats.bt_links++;

		W_DO( p2.fix(pid, LATCH_SH) );
		if (p2.nrecs())  
		    break;
		w_assert3(p2.is_phantom());
	    }
	    if (pid.page)  {
		child = &p2;
		rec.set(p2, p2.nrecs()-1);
	    } else {
		if (p2) p2.unfix();
	    } 
	}

	if (rec) {
	    // we have a previous record
	    found = true;
	    rec.key().copy_to(key_prev, key_prev_len);
	    key_prev_len = MAX(rec.klen(), key_prev_len);
	} else {
	    // there is no previous record
	    found = false;
	    key_prev_len = 0;
	}
	p1.unfix();
	p2.unfix();
    }

    if (found) {
	/*
	 * need to unscramble key
	 */
	cvec_t found_key(key_prev, key_prev_len);
	_unscramble_key(real_key, found_key, nkc, kc);
	real_key->copy_to(key_prev);
    }
    return RCOK;
}


rc_t
btree_m::_lookup(
    const lpid_t& 	root,	// I-  root of btree
    bool		unique, // I-  TRUE if btree is unique
    concurrency_t	cc,	// I-  concurrency control
    const cvec_t& 	key,	// I-  key we want to find
    void* 		el,	// I-  buffer to put el found
    smsize_t& 		elen,	// IO- size of el
    bool& 		found)	// O-  TRUE if key is found
{
    stid_t stid = root.stid();
 again:
    DBG(<<"_lookup.again");
    {
	found = false;
	btree_p p1, p2;
	btree_p* child = &p1;	// child points to p1 or p2
	int slot;
	cvec_t null;

	/*
	 *  Walk down the tree
	 */
	W_DO( _traverse(root, key, null, found, LATCH_SH, p1, slot) );
	DBG(<<"found = " << found << " p1=" << p1.pid() << " slot=" << slot);

	/*
	 *  Get a handle on the record and its KVL
	 */
	kvl_t	kvl;
	btrec_t	rec;
	if (slot < p1.nrecs())  {
	    rec.set(p1, slot);
	    if (cc == t_cc_kvl) mk_kvl(kvl, stid, unique, rec);

	    // the only way the _traverse could return found==true
	    // is if the element length was zero
	    w_assert3(!found || rec.elen() == 0);
	} else {
	    w_assert3(!found);
	    slot = 0;
	    lpid_t pid = root;
	    for (pid.page = p1.next(); pid.page; pid.page = p2.next()) {

		// "following" a link here means fixing the page
		smlevel_0::stats.bt_links++;

		W_DO( p2.fix(pid, LATCH_SH) );
		if (p2.nrecs())  
		    break;
		w_assert3(p2.is_phantom());
	    }
	    if (pid.page)  {
		child = &p2;
		rec.set(p2, 0);
		if (cc == t_cc_kvl)
		    mk_kvl(kvl, stid, unique, rec);
	    } else {
		if (cc == t_cc_kvl)
		    kvl.set(stid, kvl_t::eof);
		if (p2) p2.unfix();
	    } 
	}

	if (cc == t_cc_kvl)  {
	    /*
	     *  Lock current entry
	     */
	    if (lm->lock(kvl, SH, 
			 t_long, WAIT_IMMEDIATE)) {
		/*
		 *  Failed to get lock immediately. Unfix pages
		 *  and try to wait for the lock.
		 */
	        lsn_t lsn = child->lsn();
	        lpid_t pid = child->pid();
	        p1.unfix();
	        p2.unfix();
	        W_DO( lm->lock(kvl, SH) );

		/*
		 *  Got the lock. Fix child. If child has
		 *  changed (lsn does not match) then retry.
		 */
	        W_DO( child->fix(pid, LATCH_SH) );
	        if (lsn == child->lsn() && child == &p1)  {
		    /* do nothing */;
	        } else {
		    DBG(<<"->again");
		    goto again;	// retry
	        }
	    }
	}

	if (! rec)  {
	    found = false;
	} else {
	    if (found = (key == rec.key()))  {
		// Copy the element data assume caller provided space
		if (el) {
		    if (elen < rec.elen())  {
			return RC(eRECWONTFIT);
		    }
		    elen = rec.elen();
		    rec.elem().copy_to(el, elen);
		}
	    }
	}
	return RCOK;
    }
}




/*********************************************************************
 *
 *  btree_m::fetch_init(cursor, root, numkeys, unique, 
 *        is-unique, cc, key, elem,
 *        cond1, cond2, bound2)
 *
 *  Initialize cursor for a scan for entries greater than or 
 *  equal to <key, elem>.
 *
 *********************************************************************/
rc_t
btree_m::fetch_init(
    cursor_t& 		cursor, // IO- cursor to be filled in
    const lpid_t& 	root,	// I-  root of the btree
    int			nkc,
    const key_type_s*	kc,
    bool		unique,	// I-  TRUE if btree is unique
    concurrency_t	cc,	// I-  concurrency control
    const cvec_t& 	ukey,	// I-  <key, elem> to start
    const cvec_t& 	elem,	// I-
    cmp_t		cond1,	// I-  condition on lower bound
    cmp_t		cond2,	// I-  condition on upper bound
    const cvec_t&	bound2)	// I-  upper bound
{
    w_assert1(kc && nkc > 0);

    /*
     *  Initialize constant parts of the cursor
     */
    cvec_t* key;
    cvec_t* bound2_key;

    _scramble_key(bound2_key, bound2, nkc, kc);

    W_DO(cursor.set_up(root, nkc, kc, unique, cc, 
	cond2, *bound2_key));

    _scramble_key(key, ukey, nkc, kc);
    W_DO(cursor.set_up_part_2( cond1, *key));

    cursor.first_time = true;

    return _fetch_init(cursor, key, elem);
}


/*********************************************************************
 *
 *  btree_m::fetch_reinit(cursor)
 *
 *  Reinitialize cursor for a scan.
 *
 *********************************************************************/
rc_t
btree_m::fetch_reinit(
    cursor_t& 		cursor) // IO- cursor to be filled in
{
    cursor.first_time = true;
    return _fetch_init(cursor, &cvec_t(cursor.key(), cursor.klen()),
			cvec_t(cursor.elem(), cursor.elen()));
}


/*********************************************************************
 *
 *  btree_m::_fetch_init(cursor, key, elem)
 *
 *  Initialize cursor for a scan for entries greater than or 
 *  equal to <key, elem>.
 *
 *********************************************************************/
rc_t
btree_m::_fetch_init(
    cursor_t& 		cursor, // IO- cursor to be filled in
    cvec_t* 		key,	// I-
    const cvec_t& 	elem)	// I-
{
    FUNC(btree_m::_fetch_init);

    const lpid_t& root	= cursor.root();
    stid_t stid 	= root.stid();
    bool unique		= cursor.unique();
    concurrency_t cc	= cursor.cc();
    int slot            = -1;
    bool  __eof;

  again:
    DBG(<<"_fetch_init.again");
    {
	bool 		found;
	btree_p 	p1, p2;
	btree_p* 	child = &p1;	// child points to p1 or p2

	/*
	 *  Walk down the tree
	 */
	W_DO( _traverse(root, *key, elem, found, LATCH_SH, *child, slot) );
	DBG(<<"found = " << found << " *child=" << child->pid() << " slot=" << slot);

	kvl_t kvl;
	if (slot >= child->nrecs())  {
	    /*
	     *  Move to right sibling
	     */
	    w_assert3(slot == child->nrecs());
	    _skip_one_slot(p1, p2, child, slot, __eof);
	    if(__eof) {
		// we hit the end.
		cursor.keep_going = false;
		cursor.free_rec();
	    }
	}

        while (cursor.keep_going) {
	    if (cc == t_cc_kvl)  {
		/*
		 *  Get KVL locks
		 */
		if (slot >= child->nrecs()) {
		    w_assert3(slot == child->nrecs());
		    kvl.set(stid, kvl_t::eof); 
		} else {
		    w_assert3(slot < child->nrecs());
		    btrec_t r(*child, slot);
		    mk_kvl(kvl, stid, unique, r);
		}

		if (lm->lock(kvl, SH, t_long, WAIT_IMMEDIATE))  {
		    lpid_t pid = child->pid();
		    lsn_t lsn = child->lsn();
		    p1.unfix();
		    p2.unfix();
		    W_DO( lm->lock(kvl, SH) );
		    W_DO( child->fix(pid, LATCH_SH) );
		    if (lsn == child->lsn() && child == &p1)  {
			;
		    } else {
			DBG(<<"->again");
			goto again;	// retry
		    }
		}
	    }

	    if (slot >= child->nrecs())  {
		/*
		 *  Reached EOF
		 */
		cursor.free_rec();
	    } else {
		/*
		 *  Point cursor to first satisfying key
		 */
		W_DO( cursor.make_rec(*child, slot) );
		if(cursor.keep_going) {
		    _skip_one_slot(p1, p2, child, slot, __eof);
		    if(__eof) {
			// no change : we're at the end
			cursor.keep_going = false;
			cursor.free_rec();
		    }
		}
	    }
	}
    }

    return RCOK;
}



/*********************************************************************
 *
 *  btree_m::fetch(cursor)
 *
 *  Fetch the next key of cursor, and advance the cursor.
 *
 *********************************************************************/
rc_t
btree_m::fetch(cursor_t& cursor)
{
    FUNC(btree_m::fetch);
    bool __eof;
    if (cursor.first_time)  {
	/*
	 *  Fetch_init() already placed cursor on
	 *  first satisfying key. Done.
	 */
	cursor.first_time = false;
	return RCOK;
    }

    /*
     *  We now need to move cursor one slot to the right
     */
    stid_t stid = cursor.root().stid();
    int slot = -1;
  again: 
    DBG(<<"fetch.again");
    {
	btree_p p1, p2;
	btree_p* child = &p1;	// child points to p1 or p2

	while (cursor.is_valid()) {
	    /*
	     *  Fix the cursor page. If page has changed (lsn
	     *  mismatch) then call fetch_init to re-traverse.
	     */
	    W_DO( child->fix(cursor.pid(), LATCH_SH) );
	    if (cursor.lsn() == child->lsn())  {
		break;
	    }
	    p1.unfix();
	    p2.unfix();
	    W_DO( fetch_reinit(cursor));
	    cursor.first_time = FALSE;
	}

	if (cursor.is_valid())  {
	    w_assert3(child->pid() == cursor.pid());

	    /*
	     *  Move one slot to the right
	     */
	    slot = cursor.slot();
	    _skip_one_slot(p1, p2, child, slot, __eof);

	    if (cursor.cc() == t_cc_kvl)  {
		/*
		 *  Get KVL locks
		 */
		kvl_t kvl;
		if (slot >= child->nrecs())  {
		    kvl.set(stid, kvl_t::eof);
		} else {
		    w_assert3(slot < child->nrecs());
		    btrec_t r(*child, slot);
		    mk_kvl(kvl, stid, cursor.unique(), r);
		}
	
		if (lm->lock(kvl, SH, 
			     t_long, WAIT_IMMEDIATE))  {
		    lpid_t pid = child->pid();
		    lsn_t lsn = child->lsn();
		    p1.unfix();
		    p2.unfix();
		    W_DO( lm->lock(kvl, SH) );
		    W_DO( child->fix(pid, LATCH_SH) );
		    if (lsn == child->lsn() && child == &p1)  {
			;
		    } else {
			DBG(<<"->again");
			goto again;
		    }
		}
	    }
	    if (slot >= child->nrecs()) {
		/*
		 *  Reached EOF
		 */
		cursor.free_rec();

	    } else {
		/*
		 *  Point cursor to satisfying key
		 */
		W_DO( cursor.make_rec(*child, slot) );
		if(cursor.keep_going) {
		    // keep going until we reach the
		    // first satisfying key.
		    // This should only happen if we
		    // have something like:
		    // >= x && == y where y > x
		    DBG(<<"->again");
		    goto again;
		}
	    }
	} else {
	    w_assert3(!cursor.key());
	}
    }

    return RCOK;
}



/*********************************************************************
 *
 *  btree_m::get_du_statistics(root, stats, audit)
 *
 *********************************************************************/
rc_t
btree_m::get_du_statistics(
    const lpid_t&	root,
    btree_stats_t&	stats,
    bool 		audit)
{
    lpid_t pid = root;
    lpid_t child = root;
    child.page = 0;

    base_stat_t	lf_cnt = 0;
    base_stat_t	int_cnt = 0;
    base_stat_t	level_cnt = 0;

    btree_p page[2];
    btrec_t rec[2];
    int c = 0;

    /*
       Traverse the btree gathering stats.  This traversal scans across
       each level of the btree starting at the root.  Unfortunately,
       this scan misses "unlinked" pages.  Unlinked pages are empty
       and will be free'd during the next top-down traversal that
       encounters them.  This traversal should really be DFS so it
       can find "unlinked" pages, but we leave it as is for now.
       We account for the unlinked pages after the traversal.
    */
    do {
	btree_lf_stats_t	lf_stats;
	btree_int_stats_t	int_stats;

	W_DO( page[c].fix(pid, LATCH_SH) );
	if (page[c].level() > 1)  {
	    int_cnt++;;
	    W_DO(page[c].int_stats(int_stats));
	    if (audit) {
		W_DO(int_stats.audit());
	    }
	    stats.int_pg.add(int_stats);
	} else {
	    lf_cnt++;
	    W_DO(page[c].leaf_stats(lf_stats));
	    if (audit) {
		W_DO(lf_stats.audit());
	    }
	    stats.leaf_pg.add(lf_stats);
	}
	if (page[c].prev() == 0)  {
	    child.page = page[c].pid0();
	    level_cnt++;
	}
	if (! (pid.page = page[c].next()))  {
	    pid = child;
	    child.page = 0;
	}
	c = 1 - c;

	// "following" a link here means fixing the page,
	// which we'll do on the next loop through, if pid.page
	// is non-zero
	smlevel_0::stats.bt_links++;
    } while (pid.page);

    // count unallocated pages
    rc_t rc;
    bool allocated;
    base_stat_t alloc_cnt = 0;
    base_stat_t unlink_cnt = 0;
    base_stat_t unalloc_cnt = 0;
    rc = io->first_page(root.stid(), pid, &allocated);
    while (!rc) {
	// no error yet;
	if (allocated) {
	    alloc_cnt++;
	} else {
	    unalloc_cnt++;
	}
	rc = io->next_page(pid, &allocated);
    }
    unlink_cnt = alloc_cnt - (lf_cnt + int_cnt);
    if (rc.err_num() != eEOF) return rc;

    if (audit) {
	if (!((alloc_cnt+unalloc_cnt) % smlevel_0::ext_sz == 0)) {
	    return RC(fcINTERNAL);
	}
        if (!((lf_cnt + int_cnt + unlink_cnt + unalloc_cnt) % 
			smlevel_0::ext_sz == 0)) {
	    return RC(fcINTERNAL);
	}
    }

    stats.unalloc_pg_cnt += unalloc_cnt;
    stats.unlink_pg_cnt += unlink_cnt;
    stats.leaf_pg_cnt += lf_cnt;
    stats.int_pg_cnt += int_cnt;
    stats.level_cnt = MAX(stats.level_cnt, level_cnt);
    return RCOK;
}


/*********************************************************************
 *
 *  btree_m::copy_1_page_tree(old_root, new_root)
 *
 *  copy_1_page_tree is used by the sm index code when a 1-page
 *  btree (located in a special store) needs to grow to
 *  multiple pages (ie. have a store of its own).
 *
 *********************************************************************/
rc_t
btree_m::copy_1_page_tree(const lpid_t& old_root, const lpid_t& new_root)
{
    FUNC(btree_m::copy_1_page_tree);

    btree_p old_root_page;
    btree_p new_root_page;
    W_DO(old_root_page.fix(old_root, LATCH_EX));
    W_DO(new_root_page.fix(new_root, LATCH_EX));

    if (old_root_page.nrecs() == 0) {
	// nothing to do
	return RCOK;
    }

    // This operation is compensated, since it cannot be
    // physically un-done.  Physical undo is impossible, since
    // future inserts may move records to other pages.


    lsn_t anchor;
    if (xct())  anchor = xct()->anchor();


    // copy entries from the old page to the new one.
    X_DO(old_root_page.copy_to_new_page(new_root_page));


    if (xct())  {
	CRASHTEST("toplevel.btree.3");
	xct()->compensate(anchor);
    }

    CRASHTEST("btree.1page.2");

    return RCOK;
}
   




/*********************************************************************
 *
 *  btree_m::_alloc_page(root, level, near, ret_page, pid0)
 *
 *  Allocate a page for btree rooted at root. Fix the page allocated
 *  and return it in ret_page.
 *
 *  	============ IMPORTANT ============
 *  This function should be called in a compensating
 *  action so that it does not get undone if the xct
 *  abort. The page allocated would become part of
 *  the btree as soon as other pages started pointing
 *  to it. So, page allocation as well as other
 *  operations to linkup the page should be a single
 *  top-level action.
 *
 *********************************************************************/
rc_t
btree_m::_alloc_page(
    const lpid_t& root, int2 level,
    const lpid_t& near,
    btree_p& ret_page,
    shpid_t pid0)
{
    FUNC(btree_m::_alloc_page);

    lpid_t pid;
    w_assert3(near != lpid_t::null);

    // if this is a 1-page btree, return that it needs to grow
    if (root.store() == small_store_id) {
	return RC(e1PAGESTORELIMIT);
    }

    W_DO( io->alloc_pages(root.stid(), near, 1, &pid) );

    W_DO( ret_page.fix(pid, LATCH_EX, ret_page.t_virgin) );
    
    DBG(<<"allocated btree page " << pid << " at level " << level);

    W_DO( ret_page.set_hdr(root.page, level, pid0, 0) );

    return RCOK;
}




/*********************************************************************
 *
 *  btree_m::_traverse(root, key, elem, found, mode, leaf, slot)
 *
 *  Traverse the btree to find <key, elem>. Return the leaf and slot
 *  that <key, elem> resides or, if not found, the leaf and slot 
 *  where <key, elem> SHOULD reside.
 *
 *********************************************************************/
rc_t
btree_m::_traverse(
    const lpid_t&	root,	// I-  root of tree 
    const cvec_t&	key,	// I-  target key
    const cvec_t&	elem,	// I-  target elem
    bool& 		found,	// O-  TRUE if sep is found
    latch_mode_t 	mode,	// I-  EX for insert/remove, SH for lookup
    btree_p& 		leaf,	// O-  leaf satisfying search
    int& 		slot)	// O-  slot satisfying search
{
    FUNC(btree_m::_traverse);
  again:
    DBG(<<"_traverse.again:");
#ifdef DEBUG
    DBG(<<"");
    if(print_traverse) {
	cout << "TRAVERSE.again "<<endl;
	if(print_wholetree) {
	    btree_m::print_key_str(root,false);
	}
    }
#endif
    {
	btree_p p[2];		// for latch coupling
	lpid_t  pid[2];		// for latch coupling
	lsn_t   lsns[2];	// for remembering lsns // NEH
		// TODO: either use lsns[] to avoid re-traversals OR
		// remove this lsns[] if not needed to remember NEH
		// lsns -- it's not clear how the rememebering
		// works in the orig code

	int c;			// toggle for p[] and pid[]. 
				// p[1-c] is child of p[c].

	w_assert3(! p[0]);
	w_assert3(! p[1]);
	c = 0;

	found = FALSE;

	pid[0] = pid[1] = root;

	/*
	 *  Fix root
	 */
	W_DO( p[c].fix(pid[c], mode) );
	lsns[c] = p[c].lsn(); // NEH

	if (p[c].is_smo())  {
	    /*
	     *  Grow tree and re-traverse
	     */
	    w_assert3(p[c].next());
	    W_DO( p[c].fix(pid[c], LATCH_EX) );
	    W_DO( _grow_tree(p[c]) );
	    DBG(<<"->again");
	    goto again;

	} if (p[c].is_phantom())  {
	    /*
	     *  Shrink the tree
	     */
	    w_assert3(p[c].nrecs() == 0);
	    W_DO( p[c].fix(pid[c], LATCH_EX) );
	    W_DO( _shrink_tree(p[c]) );
	    DBG(<<"->again");
	    goto again;
	}

	/*
	 *  Traverse towards the leaf with latch-coupling
	 */
	for ( ; p[c].is_node(); c = 1 - c)  {

#ifdef DEBUG
	if(print_traverse)
	{
	    vec_t k;
	    k.put(key, 0, key.size());
	    cout << " search for key ";
	    cout.write(k.ptr(0),k.len(0));
	    cout << " in leaf " << p[c].pid()  <<endl;
	}
#endif

	    W_COERCE( _search(p[c], key, elem, found, slot) );
	    if (! found) --slot;
	    DBG(<<" found = " << found << " slot=" << slot);

	    /*
	     *  get pid of the child, and fix it
	     */
	    pid[1-c].page = ((slot < 0) ? p[c].pid0() : p[c].child(slot));
	    W_DO( p[1-c].fix(pid[1-c], mode) );
	    lsns[1-c] = p[1-c].lsn(); // NEH

	    DBG(<<" inspecting slot " << 1-c << " page=" << p[1-c].pid()
		<< " for smo " << p[1-c].is_smo()
	    );

	    while (p[1-c].is_smo()) {
		/*
		 *  Child has unpropagated split. Determine which
		 *  child suits us, and propagate the split.
		 */
		DBG(<<"child " << 1-c 
			<< " page " << p[1-c].pid()
			<< " has un-propagated split");
		w_assert3(! p[1-c].is_phantom());
		if (mode != LATCH_EX) {
		    p[1-c].unfix();	// precaution to avoid deadlock
		    W_DO( p[c].fix(pid[c], LATCH_EX) );
		    lsns[c] = p[c].lsn(); // NEH
		    W_DO( p[1-c].fix(pid[1-c], LATCH_EX) );
		    lsns[1-c] = p[1-c].lsn(); // NEH
		}
		    
		/*  
		 *  Fix right sibling
		 */

		lpid_t rsib_pid = pid[1-c]; // get vol.store
		rsib_pid.page = p[1-c].next(); // get page
		btree_p rsib;
		DBG(<<"right sib is " << rsib_pid);

		// "following" a link here means fixing the page
		smlevel_0::stats.bt_links++;
		W_DO( rsib.fix(rsib_pid, LATCH_EX) );

		/*  (whole section added by NEH)
		 *  We have 2 cases here: the
		 *  insertion that causes the SMO bit to be
		 *  set did/did not complete (before a crash)
		 *  (say that we are in recovery right now..)
		 *  In the former case, right-sibling.nrecs()>0.
		 *  In the latter case, right-sibling might be empty!
		 *
		 *  Because splits are propagated AFTER the records
		 *  are inserted into the leaf, we can safely assume
		 *  that the right-sibling has no parent (pointing to it).
		 */
		if( rsib.nrecs() == 0) {// NEH: whole section
		    w_assert1(rsib.is_smo());  


		    /*
		     * This page has to be 
		     * removed instead of propagating the
		     * split
		     *
		     * It's similar to cut_page, but we're
		     * ONLY doing the unlink.  
		     * We have to compensate it.
		     */
		    {
			lsn_t anchor;
			xct_t* xd = xct();
			w_assert1(xd);
			anchor = xd->anchor();

			// Clear smo in the left-hand page 
			X_DO( p[1-c].clr_smo() );
			CRASHTEST("btree.smo.1");

			// unlink the right-hand page
			X_DO( rsib.unlink() );

			CRASHTEST("toplevel.btree.12");
			xd->compensate(anchor);
		    }
		    DBG(<<" rsib should now be unfixed ");
		    goto again;
		} // end NEH
		CRASHTEST("btree.smo.2");

		btrec_t r0(rsib, 0);

#ifdef DEBUG
		{
		    vec_t k;
		    k.put(r0.key(), 0, r0.key().size());

		    DBG(<<"right sib key is " << k.ptr(0));
		    DBG(<< "r0.key() < key = " << (bool) (r0.key() < key));
		    DBG(<< "r0.key() == key = " << (bool) (r0.key() == key));
		    DBG(<< "r0.elem() == elem = " << (bool) (r0.elem() == elem));
		}
#endif
		if (r0.key() < key ||
		    (r0.key() == key && r0.elem() <= elem))  {
		    /*
		     *  We will be visiting right sibling
		     */
		    DBG(<< "visit right siblling");
		    p[1-c] = rsib;
		    pid[1-c]  = rsib_pid;
		}
		
		/*
		 *  Propagate the split
		 */
		DBG(<<"propagate split, page " 
			<< p[c].pid() << "==" << pid[c]);

		W_DO( _propagate_split(p[c], slot) );

		if (p[c].is_smo())  {
		    DBG(<<"parent page " << c << " split ... re-traverse");
		    /*
		     *  parent page split ... re-traverse
		     */
		    p[c].unfix();
		    DBG(<<"->again");
		    goto again;
		}
		w_assert3(! p[1-c].is_smo() ); // not handled

		// Have to re-traverse because we might now NEH
		// want to find ourselves inspecing a different NEH
		// page for the insert, after the split was propagated NEH
		DBG(<<"->again"); // NEH
		goto again; // NEH
	    }
	    
	    bool removed_phantom = false; // NEH
	    while (p[1-c].is_phantom())  {
		DBG(<<"child page " << p[1-c].pid() << " is empty");
		/*
		 *  Child page is empty. Try to remove it.
		 */
	        // w_assert3(p[1-c].is_smo()); // NEH

		smlevel_0::stats.bt_cuts++;

		w_assert3(p[1-c].pid0() == 0);
		w_assert3(p[1-c].nrecs() == 0);

		p[1-c].unfix();
		W_DO( p[c].fix(pid[c], LATCH_EX) );
		lsns[c] = p[c].lsn(); // NEH

		/*
		 *  Cut out index entry in parent into the empty child
		 */
		W_DO( _cut_page(p[c], slot) );
		removed_phantom = true; // NEH
		--slot;

		if (p[c].is_phantom())  {
		    /*
		     *  Parent page is now empty ... re-traverse
		     */
		    DBG(<<"parent page " << c << " is empty");
		    w_assert3(p[c].nrecs() == 0 && p[c].pid0() == 0);
		    p[c].unfix();
		    DBG(<<"->again");
		    goto again;
		}
		w_assert3(p[c].pid0());
		w_assert3(slot < p[c].nrecs());

		/*
		 *  Find and fix the new child page
		 */
		pid[1-c].page = ((slot < 0) ?
				 p[c].pid0() : p[c].child(slot));
		DBG(<<"new child page is " << pid[1-c]);

		w_assert3(pid[1-c].page);
		W_DO( p[1-c].fix(pid[1-c], mode) );
		lsns[1-c] = p[1-c].lsn(); //NEH
	    }

	    p[c].unfix();	// unfix parent
	    if(removed_phantom) { // NEH
		// have to re-traverse because the process // NEH
		// of removing phantoms might leave us  // NEH
		// a prior page that happens to have an  // NEH
		// unfinished SMO to propagate... // NEH

		DBG(<<"->again"); // NEH
		goto again; // NEH
	    } // NEH
	}

	/*
	 *  Set leaf to fix pid[c]
	 */
	DBG(<<"p[c].pid()= " << p[c].pid()
		<< " lsn is " << p[c].lsn()
		<< " saved lsn is " << lsns[c]
		);
	lsn_t lsn = p[c].lsn();
	lpid_t leaf_pid = p[c].pid();
	p[c].unfix();
	W_DO( leaf.fix(leaf_pid, mode) );
	DBG(<<"leaf is " << leaf.pid()
		<< "leafpid is " << leaf_pid
		<< " leaf.lsn is " << leaf.lsn()
	    );

	if (leaf.lsn() != lsn)  {
	    /*
	     *  Leaf page changed. Re-traverse.
	     */
	    leaf.unfix();
	    DBG(<<"->again");
	    goto again;
	}

	w_assert3(leaf.is_leaf());
	w_assert3(! leaf.is_phantom());
	w_assert3(! leaf.is_smo());
	
	/*
	 *  Search for <key, elem> in the leaf
	 */
	found = false;
	slot = 0;
#ifdef DEBUG
	if(print_traverse)
	{
	    vec_t k;
	    k.put(key, 0, key.size());
	    cout << " search for key ";
	    cout.write(k.ptr(0),k.len(0));
	    cout << " in leaf " << leaf.pid() <<endl;
	}
#endif
	W_COERCE( _search(leaf, key, elem, found, slot) );
	DBG(<<" key = " << key );

	// NB: this "found" will be true iff match for both
	// key & value are found. 
	DBG(<<" found = " << found << " slot= " << slot);
    }

#ifdef DEBUG
    DBG(<<"");
    if(print_traverse) {
	cout << "END OF TRAVERSE "<<endl;
	if(print_wholetree) {
	    btree_m::print_key_str(root,false);
	}
    }
#endif
    return RCOK;
}



/*********************************************************************
 *
 *  btree_m::_cut_page(parent, slot)
 *
 *  Remove the child page of separator at "slot" in "parent". This
 *  action is compensated.
 *
 *********************************************************************/
rc_t
btree_m::__cut_page(btree_p& parent, int slot)
{
    /*
     *  Get the child pid
     */
    lpid_t pid = parent.pid();
    pid.page = (slot < 0) ? parent.pid0() : parent.child(slot);

    W_IFDEBUG(btree_p child);
    W_IFDEBUG(W_COERCE(child.fix(pid, LATCH_SH)));
    W_IFDEBUG(w_assert1(child.is_phantom()));
    
    /*
     *  Free the child
     */
    W_DO( io->free_page(pid) );

    /*
     *  Remove the slot from the parent
     */
    if (slot >= 0)  {
	W_DO( parent.remove(slot) );
    } else {
	/*
	 *  Special case for removing pid0 of parent. 
	 *  Delete first sep and set its child pointer as pid0.
	 */
	shpid_t pid0 = 0;
	if (parent.nrecs() > 0)   {
	    pid0 = parent.child(0);
	    W_DO( parent.remove(0) );
	}
	W_DO( parent.set_pid0(pid0) );
	slot = 0;
    }

    /*
     *  if pid0 == 0, then page must be empty
     */
    w_assert3(parent.pid0() != 0 || parent.nrecs() == 0);

    while (parent.pid0() == 0 && parent.nrecs() == 0)   {
	/*
	 *  Parent is now empty. Unlink and make it a phantom.
	 */
	w_rc_t rc = parent.unlink();
	if (rc)  {
	    if (rc.err_num() != ePAGECHANGED)  return rc.reset();
	    if (! parent.is_phantom())
		continue;	// retry
	}
	break;
    }
    return RCOK;
}
rc_t
btree_m::_cut_page(btree_p& parent, int slot)
{
    lsn_t anchor;
    xct_t* xd = xct();
    w_assert1(xd);
    anchor = xd->anchor();
    X_DO(__cut_page(parent, slot));

    CRASHTEST("toplevel.btree.4");
    xd->compensate(anchor);
    return RCOK;
}




/*********************************************************************
 *
 *  btree_m::_propagate_split(parent, slot)
 *
 *  Propagate the split child in "slot". 
 *
 *********************************************************************/
rc_t
btree_m::__propagate_split(
    btree_p& parent, 	// I-  parent page 
    int slot)		// I-  child that did a split (-1 ==> pid0)
{
    /*
     *  Fix first child
     */
    lpid_t pid = parent.pid();
    pid.page = ((slot < 0) ? parent.pid0() : parent.child(slot));
    btree_p c1;
    W_DO( c1.fix(pid, LATCH_EX) );
    w_assert3(c1.is_smo());

    w_assert1(c1.nrecs());
    btrec_t r1(c1, c1.nrecs() - 1);

    /*
     *  Fix second child
     */
    pid.page = c1.next();
    btree_p c2;

    // "following" a link here means fixing the page
    smlevel_0::stats.bt_links++;
    W_DO( c2.fix(pid, LATCH_EX) );
    shpid_t childpid = pid.page;
    w_assert1(c2.pid0() == 0);

#ifdef DEBUG
    /*
     *  See if third child is also smo -- shouldn't happen
     *  Fix third child, if there is one
     *
     *  NB: This section should be removed most of the time, since it
     *  changes the pinning behavior of the -DDEBUG version
     *  substantially, and, in fact, may result in deadlock
     *  because we haven't followed the crabbing protocol
     *  through the btree in this debugging stuff below. (hence "supertest")
     */
    if(supertest) {
	lpid_t pid3 = parent.pid(); 
	pid3.page = c2.next();
	btree_p c3;

	//slot3 is slot in parent that should point to pid3
	int slot3 = slot>=0? slot + 1 : 0; 

	if(pid3.page) {
	    W_DO( c3.fix(pid3, LATCH_EX) );
	    // This could be the case, but we must make
	    // sure that there are an even number of them.
	    // Equivalently, even if this is smo, the parent must
	    // contain an entry for it.
	    if(c3.is_smo()) {
		CRASHTEST("btree.smo.10");
	    }
	    if(parent.nrecs() > slot3) {
		w_assert3(parent.child(slot3) == pid3.page);
	    } else {
		// ok, we have yet-another level, and we're
		// at the boundary between parent and parent->next()
		c3.unfix();
		{
		    lpid_t pid4 = parent.pid(); 
		    pid4.page = parent.next();
		    W_DO( c3.fix(pid4, LATCH_EX) );
		    w_assert3(c3.pid0() == pid3.page);
		    c3.unfix();
		}
	    }
	}
    }
#endif

    w_assert1(c2.nrecs());
    btrec_t r2(c2, 0);

    /*
     *  Construct sep (do suffix compression if children are leaves)
     */
    vec_t pkey;
    vec_t pelem;

    if (c2.is_node())  {
	pkey.put(r2.key());
	pelem.put(r2.elem());
    } else {
	/*
	 *   Compare key first
	 *   If keys are different, compress away all element parts.
	 *   Otherwise, extract common element parts.
	 */
	size_t common_size;
	int diff = cvec_t::cmp(r1.key(), r2.key(), &common_size);
	DBG(<<"diff = " << diff << " common_size = " << common_size);
	if (diff)  {
	    if (common_size < r2.key().size())  {
		pkey.put(r2.key(), 0, common_size + 1);
	    } else {
		w_assert3(common_size == r2.key().size());
		pkey.put(r2.key());
		pelem.put(r2.elem(), 0, 1);
	    }
	} else {
	    /*
	     *  keys are the same, r2.elem() msut be greater than r1.elem()
	     */
	    pkey.put(r2.key());
	    cvec_t::cmp(r1.elem(), r2.elem(), &common_size);
	    w_assert3(common_size < r2.elem().size());
	    pelem.put(r2.elem(), 0, common_size + 1);
	}
    }
#ifdef DEBUG
	if(print_propagate) {
	    cout << "inserting pkey " ;
	    cout.write(pkey.ptr(0), pkey.len(0));
	    cout << " pelem ";
	    cout.write(pelem.ptr(0), pelem.len(0));
	    cout << " with page " << c2.pid().page << endl;
	}
#endif

    /*
     *  Move sep to parent
     */
    rc_t rc = parent.insert(pkey, pelem, ++slot, c2.pid().page);
    if (rc) {
	if (rc.err_num() != eRECWONTFIT) {
	    return RC_AUGMENT(rc);
	}

	/*
	 *  Parent is full --- split parent node
	 */
	DBG(<<"parent is full -- split parent node");
	btree_p rsib;
	int addition = (pkey.size() + pelem.size() + 2 + sizeof(shpid_t));
	bool left_heavy;
	CRASHTEST("btree.propagate.1");

	W_DO( _split_page(parent, rsib, left_heavy,
			  slot, addition, 50) );

	CRASHTEST("btree.propagate.2");

	btree_p& target = (left_heavy ? parent : rsib);
	W_COERCE(target.insert(pkey, pelem, slot, childpid));
    }

    /*
     *  For node, remove first record
     */
    if (c2.is_node())  {
	shpid_t pid0 = c2.child(0);
	DBG(<<"remove first record, pid0=" << pid0);
	W_DO(c2.remove(0));
	W_DO(c2.set_pid0(pid0));
    }
    CRASHTEST("btree.propagate.3");

    DBG(<<"clearing smo for page " << c1.pid().page);
    W_DO( c1.clr_smo() );
    CRASHTEST("btree.smo.3");
    DBG(<<"clearing smo for page " << c2.pid().page); // NEH
    W_DO( c2.clr_smo() );// NEH
    CRASHTEST("btree.smo.4");

    // c1 and c2 are unfixed by their destructors


    return RCOK;
}

rc_t
btree_m::_propagate_split(
    btree_p& parent, 	// I-  parent page 
    int slot)		// I-  child that did a split (-1 ==> pid0)
{
    lsn_t anchor;
    xct_t* xd = xct();

    w_assert1(xd);
    anchor = xd->anchor();
    X_DO( __propagate_split(parent,slot));

    CRASHTEST("toplevel.btree.5");
    xd->compensate(anchor);

    return RCOK;
}



/*********************************************************************
 *
 *  btree_m::_shrink_tree(rootpage)
 *
 *  Shrink the tree. Copy the child page over the root page so the
 *  tree is effectively one level shorter.
 *
 *********************************************************************/
rc_t
btree_m::_shrink_tree(btree_p& rp)
{
    FUNC(btree_m::_shrink_tree);
    smlevel_0::stats.bt_shrinks++;
    lsn_t anchor;
    xct_t* xd = xct();

    if (xd) anchor = xd->anchor();

    /*
     *  Sanity checks
     */
    w_assert3(rp.latch_mode() == LATCH_EX);
    w_assert1( rp.nrecs() == 0);
    w_assert1( !rp.prev() && !rp.next() );

    lpid_t pid = rp.pid();
    if ((pid.page = rp.pid0()))  {
	/*
	 *  There is a child in pid0. Copy child page over parent,
	 *  and free child page.
	 */
	btree_p cp;
	X_DO( cp.fix(pid, LATCH_EX) );
	
	w_assert3(rp.level() == cp.level() + 1);
	w_assert3(!cp.next() && !cp.prev());
	X_DO( rp.set_hdr(rp.pid().page, rp.level() - 1,
		       cp.pid0(), 0) );
	
	if (cp.nrecs()) {
	    X_DO( cp.shift(0, rp) );
	}
	CRASHTEST("btree.shrink.1");
	X_DO( io->free_page(pid) );
	CRASHTEST("btree.shrink.2");

    } else {
	/*
	 *  No child in pid0. Simply set the level of root to 1.
	 */
        // w_assert3(rp.level() == 2);
	W_DO( rp.set_hdr(rp.pid().page, 1, 0, 0) );
    }


    if (xd) {
	CRASHTEST("toplevel.btree.6");
	xd->compensate(anchor);
    }

    CRASHTEST("btree.shrink.3");

    return RCOK;
}




/*********************************************************************
 *
 *  btree_m::_grow_tree(rootpage)
 *
 *  Root page has split. Allocate a new child, shift all entries of
 *  root to new child, and have the only entry in root (pid0) point
 *  to child. Tree grows by 1 level.
 *
 *********************************************************************/
rc_t
btree_m::_grow_tree(btree_p& rp)
{
    FUNC(btree_m::_grow_tree);
    smlevel_0::stats.bt_grows++;

    lsn_t anchor;
    xct_t* xd = xct();

    if (xd)  anchor = xd->anchor();

    /*
     *  Sanity check
     */
    w_assert3(rp.latch_mode() == LATCH_EX);
    w_assert1(rp.next());
    w_assert3(rp.is_smo());

    /*
     *  First right sibling
     */
    lpid_t nxtpid = rp.pid();
    nxtpid.page = rp.next();
    btree_p np;

    // "following" a link here means fixing the page
    smlevel_0::stats.bt_links++;
    X_DO( np.fix(nxtpid, LATCH_EX) );
    w_assert1(!np.next());

    /*
     *  Allocate a new child, link it up with right sibling,
     *  and shift everything over to it (i.e. it is a copy
     *  of root).
     */
    btree_p cp;
    X_DO( _alloc_page(rp.pid(), rp.level(),
		      np.pid(), cp, rp.pid0()) );
    
    X_DO( cp.link_up(rp.prev(), rp.next()) );
    X_DO( np.link_up(cp.pid().page, np.next()) );
    X_DO( rp.shift(0, cp) );
    
    w_assert3(rp.prev() == 0);

    /*
     *  Reset new root page with only 1 entry:
     *  	pid0 points to new child.
     */
    CRASHTEST("btree.grow.1");
    X_DO( rp.link_up(0, 0) );

    CRASHTEST("btree.grow.2");

    X_DO( rp.set_hdr(rp.pid().page, rp.level() + 1,
		     cp.pid().page, FALSE) );

    DBG(<<"growing to level " << rp.level() + 1);

    CRASHTEST("btree.grow.3");
    /*
     *  We will propagate later.
     */
    DBG(<<"SMO set in _grow");
    X_DO( cp.set_smo() );
    CRASHTEST("btree.grow.5");

    if (xd)  {
	CRASHTEST("toplevel.btree.7");
	xd->compensate(anchor);
    }
    
    CRASHTEST("btree.grow.4");
    return RCOK;
}

    
/*********************************************************************
 *
 *  btree_m::_split_page(page, sibling, left_heavy, slot, 
 *                       addition, split_factor)
 *
 *  Split the page. The newly allocated right sibling is returned in
 *  "sibling". Based on the "slot" into which an "additional" bytes 
 *  would be inserted after the split, and the "split_factor", 
 *  routine computes the a new "slot" for the insertion after the
 *  split and a boolean flag "left_heavy" to indicate if the
 *  new "slot" is in the left or right sibling.
 *
 *********************************************************************/
rc_t
btree_m::_split_page(
    btree_p&	page,		// IO- page that needs to split
    btree_p&	sibling,	// O-  new sibling
    bool&	left_heavy,	// O-  TRUE if insert should go to left
    int&	slot,		// IO- slot of insertion after split
    int		addition,	// I-  # bytes intended to insert
    int		split_factor)	// I-  % of left page that should remain
{
    FUNC(btree_m::_split_page);
#ifdef DEBUG
    bool crashsplit = false;
#endif

    smlevel_0::stats.bt_splits++;

    DBG( << "split page " << page.pid()
	<< " slot " << slot
	<< " addition " << addition
	);
    
    lsn_t anchor;
    xct_t* xd = xct();

    if (xd) anchor = xd->anchor();

    /*
     *  Allocate new sibling
     */
    {
	w_assert3(! sibling);
	lpid_t root = page.root();
	X_DO( _alloc_page(root, page.level(), page.pid(), sibling) );
    }
    CRASHTEST("btree.split.1");

    /*
     *  Hook up all three siblings: cousin is the original right
     *  sibling; 'sibling' is the new right sibling.
     */
    btree_p cousin;
    if (page.next()) {
	lpid_t cpid = page.pid();
	DBG(<<"Cousin=" << cpid);
	cpid.page = page.next();

	// "following" a link here means fixing the page
	smlevel_0::stats.bt_links++;
	X_DO( cousin.fix(cpid, LATCH_EX) );
#ifdef DEBUG
	if(cousin.is_smo()) {
	    // we're going to end up with three smos
	    crashsplit = true;
	    CRASHTEST("btree.split.7");
	    crashsplit = true;
	}
#endif
    }
    DBG(<<"page=" << page.pid()
	<< " sibling=" << sibling.pid()
    );

    X_DO( sibling.link_up(page.pid().page, page.next()) );
    X_DO( page.link_up(page.prev(), sibling.pid().page) );
    if (cousin) {
	X_DO( cousin.link_up(sibling.pid().page, cousin.next()));
    }

    CRASHTEST("btree.split.2");
    /*
     *  Distribute content to sibling
     */
    X_DO( page.distribute(sibling, left_heavy,
			  slot, addition, split_factor) );

    /*
     *  Page has modified tree structure
     */
    DBG(<<"SMO set in _split");

    CRASHTEST("btree.split.3");

    X_DO( page.set_smo() );

    CRASHTEST("btree.split.4");

    X_DO( sibling.set_smo() ); // NEH

    CRASHTEST("btree.split.6");

#ifdef DEBUG
    if(crashsplit) {
	CRASHTEST("btree.split.8");
    }
#endif

    if (xd) {
	CRASHTEST("toplevel.btree.8");
	xd->compensate(anchor);
    }

    CRASHTEST("btree.split.5");
#ifdef DEBUG
    if(crashsplit) {
	CRASHTEST("btree.split.9");
    }
#endif

    DBG(<< " after split  new sibling " << sibling.pid()
	<< " left_heavy=" << left_heavy
	<< " slot " << slot
    );
    return RCOK;
}    



/*********************************************************************
 *
 *  btree_m::_skip_one_slot(p1, p2, child, slot, eof)
 *
 *  Given two neighboring page p1, and p2, and child that
 *  points to either p1 or p2, and the slot in child, 
 *  compute the next slot and return in "child", and "slot".
 *
 *********************************************************************/
#ifndef W_DEBUG
#define p1 /* p1 not used */
#endif
void 
btree_m::_skip_one_slot(
    btree_p& 		p1,
    btree_p& 		p2, 
    btree_p*&		child, 
    int& 		slot,
    bool&		eof
    )
#undef p1
{
    w_assert3(child == &p1 || child == &p2);
    w_assert3(*child);

    w_assert3(slot <= child->nrecs());
    ++slot;
    eof = false;
    while (slot >= child->nrecs()) {
	/*
	 *  Move to right sibling
	 */
	lpid_t pid = child->pid();
	if (! (pid.page = child->next())) {
	    /*
	     *  EOF reached.
	     */
	    slot = child->nrecs();
	    eof = true;
	    return;
	}
	child = &p2;
	W_COERCE( child->fix(pid, LATCH_SH) );
	w_assert3(child->nrecs() || child->is_phantom());
	slot = 0;
    }
}




/*********************************************************************
 *
 *  btree_m::_scramble_key(ret, key, nkc, kc)
 *  btree_m::_unscramble_key(ret, key, nkc, kc)
 *
 *  These functions put a key into lexicographic order.
 *
 *********************************************************************/
void
btree_m::_scramble_key(
    cvec_t*& 		ret,
    const cvec_t& 	key, 
    int 		nkc,
    const key_type_s* 	kc)
{
    w_assert1(kc && nkc > 0);
    if (&key == &key.neg_inf || &key == &key.pos_inf)  {
	ret = (cvec_t*) &key;
	return;
    }
    ret = &me()->kc_vec();
    ret->reset();

    char* p = 0;
    for (int i = 0; i < nkc; i++)  {
	key_type_s::type_t t = (key_type_s::type_t) kc[i].type;
	if (t == key_type_s::i || t == key_type_s::u)  {
	    p = me()->kc_buf();
	    break;
	}
    }

    if (! p)  {
	ret->put(key);
    } else {
	key.copy_to(p);
	char* s = p;
	for (int i = 0; i < nkc; i++)  {
	    key_type_s::type_t t = (key_type_s::type_t) kc[i].type;
	    switch (t) {
	    case key_type_s::i: 
		{
		    if (kc[i].length == 2)  {
			int2& k = *(int2*)s;
			//memcpy(&k, s, sizeof(k));
			uint2 kk;
			if (k >= 0) 
			    kk = ((uint2)k) + SHRT_MAX + 1;
			else 
			    kk = k + SHRT_MAX + 1;
			memcpy(s, &kk, sizeof(kk));
		    } else {
			w_assert3(kc[i].length == 4);
			int4& k = *(int4*)s;
			//memcpy(&k, s, sizeof(k));
			uint4 kk;
			if (k >= 0) 
			    kk = ((uint4)k) + INT_MAX + 1;
			else 
			    kk = k + INT_MAX + 1;
			memcpy(s, &kk, sizeof(kk));
		    }
		}
		/* fall thru */
	    case key_type_s::u:
		/* for little-endian, do byte swapping */
		{
		    if (kc[i].length == 2)  {
			uint2& k = *(uint2*)s;
			uint2 swapped = htons(k);
			k = swapped;
		    } else {
			uint4& k = *(uint4*)s;
			uint4 swapped = htonl(k);
			k = swapped;
		    }
		} 
		break;

	    case key_type_s::b:
	    case key_type_s::f:
		/* ignored */
		break;
	    default:
		W_FATAL(eINTERNAL);
	    }
	    s += kc[i].length;
	}
	ret->put(p, s - p);
    }
}


void
btree_m::_unscramble_key(
    cvec_t*& 		ret,
    const cvec_t& 	key, 
    int 		nkc,
    const key_type_s* 	kc)
{
    w_assert1(kc && nkc > 0);
    ret = &me()->kc_vec();
    ret->reset();
    char* p = 0;
    for (int i = 0; i < nkc; i++)  {
	key_type_s::type_t t = (key_type_s::type_t) kc[i].type;
	if (t == key_type_s::i || t == key_type_s::u)  {
	    p = me()->kc_buf();
	    break;
	}
    }
    if (! p)  {
	ret->put(key);
    } else {
	key.copy_to(p);
	char* s = p;
	for (int i = 0; i < nkc; i++)  {
	    key_type_s::type_t t = (key_type_s::type_t) kc[i].type;
	    switch (t) {
	    case key_type_s::u: case key_type_s::i:
		/* for little-endian, do byte swapping */
		{
		    if (kc[i].length == 2)  {
			uint2& k = *(uint2*)s;
			uint2 swapped = ntohs(k);
			k = swapped;
		    } else {
			uint4& k = *(uint4*)s;
			uint4 swapped = ntohl(k);
			k = swapped;
		    }
		} 
		if (t == key_type_s::i) {
		    // convert back to a negative number
		    if (kc[i].length == 2)  {
			uint2 k;
			memcpy(&k, s, sizeof(k));
			int2 kk;
			if (k >= SHRT_MAX) 
			    kk = ((int2) (k - SHRT_MAX)) - 1;
			else 
			    kk = -(SHRT_MAX - k) - 1;
			memcpy(s, &kk, sizeof(kk));
		    } else {
			w_assert3(kc[i].length == 4);
			uint4 k;
			memcpy(&k, s, sizeof(k));
			int4 kk;
			if (k >= INT_MAX)
			    kk = ((int4) (k - INT_MAX)) - 1;
			else 
			    kk = -(INT_MAX - k) - 1;
			memcpy(s, &kk, sizeof(kk));
		    }
		}
		break;
		/* fall thru */
	    case key_type_s::b:
	    case key_type_s::f:
		/* ignored */
		break;
	    default:
		W_FATAL(eINTERNAL);
	    }
	    s += kc[i].length;
	}
	ret->put(p, s - p);
    }
}




/*********************************************************************
 *
 *  btree_p::ntoh()
 *
 *  Network to host order.
 *
 *********************************************************************/
void
btree_p::ntoh()
{
    /*
     *  BUGBUG -- fill in this function
     */
    w_assert1(is_leaf());
}


/*********************************************************************
 *
 *  btree_p::_print_key_str(bool)
 *  btree_p::print_key_str()
 *  btree_p::print_key_uint4()
 *
 *  Print content of btree (for debugging).
 *
 *********************************************************************/
void
btree_p::print_key_str()
{
   _print_key_str(false);
}

void
btree_p::_print_key_str(bool print_elem)
{
    int i;
    btctrl_t hdr = _hdr();

    if (! is_leaf()) {
	for (i = 0; i < 5 - hdr.level; i++)  cout << '\t';
	cout << "    <pid0 = " << hdr.pid0 << ">\n";
    }

    for (i = 0; i < nrecs(); i++)  {
	for (int j = 0; j < 5 - hdr.level; j++) cout << '\t';

	btrec_t r(*this, i);
	char* key = new char[r.klen()];
	if (! key)  W_FATAL(eOUTOFMEMORY);
	w_auto_delete_array_t<char> auto_del(key);
	
	r.key().copy_to(key);

	const char* p;
	for (p = key; ((smsize_t)(p - key) < r.klen()) && (*p == '\0'); p++)
	    ; 
	cout << "    <key = ";
	cout.write(p, r.klen() - (p - key) ) << ", ";

	if (r.elen())  {
	    char* elem = new char[r.elen()];
	    if (! elem)  W_FATAL(eOUTOFMEMORY);
	    w_auto_delete_array_t<char> auto_del(elem);

	    r.elem().copy_to(elem);

	    cout << "elem = ";
	    if(print_elem) {
		const char* s;
		for (s = elem; ((smsize_t)(s - elem) < r.elen()) && (*s == '\0'); s++)
		    ;
		cout.write(s, r.elen() - (s - elem));
	    } else {
		cout << r.elen() << " chars ";
	    }
	}
	if (! is_leaf())  {
	    cout << "pid = " << r.child();
	}
	cout << ">" << endl;
    }
}

void
btree_p::print_key_uint4()
{
    int i;
    btctrl_t hdr = _hdr();
    
    if (! is_leaf()) {
	for (i = 0; i < 5 - hdr.level; i++)  cout << '\t';
	cout << "    <pid0 = " << hdr.pid0 << ">\n";
    }

    for (i = 0; i < nrecs(); i++)  {
	for (int j = 0; j < 5 - hdr.level; j++) cout << '\t';

	btrec_t r(*this, i);
	uint4 key;
	w_assert3(r.klen() == sizeof(uint4));
	r.key().copy_to(&key, r.klen());

	cout << "    <key = " << key << ", ";
	if (is_leaf())  {
	    ;
	} else {
	    cout << "pid = " << r.child() << ">";
	}

	cout << endl;
    }
}

void
btree_p::print_key_uint2()
{
    int i;
    btctrl_t hdr = _hdr();
    
    if (! is_leaf()) {
	for (i = 0; i < 5 - hdr.level; i++)  cout << '\t';
	cout << "    <pid0 = " << hdr.pid0 << ">\n";
    }

    for (i = 0; i < nrecs(); i++)  {
	for (int j = 0; j < 5 - hdr.level; j++) cout << '\t';

	btrec_t r(*this, i);
	uint2 key;
	w_assert3(r.klen() == sizeof(uint2));
	r.key().copy_to(&key, r.klen());

	cout << "    <key = " << key << ", ";
	if (is_leaf())  {
	    ;
	} else {
	    cout << "pid = " << r.child() << ">";
	}

	cout << endl;
    }
}




/*********************************************************************
 *
 *  btree_p::distribute(right_sibling, left_heavy, slot, 
 *			addition, split_factor)
 *
 *  Spil this page over to right_sibling. 
 *  Based on the "slot" into which an "additional" bytes 
 *  would be inserted after the split, and the "split_factor", 
 *  routine computes the a new "slot" for the insertion after the
 *  split and a boolean flag "left_heavy" to indicate if the
 *  new "slot" is in the left or right sibling.
 *
 *********************************************************************/
rc_t
btree_p::distribute(
    btree_p&	rsib,		// IO- target page
    bool& 	left_heavy,	// O-  TRUE if insert should go to left
    int& 	snum,		// IO- slot of insertion after split
    smsize_t	addition,	// I-  # bytes intended to insert
    int 	factor)		// I-  % that should remain
{
    w_assert1(snum >= 0 && snum <= nrecs());
    /*
     *  Assume we have already inserted the tuple into slot snum
     *  of this page, calculate left and right page occupancy.
     *  ls and rs are the running total of page occupancy of
     *  left and right sibling respectively.
     */
    addition += sizeof(page_s::slot_t);
    int orig = used_space();
    int ls = orig + addition;
    const keep = factor * ls / 100; // nbytes to keep on left page

    int flag = 1;		// indicates if we have passed snum

    /*
     *  i points to the current slot, and i+1-flag is the first slot
     *  to be moved to rsib.
     */
    int rs = rsib.used_space();
    int i;
    for (i = nrecs(); i >= 0; i--)  {
        int c;
	if (i == snum)  {
	    c = addition, flag = 0;
	} else {
	    c = int(align(rec_size(i-flag)) + sizeof(page_s::slot_t));
	}
	ls -= c, rs += c;
	if ((ls < keep && ls + c <= orig) || rs > orig)  {
	    ls += c;
	    rs -= c;
	    if (i == snum) flag = 1;
	    break;
	}
    }

    /*
     *  Calculate 1st slot to move and shift over to right sibling
     */
    i = i + 1 - flag;
    if (i < nrecs())  {
	W_DO( shift(i, rsib) );
    }

    w_assert3(i == nrecs());
    
    /*
     *  Compute left_heavy and new slot to insert additional bytes.
     */
    if (snum == nrecs())  {
	left_heavy = flag;
    } else {
	left_heavy = (snum < nrecs());
    }

    if (! left_heavy)  {
	snum -= nrecs();
    }

#ifdef DEBUG
    btree_p& p = (left_heavy ? *this : rsib);
    w_assert1(snum <= p.nrecs());
    w_assert1(p.usable_space() >= addition);

    DBG(<<"distribute: usable_space() : left=" << this->usable_space()
    << "; right=" << rsib. usable_space() );
#endif /*DEBUG*/
    
    return RCOK;
}



/*********************************************************************
 *
 *  btree_p::unlink()
 *
 *  Unlink this page from its neighbor. The page becomes a phantom.
 *  Return ePAGECHANGED to the caller if the caller should retry
 *  the unlink() again. This is done so that we will not unlink a
 *  page who is participating concurrently in a split operation
 *  because of its neighbor.
 *
 *********************************************************************/
rc_t
btree_p::unlink()
{
    w_assert1(! is_phantom() );
    w_assert3(latch_mode() == LATCH_EX);

    lpid_t my_pid, prev_pid, next_pid;
    prev_pid = next_pid = my_pid = pid();
    prev_pid.page = prev(), next_pid.page = next();

    /*
     *  Save my LSN and unfix self.
     */
    lsn_t old_lsn = lsn();
    unfix();
    
    /*
     *  Fix left and right sibling
     */
    btree_p lsib, rsib;
    if (prev_pid.page)  { W_DO( lsib.fix(prev_pid, LATCH_EX) ); }

    // "following" a link here means fixing the page
    // count one for right, one for left
    smlevel_0::stats.bt_links++;
    smlevel_0::stats.bt_links++;

    if (next_pid.page)	{ W_DO( rsib.fix(next_pid, LATCH_EX) ); }

    /*
     *  Fix self, if lsn has changed, return an error code
     *  so caller can retry.
     */
    W_COERCE( fix(my_pid, LATCH_EX) );
    if (lsn() != old_lsn)   {
	return RC(ePAGECHANGED);
    }

    /*
     *  Now that we have all three pages fixed, we can 
     *  proceed to the top level action:
     *	    1. unlink this page
     *      2. set its phantom state.
     */
    lsn_t anchor;
    xct_t* xd = xct();
    if (xd)  anchor = xd->anchor();
    
    if (lsib) {
	X_DO( lsib.link_up(lsib.prev(), next()) );
	CRASHTEST("btree.unlink.1");
    }
    if (rsib) {
	X_DO( rsib.link_up(prev(), rsib.next()) );
	CRASHTEST("btree.unlink.2");
    }
    CRASHTEST("btree.unlink.3");

    X_DO( set_phantom() );


    if (xd)  {
	CRASHTEST("toplevel.btree.9");
	xd->compensate(anchor);
    }

    CRASHTEST("btree.unlink.4");
    
    return RCOK;
}



/*********************************************************************
 *
 *  btree_p::set_hdr(root, level, pid0, flags)
 *
 *  Set the page header.
 *
 *********************************************************************/
rc_t
btree_p::set_hdr(shpid_t root, int l, shpid_t pid0, uint2 flags)
{
    btctrl_t hdr;
    hdr.root = root;
    hdr.pid0 = pid0;
    hdr.level = l;
    hdr.flags = flags;

    vec_t v;
    v.put(&hdr, sizeof(hdr));
    W_DO( zkeyed_p::set_hdr(v) );
    return RCOK;
}




/*********************************************************************
 *
 *  btree_p::set_pid0(pid0)
 *
 *  Set the pid0 field in header to "pid0".
 *
 ********************************************************************/
rc_t
btree_p::set_pid0(shpid_t pid0)
{
    const btctrl_t& tmp = _hdr();
    W_DO(set_hdr(tmp.root, tmp.level, pid0, tmp.flags) );
    return RCOK;
}



/*********************************************************************
 *
 *  btree_p::set_smo()
 *
 *  Mark the page as participant in a structure modification op (smo).
 *
 *********************************************************************/
rc_t
btree_p::set_smo()
{
    const btctrl_t& tmp = _hdr();
    W_DO( set_hdr(tmp.root, tmp.level, tmp.pid0, tmp.flags | t_smo) );
    return RCOK;
}



/*********************************************************************
 *
 *  btree_p::clr_smo()
 *
 *  Clear smo flag.
 *
 *********************************************************************/
rc_t
btree_p::clr_smo()
{
    const btctrl_t& tmp = _hdr();
    W_DO( set_hdr(tmp.root, tmp.level, tmp.pid0, tmp.flags & ~t_smo) );
    return RCOK;
}



/*********************************************************************
 *
 *  btree_p::set_phantom()
 *
 *  Mark page as phantom.
 *
 *********************************************************************/
rc_t
btree_p::set_phantom()
{
    FUNC(btree_p::set_phantom);
    DBG(<<"page is " << this->pid());
    const btctrl_t& tmp = _hdr();
    W_DO( set_hdr(tmp.root, tmp.level, tmp.pid0, tmp.flags | t_phantom) );
    return RCOK;
}


#ifdef UNUSED
/*********************************************************************
 *
 *  btree_p::clr_phantom()
 *
 *  Clear phantom flag.
 *  Not used for now -- keep around in case we later needed.
 *
 *********************************************************************/
rc_t
btree_p::clr_phantom()
{
    FUNC(btree_p::clr_phantom);
    DBG(<<"page is " << this->pid());

    const btctrl_t& tmp = _hdr();
    W_DO( set_hdr(tmp.root, tmp.level, tmp.pid0, tmp.flags & ~t_phantom) );
    return RCOK;
}
#endif /*UNUSED*/



/*********************************************************************
 *
 *  btree_p::format(pid, tag, page_flags)
 *
 *  Format the page.
 *
 *********************************************************************/
rc_t
btree_p::format(
    const lpid_t& 	pid, 
    tag_t 		tag, 
    uint4 		page_flags)
{
    btctrl_t ctrl;
    ctrl.level = 1, ctrl.flags = 0, ctrl.root = 0; ctrl.pid0 = 0; 
    vec_t vec;
    vec.put(&ctrl, sizeof(ctrl));
    W_DO( zkeyed_p::format(pid, tag, page_flags, vec) );
    return RCOK;
}



/*********************************************************************
 *
 *  btsink_t::btsink_t(root_pid, rc)
 *
 *  Construct a btree sink for bulkload of btree rooted at root_pid. 
 *  Any errors during construction in returned in rc.
 *
 *********************************************************************/
btsink_t::btsink_t(const lpid_t& root, rc_t& rc)
    : _height(0), _num_pages(0), _leaf_pages(0),
      _root(root), _top(0)
{
    btree_p rp;
    if (rc = rp.fix(root, LATCH_SH))  return;
    
    rc = _add_page(0, 0);
    _left_most[0] = _page[0].pid().page;
}



/*********************************************************************
 *
 *  btsink_t::map_to_root()
 *
 *  Map current running root page to the real root page. Deallocate
 *  original running root page after the mapping.
 *
 *********************************************************************/

rc_t
btsink_t::map_to_root()
{
    lsn_t anchor;
    xct_t* xd = xct();
    w_assert1(xd);
    if (xd)  anchor = xd->anchor();

    for (int i = 0; i <= _top; i++)  {
	X_DO( log_page_image(_page[i]) );
    }

    /*
     *  Fix root page
     */
    btree_p rp;
    X_DO( rp.fix(_root, LATCH_EX) );

    lpid_t child_pid;
    {
	btree_p cp = _page[_top];
	child_pid = cp.pid();

	_height = cp.level();

	if (child_pid == _root)  {
	    /*
	     *  No need to remap.
	     */
	    xd->stop_crit();
	    return RCOK;
	}

	/*
	 *  Shift everything from child page to root page
	 */
	X_DO( rp.set_hdr(_root.page, cp.level(), cp.pid0(), 0) );
	w_assert1( !cp.next() && ! cp.prev());
	w_assert1( rp.nrecs() == 0);
	X_DO( cp.shift(0, rp) );
    }
    _page[_top] = rp;

    if (xd)  {
	CRASHTEST("toplevel.btree.10");
	xd->compensate(anchor);
	xd->flush_logbuf();
    }

    /*
     *  Free the child page. It has been copied to the root.
     */
    W_DO( io->free_page(child_pid) );

    return RCOK;
}



/*********************************************************************
 *
 *  btsink_t::_add_page(i, pid0)
 *
 *  Add a new page at level i.
 *
 *********************************************************************/
rc_t
btsink_t::_add_page(int i, shpid_t pid0)
{
    lsn_t anchor;
    xct_t* xd = xct();
    if (xd) anchor = xd->anchor();

    {
	xct_log_switch_t toggle(OFF);

	btree_p lsib = _page[i];
	/*
	 *  Allocate a new page
	 */
	X_DO( _alloc_page(_root, i+1, (_page[i] ? _page[i].pid() : _root),
			  _page[i], pid0) );
    
	/*
	 *  Update stats
	 */
	_num_pages++;
	if (i == 0) _leaf_pages++;

	/*
	 *  Link up
	 */
	shpid_t left = 0;
	X_DO( _page[i].link_up(left, 0) );

	if (lsib != 0) {
	    left = lsib.pid().page;
	    X_DO( lsib.link_up(lsib.prev(), _page[i].pid().page) );

	    xct_log_switch_t toggle(ON);
	    X_DO( log_page_image(lsib) );
	}
    }

    if (xd)  {
	CRASHTEST("toplevel.btree.11");
	xd->compensate(anchor);
    }

    /*
     *  Current slot for new page is 0
     */
    _slot[i] = 0;
    
    return RCOK;
}




/*********************************************************************
 *
 *  btsink_t::put(key, el)
 *
 *  Insert a <key el> pair into the page[0] (leaf) of the stack
 *
 *********************************************************************/
rc_t
btsink_t::put(const cvec_t& key, const cvec_t& el)
{
    /*
     *  Turn OFF logging. Insertions into btree pages are not logged
     *  during bulkload. Instead, a page image log is generated
     *  when the page is filled (in _add_page()).
     */
    xct_log_switch_t toggle(OFF);

    /*
     *  Try inserting into the page[0] (leaf)
     */
    rc_t rc = _page[0].insert(key, el, _slot[0]++);
    if (rc) {
    
	if (rc.err_num() != eRECWONTFIT)  {
	    return RC_AUGMENT(rc);
	}
	
	/*
	 *  page[0] is full --- add a new page and and re-insert
	 */
	W_DO( _add_page(0, 0) );

	W_COERCE( _page[0].insert(key, el, _slot[0]++) );

	/*
	 *  Propagate up the tree
	 */
	int i;
	for (i = 1; i <= _top; i++)  {
	    rc_t rc = _page[i].insert(key, el, _slot[i]++,
				     _page[i-1].pid().page);
	    if (rc)  {
		if (rc.err_num() != eRECWONTFIT)  {
		    return RC_AUGMENT(rc);
		}

		/*
		 *  Parent is full
		 *      --- add a new page for the parent and
		 *	--- continue propagation to grand-parent
		 */
		W_DO(_add_page(i, _page[i-1].pid().page));
		
	    } else {
		
		/* parent not full --- done */
		break;
	    }
	}

	/*
	 *  Check if we need to grow the tree
	 */
	if (i > _top)  {
	    ++_top;
	    W_DO( _add_page(_top, _left_most[_top-1]) );
	    _left_most[_top] = _page[_top].pid().page;
	    W_COERCE( _page[_top].insert(key, el, _slot[_top]++,
					 _page[_top-1].pid().page) );
	}
    }

    return RCOK;
}



/*********************************************************************
 *
 *  btree_p::search(key, el, found, ret_slot)
 *
 *  Search for <key, el> in this page. Return TRUE in "found" if
 *  a total match is found. Return the slot in which <key, el> 
 *  resides or SHOULD reside.
 *  
 *********************************************************************/
rc_t
btree_p::search(
    const cvec_t& 	key,
    const cvec_t& 	el,
    bool& 		found, 
    int& 		ret_slot) const
{
    FUNC(btree_p::search);
    /*
     *  Binary search.
     */
    found = false;
    int mi, lo, hi;
    for (mi = 0, lo = 0, hi = nrecs() - 1; lo <= hi; )  {
	mi = (lo + hi) >> 1;	// ie (lo + hi) / 2

	btrec_t r(*this, mi);
	int d;
	if ((d = r.key().cmp(key)) == 0)  {
		DBG(<<"comparing el");
	    d = r.elem().cmp(el);
	}
	DBG(<<"d = " << d);
	if (d < 0) 
	    lo = mi + 1;
	else if (d > 0)
	    hi = mi - 1;
	else {
	    ret_slot = mi;
	    found = TRUE;
	    return RCOK;
	}
    }
    ret_slot = (lo > mi) ? lo : mi;
    w_assert3(ret_slot <= nrecs());
    DBG(<<"returning slot " << ret_slot);
    return RCOK;
}




    


/*********************************************************************
 *
 *  btree_p::insert(key, el, slot, child)
 *
 *  For leaf page: insert <key, el> at "slot"
 *  For node page: insert <key, el, child> at "slot"
 *
 *********************************************************************/
rc_t
btree_p::insert(
    const cvec_t& 	key,	// I-  key
    const cvec_t& 	el,	// I-  element
    int 		slot,	// I-  slot number
    shpid_t 		child)	// I-  child page pointer
{
    FUNC(btree_p::insert);
    cvec_t sep(key, el);
    
    int2 klen = key.size();
    cvec_t attr;
    attr.put(&klen, sizeof(klen));
    if (is_leaf()) {
	w_assert3(child == 0);
    } else {
	w_assert3(child);
	attr.put(&child, sizeof(child));
    }

    return zkeyed_p::insert(sep, attr, slot);
}



/*********************************************************************
 *
 *  btree_p::copy_to_new_page(new_page)
 *
 *  Copy all slots to new_page.
 *
 *********************************************************************/
rc_t
btree_p::copy_to_new_page(btree_p& new_page)
{
    FUNC(btree_p::copy_to_new_page);

    /*
     *  This code copies the old root to the new root.
     *  This code is similar to zkeyed_p::shift() except it
     *  does not remove entries from the old page.
     */

    int n = nrecs();
    for (int i = 0; i < n; ) {
	vec_t tp[20];
	int j;
	for (j = 0; j < 20 && i < n; j++, i++)  {
	    tp[j].put(page_p::tuple_addr(1 + i),
		      page_p::tuple_size(1 + i));
	}
	W_DO( new_page.insert_expand(1 + i - j, j, tp) );
	CRASHTEST("btree.1page.3");
    }

    return RCOK;

#ifdef UNDEF
    /* OLD VERSION */
    // generate an array of vectors to the entries on the old root
    int n = nrecs();
    vec_t* tp = new vec_t[n];
    w_assert1(tp);
    w_auto_delete_array_t<vec_t> auto_del(tp);
    for (int i = 0; i < n; i++) {
        tp[i].put(page_p::tuple_addr(1 + i),
                  page_p::tuple_size(1 + i));
    }

    W_DO(new_page.insert_expand(1, n, tp));

    return RCOK;
#endif /*UNDEF*/
}



/*********************************************************************
 * 
 *  btree_p::child(slot)
 *
 *  Return the child pointer of tuple at "slot". Applies to interior
 *  nodes only.
 *
 *********************************************************************/
shpid_t 
btree_p::child(int slot) const
{
    vec_t sep;
    const char* aux;
    int auxlen;

    W_COERCE( zkeyed_p::rec(slot, sep, aux, auxlen) );
    w_assert3(is_node() && auxlen == 2 + sizeof(shpid_t));

    shpid_t child;
    memcpy(&child, aux + 2, sizeof(shpid_t));
    return child;
}


// Stats on btree leaf pages
rc_t
btree_p::leaf_stats(btree_lf_stats_t& stats)
{
    btrec_t rec[2];
    int r = 0;

    stats.hdr_bs += (hdr_size() + sizeof(page_p::slot_t) + 
		     align(page_p::tuple_size(0)));
    stats.unused_bs += persistent_part().space.nfree();

    int n = nrecs();
    stats.entry_cnt += n;

    for (int i = 0; i < n; i++)  {
	rec[r].set(*this, i);
	if (rec[r].key() != rec[1-r].key())  {
	    stats.unique_cnt++;
	}
	stats.key_bs += rec[r].klen();
	stats.data_bs += rec[r].elen();
	stats.entry_overhead_bs += (align(this->rec_size(i)) - 
				       rec[r].klen() - rec[r].elen() + 
				       sizeof(page_s::slot_t));
	r = 1 - r;
    }
    return RCOK;
}

// Stats on btree interior pages
rc_t
btree_p::int_stats(btree_int_stats_t& stats)
{
    stats.unused_bs += persistent_part().space.nfree();
    stats.used_bs += page_sz - persistent_part().space.nfree();
    return RCOK;
}



/*********************************************************************
 *
 *  btrec_t::set(page, slot)
 *
 *  Load up a reference to the tuple at "slot" in "page".
 *
 *********************************************************************/
btrec_t& 
btrec_t::set(const btree_p& page, int slot)
{
    /*
     *  Invalidate old _key and _elem.
     */
    _key.reset();
    _elem.reset();

    const char* aux;
    int auxlen;
    
    vec_t sep;
    W_COERCE( page.zkeyed_p::rec(slot, sep, aux, auxlen) );

    if (page.is_leaf())  {
	w_assert3(auxlen == 2);
    } else {
	w_assert3(auxlen == 2 + sizeof(shpid_t));
    }
    int2 k;
    memcpy(&k, aux, sizeof(k));
    size_t klen = k;

#ifdef  DEBUG
    int elen_test = sep.size() - klen;
    w_assert3(elen_test >= 0);
#endif
#ifdef W_DEBUG
    smsize_t elen = sep.size() - klen;
#endif

    sep.split(klen, _key, _elem);
    w_assert3(_key.size() == klen);
    w_assert3(_elem.size() == elen);

    if (page.is_node())  {
	w_assert3(auxlen == 2 + sizeof(shpid_t));
	memcpy(&_child, aux + 2, sizeof(shpid_t));
    }

    return *this;
}


rc_t
bt_cursor_t::set_up( const lpid_t& root, int nkc, const key_type_s* kc,
		     bool unique, concurrency_t cc, 
		     cmp_t cond2, const cvec_t& bound2)
{
    _root = root;
    _nkc = nkc;
    _kc = kc;
    _unique = unique;
    _cc = cc;
    _cond2 = cond2;

    if (_bound2_buf) {
	// get rid of old _bound2_buf
	delete [] _bound2_buf;
	_bound2_buf = 0;
    }

    /*
     * Cache bound 2
     */
    if (bound2.is_pos_inf())  {
        _bound2 = &(cvec_t::pos_inf);
    } else if (bound2.is_neg_inf())  {
        _bound2 = &(cvec_t::neg_inf);
    } else {
        size_t len = bound2.size();
        _bound2_buf = new char[len];
        if (! _bound2_buf)  {
            return RC(eOUTOFMEMORY);
        }
        bound2.copy_to(_bound2_buf, len);
        _bound2 = &_bound2_tmp;
        _bound2->put(_bound2_buf, len);
    }

    return RCOK;
}
rc_t
bt_cursor_t::set_up_part_2(cmp_t cond1, const cvec_t& bound1)
{
    _cond1 = cond1;

    if (_bound1_buf) {
	// get rid of old _bound1_buf
	delete [] _bound1_buf;
	_bound1_buf = 0;
    }
    /*
     * Cache bound 1
     */
    if (bound1.is_pos_inf())  {
        _bound1 = &(cvec_t::pos_inf);
    } else if (bound1.is_neg_inf())  {
        _bound1 = &(cvec_t::neg_inf);
    } else {
        size_t len = bound1.size();
        _bound1_buf = new char[len];
        if (! _bound1_buf)  {
            return RC(eOUTOFMEMORY);
        }
        bound1.copy_to(_bound1_buf, len);
        _bound1 = &_bound1_tmp;
        _bound1->put(_bound1_buf, len);
    }
    return RCOK;
}


/*********************************************************************
 *
 *  bt_cursor_t::make_rec(page, slot)
 *
 *  Make the cursor point to record at "slot" on "page".
 *
 *********************************************************************/
rc_t
bt_cursor_t::make_rec(const btree_p& page, int slot)
{
    FUNC(bt_cursor_t::make_rec);

    /*
     *  Fill up pid, lsn, and slot
     */
    _pid = page.pid();
    _lsn = page.lsn();
    _slot = slot;

    /*
     *  Copy the record to buffer
     */
    btrec_t r(page, slot);
    _klen = r.klen();
    _elen = r.elen();
    if (_klen + _elen > _splen)  {
	if (_space) delete[] _space;
	if (! (_space = new char[_splen = _klen + _elen]))  {
	    _klen = 0;
	    return RC(eOUTOFMEMORY);
	}
    }
    w_assert3(_klen + _elen <= _splen);


    /*
     * See if key is out of bounds
     * Handle weird cases like 
     *      >= 22 == 23
     *      >= 22 <= 20
     *      == 22 <= 30
     *      >= 30 <= 20
     */

    bool ok1 = false;  // true if meets criterion 1
    bool ok2 = false;  // true if meets criterion 2
    bool in_bounds = false;  // set to true if this
			// kv pair meets BOTH criteria
    bool more = false;  // might be more if we were to keep going

    // because we start by traversing for the lower bound,
    // this should be true:
    w_assert3(r.key() >= bound1());

    switch (cond1()) {
    case eq:
	if (r.key() == bound1())
	    ok1 = true;
	break;
    case ge:
	if (r.key() >= bound1())
	    ok1 = true;
	break;
    case gt:
	if (r.key() > bound1())
	    ok1 = true;
	break;
    case le:
    case lt:
	// should not happen
	// because the scan initialization 
	// should have returned eBADCMPOP
    default:
	W_FATAL(eINTERNAL);
    }

    DBG(<< "ok wrt lower: " << ok1);

    // don't bother testing upper until we've
    // reached lower

    if(ok1) {
	switch (cond2()) {
	case eq:
	    ok2 = (r.key() == bound2());
	    more = (r.key() < bound2());
	    break;
	case le:
	    ok2= (r.key() <= bound2());
	    break;
	case lt:
	    ok2 = (r.key() < bound2());
	    break;
	case gt:
	case ge:
	    // should not happen
	    // because of checks in scan_index_i
	    // constructor
	default:
	    W_FATAL(eINTERNAL);
	}
    }
    DBG(<< "ok wrt upper: " << ok2);

    if(ok1 && ok2) {
	in_bounds = true;
	keep_going = false;
    } else {
	in_bounds = false;
	keep_going = ok1 && more;
    }
    DBG(<< "in_bounds: " << in_bounds << " keep_going: " << keep_going);

    if (in_bounds) {
	// key is in bounds, so put it into proper order
	cvec_t* user_key;
	bt->_unscramble_key(user_key, r.key(), _nkc, _kc);

	user_key->copy_to(_space);
	r.elem().copy_to(_space + _klen);
    } else if(!keep_going) {
	free_rec();
    } // else do nothing -- don't want to blow away pid

    return RCOK;
}

MAKEPAGECODE(btree_p, zkeyed_p);

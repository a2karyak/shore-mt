/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

/*
 *  $Id: logrec.c,v 1.88 1996/07/01 22:07:26 nhall Exp $
 */
#define SM_SOURCE
#define LOGREC_C
#ifdef __GNUG__
#   pragma implementation
#endif
#include "sm_int.h"
#ifdef __GNUG__
#    include <iomanip.h>
#endif
#include "logdef.i"
#include <vol.h>
#include <btree_p.h>
#include <rtree_p.h>
#ifdef USE_RDTREE
#include <rdtree_p.h>
#endif /* USE_RDTREE */


/*********************************************************************
 *
 *  logrec_t::cat_str()
 *
 *  Return a string describing the category of the log record.
 *
 *********************************************************************/
const char*
logrec_t::cat_str() const
{
    switch (_cat)  {
    case t_logical:
	return "l---";

    case t_logical | t_cpsn:
	return "l--c";

    case t_status:
	return "s---";

    case t_undo:
	return "--u-";

    case t_redo:
	return "-r--";

    case t_redo | t_cpsn:
	return "-r-c";

    case t_undo | t_redo:
	return "-ru-";

    case t_undo | t_redo | t_logical:
	return "lru-";

    case t_redo | t_logical | t_cpsn:
	return "lr_c";

    case t_undo | t_logical | t_cpsn:
	return "l_uc";

    case t_undo | t_logical : 
	return "l-u-";

    case t_format:
	return "f---";
#ifdef DEBUG
    case t_bad_cat:
	// for debugging only
	return "BAD-";
#endif /* DEBUG */

    }
#ifdef DEBUG
    cerr << "unexpected log record flags: ";
	if( _cat & t_undo ) cerr << "t_undo ";
	if( _cat & t_redo ) cerr << "t_redo ";
	if( _cat & t_logical ) cerr << "t_logical ";
	if( _cat & t_cpsn ) cerr << "t_cpsn ";
	if( _cat & t_status ) cerr << "t_status ";
	if( _cat & t_format ) cerr << "t_format ";
	cerr << endl;
#endif /* DEBUG */
	
    W_FATAL(eINTERNAL);
    return "????";
}


/*********************************************************************
 *
 *  logrec_t::type_str()
 *
 *  Return a string describing the type of the log record.
 *
 *********************************************************************/
const char* 
logrec_t::type_str() const
{
    switch (_type)  {
#	include <logstr.i>
    }

    /*
     *  Not reached.
     */
    W_FATAL(eINTERNAL);
    return 0;
}




/*********************************************************************
 *
 *  logrec_t::fill(pid, len)
 *
 *  Fill the "pid" and "length" field of the log record.
 *
 *********************************************************************/
void
logrec_t::fill(const lpid_t* p, uint2 tag, smsize_t l)
{
    w_assert3(hdr_sz == _data - (char*) this);
    w_assert3(w_base_t::is_aligned(_data));

    _pid = lpid_t::null;
    _page_tag = tag;
    if (p) _pid = *p;
    if (l != align(l)) {
	// zero out extra space to keep purify happy
	memset(_data+l, 0, align(l)-l);
    }
    _len = align(l) + hdr_sz + sizeof(lsn_t);
    w_assert1(_len <= max_sz);
    if(type() != t_skip) {
	DBG( << "Creat log rec: " << *this << " size: " << _len << " prevlsn: " << _prev );
    }
}



/*********************************************************************
 *
 *  logrec_t::fill_xct_attr(tid, prev_lsn)
 *
 *  Fill the transaction related fields of the log record.
 *  BUGBUG: should be inlined.
 *
 *********************************************************************/
void 
logrec_t::fill_xct_attr(const tid_t& tid, const lsn_t& last)
{
    _tid = tid;
    _prev = last;
}

/*
 * Determine whether the log record header looks valid
 */
bool
logrec_t::valid_header(const lsn_t & lsn) const
{
    if (_len < hdr_sz || _type > 100 || _cat == t_bad_cat || 
	lsn != lsn_ck()) {
	return false;
    }
    return true;
}


/*********************************************************************
 *
 *  logrec_t::redo(page)
 *
 *  Invoke the redo method of the log record.
 *
 *********************************************************************/
void logrec_t::redo(page_p* page)
{
    FUNC(logrec_t::redo);
    DBG( << "Redo  log rec: " << *this << " size: " << _len << " prevlsn: " << _prev );
    switch (_type)  {
#include <redo.i>
    }
    
    /*
     *  Page is dirty after redo.
     *  (not all redone log records have a page)
     */
    if(page) page->set_dirty();
}



/*********************************************************************
 *
 *  logrec_t::undo(page)
 *
 *  Invoke the undo method of the log record. Automatically tag
 *  a compensation lsn to the last log record generated for the
 *  undo operation.
 *
 *********************************************************************/
void
logrec_t::undo(page_p* page)
{
    FUNC(logrec_t::undo);
    DBG( << "Undo  log rec: " << *this << " size: " << _len  << " prevlsn: " << _prev);
    switch (_type) {
#include <undo.i>
    }
    xct()->compensate_undo(_prev);
}

/*********************************************************************
 *
 *  logrec_t::corrupt()
 *
 *  Zero out most of log record to make it look corrupt.
 *  This is for recovery testing.
 *
 *********************************************************************/
void
logrec_t::corrupt()
{
    char* end_of_corruption = ((char*)this)+length();
    char* start_of_corruption = (char*)&_type;
    size_t bytes_to_corrupt = end_of_corruption - start_of_corruption;
    memset(start_of_corruption, 0, bytes_to_corrupt);
}

/*********************************************************************
 *
 *  xct_end_log
 *
 *  Status Log to mark the end of transaction.
 *  Synchronous for commit. Async for abort.
 *
 *********************************************************************/
xct_end_log::xct_end_log()
{
    fill(0, 0, 0);
}



/*********************************************************************
 *
 *  xct_prepare_st_log -- start prepare info for a tx
 *  xct_prepare_lk_log -- add locks to the tx
 *  xct_prepare_fi_log -- fin prepare info for the tx
 *
 *  For 2 phase commit.
 *
 *********************************************************************/

xct_prepare_st_log::xct_prepare_st_log(const gtid_t *g, 
	const smlevel_0::coord_handle_t &h)
{
    fill(0, 0, (new (_data) prepare_info_t(g, h))->size());

}
void
xct_prepare_st_log::redo(page_p *) 
{
    /*
     *  Update xct state 
     */
    const prepare_info_t* 	pt = (prepare_info_t*) _data;
    xct_t *			xd = xct_t::look_up(_tid);
    w_assert3(xd);
    if(pt->is_external==1) {
	W_COERCE(xd->enter2pc(pt->g));
    }
    xd->set_coordinator(pt->h);
}


xct_prepare_lk_log::xct_prepare_lk_log(int num, 
	lock_base_t::mode_t mode, lockid_t *locks)
{
    fill(0, 0, (new (_data) prepare_lock_t(num, mode, locks))->size());
}

xct_prepare_alk_log::xct_prepare_alk_log(int num, 
	lockid_t *locks,
	lock_base_t::mode_t *modes
	)
{
    fill(0, 0, (new (_data) prepare_all_lock_t(num, locks, modes))->size());
}

void
xct_prepare_lk_log::redo(page_p *) 
{
    /*
     *  Acquire all the locks within. (all of same mode)
     *  For lm->lock() to work, we have to 
     *  attach the xct
     */
    const prepare_lock_t* 	pt = (prepare_lock_t*) _data;
    xct_t *			xd = xct_t::look_up(_tid);
    me()->attach_xct(xd);
    W_COERCE(xd->obtain_locks(pt->mode, pt->num_locks, pt->name));
    me()->detach_xct(xd);
}

void
xct_prepare_alk_log::redo(page_p *) 
{
    /*
     *  Acquire all the locks within. (different modes)
     *  For lm->lock() to work, we have to 
     *  attach the xct
     */
    const prepare_all_lock_t* 	pt = (prepare_all_lock_t*) _data;
    xct_t *			xd = xct_t::look_up(_tid);
    me()->attach_xct(xd);
    int i;
    for (i=0; i<pt->num_locks; i++) {
	W_COERCE(xd->obtain_one_lock(pt->pair[i].mode, pt->pair[i].name));
    }
    me()->detach_xct(xd);
}


xct_prepare_fi_log::xct_prepare_fi_log(int num_ex, int num_ix, int num_six, int numextent) 
{
    fill(0, 0, (new (_data) prepare_lock_totals_t(num_ex, num_ix, num_six, numextent))->size());
}
void 
xct_prepare_fi_log::redo(page_p *) 
{
    /*
     *  check that the right number of
     *  locks was acquired, and set the state
     *  -- don't set the state any earlier so that
     *  txs that didn't get the prepare fin logged
     *  are aborted during the undo phase
     */
    xct_t *			xd = xct_t::look_up(_tid);
    const prepare_lock_totals_t* pt = 
	    (prepare_lock_totals_t*) _data;
    me()->attach_xct(xd);
    W_COERCE(xd->check_lock_totals(pt->num_EX, pt->num_IX, pt->num_SIX, pt->num_extents));
    xd->change_state(xct_t::xct_prepared);
    me()->detach_xct(xd);
    w_assert3(xd->state() == xct_t::xct_prepared);
}
/*********************************************************************
 *
 *  compensate_log
 *
 *  Needed when compensation rec is written rather than piggybacked
 *  on another record
 *
 *********************************************************************/
compensate_log::compensate_log(lsn_t rec_lsn)
{
    fill(0, 0, 0);
    set_clr(rec_lsn);
}


/*********************************************************************
 *
 *  skip_log partition
 *
 *  Filler log record -- for skipping to end of log partition
 *
 *********************************************************************/
skip_log::skip_log()
{
    fill(0, 0, 0);
}

/*********************************************************************
 *
 *  chkpt_begin_log
 *
 *  Status Log to mark start of fussy checkpoint.
 *
 *********************************************************************/
chkpt_begin_log::chkpt_begin_log(const lsn_t &lastMountLSN)
{
    new (_data) lsn_t(lastMountLSN);
    fill(0, 0, sizeof(lsn_t));
}




/*********************************************************************
 *
 *  chkpt_end_log(const lsn_t &master, const lsn_t& min_rec_lsn)
 *
 *  Status Log to mark completion of fussy checkpoint.
 *  Master is the lsn of the record that began this chkpt.
 *  min_rec_lsn is the lsn of the record that began this chkpt.
 *
 *********************************************************************/
chkpt_end_log::chkpt_end_log(const lsn_t &lsn, const lsn_t& min_rec_lsn)
{
    // initialize _data
    lsn_t *l = new (_data) lsn_t(lsn);
    l++; //grot
    *l = min_rec_lsn;

    fill(0, 0, 2 * sizeof(lsn_t));
}



/*********************************************************************
 *
 *  chkpt_bf_tab_log
 *
 *  Data Log to save dirty page table at checkpoint.
 *  Contains, for each dirty page, its pid and recovery lsn.
 *
 *********************************************************************/

chkpt_bf_tab_t::chkpt_bf_tab_t(
    int 		cnt,	// I-  # elements in pids[] and rlsns[]
    const lpid_t* 	pids,	// I-  id of of dirty pages
    const lsn_t* 	rlsns)	// I-  rlsns[i] is recovery lsn of pids[i]
    : count(cnt)
{
    w_assert3( SIZEOF(*this) <= logrec_t::data_sz );
    w_assert1(count <= max);
    for (uint i = 0; i < count; i++) {
	brec[i].pid = pids[i];
	brec[i].rec_lsn = rlsns[i];
    }
}


chkpt_bf_tab_log::chkpt_bf_tab_log(
    int 		cnt,	// I-  # elements in pids[] and rlsns[]
    const lpid_t* 	pid,	// I-  id of of dirty pages
    const lsn_t* 	rec_lsn)// I-  rlsns[i] is recovery lsn of pids[i]
{
    fill(0, 0, (new (_data) chkpt_bf_tab_t(cnt, pid, rec_lsn))->size());
}




/*********************************************************************
 *
 *  chkpt_xct_tab_log
 *
 *  Data log to save transaction table at checkpoint.
 *  Contains, for each active xct, its id, state, last_lsn
 *  and undo_nxt lsn. 
 *
 *********************************************************************/
chkpt_xct_tab_t::chkpt_xct_tab_t(
    const tid_t& 			_youngest,
    int 				cnt,
    const tid_t* 			tid,
    const smlevel_1::xct_state_t* 	state,
    const lsn_t* 			last_lsn,
    const lsn_t* 			undo_nxt)
    : youngest(_youngest), count(cnt)
{
    w_assert1(count <= max);
    w_assert3( SIZEOF(*this) <= logrec_t::data_sz);
    for (uint i = 0; i < count; i++)  {
	xrec[i].tid = tid[i];
	xrec[i].state = state[i];
	xrec[i].last_lsn = last_lsn[i];
	xrec[i].undo_nxt = undo_nxt[i];
    }
}
    
chkpt_xct_tab_log::chkpt_xct_tab_log(
    const tid_t& 			youngest,
    int 				cnt,
    const tid_t* 			tid,
    const smlevel_1::xct_state_t* 	state,
    const lsn_t* 			last_lsn,
    const lsn_t* 			undo_nxt)
{
    fill(0, 0, (new (_data) chkpt_xct_tab_t(youngest, cnt, tid, state,
					 last_lsn, undo_nxt))->size());
}




/*********************************************************************
 *
 *  chkpt_dev_tab_log
 *
 *  Data log to save devices mounted at checkpoint.
 *  Contains, for each device mounted, its devname and vid.
 *
 *********************************************************************/
chkpt_dev_tab_t::chkpt_dev_tab_t(
    int 		cnt,
    const char 		dev[][smlevel_0::max_devname+1],
    const vid_t* 	vid)
    : count(cnt)
{
    w_assert3( SIZEOF(*this) <= logrec_t::data_sz );
    w_assert1(count <= max);
    for (uint i = 0; i < count; i++) {
	// zero out everything and then set the string
	memset(devrec[i].dev_name, 0, sizeof(devrec[i].dev_name));
	strncpy(devrec[i].dev_name, dev[i], sizeof(devrec[i].dev_name)-1);
	devrec[i].vid = vid[i];
    }
}

chkpt_dev_tab_log::chkpt_dev_tab_log(
    int 		cnt,
    const char 		dev[][smlevel_0::max_devname+1],
    const vid_t* 	vid)
{
    fill(0, 0, (new (_data) chkpt_dev_tab_t(cnt, dev, vid))->size());
}


mount_vol_log::mount_vol_log(
    const char* 	dev,
    const vid_t& 	vid)
{
    char		devArray[1][smlevel_0::max_devname+1];

    /* this copy could be avoided by use of a cast, but this is safer */
    strncpy(devArray[0], dev, sizeof(devArray[0]));
    DBG(<< "mount_vol_log dev_name=" << devArray[0] << " volid =" << vid);
    fill(0, 0, (new (_data) chkpt_dev_tab_t(1, devArray, &vid))->size());
}


#ifndef W_DEBUG
#define page /* page not used */
#endif
void mount_vol_log::redo(page_p* page)
#undef page
{
    w_assert3(page == 0);
    chkpt_dev_tab_t* dp = (chkpt_dev_tab_t*) _data;

    w_assert3(dp->count == 1);

    // this may fail since this log record is only redone on crash/restart and the
	// user may have destroyed the volume after using, but then there won't be
	// and pages that need to be updated on this volume.
    W_IGNORE(io_m::mount(dp->devrec[0].dev_name, dp->devrec[0].vid));
}


dismount_vol_log::dismount_vol_log(
    const char*		dev,
    const vid_t& 	vid)
{
    char		devArray[1][smlevel_0::max_devname+1];

    /* this copy could be avoided by use of a cast, but this is safer */
    strncpy(devArray[0], dev, sizeof(devArray[0]));
    DBG(<< "dismount_vol_log dev_name=" << devArray[0] << " volid =" << vid);
    fill(0, 0, (new (_data) chkpt_dev_tab_t(1, devArray, &vid))->size());
}


#ifndef W_DEBUG
#define page /* page not used */
#endif
void dismount_vol_log::redo(page_p* page)
#undef page
{
    w_assert3(page == 0);
    chkpt_dev_tab_t* dp = (chkpt_dev_tab_t*) _data;

    w_assert3(dp->count == 1);
    // this may fail since this log record is only redone on crash/restart and the
	// user may have destroyed the volume after using, but then there won't be
	// and pages that need to be updated on this volume.
    W_IGNORE(io_m::dismount(dp->devrec[0].vid, true));
}


/*********************************************************************
 *
 *   store_create_log
 *
 *   Logical Log for creation of a store. 
 *
 *********************************************************************/
struct store_create_t {
    stid_t	stid;
    uint2	eff;
    fill2	filler;
    uint4	flags;

    store_create_t(stid_t s, uint2 e, uint4 f) : stid(s), eff(e), flags(f) {};
    int size()	{ return sizeof(*this);}
};

store_create_log::store_create_log(
    const page_p& 	page,
    snum_t 		snum,
    uint2 		eff, 
    uint4 		flags)
{
    fill(&page.pid(), page.tag(),
	 (new (_data) store_create_t(stid_t(page.pid().vol(), snum), 
				     eff, flags))->size());
}


#ifndef W_DEBUG
#define page /* page not used */
#endif
void 
store_create_log::undo(page_p* page)
#undef page
{
    w_assert3(page == 0);
    store_create_t* dp = (store_create_t*) _data;

    // ***LOGICAL***
    W_COERCE( smlevel_0::io->destroy_store(dp->stid));
}

void 
store_create_log::redo(page_p* page)
{
    stnode_p* sp = (stnode_p*) page;
    store_create_t* dp = (store_create_t*) _data;

    // ***PHYSICAL***
    stnode_t stnode;
    stnode.head = 0;
    stnode.eff = dp->eff;
    stnode.flags = dp->flags;
    W_COERCE(sp->put(dp->stid.store % (stnode_p::max), stnode)); 
}



/*********************************************************************
 *
 *  page_init_log
 *
 *  Log page initialization.
 *
 *********************************************************************/
struct page_init_t {
    uint2	tag;
    fill2	filler;
    uint4	flags;
    
    size()  { return sizeof(*this); }
};

page_init_log::page_init_log(const page_p& p)
{
    page_init_t* dp = (page_init_t*) _data;
    dp->tag = p.tag();
    dp->flags = p.page_flags();

    w_assert3(p.pid() != lpid_t::null); // TODO: ever legit?

    fill(&p.pid(), p.tag(), dp->size());
}


void 
page_init_log::redo(page_p* page)
{
    page_init_t* dp = (page_init_t*) _data;
    W_COERCE( page->format(pid(), (page_p::tag_t) dp->tag, dp->flags) );
}



/*********************************************************************
 *
 *  page_link_log
 *
 *  Log a page link up operation.
 *
 *********************************************************************/
struct page_link_t {
    shpid_t old_prev, old_next;
    shpid_t new_prev, new_next;

    page_link_t(shpid_t op, shpid_t on, shpid_t np, shpid_t nn)  
    : old_prev(op), old_next(on), new_prev(np), new_next(nn)   {};

    size()	{ return sizeof(*this); }
};

page_link_log::page_link_log(
    const page_p& 	page, 
    shpid_t 		new_prev, 
    shpid_t 		new_next)
{
    fill(&page.pid(), page.tag(),
	 (new (_data) page_link_t(page.prev(), page.next(),
				  new_prev, new_next))->size());
}

void 
page_link_log::redo(page_p* p)
{
    page_link_t* dp = (page_link_t*) _data;
    w_assert3(p->next() == dp->old_next);
    w_assert3(p->prev() == dp->old_prev);

    W_COERCE(p->link_up(dp->new_prev, dp->new_next));
}

void 
page_link_log::undo(page_p* p)
{
    page_link_t* dp = (page_link_t*) _data;
    w_assert3(p->next() == dp->new_next);
    w_assert3(p->prev() == dp->new_prev);
    W_COERCE(p->link_up(dp->old_prev, dp->old_next));
}



/*********************************************************************
 *
 *  page_insert_log
 *
 *  Log insert of a record into a page.
 *
 *********************************************************************/
struct page_insert_t {
    int2	idx;
    int2	cnt;
    // array of int2 len[cnt], char data[]
    char	data[logrec_t::data_sz - 2 * sizeof(int2)]; 

    page_insert_t(const page_p& page, int idx, int cnt);
    page_insert_t(int idx, int cnt, const cvec_t* vec);
    cvec_t* unflatten(int cnt, cvec_t vec[]);
    size();
};

page_insert_t::page_insert_t(const page_p& page, int i, int c)
    : idx(i), cnt(c)
{
    w_assert1(cnt * sizeof(int2) < sizeof(data));
    int2* lenp = (int2*) data;
    int total = 0;
    for (i = 0; i < cnt; total += lenp[i++])  {
	lenp[i] = page.tuple_size(idx + i);
    }
    w_assert1(total + cnt * sizeof(int2) < sizeof(data));

    char* p = data + sizeof(int2) * cnt;
    for (i = 0; i < c; i++)  {
	memcpy(p, page.tuple_addr(idx + i), lenp[i]);
	p += lenp[i];
    }
}


page_insert_t::page_insert_t(int i, int c, const cvec_t* v)
    : idx(i), cnt(c)
{
    w_assert1(cnt * sizeof(int2) < sizeof(data));
    int2* lenp = (int2*) data;
    int total = 0;
    for (i = 0; i < cnt; total += lenp[i++])  {
	lenp[i] = v[i].size();
    }
    w_assert1(total + cnt * sizeof(int2) < sizeof(data));

    char* p = data + sizeof(int2) * cnt;
    for (i = 0; i < c; i++)  {
	p += v[i].copy_to(p);
    }
    w_assert1((uint)(p - data) == total + cnt * sizeof(int2));
}

#ifndef W_DEBUG
#define c /* c not used */
#endif
cvec_t* page_insert_t::unflatten(int c, cvec_t vec[])
#undef c
{
    w_assert3(c == cnt);
    int2* lenp = (int2*) data;
    char* p = data + sizeof(int2) * cnt;
    for (int i = 0; i < cnt; i++)  {
	vec[i].put(p, lenp[i]);
	p += lenp[i];
    }
    return vec;
}

int
page_insert_t::size()
{
    int2* lenp = (int2*) data;
    char* p = data + sizeof(int2) * cnt;
    for (int i = 0; i < cnt; i++) p += lenp[i];
    return p - (char*) this;
}

page_insert_log::page_insert_log(
    const page_p& 	page, 
    int 		idx, 
    int 		cnt,
    const cvec_t* 	vec)
{
    fill(&page.pid(), page.tag(),
		(new (_data) page_insert_t(idx, cnt, vec))->size());
}



void 
page_insert_log::redo(page_p* page)
{
    page_insert_t* dp = (page_insert_t*) _data;
    cvec_t* vec = new cvec_t[dp->cnt];	// need improvement
    if (! vec)  W_FATAL(eOUTOFMEMORY);
    w_auto_delete_array_t<cvec_t> auto_del(vec);

    W_COERCE(page->insert_expand(dp->idx, dp->cnt, 
			       dp->unflatten(dp->cnt, vec)) );
}


void 
page_insert_log::undo(page_p* page)
{
    page_insert_t* dp = (page_insert_t*) _data;
#ifdef DEBUG
    uint2* lenp = (uint2*) dp->data;
    char* p = dp->data + sizeof(int2) * dp->cnt;
    for (int i = 0; i < dp->cnt; i++)  {
	w_assert1(lenp[i] == page->tuple_size(i + dp->idx));
	w_assert1(memcmp(p, page->tuple_addr(i + dp->idx), lenp[i]) == 0);
	p += lenp[i];
    }
#endif /*DEBUG*/
    W_COERCE(page->remove_compress(dp->idx, dp->cnt));
}



/*********************************************************************
 *
 *  page_remove_log
 *
 *  Log removal of a record from page.
 *
 *********************************************************************/
typedef page_insert_t page_remove_t;

page_remove_log::page_remove_log(
    const page_p& 	page,
    int 		idx, 
    int 		cnt)
{
    fill(&page.pid(), page.tag(),
		(new (_data) page_remove_t(page, idx, cnt))->size());
}

void page_remove_log::redo(page_p* page)
{
    ((page_insert_log*)this)->undo(page);
}

void page_remove_log::undo(page_p* page)
{
    ((page_insert_log*)this)->redo(page);
}
    


/*********************************************************************
 *
 *  page_mark_log
 *
 *  Log a mark operation on a record in page. The record is
 *  marked as removed.
 *
 *********************************************************************/
struct page_mark_t {
    int2 idx;
    uint2 len;
    char data[logrec_t::data_sz - 2 * sizeof(int2)];

    page_mark_t(int i, smsize_t l, const void* d) : idx(i), len(l) {
	w_assert1(l <= sizeof(data));
	memcpy(data, d, l);
    }
    page_mark_t(int i, const cvec_t& v) : idx(i), len(v.size()) {
	w_assert1(len <= sizeof(data));
	v.copy_to(data);
    }
    size() {
	return data + len - (char*) this;
    }
};

page_mark_log::page_mark_log(const page_p& page, int idx)
{
    fill(&page.pid(), page.tag(),
	 (new (_data) page_mark_t(idx, page.tuple_size(idx), 
				 page.tuple_addr(idx)))->size() );
}

void
page_mark_log::redo(page_p* page)
{
    page_mark_t* dp = (page_mark_t*) _data;
    w_assert3( page->tuple_size(dp->idx) == dp->len);
    w_assert3( memcmp(page->tuple_addr(dp->idx), dp->data, dp->len) == 0);
    W_COERCE( page->mark_free(dp->idx) );
}

void
page_mark_log::undo(page_p* page)
{
    page_mark_t* dp = (page_mark_t*) _data;
    cvec_t v(dp->data, dp->len);
    W_COERCE( page->reclaim(dp->idx, v) );
}



/*********************************************************************
 *
 *  page_reclaim_log
 *
 *  Mark a reclaim operation on a record in page. A marked record
 *  is reclaimed this way.
 *
 *********************************************************************/
typedef page_mark_t page_reclaim_t;

page_reclaim_log::page_reclaim_log(const page_p& page, int idx,
				   const cvec_t& vec)
{
    fill(&page.pid(), page.tag(),
	 (new (_data) page_reclaim_t(idx, vec))->size()) ;
}

void
page_reclaim_log::redo(page_p* page)
{
    ((page_mark_log*) this)->undo(page);
}

void
page_reclaim_log::undo(page_p* page)
{
    ((page_mark_log*) this)->redo(page);
}




/*********************************************************************
 *
 *  page_splice_log
 *
 *  Log a splice operation on a record in page.
 *
 *********************************************************************/
struct page_splice_t {
    int2 		idx;	// slot affected
    uint2 		start;  // offset in the slot where the splice began
    uint2 		old_len;// len of old data (# bytes removed or overwritten by
				// the splice operation
    uint2 		new_len;// len of new data (# bytes inserted or written by the splice)
    char 		data[logrec_t::data_sz - 4 * sizeof(int2)]; // old data & new data

    NORET		page_splice_t(
	int 		    i, 
	uint 		    start, 
	uint 		    len, 
	const void* 	    tuple,
	const cvec_t& 	    v);
    void* 		old_image()   { return data; }
    void* 		new_image()   { return data + old_len; }
    int			size()  { 
	return data + old_len + new_len - (char*) this; 
    }
};

page_splice_t::page_splice_t(
    int 		i,
    uint 		start_,
    uint 		len_,
    const void* 	tuple,
    const cvec_t& 	v)
    : idx(i), start(start_), 
      old_len(len_), new_len(v.size())
{
    w_assert1((size_t)(old_len + new_len) < sizeof(data));
    memcpy(old_image(), ((char*)tuple)+start, old_len);
    v.copy_to(new_image());
#ifdef DEBUG
    // if(old_len > 0) {
	// w_assert3(memcmp(old_image(), new_image(), old_len) != 0);
    // }
#endif
}

page_splice_log::page_splice_log(
    const page_p& 	page,
    int 		idx,
    int 		start, 
    int 		len,
    const cvec_t& 	v)
{
    fill(&page.pid(), page.tag(),
	 (new (_data) page_splice_t(idx, start, len,
				    page.tuple_addr(idx), v))->size());
}

void 
page_splice_log::redo(page_p* page)
{
    page_splice_t* dp = (page_splice_t*) _data;
#ifdef DEBUG
    char* p = ((char*) page->tuple_addr(dp->idx)) + dp->start;
    w_assert1(memcmp(dp->old_image(), p, dp->old_len) == 0);
#endif /*DEBUG*/

    W_COERCE(page->splice(dp->idx, dp->start, dp->old_len,
			vec_t(dp->new_image(), dp->new_len)));
}

void
page_splice_log::undo(page_p* page)
{
    page_splice_t* dp = (page_splice_t*) _data;
#ifdef DEBUG
    if (page->tag() != page_p::t_extlink_p)  {
	/* do this only for vol map file. this is due to the fact
	 * that page map resides in extlink.
	 */
	char* p = ((char*) page->tuple_addr(dp->idx)) + dp->start;
	w_assert1(memcmp(dp->new_image(), p, dp->new_len) == 0);
    }
#endif /*DEBUG*/

    W_COERCE(page->splice(dp->idx, dp->start, dp->new_len,
			vec_t(dp->old_image(), dp->old_len)));
}

/*********************************************************************
 *
 *  page_splicez_log
 *
 *  Log a splice operation on a record in page, in which
 *  either the old or the new data (or both, i suppose) are zeroes
 *
 *********************************************************************/


struct page_splicez_t {
    int2 		idx;	// slot affected
    uint2 		start;  // offset in the slot where the splice began
    uint2 		old_len;// len of old data (# bytes removed or overwritten by
				// the splice operation
    uint2 		olen;   // len of old data stored in _data[]
    uint2 		new_len;// len of new data (# bytes inserted or written by the splice)
    uint2 		nlen;   // len of new data stroed in _data[]
    char 		data[logrec_t::data_sz - 6 * sizeof(int2)]; // old data & new data

    NORET		page_splicez_t(
	int 		    i, 
	uint 		    start, 
	uint 		    len, 
	uint 		    olen_, 
	uint 		    nlen_,
	const void* 	    tuple,
	const cvec_t& 	    v);
    void* 		old_image()   { return data; }
    void* 		new_image()   { return data + olen; }
    int			size()  { 
	return data + olen + nlen - (char*) this; 
    }
};
page_splicez_t::page_splicez_t(
    int 		i,
    uint 		start_,
    uint 		len_,
    uint 		save_,
    uint 		zlen,
    const void* 	tuple,
    const cvec_t& 	v)
    : idx(i), start(start_), old_len(len_), 
    olen(save_),
    new_len(v.size()),
    nlen(zlen)
{
    w_assert1((size_t)(old_len + new_len) < sizeof(data));
    w_assert1(old_len == olen || olen == 0);
    w_assert1(new_len == nlen || nlen == 0);

    if(olen== old_len) {
	memcpy(old_image(), ((char*)tuple)+start, old_len);
    }
    if(nlen== new_len) {
	w_assert1(nlen == v.size());
	v.copy_to(new_image());
    }

    DBG(<<"splicez log: " 
	<< " olen: " << olen
	<< " old_len: " << old_len
	<< " new_len: " << new_len
    );
}

page_splicez_log::page_splicez_log(
    const page_p& 	page,
    int 		idx,
    int 		start, 
    int 		len,
    int 		olen,
    int 		nlen,
    const cvec_t& 	v)
{
    // todo: figure out if old or new data are zerovec
    fill(&page.pid(), page.tag(),
	 (new (_data) page_splicez_t(idx, start, len, olen, nlen,
				    page.tuple_addr(idx), v))->size());
}

void 
page_splicez_log::redo(page_p* page)
{
    page_splicez_t* dp = (page_splicez_t*) _data;

#ifdef DEBUG
    {
	char* p = ((char*) page->tuple_addr(dp->idx)) + dp->start;


	// one or the other length must be 0, else we would
	// have logged a regular page splice, not a splicez
	w_assert1(dp->olen == 0 || dp->nlen == 0);
	w_assert1( dp->new_len <= smlevel_0::page_sz);
	w_assert1( dp->old_len <= smlevel_0::page_sz);

	// three cases:
	// saved old image, new image is zeroes
	// saved new image, old image is zeroes
	// saved neither image, both were zeroes


	if(dp->old_len > 0) { 
	    // there was an old image to consider saving
	    if(dp->olen == 0) {
		// the old image was zeroes so we didn't bother saving it.
		// make sure the page is consistent with that analysis
		w_assert1(memcmp(smlevel_0::zero_page, p, dp->old_len) == 0);
	    } else {
		// the old image was saved -- check it
		w_assert1(memcmp(dp->old_image(), p, dp->old_len) == 0);
	    }
	} // else nothing to compare, since old len is 0 (it was an insert)
    }
#endif /*DEBUG*/
    vec_t z;

    if(dp->nlen == 0) {
	z.set(zvec_t(dp->new_len));
    } else {
	z.set(dp->new_image(), dp->new_len);
    }

    W_COERCE(page->splice(dp->idx, dp->start, dp->old_len, z));
}

void
page_splicez_log::undo(page_p* page)
{
    page_splicez_t* dp = (page_splicez_t*) _data;
#ifdef DEBUG
    if (page->tag() != page_p::t_extlink_p)  {
	/* do this only for vol map file. this is due to the fact
	 * that page map resides in extlink.
	 */
	char* p = ((char*) page->tuple_addr(dp->idx)) + dp->start;

	// one or the other length must be 0, else we would
	// have logged a regular page splice, not a splicez
	w_assert1(dp->olen == 0 || dp->nlen == 0);
	w_assert1( dp->new_len <= smlevel_0::page_sz);
	w_assert1( dp->old_len <= smlevel_0::page_sz);

	// three cases:
	// saved old image, new image is zeroes
	// saved new image, old image is zeroes
	// saved neither image, both were zeroes

	if(dp->nlen == 0) {
	    // the new image was zeroes so we didn't bother saving it.
	    // make sure the page is consistent with that analysis
	    w_assert1(memcmp(smlevel_0::zero_page, p, dp->new_len) == 0);
	} else {
	    // the new image was saved
	    w_assert1(memcmp(dp->new_image(), p, dp->new_len) == 0);
	}
    }
#endif /*DEBUG*/
    DBG(<<"splicez undo: "
	<< " olen (stored) : " << dp->olen
	<< " old_len (orig): " << dp->old_len
	<< " nlen (stored) : " << dp->nlen
	<< " new_len (orig): " << dp->new_len
    );

    vec_t z;
    if(dp->olen == 0) {
	z.set(zvec_t(dp->old_len));
    } else {
	z.set(dp->old_image(), dp->old_len);
    }
    W_COERCE(page->splice(dp->idx, dp->start, dp->new_len, z));
}



/*********************************************************************
 *
 *  page_set_bit_log
 *
 *  Log a set bit operation for a record in page.
 *
 *********************************************************************/
struct page_set_bit_t {
    uint2 	idx;
    fill2	filler;
    int		bit;
    NORET	page_set_bit_t(uint2 i, int b) : idx(i), bit(b)  {};
    int		size()   { return sizeof(*this); }
};

page_set_bit_log::page_set_bit_log(const page_p& page, int idx, int bit)
{
    fill(&page.pid(), page.tag(),
		(new (_data) page_set_bit_t(idx, bit))->size());
}

void 
page_set_bit_log::redo(page_p* page)
{
    page_set_bit_t* dp = (page_set_bit_t*) _data;
    W_COERCE( page->set_bit(dp->idx, dp->bit) );
}

void 
page_set_bit_log::undo(page_p* page)
{
    page_set_bit_t* dp = (page_set_bit_t*) _data;
    W_COERCE( page->clr_bit(dp->idx, dp->bit) );
}



/*********************************************************************
 *
 *  page_clr_bit_log
 *
 *  Log a clear bit operation for a record in page.
 *
 *********************************************************************/
typedef page_set_bit_t page_clr_bit_t;

page_clr_bit_log::page_clr_bit_log(const page_p& page, int idx, int bit)
{
    fill(&page.pid(), page.tag(),
		(new (_data) page_clr_bit_t(idx, bit))->size());
}

void 
page_clr_bit_log::undo(page_p* page)
{
    ((page_set_bit_log*)this)->redo(page);
}

void 
page_clr_bit_log::redo(page_p* page)
{
    ((page_set_bit_log*)this)->undo(page);
}



/*********************************************************************
 *
 *  page_image_log
 *
 *  Log page image. Redo only.
 *
 *********************************************************************/
struct page_image_t {
    char image[sizeof(page_s)];
    page_image_t(const page_p& page)  {
	memcpy(image, &page.persistent_part_const(), sizeof(image));
    }
    size()	{ return sizeof(*this); }
};

page_image_log::page_image_log(const page_p& page)
{
    fill(&page.pid(), page.tag(), (new (_data) page_image_t(page))->size());
}

void 
page_image_log::redo(page_p* page)
{
    page_image_t* dp = (page_image_t*) _data;
    memcpy(&page->persistent_part(), dp->image, sizeof(dp->image));
}



/*********************************************************************
 *
 *  btree_purge_log
 *
 *  Log a logical purge operation for btree.
 *
 *********************************************************************/
struct btree_purge_t {
    lpid_t	root;
    btree_purge_t(const lpid_t& pid) : root(pid)  {};
    size()	{ return sizeof(*this); }
};


btree_purge_log::btree_purge_log(const page_p& page)
{
    fill(&page.pid(), page.tag(),
		(new (_data) btree_purge_t(page.pid()))->size());
}


void
btree_purge_log::undo(page_p* page)
{
    // keep compiler quiet about unused parameter
    if (page) {}
    btree_purge_t* dp = (btree_purge_t*) _data;
    W_COERCE( smlevel_2::bt->purge(dp->root, false));
}


void
btree_purge_log::redo(page_p* page)
{
    btree_p* bp = (btree_p*) page;
    btree_purge_t* dp = (btree_purge_t*) _data;
    W_COERCE( bp->set_hdr(dp->root.page, 1, 0, 0) );
}




/*********************************************************************
 *
 *   btree_insert_log
 *
 *   Log a logical insert into a btree.
 *
 *********************************************************************/
struct btree_insert_t {
    lpid_t	root;
    int2	idx;
    uint2	klen;
    uint2	elen;
    int2	unique;
    char	data[logrec_t::data_sz - sizeof(lpid_t) - 4*sizeof(int2)];

    btree_insert_t(const btree_p& page, int idx, const cvec_t& key,
		   const cvec_t& el, bool unique);
    size()	{ return data + klen + elen - (char*) this; }
};

btree_insert_t::btree_insert_t(
    const btree_p& 	_page, 
    int 		_idx,
    const cvec_t& 	key, 
    const cvec_t& 	el,
    bool 		uni)
    : idx(_idx), klen(key.size()), elen(el.size()), unique(uni)
{
    root = _page.root();
    w_assert1(key.size() > 0);
    w_assert1((size_t)(klen + elen) < sizeof(data));
    key.copy_to(data);
    el.copy_to(data + klen);
}

btree_insert_log::btree_insert_log(
    const page_p& 	page, 
    int 		idx, 
    const cvec_t& 	key,
    const cvec_t& 	el,
    bool 		unique)
{
    const btree_p& bp = * (btree_p*) &page;
    fill(&page.pid(), page.tag(),
	 (new (_data) btree_insert_t(bp, idx, key, el, unique))->size());
}

#ifndef W_DEBUG
#define page /* page not used */
#endif
void 
btree_insert_log::undo(page_p* page)
#undef page
{
    w_assert3(page == 0);
    btree_insert_t* dp = (btree_insert_t*) _data;

    cvec_t key, el;
    key.put(dp->data, dp->klen);
    el.put(dp->data + dp->klen, dp->elen);

    // ***LOGICAL***
    W_COERCE( smlevel_2::bt->_remove(dp->root, dp->unique, 
				     smlevel_0::t_cc_kvl, key, el) ); 
}

void
btree_insert_log::redo(page_p* page)
{
    btree_p* bp = (btree_p*) page;
    btree_insert_t* dp = (btree_insert_t*) _data;
    vec_t key, el;
    key.put(dp->data, dp->klen);
    el.put(dp->data + dp->klen, dp->elen);

    // ***PHYSICAL***
    W_COERCE( bp->insert(key, el, dp->idx) ); 
}



/*********************************************************************
 *
 *   btree_remove_log
 *
 *   Log a logical removal from a btree.
 *
 *********************************************************************/
typedef btree_insert_t btree_remove_t;

btree_remove_log::btree_remove_log(
    const page_p& 	page, 
    int 		idx,
    const cvec_t& 	key, 
    const cvec_t& 	el,
    bool 		unique)
{
    const btree_p& bp = * (btree_p*) &page;
    fill(&page.pid(), page.tag(),
	 (new (_data) btree_remove_t(bp, idx, key, el, unique))->size());
}

#ifndef W_DEBUG
#define page /* page not used */
#endif
void
btree_remove_log::undo(page_p* page)
#undef page
{
    w_assert3(page == 0);
    btree_remove_t* dp = (btree_remove_t*) _data;

    cvec_t key, el;
    key.put(dp->data, dp->klen);
    el.put(dp->data + dp->klen, dp->elen);

    /*
     *  Note: we do not care if this is a unique btree because
     *	      if everything is correct, we could assume a 
     *        non-unique btree.
     *  Note to note: if we optimize kvl locking for unique btree, we
     *                cannot assume non-uniqueness.
     */
    // ***LOGICAL***
    W_COERCE( smlevel_2::bt->_insert(dp->root, dp->unique, 
				     smlevel_0::t_cc_kvl, key, el) ); 
}

void 
btree_remove_log::redo(page_p* page)
{
    btree_p* bp = (btree_p*) page;
    btree_remove_t* dp = (btree_remove_t*) _data;

#ifdef DEBUG
    cvec_t key, el;
    key.put(dp->data, dp->klen);
    el.put(dp->data + dp->klen, dp->elen);

    btrec_t r(*bp, dp->idx);
    w_assert3(el == r.elem());
    w_assert3(key == r.key());
#endif /*DEBUG*/

    W_COERCE( bp->remove(dp->idx) ); // ***PHYSICAL***
}



/*********************************************************************
 *
 *   rtree_insert_log
 *
 *********************************************************************/
struct rtree_insert_t {
    lpid_t	root;
    int2	idx;
    uint2	klen;
    uint2	elen;
    char	data[logrec_t::data_sz - sizeof(lpid_t) - 3*sizeof(int2)];

    rtree_insert_t(const rtree_p& page, int idx, const nbox_t& key,
		   const cvec_t& el);
    size()	{ return data + klen + elen - (char*) this; }
};

rtree_insert_t::rtree_insert_t(const rtree_p& _page, int _idx,
			       const nbox_t& key, const cvec_t& el)
: idx(_idx), klen(key.klen()), elen(el.size())
{
    _page.root(root);
    w_assert1((size_t)(klen + elen) < sizeof(data));
    memcpy(data, key.kval(), klen);
    el.copy_to(data + klen);
}

rtree_insert_log::rtree_insert_log(const page_p& page, int idx, 
				   const nbox_t& key, const cvec_t& el)
{
    const rtree_p& rp = * (rtree_p*) &page;
    fill(&page.pid(), page.tag(),
	 (new (_data) rtree_insert_t(rp, idx, key, el))->size());
}

#ifndef W_DEBUG
#define page /* page not used */
#endif
void
rtree_insert_log::undo(page_p* page)
#undef page
{
    w_assert3(page == 0);
    rtree_insert_t* dp = (rtree_insert_t*) _data;

    nbox_t key(dp->data, dp->klen);
    cvec_t el;
    el.put(dp->data + dp->klen, dp->elen);
    W_COERCE( smlevel_2::rt->remove(dp->root, key, el) ); // ***LOGICAL***
}

void
rtree_insert_log::redo(page_p* page)
{
    rtree_p* rp = (rtree_p*) page;
    rtree_insert_t* dp = (rtree_insert_t*) _data;
    
    nbox_t key(dp->data, dp->klen);
    cvec_t el;
    el.put(dp->data + dp->klen, dp->elen);

    W_COERCE( rp->insert(key, el) ); // ***PHYSICAL***
}



/*********************************************************************
 *
 *   rtree_remove_log
 *
 *********************************************************************/
typedef rtree_insert_t rtree_remove_t;

rtree_remove_log::rtree_remove_log(const page_p& page, int idx,
				   const nbox_t& key, const cvec_t& el)
{
    const rtree_p& rp = * (rtree_p*) &page;
    fill(&page.pid(), page.tag(),
	 (new (_data) rtree_remove_t(rp, idx, key, el))->size());
}

#ifndef W_DEBUG
#define page /* page not used */
#endif
void
rtree_remove_log::undo(page_p* page)
#undef page
{
    w_assert3(page == 0);
    rtree_remove_t* dp = (rtree_remove_t*) _data;

    nbox_t key(dp->data, dp->klen);
    cvec_t el;
    el.put(dp->data + dp->klen, dp->elen);
    W_COERCE( smlevel_2::rt->insert(dp->root, key, el) ); // ***LOGICAL***
}

void
rtree_remove_log::redo(page_p* page)
{
    rtree_p* rp = (rtree_p*) page;
    rtree_remove_t* dp = (rtree_remove_t*) _data;

#ifdef DEBUG
    nbox_t key(dp->data, dp->klen);
    cvec_t el;
    el.put(dp->data + dp->klen, dp->elen);
    w_assert3(el.cmp(rp->rec(dp->idx).elem(), rp->rec(dp->idx).elen()) == 0);
    w_assert3(memcmp(key.kval(), rp->rec(dp->idx).key(),
				rp->rec(dp->idx).klen()) == 0);
#endif /*DEBUG*/

    W_COERCE( rp->remove(dp->idx) ); // ***PHYSICAL***
}



#ifdef USE_RDTREE
/*********************************************************************
 *
 *   rdtree_insert_log
 *
 *********************************************************************/
struct rdtree_insert_t {
    lpid_t	root;
    int2	idx;
    int2	klen;
    int2	elen;
    char	data[logrec_t::data_sz - sizeof(lpid_t) - 3*sizeof(int2)];

    rdtree_insert_t(const rdtree_p& page, int idx, const rangeset_t& key,
		   const cvec_t& el);
    size()	{ return data + klen + elen - (char*) this; }
};

rdtree_insert_t::rdtree_insert_t(const rdtree_p& _page, int _idx,
				 const rangeset_t& key, const cvec_t& el)
: idx(_idx), klen(key.klen()), elen(el.size())
{
    _page.root(root);
    w_assert1(klen + elen < sizeof(data));
    memcpy(data, key.kval(), key.hdrlen());
    memcpy(data + key.hdrlen(), key.dataaddr(), key.datalen());
    el.copy_to(data + klen);
}

rdtree_insert_log::rdtree_insert_log(const page_p& page, int idx, 
				   const rangeset_t& key, const cvec_t& el)
{
    const rdtree_p& rp = * (rdtree_p*) &page;
    fill(&page.pid(), page.tag(),
	 (new (_data) rdtree_insert_t(rp, idx, key, el))->size());
}

void
rdtree_insert_log::undo(page_p* page)
{
    w_assert3(page == 0);
    rdtree_insert_t* dp = (rdtree_insert_t*) _data;

    rangeset_t key(dp->data, (int)dp->klen);
    cvec_t el;
    el.put(dp->data + dp->klen, dp->elen);
    W_COERCE( smlevel_2::rdt->remove(dp->root, key, el) ); // ***LOGICAL***
}

void
rdtree_insert_log::redo(page_p* page)
{
    rdtree_p* rp = (rdtree_p*) page;
    rdtree_insert_t* dp = (rdtree_insert_t*) _data;
    
    rangeset_t key(dp->data, (int) dp->klen);
    cvec_t el;
    el.put(dp->data + dp->klen, dp->elen);

    W_COERCE( rp->insert(key, el) ); // ***PHYSICAL***
}


/*********************************************************************
 *
 *   rdtree_remove_log
 *
 *********************************************************************/
typedef rdtree_insert_t rdtree_remove_t;

rdtree_remove_log::rdtree_remove_log(const page_p& page, int idx,
				   const rangeset_t& key, const cvec_t& el)
{
    const rdtree_p& rp = * (rdtree_p*) &page;
    fill(&page.pid(), page.tag(),
	 (new (_data) rdtree_remove_t(rp, idx, key, el))->size());
}

void
rdtree_remove_log::undo(page_p* page)
{
    w_assert3(page == 0);
    rdtree_remove_t* dp = (rdtree_remove_t*) _data;

    rangeset_t key(dp->data, (int) dp->klen);
    cvec_t el;
    el.put(dp->data + dp->klen, dp->elen);
    W_COERCE( smlevel_2::rdt->insert(dp->root, key, el) ); // ***LOGICAL***
}


void
rdtree_remove_log::redo(page_p* page)
{
    rdtree_p* rp = (rdtree_p*) page;
    rdtree_remove_t* dp = (rdtree_remove_t*) _data;

#ifdef DEBUG
    rangeset_t key(dp->data, (int) dp->klen);
    cvec_t el;
    el.put(dp->data + dp->klen, dp->elen);
    w_assert3(el.cmp(rp->rec(dp->idx).elem(), rp->rec(dp->idx).elen()) == 0);
    w_assert3(memcmp(key.kval(), rp->rec(dp->idx).key(),
				key.hdrlen()) == 0);
    w_assert3(memcmp(key.dataaddr(), rp->rec(dp->idx).key() + key.hdrlen(),
		   rp->rec(dp->idx).klen() - key.hdrlen()) == 0);
#endif /*DEBUG*/

    W_COERCE( rp->remove(dp->idx) ); // ***PHYSICAL***
}
#endif /* USE_RDTREE */


/*********************************************************************
 *
 *   ext_insert_log
 *
 *   Log a logical allocation of an extent (insertion into extent list)
 *
 *********************************************************************/
struct ext_log_t {
    stid_t	stid;
    bool        force;
    fill1       filler1; // for purify
    fill2       filler2; // for purify
    int         count;
    const       int max_loggable =  ((logrec_t::data_sz - sizeof(stid_t) -
				    sizeof(int))/sizeof(ext_log_info_t));
    ext_log_info_t list[ext_log_t::max_loggable];

    ext_log_t(stid_t _stid, int _count, const ext_log_info_t *_list, bool force=false);
    size()	{ return ((char *)list + (count * sizeof(ext_log_info_t)))  - (char*) this; }
};

ext_log_t::ext_log_t(stid_t _stid, int _count, const ext_log_info_t *_list, bool f)
    : stid(_stid), force(f), count(_count)
{
    FUNC(ext_log_t::ext_log_t);
    w_assert1(_count < ext_log_t::max_loggable);
    w_assert3((int)force == 0 || (int)force == 1 );

    DBG(<<"ext_log_t store " << stid << " count " << count );

    for(int j=0; j< _count; j++) {
	DBG( << " ext " << _list[j].ext 
	    << " next " << _list[j].next
	    << " prev " << _list[j].prev
	    );

        list[j] = _list[j];
    }
}

ext_insert_log::ext_insert_log(
    const page_p& page, 
    stid_t& _stid, int _count, 
    const ext_log_info_t *_list
)
{
    w_assert1(_count < ext_log_t::max_loggable);
    fill(&page.pid(), page.tag(),  (new (_data) ext_log_t(_stid,_count,_list))->size());
}

ext_remove_log::ext_remove_log(const page_p& page, 
	stid_t& _stid, int _count, 
	const ext_log_info_t *_list,
	bool force
	)
{
    fill(&page.pid(), page.tag(), (new (_data) ext_log_t(_stid,_count,_list,force))->size());
}

#define page /* page not used */
void 
ext_insert_log::undo(page_p* page)
#undef page
{
    FUNC(ext_insert_log::undo);
    // ***LOGICAL*** 

    ext_log_t *t = (ext_log_t *) _data;
    w_assert3(t->force == false);
    W_COERCE(smlevel_0::io->recover_extent(false, t->stid, t->count, t->list)) ;
}

#define page /* page not used */
void 
ext_remove_log::undo(page_p* page)
#undef page
{
    FUNC(ext_insert_log::redo);
    // ***LOGICAL*** 
    ext_log_t *t = (ext_log_t *) _data;
    W_COERCE(smlevel_0::io->recover_extent(true, t->stid, t->count, t->list)) ;
}

/*********************************************************************
 *
 *  operator<<(ostream, logrec)
 *
 *  Pretty print a log record to ostream.
 *
 *********************************************************************/
#include <logtype.i>
ostream& 
operator<<(ostream& o, const logrec_t& l)
{
    long f = o.flags();
    o.setf(ios::left, ios::left);

    o << l._tid << ' ' << setw(15) << l.type_str() 
      << setw(5) << l.cat_str() << "  " << l._pid;

    switch(l.type()) {
	case t_ext_insert : 
	case t_ext_remove : 
		{
		    // print the store/extent/ord #
		    ext_log_t *t = (ext_log_t *) l._data;
		    ext_log_info_t *u =  t->list;
		    o << t->stid 
			<< "/" << u->prev 
			<< ">" << u->ext 
			<< "<" << u->next ; 
		}
		break;
	case t_page_reclaim : 
	case t_page_mark : 
	 	{ 
		    page_mark_t *t = (page_mark_t *)l._data;
		    o << t->idx ;
		}
		break;

	default: /* nothing */
		break;
    }

    if (l.is_cpsn())  o << " (" << l._undo_nxt << ')';

    o.flags(f);
    return o;
}

/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

/*
 *  $Id: dir.c,v 1.63 1996/05/06 20:10:31 nhall Exp $
 */

#define SM_SOURCE
//
#define DIR_C

#ifdef __GNUG__
#pragma implementation "dir.h"
#pragma implementation "sdesc.h"
#endif

#include <sm_int.h>

// for btree_p::slot_zero_size()
#include "btree_p.h"  


/*
 *  Directory is keyed on snum_t which is a ushort.
 */
static const key_type_s dir_key_type(key_type_s::u, 0, 2);

rc_t
dir_vol_m::_mount(const char* const devname, vid_t vid)
{
    w_assert1(! vid.is_remote());

    if (_cnt >= max)   return RC(eNVOL);

    if (vid == vid_t::null) return RC(eBADVOL);

    int i;
    for (i = 0; i < max; i++)  {
	if (_root[i].vol() == vid)  return RC(eALREADYMOUNTED);
    }
    for (i = 0; i < max && (_root[i] != lpid_t::null); i++);
    w_assert1(i < max);

    W_DO(io->mount(devname, vid));

    stid_t stid;
    stid.vol = vid;
    stid.store = 1;

    lpid_t pid;
    rc_t rc = io->first_page(stid, pid);
    if (rc)  {
	W_COERCE(io->dismount(vid, FALSE));
	return RC(rc.err_num());
    }
    _root[i] = pid;
    ++_cnt;
    return RCOK;
}

#ifdef MULTI_SERVER

rc_t
dir_vol_m::_mount_remote(vid_t vid, const lpid_t& dir_root)
{
    w_assert3(vid.is_remote() && dir_root.vol() == vid);

    if (_cnt >= max)   return RC(eNVOL);

    if (vid == vid_t::null) return RC(eBADVOL);

    for (int i = 0; i < max; i++)  {
	if (_root[i].vol() == vid)  return RC(eALREADYMOUNTED);
    }
    for (i = 0; i < max && (_root[i] != lpid_t::null); i++);
    w_assert1(i < max);

    _root[i] = dir_root;
    ++_cnt;
    return RCOK;
}
#endif /* MULTI_SERVER */

rc_t
dir_vol_m::_dismount(vid_t vid, bool flush)
{
    int i;
    for (i = 0; i < max && _root[i].vol() != vid; i++);
    if (i >= max)  return RC(eBADVOL);
    
    if (vid.is_remote())	; // W_FATAL(eNOTIMPLEMENTED);
    else			W_DO(io->dismount(vid, flush));
    _root[i] = lpid_t::null;
    --_cnt;
    return RCOK;
}

rc_t
dir_vol_m::_dismount_all(bool flush)
{
    for (int i = 0; i < max; i++)  {
	if (_root[i])  {
	    W_DO( _dismount(_root[i].vol(), flush));
	}
    }
    return RCOK;
}

rc_t
dir_vol_m::_insert(const stpgid_t& stpgid, const sinfo_s& si)
{
    w_assert1(! stpgid.vol().is_remote());

    if (!si.store)   return RC(eBADSTID);

    int i = _find_root(stpgid.vol());
    if (i < 0) return RC(eBADSTID);

    vec_t el;
    el.put(&si, sizeof(si));
    if (stpgid.is_stid()) {
	vec_t key;
	key.put(&si.store, sizeof(si.store));
	W_DO( bt->insert(_root[i], 1, &dir_key_type,
			 TRUE, t_cc_none, key, el) );
    } else {
	// for 1-page stores, append sinfo to slot 0
	w_assert3(si.store == small_store_id);

	page_p page;
	W_DO(page.fix(stpgid.lpid, page_p::t_store_p, LATCH_EX, 0/*page flags*/));
	int slot_zero_size = page.tuple_size(0);
	W_DO(page.splice(0, slot_zero_size, 0, el));
    }

    return RCOK;
}

rc_t
dir_vol_m::_access(const stpgid_t& stpgid, sinfo_s& si)
{
    int i = _find_root(stpgid.vol());
    if (i < 0) return RC(eBADSTID);


    if (stpgid.is_stid()) {
	bool found;
	vec_t key;
	stid_t stid = stpgid.stid();
	key.put(&stid.store, sizeof(stid.store));

	smsize_t len = sizeof(si);
	W_DO( bt->lookup(_root[i], 1, &dir_key_type,
			 TRUE, t_cc_none, 
			 key, &si, len, found) );
	if (!found)	return RC(eBADSTID);
	w_assert1(len == sizeof(si));

    } else {
	page_p page;
	W_DO(page.fix(stpgid.lpid, page_p::t_store_p, LATCH_EX, 0/*page flags*/));
	smsize_t slot_zero_size = page.tuple_size(0);
	w_assert3(slot_zero_size >= sizeof(si));
	char* tuple = (char*)page.tuple_addr(0);
	memcpy((char*)&si, tuple+(slot_zero_size-sizeof(si)), sizeof(si));
	w_assert3(si.store == stpgid.store());
	w_assert3(si.root == stpgid.lpid.page);
    }
    return RCOK;
}

rc_t
dir_vol_m::_remove(const stpgid_t& stpgid)
{
    w_assert1(! stpgid.vol().is_remote());

    int i = _find_root(stpgid.vol());
    if (i < 0) return RC(eBADSTID);

    if (stpgid.is_stid()) {
	vec_t key, el;
	stid_t stid = stpgid.stid();
	key.put(&stid.store, sizeof(stid.store));
	sinfo_s si;
	W_DO( _access(stpgid, si) );
	el.put(&si, sizeof(si));

	W_DO( bt->remove(_root[i], 1, &dir_key_type,
			 TRUE, t_cc_none, key, el) );
    } else {
	// no need to do anything because store information is stored
	// on the page and the page will be deallocated by code in
	// other layers.
    }

    return RCOK;
}

rc_t
dir_vol_m::_create_dir(vid_t vid)
{
    w_assert1(! vid.is_remote());

    stid_t stid;
    W_DO( io->create_store(vid, 100/*unused*/, page_p::st_regular, stid) );
    w_assert1(stid.store == 1);

    lpid_t root;
    W_DO( bt->create(stid, root) );

    // add the directory index to the directory index
    sinfo_s sinfo(stid.store, t_index, 100, 100, t_uni_btree, t_regular,
		  root.page, serial_t::null, 0, NULL);
    vec_t key, el;
    key.put(&sinfo.store, sizeof(sinfo.store));
    el.put(&sinfo, sizeof(sinfo));
    W_DO( bt->insert(root, 1, &dir_key_type, TRUE, t_cc_none, key, el) );

    return RCOK;
}

int dir_vol_m::_find_root(vid_t vid)
{
    if (vid <= 0) return -1; // not found
    int i;
    for (i = 0; i < max && _root[i].vol() != vid; i++);
    if (i >= max) return -1;

    return i; // found
}

rc_t
dir_m::insert(const stpgid_t& stpgid, const sinfo_s& sinfo)
{
    w_assert3(stpgid.is_stid() || stpgid.lpid.page == sinfo.root);
    W_DO(_dir.insert(stpgid, sinfo)); 

    // as an optimization, add the sd to the dir_m hash table
    if (xct()) {
	w_assert3(xct()->sdesc_cache());
	xct()->sdesc_cache()->add(stpgid, sinfo);
    }

    return RCOK;
}

rc_t
dir_m::remove(const stpgid_t& stpgid)
{
    if (xct()) {
	w_assert3(xct()->sdesc_cache());
	xct()->sdesc_cache()->remove(stpgid);
    }

    W_DO(_dir.remove(stpgid)); 
    return RCOK;
}

//
// This is a method used only for large obejct sort to
// to avoid copying large objects around. It transfers
// the large store from old_stid to new_stid and destroy
// the old_stid store.
//
rc_t
dir_m::remove_n_swap(const stid_t& old_stid, const stid_t& new_stid)
{
    sinfo_s new_sinfo;
    sdesc_t* desc;

    // read and copy the new store info
    W_DO( access(new_stid, desc, EX) );
    new_sinfo = desc->sinfo();

    // read the old store info and swap the large object store
    W_DO( access(old_stid, desc, EX) );
    new_sinfo.large_store = desc->sinfo().large_store;

    // remove entries in the cache 
    if (xct()) {
	w_assert3(xct()->sdesc_cache());
	xct()->sdesc_cache()->remove(old_stid);
	xct()->sdesc_cache()->remove(new_stid);
    }

    // remove the old entries
    W_DO(_dir.remove(old_stid)); 
    W_DO(_dir.remove(new_stid));

    // reinsert the new sinfo
    W_DO( insert(new_stid, new_sinfo) );

    return RCOK;
}

/*
 * dir_m::access(stpgid, sd, mode, lklarge)
 *
 *  cache the store descriptor for the given store id (or small
 *     btree)
 *  lock the store in the given mode
 *  If lklarge==true and the store has an associated large-object
 *     store,  lock it also in the given mode.  NB : THIS HAS
 *     IMPLICATIONS FOR LOGGING LOCKS for prepared txs -- there
 *     could exist an IX lock for a store w/o any higher-granularity
 *     EX locks under it, for example.  Thus, the assumption in 
 *     recovering from prepare/crash, that it is sufficient to 
 *     reaquire only the EX locks, could be violated.  See comments
 *     in xct.c, in xct_t::log_prepared()
 */

rc_t
dir_m::access(const stpgid_t& stpgid, sdesc_t*& sd, lock_mode_t mode, 
	bool lklarge)
{
#ifdef DEBUG
    if(xct()) {
	w_assert3(xct()->sdesc_cache());
    }
#endif
    sd = xct() ? xct()->sdesc_cache()->lookup(stpgid): 0;

    if (! sd) {

	// lock the store
	if (mode != NL) {
	    W_DO(lm->lock(stpgid, mode, t_long,
				   WAIT_SPECIFIED_BY_XCT));
	}

	sinfo_s  sinfo;
	W_DO(_dir.access(stpgid, sinfo));
	w_assert3(xct()->sdesc_cache());
	sd = xct()->sdesc_cache()->add(stpgid, sinfo);

	// this assert forces an assert check in root() that
	// we want to run
	w_assert3(sd->root().store() != 0);

	// NB: see comments above function header
	if (lklarge && mode != NL && sd->large_stid()) {
	    W_DO(lm->lock(sd->large_stid(), mode, t_long,
				   WAIT_SPECIFIED_BY_XCT));
	}
    } else     {
	// this assert forces an assert check in root() that
	// we want to run
	w_assert3(sd->root().store() != 0);
	w_assert3(sd->store_flags() != 0);

	//
	// the info on this store is cached, therefore we assume
	// it is IS locked.  Note, that if the sdesc held a lock
	// mode we could avoid other locks as well.  However,
	// This only holds TRUE for long locks that cannot be
	// release.  If they can be released, then this must be
	// rethought.
	//
	if (mode != IS && mode != NL) {
	    W_DO(lm->lock(stpgid, mode,
			   t_long, /*see above comment before changing*/
			   WAIT_SPECIFIED_BY_XCT));
	}

	// NB: see comments above function header
	if (lklarge && mode != IS && mode != NL && sd->large_stid()) {
	    W_DO(lm->lock(sd->large_stid(), mode, t_long,
				   WAIT_SPECIFIED_BY_XCT));
	}
	
    }

    w_assert3(stpgid == sd->stpgid());
    return RCOK;
}


sdesc_cache_t::sdesc_cache_t()
{
    _cache_size = min_sdesc;
    _free = new bool[cache_size()];
    _sdescs = new sdesc_t[cache_size()];

    for (int i = 0; i < cache_size(); i++) _free[i] = TRUE;
    _last_access = 0;
    _last_alloc = -1;
}

sdesc_cache_t::~sdesc_cache_t()
{
    _cache_size = 0;
    delete[] _free;
    _free = 0;
    delete[] _sdescs;
    _sdescs = 0;
}


sdesc_t* sdesc_cache_t::lookup(const stpgid_t& stpgid)
{
    if (!_free[_last_access] && _sdescs[_last_access].stpgid() == stpgid) {
	return _sdescs+_last_access;
    }

    int i;
    for (i = 0; i < cache_size(); i++) {
	if (!_free[i] && _sdescs[i].stpgid() == stpgid) {
	    _last_access = i;
	    return _sdescs+i;
	}
    }
    return NULL;
}

void sdesc_cache_t::remove(const stpgid_t& stpgid)
{
    for (int i = 0; i < cache_size(); i++) {
	if (!_free[i] && _sdescs[i].stpgid() == stpgid) {
	    _free[i] = TRUE;
	}
    }
}

void sdesc_cache_t::remove_all()
{
    for (int i = 0; i < cache_size(); i++) {
	_free[i] = TRUE;
    }
}


sdesc_t* sdesc_cache_t::add(const stpgid_t& stpgid, const sinfo_s& sinfo)
{
    sdesc_t *result=0;

    w_assert3(stpgid.is_stid() || stpgid.lpid.page == sinfo.root);
    int i;
    for (i = 0; i < cache_size(); i++) {
	if (_free[i]) break;
    }
    if (i < cache_size())  {
	_last_alloc = i;
    } else {

#undef OLDWAY
#ifdef OLDWAY
	// need to remove one that is cached 
	_last_alloc = (_last_alloc+1)%cache_size();
	w_assert3(_free[_last_alloc] == FALSE);
#else
	// expand the cache instead

	int old_size = cache_size();
	bool *old_free = _free;
	sdesc_t *old_sdescs = _sdescs;

	_cache_size <<= 1; // incr by power of 2
	_free = new bool[cache_size()];
	_sdescs = new sdesc_t[cache_size()];

	int i;
	for (i = 0; i < old_size; i++) {
	    _free[i] = old_free[i];
	    _sdescs[i] = old_sdescs[i];
	}
	delete[] old_free;
	delete[] old_sdescs;

	// _last_alloc will be the  first new slot 
	for (_last_alloc=i; i < cache_size(); i++) {
	    _free[i] = TRUE;
	}
	// _last_access unchanged
#endif
    }
    _sdescs[_last_alloc].init(stpgid, sinfo);
    _free[_last_alloc] = FALSE;

    result =  _sdescs+_last_alloc;
    {
	uint4 flags;
	// If we cache this info, we can avoid many future other
	// page fixes of the stnode_p
	W_COERCE( smlevel_0::io->get_store_flags(stpgid.stid(), flags));
	result->set_store_flags(flags);
    }
    return result;
}


pid_cache_t::pid_cache_t(int sz, const stid_t& id) :
                        _stid(id), _num(0), _curr(0), _size(sz), _pids(0)
{
    _pids = new shpid_t[_size];
    if (!_pids) W_FATAL(eOUTOFMEMORY);
}


#ifndef W_DEBUG
#define pid /* pid not used */
#endif
shpid_t pid_cache_t::next_page(const lpid_t& pid)
#undef pid
{
    w_assert3(pid.stid() == _stid);
    w_assert3(num == 0 || _curr == 0 || _pids[_curr-1] == pid.page);

    return (num > 0 && _curr < _num) ? _pids[_curr++] : 0;
}

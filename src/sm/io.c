/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

/*
 *  $Id: io.c,v 1.110 1996/07/19 17:09:59 nhall Exp $
 */

#define SM_SOURCE
#define IO_C

#ifdef __GNUG__
#   pragma implementation
#endif

#include "hash_lru.h"
#include "sm_int.h"
#include "vol.h"
#include "device.h"
#include "lock.h"
#include "xct.h"
#include "logrec.h"
#include "logdef.i"
#include <crash.h>

#define DBGTHRD(arg) DBG(<<" th."<<me()->id << " " arg)

struct extnum_struct_t {
    // last extent used for the store
    extnum_t 	ext;

    // hint about free pages remaining in the last 2 extents
    bool 	has_free_pages;
};

//typedef hash_lru_t<extnum_t, stid_t> last_extent_used_t;
//typedef hash_lru_i<extnum_t, stid_t> last_extent_used_i;
#define last_extent_used_t hash_lru_t<extnum_struct_t, stid_t>
#define last_extent_used_i hash_lru_i<extnum_struct_t, stid_t>

#ifdef __GNUG__
template class w_auto_delete_array_t<extnum_t>;
template class last_extent_used_t;
template class last_extent_used_i;
//typedef hash_lru_entry_t<extnum_t, stid_t> last_extent_used_entry_t;
#define last_extent_used_entry_t hash_lru_entry_t<extnum_struct_t, stid_t>
template class last_extent_used_entry_t;
template class w_list_t<last_extent_used_entry_t>;
template class w_list_i<last_extent_used_entry_t>;
template class w_hash_t<last_extent_used_entry_t, stid_t>;
#endif


/*********************************************************************
 *
 *  Class static variables
 *
 *	_msec_disk_delay	: delay in performing I/O (for debugging)
 *	_mutex			: make io_m a monitor
 *	vol_cnt			: # volumes mounted in vol[]
 *	vol[]			: array of volumes mounted
 *	_last_extent_used	: see comments below
 *
 *********************************************************************/
uint4  		io_m::_msec_disk_delay = 0;
smutex_t 	io_m::_mutex("io_m");
int 		io_m::vol_cnt = 0;
vol_t* 		io_m::vol[io_m::max_vols] = { 0 };
lsn_t		io_m::_lastMountLSN = lsn_t::null;

static last_extent_used_t* _last_extent_used = 0;
/*********************************************************************
 *  _last_extent_used: a cache of the last extent used for dealloc/
 *  	or allocation of pages within an extent.  This is used in order
 *   	to achieve a quasi-round-robin behavior when looking for an
 *    	allocated extent with free pages.   We keep a cached element
 * 	per store (key is store id).  This info is NOT tied to the
 *  	store descriptor cache because the whole point of this is for
 *      the cached info to survive transactions and to be shared between
 * 	transactions, whereas teh sdesc cache is a per-thread/xct thing.
 *********************************************************************/

void			
io_m::enter()
{
    if(xct()) { xct()->start_crit(); }
    W_COERCE( _mutex.acquire() );
}
void			
io_m::leave()
{
    // flush the log before we leave, to free
    // the last page pinned-- necessary to avoid
    // latch-latch deadlock
    // this gets done
    // in xct_t::stop_crit() if(xct()) xct()->flush_logbuf();
    // 
    if(xct()) { xct()->stop_crit(); }

    _mutex.release();
}

rc_t
io_m::lock_force(
    const lockid_t&	    n,
    lock_mode_t		    m,
    lock_duration_t         d,
    long		    timeout, 
    bool		    
#ifdef MULTI_SERVER
	    optimistic
#endif /*MULTI_SERVER*/
	,
    extlink_p		    *page
)
{
   /* Try to acquire the lock w/o a timeout first */
#ifndef MULTI_SERVER
   rc_t  rc = lm->lock_force(n, m, d, WAIT_IMMEDIATE);
#else
   rc_t  rc = lm->lock_force(n, m, d, WAIT_IMMEDIATE, optimistic);
#endif
   if(rc && (rc.err_num() == eMAYBLOCK || rc.err_num() == eLOCKTIMEOUT)
	&& timeout != WAIT_IMMEDIATE) {
       w_assert3(me()->xct());
       if(page && page->is_fixed()) {
	  page->unfix();
       }

       // flush the log buf to unfix the page
       me()->xct()->flush_logbuf();
       _mutex.release();
#ifndef MULTI_SERVER
       rc = lm->lock_force(n, m, d, timeout);
#else
       rc = lm->lock_force(n, m, d, timeout, optimistic);
#endif
       W_COERCE( _mutex.acquire() );
   }
   return rc;
}

/* friend -- called from vol.c */
rc_t
io_lock_force(
    const lockid_t&	    n,
    lock_mode_t		    m,
    lock_duration_t         d,
    long		    timeout, 
#ifdef MULTI_SERVER
    bool		    optimistic,
    extlink_p		    *page
#else
    bool		    ,
    extlink_p		    *
#endif /*MULTI_SERVER*/
    )
{
#ifndef MULTI_SERVER
    return smlevel_0::io->lock_force(n,m,d,timeout);
#else
    return smlevel_0::io->lock_force(n,m,d,timeout,optimistic, page);
#endif
}

io_m::io_m()
{
    _lastMountLSN = lsn_t::null;

    // Initialize a 32 element free page cache (see io.h for more info
    if (_last_extent_used == 0) {
	if (!(_last_extent_used = new last_extent_used_t (32, "io_m free page cache"))) {
	    W_FATAL(eOUTOFMEMORY);
	}
    }
}

/*********************************************************************
 *
 *  io_m::~io_m()
 *
 *  Destructor. Dismount all volumes.
 *
 *********************************************************************/
io_m::~io_m()
{
    W_COERCE(_dismount_all(shutdown_clean));
    if (_last_extent_used) {
	delete _last_extent_used;
	_last_extent_used = 0;
    }
}


/*********************************************************************
 *
 *  io_m::invalidate_free_page_cache()
 *
 * This method is used by the transaction manager whenever to indicate
 * that the free page (extent) cache is invalid.  If a transaction that
 * has deallocated pages commits or a transaction that has allocated
 * pages aborts, this method will be called.
 *
 *********************************************************************/
void io_m::invalidate_free_page_cache()
{
    enter();
    _clear_last_extent_used();
    leave();
}

/*********************************************************************
 *
 *  io_m::find(vid)
 *
 *  Search and return the index for vid in vol[]. 
 *  If not found, return -1.
 *
 *********************************************************************/
inline int 
io_m::_find(vid_t vid)
{
    if (!vid) return -1;
    register i;
    for (i = 0; i < max_vols; i++)  {
	if (vol[i] && vol[i]->vid() == vid) break;
    }
    return (i >= max_vols) ? -1 : i;
}



/*********************************************************************
 *
 *  io_m::is_mounted(vid)
 *
 *  Return true if vid is mounted. False otherwise.
 *
 *********************************************************************/
bool
io_m::is_mounted(vid_t vid)
{
    enter();

    int i = _find(vid);
    leave();
    return i >= 0;
}



/*********************************************************************
 *
 *  io_m::_dismount_all(flush)
 *
 *  Dismount all volumes mounted. If "flush" is true, then ask bf
 *  to flush dirty pages to disk. Otherwise, ask bf to simply
 *  invalidate the buffer pool.
 *
 *********************************************************************/
rc_t
io_m::_dismount_all(bool flush)
{
    for (int i = 0; i < max_vols; i++)  {
	if (vol[i])	{
	    errlog->clog << "warning: volume " << vol[i]->vid() << " still mounted\n"
	    << "         automatic dismount" << flushl;
	    W_DO(_dismount(vol[i]->vid(), flush));
	}
    }
    
    w_assert3(vol_cnt == 0);

    smlevel_0::stats.vol_reads = smlevel_0::stats.vol_writes = 0;
    return RCOK;
}




/*********************************************************************
 *
 *  io_m::sync_all_disks()
 *
 *  Sync all volumes.
 *
 *********************************************************************/
rc_t
io_m::sync_all_disks()
{
    for (int i = 0; i < max_vols; i++) {
	if (_msec_disk_delay > 0) {
	    W_IGNORE( me()->block(_msec_disk_delay, 
				  0, "io_m::sync_all_disks") );
	}
	if (vol[i]) vol[i]->sync();
    }
    return RCOK;
}



/*********************************************************************
 *
 *  io_m::_dev_name(vid)
 *
 *  Return the device name for volume vid if it is mounted. Otherwise,
 *  return NULL.
 *
 *********************************************************************/
const char* 
io_m::_dev_name(vid_t vid)
{
    int i = _find(vid);
    return i >= 0 ? vol[i]->devname() : 0;
}




/*********************************************************************
 *
 *  io_m::_is_mounted(dev_name)
 *
 *********************************************************************/
bool
io_m::_is_mounted(const char* dev_name)
{
    return dev->is_mounted(dev_name);
}



/*********************************************************************
 *
 *  io_m::_mount_dev(dev_name, vol_cnt)
 *
 *********************************************************************/
rc_t
io_m::_mount_dev(const char* dev_name, u_int& vol_cnt)
{
    FUNC(io_m::_mount_dev);

    volhdr_t vhdr;
    W_DO(vol_t::read_vhdr(dev_name, vhdr));
    device_hdr_s dev_hdr(vhdr.format_version, vhdr.device_quota_KB, vhdr.lvid);
    rc_t result = dev->mount(dev_name, dev_hdr, vol_cnt);
    return result;
}



/*********************************************************************
 *
 *  io_m::_dismount_dev(dev_name)
 *
 *********************************************************************/
rc_t
io_m::_dismount_dev(const char* dev_name)
{
    return dev->dismount(dev_name);
}


/*********************************************************************
 *
 *  io_m::_dismount_all_dev()
 *
 *********************************************************************/
rc_t
io_m::_dismount_all_dev()
{
    return dev->dismount_all();
}


/*********************************************************************
 *
 *  io_m::_list_devices(dev_list, devid_list, dev_cnt)
 *
 *********************************************************************/
rc_t
io_m::_list_devices(
    const char**& 	dev_list, 
    devid_t*& 		devid_list, 
    u_int& 		dev_cnt)
{
    return dev->list_devices(dev_list, devid_list, dev_cnt);
}


/*********************************************************************
 *
 *  io_m::_get_vid(lvid)
 *
 *********************************************************************/
inline vid_t 
io_m::_get_vid(const lvid_t& lvid)
{
    register i;
    for (i = 0; i < max_vols; i++)  {
	if (vol[i] && vol[i]->lvid() == lvid) break;
    }
    return (i >= max_vols) ? vid_t::null : vol[i]->vid();
}


/*********************************************************************
 *  io_m::_get_device_quota()
 *********************************************************************/
rc_t
io_m::_get_device_quota(const char* device, smksize_t& quota_KB,
			smksize_t& quota_used_KB)
{
    W_DO(dev->quota(device, quota_KB));

    lvid_t lvid;
    W_DO(_get_lvid(device, lvid));
    if (lvid == lvid_t::null) {
	// no device on volume
	quota_used_KB = 0;
    } else {
	smksize_t dummy;
	W_DO(_get_volume_quota(_get_vid(lvid), quota_used_KB, dummy));
    }
    return RCOK;
}


/*********************************************************************
 *
 *  io_m::_get_lvid(dev_name, lvid)
 *
 *********************************************************************/
rc_t
io_m::_get_lvid(const char* dev_name, lvid_t& lvid)
{
    if (!dev->is_mounted(dev_name)) return RC(eDEVNOTMOUNTED);
    register i;
    for (i = 0; i < max_vols; i++)  {
	if (vol[i] && (strcmp(vol[i]->devname(), dev_name) == 0) ) break;
    }
    lvid = (i >= max_vols) ? lvid_t::null : vol[i]->lvid();
    return RCOK;
}



/*********************************************************************
 *
 *  io_m::_get_vols(start, count, dname, vid, ret_cnt)
 *
 *  Fill up dname[] and vid[] starting from volumes mounted at index
 *  "start". "Count" indicates number of entries in dname and vid.
 *  Return number of entries filled in "ret_cnt".
 *
 *********************************************************************/
rc_t
io_m::_get_vols(
    int 	start, 
    int 	count,
    char 	dname[][smlevel_0::max_devname+1],
    vid_t 	vid[], 
    int& 	ret_cnt)
{
    ret_cnt = 0;
    w_assert1(start + count <= max_vols);
   
    /*
     *  i iterates over vol[] and j iterates over dname[] and vid[]
     */
    int i, j;
    for (i = start, j = 0; i < max_vols; i++)  {
	if (vol[i])  {
	    w_assert3(j < count);
	    vid[j] = vol[i]->vid();
	    strncpy(dname[j], vol[i]->devname(), max_devname);
	    j++;
	}
    }
    ret_cnt = j;
    return RCOK;
}



/*********************************************************************
 *
 *  io_m::_get_lvid(vid)
 *
 *********************************************************************/
inline lvid_t
io_m::_get_lvid(const vid_t vid)
{
    int i = _find(vid);
    return (i >= max_vols) ? lvid_t::null : vol[i]->lvid();
}



/*********************************************************************
 *
 *  io_m::get_lvid(dev_name, lvid)
 *
 *********************************************************************/
rc_t
io_m::get_lvid(const char* dev_name, lvid_t& lvid)
{
    enter();
    rc_t rc = _get_lvid(dev_name, lvid);
    leave();
    return rc.reset();
}


/*********************************************************************
 *
 *  io_m::get_lvid(vid)
 *
 *********************************************************************/
lvid_t
io_m::get_lvid(const vid_t vid)
{
    enter();
    lvid_t lvid = _get_lvid(vid);
    leave();
    return lvid;
}



/*********************************************************************
 *
 *  io_m::mount(device, vid)
 *  io_m::_mount(device, vid)
 *
 *  Mount "device" with vid "vid".
 *
 *********************************************************************/
rc_t
io_m::mount(const char* device, vid_t vid)
{
	// grab chkpt_mutex to prevent mounts during chkpt
	// need to serialize writing dev_tab and mounts
	smlevel_3::chkpt->chkpt_mutex_acquire();
    enter();
    rc_t r = _mount(device, vid);
    leave();
	smlevel_3::chkpt->chkpt_mutex_release();
    return r;
}


rc_t
io_m::_mount(const char* device, vid_t vid)
{
    FUNC(io_m::_mount);
    DBG( << "_mount(name=" << device << ", vid=" << vid << ")");
    register i;
    for (i = 0; i < max_vols && vol[i]; i++);
    if (i >= max_vols) return RC(eNVOL);

    vol_t* v = new vol_t;
    if (! v) return RC(eOUTOFMEMORY);

    w_rc_t rc = v->mount(device, vid);
    if (rc)  {
	delete v;
	return RC_AUGMENT(rc);
    }

    int j = _find(vid);
    if (j >= 0)  {
	W_COERCE( v->dismount(FALSE) );
	delete v;
	return RC(eALREADYMOUNTED);
    }
    
    ++vol_cnt;

    w_assert3(vol[i] == 0);
    vol[i] = v;

    if (log && !log->IsInRecovery())  {
        logrec_t* logrec = new logrec_t;
        w_assert1(logrec);

        new (logrec) mount_vol_log(device, vid);
	logrec->fill_xct_attr(tid_t::null, GetLastMountLSN());
	lsn_t theLSN;
        W_COERCE( log->insert(*logrec, &theLSN) );
	DBG( << "mount_vol_log(" << device << ", vid=" << vid << ") lsn=" << theLSN << " prevLSN=" << GetLastMountLSN());
	SetLastMountLSN(theLSN);

        delete logrec;
    }

    CRASHTEST("io_m::_mount.1");

    return RCOK;
}



/*********************************************************************
 *
 *  io_m::dismount(vid, flush)
 *  io_m::_dismount(vid, flush)
 *
 *  Dismount the volume "vid". "Flush" indicates whether to write
 *  dirty pages of the volume in bf to disk.
 *
 *********************************************************************/
rc_t
io_m::dismount(vid_t vid, bool flush)
{
    // grab chkpt_mutex to prevent dismounts during chkpt
    // need to serialize writing dev_tab and dismounts

    smlevel_3::chkpt->chkpt_mutex_acquire();
    enter();
    rc_t r = _dismount(vid, flush);
    leave();
    smlevel_3::chkpt->chkpt_mutex_release();

    return r;
}


rc_t
io_m::_dismount(vid_t vid, bool flush)
{
    FUNC(io_m::_dismount);
    DBG( << "_dismount(" << "vid=" << vid << ")");
    int i = _find(vid);
    if (i < 0) return RC(eBADVOL);

    W_COERCE(vol[i]->dismount(flush));

    if (log && !log->IsInRecovery())  {
        logrec_t* logrec = new logrec_t;
        w_assert1(logrec);

        new (logrec) dismount_vol_log(_dev_name(vid), vid);
	logrec->fill_xct_attr(tid_t::null, GetLastMountLSN());
	lsn_t theLSN;
        W_COERCE( log->insert(*logrec, &theLSN) );
	DBG( << "dismount_vol_log(" << _dev_name(vid) << ", vid=" << vid << ") lsn=" << theLSN << " prevLSN=" << GetLastMountLSN());;
	SetLastMountLSN(theLSN);

        delete logrec;
    }

    delete vol[i];
    vol[i] = 0;
    
    --vol_cnt;
  
    _remove_last_extent_used(vid);

    CRASHTEST("io_m::_dismount.1");

    return RCOK;
}

/*********************************************************************
 *
 *  io_m::_get_volume_quota(vid, quota_KB, quota_used_KB)
 *
 *  Return the "capacity" of the volume and number of Kbytes "used"
 *  (allocated to extents)
 *
 *********************************************************************/
rc_t
io_m::_get_volume_quota(vid_t vid, smksize_t& quota_KB, smksize_t& quota_used_KB)
{
    int i = _find(vid);
    if (i < 0)  return RC(eBADVOL);

    uint4 used;
    W_DO( vol[i]->num_used_exts(used) );
    quota_used_KB = used*ext_sz*page_sz/1024;

    quota_KB = vol[i]->num_exts()*ext_sz*page_sz/1024;
    return RCOK;
}



/*********************************************************************
 *
 *  io_m::_check_disk(vid)
 *
 *  Check the volume "vid".
 *
 *********************************************************************/
rc_t
io_m::_check_disk(vid_t vid)
{
    int i = _find(vid);
    if (i < 0) return RC(eBADVOL);

    W_DO( vol[i]->check_disk() );

    return RCOK;
}


/*********************************************************************
 *
 *  io_m::_get_new_vid(vid)
 *
 *********************************************************************/
rc_t
io_m::_get_new_vid(vid_t& vid)
{
    for (vid = vid_t(1); vid != vid_t::null; vid.incr_local()) {
	int i = _find(vid);
	if (i < 0) return RCOK;;
    }
    return RC(eNVOL);
}


vid_t
io_m::get_vid(const lvid_t& lvid)
{
    enter();
    vid_t vid = _get_vid(lvid);
    leave();
    return vid;
}



/*********************************************************************
 *
 *  io_m::read_page(pid, buf)
 * 
 *  Read the page "pid" on disk into "buf".
 *
 *********************************************************************/
rc_t
io_m::read_page(const lpid_t& pid, page_s& buf)
{
    FUNC(io_m::read_page);

    /*
     *  NEVER acquire mutex to read page
     */

    if (_msec_disk_delay > 0) {
	W_IGNORE( me()->block(_msec_disk_delay, 
			      0, "io_m::read_page") );
    }

    w_assert3(! pid.vol().is_remote());

    int i = _find(pid.vol());
    if (i < 0) {
	return RC(eBADVOL);
    }
    DBG( << "reading page: " << pid );
    W_DO( vol[i]->read_page(pid.page, buf) );
    smlevel_0::stats.vol_reads++;
    buf.ntoh(pid.vol());

    /*  Verify that we read in the correct page.
     *
     *  w_assert3(buf.pid == pid);
     *
     *  NOTE: that the store ID may not be correct during redo-recovery
     *  in the case where a page has been deallocated and reused.
     *  This can arise because the page will have a new store ID.
     *  If the page LSN is 0 then the page is
     *  new and should have a page ID of 0.
     */
#ifdef DEBUG
    if (buf.lsn1 == lsn_t::null)  {
	w_assert3(buf.pid.page == 0);
    } else {
	w_assert3(buf.pid.page == pid.page && buf.pid.vol() == pid.vol());
    }
    DBG(<<"buf.pid.page=" << buf.pid.page << " buf.lsn1=" << buf.lsn1);
#endif /* DEBUG */
    
    return RCOK;
}



/*********************************************************************
 *
 *  io_m::write_many_pages(bufs, cnt)
 *
 *  Write "cnt" pages in "bufs" to disk.
 *
 *********************************************************************/
void 
io_m::write_many_pages(page_s** bufs, int cnt)
{
    // NEVER acquire monitor to write page
    vid_t vid = bufs[0]->pid.vol();
    w_assert3(! vid.is_remote());
    int i = _find(vid);
    w_assert1(i >= 0);

    if (_msec_disk_delay > 0) {
	W_IGNORE( me()->block(_msec_disk_delay, 0, 
			      "io_m::write_many_pages") );
    }

#ifdef DEBUG
    {
	for (int j = 1; j < cnt; j++) {
	    w_assert1(bufs[j]->pid.page - 1 == bufs[j-1]->pid.page);
	    w_assert1(bufs[j]->pid.vol() == vid);
	}
    }
#endif /*DEBUG*/

    W_COERCE( vol[i]->write_many_pages(bufs[0]->pid.page, bufs, cnt) );
	smlevel_0::stats.vol_writes ++;
	smlevel_0::stats.vol_blks_written += cnt;
}



/*********************************************************************
 *
 *  io_m::alloc_pages(stid, near, npages, pids[], forward_alloc, sd)
 *  io_m::_alloc_pages(stid, near, npages, pids[], forward_alloc, sd)
 *
 *  Allocates "npages" pages for store "stid" and return the page id
 *  allocated in "pids[]". If a "near" pid is given, efforts are made
 *  to allocate the pages near it. "Forward_alloc" indicates whether
 *  to start search for free extents from the last extent of "stid".
 *
 *  sd is pointer to store descriptor, in case caller has already
 *  got it cached.
 *
 *********************************************************************/
rc_t
io_m::alloc_pages(const stid_t& stid, const lpid_t& near,
		  int cnt, lpid_t pids[], bool forward_alloc,
		  sdesc_t*	sd
		  ) 
{
    enter();
    xct_log_switch_t toggle(ON);
    rc_t r = _alloc_pages(stid, near, cnt, pids, forward_alloc, sd);
    leave();
    return r;
}

rc_t
io_m::_alloc_pages(
    const stid_t& 	stid,
    const lpid_t& 	near,
    int 		npages,
    lpid_t 		pids[],
    bool 		forward_alloc,
    sdesc_t*		//sd  -- to be used later when more performance improvements	
	// are added?
    )
{
    FUNC(io_m::_alloc_pages);

    /*
     *  Find the volume of "stid"
     */
    vid_t vid = stid.vol;
    int i = _find(vid);
    if (i < 0)  return RC(eBADVOL);
    vol_t* v = vol[i];
    
    int 	count = 0;
    int 	allocated = 0;
    int 	remaining_in_ext = 0;
    extnum_t 	ext = 0;
    bool 	found, has_free = false;
    extnum_t 	prevExt = 0;

    extnum_t    last_ext_in_store=0;
    bool        is_last_ext_in_store=false;

    /*
     *  Set up near_pid
     */
    lpid_t near_pid = lpid_t::null;
    if (&near == &lpid_t::eof)  {
	rc_t rc = v->last_page(stid.store, near_pid);
	if (rc)  {
	    if (rc.err_num() != eEOF)  
	        return RC_AUGMENT(rc);
	}
    } else if (&near == &lpid_t::bof)  {
	rc_t rc = v->first_page(stid.store, near_pid);
	if (rc)  {
	    if (rc.err_num() != eEOF)
	        return RC_AUGMENT(rc);
	}
    } else {
	near_pid = near;
    }

    found = _lookup_last_extent_used(stid, prevExt, has_free);
    DBGTHRD(<<"lookup_last_extent_used for stid " <<stid
	<< "returned found=" << found
	<< " prevExt = " << prevExt
	<< " has_free= " << has_free);

    extnum_t near_ext = 0;
    if (near_pid) {
	w_assert1(near_pid.vol() == vid && near_pid.store() == stid.store);
	
	near_ext = v->pid2ext(near_pid);
	if (v->is_alloc_ext(near_ext))  {
	    // Moved up to top
	    // found = _lookup_last_extent_used(stid, ext, has_free);
	    DBGTHRD(
		<< " found " << found
		<<" near_ext " << near_ext 
		<<" got from near_pid: " << near_pid 
	    );
	    if(found && prevExt == near_ext && !has_free) {
	       // skip trying this one
	       ext = 0;
	     } else {
		W_DO(v->alloc_page_in_ext(near_ext, 100, stid.store,
				      npages - count, 
				      pids + count, allocated,
				      remaining_in_ext, is_last_ext_in_store));
		DBGTHRD(<<"ALLOCATED " << allocated
		    << " pages in " << near_ext
		    << " remaining: " << remaining_in_ext
		    << " is_last_ext_in_store " << stid.store
		    << " = " << is_last_ext_in_store
		    << " last_ext_in_store=" << last_ext_in_store
		    );
		count += allocated;
		ext = near_ext;
		if(is_last_ext_in_store)  last_ext_in_store = near_ext;
	    } 
	}
    }
    DBGTHRD(<<"count " << count
	    <<" npages " <<  npages
	    <<" ext " <<  ext 
	    <<" alloc " <<  allocated 
	    <<" remaining_in_ext " <<  remaining_in_ext 
	    << " last_ext_in_store=" << last_ext_in_store
	    );

    // Maintain ext pointing to last extent from which
    // we allocated a page, so that before we return,
    // we can adjust the information about last extent used.
    
    if (count < npages)  {
	/*
	 *  Follow down the chain of stid extents and allocate
	 */

	// moved up to top
	// found = _lookup_last_extent_used(stid, prevExt, has_free);

	if (found && has_free) {
	    w_assert3(prevExt > 0);
	    // there may be space in the last extent,
	    // assuming we didn't just allocate in it for the near hint

	    if (prevExt != near_ext) {
		W_DO(v->alloc_page_in_ext(prevExt, 100, stid.store,
					  npages - count, 
					  pids + count, allocated,
					  remaining_in_ext,
					  is_last_ext_in_store));
		DBGTHRD(<<"ALLOCATED " << allocated
		    << " pages in " << prevExt
		    << " remaining: " << remaining_in_ext
			    << " is_last_ext_in_store " << stid.store
			    << " = " << is_last_ext_in_store
			    << " last_ext_in_store=" << last_ext_in_store
		    );
		count += allocated;
		ext = prevExt;
		if(is_last_ext_in_store)  last_ext_in_store = prevExt;
	    }
	} else {
	    // !found:
	    // we don't have any cached hint.
	    // Circle through the store starting with the first extent
	    //
	    // found but !has_free:
	    // the last-used extent is full.  Circle through
	    // the store starting with last-used extent
	    //
	    extnum_t start = 0;
	    extnum_t e;

	    if(found) {
	       W_DO(v->next_ext(prevExt, start));
	       // If start is 0, we will skip the round-robin
	       // through the whole file, below.
	       DBGTHRD(<<" found  -- start with next extent:" << start);
	    } else { 
	       W_DO(v->first_ext(stid.store, start));
	       DBGTHRD(<<" start with first extent:" << start);
	    }

	    // this algorithm is linear in the # of extents in the store

	    for (e = start; e > 0  &&  count < npages; ) {

		if(e != near_ext) {
		    W_DO(v->alloc_page_in_ext(e, 100, stid.store, 
					npages - count, 
					pids + count, allocated,
					remaining_in_ext,
					is_last_ext_in_store));
		    DBGTHRD(<<"ALLOCATED " << allocated
			<< " pages in " << e
			<< " remaining: " << remaining_in_ext
			);
		    count += allocated;
		    ext = e;
		    if(is_last_ext_in_store) last_ext_in_store = e;
		}
	        W_DO(v->next_ext(prevExt=e, e));
	    }
	    DBGTHRD(<<"ext=" <<ext);
	    /*
	    // Warning: ext could be near_ext at this point here
	    // But this should not be the case unless
	    // count is still < npages 
	    */

	    // Ok, we've gone to the end -- is there something at the
	    // beginning that we have yet to inspect?

	    if (count < npages  && start != 0)  {
	       extnum_t new_start;
	       W_DO(v->first_ext(stid.store, new_start));
	       for (e = new_start; 
		    e > 0  &&  count < npages && e != start; )  {

		    if(e != near_ext) {
			W_DO(v->alloc_page_in_ext(e, 100, stid.store, 
					    npages - count, 
					    pids + count, allocated,
					    remaining_in_ext,
					    is_last_ext_in_store));
			DBGTHRD(<<"ALLOCATED " << allocated
			    << " pages in " << e
			    << " remaining: " << remaining_in_ext
			    << " is_last_ext_in_store " << stid.store
			    << " = " << is_last_ext_in_store
			    << " last_ext_in_store=" << last_ext_in_store
			    );
			count += allocated;
			ext = e;
			if(is_last_ext_in_store) last_ext_in_store = e;
		    }
		    W_DO(v->next_ext(prevExt = e, e));
		}
	    }
	    DBGTHRD(<<"ext=" <<ext);
	}	
	DBGTHRD(<<"count " << count
	    <<" npages " <<  npages
	    <<" ext " <<  ext 
	    <<" alloc " <<  allocated 
	    <<" remaining_in_ext " <<  remaining_in_ext 
	    <<" last_ext_in_store " <<  last_ext_in_store 
	    );

	// NB: it *is* possible that ext == start == near_ext
	// but if that's the case, we're never allocated enough
	// and we are going to change ext below ...

	if (count < npages)  {
	    /*
	     *  Allocate new extents
	     *  -- extent allocation is a top-level action because
	     *  -- after an extent is allocated, two transaction can 
	     *  -- consume its pages concurrently. 
	     */

	    /*
	     *  NOTE: should do a for loop and allocate from stack
	     */
	    int ext_needed = 0;
	    int eff;
	    extnum_t* exts = 0;

	    {

		/*
		 *  Calculate extents needed according to its fill factor.
		 */
		eff = v->fill_factor(stid.store);
		{
		    int ext_sz = v->ext_size() * eff / 100;
		    w_assert3(ext_sz > 0);
		    ext_needed = (npages - count - 1) / ext_sz + 1;
		}
		exts = new extnum_t[ext_needed];
		if (! exts)  W_FATAL(eOUTOFMEMORY);

		int found;
		rc_t rc;
		extnum_t next_new_ext = (
			forward_alloc ? ( last_ext_in_store ? last_ext_in_store : prevExt ) : 0);

		if (rc = v->find_free_exts(ext_needed, exts, found, next_new_ext)) {
		    if (rc.err_num() != eOUTOFSPACE) {
			DBG(<<"error: " << rc);
			delete [] exts;
			return RC_AUGMENT(rc);
		    }
		    if (found * v->ext_size() < npages - count)  {
			DBG(<<"error: " << rc);
			delete [] exts;
			return RC(eOUTOFSPACE);
		    }
		    eff = (npages - count) * 100 / (v->ext_size() * found);
		    ext_needed = found;
		}
		w_assert3(prevExt != 0);

		if(last_ext_in_store == 0) {
		    DBGTHRD(<<" Have to find the last extent in the store! " );
		    lpid_t last_pid;
		    bool   is_allocated=false;
		    // We need to pass in the boolean * argument if we want
		    // it not to return an error should the store contain
		    // nothing but an empty extent...
		    W_DO(v->last_page(stid.store, last_pid, &is_allocated));

		    last_ext_in_store = v->pid2ext(last_pid);
		}
		DBGTHRD(<<"Last extent in store " << stid.store << " is " << last_ext_in_store);
		W_DO( v->alloc_exts(stid.store, last_ext_in_store, ext_needed, exts) );

	    }

	    /*
	     *  allocate pages in the new extents
	     */
	    for (i = 0; i < ext_needed && count < npages; i++)  {
		w_assert3(v->is_alloc_ext(exts[i]));
		if (i == ext_needed - 1) eff = 100;
		W_COERCE(
		    v->alloc_page_in_ext(
			exts[i], eff,
			stid.store, npages - count,
			pids + count, allocated,
			remaining_in_ext,
			is_last_ext_in_store));
		DBGTHRD(<<"ALLOCATED " << allocated
		    << " pages in " << exts[i]
		    << " remaining: " << remaining_in_ext
		    );
		    
		count += allocated;
		ext = exts[i];
	    }
	    w_assert3(is_last_ext_in_store);
	    if(exts) delete[] exts;
	}
    }
    DBGTHRD(<<"count " << count
	    <<" npages " <<  npages
	    <<" ext " <<  ext 
	    <<" alloc " <<  allocated 
	    <<" remaining_in_ext " <<  remaining_in_ext 
	    );

// done:

    _adjust_last_extent_used(stid, ext, 
			allocated, remaining_in_ext);
    
    w_assert1(count == npages);

    DBGTHRD( << "allocated " << npages << " at pid: " << pids[0] );

    smlevel_0::stats.page_alloc_cnt += count;
    return RCOK;
}

rc_t
io_m::alloc_new_last_page(
    const stid_t& 	stid,
    const lpid_t& 	near,
    lpid_t& 		pids,
    sdesc_t		*sd)
{
    FUNC(io_m::_alloc_new_last_pages);
    bool is_last_ext_in_store = false;

    /*
     *  Find the volume of "stid"
     */
    vid_t vid = stid.vol;
    int i = _find(vid);
    if (i < 0)  return RC(eBADVOL);
    vol_t* v = vol[i];
    
    int 	allocated = 0;
    int 	remaining_in_ext = 0;
    extnum_t 	ext = 0;

    extnum_t near_ext = 0;
    bool 	has_free = true;

#ifdef DEBUG
    bool 	found;
    found = _lookup_last_extent_used(stid, near_ext, has_free);
    DBGTHRD(<<"near_ext=" << near_ext);

    if(!found) {
	lpid_t near_pid;
	rc_t rc = v->last_page(stid.store, near_pid);
	if (rc) {
	   if(rc.err_num() != eEOF) {
		return RC_AUGMENT(rc);
	   }
	   // == eof
	   rc = v->first_page(stid.store, near_pid);
	   if(rc) {
		return RC_AUGMENT(rc);
	   }
	} else {
	    w_assert3(near_pid.vol() == vid && near_pid.store() == stid.store);
	}
	w_assert3(near_pid == near);
	has_free  = true; // to our best knowledge...
    }
#endif
    near_ext = v->pid2ext(near);
    DBGTHRD(<<"near_ext=" << near_ext);

    // w_assert3(v->is_alloc_ext(near_ext))  ;

    DBGTHRD(<<"");

    if(has_free) {
	DBGTHRD(
	    <<" ext " << ext 
	);
	 {
	    W_DO(v->alloc_page_in_ext(near_ext, 100, stid.store,
				  1,
				  &pids, allocated,
				  remaining_in_ext, 
				  is_last_ext_in_store));
	    DBGTHRD(<<"ALLOCATED " << allocated
		<< " pages in " << near_ext
		<< " remaining: " << remaining_in_ext
		);
	    ext = near_ext;
	    w_assert3(is_last_ext_in_store);
	} 
    }

    DBGTHRD(<<" allocated=" << allocated);

    if (allocated == 0) {
	/*
	 *  Allocate new extent
	 *  -- extent allocation is a top-level action because
	 *  -- after an extent is allocated, two transaction can 
	 *  -- consume its pages concurrently. 
	 */

	int eff;
	extnum_t exts = 0;

	{

	    /*
	     *  Calculate extents needed according to its fill factor.
	     */
	    if(!sd) {
		eff = v->fill_factor(stid.store);
	    } else {
		eff = sd->sinfo().eff;
	    }

	    int found;
	    rc_t rc;
	    DBGTHRD(<<"");

	    if (rc = v->find_free_exts(1, &exts, found, near_ext)) {
		if (rc.err_num() != eOUTOFSPACE) {
		    return RC_AUGMENT(rc);
		}
		if (found * v->ext_size() < 1 )  {
		    return RC(eOUTOFSPACE);
		}
	    }
	    w_assert3(near_ext != 0);
	    w_assert3(exts != 0);
	    DBGTHRD(<<"");
	    W_DO( v->alloc_exts(stid.store, near_ext, 1, &exts) );

	}

	/*
	 *  allocate pages in the new extents
	 */
	{
	    DBGTHRD(<<"");
	    // w_assert3(v->is_alloc_ext(exts));
	    eff = 100;
	    W_COERCE(
		v->alloc_page_in_ext(
		    exts, eff,
		    stid.store, 1,
		    &pids, allocated,
		    remaining_in_ext, is_last_ext_in_store));
	    DBGTHRD(<<"ALLOCATED " << allocated
		<< " pages in " << exts
		<< " remaining: " << remaining_in_ext
		);
	    w_assert3(is_last_ext_in_store);
		
	}
	ext = exts;
    }
    DBGTHRD( <<" ext " <<  ext 
	    <<" alloc " <<  allocated 
	    <<" remaining_in_ext " <<  remaining_in_ext 
	    );

// done:

    _adjust_last_extent_used(stid, ext, 
			allocated, remaining_in_ext);
    
    DBGTHRD( << "allocated " << 1 << " at pid: " << pids );

    smlevel_0::stats.page_alloc_cnt ++;
    return RCOK;
}



/*********************************************************************
 *
 *  io_m::free_page(pid)
 *  io_m::_free_page(pid)
 *
 *  Free the page "pid".
 *
 *********************************************************************/
rc_t
io_m::free_page(const lpid_t& pid) 
{
    enter();
    xct_log_switch_t toggle(ON);
    rc_t r = _free_page(pid);
    leave();
    return r;
}


rc_t
io_m::_free_page(const lpid_t& pid)
{
    FUNC(io_m::_free_page)
    vid_t vid = pid.vol();
    int i = _find(vid);
    if (i < 0)  return RC(eBADVOL);
    w_assert3(vol[i]->vid() == pid.vol());


    extnum_t ext = vol[i]->pid2ext(pid);
    _adjust_last_extent_used(pid.stid(), ext, -1, -1);

    if ( ! vol[i]->is_valid_page(pid) )  return RC(eBADPID);
    if ( ! vol[i]->is_alloc_page(pid) )  return RC(eBADPID);

    /*
     *  NOTE: do not discard the page in the buffer
     */
    DBGTHRD(<<"freeing page in extent " << ext);
    W_DO( vol[i]->free_page(pid) );

    DBGTHRD( << "freed pid: " << pid );

    smlevel_0::stats.page_dealloc_cnt++;

    w_assert3(vol[i]->vid() == pid.vol());
    bool ext_was_freed;
    W_DO(vol[i]->free_extent(pid.store(), ext, ext_was_freed));
    if(ext_was_freed) {
	_remove_last_extent_used(pid.stid(), ext);
    }

    return RCOK;
}



/*********************************************************************
 *
 *  io_m::_is_valid_page(pid)
 *
 *  Return true if page "pid" is valid. False otherwise.
 *
 *********************************************************************/
bool 
io_m::_is_valid_page(const lpid_t& pid)
{
    int i = _find(pid.vol());
    if (i < 0) { return FALSE; }
    
    if ( ! vol[i]->is_valid_page(pid) )  {
	return FALSE;
    }
    
    return vol[i]->is_alloc_page(pid);
}


/*********************************************************************
 *
 *  io_m::_set_store_flags(stid, flags)
 *
 *  Set the store flag for "stid" to "flags".
 *
 *********************************************************************/
rc_t
io_m::_set_store_flags(const stid_t& stid, uint4_t flags)
{
    int i = _find(stid.vol);
    if (i < 0)   return RC(eBADVOL);
    W_DO( vol[i]->set_store_flags(stid.store, flags) );
    return RCOK;
}


/*********************************************************************
 *
 *  io_m::_get_store_flags(stid, flags)
 *
 *  Get the store flag for "stid" in "flags".
 *
 *********************************************************************/
rc_t
io_m::_get_store_flags(const stid_t& stid, uint4_t& flags)
{
    int i = _find(stid.vol);
    if (i < 0)   return RC(eBADVOL);
    W_DO( vol[i]->get_store_flags(stid.store, flags) );
    return RCOK;
}



/*********************************************************************
 *
 *  io_m::create_store(volid, eff, flags, stid, first_ext)
 *  io_m::_create_store(volid, eff, flags, stid, first_ext)
 *
 *  Create a store on "volid" and return its id in "stid". The store
 *  flag for the store is set to "flags". "First_ext" is a hint to
 *  vol_t to allocate the first extent for the store at "first_ext"
 *  if possible.
 *
 *********************************************************************/
rc_t
io_m::create_store(
    vid_t 	volid, 
    int 	eff, 
    uint4_t 	flags,
    stid_t& 	stid,
    extnum_t	first_ext)
{
    enter();
    xct_log_switch_t toggle(ON);
    rc_t r = _create_store(volid, eff, flags, stid, first_ext);
    leave();
    return r;
}

rc_t
io_m::_create_store(
    vid_t 	volid, 
    int 	eff, 
    uint4_t 	flags,
    stid_t& 	stid,
    extnum_t	first_ext)
{
    w_assert1(flags);

    int i = _find(volid);
    if (i < 0)   return RC(eBADVOL);
    vol_t* v = vol[i];

    /*
     *  Find a free store slot
     */
    stid.vol = volid;
    W_DO(v->find_free_store(stid.store));

    /*
     *  Find a free extent 
     */
    extnum_t ext;
    int found;
    W_DO(v->find_free_exts(1, &ext, found, first_ext));
    w_assert3(found == 1);
    
    if (v->ext_size() * eff / 100 == 0)
	eff = 99 / v->ext_size() + 1;
    
    /*
     *  Allocate the store
     */
    W_DO( v->alloc_store(stid.store, eff, flags) );
    W_DO( v->alloc_exts(stid.store, 0, 1, &ext) );

    return RCOK;
}



/*********************************************************************
 * 
 *  io_m::destroy_store(stid)
 *  io_m::_destroy_store(stid)
 *
 *  Destroy the store "stid".
 *
 *********************************************************************/
rc_t
io_m::destroy_store(const stid_t& stid) 
{
    enter();
    xct_log_switch_t toggle(ON);
    rc_t r = _destroy_store(stid);
    leave();
    return r;
}


rc_t
io_m::_destroy_store(const stid_t& stid)
{
    _remove_last_extent_used(stid);

    vid_t volid = stid.vol;
    int i = _find(volid);
    if (i < 0) return RC(eBADVOL);
    vol_t* v = vol[i];
    
    if ( ! v->is_valid_store(stid.store) )  return RC(eBADSTID);
    
    /*
     *  NOTE: do not discard the pages of the file in the buffer
     */
    extnum_t tmp;
    W_DO( v->first_ext(stid.store, tmp));
    if(tmp) {
	/*
	 * Originally, we had the assumption that there
	 * is always an extent with a store, but there
	 * is a gap between the allocation of the store
	 * and the allocation of the first extent, during
	 * which a crash could leave the store with 0 extents.
	 * Recovering from that case requires this conditional
	 * statement if(tmp)...
	 */
	W_DO( v->free_exts(tmp));
    }

    W_DO( v->free_store(stid.store) );

    return RCOK;
}



/*********************************************************************
 *
 *  io_m::_is_valid_store(stid)
 *
 *  Return true if store "stid" is valid. False otherwise.
 *
 *********************************************************************/
bool
io_m::_is_valid_store(const stid_t& stid)
{
    int i = _find(stid.vol);
    if (i < 0) {
	return FALSE;
    }
    
    if ( ! vol[i]->is_valid_store(stid.store) )   {
	return FALSE;
    }
    
    return vol[i]->is_alloc_store(stid.store);
}



/*********************************************************************
 *
 *  io_m::_first_page(stid, pid, allocated, lock)
 *
 *  Find the first page of store "stid" and return it in "pid".
 *  If "allocated" is NULL, narrow search to allocated pages only.
 *  Otherwise, return the allocation status of the page in "allocated".
 *
 *********************************************************************/
rc_t
io_m::_first_page(
    const stid_t&	stid, 
    lpid_t&		pid, 
    bool*		allocated,
    bool		lock)
{
    if (stid.vol.is_remote()) W_FATAL(eBADSTID);

    int i = _find(stid.vol);
    if (i < 0)   return RC(eBADVOL);
    
    W_DO( vol[i]->first_page(stid.store, pid, allocated) );

    if (lock)
	W_DO(lock_force(pid, IS, t_long, WAIT_IMMEDIATE));

    return RCOK;
}



/*********************************************************************
 *
 *  io_m::last_page(stid, pid, allocated, lock)
 *
 *  Find the last page of store "stid" and return it in "pid".
 *  If "allocated" is NULL, narrow search to allocated pages only.
 *  Otherwise, return the allocation status of the page in "allocated".
 *
 *********************************************************************/
rc_t
io_m::_last_page(
    const stid_t&	stid, 
    lpid_t&		pid, 
    bool*		allocated,
    bool		lock)
{
    if (stid.vol.is_remote()) W_FATAL(eBADSTID);

    int i = _find(stid.vol);
    if (i < 0)   return RC(eBADVOL);
    
    W_DO( vol[i]->last_page(stid.store, pid, allocated) );

    if (lock)
	W_DO(lock_force(pid, IS, t_long, WAIT_IMMEDIATE));

    return RCOK;
}


/*********************************************************************
 * 
 *  io_m::_next_page(pid, allocated, lock)
 *
 *  Get the next page of "pid". 
 *  If "allocated" is NULL, narrow search to allocated pages only.
 *  Otherwise, return the allocation status of the page in "allocated".
 *
 *********************************************************************/
rc_t io_m::_next_page(
    lpid_t&		pid, 
    bool*		allocated,
    bool		lock)
{
    if (pid.is_remote()) W_FATAL(eBADPID);

    int i = _find(pid.vol());
    if (i < 0)  return RC(eBADVOL);

    W_DO( vol[i]->next_page(pid, allocated));

    if (lock)
	W_DO(lock_force(pid, IS, t_long, WAIT_IMMEDIATE));

    return RCOK;
}



/*********************************************************************
 *
 *, extnum  io_m::_get_du_statistics()         DU DF
 *
 *********************************************************************/
rc_t io_m::_get_du_statistics(vid_t vid, volume_hdr_stats_t& stats, bool audit)
{
    int i = _find(vid);
    if (i < 0)  return RC(eBADVOL);
    W_DO( vol[i]->get_du_statistics(stats, audit) );

    return RCOK;
}


void io_m::_remove_last_extent_used(const stid_t& stid)
{
    FUNC(io_m::_remove_last_extent_used);
    extnum_struct_t* hit = _last_extent_used->find(stid);
    if (hit) {
	_last_extent_used->remove(hit);
	w_assert3(hit == NULL);
	_last_extent_used->release_mutex();
    }
    w_assert3( _last_extent_used->find(stid) == 0);
}

void io_m::_remove_last_extent_used(const stid_t& stid, extnum_t ext)
{
    FUNC(io_m::_remove_last_extent_used);
    extnum_struct_t* hit = _last_extent_used->find(stid);
    if (hit && hit->ext == ext) {
	_last_extent_used->remove(hit);
	w_assert3(hit == NULL);
	_last_extent_used->release_mutex();
    }
}

// remove all stores belonging to volume vid 
void io_m::_remove_last_extent_used(const vid_t& vid)
{
    FUNC(io_m::_remove_last_extent_used);
    last_extent_used_i iter(*_last_extent_used);
    for (iter.next(); iter.curr(); iter.next())  {
	if ( iter.curr_key()->vol == vid) {
	    iter.discard_curr();
	}
    }

}

// clear entire cache
void io_m::_clear_last_extent_used()
{
    FUNC(io_m::_clear_last_extent_used);
    last_extent_used_i iter(*_last_extent_used);
    for (iter.next(); iter.curr(); iter.next())  {
	iter.discard_curr();
    }
}

bool
io_m::_lookup_last_extent_used(const stid_t& stid,
	extnum_t& result,
	bool&     has_free_pages)
{
    FUNC(io_m::_lookup_last_extent_used);
    extnum_struct_t* hit = _last_extent_used->find(stid);
    DBGTHRD(<<"store " << stid.store << " hit=" << hit );
    // Consider store 0 not to count
    if (hit) {
	if(hit->ext>0) {
	    DBGTHRD(<<" hit with extent " << hit->ext
		<< " has_free_pages " << hit->has_free_pages);
	    result = hit->ext;
	    has_free_pages = hit->has_free_pages;
	    smlevel_0::stats.ext_lookup_hits++;
	    _last_extent_used->release_mutex();
	    return true;
	}
	_last_extent_used->release_mutex();
    }
    DBGTHRD(<<" miss " );
    smlevel_0::stats.ext_lookup_misses++;
    return false; // miss
}

/*
 * Adjust the cached info for the extent
 */
void 
io_m::_adjust_last_extent_used(
    const stid_t& stid,
    extnum_t ext, int 
#ifdef DEBUG
    allocated
#endif
    , 
    int remaining
) 
{
    FUNC(io_m::_adjust_last_extent_used);
    bool found;
    bool is_new;
    extnum_struct_t* hit = _last_extent_used->grab(stid,found,is_new);

    DBGTHRD(<<" store " << stid
	<< " ext " << ext
	<< " allocated " << allocated
	<< " remaining " << remaining);

    DBGTHRD(<< " hit=" << hit << " found=" << found
	<< " is_new=" << is_new);

    w_assert3(hit!=0);
    hit->ext = ext;

   if(remaining == 0) {
       // remove this extent from the cached info
       // since it's empty
       // NB: nope -don't remove it
       DBGTHRD(<< "used last free page for ext " << ext << " old:" << hit->ext);
       hit->has_free_pages = false;
    } else {
       // allocated > 0 means
       // we just allocated a page, and the
       // remaining value is legit
       // 
       // allocated < 0 means we deallocated
       // a page in this extent
       w_assert3(remaining > 0 || allocated < 0);

       hit->has_free_pages = true;
       DBGTHRD(<< "alloced or dealloced, cached ext, has free pages " );
    }
    _last_extent_used->release_mutex();
}

/*
 * For use by rollback and recovery ONLY:
 *
 * io_m::recover_extent(bool alloc, stid_t& stid, 
 *          int cnt, ext_log_info_t* e, bool f)
 * if alloc == true, we're re-allocating a list of extents
 * if alloc == false, we're re-freeing a list of extents
 * if f == true, it means we'll force the deallocation, even 
 *	if there are bits set
 *  in the extent (for deallocating whole files)
 *
 */ 
rc_t
io_m::recover_extent(bool alloc, stid_t& stid, int cnt, 
	ext_log_info_t* e, bool force)
{
    FUNC(io_m::recover_extent)
    DBGTHRD(<<"recover_extent store " << stid
	<< " count=" << cnt);

    vid_t vid = stid.vol;
    int i = _find(vid);
    if (i < 0)  return RC(eBADVOL);

    vol_t *v = vol[i];
    w_assert3(v->vid() == stid.vol);
    w_assert3(v->is_valid_store(stid.store));
    w_assert3(v->is_alloc_store(stid.store) || (force && !alloc));

    if(alloc) {
	// 2nd arg=0 and last arg == true
	// means we don't have the prev extent info--
	// We let the function alloc_exts() find it.
#ifdef DEBUG
	for(i = 0;  i< cnt; i++) {
	    DBGTHRD(<<"doing logical allocation of extent " << e[i].ext
		<< " prv " << e[i].prev
		<< "/nxt " << e[i].next
		);
	    w_assert3(v->is_valid_ext(e[i].ext));
	    w_assert3(!v->is_alloc_ext(e[i].ext));
	}
#endif
	if( me()->xct() == 0) {
	    // we're in redo phase
	    w_assert3(in_recovery==true);

	    // make a list of extents from the logging info
	    // so that we can just call the normal routine
	    extnum_t *l = new extnum_t[cnt];
	    for(i = 0;  i< cnt; i++) {
	       l[i] = e[i].ext;
	    }
	    W_COERCE(v->alloc_exts(stid.store, 0, cnt, l));
	    delete[] l;
	} else {
	    w_assert3( me()->xct() != 0 );
	    w_assert3(
		(me()->xct()->state() == xct_t::xct_aborting) 
		||
		(smlevel_0::in_recovery && (me()->xct()->state() == xct_t::xct_active))
		);
	    W_COERCE(v->alloc_exts_in_order(stid.store, cnt, e));
	}
    } else {
	bool ext_was_freed;
	extnum_t ext;
	for(i = 0;  i< cnt; i++) {
	    ext = e[i].ext;
	    DBGTHRD(<<"doing logical deallocation of extent " << "ext " << ext
		<< " nxt " << e[i].next
		<< " prev " << e[i].prev
		<< " valid: " << v->is_valid_ext(ext)
		<< " allocated: " << v->is_alloc_ext(ext)
		);
	    w_assert3(v->is_valid_ext(ext));
	    w_assert3(v->is_alloc_ext(ext));
	    if(!force) {
		W_COERCE(v->free_extent(stid.store, ext, ext_was_freed));
		w_assert3(v->is_valid_ext(ext));
		if(ext_was_freed) {
		    w_assert3(! v->is_alloc_ext(ext));
		}
		// would not be freed if it's the first in the store
		// w_assert1(ext_was_freed);
	    }
	}
	if(force) {
	    extnum_t tmp;
	    W_DO( v->first_ext(stid.store, tmp));
	    w_assert3(tmp>0);
	    W_DO( v->free_exts(tmp));
	}
    }

    // in some cases not necessary, since this is recovery/ rollback...
    // but it could be a partial rollback, so we just blow away
    // the cached info:
    _remove_last_extent_used(stid);

    return RCOK;
}

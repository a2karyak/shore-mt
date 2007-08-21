/*<std-header orig-src='shore'>

 $Id: sm_io.cpp,v 1.35 2007/08/21 19:50:43 nhall Exp $

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

#define SM_SOURCE
#define IO_C

#ifdef __GNUG__
#pragma implementation
#endif

#include "hash_lru.h"
#include "sm_int_2.h"
#include "chkpt_serial.h"
#include "sm_du_stats.h"
#include "auto_release.h"
#include "device.h"
#include "lock.h"
#include "xct.h"
#include "logrec.h"
#include "logdef_gen.cpp"
#include "crash.h"
#include "vol.h"

#include <new>

#include "sm_vtable_enum.h"
#include "vtable_info.h"

class extnum_struct_t {
public:
    NORET extnum_struct_t() : ext(0), nresvd(0) {}
    NORET ~extnum_struct_t() {}

    extnum_t 	ext; // last extent used for the store

    int 	nresvd; // hint about free but reserved
			// pages remaining in all the extents
			// of the store
};

//typedef hash_lru_t<extnum_t, stid_t> extent_info_cache_t;
//typedef hash_lru_i<extnum_t, stid_t> extent_info_cache_i;
#define extent_info_cache_t hash_lru_t<extnum_struct_t, stid_t>
#define extent_info_cache_i hash_lru_i<extnum_struct_t, stid_t>

#ifdef EXPLICIT_TEMPLATE
template class extent_info_cache_t;
template class extent_info_cache_i;
//typedef hash_lru_entry_t<extnum_t, stid_t> extent_info_cache_entry_t;
#define extent_info_cache_entry_t hash_lru_entry_t<extnum_struct_t, stid_t>
template class extent_info_cache_entry_t;
template class w_list_t<extent_info_cache_entry_t>;
template class w_list_i<extent_info_cache_entry_t>;
template class w_hash_t<extent_info_cache_entry_t, stid_t>;
template class vtable_func<vol_t>;
#endif


/*********************************************************************
 *
 *  Class static variables
 *
 *	_msec_disk_delay	: delay in performing I/O (for debugging)
 *	_mutex			: make io_m a monitor
 *	vol_cnt			: # volumes mounted in vol[]
 *	vol[]			: array of volumes mounted
 *	_extent_info_cache	: see comments below
 *
 *********************************************************************/
uint4_t  		io_m::_msec_disk_delay = 0;
smutex_t 	io_m::_mutex("io_m");
int 		io_m::vol_cnt = 0;
vol_t* 		io_m::vol[io_m::max_vols] = { 0 };
lsn_t		io_m::_lastMountLSN = lsn_t::null;

static extent_info_cache_t* _extent_info_cache = 0;
/*********************************************************************
 *  _extent_info_cache: a cache of the last extent used for dealloc/
 *  	or allocation of pages within an extent.  This is used in order
 *   	to achieve a quasi-round-robin behavior when looking for an
 *    	allocated extent with free pages.   We keep a cached element
 * 	per store (key is store id).  This info is NOT tied to the
 *  	store descriptor cache because the whole point of this is for
 *      the cached info to survive transactions and to be shared between
 * 	transactions, whereas teh sdesc cache is a per-thread/xct thing.
 *********************************************************************/

int			
io_m::max_extents_on_page() 
{
    return vol_t::max_extents_on_page();
}

void			
io_m::enter()
{
    if(xct()) { xct()->start_crit(); }
    _mutex_acquire();
}

void			
io_m::leave(bool release)
{
    // flush the log before we leave, to free
    // the last page pinned-- necessary to avoid
    // latch-latch deadlock
    // this gets done
    // done in stop_crit:
    if(xct()) { xct()->stop_crit(); }
    if(xct()) { xct()->flush_logbuf(); }

    if(release) {
	_mutex_release();
    }
}

rc_t
io_m::lock_force(
    const lockid_t&	    n,
    lock_mode_t		    m,
    lock_duration_t         d,
    timeout_in_ms	    timeout, 
    page_p		    *page, // = 0  -- should be an extlink_p
    lock_mode_t*	    prev_mode, // = 0
    lock_mode_t*	    prev_pgmode, // = 0
    lockid_t**		    nameInLockHead // = 0
)
{
    /* Might not hold I/O mutex, but might hold vol-specfic
     * mutex instead.
     */
    bool held_mutex = _mutex.is_mine();


    /*
     * Why lock_force(), as opposed to simple lock()?
     * Lock_force forces the lock to be acquired in the core
     * lock table, even if the lock cache contains a parent
     * lock that subsumes the requested lock.
     * The I/O layer needs this to prevent re-allocation of
     * (de-allocated) pages before their time.
     * In other words, a lock serves to reserve a page.
     * When looking for a page to allocate, the lock manager
     * is queried to see if ANY lock is held on the page (even
     * by the querying transaction).
     */

   /* Try to acquire the lock w/o a timeout first */
   rc_t  rc = lm->lock_force(n, m, d, WAIT_IMMEDIATE,
	    prev_mode, prev_pgmode, nameInLockHead);
   if(rc && (rc.err_num() == eMAYBLOCK || rc.err_num() == eLOCKTIMEOUT)
	&& timeout != WAIT_IMMEDIATE) {
       w_assert3(me()->xct());
       if(page && page->is_fixed()) {
	  page->unfix();
       }

       // flush the log buf to unfix the page
       me()->xct()->flush_logbuf();
       if(held_mutex) {
	   _mutex_release();
       }

       rc = lm->lock_force(n, m, d, timeout,
		prev_mode, prev_pgmode, nameInLockHead);

       if(held_mutex) {
	// re-acquire
	_mutex_acquire();
       }
   }
   return rc;
}

/* friend -- called from vol.cpp */
rc_t
io_lock_force(
    const lockid_t&	    n,
    lock_mode_t		    m,
    lock_duration_t         d,
    timeout_in_ms	    timeout,
    lock_mode_t*	    prev_mode,
    lock_mode_t*	    prev_pgmode,
    lockid_t**		    nameInLockHead
    )
{
    return smlevel_0::io->lock_force(n,m,d,timeout, 0/*page*/, 
	prev_mode, prev_pgmode, nameInLockHead);
}

io_m::io_m()
{
    _lastMountLSN = lsn_t::null;

    // Initialize a 32 element free page cache (see sm_io.h for more info
    if (_extent_info_cache == 0) {
	if (!(_extent_info_cache = new extent_info_cache_t (32, "io_m free page cache"))) {
	    W_FATAL(eOUTOFMEMORY);
	}
	w_assert3(!_extent_info_cache->is_mine());
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
    if (_extent_info_cache) {
	w_assert3(!_extent_info_cache->is_mine());
	delete _extent_info_cache;
	_extent_info_cache = 0;
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
    _clear_extent_info_cache();
    // didn't grab volume mutex
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
    uint4_t i;
    for (i = 0; i < max_vols; i++)  {
	if (vol[i] && vol[i]->vid() == vid) break;
    }
    return (i >= max_vols) ? -1 : int(i);
}

inline vol_t * 
io_m::_find_and_grab(vid_t vid)
{
    if (!vid) {
	_mutex_release();
	DBG(<<"");
	return 0;
    }
    vol_t** v = &vol[0];
    uint4_t i;
    for (i = 0; i < max_vols; i++, v++)  {
	if (*v) {
	    if ((*v)->vid() == vid) break;
	}
    }
    if (i < max_vols) {
	w_assert1(*v);
	(*v)->acquire_mutex();
	_mutex_release();
	w_assert3(*v && (*v)->vid() == vid);
	return *v;
    } else {
	_mutex_release();
	return 0;
    }
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
    // didn't grab volume mutex
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
	    if(errlog) {
		errlog->clog << "warning: volume " << vol[i]->vid() << " still mounted\n"
		<< "         automatic dismount" << flushl;
	    }
	    W_DO(_dismount(vol[i]->vid(), flush));
	}
    }
    
    w_assert3(vol_cnt == 0);

    SET_STAT(vol_reads,0);
    SET_STAT(vol_writes,0);
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
	    if (_msec_disk_delay > 0)
		    me()->sleep(_msec_disk_delay, "io_m::sync_all_disks");
	    if (vol[i])
		    vol[i]->sync();
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

	/* XXX possible bit-loss */
    device_hdr_s dev_hdr(vhdr.format_version(), 
			 vhdr.device_quota_KB(), vhdr.lvid());
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
	uint4_t	  dummy2;
	W_DO(_get_volume_quota(_get_vid(lvid), quota_used_KB, dummy, dummy2));
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
    uint4_t i;
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
    char 	**dname,
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
    // didn't grab volume mutex
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
    // didn't grab volume mutex
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
    chkpt_serial_m::chkpt_mutex_acquire();
    enter();
    rc_t r = _mount(device, vid);
    // didn't grab volume mutex
    leave();
    chkpt_serial_m::chkpt_mutex_release();
    return r;
}


rc_t
io_m::_mount(const char* device, vid_t vid)
{
    FUNC(io_m::_mount);
    DBG( << "_mount(name=" << device << ", vid=" << vid << ")");
    uint4_t i;
    for (i = 0; i < max_vols && vol[i]; i++);
    if (i >= max_vols) return RC(eNVOL);

    vol_t* v = new vol_t;  // deleted on dismount
    if (! v) return RC(eOUTOFMEMORY);

    w_rc_t rc = v->mount(device, vid);
    if (rc)  {
	delete v;
	return RC_AUGMENT(rc);
    }

    int j = _find(vid);
    if (j >= 0)  {
	W_COERCE( v->dismount(false) );
	delete v;
	return RC(eALREADYMOUNTED);
    }
    
    ++vol_cnt;

    w_assert3(vol[i] == 0);
    vol[i] = v;

    if (log && smlevel_0::logging_enabled)  {
        logrec_t* logrec = new logrec_t; //deleted at end of scope
        w_assert1(logrec);

        new (logrec) mount_vol_log(device, vid);
	logrec->fill_xct_attr(tid_t::null, GetLastMountLSN());
	lsn_t theLSN;
        W_COERCE( log->insert(*logrec, &theLSN) );
	DBG( << "mount_vol_log(" << device << ", vid=" << vid << ") lsn=" << theLSN << " prevLSN=" << GetLastMountLSN());
	SetLastMountLSN(theLSN);

        delete logrec;
    }

    SSMTEST("io_m::_mount.1");

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

    chkpt_serial_m::chkpt_mutex_acquire();
    enter();
    rc_t r = _dismount(vid, flush);
    // didn't grab volume mutex
    leave();
    chkpt_serial_m::chkpt_mutex_release();

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

    if (log && smlevel_0::logging_enabled)  {
        logrec_t* logrec = new logrec_t; //deleted at end of scope
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
  
    _remove_extent_info_cache(vid);

    SSMTEST("io_m::_dismount.1");

    return RCOK;
}

/*********************************************************************
 *
 *  io_m::_get_volume_quota(vid, quota_KB, quota_used_KB, exts_used)
 *
 *  Return the "capacity" of the volume and number of Kbytes "used"
 *  (allocated to extents)
 *
 *********************************************************************/
rc_t
io_m::_get_volume_quota(vid_t vid, smksize_t& quota_KB, smksize_t& quota_used_KB, uint4_t &used)
{
    int i = _find(vid);
    if (i < 0)  return RC(eBADVOL);

    W_DO( vol[i]->num_used_exts(used) );
    quota_used_KB = used*ext_sz*(page_sz/1024);

    quota_KB = vol[i]->num_exts()*ext_sz*(page_sz/1024);
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
    vol_t *v = _find_and_grab(vid);
    if (!v)  return RC(eBADVOL);
    auto_release_t<smutex_t> release_on_return(v->vol_mutex());

    W_DO( v->check_disk() );

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
    // didn't grab volume mutex
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
     *  NO enter() *********************
     *  NEVER acquire mutex to read page
     */

    if (_msec_disk_delay > 0)
	    me()->sleep(_msec_disk_delay, "io_m::read_page");

    w_assert3(! pid.vol().is_remote());

    int i = _find(pid.vol());
    if (i < 0) {
	return RC(eBADVOL);
    }
    DBG( << "reading page: " << pid );

    W_DO( vol[i]->read_page(pid.page, buf) );

    INC_STAT(vol_reads);
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
#ifdef W_DEBUG
    if (buf.lsn1 == lsn_t::null)  {
		if(smlevel_1::log && smlevel_0::logging_enabled) {
			w_assert3(buf.pid.page == 0);
		}
    } else {
		w_assert3(buf.pid.page == pid.page && buf.pid.vol() == pid.vol());
    }
    DBG(<<"buf.pid.page=" << buf.pid.page << " buf.lsn1=" << buf.lsn1);
#endif /* W_DEBUG */
    
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

    if (_msec_disk_delay > 0)
	    me()->sleep(_msec_disk_delay, "io_m::write_many_pages");

#ifdef W_DEBUG
    {
	for (int j = 1; j < cnt; j++) {
	    w_assert1(bufs[j]->pid.page - 1 == bufs[j-1]->pid.page);
	    w_assert1(bufs[j]->pid.vol() == vid);
	}
    }
#endif /*W_DEBUG*/

    W_COERCE( vol[i]->write_many_pages(bufs[0]->pid.page, bufs, cnt) );
	INC_STAT(vol_writes);
	ADD_STAT(vol_blks_written, cnt);
}


/*********************************************************************
 *
 *  io_m::alloc_pages(stid, near, npages, pids[], 
 * 	may_realloc, desired_lock_mode, search_file)
 *  io_m::_alloc_pages(vol_t *, stid, near, npages, pids[], 
 * 	may_realloc, desired_lock_mode, search_file)
 *
 *  Allocates "npages" pages for store "stid" and return the page id
 *  allocated in "pids[]". If a "near" pid is given, efforts are made
 *  to allocate the pages near it. "Forward_alloc" indicates whether
 *  to start search for free extents from the last extent of "stid".
 *
 *  may_realloc: volume layer uses this to determine whether it
 *  should grab an instant(EX) or long-term(desired_lock_mode) 
 *  lock on the page
 *
 *  search_file: true means go ahead and look for more free pages
 *  in existing extents in the file.  False means if you can't
 *  get the # pages you want right away, just allocate more extents.
 *  Furthermore, all pages returned must be at the end of the file.
 *
 *********************************************************************/

rc_t
io_m::alloc_pages(
    const stid_t&	stid,
    const lpid_t&	near_p,
    int			npages,
    lpid_t		pids[],
    bool		may_realloc,
    lock_mode_t		desired_lock_mode,
    bool		search_file // true-->append-only semantics
    ) 
{
    enter();

    /*
     *  Find the volume of "stid"
     *
     *  In order to 
     *  increase parallelism over multiple disks, we
     *  grab a mutex on the volume in question, and
     *  release the I/O monitor's mutex.  
     */
    vid_t vid = stid.vol;

    vol_t *v = _find_and_grab(vid);
    if (!v)  return RC(eBADVOL);
    auto_release_t<smutex_t> release_on_return(v->vol_mutex());
    w_assert1(v->vol_mutex().is_mine());
    w_assert3(! _mutex.is_mine());

    rc_t r = 
	_alloc_pages(v, stid, near_p, npages, pids, 
	    may_realloc, desired_lock_mode, search_file);

    // _mutex is released by _alloc_pages after the volume-specfic
    // mutex is grabbed!
    leave(false);
    return r;
}

rc_t
io_m::_alloc_pages(
    vol_t * 		v,
    const stid_t&	stid,
    const lpid_t& 	near_p,
    int 		npages,
    lpid_t 		pids[],
    bool	 	may_realloc,  
    lock_mode_t		desired_lock_mode,
    bool		search_file // append-only semantics if true
)
{
    FUNC(io_m::_alloc_pages);

    /*
     *  Should we find that we need to allocate new 
     *  extents, we'll need to know the extnum_t of the
     *  last extent in the store, so that the volume layer
     *  can link the new extents to the end of the store.
     *  So we'll stash that information if we learn it 
     *  earlier.
     */
    extnum_t	last_ext_in_store = 0;
    bool	is_last_ext_in_store = false;

    /*
     *  Do we have a near hint?  If so use it to locate
     *  the pid of a page in an extent where we'll start
     *  looking for free space.  (NB: if we want append-only
     *  semantics, we need to pass in last page of store for
     *  the near hint.)
     *  The extent# is really what we're after.  
     *  We will start with an extent number and some idea 
     *  whether or not that extent
     *  contains reserved but unallocted pages.
     */

    extnum_t	extent=0;
    int  	nresvd=0; // # free pages thought to be in the store

    bool	not_worth_trying=false;
		// vol_t::last_page(...,&not_worth_trying)
		// sets not_worth_trying to true if 
		// last reserved page is already allocated

    lpid_t 	near_pid = lpid_t::null;

    if (&near_p == &lpid_t::eof)  {
	W_DO(v->last_page(stid.store, near_pid, &not_worth_trying));
	w_assert3(near_pid.page);
	is_last_ext_in_store = true;
    } else if (&near_p == &lpid_t::bof)  {
	W_DO(v->first_page(stid.store, near_pid, &not_worth_trying));
	w_assert3(near_pid.page);
    } else if (near_p.page) {
	near_pid = near_p;
    } 

    DBGTHRD(<<"near_pid = " << near_pid);

    /*
     * Got a starting page yet? If not, try last extent used (cached)
     * 
     *  NB: this might not be a good idea because if the
     *  last extent used cached by another tx, there might
     *  be lock conflicts
     */
    if(near_pid.page) {
	/* See if the near_page given is legit */
	DBGTHRD(<<"given near_pid " << near_pid);
	extent = v->pid2ext(near_pid);
    	if( extent &&  !v->is_alloc_ext_of(extent, stid.store) ) {
	    // Not valid
	    near_pid = lpid_t::null;
	}
	nresvd=1; // Assume that the near page's extent isn't empty
    }
    if(!near_pid.page) {
	DBGTHRD(<<"no valid near_pid");
	bool found = _lookup_extent_info_cache(stid, extent, nresvd);
	if(found) {
	    w_assert1(extent != 0);
	    if( !v->is_alloc_ext_of(extent, stid.store)) {
		 found = false;
	    }
	}
	if( 
	    (!found) // there wasn't a cached last-extent-used
	||  (nresvd==0) // was cached but no free pages in the store
	    ) {
	    // don't waste time looking in this extent
	    // get last page of store: there had better be one 
	    // not_worth_trying will be returned true 
	    // if this page is already allocated 
	    W_DO(v->last_page(stid.store, near_pid, &not_worth_trying));
	    DBGTHRD(<<"got last page = " << near_pid);
	    w_assert1(near_pid.page); 

	    is_last_ext_in_store = true;
	} 
	/* 
	 * else { found && nresvd>0: have an extent ctng at least 1 free page
	 *   w_assert3(extent);
	 *   w_assert3(near_pid.page == 0);
	 *   w_assert3(nresvd > 0);
	 *  }
	 */
	nresvd = not_worth_trying? 0: 1; //for starters
	if(near_pid.page) {
	    extent = v->pid2ext(near_pid);
	} else {
	    w_assert1(extent != 0);
	}
    	w_assert1(v->is_alloc_ext_of(extent, stid.store));
    } 

    if(is_last_ext_in_store) {
	last_ext_in_store = extent;
    }

    w_assert3(v->is_alloc_ext(extent));
    w_assert1(v->is_alloc_ext_of(extent, stid.store));

    /*
     * get extent fill factor for store, so that we can use
     * it when allocating pages from extents
     */
    int eff;
    eff = v->fill_factor(stid.store);
    if(eff < 100) {
	/* 
	 * Not implemented because 1) it doesn't work
	 * with our nresvd hint, 2) nowhere does anything
	 * pass in anything but 100 through the ss_m interface
	 */
	return RC(fcNOTIMPLEMENTED);
    }

    int count = 0; // number of pages already allocated
    {
	/*
	 * Allocate from existing extents in the store
	 * as long as we reasonably expect to find some
	 * free pages there.
	 *
	 * nresvd is a hint now -- in theory,
	 * other txs could be working on this store, but
	 * since we've got the volume mutex, no other tx
	 * should be able to update it until we're done
	 */
	    
	extnum_t starting_point = extent;
	DBGTHRD(<<"starting point=" << extent
		<< " nresvd=" << nresvd
	);

	while ( (nresvd>0) && (count < npages) )  {

	    /*
	    // this part of the algorithm is linear in the # of extents 
	    // in the store, until nresvd hits 0
	    // You can see how many times we went through here
	    // by looking at the page_alloc_in_ext stat 
	    */

	    int  remaining_in_ext=0;
	    int  allocated=0;

	    is_last_ext_in_store = false;

	    W_DO(v->alloc_page_in_ext(
			!search_file, // if !search_file, is append_only
			extent, 
			eff, // fill factor
			stid.store,
			npages - count, 
			pids + count, 
			allocated, // returns how many allocated
			remaining_in_ext,  // return
			is_last_ext_in_store,  // return
			may_realloc,
			desired_lock_mode));

	    if(is_last_ext_in_store) {
		last_ext_in_store = extent;
	    }

	    DBGTHRD(<<" ALLOCATED " << allocated
		    <<" requested=" << npages-count
		    << " pages in " << extent
		    << " remaining: " << remaining_in_ext
		    << " nresvd= " << nresvd
		    << " is_last_ext_in_store " << stid.store
			<< " = " << is_last_ext_in_store
		    );

	    /* 
	     * update hint about whether it's worth trying 
	     * more extents in this store:
	     */
	    count += allocated;
	    nresvd -= allocated; 

#ifdef W_DEBUG
	    // These assertions aren't right, because nresvd
	    // is only a hint; could be too small. After all,
	    // we figure if it's not hopeless, nresvd is 1 above,
	    // but it could have been 8!
	    // w_assert3(remaining_in_ext <= nresvd);
	    if(remaining_in_ext > nresvd) {
		nresvd = remaining_in_ext;
	    }
	    w_assert3(nresvd >= 0);
	    w_assert3(remaining_in_ext <= nresvd);
#endif /* W_DEBUG */

	    if(count < npages) { 
	       // Need to try more extents
	       extnum_t try_next=0;

	       // is_last_ext_in_store is 
	       // reliable now; it refers to "extent",
	       // not to "last_ext_in_store", which is
	       // 0 or is correct.

	       if(search_file) {
		   if(is_last_ext_in_store) {
		       W_DO(v->first_ext(stid.store, try_next));
		   } else {
		       W_DO(v->next_ext(extent, try_next));
		   }
		} else {
	           // Break out of loop
		   nresvd = 0;
		}

	       // Have we come full-circle?
	       if(try_next == starting_point) {
		    // If our hints are right, we should never
		    // get here, or we should get here exactly 
		    // when nresvd goes to 0 
		    // NB: That's a big "if" because we don't
		    // init it to the correct amount when we mount
		    // the volume or open the (existing) store.
	           // Break out of loop
		   nresvd = 0;
	       } else {
		    extent = try_next;
		    INC_TSTAT(extent_lsearch);
	       }
	    } /* if (count < npages) */
	} /* while (nresvd > 0 && count < npages) */
    }


    /*
     * OK, we've allocated all we can expect to allocate
     * from existing pages.  If we still need more,
     * we'll allocate new extents to the store
     */
    
    if (count < npages)  {
	/*
	 *  Allocate new extents & pages in them
	 */

	int allocated =0;
	int needed = 0;
	{   /* Step 1:
	     *  Calculate #extents needed 
	     */
	    int ext_sz = v->ext_size() * eff / 100;
	    w_assert3(ext_sz > 0);
	    needed = (npages - count - 1) / ext_sz + 1;
	} /* step 1 */

	/* Step 2:
         * create an auto-deleted list of extent nums
	 * for the volume layer to fill in when it allocates
	 */
	extnum_t* extentlist = 0;
	extentlist = new extnum_t[needed]; // auto-del
	if (! extentlist)  W_FATAL(eOUTOFMEMORY);
	w_auto_delete_array_t<extnum_t> autodel(extentlist);

	{   /* Step 3:
	     * Locate some free extents
	     */
	    int numfound=0;
	    rc_t rc;

	    //Don't bother giving hint about where to start looking--
	    //let the volume layer deal with that, because we have
	    //no clue
	    rc = v->find_free_exts(needed, extentlist, numfound, 0);
#ifdef W_DEBUG
	    if (shpid_t(numfound * v->ext_size()) < shpid_t(npages - count))  {
		//NB: shouldn't get here unless we got an OUTOFSPACE
		// error from the volume layer
		w_assert3(rc && rc.err_num() == eOUTOFSPACE);
		rc.reset();
	    }
#endif /* W_DEBUG */
	    if (rc) {
		/*
		 * If the fill factor is < 100 and we ran
		 * out of space, we need to do something 
		 * else -- go back and revisit the extents
		 * already allocated, but that's not supported
		 * But then, neither is any fill factor other
		 * than 100.
		 */
		if((eff < 100) && (rc.err_num() == eOUTOFSPACE)) {
		    W_FATAL(fcNOTIMPLEMENTED);
		}
		cerr << "Ran out of space on disk at line "
			<< __LINE__ << " of file " << __FILE__ 
		        << endl;
		return RC_AUGMENT(rc);
	    }

	    w_assert1(numfound == needed);

	} /* step 3 -- locate free extents */

	{   /* Step 4:
	     * Allocate the free extents we found.
	     * In order to allocate them, we need to know the
	     * extnum_t of the last extent in the store.
	     * If last_ext_in_store is non-zero, it's credible.
	     */

	    if( last_ext_in_store==0 ) {
		// Get it
		W_DO(v->last_page(stid.store, near_pid));
		last_ext_in_store = v->pid2ext(near_pid);
		w_assert1( last_ext_in_store != 0);
		w_assert3(v->is_alloc_ext(last_ext_in_store));
	    }
	    W_DO(v->alloc_exts(stid.store,last_ext_in_store,needed,extentlist));
#ifdef W_DEBUG
	    for(int kk = 0; kk< needed; kk++) {
		w_assert3(v->is_alloc_ext(extentlist[kk]));
		w_assert3(v->is_alloc_ext_of(extentlist[kk], stid.store));
	    }
#endif

	    nresvd += (needed * v->ext_size());

	} /* step 4 -- allocate the free extents  */

	{   /* Step 5:
	     *  Allocate pages in the new extents
	     */
	    int remaining_in_ext=0;
	    int i=0;
	    for (i = 0; i < needed && count < npages; i++)  {
		w_assert3(v->is_alloc_ext(extentlist[i]));
		w_assert3(v->is_alloc_ext_of(extentlist[i], stid.store));

		if (i == needed - 1) eff = 100;

		W_COERCE(
		    v->alloc_page_in_ext(
			!search_file, // if !search_file, is append_only
			extentlist[i], eff,
			stid.store, npages - count,
			pids + count, allocated,
			remaining_in_ext,
			is_last_ext_in_store, 
			may_realloc, desired_lock_mode
			));

		DBGTHRD(<<"ALLOCATED " << allocated
		    << " pages in " << extentlist[i]
		    << " remaining: " << remaining_in_ext
		    );
		    
		count += allocated;

		// for caching info about extents: 
		nresvd -= allocated;
		extent = extentlist[i];
	    }
	    w_assert3(is_last_ext_in_store);
	    w_assert3(count == npages);
	    w_assert3(i == needed);
	    w_assert3(nresvd == remaining_in_ext);

	} /* step 5  -- allocate pages in the new extents */
    } /* if count < npages */

// done:

    w_assert3(v->is_alloc_ext(extent));
    _insert_extent_info_cache(stid, extent, nresvd);
    w_assert1(count == npages);
    DBGTHRD( << "allocated " << npages);
    ADD_TSTAT(page_alloc_cnt, count);
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
    rc_t r = _free_page(pid);
    // exchanges vol mutex for i/o mutex
    leave(false);
    return r;
}


rc_t
io_m::_free_page(const lpid_t& pid)
{
    FUNC(io_m::_free_page)
    vid_t vid = pid.vol();

    vol_t *v = _find_and_grab(vid);
    if (!v)  return RC(eBADVOL);
    {
	auto_release_t<smutex_t> release_on_return(v->vol_mutex());
	w_assert1(v->vol_mutex().is_mine());

	w_assert3(v->vid() == pid.vol());

#ifdef W_TRACE
	extnum_t ext = v->pid2ext(pid);
#endif

	if ( ! v->is_valid_page(pid) ) {
	    DBGTHRD(<<" invalid pid is " << pid);
	    return RC(eBADPID);
	}
	if ( ! v->is_alloc_page(pid) ) {
	    DBGTHRD(<<" unallocated pid is " << pid);
	    return RC(eBADPID);
	}

	/*
	 *  NOTE: do not discard the page in the buffer
	 */
	DBGTHRD(<<"freeing page " << pid << " in extent " << ext);
	W_DO( v->free_page(pid) );

	DBGTHRD( << "freed pid: " << pid );

	INC_TSTAT(page_dealloc_cnt);
	_adjust_extent_info_cache(pid.stid(), 1);

	w_assert3(v->vid() == pid.vol());

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
    vol_t *v = _find_and_grab(pid.vol());
    if (!v)  return false;
    auto_release_t<smutex_t> release_on_return(v->vol_mutex());
    w_assert1(v->vol_mutex().is_mine());

    if ( ! v->is_valid_page(pid) )  {
	return false;
    }
    
    return v->is_alloc_page(pid);
}
/*********************************************************************
 *
 *  io_m::_is_valid_page(pid)
 *
 *  Return true if page "pid" is valid. False otherwise.
 *
 *********************************************************************/
bool 
io_m::_is_valid_page_of(const lpid_t& pid, snum_t s)
{
    vol_t *v = _find_and_grab(pid.vol());
    if (!v)  return false;
    auto_release_t<smutex_t> release_on_return(v->vol_mutex());
    w_assert1(v->vol_mutex().is_mine());

    if ( ! v->is_valid_page(pid) )  {
	return false;
    }
    
    return v->is_alloc_page_of(pid, s);
}


/*********************************************************************
 *
 *  io_m::_set_store_flags(stid, flags)
 *
 *  Set the store flag for "stid" to "flags".
 *
 *********************************************************************/
rc_t
io_m::_set_store_flags(const stid_t& stid, store_flag_t flags, bool sync_volume)
{
    vol_t *v = _find_and_grab(stid.vol);
    if (!v)  return RC(eBADVOL);
    auto_release_t<smutex_t> release_on_return(v->vol_mutex());
    w_assert1(v->vol_mutex().is_mine());
    W_DO( v->set_store_flags(stid.store, flags, sync_volume) );
    if ((flags & st_insert_file) && !smlevel_0::in_recovery())  {
	xct()->AddLoadStore(stid);
    }
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
io_m::_get_store_flags(const stid_t& stid, store_flag_t& flags)
{
    // v->get_store_flags grabs the mutex
    int i = _find(stid.vol);
    if (i < 0) return RC(eBADVOL);
    vol_t *v = vol[i];

    if (!v)  W_FATAL(eINTERNAL);
    W_DO( v->get_store_flags(stid.store, flags) );
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
    vid_t 			volid, 
    int 			eff, 
    store_flag_t		flags,
    stid_t& 			stid,
    extnum_t			first_ext,
    extnum_t			num_exts)
{
    enter();
    rc_t r = _create_store(volid, eff, flags, stid, first_ext, num_exts);
    // replaced io mutex with volume mutex
    leave(false);

    /* load and insert stores get converted to regular on commit */
    if (flags & st_load_file || flags & st_insert_file)  {
	xct()->AddLoadStore(stid);
    }

    return r;
}

rc_t
io_m::_create_store(
    vid_t 			volid, 
    int 			eff, 
    store_flag_t		flags,
    stid_t& 			stid,
    extnum_t			first_ext,
    extnum_t			num_extents)
{
    w_assert1(flags);

    vol_t *v = _find_and_grab(volid);
    if (!v)  return RC(eBADVOL);
    auto_release_t<smutex_t> release_on_return(v->vol_mutex());

    /*
     *  Find a free store slot
     */
    stid.vol = volid;
    W_DO(v->find_free_store(stid.store));

    rc_t	rc;

    extnum_t *ext =  0;
    if(num_extents>0) {
	ext = new extnum_t[num_extents]; // auto-del
	w_assert1(ext);
    }

    w_auto_delete_array_t<extnum_t> autodel(ext);

    if(num_extents>0) {
	/*
	 *  Find a free extent 
	 */
	int found;
	W_DO(v->find_free_exts(num_extents, ext, found, first_ext));
	w_assert3(found == 1);
	
	if (v->ext_size() * eff / 100 == 0)
	    eff = 99 / v->ext_size() + 1;
    }
    
    /*
     * load files are really temp files until commit
     *
     * really want to say 'flags = st_tmp', but must or this
     * in since there are assertions which need to know if
     * the file is a load_file
     */
    if (flags & st_load_file)  {
	flags = (store_flag_t)  (flags | st_tmp);
    }

    /*
     *  Allocate the store
     */
    W_DO( v->alloc_store(stid.store, eff, flags) );

    if(num_extents> 0) {
	W_DO( v->alloc_exts(stid.store, 0/*prev*/, 
		num_extents, ext) );
	int nresvd = int(num_extents * v->ext_size());
        w_assert3(v->is_alloc_ext(ext[0]));
	_insert_extent_info_cache(stid, ext[0], nresvd);
    }

    return rc;
}



/*********************************************************************
 * 
 *  io_m::destroy_store(stid, acquire_lock)
 *  io_m::_destroy_store(stid, acquire_lock)
 *
 *  Destroy the store "stid".  This routine now just marks the store
 *  for destruction.  The actual destruction is done at xct completion
 *  by io_m::free_store_after_xct below.
 *
 *  Acquire_lock defaults to true and set to false only by
 *  destroy_temps to destroy stores which might have a share lock on
 *  them by a prepared xct.
 *
 *********************************************************************/
rc_t
io_m::destroy_store(const stid_t& stid, bool acquire_lock) 
{
    enter();
    rc_t r = _destroy_store(stid, acquire_lock);
    // exchanges vol mutex for i/o mutex
    leave(false);
    return r;
}


rc_t
io_m::_destroy_store(const stid_t& stid, bool acquire_lock)
{
    _remove_extent_info_cache(stid);

    vid_t volid = stid.vol;

    vol_t *v = _find_and_grab(volid);
    if (!v)  return RC(eBADVOL);
    auto_release_t<smutex_t> release_on_return(v->vol_mutex());
    
    if (!v->is_valid_store(stid.store)) {
	DBG(<<"_destroy_store: BADSTID");
	return RC(eBADSTID);
    }
    
    W_DO( v->free_store(stid.store, acquire_lock) );

    return RCOK;
}



/*********************************************************************
 *
 *  io_m::free_store_after_xct(stid)
 *  io_m::_free_store_after_xct(stid)
 *
 *  free the store.  only called after a xct has completed or during
 *  recovery.
 *
 *********************************************************************/
rc_t
io_m::free_store_after_xct(const stid_t& stid)
{
    enter();
    rc_t r = _free_store_after_xct(stid);
    // exchanges I/O mutex for volume mutex
    leave(false);
    return r;
}

rc_t
io_m::_free_store_after_xct(const stid_t& stid)
{
    vid_t volid = stid.vol;

    vol_t *v = _find_and_grab(volid);
    if (!v)  return RC(eBADVOL);
    auto_release_t<smutex_t> release_on_return(v->vol_mutex());
    w_assert1(v->vol_mutex().is_mine());

    W_DO( v->free_store_after_xct(stid.store) );

    return RCOK;
}


/*********************************************************************
 *
 *  io_m::free_ext_after_xct(extid)
 *  io_m::_free_ext_after_xct(extid)
 *
 *  free the ext.  only called after a xct has completed.
 *
 *********************************************************************/
rc_t
io_m::free_ext_after_xct(const extid_t& extid)
{
    enter();
    rc_t r = _free_ext_after_xct(extid);
    // exchanges I/O mutex for volume mutex
    leave(false);
    return r;
}

rc_t
io_m::_free_ext_after_xct(const extid_t& extid)
{
    vid_t volid = extid.vol;
    vol_t *v = _find_and_grab(volid);
    if (!v)  return RC(eBADVOL);
    auto_release_t<smutex_t> release_on_return(v->vol_mutex());
    w_assert1(v->vol_mutex().is_mine());

    snum_t owner=0;
    W_DO( v->free_ext_after_xct(extid.ext, owner) );

    DBG(<<"_free_ext_after_xct " << extid );
    _remove_extent_info_cache(stid_t(volid,owner), extid.ext);

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
    vol_t *v = _find_and_grab(stid.vol);
    if (!v)  return false;
    
    auto_release_t<smutex_t> release_on_return(v->vol_mutex());
    w_assert1(v->vol_mutex().is_mine());
    if ( ! v->is_valid_store(stid.store) )   {
	return false;
    }
    
    return v->is_alloc_store(stid.store);
}


/*********************************************************************
 *
 *  io_m::_max_store_id_in_use(vid, snum)
 *
 *  Return in snum the maximum store id which is in use on volume vid.
 *
 *********************************************************************/
rc_t
io_m::_max_store_id_in_use(vid_t vid, snum_t& snum) 
{
    vol_t *v = _find_and_grab(vid);
    if (!v) return RC(eBADVOL);

    auto_release_t<smutex_t> release_on_return(v->vol_mutex());
    w_assert1(v->vol_mutex().is_mine());
    snum =  v->max_store_id_in_use();
    return RCOK;
}


/*********************************************************************
 *
 *  io_m::_get_volume_meta_stats(vid, volume_stats)
 *
 *  Returns in volume_stats the statistics calculated from the volumes
 *  meta information.
 *
 *********************************************************************/
rc_t
io_m::_get_volume_meta_stats(vid_t vid, SmVolumeMetaStats& volume_stats)
{
    vol_t *v = _find_and_grab(vid);
    if (!v) return RC(eBADVOL);

    auto_release_t<smutex_t> release_on_return(v->vol_mutex());
    w_assert1(v->vol_mutex().is_mine());
    W_DO( v->get_volume_meta_stats(volume_stats) );
    return RCOK;
}


/*********************************************************************
 *
 *  io_m::_get_file_meta_stats(vid, num_files, file_stats)
 *
 *  Returns the pages usage statistics from the stores listed in
 *  the file_stats structure on volume vid.  This routine traverses
 *  the extent list of each file, it can also have random seek
 *  behavior while traversing these..
 *
 *********************************************************************/
rc_t
io_m::_get_file_meta_stats(vid_t vid, uint4_t num_files, SmFileMetaStats* file_stats)
{
    vol_t *v = _find_and_grab(vid);
    if (!v) return RC(eBADVOL);

    auto_release_t<smutex_t> release_on_return(v->vol_mutex());
    w_assert1(v->vol_mutex().is_mine());
    W_DO( v->get_file_meta_stats(num_files, file_stats) );
    return RCOK;
}


/*********************************************************************
 *
 *  io_m::_get_file_meta_stats_batch(vid, max_store, mapping)
 *
 *  Returns the pages usage statistics from the stores which have
 *  a non-null in the mapping structure indixed by store number
 *  on volume vid.  This routine makes one pass in order of each
 *  extlink page.
 *
 *********************************************************************/
rc_t
io_m::_get_file_meta_stats_batch(vid_t vid, uint4_t max_store, SmStoreMetaStats** mapping)
{
    vol_t *v = _find_and_grab(vid);
    if (!v) return RC(eBADVOL);

    auto_release_t<smutex_t> release_on_return(v->vol_mutex());
    w_assert1(v->vol_mutex().is_mine());
    W_DO( v->get_file_meta_stats_batch(max_store, mapping) );
    return RCOK;
}

/*********************************************************************
 *
 *  io_m::get_store_meta_stats_batch(stid_t, stats)
 *
 *  Returns the pages usage statistics for the given store.
 *
 *********************************************************************/
rc_t
io_m::get_store_meta_stats(stid_t stid, SmStoreMetaStats& mapping)
{
    vol_t *v = _find_and_grab(stid.vol);
    if (!v) return RC(eBADVOL);

    auto_release_t<smutex_t> release_on_return(v->vol_mutex());
    w_assert1(v->vol_mutex().is_mine());
    W_DO( v->get_store_meta_stats(stid.store, mapping) );
    return RCOK;
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
    lock_mode_t		lock)
{
    if (stid.vol.is_remote()) W_FATAL(eBADSTID);

    vol_t *v = _find_and_grab(stid.vol);
    if (!v)  return RC(eBADVOL);
    auto_release_t<smutex_t> release_on_return(v->vol_mutex());
    w_assert1(v->vol_mutex().is_mine());

    W_DO( v->first_page(stid.store, pid, allocated) );

    if (lock != NL) {
	// Can't block on lock while holding mutex
	W_DO(lock_force(pid, lock, t_long, WAIT_IMMEDIATE));
    }

    return RCOK;
}



/*********************************************************************
 *
 *  io_m::_last_page(stid, pid, allocated, lock)
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
    lock_mode_t		desired_lock_mode
    )
{
    if (stid.vol.is_remote()) W_FATAL(eBADSTID);

    vol_t *v = _find_and_grab(stid.vol);
    if (!v)  return RC(eBADVOL);
    auto_release_t<smutex_t> release_on_return(v->vol_mutex());
    w_assert1(v->vol_mutex().is_mine());
    
    W_DO( v->last_page(stid.store, pid, allocated) );

    if (desired_lock_mode != NL) {
	// Can't block on lock while holding mutex
	W_DO(lock_force(pid, desired_lock_mode, t_long, WAIT_IMMEDIATE));
    }

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
    lock_mode_t		lock)
{
    if (pid.is_remote()) W_FATAL(eBADPID);

    vol_t *v = _find_and_grab(pid.vol());
    if (!v)  return RC(eBADVOL);
    auto_release_t<smutex_t> release_on_return(v->vol_mutex());
    w_assert1(v->vol_mutex().is_mine());

    W_DO( v->next_page(pid, allocated));

    if (lock != NL)
	W_DO(lock_force(pid, lock, t_long, WAIT_IMMEDIATE));

    return RCOK;
}

/*********************************************************************
 * 
 *  io_m::_next_page_with_space(pid, space_bucket_t needed, lock)
 *
 *  Get the next page of "pid" that is in bucket "needed" or higher
 *
 *********************************************************************/
rc_t io_m::_next_page_with_space(
    lpid_t&		pid, 
    space_bucket_t	needed,
    lock_mode_t		lock
    )
{
    if (pid.is_remote()) W_FATAL(eBADPID);

    vol_t *v = _find_and_grab(pid.vol());
    if (!v)  return RC(eBADVOL);
    auto_release_t<smutex_t> release_on_return(v->vol_mutex());
    w_assert1(v->vol_mutex().is_mine());

    W_DO( v->next_page_with_space(pid, needed));
    DBGTHRD(<<"next page with space is " << pid);

    if (lock != NL) {
	DBGTHRD(<<"locking pid " << pid);
	W_DO(lock_force(pid, lock, t_long, WAIT_IMMEDIATE));
    }

    return RCOK;
}


/*********************************************************************
 *
 *, extnum  io_m::_get_du_statistics()         DU DF
 *
 *********************************************************************/
rc_t io_m::_get_du_statistics(vid_t vid, volume_hdr_stats_t& stats, bool audit)
{
    vol_t *v = _find_and_grab(vid);
    if (!v)  return RC(eBADVOL);
    auto_release_t<smutex_t> release_on_return(v->vol_mutex());
    w_assert1(v->vol_mutex().is_mine());
    W_DO( v->get_du_statistics(stats, audit) );

    return RCOK;
}


void io_m::_remove_extent_info_cache(const stid_t& stid)
{
    FUNC(io_m::_remove_extent_info_cache);
    w_assert3(!_extent_info_cache->is_mine());
    const extnum_struct_t* hit = _extent_info_cache->find(stid);
    if (hit) {
	_extent_info_cache->remove(hit);
	w_assert3(hit == NULL);
	_extent_info_cache->release_mutex();
    }
    w_assert3( _extent_info_cache->find(stid) == 0);
    w_assert3(!_extent_info_cache->is_mine());
}

void io_m::_remove_extent_info_cache(const stid_t& stid, extnum_t ext)
{
    FUNC(io_m::_remove_extent_info_cache);
    w_assert3(!_extent_info_cache->is_mine());
    const extnum_struct_t* hit = _extent_info_cache->find(stid);
    if (hit) {
	if(hit->ext == ext) {
	    _extent_info_cache->remove(hit);
	    w_assert3(hit == NULL);
	}
	_extent_info_cache->release_mutex();
    }
    w_assert3(!_extent_info_cache->is_mine());
}

// remove all stores belonging to volume vid 
void io_m::_remove_extent_info_cache(const vid_t& vid)
{
    FUNC(io_m::_remove_extent_info_cache);
    {
	extent_info_cache_i iter(*_extent_info_cache);
	for (iter.next(); iter.curr(); iter.next())  {
	    if ( iter.curr_key()->vol == vid) {
		    iter.discard_curr();
	    }
	}
    } // destructor for iter should free mutex
    w_assert3(!_extent_info_cache->is_mine());

}

// clear entire cache
void io_m::_clear_extent_info_cache()
{
    FUNC(io_m::_clear_extent_info_cache);
    w_assert3(!_extent_info_cache->is_mine());
    {
	extent_info_cache_i iter(*_extent_info_cache);
	for (iter.next(); iter.curr(); iter.next())  {
	    iter.discard_curr();
	}
	// destructor for iter should free mutex
    }
    w_assert3(!_extent_info_cache->is_mine());
}

bool
io_m::_lookup_extent_info_cache(const stid_t& stid,
	extnum_t& result,
	int&     nresvd)
{
    FUNC(io_m::_lookup_extent_info_cache);
    w_assert3(!_extent_info_cache->is_mine());
    extnum_struct_t* hit = _extent_info_cache->find(stid);
    DBGTHRD(<<"store " << stid.store << " hit=" << hit );
    // Consider store 0 not to count
    if (hit) {
	if(hit->ext>0) {
	    DBGTHRD(<<" hit with extent " << hit->ext
		<< " nresvd " << hit->nresvd);
	    result = hit->ext;
	    nresvd = hit->nresvd;
	    INC_TSTAT(ext_lookup_hits);
	    _extent_info_cache->release_mutex();
	    w_assert3(!_extent_info_cache->is_mine());
	    return true;
	}
	_extent_info_cache->release_mutex();
    }
    DBGTHRD(<<" miss " );
    INC_TSTAT(ext_lookup_misses);
    w_assert3(!_extent_info_cache->is_mine());
    return false; // miss
}

/*
 * Adjust the cached info for the extent
 * by the amount given - does not change the extent
 * info
 */
void 
io_m::_adjust_extent_info_cache(
    const stid_t& stid,
    int 	amt
) 
{
    FUNC(io_m::_adjust_extent_info_cache);
    w_assert3(!_extent_info_cache->is_mine());
    bool found;
    bool is_new;
    extnum_struct_t* hit = _extent_info_cache->grab(stid,found,is_new);

    w_assert3(hit!=0);
    hit->nresvd += amt;

    // don't let it go negative. It's a hint, after all.
    if(hit->nresvd < 0) hit->nresvd=0;

    _extent_info_cache->release_mutex();
    w_assert3(!_extent_info_cache->is_mine());
}

/*
 * insert/replace the cached info for the extent
 */
void 
io_m::_insert_extent_info_cache(
    const stid_t& stid,
    extnum_t 	ext, 
    int 	nresvd
) 
{
    FUNC(io_m::_insert_extent_info_cache);
    w_assert3(!_extent_info_cache->is_mine());
    bool found;
    bool is_new;
    extnum_struct_t* hit = _extent_info_cache->grab(stid,found,is_new);

    DBGTHRD(<<"_insert_extent_info_cache: store " << stid
	<< " ext " << ext
	<< " nresvd " << nresvd
	);

    DBGTHRD(<< " hit=" << hit << " found=" << found
	<< " is_new=" << is_new);

    w_assert3(hit!=0);
    hit->ext = ext;
    hit->nresvd = nresvd;

    _extent_info_cache->release_mutex();
    w_assert3(!_extent_info_cache->is_mine());
}

rc_t
io_m::recover_pages_in_ext(vid_t vid, extnum_t ext, const Pmap& pmap, bool is_alloc)
{
    enter();
    rc_t r = _recover_pages_in_ext(vid, ext, pmap, is_alloc);
    // exchanges vol mutex for i/o mutex
    leave(false);
    return r;
}

rc_t
io_m::_recover_pages_in_ext(vid_t vid, extnum_t ext, const Pmap& pmap, bool is_alloc)
{
    FUNC(io_m::_recover_pages_in_ext)

    vol_t *v = _find_and_grab(vid);
    if (!v)  return RC(eBADVOL);
    auto_release_t<smutex_t> release_on_return(v->vol_mutex());

    w_assert3(v->vid() == vid);
	
    W_COERCE( v->recover_pages_in_ext(ext, pmap, is_alloc) );

    return RCOK;
}

rc_t
io_m::store_operation(vid_t vid, const store_operation_param& param)
{
    enter();
    rc_t r = _store_operation(vid, param);
    // exchanges vol mutex for i/o mutex
    leave(false);
    return r;
}

rc_t
io_m::_store_operation(vid_t vid, const store_operation_param& param)
{
    FUNC(io_m::_set_store_deleting)

    vol_t *v = _find_and_grab(vid);
    if (!v)  return RC(eBADVOL);
    auto_release_t<smutex_t> release_on_return(v->vol_mutex());
    w_assert3(v->vid() == vid);

    W_DO( v->store_operation(param) );

    return RCOK;
}


/*
 * ONLY called during crash recovery, so it doesn't
 * have to grab the mutex
 */
rc_t
io_m::free_exts_on_same_page(const stid_t& stid, extnum_t ext, extnum_t count)
{
    w_assert3(smlevel_0::in_recovery());

    int i = _find(stid.vol);
    if (i < 0) return RC(eBADVOL);

    vol_t *v = vol[i];
    w_assert3(v->vid() == stid.vol);

    W_DO( v->free_exts_on_same_page(ext, stid.store, count) );

    return RCOK;
}

/*
 * ONLY called during crash recovery, so it doesn't
 * have to grab the mutex
 */
rc_t
io_m::set_ext_next(vid_t vid, extnum_t ext, extnum_t new_next)
{
    w_assert3(smlevel_0::in_recovery());

    int i = _find(vid);
    if (i < 0) return RC(eBADVOL);

    vol_t *v = vol[i];
    w_assert3(v->vid() == vid);

    W_DO( v->set_ext_next(ext, new_next) );

    return RCOK;
}


/*
 * for each mounted volume search free the stores which have the deleting
 * attribute set to the typeToRecover.
 * ONLY called during crash recovery, so it doesn't
 * have to grab the mutex
 */
rc_t
io_m::free_stores_during_recovery(store_deleting_t typeToRecover)
{
    w_assert3(smlevel_0::in_recovery());

    for (int i = 0; i < max_vols; i++)  {
	if (vol[i])  {
	    W_DO( vol[i]->free_stores_during_recovery(typeToRecover) );
	}
    }

    return RCOK;
}

/*
 * ONLY called during crash recovery, so it doesn't
 * have to grab the mutex
 */
rc_t
io_m::free_exts_during_recovery()
{
    w_assert3(smlevel_0::in_recovery());

    for (int i = 0; i < max_vols; i++)  {
	if (vol[i])  {
	    W_DO( vol[i]->free_exts_during_recovery() );
	}
    }

    return RCOK;
}

/*
 * ONLY called during crash recovery, so it doesn't
 * have to grab the mutex
 */
rc_t
io_m::create_ext_list_on_same_page(const stid_t& stid, extnum_t prev, extnum_t next, extnum_t count, extnum_t* list)
{
    w_assert3(smlevel_0::in_recovery());

    int i = _find(stid.vol);
    if (i < 0) return RC(eBADVOL);

    vol_t *v = vol[i];
    w_assert3(v->vid() == stid.vol);

    W_DO( v->create_ext_list_on_same_page(stid.store, prev, next, count, list) );

    return RCOK;
}

rc_t 		        
io_m::_update_ext_histo(const lpid_t& pid, space_bucket_t b)
{
    int i = _find(pid._stid.vol);
    if (i < 0) return RC(eBADVOL);

    vol_t *v = vol[i];
    w_assert3(v->vid() == pid._stid.vol);

    DBGTHRD(<<"updating histo for pid " << pid);
    W_DO( v->update_ext_histo(pid, b) );
    return RCOK;
}

rc_t 		        
io_m::_init_store_histo(store_histo_t *h, const stid_t& stid,
	pginfo_t* pages, int& numpages)
{
    int i = _find(stid.vol);
    if (i < 0) return RC(eBADVOL);

    vol_t *v = vol[i];
    w_assert3(v->vid() == stid.vol);

    W_DO( v->init_histo(h, stid.store, pages, numpages) );
    return RCOK;
}

ostream& operator<<(ostream& o, const store_operation_param& param)
{
    o << "snum="    << param.snum()
      << ", op="    << param.op();
    
    switch (param.op())  {
	case smlevel_0::t_delete_store:
	    break;
	case smlevel_0::t_create_store:
	    o << ", flags="	<< param.new_store_flags()
	      << ", eff="	<< param.eff();
	    break;
	case smlevel_0::t_set_deleting:
	    o << ", newValue="	<< param.new_deleting_value()
	      << ", oldValue="	<< param.old_deleting_value();
	    break;
	case smlevel_0::t_set_store_flags:
	    o << ", newFlags="	<< param.new_store_flags()
	      << ", oldFlags="	<< param.old_store_flags();
	    break;
	case smlevel_0::t_set_first_ext:
	    o << ", ext="	<< param.first_ext();
	    break;
    }

    return o;
}


int
io_m::collect(vtable_info_array_t& v)
{
    if (v.init(vol_cnt, vol_last)) 
        return -1;

    vtable_func<vol_t> f(v);
    int i;
    for (i=0; i<vol_cnt; i++)
    {
        f(*vol[i]);
    }
    return 0;
}



/*
 * WARNING: dump_exts and dump_stores are for debugging and don't acquire the
 * proper mutexes.  use at your own risk.
 */

rc_t
io_m::dump_exts(ostream& o, vid_t vid, extnum_t start, extnum_t end)
{
    int i = _find(vid);
    if (i == -1)  {
	return RC(eBADVOL);
    }

    W_DO( vol[i]->dump_exts(o, start, end) );

    return RCOK;
}

rc_t
io_m::dump_stores(ostream& o, vid_t vid, int start, int end)
{
    int i = _find(vid);
    if (i == -1)  {
	return RC(eBADVOL);
    }

    W_DO( vol[i]->dump_stores(o, start, end) );

    return RCOK;
}

/*********************************************************************
 *
 *  io_m::_get_vid(lvid)
 *
 *********************************************************************/
inline vid_t 
io_m::_get_vid(const lvid_t& lvid)
{
    uint4_t i;
    for (i = 0; i < max_vols; i++)  {
	if (vol[i] && vol[i]->lvid() == lvid) break;
    }

#ifndef EGCS_BUG_1
    // egcs 1.1.1 croaks on this stmt:
    return (i >= max_vols) ? vid_t::null : vol[i]->vid();
#else
    if(i >= max_vols) {
	return vid_t::null;
    } else {
	return vol[i]->vid();
    }
#endif
}


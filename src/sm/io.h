/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

/*
 *  $Id: io.h,v 1.60 1996/06/30 21:56:56 kupsch Exp $
 */
#ifndef IO_H
#define IO_H

class vol_t;
class sdesc_t;
class extlink_p;

#ifdef __GNUG__
#pragma interface
#endif

struct volume_hdr_stats_t;

/*
 * IO Manager.
 */
class io_m : public smlevel_0 {
public:
    NORET			io_m();
    NORET			~io_m();
    
    static void 		clear_stats();
    static int 			disk_reads();
    static int 			disk_writes();
    static int 			num_vols();
    
  
    // the io manager needs to be notified whenever a xct abort
    // is started
    void			invalidate_free_page_cache();

    /*
     * Device related
     */
    static bool			is_mounted(const char* dev_name);
    static rc_t			mount_dev(const char* device, u_int& vol_cnt);
    static rc_t			dismount_dev(const char* device);
    static rc_t			dismount_all_dev();
    static rc_t			get_lvid(const char* dev_name, lvid_t& lvid);
    static rc_t			list_devices(
	const char**& 		    dev_list, 
        devid_t*& 		    devid_list, 
        u_int& 			    dev_cnt);

    static rc_t			get_device_quota(
	const char*		    device, 
	smksize_t&		    quota_KB, 
	smksize_t&		    quota_used_KB);
    

    /*
     * Volume related
     */
    static rc_t			get_vols(
	int			    start,
	int 			    count, 
	char 			    dname[][max_devname+1], 
	vid_t 			    vid[],
	int&			    return_cnt);
    static rc_t			check_disk(vid_t vid);
    // return an unused vid_t
    static rc_t			get_new_vid(vid_t& vid);
    static bool			is_mounted(vid_t vid);
    static vid_t		get_vid(const lvid_t& lvid);
    static lvid_t		get_lvid(const vid_t vid);
    static const char* 		dev_name(vid_t vid);
    static lsn_t		GetLastMountLSN();		// used for logging/recovery purposes
    static void			SetLastMountLSN(lsn_t theLSN);

    static rc_t 		read_page(
	const lpid_t&		    pid,
	page_s&			    buf);
    //static void 		write_page(page_s& buf);
    static void 		write_many_pages(page_s** bufs, int cnt);
    
    static rc_t			mount(const char* device, vid_t vid);
    static rc_t			dismount(vid_t vid, bool flush = TRUE);
    static rc_t			dismount_all(bool flush = TRUE);
    static rc_t			sync_all_disks();

    static rc_t			get_volume_quota(
	vid_t 			    vid, 
	smksize_t&		    quota_KB, 
	smksize_t&		    quota_used_KB);
    
    static rc_t			alloc_pages(
	const stid_t& 		    stid,
	const lpid_t& 		    near,
	int 			    cnt,
	lpid_t 			    pids[],
	bool			    forward_alloc = FALSE,
	sdesc_t*		    sd=0);

    static rc_t			alloc_new_last_page(
	const stid_t& 		    stid,
	const lpid_t& 		    near,
	lpid_t& 		    pid,
	sdesc_t*		    sd=0);

    static rc_t			free_page(const lpid_t& pid);
    static bool 		is_valid_page(const lpid_t& pid);

    static rc_t			create_store(
	vid_t 			    vid, 
	int 			    EFF,
	uint4_t			    flags,
	stid_t& 		    stid,
	extnum_t		    first_ext = 0);
    static rc_t			destroy_store(const stid_t& stid);
    static rc_t			get_store_flags(
	const stid_t&		    stid,
	uint4_t&		    flags);
    static rc_t			set_store_flags(
	const stid_t&		    stid,
	uint4_t			    flags);
    static bool 		is_valid_store(const stid_t& stid);

    // The following functions return the first/last/next pages in a
    // store.  If "allocated" is NULL then only allocated pages will be
    // returned.  If "allocated" is non-null then all pages will be
    // returned and the bool pointed to by "allocated" will be set to
    // indicate whether the page is allocated.
    static rc_t			first_page(
	const stid_t&		    stid,
	lpid_t&			    pid,
	bool*			    allocated = NULL,
	bool			    lock = 0);
    static rc_t			last_page(
	const stid_t&		    stid,
	lpid_t&			    pid,
	bool*			    allocated = NULL,
	bool			    lock = 0);
    static rc_t                 next_page(
        lpid_t&                     pid,
        bool*                     allocated = NULL,
	bool			    lock = 0);

    // this reports du statistics
    static rc_t                 get_du_statistics( // DU DF
	vid_t			    vid,
	volume_hdr_stats_t&	    stats,
	bool			    audit);

    // This function sets a milli_sec delay to occur before 
    // each disk read/write operation.  This is useful in discovering
    // thread sync bugs
    static rc_t			set_disk_delay(uint4 milli_sec)
		{ _msec_disk_delay = milli_sec; return RCOK; }
  
    //
    // Statistics information
    //
    static void 		io_stats(
	u_long& 		    reads, 
	u_long& 		    writes, 
	u_long& 		    allocs,
	u_long& 		    deallocs, 
	bool 			    reset);

    //
    // For recovery & rollback ONLY:
    //
    static rc_t			recover_extent(bool alloc, stid_t& stid, int cnt,
				    struct ext_log_info_t* list, bool force=false);

private:
    static void			enter();
    static void			leave();
    static smutex_t		_mutex;
    static int			vol_cnt;
    static vol_t* 		vol[max_vols];
    static uint4		_msec_disk_delay;
    static lsn_t		_lastMountLSN;

protected:
    /* lock_force: A function that calls the lock manager, but avoids
     * lock-mutex deadlocks in the process:
     * (Share this with the volume manager.)
     */
    static rc_t		        lock_force(
				    const lockid_t&	    n,
				    lock_mode_t		    m,
				    lock_duration_t         d,
				    long		    timeout,
				    bool		    optimistic = FALSE,
				    extlink_p*		    page = 0
				    );

    friend
    rc_t		        io_lock_force(
				    const lockid_t&	    n,
				    lock_mode_t		    m,
				    lock_duration_t         d,
				    long		    timeout, 
				    bool		    optimistic = FALSE,
				    extlink_p*		    page = 0
				    );

private:

    // The io_m manages a cache of information, keyed on store ID
    // indicating the only extent in the store with unallocated
    // pages.  The purpose of this cache is to eliminate the n**2
    // part of the algorithm for allocating pages in a store.
    // If there are no free pages in the extent with
    // the near hint, then all extents in the store are checked for 
    // a free page.  If a store has an entry in this cache, the
    // the extent referred to in the entry is the only one with free
    // pages. (well, "the only one" no longer holds-- NEH )
    // 
    // The following are the member functions on the cache.
    // The cache "object" is a static variable in io.c
    // so that users of io.h do not need to know about
    // it's template-based implementation.
    //
    static void			_adjust_last_extent_used(
	const stid_t&		    stid,
	extnum_t		    ext,
	int			    allocated,
	int			    remaining
    );
    static void			_remove_last_extent_used(
	const stid_t&		    stid
    );
    static void			_remove_last_extent_used(
	const stid_t&		    stid,
	extnum_t 		    ext
    );
    static void			_remove_last_extent_used(
	const vid_t&		    vid
    );
    static void			_clear_last_extent_used();
    // return extent>0 if stid is in the cache
    static bool			_lookup_last_extent_used(
	const stid_t&		    stid,
	extnum_t& 		    result,
	bool&     		    has_free_pages
    );

    static bool			_is_mounted(const char* dev_name);
    static rc_t			_mount_dev(const char* device, u_int& vol_cnt);
    static rc_t			_dismount_dev(const char* device);
    static rc_t			_dismount_all_dev();
    static rc_t			_get_lvid(const char* dev_name, lvid_t& lvid);
    static rc_t			_list_devices(const char**& dev_list, devid_t*& devid_list, u_int& dev);
    static rc_t			_get_device_quota(
	const char*		    device, 
	smksize_t&		    quota_KB, 
	smksize_t&		    quota_used_KB);
    
    static const char* 		_dev_name(vid_t vid);
    static int 			_find(vid_t vid);
    static rc_t			_get_vols(
	int			    start,
	int 			    count, 
	char 			    dname[][max_devname+1], 
	vid_t 			    vid[],
	int&			    return_cnt);
    
    static rc_t			_get_volume_quota(
	vid_t 			    vid, 
	smksize_t&		    quota_KB, 
	smksize_t&		    quota_used_KB);
    
    static rc_t			_check_disk(vid_t vid);
    static rc_t			_get_new_vid(vid_t& vid);
    static vid_t		_get_vid(const lvid_t& lvid);
    static lvid_t		_get_lvid(const vid_t vid);
    static rc_t			_mount(const char* device, vid_t vid);
    static rc_t			_dismount(vid_t vid, bool flush);
    static rc_t			_dismount_all(bool flush);
    static rc_t			_alloc_pages(
	const stid_t&		    stid, 
	const lpid_t& 		    near,
	int 			    cnt,
	lpid_t 			    pids[],
	bool			    forward_alloc = FALSE,
	sdesc_t*		    sd=0);
    static rc_t			_free_page(const lpid_t& pid);
    static rc_t			_first_page(
	const stid_t&		    stid,
	lpid_t&			    pid,
	bool*			    allocated = NULL,
	bool			    lock = 0);
    static rc_t			_next_page(
	lpid_t& 		    pid,
	bool*			    allocated,
	bool			    lock = 0);
    static rc_t			_last_page(
	const stid_t&		    stid,
	lpid_t&			    pid,
	bool*			    allocated = NULL,
	bool			    lock = 0);
    static bool 		_is_valid_page(const lpid_t& pid);
    static rc_t			_create_store(
	vid_t 			    vid, 
	int 			    EFF, 
	uint4_t			    flags,
	stid_t& 		    stid,
	extnum_t		    first_ext = 0);
    static rc_t			_destroy_store(const stid_t& stid);
    static rc_t			_get_store_flags(
	const stid_t&		    stid,
	uint4_t&		    flags);
    static rc_t			_set_store_flags(
	const stid_t&		    stid,
	uint4_t			    flags);
    static bool 		_is_valid_store(const stid_t& stid);

    // this reports du statistics
    static rc_t                 _get_du_statistics( // DU DF
        vid_t                       vid,
        volume_hdr_stats_t&	    stats,
	bool			    audit);

};

inline int
io_m::disk_reads()
{
    return smlevel_0::stats.vol_reads;
}

inline int
io_m::disk_writes()
{
    return smlevel_0::stats.vol_writes;
}

inline int
io_m::num_vols()
{
    return vol_cnt;
}

inline bool
io_m::is_mounted(const char* dev_name)
{
    enter();
    bool result = _is_mounted(dev_name); 
    leave();
    return result;
}

inline rc_t
io_m::mount_dev(const char* dev_name, u_int& vol_cnt)
{
    enter();
    rc_t rc = _mount_dev(dev_name, vol_cnt); 
    leave();
    return rc.reset();
}

inline rc_t
io_m::dismount_dev(const char* dev_name)
{
    enter();
    rc_t rc = _dismount_dev(dev_name); 
    leave();
    return rc.reset();
}

inline rc_t
io_m::dismount_all_dev()
{
    enter();
    rc_t rc = _dismount_all_dev(); 
    leave();
    return rc.reset();
}

inline rc_t
io_m::list_devices(const char**& dev_list, devid_t*& devid_list, u_int& dev_cnt)
{
    enter();
    rc_t rc = _list_devices(dev_list, devid_list, dev_cnt); 
    leave();
    return rc.reset();
}

inline rc_t 
io_m::get_device_quota(
	const char*		    device, 
	smksize_t&		    quota_KB, 
	smksize_t&		    quota_used_KB)
{
    enter();
    rc_t r = _get_device_quota(device, quota_KB, quota_used_KB);
    leave();
    return r;
}

inline rc_t
io_m::get_vols(int start, int count, char dname[][smlevel_0::max_devname+1],
               vid_t vid[], int& return_cnt)
{
    enter();
    rc_t r = _get_vols(start, count, dname, vid, return_cnt);
    leave();
    return r;
}

inline const char* 
io_m::dev_name(vid_t vid) 
{
    enter();
    const char* r = _dev_name(vid);
    leave();
    return r;
}

inline lsn_t
io_m::GetLastMountLSN()
{
    return _lastMountLSN;
}

inline void
io_m::SetLastMountLSN(lsn_t theLSN)
{
    w_assert3(theLSN >= _lastMountLSN);
    _lastMountLSN = theLSN;
}

inline rc_t 
io_m::get_volume_quota(
	vid_t 			    vid, 
	smksize_t&		    quota_KB, 
	smksize_t&		    quota_used_KB)
{
    enter();
    rc_t r = _get_volume_quota(vid, quota_KB, quota_used_KB);
    leave();
    return r;
}

inline rc_t
io_m::check_disk(vid_t vid)
{
    enter();
    rc_t r = _check_disk(vid);
    leave();
    return r;
}

inline rc_t
io_m::get_new_vid(vid_t& vid)
{
    enter();
    rc_t r = _get_new_vid(vid);
    leave();
    return r;
}


inline rc_t 
io_m::dismount_all(bool flush)
{
    enter();
    rc_t r = _dismount_all(flush);
    leave();
    return r;
}


inline bool 
io_m::is_valid_page(const lpid_t& pid) 
{
    enter();
    bool r = _is_valid_page(pid);
    leave();
    return r;
}

inline rc_t
io_m::first_page(const stid_t& stid, lpid_t& pid, 
		 bool* allocated, bool /*lock*/)
{
    enter();
    rc_t r = _first_page(stid, pid, allocated);
    leave();
    return r;
}

inline rc_t
io_m::next_page(lpid_t& pid, bool* allocated, bool /*lock*/) 
{
    enter();
    rc_t r = _next_page(pid, allocated);
    leave();
    return r;
}

inline rc_t
io_m::last_page(const stid_t& stid, lpid_t& pid,
		bool* allocated, bool /*lock*/)
{
    enter();
    rc_t r = _last_page(stid, pid, allocated);
    leave();
    return r;
}

inline rc_t
io_m::set_store_flags(const stid_t& stid, uint4_t flags)
{
    rc_t r;
    if (stid.store)  {
	enter();
    	r = _set_store_flags(stid, flags);
	leave();
    }
    return r;
}

inline rc_t
io_m::get_store_flags(const stid_t& stid, uint4_t& flags)
{
    return _get_store_flags(stid, flags);
}


    
inline bool
io_m::is_valid_store(const stid_t& stid) 
{
    enter();
    bool r = _is_valid_store(stid);
    leave();
    return r;
}

inline rc_t
io_m::get_du_statistics(vid_t vid, volume_hdr_stats_t& stats, bool audit)
{
    enter();
    rc_t rc = _get_du_statistics(vid, stats, audit);       // DU DF
    leave();
    return rc.reset();
}

#endif  /* IO_H */


/*<std-header orig-src='shore' incl-file-exclusion='VOL_H'>

 $Id: vol.h,v 1.93 1999/06/07 19:04:47 kupsch Exp $

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

#ifndef VOL_H
#define VOL_H

#include "w_defines.h"

/*  -- do not edit anything above this line --   </std-header>*/

#ifdef __GNUG__
#pragma interface
#endif


struct volume_hdr_stats_t;
class store_histo_t; // page_h.h
class pginfo_t; // page_h.h

class volhdr_t {
    // For compatibility checking, we record a version number
    // number of the Shore SM version which formatted the volume.
    // This number is called volume_format_version in sm_base.h.
    uint4_t			_format_version;
#ifdef SM_ODS_COMPAT_13
    sthread_base_t::base_stat_t	_device_quota_KB;
#else
    sm_diskaddr_t		_device_quota_KB;
#endif
    lvid_t			_lvid;
    extnum_t			_ext_size;
    shpid_t			_epid;		// extent pid
    shpid_t			_spid;		// store pid
    extnum_t			_num_exts;
    extnum_t			_hdr_exts;
    uint4_t			_page_sz;	// page size in bytes
public:
    uint4_t			format_version() const { 
				    return _format_version; }
    void			set_format_version(uint v) { 
					_format_version = v; }

    sm_diskaddr_t		device_quota_KB() const { 
				    return _device_quota_KB; }
    void			set_device_quota_KB(sm_diskaddr_t q) { 
					_device_quota_KB = q; }

    const lvid_t&		lvid() const { return _lvid; }
    void 			set_lvid(const lvid_t &l) { 
					_lvid = l; }

    extnum_t			ext_size() const { return _ext_size; } 
    void			set_ext_size(extnum_t e) { _ext_size=e; } 

    const shpid_t&		epid() const { return _epid; }
    void 			set_epid(const shpid_t& p) { 
					_epid = p; }

    const shpid_t&		spid() const { return _spid; }
    void 			set_spid(const shpid_t& p) { 
					_spid = p; }

    extnum_t			num_exts() const {  return _num_exts; }
    void			set_num_exts(extnum_t n) {  _num_exts = n; }

    extnum_t			hdr_exts() const {  return _hdr_exts; }
    void			set_hdr_exts(extnum_t n) {  _hdr_exts = n; }

    uint4_t			page_sz() const {  return _page_sz; }
    void			set_page_sz(uint4_t n) {  _page_sz = n; }

};

class vol_stats_t
{
  public:
    uint4_t         vol_writes;
    uint4_t         vol_blks_written;
    uint4_t         vol_reads;
    uint4_t         vol_blks_read;

    vol_stats_t() : vol_writes(0), vol_blks_written(0),
                    vol_reads(0), vol_blks_read(0) {};

    void reset() { vol_writes = vol_blks_written = 0;
                   vol_reads = vol_blks_read = 0; }
};

class vol_t : public smlevel_1 {
public:
    NORET			vol_t();
    NORET			~vol_t();
    
    static int			max_extents_on_page();

    rc_t 			mount(const char* devname, vid_t vid);
    rc_t 			dismount(bool flush = true);
    rc_t 			check_disk();

    const char* 		devname() const;
    vid_t 			vid() const ;
    lvid_t 			lvid() const ;
    extnum_t 			ext_size() const;
    extnum_t 			num_exts() const;
    extnum_t 			pid2ext(const lpid_t& pid) const;
    

public:
    int				fill_factor(snum_t fnum);
 
    bool 			is_valid_page(const lpid_t& p) const;
    bool 			is_valid_store(snum_t f) const;
    bool 			is_alloc_ext(extnum_t e) const;
    bool 			is_alloc_ext_of(extnum_t e, snum_t s)const;
    bool 			is_alloc_page(const lpid_t& p) const;
    bool 			is_alloc_page_of(const lpid_t& p, snum_t s)const ;
    bool 			is_alloc_store(snum_t f) const;
    

    rc_t 			write_page(shpid_t page, page_s& buf);
    rc_t 			write_many_pages(
	shpid_t 		    first_page,
	page_s**		    buf, 
	int 			    cnt);
    rc_t 			read_page(
	shpid_t 		    page,
	page_s& 		    buf);

    rc_t			alloc_page_in_ext(
	bool			    append_only,
	extnum_t		    ext, 
	int 			    eff,
	snum_t 			    fnum,
	int 			    cnt,
	lpid_t 			    pids[],
	int& 			    allocated,
	int&			    remaining,
	bool&			    is_last,
	bool	 		    may_realloc  = false,
	lock_mode_t		    desired_lock_mode = IX	 
	);

    rc_t			recover_pages_in_ext(
	extnum_t		    ext,
	const Pmap&		    pmap,
	bool			    is_alloc);
    
    rc_t			store_operation(
	const store_operation_param&	param);

    rc_t			free_stores_during_recovery(
	store_deleting_t	    typeToRecover);

    rc_t			free_exts_during_recovery();

    rc_t			free_page(const lpid_t& pid);

    rc_t			find_free_exts(
	uint 			    cnt,
	extnum_t 		    exts[],
	int& 			    found,
	extnum_t		    first_ext = 0);
    rc_t			num_free_exts(uint4_t& cnt);
    rc_t			num_used_exts(uint4_t& cnt);
    rc_t			alloc_exts(
	snum_t 			    num,
	extnum_t 		    prev,
	int 			    cnt,
	const extnum_t 		    exts[]);


    rc_t 		        update_ext_histo(const lpid_t& pid, space_bucket_t b);
    rc_t 			next_ext(extnum_t ext, extnum_t &res);
    rc_t			dump_exts(ostream &,
					 extnum_t start, extnum_t end);
    rc_t			dump_stores(ostream &,
					int start, int end);

    rc_t			find_free_store(snum_t& fnum);
    rc_t			alloc_store(
	snum_t 			    fnum,
	int 			    eff,
	store_flag_t		    flags);
    rc_t			set_store_first_ext(
	snum_t 			    fnum,
	extnum_t 		    head);

    rc_t			set_store_flags(
	snum_t 			    fnum,
	store_flag_t 		    flags,
	bool			    sync_volume);
    rc_t			get_store_flags(
	snum_t 			    fnum,
	store_flag_t&		    flags);
    rc_t			free_store(
	snum_t			    fnum,
	bool			    acquire_lock);
    rc_t			free_store_after_xct(snum_t snum);
    rc_t			free_ext_after_xct(extnum_t ext, snum_t&);
    rc_t			set_ext_next(
	extnum_t		    ext,
	extnum_t		    new_next);

    rc_t 			first_ext(snum_t fnum, extnum_t &result);
private:
    bool			_is_valid_ext(extnum_t e) const;

    rc_t			_free_ext_list(
	extnum_t		    head,
	snum_t			    snum);
    rc_t			_append_ext_list(
	snum_t			    snum,
	extnum_t		    prev,
	extnum_t		    count,
	const extnum_t*		    list);
public:
    rc_t			free_exts_on_same_page(
	extnum_t		    ext,
	snum_t			    snum,
	extnum_t		    count);
    rc_t			create_ext_list_on_same_page(
	snum_t			    snum,
	extnum_t		    prev,
	extnum_t		    next,
	extnum_t		    count,
	const extnum_t*		    list);

public:
    rc_t 			init_histo(store_histo_t* h,  snum_t snum,
					pginfo_t* pages, int& numpages);
    snum_t			max_store_id_in_use() const;

    // The following functinos return space utilization statistics
    // on the volume or selected stores.  These functions use only
    // the store and page/extent meta information.

    rc_t                 	get_volume_meta_stats(
        SmVolumeMetaStats&          volume_stats);
    rc_t                 	get_file_meta_stats(
        uint4_t                       num_files,
        SmFileMetaStats*            file_stats);
    rc_t                 	get_file_meta_stats_batch(
        uint4_t                       max_store,
        SmStoreMetaStats**          mapping);
    rc_t			get_store_meta_stats(
	snum_t			    snum,
	SmStoreMetaStats&	    storeStats);
	
    // The following functions return the first/last/next pages in a
    // store.  If "allocated" is NULL then only allocated pages will be
    // returned.  If "allocated" is non-null then all pages will be
    // returned and the bool pointed to by "allocated" will be set to
    // indicate whether the page is allocated.
    rc_t			first_page(
	snum_t 			    fnum,
	lpid_t&			    pid,
	bool*			    allocated = NULL);
    rc_t			last_page(
	snum_t			    fnum,
	lpid_t&			    pid,
	bool*			    allocated = NULL);
    rc_t 			next_page(
	lpid_t&			    pid,
	bool*			    allocated = NULL);
    rc_t 			next_page_with_space(lpid_t& pid, 
				    space_bucket_t needed);

    rc_t			num_pages(snum_t fnum, uint4_t& cnt);
    rc_t			num_exts(snum_t fnum, uint4_t& cnt);
    bool 			is_raw() { return _is_raw; };

    rc_t			sync();

    // format a device (actually, just zero out the header for now)
    static rc_t			format_dev(
	const char* 		    devname,
	shpid_t 		    num_pages,
	bool 			    force);

    static rc_t			format_vol(
	const char* 		    devname,
	lvid_t 			    lvid,
	shpid_t 		    num_pages,
	bool			    skip_raw_init);
    static rc_t			read_vhdr(const char* devname, volhdr_t& vhdr);
    static rc_t			read_vhdr(int fd, volhdr_t& vhdr);

    static rc_t			write_vhdr(           // SMUF-SC3: moved to public
	int			    fd, 
	volhdr_t& 		    vhdr, 
	bool 			    raw_device);

    static rc_t			check_raw_device(
	const char* 		    devname,
	bool&			    raw);
    

    // methods for space usage statistics for this volume
    rc_t 			get_du_statistics(
	struct			    volume_hdr_stats_t&,
	bool			    audit);

    void			acquire_mutex();
    smutex_t&			vol_mutex() { return _mutex; }

    void            vtable_collect(vtable_info_t &);

    NORET			W_FASTNEW_CLASS_DECL(vol_t);
private:
    char 			_devname[max_devname];
    int				_unix_fd;
    vid_t			_vid;
    lvid_t			_lvid;
    extnum_t			_num_exts;
    uint			_hdr_exts;
    extnum_t			_min_free_ext_num;
    lpid_t			_epid;
    lpid_t			_spid;
    int				_page_sz;  // page size in bytes
    bool			_is_raw;   // notes if volume is a raw device

    smutex_t			_mutex;   // make each volume mgr a monitor
					// so that once we descend into
					// the volume manager, we can
					// release the I/O monitor's
					// mutex and get some parallelism
					// with multiple volumes.

    shpid_t 			ext2pid(snum_t s, extnum_t e) const;
    extnum_t 			pid2ext(snum_t s, shpid_t p) const;
    vol_stats_t     		_stats; // per volume io statistics


    static const char* 		prolog[];

};


/*
 * STORES:
 * Each volume contains a few stores that are "overhead":
 * 0 -- is reserved for the extent map and the store map
 * 1 -- directory (see dir.cpp)
 * 2 -- root index (see sm.cpp)
 * 3 -- small (1-page) index (see sm.cpp)
 *
 * If the volume has a logical id index on it, it also has
 * 4 -- local logical id index  (see sm.cpp, ss_m::add_logical_id_index)
 * 5 -- remote logical id index  (ditto)
 *
 * After that, for each file created, 2 stores are used, one for
 * small objects, one for large objects.
 * Each index(btree, rtree) uses one store. 
 */


inline vol_t::vol_t() : _unix_fd(-1), _min_free_ext_num(1), _mutex("vol")  {};

inline vol_t::~vol_t() 			{ w_assert1(_unix_fd == -1); }

inline const char* vol_t::devname() const
{
    return _devname;
}

/*
 * NB: pageids are: vol.store.page
 * but that does not mean that the .page part is relative to the
 * .store part.  In fact, the .page is relative to the volume
 * and the .store part ONLY indicates what store owns that page.
 */

inline extnum_t vol_t::pid2ext(snum_t /*snum*/, shpid_t p) const
{
    //snum = 0; or store_id_extentmap
    return (extnum_t) (p / ext_sz);
}

inline shpid_t vol_t::ext2pid(snum_t/*snum*/, extnum_t ext) const
{
    return ext * ext_sz;
}

inline extnum_t vol_t::pid2ext(const lpid_t& pid) const
{
    w_assert3(pid.vol() == _vid);
    return pid2ext(pid.store(), pid.page);
}

inline vid_t vol_t::vid() const
{
    return _vid;
}

inline lvid_t vol_t::lvid() const
{
    return _lvid;
}

inline extnum_t vol_t::ext_size() const
{
    return ext_sz;
}

inline extnum_t vol_t::num_exts() const
{
    return (extnum_t) _num_exts;
}

inline bool vol_t::_is_valid_ext(extnum_t e) const
{
    return (e < _num_exts);
}

inline bool vol_t::is_valid_page(const lpid_t& p) const
{
    return (((unsigned int)( _num_exts) * ext_sz) > p.page);
}

inline bool vol_t::is_valid_store(snum_t f) const
{
    // Can't have more stores than extents, so
    // this is a sufficient check for a reasonable
    // store number, although it doesn't tell if the
    // store is allocated.

    return (f < _num_exts );
}

/*<std-footer incl-file-exclusion='VOL_H'>  -- do not edit anything below this line -- */

#endif          /*</std-footer>*/

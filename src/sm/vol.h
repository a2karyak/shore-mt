/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

/*
 *  $Id: vol.h,v 1.61 1996/07/09 20:41:32 nhall Exp $
 */
#ifndef VOL_H
#define VOL_H

#ifdef __GNUG__
#pragma interface
#endif

struct volume_hdr_stats_t;

struct volhdr_t {
    // For compatibility checking, we record a version number
    // number of the Shore SM version which formatted the volume.
    // This number is called volume_format_version in sm_base.h.
    uint4			format_version;

    uint4			device_quota_KB;
    lvid_t			lvid;
    short			ext_size;
    shpid_t			epid;		// extent pid
    shpid_t			spid;		// store pid
    uint4			num_exts;
    uint4			hdr_exts;
    uint4			page_sz;	// page size in bytes

};

class vol_t : public smlevel_1 {
public:
    NORET			vol_t();
    NORET			~vol_t();
    
    rc_t 			mount(const char* devname, vid_t vid);
    rc_t 			dismount(bool flush = TRUE);
    rc_t 			check_disk();

    const char* 		devname() const;
    vid_t 			vid() const ;
    lvid_t 			lvid() const ;
    extnum_t 			ext_size() const;
    extnum_t 			num_exts() const;
    extnum_t 			pid2ext(const lpid_t& pid);
    int 			num_stores() const;
    
    rc_t 			first_ext(snum_t fnum, extnum_t &result);
    int				fill_factor(snum_t fnum);
 
    bool			is_valid_ext(extnum_t e) const;
    bool 			is_valid_page(const lpid_t& p) const;
    bool 			is_valid_store(snum_t f) const;
    bool 			is_alloc_ext(extnum_t e);
    bool 			is_alloc_page(const lpid_t& p);
    bool 			is_alloc_store(snum_t f);
    //bool 			is_remote()  { return FALSE; }  // for now
    

    rc_t 			write_page(shpid_t page, page_s& buf);
    rc_t 			write_many_pages(
	shpid_t 		    first_page,
	page_s**		    buf, 
	int 			    cnt);
    rc_t 			read_page(
	shpid_t 		    page,
	page_s& 		    buf);

    rc_t			alloc_page_in_ext(
	extnum_t		    ext, 
	int 			    eff,
	snum_t 			    fnum,
	int 			    cnt,
	lpid_t 			    pids[],
	int& 			    allocated,
	int&			    remaining,
	bool&			    is_last
	);

    //not used rc_t			alloc_page(const lpid_t& pid);

    rc_t			free_page(const lpid_t& pid);

    rc_t			find_free_exts(
	uint 			    cnt,
	extnum_t 		    exts[],
	int& 			    found,
	extnum_t		    first_ext = 0);
    rc_t			num_free_exts(uint4& cnt);
    rc_t			num_used_exts(uint4& cnt);
    rc_t			alloc_exts_in_order(
	snum_t 			    num,
	int 			    cnt,
	const ext_log_info_t        exts[]);
    rc_t			alloc_exts(
	snum_t 			    num,
	extnum_t 		    prev,
	int 			    cnt,
	const extnum_t 		    exts[]);

    rc_t			free_exts(extnum_t head);
    rc_t			free_extent(snum_t snum, extnum_t extnum,
					bool& was_freed);

    rc_t 			next_ext(extnum_t ext, extnum_t &res);

    rc_t			find_free_store(snum_t& fnum);
    rc_t			alloc_store(
	snum_t 			    fnum,
	int 			    eff,
	uint4_t			    flags);
    rc_t			set_store_first_ext(
	snum_t 			    fnum,
	extnum_t 		    head);
    rc_t			set_store_flags(
	snum_t 			    fnum,
	uint4_t 		    flags);
    rc_t			get_store_flags(
	snum_t 			    fnum,
	uint4_t&		    flags);
    rc_t			free_store(snum_t fnum);

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
    static rc_t			check_raw_device(
	const char* 		    devname,
	bool&			    raw);
    

    // methods for space usage statistics for this volume
    rc_t 			get_du_statistics(
	struct			    volume_hdr_stats_t&,
	bool			    audit);

private:
    char 			_devname[max_devname];
    int				_unix_fd;
    vid_t			_vid;
    lvid_t			_lvid;
    u_long			_num_exts;
    uint			_hdr_exts;
    lpid_t			_epid;
    lpid_t			_spid;
    int				_page_sz;  // page size in bytes
    bool			_is_raw;   // notes if volume is a raw device

    shpid_t 			ext2pid(snum_t s, extnum_t e);
    extnum_t 			pid2ext(snum_t s, shpid_t p);

    static rc_t			write_vhdr(
	int			    fd, 
	volhdr_t& 		    vhdr, 
	bool 			    raw_device);
    static const char* 		prolog[];

};

class extlink_t {
    Pmap			pmap; 	   // 1 byte
public:
    fill1			filler;    // 1 byte for alignment
					   // correct size is checked
					   // in constructor			
    extnum_t			next; 	   // 2 bytes
    extnum_t			prev; 	   // 2 bytes
    snum_t 			owner;	   // 2 bytes
    tid_t 			tid;	   // tid that freed this -- 8 bytes

    NORET			extlink_t();
    NORET			extlink_t(const extlink_t& e);
    extlink_t& 			operator=(const extlink_t&);

    void 			zero();
    void 			fill();
    void 			setmap(const Pmap &m);
    void 			getmap(Pmap &m) const;
    void 			set(int i);
    void 			clr(int i);
    bool 			is_set(int i) const;
    bool 			is_clr(int i) const;
    int 			first_set(int start) const;
    int 			first_clr(int start) const;
    int 			num_set() const;
    int 			num_clr() const;

    friend ostream& operator<<(ostream &, const extlink_t &e);
};

class extlink_p : public page_p {
public:
    MAKEPAGE(extlink_p, page_p, 2); // make extent links a little hotter than
	// others

    enum { max = data_sz / sizeof(extlink_t) };

    const extlink_t& 		get(int idx);
    void 			put(int idx, const extlink_t& e);
    void 			set_bit(int idx, int bit);
    void 			clr_bit(int idx, int bit); 

private:
    extlink_t& 			item(int i);

    struct layout_t {
	extlink_t 		    item[max];
    };

    // disable
    friend class page_link_log;   // just to keep g++ happy
};


/*
 * STORES:
 * Each volume contains a few stores that are "overhead":
 * 0 -- is reserved for the extent map and the store map
 * 1 -- directory (see dir.c)
 * 2 -- root index (see sm.c)
 * 3 -- small (1-page) index (see sm.c)
 *
 * If the volume has a logical id index on it, it also has
 * 4 -- local logical id index  (see sm.c, ss_m::add_logical_id_index)
 * 5 -- remote logical id index  (ditto)
 *
 * After that, for each file created, 2 stores are used, one for
 * small objects, one for large objects.
 * Each index(btree, rtree) uses one store. 
 */
	

struct stnode_t {
    extnum_t			head;
    w_base_t::uint2_t		eff;
    w_base_t::uint4_t		flags;
};

    
class stnode_p : public page_p {
public:
    MAKEPAGE(stnode_p, page_p, 1);
    enum { max = data_sz / sizeof(stnode_t) };

    const stnode_t& 		get(int idx);
    rc_t 			put(int idx, const stnode_t& e);

private:
    stnode_t& 			item(int i);
    struct layout_t {
	stnode_t 		    item[max];
    };
    friend class page_link_log;   // just to keep g++ happy

};    

inline extlink_t&
extlink_p::item(int i)
{
    w_assert3(i < max);
    return ((layout_t*)tuple_addr(0))->item[i];
}


inline const extlink_t&
extlink_p::get(int idx)
{
    return item(idx);
}

inline void
extlink_p::put(int idx, const extlink_t& e)
{
    DBG(<<"extlink_p::put(" <<  idx << " owner=" <<
	    e.owner << ", " << e.next << ")");
    W_COERCE(overwrite(0, idx * sizeof(extlink_t),
		     vec_t(&e, sizeof(e))));
}

inline void
extlink_p::set_bit(int idx, int bit)
{
    W_COERCE(page_p::set_bit(0, idx * sizeof(extlink_t) * 8 + bit));
}

inline void
extlink_p::clr_bit(int idx, int bit)
{
    W_COERCE(page_p::clr_bit(0, idx * sizeof(extlink_t) * 8 + bit));
}

inline stnode_t&
stnode_p::item(int i)
{
    w_assert3(i < max);
    return ((layout_t*)tuple_addr(0))->item[i];
}

inline const stnode_t&
stnode_p::get(int idx)
{
    return item(idx);
}

inline w_rc_t 
stnode_p::put(int idx, const stnode_t& e)
{
    W_DO(overwrite(0, idx * sizeof(stnode_t), vec_t(&e, sizeof(e))));
    return RCOK;
}

inline vol_t::vol_t() : _unix_fd(-1)	{};

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

inline extnum_t vol_t::pid2ext(snum_t /*snum*/, shpid_t p)
{
    //snum = 0;
    return p / ext_sz;
}

inline shpid_t vol_t::ext2pid(snum_t/*snum*/, extnum_t ext)
{
    return ext * ext_sz;
}

inline extnum_t vol_t::pid2ext(const lpid_t& pid)
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
    return _num_exts;
}

inline bool vol_t::is_valid_ext(extnum_t e) const
{
    return (e < _num_exts);
}

inline bool vol_t::is_valid_page(const lpid_t& p) const
{
#ifdef SOLARIS2
    return (_num_exts * ext_sz > p.page);
#else
    return (p.page < _num_exts * ext_sz);
#endif
}

inline bool vol_t::is_valid_store(snum_t f) const
{
    return (f < _num_exts);
}

inline extlink_t::extlink_t(const extlink_t& e) 
: next(e.next), prev(e.prev), owner(e.owner), tid(tid_t::null)
{
    memcpy(pmap, e.pmap, sizeof(pmap));
}

inline extlink_t& extlink_t::operator=(const extlink_t& e)
{
    prev = e.prev, 
	next = e.next, 
	tid = e.tid, 
	owner = e.owner; 
    memcpy(pmap, e.pmap, sizeof(pmap));
    return *this;
}
inline void extlink_t::setmap(const Pmap &m)
{
    memcpy(pmap, m, sizeof(pmap));
}
inline void extlink_t::getmap(Pmap &m) const
{
    memcpy(m, pmap, sizeof(pmap));
}

inline void extlink_t::zero()
{
    bm_zero(pmap, smlevel_0::ext_sz);
}

inline void extlink_t::fill()
{
    bm_fill(pmap, smlevel_0::ext_sz);
}

inline void extlink_t::set(int i)
{
    bm_set(pmap, i);
}

inline void extlink_t::clr(int i)
{
    bm_clr(pmap, i);
}

inline bool extlink_t::is_set(int i) const
{
    w_assert3(i < smlevel_0::ext_sz);
    return (bm_is_set(pmap, i));
}

inline bool extlink_t::is_clr(int i) const
{
    return (! is_set(i));
}

inline int extlink_t::first_set(int start) const
{
    return bm_first_set(pmap, smlevel_0::ext_sz, start);
}

inline int extlink_t::first_clr(int start) const
{
    return bm_first_clr(pmap, smlevel_0::ext_sz, start);
}

inline int extlink_t::num_set() const
{
    return bm_num_set(pmap, smlevel_0::ext_sz);
}

inline int extlink_t::num_clr() const
{
    return bm_num_clr(pmap, smlevel_0::ext_sz);
}

#endif /* VOL_H */

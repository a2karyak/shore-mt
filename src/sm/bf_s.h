/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

/*
 *  $Id: bf_s.h,v 1.21 1996/05/06 20:10:07 nhall Exp $
 */
#ifndef BF_S_H
#define BF_S_H

struct page_s;

class bfpid_t : public lpid_t {
public:
    NORET			bfpid_t();
    NORET			bfpid_t(const lpid_t& p);
    bfpid_t& 			operator=(const lpid_t& p);
    bool 		        operator==(const bfpid_t& p) const;
};

inline NORET
bfpid_t::bfpid_t()
{
}

inline NORET
bfpid_t::bfpid_t(const lpid_t& p) : lpid_t(p)
{
}

inline bfpid_t&
bfpid_t::operator=(const lpid_t& p)  
{
    *(lpid_t*)this = p;
    return *this;
}

inline bool 
bfpid_t::operator==(const bfpid_t& p) const
{
    return vol() == p.vol() && page == p.page;
}


/*
 *  bfcb_t: buffer frame control block.
 */
struct bfcb_t {
public:
    NORET       bfcb_t()    {};
    NORET       ~bfcb_t()   {};

    inline void	clear();

    w_link_t    link;           // used in hash table

    bfpid_t	pid;            // page currently stored in the frame
    bfpid_t     old_pid;        // previous page in the frame
    bool        old_pid_valid;  // is the previous page in-transit-out?
    page_s*     frame;          // pointer to the frame

    bool	dirty;          // TRUE if page is dirty
    lsn_t       rec_lsn;        // recovery lsn

#if defined(OBJECT_CC) && defined(MULTI_SERVER)
    smutex_t    recmap_mutex;           // protection for "recmap" and remote_io
    u_char      recmap[smlevel_0::recmap_sz];	// map of available recs
    int         remote_io;              // > 0 if there are outstanding rem IOs
#endif

    int4        pin_cnt;        // count of pins on the page
    latch_t     latch;          // latch on the frame
    scond_t     exit_transit;   //signaled when frame exits the in-transit state

    uint4       refbit;         // ref count (for strict clock algorithm)
				// for replacement policy only

    uint4       hot;         	// keeps track of how many sweeps a page should
				// remain dirty in the buffer pool (skipped by
				// the cleaner) because it is just going to be
				// dirtied again soon

    inline ostream&    print_frame(ostream& o, int nslots, bool in_htab);

private:
    // disabled
    NORET       bfcb_t(const bfcb_t&);
    bfcb_t&     operator=(const bfcb_t&);
};


#if !(defined(OBJECT_CC) && defined(MULTI_SERVER))
#define nslots /*not used*/
#endif
inline ostream&
bfcb_t::print_frame(ostream& o, int nslots, bool in_htab)
#undef nslots
{
    if (in_htab) {
        o << pid << '\t'
	  << (dirty ? "dirty" : "clean") << '\t'
	  << rec_lsn << '\t'
	  << pin_cnt << '\t'
	  << latch_t::latch_mode_str[latch.mode()] << '\t'
	  << latch.lock_cnt() << '\t'
	  << latch.is_hot() << '\t'
	  << latch.id() << '\t'
#if defined(OBJECT_CC) && defined(MULTI_SERVER)
	  << remote_io
#endif
	  << endl;

#if defined(OBJECT_CC) && defined(MULTI_SERVER)
	o << "RECMAP: ";
	for (int i = 0; i < nslots; i++) {
	    if (bm_is_set(recmap, i))	o << '1';
	    else			o << '0';
	}
	o << " dummy: "
	  << (bm_is_set(recmap, smlevel_0::max_recs-1) ? '1' : '0')
	  << endl << flush;
#endif
    } else {
	o << pid << '\t' 
	  << " InTransit " << (old_pid_valid ? (lpid_t)old_pid : lpid_t::null)
#if defined(OBJECT_CC) && defined(MULTI_SERVER)
	  << " remote_io=" << remote_io
#endif
	  << endl << flush;
    }
    return o;
}


inline void
bfcb_t::clear() 
{
    pid = lpid_t::null;
    old_pid = lpid_t::null;
    old_pid_valid = false;
    dirty = false;
    rec_lsn = lsn_t::null;
    hot = 0;
    refbit = 0;
#if defined(OBJECT_CC) && defined(MULTI_SERVER)
    remote_io = 0;
    bm_zero(recmap, smlevel_0::max_recs);
#endif
    w_assert3(pin_cnt == 0);
    w_assert3(latch.num_holders() == 0);
}


#if defined(OBJECT_CC) && defined(MULTI_SERVER)

struct cb_race_t {
		cb_race_t() {}
		~cb_race_t() { link.detach(); }

    w_link_t	link;
    rid_t	rid;
};
#endif /* OBJECT_CC && MULTI_SERVER */

#endif /*BF_S_H*/

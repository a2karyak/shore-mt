/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

/*
 *  $Id: bf.h,v 1.69 1996/07/01 22:07:16 nhall Exp $
 */
#ifndef BF_H
#define BF_H

#include <bf_s.h>
#include <page_s.h>

#ifdef __GNUG__
#pragma interface
#endif

#include <lock_s.h>

struct bfcb_t;
class page_p;
class bf_core_m;
class bf_cleaner_thread_t;
class bf_filter_t;

/*********************************************************************
 *
 *  class bf_m
 *
 *  Buffer Manager.  
 *
 *  Note: everything in bf_m is static since there is only one
 *        buffer manager.  
 *
 *********************************************************************/

class bf_m : public smlevel_0 {
public:
    NORET			bf_m(uint4 max, char *bf, uint4 stratgy);
    NORET			~bf_m();

    static int			shm_needed(int n);
    
    static int			npages();
    static int			total_fix();

    static void			mutex_acquire();
    static void			mutex_release();

    static bool			has_frame(const lpid_t& pid);
    static bool			is_cached(const bfcb_t* e);

    static rc_t 		fix(
	page_s*&		    page,
	const lpid_t&		    pid, 
	uint2			    tag,
	latch_mode_t 		    mode,
	bool 			    no_read,
	bool 			    ignore_store_id = FALSE);

    static rc_t 		fix_if_cached(
	page_s*&		    page,
	const lpid_t& 		    pid,
	latch_mode_t 		    mode);

    static void 		refix(
	const page_s* 		    p,
	latch_mode_t 		    mode);

    static rc_t                 get_page(
        const lockid_t&             name,
        bfcb_t*                     b,
        uint2                       ptag,
        bool                        no_read,
        bool                        ignore_store_id,
        bool                        parallel_read,
        bool                        detect_race,
        bool                        is_new);

#ifdef OBJECT_CC
    static rc_t			fix_record(
	page_s*&		    page,
	const rid_t&		    rid,
	latch_mode_t		    mode);

    static bool                 is_partially_cached(const page_s* page);
    static rc_t			discard_record(
	const rid_t&		    rid,
	bool			    aborting);
    static rc_t			discard_many_records(
	const lpid_t&		    pid,
	u_char*			    dirty_map);

#endif

    // upgrade page latch, only if would not block
    // set would_block to true if upgrade would block
    static void 		upgrade_latch_if_not_block(
	const page_s* 		    p,
	bool& 			    would_block);

    static void			unfix(
	const page_s*&		    buf, 
	bool			    dirty = FALSE,
	int			    refbit = 1);

    static void 		unfix_dirty(
	const page_s*&		    buf, 
    	int			    refbit = 1) {
	unfix(buf, true, refbit); 
    }

    static rc_t			set_dirty(const page_s* buf);
    static bool			is_dirty(const page_s* buf) ;
    static void			set_clean(const lpid_t& pid);

#ifdef MULTI_SERVER
    static rc_t			discard_page(const lpid_t& pid);
#endif /*MULTI_SERVER*/

    static rc_t			discard_store(stid_t stid);
    static rc_t			discard_volume(vid_t vid);
    static rc_t			discard_all();
    
    static rc_t			force_store(
	stid_t 			    stid,
	bool 			    flush = false);
    static rc_t			force_page(
	const lpid_t& 		    pid,
	bool 			    flush = false);
    static rc_t			force_until_lsn(
	const lsn_t& 		    lsn,
	bool 			    flush = false);
    static rc_t			force_all(bool flush = false);
    static rc_t			force_volume(
	vid_t 			    vid, 
	bool 			    flush = false);

    static bool 		is_bf_page(const page_s* p);
    static bfcb_t*              get_cb(const page_s*);

    static void 		dump();
    static void 		stats(
	u_long& 		    fixes, 
	u_long& 		    unfixes,
	bool 			    reset);

    static void 		snapshot(
	u_int& 			    ndirty, 
	u_int& 			    nclean,
	u_int& 			    nfree, 
	u_int& 			    nfixed);

    static lsn_t		min_rec_lsn();
    static rc_t			get_rec_lsn(
	int 			    start_idx, 
	int 			    count,
	lpid_t			    pid[],
	lsn_t 			    rec_lsn[], 
	int& 			    ret);

    static rc_t			disable_background_flushing();
    static rc_t			enable_background_flushing();
    static void			activate_background_flushing();

#if defined(OBJECT_CC) && defined(MULTI_SERVER)
    static w_hash_t<cb_race_t, rid_t>*	race_tab;
#endif

private:
    static bf_core_m*		_core;

#ifdef MULTI_SERVER
    static rc_t			_discard_pinned_page(bfcb_t *b, const lpid_t &pid);
#endif /*MULTI_SERVER*/

    static rc_t                 _scan(
        const bf_filter_t&          filter,
        bool                      write_dirty,
        bool                      discard);
    
    static rc_t			_get_page(
	const lockid_t& 	    name, 
	bfcb_t* 		    b,
	uint2			    ptag,
	bool			    no_read,
	bool			    ignore_store_id,
	bool			    parallel_read,
	bool			    detect_race,
	bool                        is_new);
    static rc_t 		_write_out(bfcb_t** b, int cnt);
    static rc_t			_replace_out(bfcb_t* b);

    static bf_cleaner_thread_t*	_cleaner_thread;

    static rc_t			_clean_buf(
	int 			    mincontig, 
	int 			    count, 
	lpid_t 			    pids[],
	int4_t			    timeout,
	bool*			    retire_flag);

    // more stats
    static void 		_incr_fixes();
    static void 		_incr_unfixes();
    static void 		_incr_replaced(bool dirty);
    static void 		_incr_page_write(int number, bool bg);
    static void 		_incr_log_flush_lsn();
protected:
    static void 		_incr_log_flush_all();
    static void 		_incr_sweeps();
    static void 		_incr_await_clean();
    static int			_dirty_threshhold;
    static int			_ndirty;

public:
    static int			_strategy;

    friend class bf_cleaner_thread_t;
};

inline void
bf_m::_incr_page_write(int number, bool bg)
{
    switch(number) {
    case 1:
	smlevel_0::stats.bf_one_page_write++;
	break;
    case 2:
	smlevel_0::stats.bf_two_page_write++;
	break;
    case 3:
	smlevel_0::stats.bf_three_page_write++;
	break;
    case 4:
	smlevel_0::stats.bf_four_page_write++;
	break;
    case 5:
	smlevel_0::stats.bf_five_page_write++;
	break;
    case 6:
	smlevel_0::stats.bf_six_page_write++;
	break;
    case 7:
	smlevel_0::stats.bf_seven_page_write++;
	break;
    case 8:
	smlevel_0::stats.bf_eight_page_write++;
	break;
    }
    if(bg) {
	smlevel_0::stats.bf_write_out += number;
    } else {
	smlevel_0::stats.bf_replace_out += number;
    }
}
inline void
bf_m::_incr_replaced(bool dirty)
{
    if(dirty) {
	smlevel_0::stats.bf_replaced_dirty++;
    } else {
	smlevel_0::stats.bf_replaced_clean++;
    }
}
inline void
bf_m::_incr_log_flush_all()
{
	smlevel_0::stats.bf_log_flush_all++;
}
inline void
bf_m::_incr_log_flush_lsn()
{
	smlevel_0::stats.bf_log_flush_lsn++;
}
inline void
bf_m::_incr_sweeps()
{
	smlevel_0::stats.bf_cleaner_sweeps++;
}

inline void
bf_m::_incr_fixes()
{
	smlevel_0::stats.page_fix_cnt++;
}

inline void
bf_m::_incr_unfixes()
{
	smlevel_0::stats.page_unfix_cnt++;
}

inline void
bf_m::_incr_await_clean()
{
	smlevel_0::stats.bf_await_clean++;
}


#endif  /* BF_H */


/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

/*
 *  $Id: sm_base.h,v 1.90 1996/07/17 22:32:57 nhall Exp $
 */
#ifndef SM_BASE_H
#define SM_BASE_H

#ifdef __GNUG__
#pragma interface
#endif

#include <limits.h>
#include "option.h"
#include "opt_error_def.h"
/*
#include <debug.h>
#include "smstats.h"
*/

class ErrLog;
class sm_stats_info_t;

class device_m;
class io_m;
class bf_m;
class cache_m;
class comm_m;
class log_m;
class remote_client_m;
class callback_m;
class lock_m;
class remote_lock_m;

class option_t;


class smlevel_0 : public w_base_t {
public:
    enum { 
	page_sz = SM_PAGESIZE,	// page size (SM_PAGESIZE is set by makemake)
	ext_sz = 8,		// extent size
	max_exts = max_int2,	// max no. extents, must fit extnum_t
	max_devname = _POSIX_PATH_MAX,	// max length of unix path name
	max_vols = 20,		// max mounted volumes
	max_xcts = 20,		// max active transactions
	max_servers = 15,       // max servers to be connected with
	max_keycomp = 20,	// max key component (for btree)
	max_openlog = 8,	// max # log partitions
	max_copytab_sz = 32749,
	max_dir_cache = max_vols * 10,

	max_rec_len = (1<<31)-1,// max length of a record

	srvid_map_sz = (max_servers - 1) / 8 + 1,
	ext_map_sz_in_bytes = ((ext_sz - 1)/8 + 1),

	dummy = 0
    };

    /*
     * max_recs is the maximum number of records in a file page.
     * Note: the last slot is reserved for a "dummy" record which is used
     * when calling back after an IX or SIX page lock request.
     */
    enum { max_recs = 256, recmap_sz = max_recs/8 };

    typedef vote_t	xct_vote_t;
    typedef int	coord_handle_t; //for now  (not implemented)

    enum switch_t {
	ON = 1,
	OFF = 0
    };

    // shorthand for basics.h CompareOp
    enum cmp_t { bad_cmp_t=badOp, eq=eqOp,
                 gt=gtOp, ge=geOp, lt=ltOp, le=leOp };

    enum store_t { t_bad_store_t, t_index, t_file,
		   t_lgrec, // t_lgrec is used for storing large record
			    // pages and is always associated with some
			    // t_file store
		   t_1page, // for special store holding 1-page btrees 
		  };
    
    // types of indexes
    enum ndx_t { 
	t_bad_ndx_t,		// illegal value
	t_btree,		// B+tree with duplicates
	t_uni_btree,		// unique-keys btree
	t_rtree,		// R*tree
	t_rdtree, 		// russion doll tree (set index)
	t_lhash 		// linear hashing (not implemented)
    };

    // locking granularity options
    enum concurrency_t {
	t_cc_bad,		// this is an illegal value
	t_cc_none,		// no locking
	t_cc_record,		// record-level
	t_cc_page,		// page-level
	t_cc_file,		// file-level
	t_cc_vol,
	t_cc_kvl,		// key-value
	t_cc_im, 		// ARIES IM, not supported yet
	t_cc_append, 		// append-only with scan_file_i
    };

    static concurrency_t cc_alg;	// concurrency control algorithm
    static bool		 cc_adaptive;	// is PS-AA (adaptive) algorithm used?

#include <e_error.h>
    static const w_error_info_t error_info[];
	static void init_errorcodes();

    static device_m* dev;
    static io_m* io;
    static bf_m* bf;
#ifndef MULTI_SERVER
    static lock_m* lm;
#else
    static lock_m* llm;
    static remote_lock_m* rlm;
    static remote_lock_m* lm;	// alias for rlm
#endif

    static comm_m* comm;
    static log_m* log;
    static remote_client_m* rm;
#ifdef MULTI_SERVER
    static cache_m* cm;
    static callback_m* cbm;
#endif /* MULTI_SERVER */

    static ErrLog* errlog;
#ifdef DEBUG
    static ErrLog* scriptlog;
#endif
    static sm_stats_info_t &stats;

    static bool	shutdown_clean;
    static bool	shutting_down;
    static bool	in_recovery;

    // These variables control the size of the log.
    static uint4 max_logsz; 		// max log file size

    // This variable controls checkpoint frequency.
    // Checkpoints are taken every chkpt_displacement bytes
    // written to the log.
    static uint4 chkpt_displacement;

    // The volume_format_version is used to test compatability
    // of software with a volume.  Whenever a change is made
    // to the SM software that makes it incompatible with
    // previouly formatted volumes, this volume number should
    // be incremented.  The value is set in sm.c.
    static uint4 volume_format_version;

    // This is a zeroed page for use wherever initialized memory
    // is needed.
    static char zero_page[page_sz];

    // option for controlling background buffer flush thread
    static option_t* _backgroundflush;

};

enum {
	eINTERNAL = fcINTERNAL,
	eOS = fcOS,
	eOUTOFMEMORY = fcOUTOFMEMORY,
	eNOTFOUND = fcNOTFOUND,
	eNOTIMPLEMENTED = fcNOTIMPLEMENTED,
};

typedef w_rc_t	rc_t;

class smlevel_1 : public smlevel_0 {
public:
    enum xct_state_t {
	xct_stale, xct_active, xct_prepared, 
	xct_aborting, xct_chaining, 
	xct_committing, xct_ended
    };
};

class btree_m;
class file_m;
class rtree_m;
class rdtree_m;

enum sm_store_property_t {
    t_regular 	= 0x0,
    t_no_log 	= 0x1,
    t_temporary	= 0x2,
};

inline ostream&
operator<<(ostream& o, sm_store_property_t p)
{
    return o << (p == t_regular ? "regular" : 
		 (p == t_no_log ? "no_log" :
		  (p == t_temporary ? "temporary" : "")));
}

class smlevel_2 : public smlevel_1 {
public:
    static btree_m* bt;
    static file_m* fi;
    static rtree_m* rt;
    static rdtree_m* rdt;

    // ID of special volume store used for small (ie. 1 page)
    // files and btrees
    static snum_t    small_store_id;
};

class chkpt_m;
class dir_m;
class lid_m;

class smlevel_3 : public smlevel_2 {
public:
    static chkpt_m*	chkpt;
    static dir_m*	dir;
    static lid_m*	lid;
};

class ss_m;
class remote_m;


class smlevel_4 : public smlevel_3 {
public:
    static ss_m*	SSM;	// we will change to lower case later
    static remote_m*	remote;
};

typedef smlevel_4 smlevel_top;
enum RC { eNOERROR = 0,
	  eFAILURE = -1 };

class xct_t;

class xct_dependent_t {
public:
    virtual NORET		~xct_dependent_t();

    virtual void		xct_state_changed(
	smlevel_1::xct_state_t	    old_state,
	smlevel_1::xct_state_t	    new_state) = 0;

protected:
    NORET			xct_dependent_t(xct_t* xd); 
private:
    friend class xct_t;
    w_link_t			_link;
};

#endif /*SM_BASE_H*/

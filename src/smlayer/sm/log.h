/*<std-header orig-src='shore' incl-file-exclusion='LOG_H'>

 $Id: log.h,v 1.77 1999/12/10 05:29:43 bolo Exp $

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

#ifndef LOG_H
#define LOG_H

#include "w_defines.h"

/*  -- do not edit anything above this line --   </std-header>*/

#include <spin.h>
#undef ACQUIRE

#ifdef __GNUG__
#pragma interface
#endif


class logrec_t;
class log_buf;

/*
 *  NOTE:  If C++ had interfaces (as opposed to classes, which
 *  include the entire private definition of the class), this 
 *  separation of log_m from srv_log wouldn't be necessary.
 *  
 *  It would be nice not to have the extra indirection, but the
 *  if we put the implementation in log_m, in order to compile
 *  the sm, the entire world would have to know the entire implementation.
 */


class log_base : public smlevel_0 {
    friend 		class log_i;

public:
    static lsn_t	first_lsn(uint4_t pnum);

    enum { XFERSIZE =	SM_PAGESIZE };
    // const int XFERSIZE =	SM_PAGESIZE;

    enum { invalid_fhdl = -1 };

    virtual void                check_wal(const lsn_t &ll) ;
    virtual void                compute_space() ;
    virtual
    NORET			~log_base();
protected:
    NORET			log_base(char *shmbase);
private:
    // disabled
    NORET			log_base(const log_base&);
    // log_base& 		operator=(const log_base&);


public:
    //////////////////////////////////////////////////////////////////////
    // This is an abstract class; represents interface common to client
    // and server sides
    //////////////////////////////////////////////////////////////////////
#define VIRTUAL(x) virtual x = 0;
#define NULLARG = 0


#define COMMON_INTERFACE\
    VIRTUAL(rc_t		insert(logrec_t& r, lsn_t* ret))\
    VIRTUAL(rc_t		compensate(const lsn_t &r, const lsn_t &u))\
    VIRTUAL(rc_t		fetch(                  \
	lsn_t& 		    	    lsn,                \
	logrec_t*& 		    rec,                \
	lsn_t* 			    nxt NULLARG ) )     \
    VIRTUAL(rc_t		flush(const lsn_t& l))  \
    VIRTUAL(rc_t		scavenge(               \
	const lsn_t& 		    min_rec_lsn,        \
	const lsn_t& 		    min_xct_lsn)) 	

   
    VIRTUAL(void 		 set_master(
	const lsn_t& 		    lsn,
	const lsn_t&		    min_rec_lsn,
        const lsn_t&		    min_xct_lsn))

    COMMON_INTERFACE

#undef VIRTUAL
#undef NULLARG


    //////////////////////////////////////////////////////////////////////
    // SHARED DATA:
    // All data members are shared between log processes. 
    // This was supposed to be put in shared memory and it 
    // was supposed to support a multi-process log manager.
    //////////////////////////////////////////////////////////////////////
protected:
    //w_shmem_t			_shmem_seg;
    // TODO: remove

    struct _shared_log_info {
	bool			_log_corruption_on;
	fileoff_t _max_logsz;	// input param from cli -- partition size
	fileoff_t  _maxLogDataSize;// _max_logsz - sizeof(skiplog record)

	lsn_t			_curr_lsn;	// lsn of next record
	lsn_t			_append_lsn;    // max lsn appended to file 
	lsn_t			_durable_lsn;	// max lsn synced to disk

	///////////////////////////////////////////////////////////////////////
	// set and used for restart and checkpoints
	///////////////////////////////////////////////////////////////////////
	lsn_t			_master_lsn;	// lsn of most recent chkpt
	lsn_t			_old_master_lsn;// lsn of 2nd most recent chkpt
	//    min rec_lsn of dirty page list in the most recent chkpt
	lsn_t			_min_chkpt_rec_lsn; 


	///////////////////////////////////////////////////////////////////////
	// a number computed occasionally, used for log-space computations
	// computed by client using info in partitions
	///////////////////////////////////////////////////////////////////////
	fileoff_t		_space_available;// in freeable or freed partitions

    };
    struct _shared_log_info  *	_shared;
public:
    void			start_log_corruption() { _shared->_log_corruption_on = true; }
    lsn_t 			master_lsn()	{ return _shared->_master_lsn; }
    lsn_t			old_master_lsn(){ return _shared->_old_master_lsn; }
    lsn_t			min_chkpt_rec_lsn() { return _shared->_min_chkpt_rec_lsn; }
    const lsn_t& 		curr_lsn() const	{ return _shared->_curr_lsn; }
    const lsn_t& 		durable_lsn() const	{ return _shared->_durable_lsn; }


    /*********************************************************************
     *
     *  log_base::global_min_lsn()
     *  log_base::global_min_lsn(a)
     *  log_base::global_min_lsn(a,b)
     *
     *  Returns the lowest lsn of a log record that's still needed for
     *  any reason, namely the smallest of the arguments (if present) and
     *  the  _master_lsn and  _min_chkpt_rec_lsn.  
     *  Used to scavenge log space, among other things. 
     *
     *********************************************************************/
    const lsn_t 		global_min_lsn() const {
				    lsn_t lsn = 
				    MIN(_shared->_master_lsn, _shared->_min_chkpt_rec_lsn);
				    return lsn;
				}

    const lsn_t 		global_min_lsn(const lsn_t& a) const {
				    lsn_t lsn = global_min_lsn(); 
				    lsn = MIN(lsn, a);
				    return lsn;
				}

    const lsn_t 		global_min_lsn(const lsn_t& min_rec_lsn, 
						const lsn_t& min_xct_lsn) const {
				    lsn_t lsn =  global_min_lsn();
				    lsn = MIN(lsn, min_rec_lsn);
				    lsn = MIN(lsn, min_xct_lsn);
				    return lsn;
				}
public:
    ////////////////////////////////////////////////////////////////////////
    // MISC:
    ////////////////////////////////////////////////////////////////////////
    enum { max_open_log = smlevel_0::max_openlog };
    // const	max_open_log = smlevel_0::max_openlog;

    ////////////////////////////////////////////////////////////////////////
    // check_raw_device: static and used elsewhere in sm. Should really
    // be a stand-alone function in fc/
    ////////////////////////////////////////////////////////////////////////
    static rc_t			check_raw_device(
				    const char* devname,
				    bool&	raw
				    );
    

    static uint4_t		version_major;
    static uint4_t		version_minor;

    static rc_t			check_version(
				    uint4_t	major,
				    uint4_t	minor
				    );
    static rc_t			parse_master_chkpt_string(
				    istream& 		s,
				    lsn_t&		master_lsn,
				    lsn_t&		min_chkpt_rec_lsn,
				    int&		number_of_others,
				    lsn_t*		others,
				    bool&		old_style
				    );
    static rc_t			parse_master_chkpt_contents(
				    istream&	    s,
				    int&		    listlength,
				    lsn_t*		    lsnlist
				    );
    static void			create_master_chkpt_string(
				    ostream&		o,
				    int			arraysize,
				    const lsn_t*	array,
				    bool		old_style = false
				    );
    static void			create_master_chkpt_contents(
				    ostream&	s,
				    int		arraysize,
				    const lsn_t*	array
				    );
};

inline lsn_t 
log_base::first_lsn(uint4_t pnum) { 
    return lsn_t(pnum, 0); 
}


struct log_stats_t
{
    w_base_t::uint8_t 	log_reads;		// number of log reads
    w_base_t::uint8_t 	log_bytes_read;		// log bytes read
    w_base_t::uint8_t 	log_writes;		// number of log writes
    w_base_t::uint8_t 	log_bytes_written;	// log bytes written
};


class log_m : public log_base {

    NORET log_m(const char *path, 
	fileoff_t max_logsz, 
	int rdlogbufsize,
	int wrlogbufsize,
	char *shmbase,
	bool reformat);

public:
    void 	check_wal(const lsn_t &) ;
    void 	compute_space() ;
    rc_t   	flush_all() { return flush(curr_lsn()); }
    static int  shm_needed(int n);
	

    static	rc_t	new_log_m(log_m		*&the_log,
				  const char	*path,
				  fileoff_t	max_logsz,
				  int		rdlogbufsize,
				  int		wrlogbufsize,
				  char		*shmbase,
				  bool		reformat);
    NORET ~log_m();

#define VIRTUAL(x) x;
#define NULLARG
    COMMON_INTERFACE
#undef VIRTUAL
#undef NULLARG

    fileoff_t 		 space_left() { return _shared->_space_available; }
			
    void 		 set_master(
	const lsn_t& 		    lsn,
	const lsn_t&		    min_rec_lsn,
        const lsn_t&		    min_xct_lsn);

    rc_t		wait( 
	fileoff_t 		nbytes,
	sevsem_t&		sem,
	timeout_in_ms		timeout = WAIT_FOREVER);

    /*
     * Logging stats
     */
    void 		release(); // release mutex acquired by fetch()
    w_rc_t 		acquire(); // reacquire mutex acquired by fetch()

    static log_stats_t	stats;	// log io statistics

    fileoff_t		limit() const;

private:
    rc_t		_update_shared();

    void		acquire_var_mutex();
    void		release_var_mutex();
    smutex_t		_var_mutex;
    void		acquire_mutex();
    smutex_t		_mutex;
    log_base*           _peer; // srv_log *
	
    ///////////////////////////////////////////////////////////////////////
    // Kept entirely on client side:
    ///////////////////////////////////////////////////////////////////////
    fileoff_t		_countdown;	// # bytes to go
    sevsem_t*		_countdown_expired;
};



class log_i {
public:
    NORET			log_i(log_m& l, const lsn_t& lsn) ;
    NORET			~log_i();

    bool 			next(lsn_t& lsn, logrec_t*& r);
    w_rc_t&			get_last_rc();
private:
    log_m&			log;
    lsn_t			cursor;
    w_rc_t			last_rc;
};

inline NORET
log_i::log_i(log_m& l, const lsn_t& lsn) 
    : log(l), cursor(lsn)
{
    DBG(<<"creating log_i with cursor " << lsn);
}

inline
log_i::~log_i()
{
    last_rc.verify();
}

inline w_rc_t&
log_i::get_last_rc()
{
    return last_rc;
}

/*<std-footer incl-file-exclusion='LOG_H'>  -- do not edit anything below this line -- */

#endif          /*</std-footer>*/

/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

/*
 *  $Id: log.h,v 1.47 1996/07/18 14:43:03 kupsch Exp $
 */
#ifndef LOG_H
#define LOG_H

#include <w_shmem.h>
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


class log_base : public smlevel_1 {
    friend 	class log_i;


protected:

    NORET			log_base(
	int rdbufsize,
	int wrbufsize,
	char *shmbase
	);
public:
    virtual void                check_wal(const lsn_t &ll) ;
    log_buf *			writebuf() { return _writebuf; }
    int				writebufsize() const { return _wrbufsize; }
    char *			readbuf() { return _readbuf; }
    int 			readbufsize() const { return _rdbufsize; }

    virtual
    NORET			~log_base();

private:
    // disabled
    NORET			log_base(const log_base&);
    // log_base& 			operator=(const log_base&);

public:
    const			max_open_log = smlevel_0::max_openlog;

    static rc_t			check_raw_device(
				    const char* devname,
				    bool&	raw
				    );

    lsn_t 			master_lsn()	{ return _shared->_master_lsn; }
    lsn_t			old_master_lsn(){ return _shared->_old_master_lsn; }
    lsn_t			min_chkpt_rec_lsn() { return _shared->_min_chkpt_rec_lsn; }
    const lsn_t& 		curr_lsn() const	{ return _shared->_curr_lsn; }
    const lsn_t& 		durable_lsn() const	{ return _shared->_durable_lsn; }
    const lsn_t 		global_min_lsn() const;
    const lsn_t 		global_min_lsn(const lsn_t&) const;
    const lsn_t 		global_min_lsn(const lsn_t&, const lsn_t&) const;

    rc_t		        flush_all() { return flush(curr_lsn()); }

    void			start_log_corruption() {
					_log_corruption_on = true;
				}


    //////////////////////////////////////////////////////////////////////
    // This is an abstract class; represents interface common to client
    // and server sides
    //////////////////////////////////////////////////////////////////////
#define VIRTUAL(x) virtual x = 0;
#define NULLARG = 0


#define COMMON_INTERFACE\
    VIRTUAL(rc_t		insert(logrec_t& r, lsn_t* ret))\
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

protected:
    int 			_rdbufsize;
    int 			_wrbufsize;
    char*   			_readbuf;  
    log_buf*   			_writebuf;  
    bool			_log_corruption_on;

    //////////////////////////////////////////////////////////////////////
    // All data members are
    // shared between client(sm) and server(diskrw) side
    //////////////////////////////////////////////////////////////////////
    //w_shmem_t			_shmem_seg;
    // TODO: remove

    struct _shared_log_info {
	uint4			_max_logsz;	// input param from cli -- partition size
	uint4			_maxLogDataSize;// _max_logsz - sizeof(skiplog record)

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
	uint4_t			_space_available;// in freeable or freed partitions

    };
    struct _shared_log_info  *	_shared;
};



class log_m : public log_base {

public:
           void check_wal(const lsn_t &) ;
    static void reset_stats();
    static int shm_needed(int n);
	
    NORET log_m(const char *path, 
	uint4 max_logsz, 
	int rdlogbufsize,
	int wrlogbufsize,
	char *shmbase,
	bool reformat);

    NORET ~log_m();

#define VIRTUAL(x) x;
#define NULLARG
    COMMON_INTERFACE
#undef VIRTUAL
#undef NULLARG

    uint4_t 		 space_left() { return _shared->_space_available; }
			
    void 		 set_master(
	const lsn_t& 		    lsn,
	const lsn_t&		    min_rec_lsn,
        const lsn_t&		    min_xct_lsn);

    rc_t		wait( 
	uint4_t 		nbytes,
	sevsem_t&		sem,
	int4_t 			timeout = WAIT_FOREVER);

    /*
     * Logging stats
     */
    static void 	incr_log_rec_cnt() {
				smlevel_0::stats.log_records_generated++;
    }
    static void 	incr_log_byte_cnt(int cnt) {
				smlevel_0::stats.log_bytes_generated += cnt;
    }
    static void 	incr_log_sync_cnt(unsigned int lsncnt, unsigned int reccnt);

    void 		release(); // release mutex acquired by fetch()
    w_rc_t 		acquire(); // reacquire mutex acquired by fetch()

    void		SetInRecovery(bool b)  {
			    _in_recovery = b;
			};
    bool		IsInRecovery()  {
			    return _in_recovery;
			};

private:
    rc_t		_update_shared();

    smutex_t		_mutex;
    log_base*           _peer; // srv_log *
    bool		_in_recovery;	// true if in recovery phase
	
    ///////////////////////////////////////////////////////////////////////
    // Kept entirely on client side:
    ///////////////////////////////////////////////////////////////////////
    uint4_t		_countdown;	// # bytes to go
    sevsem_t*		_countdown_expired;
};


class log_buf {

private:
    char 	*buf;
    const int   Nblocks  = 8;

    lsn_t 	_lsn_firstbyte; // of first byte in the buffer(not necessarily
			     // the beginning of a log record)
    lsn_t 	_lsn_flushed; // we've written to disk up to (but not including) 
			    // this lsn  -- lies between lsn_firstbyte and
			    // lsn_next 
    lsn_t 	_lsn_nextrec; // of next record to be buffered
    lsn_t 	_lsn_lastrec; // last record inserted 
    bool        _durable;     // true iff everything buffered has been
			     // flushed to disk
    bool        _written;     // true iff everything buffered has been
			     // written to disk (for debugging)

    uint4	_len; // logical end of buffer
    uint4	_bufsize; // physical end of buffer

    /* NB: the skip_log is not defined in the class because it would
     necessitate #include-ing all the logrec_t definitions and we
     don't want to do that, sigh.  */

    char	*___skip_log;	// memory for the skip_log
    char	*__skip_log;	// properly aligned skip_log

    void		init(const lsn_t &f, const lsn_t &n, bool, bool); 

public:

    const int XFERSIZE =	8192;

    			log_buf(char *, int sz);
    			~log_buf();

    const lsn_t	&	firstbyte() const { return _lsn_firstbyte; }
    const lsn_t	&	flushed() const { return _lsn_flushed; }
    const lsn_t	&	nextrec() const { return _lsn_nextrec; }
    const lsn_t	&	lastrec() const { return _lsn_lastrec; }
    bool        	durable() const { return _durable; }
    bool        	written() const { return _written; }
    uint4         	len() const { return _len; }
    uint4 		bufsize() const { return _bufsize; }

    bool		fits(const logrec_t &l) const;
    void 		prime(int, off_t, const lsn_t &);
    void 		insert(const logrec_t &r);
    void 		insertskip();
    void 		mark_durable() { _durable = true; }

    void 		write_to(int fd);

    friend ostream&     operator<<(ostream &, const log_buf &);
};


class log_i {
public:
    NORET			log_i(log_m& l, const lsn_t& lsn) ;
    NORET			~log_i()	{};

    bool 			next(lsn_t& lsn, logrec_t*& r);
private:
    log_m&			log;
    lsn_t			cursor;
};

inline NORET
log_i::log_i(log_m& l, const lsn_t& lsn) 
    : log(l), cursor(lsn)
{
}


#endif /* LOG_H */


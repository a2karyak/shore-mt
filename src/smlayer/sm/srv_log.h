/*<std-header orig-src='shore' incl-file-exclusion='SRV_LOG_H'>

 $Id: srv_log.h,v 1.31 1999/12/10 05:29:47 bolo Exp $

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

#ifndef SRV_LOG_H
#define SRV_LOG_H

#include "w_defines.h"

/*  -- do not edit anything above this line --   </std-header>*/

#include <log.h>

#ifdef __GNUG__
#pragma interface
#endif


typedef	uint4_t	partition_number_t;
typedef	int	partition_index_t;

typedef enum    {  /* partition_t::_mask values */
	m_exists=0x2,
	m_open_for_read=0x4,
	m_open_for_append=0x8,
	m_flushed=0x10	// has no data cached
} partition_mask_values;

#define CHKPT_META_BUF 512
			

class log_buf; //forward
class srv_log; //forward
class partition_t; //forward


class srv_log : public log_base {
     friend class partition_t;


protected:

    char *		_chkpt_meta_buf;
			// in a log partition.  It maps to the disk
			// address at logfile, partition->start()
			// whereas the physical beginning of partition is
			// at partition->end_lsn()

public:
    void                check_wal(const lsn_t &) ;
    void 		compute_space(); 

    // CONSTRUCTOR -- figures out which srv type to construct
    static rc_t	new_log_m(
		srv_log	*&srv_log_p,		  
		const char *logdir,
		int rdbufsize,
		int wrbufsize,
		char *shmbase,
		bool reformat
	  );
    // NORET 		srv_log(const char *segid); is protected, below

    virtual
    NORET ~srv_log();



#define VIRTUAL(x) x;
#define NULLARG = 0
    COMMON_INTERFACE
#undef VIRTUAL
#undef NULLARG

    ///////////////////////////////////////////////////////////////////////
    // done entirely by server side, in generic way:
    ///////////////////////////////////////////////////////////////////////
    void 		 	set_master(
					const lsn_t& 		    lsn,
					const lsn_t&		    min_rec_lsn,
					const lsn_t&		    min_xct_lsn);

    partition_t *		close_min(partition_number_t n);
				// the defaults are for the case
				// in which we're opening a file to 
				// be the new "current"
    partition_t *		open(partition_number_t n, 
				    const lsn_t&  end_hint,
				    bool existing = false,
				    bool forappend = true,
				    bool during_recovery = false
				); 
    partition_t *		n_partition(partition_number_t n) const;
    partition_t *		curr_partition() const;

    void			set_current(partition_index_t, partition_number_t); 
    void			unset_current(); 

    partition_index_t		partition_index() const { return _curr_index; }
    partition_number_t		partition_num() const { return _curr_num; }
    fileoff_t			limit() const { return _shared->_max_logsz; }
    fileoff_t			logDataLimit() const { return _shared->_maxLogDataSize; }

    virtual
    void			_write_master(const lsn_t& l, const lsn_t& min)=0;

    virtual
    partition_t *		i_partition(partition_index_t i) const = 0;

    void			set_durable(const lsn_t &ll);

protected:
    NORET 			srv_log( int rdbufsize,
				    int wrbufsize,
				    char *shmbase
				);
    void			sanity_check() const;
    partition_index_t		get_index(partition_number_t)const; 
    void 			_compute_space(); 

    // Data members:

    static bool			_initialized;
    static char 		_logdir[max_devname];
    partition_index_t		_curr_index; // index of partition
    partition_number_t		_curr_num;   // partition number

    // log_buf stuff:
public:
    log_buf *			writebuf() { return _writebuf; }
    const log_buf *		writebuf_const() const { return _writebuf; }
    int				writebufsize() const { return _wrbufsize; }
    char *			readbuf() { return _readbuf; }
    int 			readbufsize() const { return _rdbufsize; }
    lsn_t			last_durable_skip() const;
protected:
    int 			_rdbufsize;
    int 			_wrbufsize;
    char*   			_readbuf;  
    log_buf*   			_writebuf;  
};


/* abstract class: */
class partition_t {
	friend class srv_log;

public:
	/* XXX why not inerhit from log_base ??? */
	typedef smlevel_0::fileoff_t fileoff_t;
	enum { XFERSIZE = log_base::XFERSIZE };
	enum { invalid_fhdl = log_base::invalid_fhdl };

	partition_t() :_start(0),
		_index(0), _num(0), _size(0), _mask(0), 
		_owner(0) {}

	virtual
	~partition_t() {};

protected: // these are common to all partition types:
	// const   max_open_log = smlevel_0::max_openlog;
	enum { max_open_log = smlevel_0::max_openlog };

	fileoff_t		_start;
	/* store end lsn at the beginning of each partition; updated
	* when partition closed 
	*/
	fileoff_t		start() const { return _start; }
	lsn_t			first_lsn() const {
				    return log_base::first_lsn(uint4_t(_num));
				}


	partition_index_t	_index;
	partition_number_t 	_num;
	fileoff_t 		_size;
	uint4_t			_mask;
	srv_log			*_owner;
	lsn_t			_last_skip_lsn;

protected:
	fileoff_t		_eop; // physical end of partition
	// logical end of partition is _size;

public:
	// const uint4_t		nosize = max_uint4_t;
	enum { nosize = -1 };

	const log_buf &		writebuf() const { return *_owner->writebuf(); }
	int			writebufsize() const { return _owner->
						writebufsize(); }
	char   *		readbuf() { return _owner->readbuf(); }
	int			readbufsize() const { return _owner->
						readbufsize(); }

	virtual 
	int 			fhdl_app() const = 0;
	virtual 
	int 			fhdl_rd() const = 0;

	void			set_state(uint4_t m) { _mask |= m ; }
	void			clr_state(uint4_t m) { _mask &= ~m ; }

	virtual
	void			_clear()=0;
	void			clear() {  	_num=0;_size=nosize;
						_mask=0; _clear(); }
	void			init_index(partition_index_t i) { _index=i; }

	partition_index_t	index() const {  return _index; }
	partition_number_t	num() const { return _num; }


	fileoff_t		size()const { return _size; }
	void			set_size(fileoff_t v) { 
					_size =  v;
				}

	virtual
	void			open_for_read(partition_number_t n, bool err=true) = 0;

	void			open_for_append(partition_number_t n,
					const lsn_t& end_hint);

	void			skip(const lsn_t &ll, int fd);

	virtual
	w_rc_t			write(const logrec_t &r, const lsn_t &ll) ;

	w_rc_t			read(logrec_t *&r, lsn_t &ll, int fd = invalid_fhdl);

	virtual
	void			close(bool both) = 0;
	void			close();

	virtual
	void			destroy() = 0;

	void			flush(int fd, bool force=false);

	virtual
	void			_flush(int fd)=0;

	virtual
	void			sanity_check() const =0;

	virtual
	bool			exists() const;

	virtual
	bool			is_open_for_read() const;

	virtual
	bool			is_open_for_append() const;

	virtual
	bool			flushed() const;

	virtual
	bool			is_current() const;

	virtual
	void			peek(partition_number_t n, 
					const lsn_t&	end_hint,
					bool, 
					int* fd=0) = 0;

	virtual
	void			set_fhdl_app(int fd)=0;

	void			_peek(partition_number_t n, 
					fileoff_t startloc,
					fileoff_t wholesize,
					bool, int fd);

	const lsn_t&		last_skip_lsn() const { return _last_skip_lsn; }
	void			set_last_skip_lsn(const lsn_t &l) { _last_skip_lsn = l; }
};

inline void			
partition_t::close()
{
    this->close(false); 
}

/*<std-footer incl-file-exclusion='SRV_LOG_H'>  -- do not edit anything below this line -- */

#endif          /*</std-footer>*/

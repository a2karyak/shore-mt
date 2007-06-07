/*<std-header orig-src='shore' incl-file-exclusion='LOG_BUF_H'>

 $Id: log_buf.h,v 1.11 1999/06/07 19:04:15 kupsch Exp $

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

#ifndef LOG_BUF_H
#define LOG_BUF_H

#include "w_defines.h"

/*  -- do not edit anything above this line --   </std-header>*/

#include <w_shmem.h>
#include <spin.h>
#undef ACQUIRE

#ifdef __GNUG__
#pragma interface
#endif

class log_buf {
public:
    typedef smlevel_0::fileoff_t fileoff_t;

private:
    char 	*buf;

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

    fileoff_t	_len; // logical end of buffer
    fileoff_t	_bufsize; // physical end of buffer

    /* NB: the skip_log is not defined in the class because it would
     necessitate #include-ing all the logrec_t definitions and we
     don't want to do that, sigh.  */

    char	*___skip_log;	// memory for the skip_log
    char	*__skip_log;	// properly aligned skip_log

    void		init(const lsn_t &f, const lsn_t &n, bool, bool); 

public:

    enum { XFERSIZE =	log_base::XFERSIZE };
    // const int XFERSIZE = log_base::XFERSIZE;

    			log_buf(char *, fileoff_t sz);
    			~log_buf();

    const lsn_t	&	firstbyte() const { return _lsn_firstbyte; }
    const lsn_t	&	flushed() const { return _lsn_flushed; }
    const lsn_t	&	nextrec() const { return _lsn_nextrec; }
    const lsn_t	&	lastrec() const { return _lsn_lastrec; }
    bool        	durable() const { return _durable; }
    bool        	written() const { return _written; }
    fileoff_t         	len() const { return _len; }
    fileoff_t 		bufsize() const { return _bufsize; }

    bool		fits(const logrec_t &l) const;
    void 		prime(int, fileoff_t, const lsn_t &);
    void 		insert(const logrec_t &r);
    void 		insertskip();
    void 		mark_durable() { _durable = true; }

    void 		write_to(int fd);
    bool 		compensate(const lsn_t &rec, const lsn_t &undo_lsn);

    friend ostream&     operator<<(ostream &, const log_buf &);
};

/*<std-footer incl-file-exclusion='LOG_BUF_H'>  -- do not edit anything below this line -- */

#endif          /*</std-footer>*/

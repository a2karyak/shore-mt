/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

/*
 *  $Id: log_base.c,v 1.6 1996/05/03 04:07:40 kupsch Exp $
 */
#define SM_SOURCE
#define LOG_C
#ifdef __GNUG__
#   pragma implementation
#endif

#include <dirent.h>
#include <sm_int.h>
#include <sys/stat.h>

/* Until/unless we put this into shared memory: */
static struct log_base::_shared_log_info  __shared;

/*********************************************************************
 *
 *  Constructor 
 *  log_base::log_base 
 *
 *********************************************************************/

NORET
log_base::log_base(
	int rdbufsize,
	int wrbufsize,
	char */*shmbase*/
	)
    : 
    _rdbufsize(rdbufsize),
    _wrbufsize(wrbufsize),
    _readbuf(0),
    _writebuf(0),
    _log_corruption_on(false),
    _shared (&__shared)
{
    /*
     * writebuf and readbuf are "allocated"
     * in srv_log constructor
     */

#ifdef notdef
    /* 
     * create shm seg for the shared data members
     */
    w_assert3(_shmem_seg.base()==0);
    if(segid) {
	// server size -- attach to it
	int i;
	istrstream(segid) >> i;
	w_rc_t rc = _shmem_seg.attach(i);
	if(rc) {
	    cerr << "log daemon:-  cannot attach to shared memroy " << segid << endl;
	    cerr << rc;
	    W_COERCE(rc);
	}
    } else {
	// client side -- create it

	// TODO add diskport-style queue
	w_rc_t rc = _shmem_seg.create(sizeof(struct _shared_log_info));

	if(rc) {
	    cerr << "fatal error: cannot create shared memory for log" << endl;
	    W_COERCE(rc);
	}
    }
    _shared = new(_shmem_seg.base()) _shared_log_info;
    w_assert3(_shared != 0);
#endif

    // re-initialize
    _shared->_min_chkpt_rec_lsn = lsn_t(uint4(1),uint4(0));

    DBG(<< "_shared->_min_chkpt_rec_lsn = " 
	<<  _shared->_min_chkpt_rec_lsn);

}

NORET
log_base::~log_base()
{
/*
    if(_shmem_seg.base()) {
	W_COERCE( _shmem_seg.destroy() );
    }
*/
    if(_writebuf) {
	delete _writebuf;
	_writebuf = 0;
    }
    _readbuf = 0;
}

/*********************************************************************
 *
 *  log_base::check_raw_device(devname, raw)
 *
 *  Check if "devname" is a raw device. Return result in "raw".
 *
 *********************************************************************/
rc_t
log_base::check_raw_device(const char* devname, bool& raw)
{
    struct stat     statInfo;

    raw = FALSE;

    if (stat(
#ifdef I860
	(char *)
#endif
	devname, &statInfo) < 0) {
        return RC(eOS);
    }

    /*
     *  if it's a character device, its a raw disk
     */
    if ((statInfo.st_mode & S_IFMT) == S_IFCHR) {
    	raw = TRUE;
    }
    
    return RCOK;
}

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

const lsn_t 		
log_base::global_min_lsn() const
{
    lsn_t lsn = MIN(_shared->_master_lsn, _shared->_min_chkpt_rec_lsn);
    return lsn;
}

const lsn_t 		
log_base::global_min_lsn(const lsn_t& a) const
{
    lsn_t lsn = global_min_lsn();
    lsn = MIN(lsn, a);
    return lsn;
}

const lsn_t 		
log_base::global_min_lsn(const lsn_t& min_rec_lsn, const lsn_t& min_xct_lsn) const
{
    lsn_t lsn =  global_min_lsn();
    lsn = MIN(lsn, min_rec_lsn);
    lsn = MIN(lsn, min_xct_lsn);
    return lsn;
}

/*********************************************************************
 *
 *  log_i::next(lsn, r)
 *
 *  Read the next record into r and return its lsn in lsn.
 *  Return false if EOF reached. True otherwise.
 *
 *********************************************************************/
bool log_i::next(lsn_t& lsn, logrec_t*& r)  
{
    bool eof = (cursor == null_lsn);
    if (! eof) {
	lsn = cursor;
	rc_t rc = log.fetch(lsn, r, &cursor);
	// release right away, since this is only
	// used in recovery.
	log.release();
	if (rc)  {
	    if (rc.err_num() == smlevel_0::eEOF)  
		eof = TRUE;
	    else  {
		smlevel_0::errlog->clog << error_prio 
		<< "Fatal error : " << RC_PUSH(rc, eINTERNAL) << flushl;
	    }
	}
    }
    return ! eof;
}

/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

/*
 *  $Id: log.c,v 1.85 1996/07/18 14:42:57 kupsch Exp $
 */
#define SM_SOURCE
#define LOG_C
#ifdef __GNUG__
#   pragma implementation
#endif

#include <dirent.h>
#include <sm_int.h>
#include <srv_log.h>
#include <logdef.i>
#include <crash.h>

#define DBGTHRD(arg) DBG(<<" th."<<me()->id << " " arg)

NORET
log_m::log_m(
	const char *path,
	uint4 max_logsz,
	int   rdbufsize,
	int   wrbufsize,
	char  *shmbase,
	bool  reformat
    ) : 
    log_base(rdbufsize, wrbufsize, shmbase),
   _mutex("log_m"),
   _peer(0),
   _in_recovery(false),
   _countdown(0), 
   _countdown_expired(0)
{
    /*
     *  make sure logging parameters are something reasonable
     */
    w_assert1(smlevel_0::max_openlog > 0 &&
	      (max_logsz == 0 || max_logsz >= 64*1024 &&
	      smlevel_0::chkpt_displacement >= 64*1024));



    _shared->_curr_lsn =
    _shared->_append_lsn =
    _shared->_durable_lsn = // set_durable(...)
    _shared->_master_lsn =
    _shared->_old_master_lsn = lsn_t::null;

    lsn_t	starting(1,0);
    _shared->_min_chkpt_rec_lsn = starting;

    DBGTHRD(<< "_shared->_min_chkpt_rec_lsn = " 
	<<  _shared->_min_chkpt_rec_lsn);

    _shared->_space_available = 0;
    _shared->_max_logsz = max_logsz; // max size of a partition

    skip_log *s = new skip_log;
    _shared->_maxLogDataSize = max_logsz - s->length();
    delete s;

    /*
     * mimic argv[]
     */
    char arg1[100];
  // TODO remove  char arg2[100];

    ostrstream s1(arg1, sizeof(arg1));
    s1 << path << '\0';

/*
    ostrstream s2(arg2, sizeof(arg2));
    s2 << _shmem_seg.id() << '\0';
*/

    DBGTHRD(<< "_shared->_min_chkpt_rec_lsn = " 
	<<  _shared->_min_chkpt_rec_lsn);

    //_peer = srv_log::new_log_m(s1.str(), s2.str(), reformat);
    _peer = srv_log::new_log_m(s1.str(),  
	rdbufsize, wrbufsize, shmbase,
	reformat);

    DBGTHRD(<< "_shared->_min_chkpt_rec_lsn = " 
	<<  _shared->_min_chkpt_rec_lsn << "\0" );

    w_assert1(_peer);
    w_assert3(curr_lsn() != lsn_t::null);
}

NORET
log_m::~log_m() 
{
    // TODO: replace with RPC
    if(_peer) {
	delete _peer;
	_peer = 0;
    }
}

/*********************************************************************
 *
 *  log_m::wait(nbytes, sem, timeout)
 *
 *  Block current thread on sem until nbytes have been written 
 *  (in which case log_m will signal sem) or timeout expires.
 *  Other threads might also signal sem.
 *
 *********************************************************************/
rc_t
log_m::wait(uint4_t nbytes, sevsem_t& sem, int4_t timeout)
{
    w_assert1(! _countdown_expired);
    _countdown = nbytes;
    _countdown_expired = &sem;
    return _countdown_expired->wait(timeout);
}

/*********************************************************************
 *
 *  log_m::incr_log_sync_cnt(numbytes, numrecs)
 *  log_m::incr_log_byte_cnt(n) -- inline
 *  log_m::incr_log_rec_cnt() -- inline
 *
 *  update global stats during a sync
 *
 *********************************************************************/
void 		
log_m::incr_log_sync_cnt(unsigned int bytecnt, unsigned int reccnt) 
{
    smlevel_0::stats.log_sync_cnt ++;

    if(reccnt == 0) {
	smlevel_0::stats.log_dup_sync_cnt ++;
    } else {
	w_assert1(reccnt > 0); // fatal if < 0

	if(smlevel_0::stats.log_sync_nrec_max < reccnt)
	    smlevel_0::stats.log_sync_nrec_max  = reccnt;

	if(smlevel_0::stats.log_sync_nbytes_max < bytecnt)
	    smlevel_0::stats.log_sync_nbytes_max = bytecnt;

	if(reccnt == 1) {
	    smlevel_0::stats.log_sync_nrec_one++;
	} else {
	    smlevel_0::stats.log_sync_nrec_more++;
	    smlevel_0::stats.log_sync_nrec_not += reccnt;
	}
    }
}

/**********************************************************************
 * RPC log functions:
 **********************************************************************/


void                
log_m::set_master(
    const lsn_t&                lsn, 
    const lsn_t&                min_rec_lsn,
    const lsn_t&                min_xct_lsn)
{
    w_assert1(_peer);
    W_COERCE(_mutex.acquire());
    _peer->set_master(lsn, min_rec_lsn, min_xct_lsn);
    _mutex.release();
}

rc_t                
log_m::insert(logrec_t& r, lsn_t* ret)
{
    w_assert1(_peer);

    FUNC(log_m::insert)
    if (r.length() > sizeof(r)) {
	// the log record is longer than a log record!
	W_FATAL(fcINTERNAL);
    }
    DBGTHRD( << 
	"Insert tx." << r 
	<< " size: " << r.length() << " prevlsn: " << r.prev() );
 
    W_COERCE(_mutex.acquire());
    if (_countdown_expired && _countdown)  {
	/*
	 *  some thread asked for countdown
	 */
	if (_countdown > r.length()) {
	    _countdown -= r.length();
	} else  {
	    W_IGNORE( _countdown_expired->post() );
	    _countdown_expired = 0;
	    _countdown = 0;
	    _mutex.release();
	    me()->yield();
	    W_COERCE(_mutex.acquire());
	}
    }

    rc_t rc = _peer->insert(r, ret);

    incr_log_rec_cnt();
    incr_log_byte_cnt(r.length());

    _mutex.release();
    return rc;
}

rc_t                        
log_m::fetch(
    lsn_t&                      lsn,
    logrec_t*&                  rec,
    lsn_t*                      nxt)
{
    FUNC(log_m::fetch);
    W_COERCE(_mutex.acquire());
    w_assert1(_peer);
    rc_t rc = _peer->fetch(lsn, rec, nxt);
    // has to be released explicitly
    // _mutex.release();
    return rc;
}

void                        
log_m::release()
{
    FUNC(log_m::release);
    _mutex.release();
}

static u_long nrecs_at_last_sync = 0;
static u_long nbytes_at_last_sync = 0;
void 
log_m::reset_stats()
{
    nrecs_at_last_sync = 0;
    nbytes_at_last_sync = 0;
}

rc_t                
log_m::flush(const lsn_t& lsn)
{
    W_COERCE(_mutex.acquire());

    w_assert1(_peer);
    FUNC(log_m::flush);
    DBGTHRD(<<" flushing to lsn " << lsn);
    rc_t rc;

    if(lsn > _shared->_durable_lsn) {
	incr_log_sync_cnt(
		smlevel_0::stats.log_bytes_generated - nbytes_at_last_sync,
		smlevel_0::stats.log_records_generated - nrecs_at_last_sync);

        nrecs_at_last_sync =  smlevel_0::stats.log_records_generated;
        nbytes_at_last_sync =  smlevel_0::stats.log_bytes_generated;

	rc =  _peer->flush(lsn);
    } else {
	incr_log_sync_cnt(0,0);
    }
    _mutex.release();
    return rc;
}

rc_t                
log_m::scavenge(
    const lsn_t&                min_rec_lsn,
    const lsn_t&                min_xct_lsn)
{
    w_assert1(_peer);
    W_COERCE(_mutex.acquire());
    rc_t rc =  _peer->scavenge(min_rec_lsn, min_xct_lsn);
    _mutex.release();
    return rc;
}
/*********************************************************************
 *
 *  log_m::shm_needed(int n)
 *
 *  Return the amount of shared memory needed (in bytes)
 *  for the given value of the sm_logbufsize option
 *
 *  This *should* be a function of the kind of log
 *  we're going to construct, but since this is called long before
 *  we've discovered what kind of log it will be, we cannot manager that.
 *
 *********************************************************************/
int
log_m::shm_needed(int n)
{
    return (int) (n * 2) + CHKPT_META_BUF;
}
/*********************************************************************
 * class log_buf
 *********************************************************************/

bool
log_buf::fits(const logrec_t &r) const
{
    w_assert3(len() <= bufsize());
    w_assert3(nextrec().lo() - firstbyte().lo() == (unsigned long)len());

    const skip_log *s = new (__skip_log) skip_log;
    if(len() + r.length() > 
	_bufsize - s->length()) {
	return false;
    }
    return true;
}

void
log_buf::init(const lsn_t &f, const lsn_t &n, 
	bool is_written,
	bool is_durable
)
{
    _durable = is_durable;
    _written = is_written;
    _lsn_lastrec = lsn_t::null;
    _lsn_firstbyte = f;
	_lsn_flushed = 
	_lsn_nextrec = n;
    _len = nextrec().lo() - firstbyte().lo();
}

void
log_buf::insert(const logrec_t &r)
{
    DBGTHRD(<<" BEFORE insert" << *this);
    // had better have been primed
    w_assert3(firstbyte() != lsn_t::null);
    w_assert3(r.type() != logrec_t::t_skip);

    memcpy(buf + len(), &r, r.length());
    _len += r.length();
    _lsn_lastrec = _lsn_nextrec;
    _lsn_nextrec.advance(r.length());

    _durable = false;
    _written = false;

    w_assert3(nextrec().lo() - firstbyte().lo() == (unsigned long)len());
#ifdef NOTDEF
    // still room for skiplog
    const skip_log *s = new (__skip_log) skip_log;
    w_assert3(_bufsize - s->length() >= len());
#endif

    DBGTHRD(<<" AFTER insert" << *this);
}

void
log_buf::insertskip()
{
    FUNC( log_buf::insertskip);
    DBGTHRD(<<" BEFORE insertskip" << *this);

    skip_log *s = new (__skip_log) skip_log;
    w_assert3(_bufsize - s->length() >= (unsigned long)len());

    // Copy a skip record to the end of the buffer.

    s->set_lsn_ck(nextrec());
    // s->fill_xct_attr(tid_t::null, prevrec());

    //
    // Don't update any of the meta data for a skip record
    memcpy(buf + len(), s, s->length());
    DBGTHRD(<<" AFTER insertskip" << *this);
}

//  assumes fd is already positioned at the right place
void 
log_buf::write_to(int fd)
{
    DBGTHRD(<<" BEFORE write_to" << *this);

    int cc=0;
    int b, xfersize;
    //
    // How much can we write out and dispense with altogether?
    // Take into account the skip record for the purpose of
    // writing, but not for the purpose of advancing the
    // metadata in writebuf.
    //
    const skip_log *s = new (__skip_log) skip_log;
    int skiploglen = s->length();
    int writelen = len() + skiploglen;

    for( b = 0, xfersize = 0;
	b < Nblocks && writelen > xfersize;
	b++
    ) xfersize += XFERSIZE;

    // b = number of whole blocks to write
    // xfersize is the smallest multiple of XFERSIZE
    // that's greater than len()

    w_assert3(xfersize <= XFERSIZE * Nblocks);
    w_assert3(xfersize == XFERSIZE * b);

    DBGTHRD(<<"we'll write " << b << " blocks, or " << xfersize
	<< " bytes" );

#ifdef DEBUG
    DBGTHRD(<<" MIDDLE write_to" << *this);
#endif

#ifndef LOCAL_LOG
    if(me()->write(fd, buf, xfersize) )
#else
    if((cc = ::write(fd, buf, xfersize)) != xfersize)  
#endif
    {
	smlevel_0::errlog->clog << error_prio 
	  << "ERROR: could not flush log buf, xfersize=" 
	  << xfersize 
	  << ", errno= " << errno
	  << ",  cc= " << cc
	  << flushl;
	W_FATAL(eOS);
    }

    // nb: this touches _lsn_flushed, but it immediately gets overwritten:
    w_assert3(_lsn_flushed.advance(xfersize) >= nextrec());

    _lsn_flushed = nextrec();

    {
	// The amount buffered was not an integral multiple
	// of the block size, but it could have been less or more
	// than a full block.

	// Now copy the last block to the front
	// of the buffer and adjust the metadata.
	// Skip the copy if the total to write was less
	// than a block, in which case, the data are where
	// we want them to be.
	// 
	// If the part that spans the boundary
	// beween blocks n-1 and n is the skip record,
	// we have to save block n-1, not block n. 
	// Figure out if this is the case:

	int i = writelen % XFERSIZE;
	DBGTHRD(<<"i=" << i << " writlen=" << writelen);
	if ( i > 0 && i < skiploglen ) {
	   DBGTHRD(<<"skip rec spans boundary -- copy block " << b-1);
	   // copy the n-1st block 
	   b --;
	   xfersize -= XFERSIZE;

	   w_assert3(b > 0);
	} else if (i == skiploglen) {
	   DBGTHRD(<<"block " << b << " is exactly a skip log");
	    // the nth block is exactly a skip log
	    w_assert3(len() % XFERSIZE == 0);

	    // we can start over with a clean buf
	    b = 0;

	    // it's written but not yet durable
	    init(nextrec(), nextrec(), true, false);

	} else {
	   DBGTHRD(<<"block " << b << " contains the last non-skip record");
	    w_assert3(i == 0 || i > skiploglen);
	    // we copy the nth block (which might be the
	    // only buf)
	   w_assert3(b > 0);
	}
	if (b>0) {
	    DBGTHRD(<<"copy block " << b << "(" << b-1 << ")");
	    w_assert3(xfersize > 0);

	    b --;
	    xfersize -= XFERSIZE;

	    // xfersize could now be 0
	    w_assert3(b >= 0);
	    w_assert3(((xfersize / XFERSIZE) * XFERSIZE) == xfersize);

	    if (xfersize>0) {
		w_assert3(len() > (uint4)xfersize);
		// copy starting at the end of the prior block
		// (xfersize has been reduced by 1 block)

		// don't bother copying the skip log
		memcpy(buf, buf+xfersize, len() - xfersize); 

		_len -= xfersize;

		_lsn_firstbyte.advance(xfersize);
	    }
	    _written = true;
	} else {
	    w_assert3(len() == 0);
	}
    }
    w_assert3(flushed() <= nextrec());
    w_assert3(firstbyte() <= flushed());
    w_assert3(nextrec().lo() - firstbyte().lo() == (unsigned long)len());
    DBGTHRD(<<" AFTER write_to" << *this);
}


log_buf::log_buf(char *b, int sz)
    : 
    _lsn_firstbyte(lsn_t::null),
    _lsn_flushed(lsn_t::null),
    _lsn_nextrec(lsn_t::null),
    _durable(true),
    _written(true),
    _len(0),
    _bufsize(sz),
    ___skip_log(0),
    __skip_log(0)
{
	const	int skip_align = 8;

	/* Allocate 2x as much as the header_size so there is room to
	   create a properly aligned (by skip_align) header. */

	___skip_log = new char[logrec_t::hdr_sz * 2 + skip_align];
	if (!___skip_log)
		W_FATAL(fcOUTOFMEMORY);

	__skip_log = (char *)(((unsigned)___skip_log + (skip_align-1))
			      & ~(skip_align-1));
	w_assert3(is_aligned(__skip_log));

	w_assert1(is_aligned(b));
	buf =  new(b) char[sz];
}

log_buf::~log_buf()
{
	// don't delete, since addr is provided
	buf = 0;

	__skip_log = 0;
	delete [] ___skip_log;
	___skip_log = 0;
}

void 		
log_buf::prime(int fd, off_t offset, const lsn_t &next)
{
    FUNC(log_buf::prime);
    // we are about to write a record with the given
    // lsn.  But if this is start-up and we've initialized
    // with a partial partition, we have to prime the
    // buf with the last block in the partition
    uint4 b = (next.lo() / XFERSIZE) * XFERSIZE;

    DBGTHRD(<<" BEFORE prime" << *this);

    DBG(<<"firstbyte()=" << firstbyte() << " next = " << next);
#ifdef DEBUG
    if( firstbyte().hi() == next.hi() ) {
	// same partition
	// it never makes sense to prime or to have primed
	// primed the buffer for anything less than the
	// durable lsn, since this buffer is used only for
	// appending records
	w_assert3(b <= nextrec().lo());
	w_assert3(next == nextrec());

	if(b >= firstbyte().lo()) {
	    // it's in the range covered by the buf

	    w_assert3(b <= nextrec().lo());
	}
    }
#endif

    if( firstbyte().hi() != next.hi() ||
        (
	// implied: firstbyte().hi() == next.hi() &&
	b < firstbyte().lo() ) 
	) {

#ifdef DEBUG
	if(durable()) {
	    w_assert3(flushed().lo() == nextrec().lo());
	} else {
	    w_assert3(flushed().lo() < nextrec().lo());
	}

	// stronger:
	w_assert3(durable());
	w_assert3(written());
	// i.e., it's durable and it's
	// ok to lose the data here
#endif

	//
	// this should happen only when we've just got
	// the size at start-up and it's not a full block
	// or
	// we've been scavenging and closing partitions
	// in the midst of normal operations
	// 


	//
	lsn_t first = lsn_t(uint4(next.hi()), uint4(b));

	offset += first.lo();

	DBG(<<" seeking to " << offset << " on fd " << fd );
#ifndef LOCAL_LOG
	off_t cc;
	if(me()->lseek(fd, offset, SEEK_SET, cc) || cc != offset )
#else
	if(::lseek(fd, offset, SEEK_SET) != offset) 
#endif
	{
	    smlevel_0::errlog->clog << error_prio 
		<< "ERROR: could not seek to "
		<< offset
		<< " to prime : "
		<< flushl;
	    W_FATAL(eOS);
	}

	DBG(<<" reading " << XFERSIZE << " on fd " << fd );
#ifndef LOCAL_LOG
	rc_t n;
	if (n = me()->read(fd, buf, XFERSIZE)) 
#else
	int n;
	if ((n = ::read(fd, buf, XFERSIZE)) != XFERSIZE) 
#endif
	{
	    smlevel_0::errlog->clog << error_prio 
		<< "cannot read log: lsn " << first << flushl;
	    smlevel_0::errlog->clog << error_prio 
		<< "read(): " << ::strerror(::errno) << flushl;
	    smlevel_0::errlog->clog << error_prio 
		<< "read() returns " << n << flushl;
	    W_FATAL(eOS);
	}

	// set durable because what we have is what
	// we just read from the file.
	init(first, next, true, true);
    }
    w_assert3(firstbyte().hi() == next.hi());
    w_assert3(firstbyte().lo() <= next.lo());
    w_assert3(flushed().lo() <= nextrec().lo());
    w_assert3(nextrec()== next);
    DBGTHRD(<<" AFTER prime" << *this);
}

ostream&     
operator<<(ostream &o, const log_buf &l) 
{
    o <<" firstbyte()=" << l.firstbyte()
        <<" flushed()=" << l.flushed()
        <<" nextrec()=" << l.nextrec()
        <<" lastrec()=" << l.lastrec()
        <<" written()=" << l.written()
        <<" durable()=" << l.durable()
        <<" len()=" << l.len() ;

#ifdef SERIOUS_DEBUG
    if(l.firstbyte().hi() > 0 && l.firstbyte().lo() == 0) {
	// go forward
	_debug.clog << "FORWARD:" <<  flushl;
	int 	  i=0;
	char      *b;
	lsn_t 	  pos =  l.firstbyte();
	logrec_t  *r;

	// limit to 10
	while(pos.lo() < l.len() && i++ < 10) {
	    b = l.buf + pos.lo();
	    r = (logrec_t *)b;
	    _debug.clog << "pos: " << pos << " -- contains " << *r << flushl;

	    b += r->length();
	    pos.advance(r->length()); 
	}
    } else if(l.lastrec() != lsn_t::null) {
        _debug.clog << "BACKWARD: " << flushl;

	char      *b = l.buf + l.len() - (int)sizeof(lsn_t); 
	lsn_t 	  pos =  *(lsn_t *)b;
	lsn_t 	  lsn_ck;

	w_assert3(pos == l.lastrec() || l.lastrec() == lsn_t::null);
	w_assert3(pos.hi() == l.firstbyte().hi());

	logrec_t  *r;
	while(pos.lo() >= l.firstbyte().lo()) {
	    w_assert3(pos.hi() == l.firstbyte().hi());
	    b = l.buf + (pos.lo() - l.firstbyte().lo());
	    r = (logrec_t *)b;
	    lsn_ck = *((lsn_t *)((char *)r + (r->length() - sizeof(lsn_t))));

	    w_assert3(lsn_ck == pos || r->type() == logrec_t::t_skip);

	    _debug.clog << "pos: " << pos << " -- contains " << *r << flushl;

	    b -=  sizeof(lsn_t);
	    pos = *((lsn_t *) b);
	}
    }
#endif /*SERIOUS_DEBUG*/

    return o;
}

void 		        
log_base::check_wal(const lsn_t &/*ll*/) { }

void 		        
log_m::check_wal(const lsn_t &ll) 
{
   _peer->check_wal(ll);
}

#ifdef DEBUG

char 	*val=0;
char 	*name=0;

void
crashtest(
    log_m *   log,
    const char *c, 
    const char *file,
    int line
) 
{
    char            *_fname_debug_ = "crashtest"; // skip the dump()
    int     	_val = 0;
    static int  _matches = 0;
    static bool  _did_lookup = false;
    w_assert3(strlen(c)>0);

    if(name==0 && _did_lookup) return;
    name = ::getenv("CRASHTEST");
    val = ::getenv("CRASHTESTVAL");
    if(val) {
       _val = atoi(val);
    }

    _did_lookup = true;

    if(name && (::strcmp(name,c) == 0) ) {
	DBG( "Crashtest " << c << " #" << _matches);
	if(_val == 0 || ++_matches == _val)  {
	    /* Flush and sync the log to the current lsn_t, just
	     * because we want the semantics of the CRASHTEST
	     * to be that it crashes after the logging was
	     * done for a given source line -- it just makes
	     * the crash tests easier to insert this way.
	     */
	    if(log) {
	       W_COERCE(log->flush(log->curr_lsn()));
	       w_assert3(log->durable_lsn() == log->curr_lsn());
		DBG( << "Crashtest " << c
		    << " durable_lsn is " << log->durable_lsn() );
	    }
	    /* skip destructors */
	    DBG( "Crashtest " << c
		    << " at " << file
		    << " line " << line
		);
	    _exit(44);
	}
    }
}
#endif /* DEBUG */

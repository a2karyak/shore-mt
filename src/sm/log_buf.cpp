/*<std-header orig-src='shore'>

 $Id: log_buf.cpp,v 1.33 2006/05/11 22:24:29 bolo Exp $

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

#include "w_defines.h"

/*  -- do not edit anything above this line --   </std-header>*/

#define SM_SOURCE
#define LOG_C

#include "sm_int_1.h"
#include "log.h"
#ifdef __GNUG__
#pragma implementation "log_buf.h"
#endif
#include "log_buf.h"
#include "logdef_gen.cpp"
#include "crash.h"

#include <new>

log_stats_t log_m::stats;


/*********************************************************************
 * class log_buf
 *********************************************************************/

bool
log_buf::compensate(const lsn_t &rec, const lsn_t &undo_lsn) 
{
    if (rec == undo_lsn)  {
	// somewhere in the calling code, we didn't actually
	// log anything.  so this would be a compensate to
	// myself.  i.e. a no-op

	return true;
    }

    //
    // We check lastrec() (last logrecord written into this buffer)
    // instead of nextrec() because
    // we don't want a compensation rec to point to itself.
    //
    if((flushed() < rec && nextrec() > rec)
	&& 
	(lastrec() > undo_lsn)
	) {
	w_assert3(rec.hi() == firstbyte().hi());

	fileoff_t offset = rec.lo() - firstbyte().lo();

	logrec_t *s = (logrec_t *)(this->buf + offset);
	w_assert1(s->prev() == lsn_t::null ||
	    s->prev() >= undo_lsn);

	if( ! s->is_undoable_clr() ) {
	    DBGTHRD(<<"REWRITING LOG RECORD " << rec
		<< " : " << *s);
	    s->set_clr(undo_lsn);
	    return true;
	}
    }
    return false;
}

bool
log_buf::fits(const logrec_t &r) const
{
    w_assert3(len() <= bufsize());
    // If r.length() > bufsize(), we've configured with
    // a logbuf too small for the page size
    w_assert1((int)r.length() <= bufsize());
    w_assert3(nextrec().lo() - firstbyte().lo() == len());

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
    _lsn_flushed = _lsn_nextrec = n;

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

    w_assert3(nextrec().lo() - firstbyte().lo() == len());
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
    w_assert3(_bufsize - (fileoff_t)s->length() >= len());

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
    fileoff_t skiploglen = s->length();
    fileoff_t writelen = len() + skiploglen;

    for( b = 0, xfersize = 0; writelen > xfersize; b++) xfersize += XFERSIZE;

    // b = number of whole blocks to write
    // xfersize is the smallest multiple of XFERSIZE
    // that's greater than len()

    w_assert3(xfersize == XFERSIZE * b);

    DBGTHRD(<<"we'll write " << b << " blocks, or " << xfersize
	<< " bytes" );

#ifdef W_DEBUG
    DBGTHRD(<<" MIDDLE write_to" << *this);
#endif /* W_DEBUG */

    w_rc_t e = me()->write(fd, buf, xfersize);
    if (e) {
	smlevel_0::errlog->clog << error_prio 
	  << "ERROR: could not flush log buf:"
	  << " fd=" << fd
	  << " b=" << b
	  << " xfersize=" << xfersize
	  << " cc= " << cc
	  << ":" << endl
	  << e
	  << flushl;
	W_COERCE(e);
    }

#ifdef W_DEBUG
    {
    //  nextrec() should always be the start of the last
    //  durable skip record.
    w_assert3(firstbyte().hi() == nextrec().hi());
    lsn_t off = nextrec();
    // nextrec() - firstbyte() == offset
    off.advance(-firstbyte().lo());
    int f = off.lo();
    logrec_t *l = (logrec_t *)(buf + f);
    w_assert3(l->type() == s->type());
    lsn_t ck =  l->get_lsn_ck();
    w_assert3(ck == nextrec());
    w_assert3((int)l->length() == skiploglen );
    }
#endif

    // jiebing: collect global log io statistics
    log_m::stats.log_writes ++;
    log_m::stats.log_bytes_written += xfersize;


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

	fileoff_t i = writelen % XFERSIZE;
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
		w_assert3(len() > xfersize);
		// copy starting at the end of the prior block
		// (xfersize has been reduced by 1 block)

		// don't bother copying the skip log
                /* XXX Possible loss of bits in cast */
		w_assert1(len() - xfersize < fileoff_t(w_base_t::int4_max));
		memcpy(buf, buf+xfersize, int(len() - xfersize)); 

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
    w_assert3(nextrec().lo() - firstbyte().lo() == len());
    DBGTHRD(<<" AFTER write_to" << *this);
}

log_buf::log_buf(char *b, fileoff_t sz)
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

	/* XXX secret alignment user */
	__skip_log = (char *)(((ptrdiff_t)___skip_log + (skip_align-1))
			      & ~(skip_align-1));
	w_assert3(is_aligned(__skip_log));

	w_assert1(is_aligned(b));

	/* XXX Possible loss of bits in cast */
	w_assert1(sz < fileoff_t(w_base_t::int4_max));
	buf =  new(b) char[int(sz)];

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
log_buf::prime(int fd, fileoff_t offset, const lsn_t &next)
{
    FUNC(log_buf::prime);
    // we are about to write a record for a certain lsn,
    // which maps to the given offset.
    //  But if this is start-up and we've initialized
    // with a partial partition, we have to prime the
    // buf with the last block in the partition

    fileoff_t b = (next.lo() / XFERSIZE) * XFERSIZE;

    DBGTHRD(<<" BEFORE prime" << *this);

    DBG(<<"firstbyte()=" << firstbyte() << " next = " << next);
#ifdef W_DEBUG
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
#endif /* W_DEBUG */

    if( firstbyte().hi() != next.hi() ||
        (
	// implied: firstbyte().hi() == next.hi() &&
	   b < firstbyte().lo() ) 
	) {

#ifdef W_DEBUG
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
#endif /* W_DEBUG */

	//
	// this should happen only when we've just got
	// the size at start-up and it's not a full block
	// or
	// we've been scavenging and closing partitions
	// in the midst of normal operations
	// 


	//
	lsn_t first = lsn_t(uint4_t(next.hi()), sm_diskaddr_t(b));

	offset += first.lo();

	DBG(<<" seeking to " << offset << " on fd " << fd );
	w_rc_t e = me()->lseek(fd, offset, sthread_t::SEEK_AT_SET);
	if (e) {
	    smlevel_0::errlog->clog << error_prio 
		<< "ERROR: could not seek to "
		<< offset
		<< " to prime : "
		<< flushl;
	    W_COERCE(e);
	}

	DBG(<<" reading " << int(XFERSIZE) << " on fd " << fd );
	int n = 0;
	e = me()->read(fd, buf, XFERSIZE);
	if (e) {
	    smlevel_0::errlog->clog << error_prio 
		<< "cannot read log: lsn " << first << flushl;
	    smlevel_0::errlog->clog << error_prio 
		<< "read(): " << e << flushl;
	    smlevel_0::errlog->clog << error_prio 
		<< "read() returns " << n << flushl;
	    W_COERCE(e);
	}

    	// jiebing: collect global log io statistics
    	log_m::stats.log_reads ++;
    	log_m::stats.log_bytes_read += XFERSIZE;

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
    if(l.firstbyte().hi() > 0 && l.firstbyte().lo() == srv_log::zero) {
	// go forward
	_w_debug.clog << "FORWARD:" <<  flushl;
	int 	  i=0;
	char      *b;
	lsn_t 	  pos =  l.firstbyte();
	logrec_t  *r;

	// limit to 10
	while(pos.lo() < l.len() && i++ < 10) {
	    b = l.buf + pos.lo();
	    r = (logrec_t *)b;
	    _w_debug.clog << "pos: " << pos << " -- contains " << *r << flushl;

	    b += r->length();
	    pos.advance(r->length()); 
	}
    } else if(l.lastrec() != lsn_t::null) {
        _w_debug.clog << "BACKWARD: " << flushl;

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

	    // lsn_ck = *((lsn_t *)((char *)r + (r->length() - sizeof(lsn_t))));
	    memcpy(&lsn_ck, ((char *)r + (r->length() - sizeof(lsn_t))),
				sizeof(lsn_ck));

	    w_assert3(lsn_ck == pos || r->type() == logrec_t::t_skip);

	    _w_debug.clog << "pos: " << pos << " -- contains " << *r << flushl;

	    b -=  sizeof(lsn_t);
	    pos = *((lsn_t *) b);
	}
    }
#endif /*SERIOUS_DEBUG*/

    return o;
}


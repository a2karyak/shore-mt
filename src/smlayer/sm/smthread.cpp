/*<std-header orig-src='shore'>

 $Id: smthread.cpp,v 1.70 1999/08/18 19:45:49 bolo Exp $

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
#define SMTHREAD_C
#ifdef __GNUG__
#   pragma implementation
#endif

#include <sm_int_1.h>
//#include <e_errmsg_gen.h>

/*
 * A few minor things for class tcb_t - for grabbing and freeing
 * statistics structures
 */
void 	
smthread_t::tcb_t::attach_stats() { 
    _stats = new smthread_stats_t; 
    // These things have no constructor
    memset(&thread_stats(),0, sizeof(smthread_stats_t)); 
}

void 
smthread_t::tcb_t::detach_stats() {
    /*
     * NB: sm stats aren't protected
     */
    smlevel_0::stats.summary_thread += thread_stats();
    delete _stats;
}


const uint4_t eERRMIN = smlevel_0::eERRMIN;
const uint4_t eERRMAX = smlevel_0::eERRMAX;

static smthread_init_t smthread_init;

int smthread_init_t::count = 0;
/*
 *  smthread_init_t::smthread_init_t()
 */
smthread_init_t::smthread_init_t()
{
}



/*
 *	smthread_init_t::~smthread_init_t()
 */
smthread_init_t::~smthread_init_t()
{
}

/*********************************************************************
 *
 *  Constructor and destructor for smthread_t::tcb_t
 *
 *********************************************************************/

void
smthread_t::no_xct(xct_t *xd)
{
    w_assert3(xd);
    /* See comments in smthread_t::new_xct() */
    DBG(<<"no_xct: id=" << me()->id);
    xd->stash(
	    tcb()._lock_hierarchy,
	    tcb()._sdesc_cache,
	    tcb()._xct_log);
}

void
smthread_t::new_xct(xct_t *x)
{
    w_assert1(x);
    /* Make 3 new per-thread xct structures.  Rather than
     * have an xct constantly deleting/re-allocating 
     * such beasts every time the last/only thread detaches/attaches,
     * we use steal/stash to effect the following protocol:
     * Each thread mallocs a set of these.  The first thread to
     * detach stores its structures in the xct.  The next thread to
     * attach gets the stored copy rather than malloc-ing a new one.
     * This will keep the caches intact for servers that attach-do work-
     * detach often during a transaction.
     */
    DBG(<<"new_xct: id=" << me()->id);
    x->steal(
	tcb()._lock_hierarchy,
	tcb()._sdesc_cache,
	tcb()._xct_log);
}


/*********************************************************************
 *
 *  smthread_t::smthread_t
 *
 *  Create an smthread_t.
 *
 *********************************************************************/
smthread_t::smthread_t(
    st_proc_t* f,
    void* arg,
    priority_t priority,
    bool block_immediate,
    bool auto_delete,
    const char* name,
    timeout_in_ms lockto,
    unsigned stack_size)
: sthread_t(priority, block_immediate, auto_delete, name, stack_size),
  _proc(f),
  _arg(arg),
  _block("m:smblock"),
  _awaken("c:smawaken")
{
	lock_timeout(lockto);
}


smthread_t::smthread_t(
    priority_t priority,
    bool block_immediate,
    bool auto_delete,
    const char* name,
    timeout_in_ms lockto,
    unsigned stack_size)
: sthread_t(priority, block_immediate, auto_delete, name, stack_size),
  _proc(0),
  _arg(0),
  _block("m:smblock"),
  _awaken("c:smawaken")
{
	lock_timeout(lockto);
}




/*********************************************************************
 *
 *  smthread_t::~smthread_t()
 *
 *  Destroy smthread. Thread is already defunct the object is
 *  destroyed.
 *
 *********************************************************************/
smthread_t::~smthread_t()
{
    // w_assert3(tcb().pin_count == 0);
    w_assert3( tcb()._lock_hierarchy == 0 );
    w_assert3( tcb()._sdesc_cache == 0 );
    w_assert3( tcb()._xct_log == 0 );
}

void smthread_t::prepare_to_block()
{
    _unblocked = false;
}

w_rc_t	smthread_t::block(smutex_t &lock,
			  timeout_in_ms timeout,
			  const char * const W_IFDEBUG(blockname))
{
	bool	timed_out = false;
	bool	was_unblocked = false;

	W_COERCE(lock.acquire());
	_waiting = true;

	/* XXX adjust timeout for "false" signals */
	while (!_unblocked && !timed_out) {
#ifdef W_DEBUG
		_awaken.rename("c:", blockname);
#endif /* W_DEBUG */
		w_rc_t	e = _awaken.wait(lock, timeout);
#ifdef W_DEBUG
		_awaken.rename("c:", "smawaken");
#endif /* W_DEBUG */
		if (e && e.err_num() == stTIMEOUT)
			timed_out = true;
		else if (e)
			W_COERCE(e);
	}

	was_unblocked = _unblocked;

	_waiting = false;

	lock.release();

	/* XXX possible race condition on sm_rc, except that
	   it DOES belong to me, this thread?? */
	/* XXX if so, the thread package _rc code has the same problem */
	return was_unblocked ? _sm_rc : RC(stTIMEOUT);
}

w_rc_t	smthread_t::unblock(smutex_t &lock, const w_rc_t &rc)
{
	W_COERCE(lock.acquire());
	if (!_waiting) {
		cerr << "Warning: smthread_t::unblock():"
			<< " async thread unblock!" << endl;
	}

	_unblocked = true;

	/* Save rc (will be returned by block());
	   this code is copied from that in the
	   sthread block/unblock sequence. */
	if (_sm_rc)  {;}
	if (&rc) 
		_sm_rc = rc;
	else
		_sm_rc = RCOK;

	_awaken.signal();
	lock.release();
	return RCOK;
}

/* thread-compatability block() and unblock.  Use the per-smthread _block
   as the synchronization primitive. */
w_rc_t	smthread_t::block(timeout_in_ms timeout,
			  const char * const caller,
			  const void *)
{
	return block(_block, timeout, caller);
}

w_rc_t	smthread_t::unblock(const w_rc_t &rc)
{
	return unblock(_block, rc);
}


/*********************************************************************
 *
 *  smthread_t::run()
 *
 *  Befault body of smthread. Could be overriden by subclass.
 *
 *********************************************************************/
#ifdef NOTDEF
// Now this function is pure virtual in an attempt to avoid
// a possible gcc bug

void smthread_t::run()
{
    w_assert1(_proc);
    _proc(_arg);
}
#endif /* NOTDEF */

smthread_t*
smthread_t::dynamic_cast_to_smthread()
{
    return this;
}


const smthread_t*
smthread_t::dynamic_cast_to_const_smthread() const
{
    return this;
}


void
smthread_t::for_each_smthread(SmthreadFunc& f)
{
    SelectSmthreadsFunc g(f);
    for_each_thread(g);
}


void 
smthread_t::attach_xct(xct_t* x)
{
    w_assert3(tcb().xct == 0);  // eTWOTRANS
    tcb().xct = x;
    int n = x->attach_thread();

    // init stat that counts
    // log bytes generated on behalf of this thread in this attachment.
    SET_TSTAT(xlog_bytes_generated, 0 );

    w_assert1(n >= 1);
}


void 
smthread_t::detach_xct(xct_t* x)
{
    w_assert3(tcb().xct == x); 
    /*
     * before detaching, add in thread stats
     * to the xct's stats
     */
    DBGTHRD(<<"detach: thread.xlog_bytes_generated = " 
	<< thread_stats().xlog_bytes_generated
	<< " (updates) x->__log_bytes_generated = " 
	<< x->__log_bytes_generated
	);

    /* XXX race condition */
    x->__log_bytes_generated += thread_stats().xlog_bytes_generated;

    if(x->is_instrumented()) {
	sm_stats_info_t &s = x->stats_ref();
	DBG(<<"Adding summary thread stats: " << thread_stats());
	s.summary_thread += thread_stats();
	/* 
	 * The stats have been added into the
	 * xct's structure, so they must be cleared for the
	 * thread.
	 */
	memset(&thread_stats(),0, sizeof(smthread_stats_t)); 
    }

    int n=x->detach_thread();
    w_assert1(n >= 0);
    tcb().xct = 0;
}

void		
smthread_t::_dump(ostream &o) const
{
	sthread_t *t = (sthread_t *)this;
	t->sthread_t::_dump(o);

	o << "smthread_t: " << (char *)(is_in_sm()?"in sm ":"");
	if(tcb().xct) {
	  o << "xct " << tcb().xct->tid() << endl;
	}
// no output operator yet
//	if(sdesc_cache()) {
//	  o << *sdesc_cache() ;
//	}
 	o << endl;
}


void SelectSmthreadsFunc::operator()(const sthread_t& thread)
{
    if (const smthread_t* smthread = thread.dynamic_cast_to_const_smthread())  {
	f(*smthread);
    }
}


void PrintSmthreadsOfXct::operator()(const smthread_t& smthread)
{
    if (smthread.xct() == xct)  {
	o << "--------------------" << endl << smthread;
    }
}


class PrintBlockedThread : public ThreadFunc
{
    public:
			PrintBlockedThread(ostream& o) : out(o) {};
			~PrintBlockedThread() {};
        void		operator()(const sthread_t& thread)
                        {
			    if (thread.status() == sthread_t::t_blocked)  {
				out << "*******" << endl;
				thread._dump(out);
			    }
			};
    private:
	ostream&	out;
};

void
DumpBlockedThreads(ostream& o)
{
    PrintBlockedThread f(o);
    sthread_t::for_each_thread(f);
}

#include <vtable_info.h>
#include <vtable_enum.h>

void		
smthread_t::vtable_collect(vtable_info_t& t) 
{
    sthread_t::vtable_collect(t);

    char *p = t[smthread_tid_attr];
    ostrstream o(p, vtable_info_t::vtable_value_size);

    if(tcb().xct) {
      o << tcb().xct->tid() 
	  << ends;
    } else {
      o << ends;
    }

#define TMP_GET_STAT(x) this->thread_stats().x

#include "smthread_stats_t_collect_gen.cpp"
}

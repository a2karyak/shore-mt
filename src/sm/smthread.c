/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

/*
 *  $Id: smthread.c,v 1.37 1996/03/13 20:23:16 bolo Exp $
 */
#define SM_SOURCE
#define SMTHREAD_C
#ifdef __GNUG__
#   pragma implementation
#endif

#include <sm_int.h>
//#include <e_error.i>

const eERRMIN = smlevel_0::eERRMIN;
const eERRMAX = smlevel_0::eERRMAX;

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
smthread_t::no_xct()
{
    if(tcb()._sdesc_cache) {
	xct_t::delete_sdesc_cache_t(tcb()._sdesc_cache);
	tcb()._sdesc_cache =0;
    }
    if(tcb()._lock_hierarchy) {
	xct_t::delete_lock_hierarchy(tcb()._lock_hierarchy);
	tcb()._lock_hierarchy =0;
    }
}
void
smthread_t::new_xct()
{
    tcb()._sdesc_cache = xct_t::new_sdesc_cache_t();
    tcb()._lock_hierarchy = xct_t::new_lock_hierarchy();
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
    long lockto)
: sthread_t(priority, block_immediate, auto_delete, name),
  _proc(f), _arg(arg)
{
    lock_timeout(lockto);
}


smthread_t::smthread_t(
    priority_t priority,
    bool block_immediate,
    bool auto_delete,
    const char* name,
    long lockto)
: sthread_t(priority, block_immediate, auto_delete, name),
  _proc(0), _arg(0)
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
    no_xct();
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


void 
smthread_t::attach_xct(xct_t* x)
{
    w_assert3(tcb().xct == 0); 
    tcb().xct = x;
    int n = x->attach_thread();
    w_assert1(n >= 1);
}


void 
smthread_t::detach_xct(xct_t* x)
{
    w_assert3(tcb().xct == x); 
    int n=x->detach_thread();
    w_assert1(n >= 0);
    tcb().xct = 0;
}

void		
smthread_t::_dump(ostream &o)
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

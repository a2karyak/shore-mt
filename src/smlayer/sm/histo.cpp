/*<std-header orig-src='shore'>

 $Id: histo.cpp,v 1.10 1999/06/07 19:04:05 kupsch Exp $

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
#define HISTO_C
#ifdef __GNUG__
#   pragma implementation "histo.h"
#endif

#include <histo.h>

// Search: find the smallest element that is Cmp.ge
// If none found return -1
// else return index of the element found .
// Root of search is element i

template<class T, class Cmp>
SearchableHeap<T, Cmp>::SearchableHeap(const Cmp& cmpFunction, 
	int initialNumElements) :  Heap<T,Cmp>(cmpFunction, initialNumElements)
{
}

template<class T, class Cmp>
int SearchableHeap<T, Cmp>::Search(int i, const T& t)
{
    w_assert3(HeapProperty(0));
#if defined(NOTDEF)
    DBGTHRD(<<"Search starting at " << i
	<< ", numElements=" << numElements
	);
#endif
    if(i > numElements-1) return -1;

    int parent = i; // root
    // First, check sibling of parent if parent != root
    if (parent >0 && (RightSibling(parent) < numElements))  {
#if defined(NOTDEF)
	DBGTHRD(<<"check right sibling: " << RightSibling(parent));
#endif
	if (cmp.ge(elements[RightSibling(parent)], t)) {
	    return RightSibling(parent);
	}
    }

#if defined(NOTDEF)
    DBGTHRD(<<"parent=" << parent);
#endif
    if (cmp.ge(elements[parent], t))  {
#if defined(NOTDEF)
	DBGTHRD(<<"LeftChild(parent)=" << LeftChild(parent)
		<< " numElements " << numElements);
#endif
	int child = LeftChild(parent);
	while (child < numElements)  {
#if defined(NOTDEF)
	    DBGTHRD(<<"child=" << child);
#endif
	    if (cmp.ge(elements[child], t))  {
#if defined(NOTDEF)
		DBGTHRD(<<"take child=" << child);
#endif
		parent = child;
	    } else  {
		child =  RightSibling(child);
#if defined(NOTDEF)
		DBGTHRD(<<"try RightSibling:=" << child);
#endif
		if((child < numElements) &&
		    (cmp.ge(elements[child], t)) )  {
			parent = child;
#if defined(NOTDEF)
			DBGTHRD(<<"take child=" << parent);
#endif
		} else {
		    return parent;
		}
	    }
	    child = LeftChild(parent);
	}  
        return parent;
    }
#if defined(NOTDEF)
    DBGTHRD(<<"parent=" << parent);
#endif
    return -1; // none found
}

template<class T, class Cmp>
int SearchableHeap<T, Cmp>::Match(const T& t) const
{
    w_assert3(HeapProperty(0));
    for (int i = 0;  i < numElements; i++) {
	if (cmp.match(elements[i], t)) {
	    return i;
	}
    }
    return -1; // none found
}

#ifdef EXPLICIT_TEMPLATE
class histoid_t;
template class Heap<pginfo_t, histoid_compare_t>;
template class SearchableHeap<pginfo_t, histoid_compare_t>;
template class w_hash_t<histoid_t, stid_t>;
template class w_hash_i<histoid_t, stid_t>;
template class w_list_t<histoid_t>;
template class w_list_i<histoid_t>;
#endif

typedef class w_hash_i<histoid_t, stid_t> w_hash_i_histoid_t_stid_t_iterator;

W_FASTNEW_STATIC_DECL(histoid_t, 20);


smutex_t			histoid_t::htab_mutex("histoid_table");
w_hash_t<histoid_t, stid_t> *	histoid_t::htab = 0;
bool				histoid_t::initialized = false;

NORET 		
histoid_t::histoid_t (stid_t s) : cmp(s), refcount(0)
{
    DBGTHRD(<<"create histoid_t for store " << s
	<< " returns this=" << unsigned(this));
    pgheap = new SearchableHeap<pginfo_t, histoid_compare_t>(
	cmp, pages_in_heap);
    w_assert3(!_in_hash_table()); 
}

NORET 		
histoid_t::~histoid_t () 
{
    DBGTHRD(<<"destruct histoid_t for store " << cmp.key()
	<< " destroying this=" << unsigned(this));
    w_assert3(!_in_hash_table()); 
    w_assert1(refcount == 0);
    delete pgheap;
}
void 
histoid_t::initialize_table()
{
    htab = new w_hash_t<histoid_t, stid_t>( histoid_t::buckets_in_table,
			offsetof(histoid_t, cmp.key()),
			offsetof(histoid_t, link));
    if(!htab) {
	W_FATAL(eOUTOFMEMORY);
    }
}

void
histoid_t::destroy_table()
{
    // Empty the hash table
    // Called at shutdown.

    // class w_hash_i<histoid_t, stid_t> iter(*htab);
    w_hash_i_histoid_t_stid_t_iterator iter(*htab);

    histoid_t* h;
    while ((h = iter.next()))  {
	bool   gotit = false;
	h->_grab_mutex_cond(gotit);
	if(gotit) {
	    w_assert1(h->refcount == 0);
	    DBGTHRD(<<"removing " << unsigned(h));
	    w_assert3(h->_in_hash_table()); 
	    htab->remove(h);
	    w_assert3(!h->_in_hash_table()); 
	    DBGTHRD(<<"deleting " << unsigned(h));
	    h->_release_mutex();
	    delete h;
	} else {
	    // Should never happen
	    W_FATAL(fcINTERNAL);
	}
    }

    delete htab;
}

void	
histoid_t::_victimize(int howmany) 
{
    // Throw away (howmany) entries with 0 ref counts
    // Called with howmany > 0 only when we exceed
    // the desired table size 
    DBGTHRD(<<"_victimize " << howmany );

    // NB: ASSUMES CALLER HOLDS htab_mutex!!!
    w_assert3(htab_mutex.is_mine());

    // class w_hash_i<histoid_t, stid_t> iter(*htab);
    w_hash_i_histoid_t_stid_t_iterator iter(*htab);

    histoid_t *h = 0;
    while( (h = iter.next()) ) {
	bool gotit=false;
	h->_grab_mutex_cond(gotit);
	if(gotit)  {
	    if(h->refcount == 0) {
		DBGTHRD(<<"deleting " << unsigned(h));
	        w_assert3(h->_in_hash_table()); 
		htab->remove(h); // h->link.detach(), decrement _cnt 
	        w_assert3(!h->_in_hash_table()); 
		h->_release_mutex();
		DBGTHRD(<<"deleting " << unsigned(h));
		delete h;
		if(--howmany == 0) break;
	    } else {
		h->_release_mutex();
	    }
	}
    }
}

//
// In order to enforce 
// ref counts, we do this grotesque
// violation of const-ness here
//
histoid_t*	
histoid_t::copy() const
{
    DBGTHRD(<<"incr refcount for store " << cmp.key() 
	<< " from " << refcount);
    histoid_t *I = (histoid_t *)this;
    I->_grab_mutex();
    I->refcount++;
    I->_release_mutex();
    return I;
}

histoid_t*	
histoid_t::acquire(const stid_t& s) 
{
    DBGTHRD(<<"acquire histoid for store " << s);
    W_COERCE(htab_mutex.acquire());
    histoid_t *h = htab->lookup(s);
    if(h) {
	DBGTHRD(<<"existing store " << s);
	w_assert1(h->refcount >= 0);
	h->_grab_mutex();
	htab_mutex.release();
    } else {
	if(htab->num_members() >= max_stores_in_table) {
	    // choose a replacement while we have the htab_mutex,
	    // throw it out
	    _victimize(1 + (htab->num_members() - max_stores_in_table));
	}
	// Create a new histoid for the given store
	DBGTHRD(<<"construct histoid_t");
	h = new histoid_t(s);
	w_assert3(h->refcount == 0);
	w_assert3(!h->_in_hash_table()); 
	htab->push(h); // insert into hash table
	w_assert3(h->_in_hash_table()); 
	h->_grab_mutex();
	htab_mutex.release();
	h->_insert_store_pages(s);
    }
    DBGTHRD(<<"incr refcount for " << s
	<< " from " << h->refcount);
    h->refcount++;
    h->_release_mutex();
    w_assert3(h);
    w_assert3(h->refcount >= 0);
    DBGTHRD(<<"acquired, store=" << s 
	<< " this=" << unsigned(h)
	<< " refcount= " << h->refcount
	);
    return h;
}

void 
histoid_t::_insert_store_pages(const stid_t& s) 
{
    DBGTHRD(<<"_insert histoid for store " << s);
    w_assert3(_in_hash_table()); 
    w_assert3(_have_mutex());
    w_assert3(pgheap->NumElements() == 0);
    w_assert3(pgheap->HeapProperty(0));

    /*
     * Scan the store's extents and initialize the histogram
     */
    pginfo_t 	pages[pages_in_heap];
    int	 	numpages = pages_in_heap;
    W_COERCE(io->init_store_histo(&histogram, s, pages, numpages));
    // DBGTHRD(<<"histogram for store " << cmp.key() << "=" << histogram << endl );
    DBGTHRD(<<"insert_store_pages for store " << cmp.key() );
    DBGTHRD(<<"io_m scan found " << numpages << " pages" );

    /*
     * init_store_histo filled numpages pginfo_t, where
     * numpages is a number it produced. The pginfo_ts contain
     * bucket#s rather than space used/free, so we'll convert
     * them here, as we stuff them into the heap.
     */
    pginfo_t	p;
    for(int i=0; i<numpages; i++) {
	DBGTHRD(<<"page index." << i << "=pid " << pages[i].page()
		<< " space is " << pages[i].space());
	DBGTHRD(<<"pgheap.AddElementDontHeapify page " 
		<<pages[i].page() << " space " << pages[i].space());
	pgheap->AddElementDontHeapify(pages[i]); // makes a copy
    }
    pgheap->Heapify();
    w_assert3(pgheap->NumElements() == numpages);
    w_assert3(_have_mutex());
}

bool 		
histoid_t::release() 
{
    DBGTHRD(<<"decr refcount for store " 
	<< cmp.key() 
	<< " this=" << unsigned(this)
	<< " from " << refcount);

    _grab_mutex();
    refcount--;
    w_assert1(refcount >= 0);
    bool deleteit = ( (refcount==0) && !_in_hash_table() )?true:false;
    _release_mutex();
    return deleteit;
}

void 	
histoid_t::destroyed_store(const stid_t&s, sdesc_t*sd) 
{
    DBGTHRD(<<"histoid_t::destroyed_store " << s);
    W_COERCE(htab_mutex.acquire());

    bool success = false;
    while (!success) {
	histoid_t *h = htab->lookup(s);
	if(h) {
	    DBGTHRD(<<"lookup found " << unsigned(h)
		    << " refcount " << h->refcount);
	    // ref count had better be no more than 1
	    // because it takes an EX lock
	    // on the store to be able to destroy it.
	    if(sd) {
		w_assert1(h->refcount == 1);
	    } else {
		w_assert1(h->refcount == 0 || h->refcount == 1);
	    }
	    DBGTHRD(<<"removing " << unsigned(h) );
	    h->_grab_mutex_cond(success);
	    if(success) {
		w_assert3( h->_in_hash_table()); 
		htab->remove(h); // h->link.detach(), decrement _cnt 
		w_assert3( !h->_in_hash_table()); 
		h->_release_mutex();
	    }
	} else {
	   success = true; // is gone already
	}
	htab_mutex.release();
	if(h) {
	    w_assert3( !h->_in_hash_table()); 
	    DBGTHRD(<<"");
	    if(sd || smlevel_0::in_recovery() ) { 
		if(h->release()) {
		    DBGTHRD(<<"deleting " << unsigned(h));
		    delete h;
		} else {
		    w_assert3(0);
		}
	    } else if(h->refcount == 0) {
		DBGTHRD(<<"deleting " << unsigned(h));
		delete h;
	    } else {
		/* We don't have sd, so we can't wipe out
		 * the reference to h, so we can't
		 * delete it, even though we have removed
		 * it from the table.
		 * The problem here is that it WILL get
		 * cleaned up if we're in fwd/rollback (when
		 * the dir cache gets invalidated), but
		 * not if we're in crash-recovery/rollback.
		 * or in pseudo-crash-recovery/rollback as 
		 * realized by ssh "sm restart" command.  
		 */
		 DBGTHRD(<<"ROLLBACK! can't delete h= " << unsigned(h));
	    }
	} 
    }
    if(sd) {
	// wipe out pointer to h
	sd->add_store_utilization(0);
    }
}



/* 
 * Search the heap for a page with given page id
 *
 * Accessory to _find_page, find_page
 * etc.
 * DOES NOT FREE MUTEX!!! Caller must do that!
 */
int
histoid_t::__find_page(
    bool		insert_if_not_found,
    const shpid_t& 	pid
    // NB: doesn't free the histoid_t::mutex!!!!
)
{
    DBGTHRD(<<"__find_page " << pid );
    _grab_mutex();

    int hook;
    while(1) {
	w_assert3(pgheap->HeapProperty(0));
	for(hook = root; hook < pgheap->NumElements(); hook++) {
	    DBG(<<"checking at hook=" << hook
		<< " page=" << pgheap->Value(hook).page() );
	    if(pgheap->Value(hook).page() == pid) {
		DBGTHRD(<<"grabbing hook " << hook
		    << " page " << pgheap->Value(hook).page()
		    << " space " << pgheap->Value(hook).space()
		    );
		return hook;
	    }
	}
	if(insert_if_not_found) {
	    /* Not found: create one & insert it */
	    pginfo_t info(pid,0); // unknown size 
	    w_assert3(_have_mutex());
	    DBG(<<"insert if not found");
	    install(info);
	    // search to get the hook
	} else {
	    return nohook;
	}
    }
    // should never happen
    W_FATAL(eINTERNAL);
}

void
histoid_t::_find_page(
    bool	create_if_not_found,
    const shpid_t& 	pid,
    bool&	found_in_table,
    pginfo_t&	result
)
{
    // Don't create a pginfo_t, insert, then remove
    int hook = __find_page(false, pid);
    w_assert3(_have_mutex());

    found_in_table = bool(hook>=0);
    if(hook < 0 && create_if_not_found) {
	result = pginfo_t(pid,0); // unknown size 
    } else if(hook >= 0) {
	result = pgheap->RemoveN(hook);
	DBGTHRD(<<"pgheap.RemoveN page " 
		<<result.page() 
		<< " space " << result.space());
	w_assert3(pgheap->HeapProperty(0));
    }
#ifdef W_DEBUG
    w_assert3(pgheap->Match(pginfo_t(pid,0)) == -1);
#endif /* W_DEBUG */
    _release_mutex();
}

/*
 * Search the heap for a page that contains enough space. 
 * If found, hang onto the mutex for the histoid.
 */
w_rc_t
histoid_t::find_page(
    smsize_t 		space_needed, 
    bool&		found,
    pginfo_t&  		info, 
    file_p* 		pagep, 	// input
    slotid_t&		idx	// output iff found
) const
{
    DBGTHRD(<<"histoid_t::find_page in store " << cmp.key()
	<< " w/ at least " << space_needed << " free bytes "
	<< " refcount=" << refcount
	);
    w_assert1(refcount >= 0);
    pginfo_t 	tmp(0, space_needed);
    int	skip = root;
    int hook;

    found =  false;

    while(!found) {
	_grab_mutex();
	hook = pgheap->Search(skip, tmp);
	DBGTHRD(<<"Search returns hook " << hook);
	if(hook >= root) {
	    // legit
	    w_assert3(_have_mutex());
	    w_assert3(pgheap->HeapProperty(0));
	    info = pgheap->RemoveN(hook);
	    w_assert3(pgheap->HeapProperty(0));
	    DBGTHRD(<<"pgheap.RemoveN page " 
		<<info.page() << " space " << info.space());
	    _release_mutex();

	    DBGTHRD(<<"histoid_t::find_page FOUND " << info.page());
	    bool	success=false;
	    shpid_t	pg = info.page();
	    {
		lpid_t pid(cmp.key(), pg);
		success = io->is_valid_page_of(pid, cmp.key().store);
		//w_assert3(success);
		DBGTHRD(<<" is valid page:" << success);
		if(!success) skip = hook;
	    }
	    if(success) {
		success=false;
		W_DO(latch_lock_get_slot(pg, pagep, space_needed,
			false, // not append-only
			success, idx));
	    }
	    if(success) {
		found = true;
		DBGTHRD(<<"histoid_t::find_page FOUND ");
		w_assert3(pagep->is_fixed());
		w_assert3(idx != 0);
		return RCOK;
	    } else {
		skip = hook;
	    }
	} else {
	    _release_mutex();
	    // No suitable pages in cache
	    DBGTHRD(<<"histoid_t::find_page NOT FOUND in cache");
	    w_assert3(!found);
	    return RCOK;
	}
    }
    // Should never get here
    W_FATAL(eINTERNAL);
    return RCOK;
}

w_rc_t		
histoid_t::latch_lock_get_slot(
    shpid_t& 	shpid,
    file_p*	pagep, 
    smsize_t	space_needed,
    bool	append_only,
    bool&	success,
    slotid_t&   idx	// only meaningful if success
) const 
{
    success	= false;
    lpid_t	pid(cmp.key(), shpid);
    /*
     * conditional_fix doesn't re-fix if it's already
     * fixed in desired mode; if mode isn't sufficient,
     * is upgrades rather than double-fixing
     */
    rc_t rc = pagep->conditional_fix(pid, LATCH_EX);
    DBGTHRD(<<"rc=" << rc);
    if(rc) {
	if(rc.err_num() == smthread_t::stTIMEOUT) {
	    // someone else has it latched - give up
	    INC_TSTAT(fm_nolatch);
	    return RCOK;
	} else {
	    // error we can't handle
	    DBGTHRD(<<"rc=" << rc);
	    return RC_AUGMENT(rc);
	}
    } else if(pagep->pid().stid() != cmp.key()) {
	// Really shouldn't happen! TODO: make that so
	// someone else raced in
	DBGTHRD(<<"stid changed to " << pagep->pid().stid());
	// reject this page
	pagep->unfix();
        INC_TSTAT(fm_page_moved);
	return RCOK; 
    } else {
	/* Try to acquire the lock */
	DBGTHRD(<<"Try to acquire slot & lock ");

	rc_t rc = pagep->_find_and_lock_free_slot(append_only,
		space_needed, idx);
	DBGTHRD(<<"rc=" <<rc);
	if(rc) {
	    pagep->unfix();
	    DBGTHRD(<<"rc=" << rc);
	    if (rc.err_num() == eRECWONTFIT) {
		INC_TSTAT(fm_page_full);
		return RCOK; 
	    } else {
		// error we can't handle
		return RC_AUGMENT(rc);
	    }
	}
	DBGTHRD(<<"acquired slot " << idx);
	success = true;
	w_assert3(pagep->is_fixed());
	// idx is set
    }
    return RCOK;
}

void		
histoid_t::install(const pginfo_t &info)
{
    DBGTHRD("install info in heap");
    w_assert3( _in_hash_table() );
    bool do_release = false;
    bool do_install = true;
    if(!_have_mutex()) {
	_grab_mutex();
	do_release = true;
    }

    // Install new info for page
    // NB: assumes it's not already there 

#ifdef W_DEBUG
    {   // verify that the page isn't already in the heap
	pginfo_t tmp(info.page(), 0);
	w_assert3(pgheap->Match(tmp) == -1);
    }
#endif /* W_DEBUG */

    w_assert3(_have_mutex());

    w_assert3(pgheap->HeapProperty(0));
    int n = pgheap->NumElements();
    if(n >= pages_in_heap) {
	// remove one iff this is greater than
	// one of the smallest there (e.g. last one)
	pginfo_t&    t = pgheap->Value(n-1);
	if( ! cmp.gt(info, t) ) {
	    DBGTHRD(<<"bypassing install: new space " 
		<< info.space()
		<< " space at bottom of heap =" << 
		t.space());
	    do_install = false;
	} 
        pginfo_t p = pgheap->RemoveN(n-1); // makes a copy
    }
    if(do_install) {
	DBGTHRD(<<"pgheap.AddElement page" << info.page()
	    << " space " << info.space()
	);
	pgheap->AddElement(info); // makes a copy
    }
    if(do_release) {
	_release_mutex();
    }
    w_assert3(pgheap->HeapProperty(0));
}

/*
 * update_page - called when we added/removed something
 * to/from a page, so we will want to cause an
 * entry to be in the table, whether or not it was
 * there before.
 */
void		
histoid_t::update_page(const shpid_t& pid, smsize_t amt)
{
    DBGTHRD(<<"update_page");
    int hook = __find_page(true, pid); // insert if not found


    // hang onto mutex while updating...
    w_assert3(_have_mutex());

    w_assert3(pgheap->HeapProperty(0));
    pginfo_t&    t = pgheap->Value(hook);

    DBGTHRD(<<"histoid_t::update_page hook " 
	<< hook 
	<< " page " <<  t.page()
	<< " amt " << amt);

    smsize_t	old = t.space();
    if(amt != old) {
    DBGTHRD(<<"before update space: " << t.space());
	t.update_space(amt);
    DBGTHRD(<<"after update space: " << t.space());
	if(old < amt) {
	    DBGTHRD(<<"pgheap.IncreasedN hook " << hook
		<< " page " << t.page()
		<< " space " << t.space());
	    pgheap->IncreasedN(hook);
	} else {
	    DBGTHRD(<<"pgheap.DecreasedN hook "  << hook
		<< " page " << t.page()
		<< " space " << t.space());
	    pgheap->DecreasedN(hook);
	}
    }
    w_assert3(pgheap->HeapProperty(0));
    _release_mutex();
}

void
histoid_t::_release_mutex() const
{
    histoid_t *I = (histoid_t *)this;
    I->mutex.release();
}
void
histoid_t::_grab_mutex() const
{
    histoid_t *I = (histoid_t *)this;
    W_COERCE(I->mutex.acquire());
}
void
histoid_t::_grab_mutex_cond(bool& got) const
{
    histoid_t *I = (histoid_t *)this;
#ifdef W_DEBUG
    if(I->mutex.acquire(WAIT_IMMEDIATE) != RCOK) {
       w_assert3(! mutex.is_mine() );
       got = false;
    } else {
       w_assert3( mutex.is_mine() );
       got = true;
    }
#else
    W_IGNORE(I->mutex.acquire(WAIT_IMMEDIATE));
    got = mutex.is_mine();
#endif
}

bool
histoid_t::_have_mutex() const
{
    return mutex.is_mine();
}

w_rc_t		
histoid_t::exists_page(
    smsize_t space_needed,
    bool&	found
) const
{
    space_bucket_t b = file_p::free_space2bucket(space_needed);
    _grab_mutex();
    // DBGTHRD(<<"histogram for store " << cmp.key() << "=" << histogram << endl );
    DBGTHRD(<<"exists_page store " << cmp.key());
    while (! (found = histogram.exists(b)) 
	&& b < (space_num_buckets-1)) b++;
    _release_mutex();
    return RCOK;
}

void		
histoid_t::bucket_change(
    smsize_t    old_space_left,
    smsize_t    new_space_left
) 
{
    space_bucket_t ob = file_p::free_space2bucket(old_space_left);
    space_bucket_t nb = file_p::free_space2bucket(new_space_left);
    if(ob != nb) {
	_grab_mutex();
	histogram.decr(ob);
	histogram.incr(nb);
	_release_mutex();
	DBGTHRD(<<"changed bucket" << histogram );
    }
}

ostream &
operator<<(ostream&o, const histoid_t&h)
{
    o << " Key=" << h.cmp.key()
      << " heap.numelements: " << h.pgheap->NumElements()
    // o << " heap: "; h.pgheap->Print(o); o <<  endl;
      << " histogram= " << h.histogram << endl;
    return o;
}


NORET 
histoid_update_t::histoid_update_t(sdesc_t *sd) : 
_found_in_table(false),  // _found_in_table ==> want to reinstall
_page(0), _h(0) 
{
    _h = sd->store_utilization()->copy();
    _info.set(0,0); // unknown size
    _old_space = 0; // unknown
#ifdef W_TRACE
    if(_debug.flag_on("histoid_update_t",__FILE__)) {
	DBGTHRD(<<"CONSTRUCT histoid_update_t: " << *this);
    }
#endif /* W_TRACE */
}

NORET 
histoid_update_t::histoid_update_t(page_p& pg) : 
    _found_in_table(false), // _found_in_table ==> will want to reinstall
    _page(&pg), _h(0)
{ 
    w_assert3(_page->is_fixed());
    lpid_t	_pid = _page->pid();
    _h = histoid_t::acquire(_pid.stid()); // incr ref count
    DBGTHRD(<<"histoid_update_t constructor");
    _h->_find_page(true, _pid.page, _found_in_table, _info);
    _old_space = _info.space();

    /* update bucket info, since we have the page fixed */
    smsize_t current_space = pg.free_space4bucket();
    if(current_space != _info.space()) {
	/*
	 * How can the value be wrong?  Let me count the ways:
	 * 1) it might have been
	 *    taken from the BUCKET in the extent's histogram,
	 *    by init_store_histo
	 * and
	 *    1.0) extent map might have it wrong -maybe has
	 *    never been set  (init to bucket 0)
	 *    1.1) the extent's bucket info isn't logged, so after
	 *    crash/recovery it could be just plain WRONG, and, 
	 *    as long as the pages aren't updated, it will remain wrong.
	 * 2) This thread might have updated the page without yet
	 *    unfixing the page at the time we are looking at it
	 * 3) This could be a newly -allocated page that's
	 *    got old histo info left in the extlink and this very
	 *    call to this function is to update that info.
	 */

#if defined(W_DEBUG) && defined(NOTDEF)
	if (!(
		(page_p::free_space2bucket(current_space) == 
		  page_p::free_space2bucket(_info.space()))
		  ||
		  (page_p::free_space2bucket(_info.space()) == 0)
	  )) {
	      // should put a counter here
        }
#endif /* W_DEBUG */

	// update _old_space and _info:
	_info.set(_info.page(), (_old_space = current_space));
    }

#ifdef W_TRACE
    if(_debug.flag_on("histoid_update_t",__FILE__)) {
	DBGTHRD(<<"CONSTRUCT histoid_update_t: " << *this);
    }
#endif /* W_TRACE */
}

NORET 
histoid_update_t::~histoid_update_t() 
{
    DBGTHRD(<<"~histoid_update_t");
#ifdef W_TRACE
    if(_debug.flag_on("histoid_update_t",__FILE__)) {
	DBGTHRD(<<"DESTRUCT histoid_update_t: " << *this);
    }
#endif /* W_TRACE */
    if(_h) {
	if(_found_in_table) {
	    // want to reinstall page
	    w_assert3(!_h->_have_mutex());
	    if(_page && _page->is_fixed())  {
		w_assert3(_info.page() == _page->pid().page);
	    }
	    DBGTHRD(<<"calling bucket change for page " << _info.page());
	    _h->bucket_change(_old_space, _info.space());
	    DBGTHRD(<<"destructor, error case, found in table, put back");
	    _h->install(_info); 
	}
	if(_h->release()) { 
	    // won't get deleted if it's in the hash table
	    delete _h; 
	}
	_h = 0;
    }
}

ostream &
operator<<(ostream&o, const histoid_update_t&u)
{
    o << " info: page= " << u._info.page() << " space=" << u._info.space();
    o << " found in table: " << u._found_in_table;
    o << " old space: " << u._old_space;
    if(u._h) {
	o << endl << "\thistoid_t:" << *u._h << endl;
    }
    return o;
}

void  
histoid_update_t::replace_page(page_p *p, bool reinstall) 
{
    if(_page) {
	DBGTHRD(<<"replace page " << _page->pid().page
		<< " refcount=" << _h->refcount
	);
    }

    // Re-install OLD page if we found it there and
    // we're not discarding it due to its being
    // entirely full
    bool differs = (_info.page() != p->pid().page) ;
    if(differs || p != _page) {
	if(_found_in_table && reinstall) {
	    w_assert3(!_h->_have_mutex());
	    DBGTHRD(<<"bucket change for page " << _info.page());
	    if(_page && _page->is_fixed()) {
		w_assert3(_info.page() == _page->pid().page);
	    }
	    _h->bucket_change(_old_space, _info.space());
	    DBGTHRD(<<"replace page ");
	    _h->install(_info); 
	}
	 _page = p;
	DBGTHRD(<<"replaced old page with " 
		<< _page->pid().page
		<< " refcount= " << _h->refcount
		);
	 // establish new page in table:
	 _h->_find_page(true, _page->pid().page, 
		_found_in_table, _info);
    }
}

void  
histoid_update_t::remove() 
{
    // called when page is deallocated
    if(_page && _page->pid().page) {
	DBGTHRD(<<"histoid_update_t remove() (page free)");
	_h->_find_page(false, _page->pid().page,
		_found_in_table, _info);
	w_assert3(!_found_in_table);
	w_assert3(_info.page() == _page->pid().page);
    }
    DBGTHRD(<<"remove page " << _page->pid().page
	    << " refcount= " << _h->refcount);
    _page = 0;
    _found_in_table = 0;
    DBGTHRD(<<"bucket change for page " << _info.page());
    w_assert3(_h);
    _h->bucket_change(_old_space, _info.space());
    _info.set(0,0); // unknown size
    if(_h->release()) { 
	delete _h; 
    }
    _h = 0;
}

void  
histoid_update_t::update() 
{
    // called when page is updated
    w_assert3(_h);
    w_assert3(_h->refcount > 0);
    w_assert3(_page->is_fixed());
    w_assert3(_page->latch_mode() == LATCH_EX);

    smsize_t newamt = _page->usable_space();

    DBGTHRD(<<"update() page " << _page->pid().page
	<< " newamt=" << newamt
	<< " refcount=" << _h->refcount
	);

    DBGTHRD(<<"bucket change for page " << _info.page());
    w_assert3(_info.page() == _page->pid().page);
    _h->bucket_change(_old_space, newamt);
    _h->update_page(_page->pid().page, newamt);
    /*
     * If the page wasn't originally in the heap, it 
     * is now, so be sure we don't try to put it in
     * a 2nd time when this object is destroyed.
     * _found_in_table means a) page was found there, and
     * b) we removed it and so we want to put it back.
     */
    _found_in_table = false;
}

bool 	
histoid_compare_t::match(const pginfo_t& left, const pginfo_t& right) const
{
    if(left.page() == right.page()) {
	return true;
    }
    return false;
}

bool 	
histoid_compare_t::gt(const pginfo_t& left, const pginfo_t& right) const
{
    // return true if left > right
    // So pages with more free space drifts to the top
    // If 2 pages have same free space, but one 
    // is presently in the buffer pool, it drifts to the top.

#if defined(NOTDEF)
    DBGTHRD(<<"histoid_compare_t::gt left=" 
	<< left.page() << ":" << left.space()
	<< " right= " 
	<< right.page() << ":" << right.space()
	);
#endif
    if( left.space() > right.space() ) {
#if defined(NOTDEF)
	DBGTHRD(<< " returning true" );
#endif
	return true;
    }
#if defined(NOTDEF)
    DBGTHRD(<< " returning false" );
#endif
    return false;

#ifdef COMMENT
// TODO: figure out how to include this -- the problem
// with it is that when the HeapProperty is being checked,
// it calls the same function.
// Might have to pass a flag or create a different
// function for use with HeapProperty

    if( left.space() < right.space() ) return false;

    // Both pages have same space
    lpid_t p(key(), right.page());
    if ( !smlevel_0::bf->has_frame(p)) return true;

    // right is cached.  If left is cached, they are truly
    // equal; if left is not cached, right > left; in either
    // case, left is not > right, so we can return false.
    return false;
#endif /* COMMENT */
}

bool 	
histoid_compare_t::ge(const pginfo_t& left, const pginfo_t& right) const
{
    // return true if left >= right

#if defined(NOTDEF)
    DBGTHRD(<<"histoid_compare_t::ge left=" 
	<< left.page() << ":" << left.space()
	<< " right= " 
	<< right.page() << ":" << right.space()
	);
#endif
    if( left.space() >= right.space() ) {
#if defined(NOTDEF)
	DBGTHRD(<< " returning true" );
#endif
	return true;
    }
#if defined(NOTDEF)
    DBGTHRD(<< " returning false" );
#endif
    return false;
}

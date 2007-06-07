/*<std-header orig-src='shore' incl-file-exclusion='HISTO_H'>

 $Id: histo.h,v 1.8 2007/05/18 21:43:25 nhall Exp $

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

#ifndef HISTO_H
#define HISTO_H

#include "w_defines.h"

/*  -- do not edit anything above this line --   </std-header>*/


/*
 * This file describes the classes that keep histograms describing
 * space utilization in a file.  The SM keeps a single transient, fixed-
 * size hash table, keyed on store ID.  In the table are elements that
 * contain
 *   1) a histogram for the space utilization for that store
 *   2) a heap of pages recently used for that store.
 * 
 * The hash table is protected by a single mutex for insertion and
 * removal of elements.  The individual elements are protected by
 * a mutex and a reference counter.  If a transaction is using the
 * element, the reference counter is > 0.   When a transaction is "using"
 * the element, the tx's store descriptor cache has a pointer to the
 * element.  When an sdesc_t get invalidated (via its method invalidate()),
 * the ref count is decremented and the sdesc_t's ptr is invalidated. 
 *
 * Entries are put into the table when a tx first "encounters" a
 * store that isn't already in the table.  Old entries are purged quasi-LRU,
 * assuming their reference counts are 0.
 *
 * The protocol for append/truncate/destroy is
 *	lock record(EX), lock page(IX)
 *	latch page(EX)
 *	grab histo, insert page entry if necessary, grab
 * 		mutex on page entry
 *	update record
 *	if(empty page) {
 *          free it
 *	    remove it from histo
 * 	} else {
 *	    update page entry in histo
 *	}
 *	unfix page (updates the extent histo)
 *
 * The protocol for free_page (only happens in destroy_rec) is
 *      latch page(EX) already done
 *      lock page(EX) (if we can - else don't free the page - in that
 * 		case we'll unlatch the page and acquire the lock -- see
 *		file_m::_free_page() )
 *	mark page for freeing (extents freed at end-of-xct)
 *
 *  
 * The protocol for create is
 *	grab histo, acquire mutex on suitable page entry
 *	try to latch page(EX) - if no luck, try another page in histo 
 *	try to acquire lock(EX) on record, if fail, unlatch page 
 *		and try another page in histo.
 *
 *	if no suitable pages, try another scheme
 *      create record
 *	update page entry in histo
 *	unfix page (updates extent histogram info)
 */

#include <w.h>
#include <w_heap.h>
#include <w_hash.h>
#include <w_factory_fast.h>
#include <sm_int_2.h>
#include <page_h.h>

#ifdef __GNUG__
#pragma interface
#endif

template <class T, class Cmp>
class SearchableHeap :  public Heap<T,Cmp>
{
    using Heap<T,Cmp>::elements; 
    using Heap<T,Cmp>::numElements;
    using Heap<T,Cmp>::LeftChild;
    using Heap<T,Cmp>::RightSibling;
    using Heap<T,Cmp>::cmp;
public:
    using Heap<T,Cmp>::HeapProperty;

public:
    SearchableHeap(const Cmp& cmpFunction, int initialNumElements);


    int		Search(int i, const T& t);
		   // Find smallest element that is
		   // Cmp::gt than t, starting at i

    int		Match(const T& t) const;
		   // See if there's a T in the heap already
		   // that matches t, "match" based not on the key,
		   // but on the value.
};

class histoid_compare_t 
{
private:
    stid_t		_key;
public:
    NORET histoid_compare_t(stid_t&s): _key(s) { }
    const stid_t&	key() const { return _key; }
    // Looks at space needed
    bool 		gt(const pginfo_t& left, const pginfo_t& right) const;  
    bool 		ge(const pginfo_t& left, const pginfo_t& right) const;  

    // Looks at pageid
    bool 		match(const pginfo_t& left, const pginfo_t& right) const;  
};


class histoid_update_t;
class sdesc_t; // so sdesc_t method can delete a histoid_t ptr

class histoid_t: public smlevel_0 
{
	friend class histoid_update_t ;
	friend class sdesc_t ;

	enum { 	root = 0, // root of heap
		pages_in_heap = 20, 
		buckets_in_table = 7,
		max_stores_in_table = 21 };
protected:	
	// constructor/destructor are protected because
	// only static member acquire can construct a histoid_t,
	// and only member _victimize can destroy a histoid_t.

	NORET 		histoid_t (stid_t s);
	NORET 		~histoid_t ();

	/* 
	 * Heap manip methods for holders of the histoid_t :
	 */
	enum { 	
		nohook = -1 // bad hook - hook not found
		};


        // called from class histoid_update_t 
	void		install(const pginfo_t& info);

        // called from class histoid_update_t 
	void		update_page(const shpid_t& p, smsize_t amt);

public:
	/*
	 * constructor/destructor -called when file mgr is initialized
	 */
	static rc_t 	initialize_table();
	static void 	destroy_table();
	/*
	 * When a tx first encounters a store (dir_m::access)
	 * it puts the store in the hash table and keeps a (ref-counted)
	 * pointer to the histoid_t in the store descriptor;  when the
	 * store descriptor returned by dir_m::access is 
	 * invalidated, the histoid_t is released.
	 */
	histoid_t* 	  copy() const;
	static histoid_t* acquire(const stid_t& s);
	bool 		  release(); //NB: release is not static
	/*
	 * When a store is destroyed, it's taken out of the
	 * hash table:
	 */
	static void 	destroyed_store(const stid_t&s, sdesc_t *sd);

	void		bucket_change(smsize_t old_space,
					smsize_t new_space);
	w_rc_t		exists_page(
			    smsize_t space_needed,
			    bool&	found
			) const; // according to histogram 

	//for space allocation:
	w_rc_t		find_page(
			    smsize_t 	space_needed,
			    bool&		found,
			    pginfo_t& 	info, 
			    file_p*		pagep,
			    slotid_t&	idx	// output iff found
			) const;
	w_rc_t		latch_lock_get_slot(
			    shpid_t&	shpid,
			    file_p*		pg, 
			    smsize_t	space_needed,
			    bool		append_only,
			    bool&		success, // output
			    slotid_t&	idx // output
			) const;

	ostream		&print(ostream &) const;
	static ostream	&print_cache(ostream &, bool locked = false);

private:
	// these mutex methods violate const-ness:
	void _grab_mutex() const; 
	void _grab_mutex_cond(bool &) const; 
	bool _have_mutex() const; 
	void _release_mutex() const;

	bool 		_in_hash_table() const {
			    return (this->link.member_of() != 0);
			}
	void		_insert_store_pages(const stid_t& s);
	static void	_victimize(int howmany);
	int 		__find_page(
			    bool		insert_if_not_found,
			    const shpid_t& 	pid
			);
	void		_find_page(
				bool	 	create_if_not_found, 
				const shpid_t& 	pid,
			        bool&		found_in_table, // out
				pginfo_t& 	info// output
			);

	W_FASTNEW_CLASS_DECL(histoid_t);


	// DATA
        histoid_compare_t cmp;
	int		refcount;
        SearchableHeap<pginfo_t, histoid_compare_t>* pgheap;
	store_histo_t	histogram;
	smutex_t	mutex;
	w_link_t	link;

	/* static stuff */
	static smutex_t	htab_mutex;
	static w_hash_t<histoid_t, stid_t> *	htab;
	static bool 	initialized;

 private: /* disabled */
	histoid_t(const histoid_t &);
	histoid_t &operator=(const histoid_t &);
};

extern ostream	&operator<<(ostream&, const histoid_t&);

class file_p;

class histoid_update_t 
{
    /*
     * This class implements part the protocol for
     * append/truncate/create, where we
     * zoom in on the page immediately, with
     * lock already held and page already pinned.
     */
private:
    bool		_found_in_table;
    file_p*		_page;
    histoid_t*		_h;
    pginfo_t		_info;
    smsize_t		_old_space;

public:
    NORET histoid_update_t(sdesc_t *sd);

    NORET histoid_update_t(file_p& pg);

    NORET ~histoid_update_t();

    void  replace_page(file_p *p, bool reinstall = false);

    void  remove();
    void  update();
    friend ostream &operator<<(ostream&, const histoid_update_t&);
};

/*<std-footer incl-file-exclusion='HISTO_H'>  -- do not edit anything below this line -- */

#endif          /*</std-footer>*/


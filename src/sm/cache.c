/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

/*
 *  $Id: cache.c,v 1.12 1996/04/30 19:27:55 bolo Exp $
 */

#define SM_SOURCE
#define CACHE_C

#ifdef __GNUG__
#pragma implementation "cache.h"
#endif

#include <basics.h>

#ifdef MULTI_SERVER

#include <sm_int.h>
#include "lock_s.h"
#include "lock_x.h"
#include "srvid_t.h"
#include "xct.h"
#include "cache.h"
#include <auto_release.h>

#ifdef __GNUG__
template class w_list_t<page_copies_t>;
template class w_list_t<file_copies_t>;
template class w_list_t<race_t>;
template class w_list_i<page_copies_t>;
template class w_list_i<file_copies_t>;
template class w_list_i<race_t>;
template class w_hash_t<page_copies_t, lpid_t>;
template class w_hash_t<file_copies_t, lockid_t>;
template class w_hash_t<race_t, lpid_t>;
template class w_hash_i<page_copies_t, lpid_t>;
template class w_hash_i<file_copies_t, lockid_t>;
template class w_hash_i<race_t, lpid_t>;
#endif


smutex_t				cache_m::_mutex("copy_mutex");
w_hash_t<page_copies_t, lpid_t>*	cache_m::_page_htab = 0;
w_hash_t<file_copies_t, lockid_t>*	cache_m::_file_htab = 0;
w_hash_t<race_t, lpid_t>*		cache_m::_read_race_htab = 0;
w_hash_t<race_t, lpid_t>*		cache_m::_purge_race_htab = 0;
u_long					cache_m::num_copies = 0;


W_FASTNEW_STATIC_DECL(page_copies_t, 2048);


cache_m::cache_m(int psz, int fsz, int rsz)
{
    _page_htab = new w_hash_t<page_copies_t, lpid_t>(psz,
		offsetof(page_copies_t, name), offsetof(page_copies_t, link));
    if (!_page_htab) { W_FATAL(eOUTOFMEMORY); }

    _file_htab = new w_hash_t<file_copies_t, lockid_t>(fsz,
		offsetof(file_copies_t, name), offsetof(file_copies_t, link));
    if (!_file_htab) { W_FATAL(eOUTOFMEMORY); }

    _purge_race_htab = new w_hash_t<race_t, lpid_t>(rsz, offsetof(race_t, name),
					offsetof(race_t, link));
    if (!_purge_race_htab) { W_FATAL(eOUTOFMEMORY); }

    _read_race_htab = new w_hash_t<race_t, lpid_t>(rsz, offsetof(race_t, name),					offsetof(race_t, link));
    if (!_read_race_htab) { W_FATAL(eOUTOFMEMORY); }
}


cache_m::~cache_m()
{
    delete _page_htab;
    delete _file_htab;
    delete _purge_race_htab;
    delete _read_race_htab;
}


void
cache_m::delete_copy(const lpid_t& pid, const srvid_t& st)
{
    TRACE(301, "deleting copy: page: " << pid << " server: " << st);
    w_assert3(st.is_valid() && !st.is_me());

    MUTEX_ACQUIRE(_mutex);
    page_copies_t* page = _page_htab->lookup(pid);

    if (!page) { W_FATAL(eCOPYNOTEXISTS); }
    if (!bm_is_set(page->sitemap, st.id)) { W_FATAL(eCOPYNOTEXISTS); }

    /*
     * Are there any read-purge races registered?
     */
    if (_purge_race_htab->num_members() > 0) {
	/*
	 * If there is a read-purge race for the "pid" page and the "st"
	 * server, then decrement the counter for that race and ignore
	 * this delete request.
	 */
        if (_delete_race(_purge_race_htab, pid, st, 1)) {
	    MUTEX_RELEASE(_mutex);
	    return;
	}
    }

    bm_clr(page->sitemap, st.id);
    num_copies--;
    if (bm_num_set(page->sitemap, max_servers) == 0) {
	_page_htab->remove(page);
	delete page;
    }

    _delete_file(pid.stid(), st, 1);
    _delete_file(pid.vol(), st, 1);

    MUTEX_RELEASE(_mutex);
    return;
}


/***************************************************************************
 * "decrement" is the amount by which the race counter for the ("pid" , "st")
 * should be decremented. If "decrement" is < 0 then the counter should be
 * zeroed and the race unregistered.
 **************************************************************************/
void
cache_m::delete_read_race(const lockid_t& name, const srvid_t& st,int decrement)
{
    if (name.lspace()==lockid_t::t_record || name.lspace()==lockid_t::t_page) {

	lpid_t pid = name.pid();

        int actual_decrement = _delete_race(_read_race_htab, pid, st,decrement);

        w_assert3(decrement < 0 || actual_decrement == decrement);

        if (actual_decrement > 0) {
            _delete_file(pid.stid(), st, actual_decrement);
            _delete_file(pid.vol(), st, actual_decrement);
        }
    } else {
	// TODO: scann the whole read-race table looking for races on pages
	// belonging to the file or volume being called back. Drop the 
	// qualifying races completely from the race table.
	if (_read_race_htab->num_members() > 0) {
	    W_FATAL(eNOTIMPLEMENTED);
	}
    }
}


int
cache_m::_delete_race(w_hash_t<race_t, lpid_t>* race_tab,
			const lpid_t& pid, const srvid_t& st, int decrement)
{
    int ret = 0;
    race_t* race = race_tab->lookup(pid);

    if (race && race->sitemap[st.id] > 0) {
	w_assert3(((int) race->sitemap[st.id]) >= decrement);

        if (decrement < 0) {
	    ret = race->sitemap[st.id];
	    race->sitemap[st.id] = 0;
	} else {
	    ret = decrement;
	    race->sitemap[st.id] -= decrement;
	}

        if (race->sitemap[st.id] == 0) race->numsites--;
        if (race->numsites == 0) {
            race_tab->remove(race);
            delete race;
        }
    }
    return ret;
}


void
cache_m::_delete_file(const lockid_t& name, const srvid_t& st, int decrement)
{
    file_copies_t* file = _file_htab->lookup(name);

    if (!file) { W_FATAL(eCOPYNOTEXISTS); }
    if (file->sitemap[st.id] == 0) { W_FATAL(eCOPYNOTEXISTS); }
    w_assert3(decrement > 0 && file->sitemap[st.id] >= decrement);

    file->sitemap[st.id] -= decrement;
    if (file->sitemap[st.id] == 0) file->numsites--;
    if (file->numsites == 0) {
        _file_htab->remove(file);
        delete file;
    }
    return;
}


void cache_m::register_copy(const lpid_t& pid, const srvid_t& st,
				bool detect_purge_race, bool read_race)
{
    TRACE(302, "registering copy: page: " << pid << " server: " << st <<
	" read_race: " << (read_race ? 1 : 0)
    );
    w_assert3(st.is_valid() && !st.is_me());

    bool mutex_acquired = FALSE;
    if (cc_alg == t_cc_page || (! MUTEX_IS_MINE(_mutex))) {
	MUTEX_ACQUIRE(_mutex);
	mutex_acquired = TRUE;
    } else {
	w_assert3(cc_alg == t_cc_record);
    }

    if (read_race) {
	/*
	 * Just register the race, but not the page. 
	 */
	_register_read_race(pid, st);

    } else {
	page_copies_t* page = _page_htab->lookup(pid);

        if (!page) {
            page = new page_copies_t(pid);
            w_assert1(page);
            _page_htab->push(page);
        }

        if (!bm_is_set(page->sitemap, st.id)) {
	    w_assert3(detect_purge_race);
    	    bm_set(page->sitemap, st.id);
	    num_copies++;
	    _register_file(pid.stid(), st);
	    _register_file(pid.vol(), st);

        } else {
	    /*
	     * The page is already registered in the copy table.
	     */
	    if (detect_purge_race) {
	        // purge race condition detected:
		// this should not occur with the current thread and comm setup.
		cout << "WARNING WARNING WARNING : copy table race detected"
		     << "for page: " << pid << endl;

	        _register_race(_purge_race_htab, pid, st);
	    }
        }

        if (_read_race_htab->num_members() > 0) {
            /*
             * If there is a read-read race for the "pid" page and the
	     * "st" server, then unregister that race.
             */
            delete_read_race(pid, st, -1);
        }
    }

    if (mutex_acquired) MUTEX_RELEASE(_mutex);
    return;
}


void
cache_m::_register_read_race(const lpid_t& pid, const srvid_t& st)
{
    _register_race(_read_race_htab, pid, st);

    _register_file(pid.stid(), st);
    _register_file(pid.vol(), st);
}


void
cache_m::_register_race(w_hash_t<race_t, lpid_t>* race_tab,
					const lpid_t& name, const srvid_t& st)
{
    race_t* race = race_tab->lookup(name);

    if (!race) {
        race = new race_t(name);
        w_assert1(race);
        race_tab->push(race);
    }

    if (race->sitemap[st.id] == 0)  race->numsites++;
    race->sitemap[st.id]++;

    return;
}


void
cache_m::_register_file(const lockid_t& name, const srvid_t& st)
{
    file_copies_t* file = _file_htab->lookup(name);

    if (!file) {
	file = new file_copies_t(name);
	w_assert1(file);
	_file_htab->push(file);
    }

    if (file->sitemap[st.id] == 0)  file->numsites++;
    file->sitemap[st.id]++;

    return;
}


bool
cache_m::query(const lockid_t& name, const srvid_t& site)
{
    MUTEX_ACQUIRE(_mutex);
#ifndef NOT_PREEMPTIVE
    auto_release_t<smutex_t> dummy(_mutex);
#endif
    uint2 level = name.lspace();
    if (level == lockid_t::t_record) level = lockid_t::t_page;

    if (level == lockid_t::t_page) {

        page_copies_t* copies = _page_htab->lookup(*(lpid_t*)name.name());
        if (copies && bm_is_set(copies->sitemap, site.id)) return TRUE;

        if (_read_race_htab->num_members() > 0) {
            race_t* read_races = _read_race_htab->lookup(*(lpid_t*)name.name());
            if (read_races && read_races->sitemap[site.id] > 0) return TRUE;
        }

    } else {
	w_assert3(level <= lockid_t::t_store);
	file_copies_t* copies = _file_htab->lookup(name);

        if (copies && copies->sitemap[site.id] > 0) return TRUE;
    }
    return FALSE;
}


void
cache_m::find_copies(const lockid_t& name,
                cb_site_t* sites, uint4& numsites, const srvid_t& exlude_site)
{
    w_assert3(exlude_site.is_valid() || exlude_site == srvid_t::null);

    numsites = 0;
    bool mutex_acquired = FALSE;
    if (!MUTEX_IS_MINE(_mutex)) {
	MUTEX_ACQUIRE(_mutex);
	mutex_acquired = TRUE;
    }

    uint2 level = name.lspace();
    if (level == lockid_t::t_record) level = lockid_t::t_page;

    if (level == lockid_t::t_page) {
	_find_page_copies(*(lpid_t *)name.name(), sites, numsites, exlude_site);
    } else {
	w_assert3(level <= lockid_t::t_store);
	_find_file_copies(name, sites, numsites, exlude_site);
    }
    if (mutex_acquired) MUTEX_RELEASE(_mutex);
}


void
cache_m::_find_page_copies(const lpid_t& name,
		cb_site_t* sites, uint4& numsites, const srvid_t& exlude_site)
{
    page_copies_t* copies = _page_htab->lookup(name);

    if (copies) {
        for (int i = 1; i < max_servers; i++) {
            if (bm_is_set(copies->sitemap, i) && i != exlude_site.id) {
                sites[numsites].srv = i;
		sites[numsites].races = 0;
                numsites++;
            }
        }
    }

    if (_read_race_htab->num_members() > 0) {
        race_t* read_races = _read_race_htab->lookup(name);

	if (read_races) {
	    for (int i = 1; i < max_servers; i++) {
		if (read_races->sitemap[i] > 0) {
		    for (int j = 0; j < numsites && sites[j].srv != i; j++);
		    if (j < numsites) {
			sites[j].races = read_races->sitemap[i];
		    } else {
			TRACE(306, "WARNING WARNING WARNING: callback to be sent
			    due to races" << " for page: " << name << endl
			);
			sites[numsites].srv = i;
			sites[numsites].races = read_races->sitemap[i];
			numsites++;
		    }
		}
	    }
	}
    }

    if (copies == 0) {
	if (xct()->is_remote_proxy()) {
	    W_FATAL(eNOCOPIES);	// this check is valid only if records are
	}			// always read before they are updated.
    }
}


void cache_m::_find_file_copies(const lockid_t& name,
		cb_site_t* sites, uint4& numsites, const srvid_t& exlude_site)
{
    file_copies_t* copies = _file_htab->lookup(name);

    if (copies) {
        for (int i = 1; i < max_servers; i++) {
            if (copies->sitemap[i] > 0 && i != exlude_site.id) {
                sites[numsites].srv = i;
		sites[numsites].races = 0;
                numsites++;
            }
        }
    }
}


void cache_m::dump_page_table()
{
    uint4 num_pages = 0;
    w_hash_i<page_copies_t, lpid_t> iter(*_page_htab);
    page_copies_t* page = 0;
    // W_COERCE(_mutex.acquire());
    cout << "PAGE COPY TABLE:" << endl;
    while (page = iter.next())  {
        cout << *page << endl;
	num_pages++;
    }
    cout << "Total #pages registered : " << num_pages << endl;
    cout << "Total #copies : " << num_copies << endl << endl;
    // _mutex.release();
}


void cache_m::dump_file_table()
{
    uint4 num_entries = 0;
    w_hash_i<file_copies_t, lockid_t> iter(*_file_htab);
    file_copies_t* file = 0;
    // W_COERCE(_mutex.acquire());
    cout << "FILE COPY TABLE:" << endl;
    while (file = iter.next())  {
        cout << *file << endl;
        num_entries++;
    }
    cout << "Total #files/volumes registered : " << num_entries << endl << endl;
    // _mutex.release();
}


void cache_m::dump_read_race_table()
{
    cout << "READ RACES:" << endl;
    _dump_race_table(_read_race_htab);
}


void cache_m::dump_purge_race_table()
{
    cout << "PURGE RACES:" << endl;
    _dump_race_table(_purge_race_htab);
}


void cache_m::_dump_race_table(w_hash_t<race_t, lpid_t>* race_tab)
{
    uint4 num_races = 0;
    w_hash_i<race_t, lpid_t> iter(*race_tab);
    race_t* race = 0;
    // W_COERCE(_mutex.acquire());
    while (race = iter.next())  {
	cout << *race << endl;
	num_races++;
    }
    cout << "Total #races registered : " << num_races << endl << endl;
    // _mutex.release();
}


ostream& operator<<(ostream& o, const page_copies_t& c)
{
    o << c.name << " site list: ";
    for (int i = 1; i < smlevel_0::max_servers; i++) {
        if (bm_is_set(c.sitemap, i))   o << i << ' ';
    }
    return o;
}

ostream& operator<<(ostream& o, const file_copies_t& c)
{
    o << c.name << " site list: ";
    for (int i = 1; i < smlevel_0::max_servers; i++) {
        if (c.sitemap[i] > 0)   o << "(" << i << ", " << c.sitemap[i] << ")  ";
    }
    return o;
}

ostream& operator<<(ostream& o, const race_t& c)
{
    o << c.name << " site list: ";
    for (int i = 1; i < smlevel_0::max_servers; i++) {
	if (c.sitemap[i] > 0)   o << "(" << i << ", " << c.sitemap[i] << ")  ";
    }
    return o;
}

#endif /* MULTI_SERVER */

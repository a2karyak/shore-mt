/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

/*
 *  $Id: cache.h,v 1.1
 */

#ifndef CACHE_H
#define CACHE_H

#ifdef __GNUG__
#pragma interface
#endif


#ifndef MULTI_SERVER

class cache_m: public smlevel_1 {
public:
    NORET               cache_m(int sz);
    NORET               ~cache_m();
private:
};

#else /* MULTI_SERVER */


//
// Info about which remote servers cache a page owned by this server.
//
class page_copies_t {
public:
    w_link_t	link;
    lpid_t	name;		// id of page cached remotely.

    // bitmap: i-th bit == 1 <==> server i caches the page
    u_char	sitemap[smlevel_0::srvid_map_sz];

    NORET	page_copies_t(const lpid_t& n);
    NORET	~page_copies_t() { link.detach(); }

    friend ostream& operator<<(ostream&, const page_copies_t& c);

    W_FASTNEW_CLASS_DECL;

private:
    // disabled
    NORET		page_copies_t(page_copies_t&);
    page_copies_t&	operator=(const page_copies_t&);
};


//
// Info about which remote servers cache a file OR volume owned by this server.
// A file or volume is "cached" at a remote server if any pages of the file or
// volume are cached at the remote server.
//
class file_copies_t {
public:
    w_link_t	link;
    lockid_t	name;		// id of file or vol cached remotely.
    uint2	numsites;	// number of remote srvs caching the file or vol

    uint4	sitemap[smlevel_0::max_servers];	// num of pages cached
							// at each remote server

    // TODO: in order to save space, the implementation of the sitemap may
    // change to a list of sites actually caching the file or volume.

    NORET       file_copies_t(const lockid_t& n);
    NORET       ~file_copies_t() { link.detach(); }

    friend ostream& operator<<(ostream&, const file_copies_t& c);

private:
    // disabled
    NORET               file_copies_t(file_copies_t&);
    file_copies_t&      operator=(const file_copies_t&);
};


class race_t {
public:
    w_link_t	link;
    lpid_t	name;
    uint2	numsites;
    uint4	sitemap[smlevel_0::max_servers];

    NORET	race_t(const lpid_t& n);
    NORET	~race_t() { link.detach(); }

    friend ostream& operator<<(ostream&, const race_t& c);
};


struct cb_site_t {
    int		srv;
    int		races;
};


//
// Implementation of hierarchical copy table.
//
class cache_m: public smlevel_1 {
public:
    enum { max_servers = smlevel_0::max_servers };

    NORET		cache_m(int psz, int fsz = 100, int rsz = 10);
    NORET		~cache_m();

    void		mutex_acquire() { MUTEX_ACQUIRE(_mutex); }
    void		mutex_release() { MUTEX_RELEASE(_mutex); }

    static void		register_copy(
	const lpid_t&		pid,
	const srvid_t&		st,
	bool			detect_purge_race,
	bool			read_race);

    static void		delete_copy(
	const lpid_t&		pid,
	const srvid_t&		st);

    static void		delete_read_race(
	const lockid_t&		name,
	const srvid_t&		st,
	int			decrement);

    static bool		query(
	const lockid_t&		name,
	const srvid_t&		site);

    static void		find_copies(
	const lockid_t&		name,
	cb_site_t*		sites,
	uint4&			numsites,
	const srvid_t&		exlude_site);

    static void		dump_page_table();
    static void		dump_file_table();
    static void		dump_read_race_table();
    static void		dump_purge_race_table();

    static u_long	num_copies;

private:
    static void		_register_read_race(
	const lpid_t&		pid,
	const srvid_t&		st);

    static void		_register_race(
	w_hash_t<race_t, lpid_t>* race_tab,
	const lpid_t&		name,
	const srvid_t&		st);

    static int		_delete_race(
	w_hash_t<race_t, lpid_t>* race_tab,
	const lpid_t&		pid,
	const srvid_t&		st,
	int			decrement);

    static void		_register_file(
	const lockid_t&		name,
	const srvid_t&		st);

    static void		_delete_file(
	const lockid_t&		name,
	const srvid_t&		st,
	int			decrement);

    static void		_find_page_copies(
	const lpid_t& name,
	cb_site_t* sites,
	uint4& numsites,
	const srvid_t& exlude_site);

    static void		_find_file_copies(
	const lockid_t& name,
	cb_site_t* sites,
	uint4& numsites,
	const srvid_t& exlude_site);

    static void         _dump_race_table(w_hash_t<race_t, lpid_t>* race_tab);

    static smutex_t				_mutex;
    static w_hash_t<page_copies_t, lpid_t>*	_page_htab;
    static w_hash_t<file_copies_t, lockid_t>*	_file_htab; //for files AND vols

    static w_hash_t<race_t, lpid_t>*		_read_race_htab;
    static w_hash_t<race_t, lpid_t>*		_purge_race_htab;
};


inline NORET
page_copies_t::page_copies_t(const lpid_t& n): name(n)
{
    bm_zero(sitemap, smlevel_0::max_servers);
}   


inline NORET
file_copies_t::file_copies_t(const lockid_t& n): name(n), numsites(0)
{
    for(int i = 0; i < smlevel_0::max_servers; i++) sitemap[i] = 0;
}  


inline NORET 
race_t::race_t(const lpid_t& n): name(n), numsites(0)
{
    for(int i = 0; i < smlevel_0::max_servers; i++) sitemap[i] = 0;
}

#endif /* MULTI_SERVER */

#endif /*CACHE_H*/


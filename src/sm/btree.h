/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

/*
 *  $Id: btree.h,v 1.115 1996/07/09 20:41:16 nhall Exp $
 */
#ifndef BTREE_H
#define BTREE_H

#ifdef __GNUG__
#pragma interface
#endif

class sort_stream_i;

class btree_p;
class btrec_t;
struct btree_stats_t;

class bt_cursor_t : smlevel_2 {
public:
    NORET			bt_cursor_t();
    NORET			~bt_cursor_t() ;

    rc_t			set_up(
	const lpid_t& 		    root, 
	int			    nkc,
	const key_type_s*	    kc,
	bool			    unique,
	concurrency_t		    cc,
	cmp_t			    cond2,
	const cvec_t&		    bound2);

    rc_t			set_up_part_2(
	cmp_t			    cond1,
	const cvec_t&		    bound1
	);
	
    lpid_t			root()	 const { return _root; }
    const lpid_t&		pid()	 const { return _pid; }
    const lsn_t&		lsn()	 const { return _lsn; }
    int				slot()   const { return _slot; }
    bool			first_time;
    bool			keep_going;
    bool			unique() const { return _unique; }
    concurrency_t		cc()	 const { return _cc; }
    int				nkc()	 const { return _nkc; }
    const key_type_s*		kc()	 const { return _kc; }
    cmp_t			cond1()	 const { return _cond1;}
    const cvec_t& 		bound1() const { return *_bound1;}
    cmp_t			cond2()	 const { return _cond2;}
    const cvec_t& 		bound2() const { return *_bound2;}

    bool			is_valid() { return _slot >= 0; }
    rc_t 			make_rec(const btree_p& page, int slot);
    void 			free_rec();
    int 			klen()   { return _klen; }
    char*			key()	 { return _klen ? _space : 0; }
    int				elen()	 { return _elen; }
    char*			elem()	 { return _klen ? _space + _klen : 0; }

    void			delegate(void*& ptr, int& kl, int& el);

private:
    lpid_t			_root;
    bool			_unique;
    smlevel_0::concurrency_t	_cc;
    int				_nkc;
    const key_type_s*		_kc;

    int				_slot;
    char*			_space;
    int				_splen;
    int				_klen;
    int				_elen;
    lsn_t			_lsn;
    lpid_t			_pid;
    cmp_t			_cond1;
    char*			_bound1_buf;
    cvec_t*			_bound1;
    cvec_t			_bound1_tmp; // used if cond1 is not
    					     // pos or neg_infinity

    cmp_t			_cond2;
    char*			_bound2_buf;
    cvec_t*			_bound2;
    cvec_t			_bound2_tmp; // used if cond2 is not
    					     // pos or neg_infinity
};

inline NORET
bt_cursor_t::bt_cursor_t()
    : first_time(false), keep_going(true), _slot(-1), 
      _space(0), _splen(0), _klen(0), _elen(0), 
      _bound1_buf(0), _bound2_buf(0)
{
}

inline NORET
bt_cursor_t::~bt_cursor_t()
{
    if (_space)  {
	delete[] _space;
	_space = 0;
    }
    if (_bound1_buf) {
	delete[] _bound1_buf;
	_bound1_buf = 0;
    }
    if (_bound2_buf) {
	delete[] _bound2_buf;
	_bound2_buf = 0;
    }
    _slot = -1;
    _pid = lpid_t::null;
}

inline void 
bt_cursor_t::free_rec()
{
    _klen = _elen = 0;
    _slot = -1;
    _pid = lpid_t::null;
}

inline void
bt_cursor_t::delegate(void*& ptr, int& kl, int& el)
{
    kl = _klen, el = _elen;
    ptr = (void*) _space;
    _space = 0; _splen = 0;
}

class bt_cursor_t;
class btsink_t;
class file_p;
class record_t;

/*--------------------------------------------------------------*
 *  class btree_m					        *
 *--------------------------------------------------------------*/
class btree_m : public smlevel_2 {
public:
    NORET			btree_m()   {};
    NORET			~btree_m()  {};

    static smsize_t		max_entry_size(); 

    typedef bt_cursor_t cursor_t;

    static rc_t			create(
	stpgid_t 		    stpgid,
	lpid_t& 		    root);
    static rc_t			bulk_load(
	const lpid_t& 		    root,
	stid_t			    src,
        int			    nkc,
        const key_type_s*	    kc,
	bool 			    unique,
	concurrency_t		    cc,
	btree_stats_t&		    stats);
    static rc_t			bulk_load(
	const lpid_t& 		    root,
	sort_stream_i&		    sorted_stream,
        int			    nkc,
        const key_type_s*	    kc,
	bool 			    unique,
	concurrency_t		    cc,
	btree_stats_t&		    stats);
    static rc_t			insert(
	const lpid_t& 		    root,
        int			    nkc,
        const key_type_s*	    kc,
	bool 			    unique,
	concurrency_t		    cc,
	const cvec_t& 		    key,
	const cvec_t& 		    elem,
	int 			    split_factor = 50);
    static rc_t			remove(
	const lpid_t&		    root,
        int			    nkc,
        const key_type_s*	    kc,
	bool 			    unique,
	concurrency_t		    cc,
	const cvec_t& 		    key,
	const cvec_t& 		    elem);
    static rc_t			remove_key(
	const lpid_t&		    root,
        int			    nkc,
        const key_type_s*	    kc,
	bool 			    unique,
	concurrency_t		    cc,
	const cvec_t& 		    key,
    	int&		    	    num_removed);
    static void 		print_key_str(const lpid_t& root,
					bool print_elem=true);

    static void 		print_key_uint4(const lpid_t& root);
    static void 		print_key_uint2(const lpid_t& root);
    static void 		print(const lpid_t& root);
    static rc_t			lookup(
	const lpid_t& 		    root, 
        int			    nkc,
        const key_type_s*	    kc,
	bool 			    unique,
	concurrency_t		    cc,
	const cvec_t& 		    key_to_find, 
	void*			    el, 
	smsize_t& 		    elen,
	bool& 		    found);
    static rc_t			lookup_prev(
	const lpid_t& 		    root, 
        int			    nkc,
        const key_type_s*	    kc,
	bool 			    unique,
	concurrency_t		    cc,
	const cvec_t& 		    key, 
	bool& 		            found,
	void*	                    key_prev,
	smsize_t&	            key_prev_len);
    static rc_t			fetch_init(
	cursor_t& 		    cursor, 
	const lpid_t& 		    root,
        int			    nkc,
        const key_type_s*	    kc,
	bool 			    unique, 
	concurrency_t		    cc,
	const cvec_t& 		    key, 
	const cvec_t& 		    elem,
	cmp_t			    cond1,
	cmp_t               	    cond2,
	const cvec_t&		    bound2);
    static rc_t			fetch_reinit(cursor_t& cursor); 
    static rc_t			fetch(cursor_t& cursor);
    static rc_t			is_empty(const lpid_t& root, bool& ret);
    static rc_t			purge(const lpid_t& root, bool check_empty);
    static rc_t 		get_du_statistics(
	const lpid_t&		    root, 
	btree_stats_t&    	    btree_stats,
	bool			    audit);

    // this function is used by ss_m::_convert_to_store
    // to copy a 1-page btree to a new root in a store
    static rc_t			copy_1_page_tree(
	const lpid_t&		    old_root, 
	const lpid_t&		    new_root);
    
protected:
    static rc_t			_alloc_page(
	const lpid_t& 		    root,
	int2 			    level,
	const lpid_t& 		    near,
	btree_p& 		    ret_page,
	shpid_t 		    pid0 = 0);
private:
    static rc_t			_fetch_init(
	cursor_t& 		    cursor, 
	cvec_t* 		    key, 
	const cvec_t& 		    elem);

    static rc_t			_insert(
	const lpid_t& 		    root,
	bool 			    unique,
	concurrency_t		    cc,
	const cvec_t& 		    key,
	const cvec_t& 		    elem,
	int 			    split_factor = 50);
    static rc_t			_remove(
	const lpid_t&		    root,
	bool 			    unique,
	concurrency_t		    cc,
	const cvec_t& 		    key,
	const cvec_t& 		    elem);
    static rc_t			_lookup(
	const lpid_t& 		    root, 
	bool 			    unique,
	concurrency_t		    cc,
	const cvec_t& 		    key_to_find, 
	void*			    el, 
	smsize_t& 		    elen,
	bool& 		    found);

    static void			_scramble_key(
	cvec_t*&		    ret,
	const cvec_t&		    key, 
        int 			    nkc,
	const key_type_s*	    kc);

    static void			_unscramble_key(
	cvec_t*&		    ret,
	const cvec_t&		    key, 
        int 			    nkc,
	const key_type_s* 	    kc);

    static bool 		_check_duplicate(
	const cvec_t&		    key,
	btree_p& 		    leaf,
	int			    slot, 
	kvl_t* 			    kvl);
    static rc_t			_search(
	const btree_p&		    page, 
	const cvec_t&		    key,
	const cvec_t&		    el,
	bool&			    found, 
	int&			    slot);
    static rc_t			_traverse(
	const lpid_t& 		    root,
	const cvec_t& 		    key, 
	const cvec_t& 		    elem,
	bool& 		    	    found,
	latch_mode_t 		    mode,
	btree_p& 		    leaf,
	int& 			    slot);
    static rc_t			_split_page(
	btree_p& 		    page, 
	btree_p& 		    sibling,
	bool& 		            left_heavy,
	int& 			    slot,
	int 			    addition, 
	int 			    split_factor);
    // helper function for _cut_page
    static rc_t			__cut_page(btree_p& parent, int slot);
    static rc_t			_cut_page(btree_p& parent, int slot);
    // helper function for _propagate_split
    static rc_t			__propagate_split(btree_p& parent, int slot);
    static rc_t			_propagate_split(btree_p& parent, int slot);
    static rc_t			_grow_tree(btree_p& root);
    static rc_t			_shrink_tree(btree_p& root);
    
    static void			 _skip_one_slot(
	btree_p& 		    p1,
	btree_p& 		    p2,
	btree_p*& 		    child,
	int& 			    slot,
	bool& 			    eof
	);

    static rc_t                 _handle_dup_keys(
	btsink_t*		    sink,
	int&			    slot,
	file_p*			    prevp,
	file_p*			    curp,
	int& 			    count,
	record_t*&	 	    r,
	lpid_t&			    pid,
	int			    nkc,
    	const key_type_s*	    kc);

    friend class btree_remove_log;
    friend class btree_insert_log;
    friend class bt_cursor_t;
};


inline void 
btree_m::print(const lpid_t& root)
{
    print_key_str(root);
}

#endif /*BTREE_H*/


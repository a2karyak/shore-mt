/*<std-header orig-src='shore' incl-file-exclusion='BF_S_H'>

 $Id: bf_s.h,v 1.42 1999/06/07 19:03:50 kupsch Exp $

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

#ifndef BF_S_H
#define BF_S_H

#include "w_defines.h"

/*  -- do not edit anything above this line --   </std-header>*/

#include <sm_s.h>

class page_s;

class bfpid_t : public lpid_t {
public:
    NORET			bfpid_t();
    NORET			bfpid_t(const lpid_t& p);
    bfpid_t& 			operator=(const lpid_t& p);
    bool 		        operator==(const bfpid_t& p) const;
    static const bfpid_t	null;
};

inline NORET
bfpid_t::bfpid_t()
{
}

inline NORET
bfpid_t::bfpid_t(const lpid_t& p) : lpid_t(p)
{
}

inline bfpid_t&
bfpid_t::operator=(const lpid_t& p)  
{
    *(lpid_t*)this = p;
    return *this;
}

inline bool 
bfpid_t::operator==(const bfpid_t& p) const
{
    return vol() == p.vol() && page == p.page;
}


/*
 *  bfcb_t: buffer frame control block.
 */
struct bfcb_t {
public:
    NORET       bfcb_t()    {};
    NORET       ~bfcb_t()   {};

    void	vtable_collect(vtable_info_t &t);

    inline void	clear();

    w_link_t    link;           // used in hash table

    bfpid_t	pid;            // page currently stored in the frame
    bfpid_t     old_pid;        // previous page in the frame
    bool        old_pid_valid;  // is the previous page in-transit-out?
    page_s*     frame;          // pointer to the frame

    bool	dirty;          // true if page is dirty
    lsn_t       rec_lsn;        // recovery lsn

    int4_t      pin_cnt;        // count of pins on the page
    latch_t     latch;          // latch on the frame
    scond_t     exit_transit;   //signaled when frame exits the in-transit state

    int4_t      refbit;         // ref count (for strict clock algorithm)
				// for replacement policy only

    int4_t      hot;         	// copy of refbit for use by the cleaner algorithm
				// without interfering with clock (replacement)
				// algorithm.

    inline ostream&    print_frame(ostream& o, bool in_htab);
    void 	       update_rec_lsn(latch_mode_t);

private:
    // disabled
    NORET       bfcb_t(const bfcb_t&);
    bfcb_t&     operator=(const bfcb_t&);
};



inline void
bfcb_t::clear() 
{
    pid = lpid_t::null;
    old_pid = lpid_t::null;
    old_pid_valid = false;
    dirty = false;
    rec_lsn = lsn_t::null;
    hot = 0;
    refbit = 0;
    w_assert3(pin_cnt == 0);
    w_assert3(latch.num_holders() <= 1);
}

/*<std-footer incl-file-exclusion='BF_S_H'>  -- do not edit anything below this line -- */

#endif          /*</std-footer>*/

/*<std-header orig-src='shore' incl-file-exclusion='SMSTATS_H'>

 $Id: smstats.h,v 1.33 2003/08/24 23:51:32 bolo Exp $

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

#ifndef SMSTATS_H
#define SMSTATS_H

#include "w_defines.h"

/*  -- do not edit anything above this line --   </std-header>*/

#ifndef W_STATISTICS_H
#include <w_statistics.h>
#endif

// This file is included in sm.h in the middle of the class ss_m
// declaration.  Member functions are defined in sm.cpp


class sm_stats_t {
public:
	void	compute();
#include "sm_stats_t_struct_gen.h"
};

class smthread_stats_t {
public:
	void	compute();
#include "smthread_stats_t_struct_gen.h"
};

class sm2pc_stats_t {
public:
	void	compute();
#include "sm2pc_stats_t_struct_gen.h"
};

class sm_stats_info_t {
public:
	sm_stats_t 	sm;
	smthread_stats_t summary_thread;
	sm2pc_stats_t 	summary_2pc;
	void	compute() { 
		sm.compute(); 
		summary_thread.compute(); 
		summary_2pc.compute(); 
	}
        friend ostream& operator<<(ostream&, const sm_stats_info_t& s);
};

extern sm_stats_info_t &operator+=(sm_stats_info_t &s, const sm_stats_info_t &t);
extern sm_stats_info_t &operator-=(sm_stats_info_t &s, const sm_stats_info_t &t);

struct sm_config_info_t {
    u_long page_size; 		// bytes in page, including all headers
    u_long max_small_rec;  	// maximum number of bytes in a "small"
				// (ie. on one page) record.  This is
				// align(header_len)+align(body_len).
    u_long lg_rec_page_space;	// data space available on a page of
				// a large record
    u_long buffer_pool_size;	// buffer pool size in kilo-bytes
    u_long lid_cache_size;      // # of entries in logical ID cache
    u_long max_btree_entry_size;// max size of key-value pair 
    u_long exts_on_page;        // #extent links on an extent (root) page
    u_long pages_per_ext;	// #page per ext (# bits in Pmap)
    bool   multi_threaded_xct;  // true-> allow multi-threaded xcts
    bool   preemptive;  	// true-> configured for preemptive threads
    bool   serial_bits64;  	// true-> configured with BITS64
    bool   logging;  		// true-> configured with logging on

    friend ostream& operator<<(ostream&, const sm_config_info_t& s);
};

/*<std-footer incl-file-exclusion='SMSTATS_H'>  -- do not edit anything below this line -- */

#endif          /*</std-footer>*/

/*<std-header orig-src='shore'>

 $Id: smstats.cpp,v 1.15 2003/08/24 23:51:32 bolo Exp $

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

#include <w_statistics.h>
#include "smstats.h"
#include "sm_stats_t_inc_gen.cpp"
#include "sm_stats_t_dec_gen.cpp"
#include "sm_stats_t_out_gen.cpp"

// the strings:
const char *sm_stats_t ::stat_names[] = {
#include "sm_stats_t_msg_gen.h"
};

// see sm.cpp for void sm_stats_t::compute()

void 
sm_stats_t::compute()
{
#ifdef EXPENSIVE_LOCK_STATS
    smlevel_0::lm->stats(
		smlevel_0::stats.sm.lock_bucket_cnt,
		smlevel_0::stats.sm.lock_max_bucket_len, 
		smlevel_0::stats.sm.lock_min_bucket_len, 
		smlevel_0::stats.sm.lock_mode_bucket_len, 
		smlevel_0::stats.sm.lock_mean_bucket_len, 
		smlevel_0::stats.sm.lock_var_bucket_len,
		smlevel_0::stats.sm.lock_std_bucket_len
    );
#endif /* EXPENSIVE_LOCK_STATS */

}

sm_stats_info_t &operator+=(sm_stats_info_t &s, const sm_stats_info_t &t)
{
	s.sm += t.sm;
	s.summary_thread += t.summary_thread;
	s.summary_2pc += t.summary_2pc;
	return s;
}

sm_stats_info_t &operator-=(sm_stats_info_t &s, const sm_stats_info_t &t)
{
	s.sm -= t.sm;
	s.summary_thread -= t.summary_thread;
	s.summary_2pc -= t.summary_2pc;
	return s;
}


sm_stats_info_t &operator-=(sm_stats_info_t &s, const sm_stats_info_t &t);

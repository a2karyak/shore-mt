/*<std-header orig-src='shore'>

 $Id: lid.cpp,v 1.153 2007/08/21 19:50:42 nhall Exp $

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
#define LID_C

#ifdef __GNUG__
#pragma implementation "lid.h"
#pragma implementation "lid_s.h"
#endif

#include <sm_int_4.h>
#include <btcursor.h>
#include "auto_release.h"

#ifdef SOLARIS2
#	define	USE_UTSNAME
#	include <sys/utsname.h>
#else
#	include <hostname.h>
#endif

#ifdef _WIN32
#	include <w_winsock.h>
#else
#	include <netdb.h>	/* XXX really should be included for all */
#endif



#ifdef EXPLICIT_TEMPLATE

template class w_hash_i<lid_m::vol_lid_info_t, lvid_t>;
template class w_hash_t<lid_m::vol_lid_info_t, lvid_t>;
template class w_list_t<lid_m::vol_lid_info_t>;
template class w_list_i<lid_m::vol_lid_info_t>;

#endif



lid_m::lid_m(int max_vols
		) :
		_num_vols(max_vols),
		_vol_mutex("lid voltab")
{
    // allocate the volume table
    _vol_table = new w_hash_t<vol_lid_info_t, lvid_t>(
	    _num_vols,
	    W_HASH_ARG(vol_lid_info_t, lvid, link));
    if (!_vol_table)
	W_FATAL(eOUTOFMEMORY);
}

lid_m::~lid_m()
{
    W_COERCE( _vol_mutex.acquire() );
    auto_release_t<smutex_t> dummy(_vol_mutex);
    w_hash_i<vol_lid_info_t, lvid_t> iter(*_vol_table);

    vol_lid_info_t* vol_info = NULL;

    while ( (vol_info = iter.next()) != NULL) {
	_vol_table->remove(vol_info);
	delete vol_info;
    }

    if (_vol_table) {
	delete _vol_table;
	_vol_table = NULL;
    }
    
    return;
}

bool
lid_m::is_mounted(const lvid_t& lvid)
{
    W_COERCE( _vol_mutex.acquire() );
    auto_release_t<smutex_t> dummy(_vol_mutex);

    // Find the record for volume containing the index
    vol_lid_info_t*  vol_info = _lookup_lvid(lvid
	    );

    if (vol_info) return true;

    return false;
}

rc_t
lid_m::_add_volume(vid_t vid, const lvid_t& lvid
	      )
{
    FUNC(lid_m::add_volume);
    vol_lid_info_t* vol_info = NULL;

    // find a place for the logical ID info for this volume
    {
	W_COERCE( _vol_mutex.acquire() );
	auto_release_t<smutex_t> dummy(_vol_mutex);
	vol_info = _lookup_lvid(lvid
		);

	if (!vol_info) {
	    // not found

	    vol_info = new vol_lid_info_t;
	    vol_info->lvid = lvid;
	    vol_info->vid = vid;
	    _vol_table->push(vol_info);
	}
    }

    return RCOK;
}

 
rc_t
lid_m::remove_volume(lvid_t lvid)
{
    W_COERCE( _vol_mutex.acquire() );
    auto_release_t<smutex_t> dummy(_vol_mutex);

    // Find the record for volume containing the index
    vol_lid_info_t*  vol_info = _lookup_lvid(lvid
	    );
    if (vol_info == NULL) {
	/* 
	 * Either the lvid is invalid or not mounted
	 * TODO: try to determine which.
	 */
       return RC(eBADVOL);	 
    }

    // remove the volume entry
    _vol_table->remove(vol_info);    
    delete vol_info;

    return RCOK;
}

rc_t
lid_m::remove_all_volumes()
{
    W_COERCE( _vol_mutex.acquire() );
    auto_release_t<smutex_t> dummy(_vol_mutex);
    w_hash_i<vol_lid_info_t, lvid_t> iter(*_vol_table);

    vol_lid_info_t* vol_info = NULL;

    while ( (vol_info = iter.next()) != NULL) {
	_vol_table->remove(vol_info);    
	delete vol_info;
    }

    return RCOK;
}


rc_t
lid_m::lookup(const lvid_t& lvid, vid_t& vid)
{
    vol_lid_info_t  vol_info;
    W_DO(_get_vol_info(lvid, vol_info));
    vid = vol_info.vid;
    return RCOK;
}

rc_t
lid_m::_get_lvid(const vid_t& vid, lvid_t& lvid)
{
    W_COERCE( _vol_mutex.acquire() );
    auto_release_t<smutex_t> dummy(_vol_mutex);
    w_hash_i<vol_lid_info_t, lvid_t> iter(*_vol_table);

    vol_lid_info_t* vol_info = NULL;

    while ( (vol_info = iter.next()) != NULL) {
	if (vol_info->vid == vid) {
	    lvid = vol_info->lvid;
	    return RCOK;
	}
    }

    return RC(eBADVOL);
}


rc_t
lid_m::generate_new_volid(lvid_t& lvid)
{
    FUNC(lid_m::_generate_new_volid);
    /*
     * For now the logical volume ID will consists of
     * the machine network address and the current time-of-day.
     *
     * Since the time of day resolution is in seconds,
     * we protect this function with a mutex to guarantee we
     * don't generate duplicates.
     */
    static smutex_t lidmgnrt_mutex("lidmgnrt");
    static long last_time = 0;

    const int max_name = 100;
    char name[max_name+1];

#ifdef USE_UTSNAME
	struct utsname uts;
	if (uname(&uts) == -1) return RC(eOS);
	strncpy(name, uts.nodename, max_name);
#else
    if (gethostname(name, max_name)) return RC(eOS);
#endif

    struct hostent* hostinfo = gethostbyname(name);

#ifdef notyet
    if (!hostinfo)
	W_FATAL(eDNS);
#else
    if (!hostinfo)
	W_FATAL(eINTERNAL);
#endif
    memcpy(&lvid.high, hostinfo->h_addr, sizeof(lvid.high));
    DBG( << "lvid " << lvid );

    /* XXXX generating ids fast enough can create a id time sequence
       that grows way faster than real time!  This could be a problem!
       Better time resolution than seconds does exist, might be worth
       using it.  */
    stime_t curr_time = stime_t::now();

    W_COERCE(lidmgnrt_mutex.acquire());
    auto_release_t<smutex_t> dummy(lidmgnrt_mutex);

    if (curr_time.secs() > last_time)
	    last_time = curr_time.secs();
    else
	    last_time++;

    lvid.low = last_time;

    return RCOK;
}


rc_t
lid_m::_get_vol_info(const lvid_t& lvid, vol_lid_info_t& vol_info)
{
    W_COERCE( _vol_mutex.acquire() );
    auto_release_t<smutex_t> dummy(_vol_mutex);
    vol_lid_info_t*  vol_info_p;

    // Find the record for volume containing the index
    vol_info_p = _lookup_lvid(lvid
	    );
    if (vol_info_p == NULL) {
	/* 
	 * Either the lvid is invalid or not mounted
	 * TODO: try to determine which.
	 */
       return RC(eBADVOL);	 
    }
    vol_info = *vol_info_p;

    return RCOK;
}


lid_m::vol_lid_info_t* lid_m::_lookup_lvid(const lvid_t& lvid
	)
{
    FUNC(lid_m::_lookup_lvid);
    DBG(<<"lvid=" << lvid);

    // the _vol_mutex must be held
    w_assert3(_vol_mutex.is_mine());

    vol_lid_info_t* vol_info = _vol_table->lookup(lvid);

    // see if the volume was not found
    if (vol_info == NULL 
	    )  {
	{
	    return NULL;
	}
    }


    DBG(<<"lvid=" << lvid);
    return vol_info;
}


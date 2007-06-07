/*<std-header orig-src='shore'>

 $Id: w_shmem.cpp,v 1.23 2007/05/18 21:38:25 nhall Exp $

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

#define W_SOURCE
#include <os_types.h>
#include <w_base.h>

#ifdef __GNUC__
#pragma implementation
#endif

#include <w_shmem.h>

#if defined(PURIFY_ZERO) 

#if defined(HAVE_MEMORY_H)
#	include <memory.h>
#else
#	if defined(HAVE_STRING_H)
#	include <string.h>
#	else
#       Error. Need definition of memset. 
#	endif
#endif

#endif

#ifdef _WINDOWS
extern w_shmem_t* global_shmem;
#endif /*_WINDOWS*/

w_rc_t									 
w_shmem_t::create(
    uint4_t	sz, 
    mode_t	mode)
{
    W_IGNORE( destroy() );
    _size = sz;

#ifdef _WINDOWS
    global_shmem = this;

    // WINDOWS: use malloc (no shared memory needed)
    _base = (char*) malloc(sz);
    if (!_base) return RC(fcOS);
    _count = 1;

#else
	
    if ((_id = shmget(IPC_PRIVATE, int(_size), IPC_CREAT|mode)) == -1)  {
	return RC(fcOS);
    }

    if ((_base = (char*)shmat(_id, 0, 0)) 
	== (char*)-1) {
	_base = 0;
	return RC(fcOS);
    }
#endif

    return RCOK;
}

w_rc_t
w_shmem_t::destroy()
{
    if (_base)  {
	W_DO( detach() );

#ifdef _WINDOWS
	if(_count == 0) {
	    if (_base) {
		free(_base);
		_base = 0;
		_id = 0;
	    }
	}
#else
	shmctl(_id, IPC_RMID, 0);
	_base = 0;
#endif
    }

    return RCOK;
}

w_rc_t
w_shmem_t::set(
    uid_t	uid, 
    gid_t	gid,
    mode_t 	mode)
{
#ifndef _WINDOWS
    shmid_ds buf;

#ifdef PURIFY_ZERO
    memset(&buf, '\0', sizeof(buf));
#endif

    if(uid)  buf.shm_perm.uid = uid;
    if(gid)  buf.shm_perm.gid = gid;
    if(mode) buf.shm_perm.mode = mode;

    if (shmctl(_id, IPC_SET, &buf) == -1) {
	return RC(fcOS); // error
    }
    _size = buf.shm_segsz;
#endif
    return RCOK;
}

#ifdef _WINDOWS
w_rc_t
w_shmem_t::attach(HANDLE i)
#else
w_rc_t
w_shmem_t::attach(int i)
#endif
{

    _id = i;

#ifndef _WINDOWS
    if ((_base = (char*) shmat(_id, 0, 0)) == (char*)-1) {
	return RC(fcOS);
    }

    shmid_ds buf;
    if (shmctl(_id, IPC_STAT, &buf) == -1) {
	return RC(fcOS);
    }
    _size = buf.shm_segsz;
#else
    _count++;
#endif

    return RCOK;
}

w_rc_t
w_shmem_t::detach()
{
#ifdef _WINDOWS
    _count--;
#else
    if((_base != NULL) && (shmdt(_base) < 0)) {
	return RC(fcOS);
    }
#endif
    return RCOK;
}


/*<std-header orig-src='shore' incl-file-exclusion='W_SHMEM_H'>

 $Id: w_shmem.h,v 1.16 1999/06/07 19:02:56 kupsch Exp $

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

#ifndef W_SHMEM_H
#define W_SHMEM_H

#include "w_defines.h"

/*  -- do not edit anything above this line --   </std-header>*/

#ifdef __GNUG__
#pragma interface
#endif

#if defined(Mips) || defined(Sparc) || defined(I860)
#define	SHM_PROTO_KLUDGE
#endif
#if defined(SHM_PROTO_KLUDGE)
#   define shmget __shmget
#   define shmat __shmat
#   define shmctl __shmctl
#   define shmdt __shmdt
#if defined(Ultrix42)
#   include <mips/pte.h>
#   include <sys/param.h>
#endif
#   include <os_types.h>
#   include <sys/ipc.h>
#   include <sys/shm.h>
#   undef shmget
#   undef shmat
#   undef shmctl
#   undef shmdt
    extern "C" {
        int shmget(key_t, int, int);
        char *shmat(int, void*, int);
        int shmctl(int, int, struct shmid_ds *);
        int shmdt(const void*);
    }
#else

#include <sys/types.h>

#ifndef _WINDOWS
#   include <sys/ipc.h>
#   include <sys/shm.h>
#endif
#endif /* SHM_PROTO_KLUDGE */

#if defined(I860)
#	ifdef __cplusplus
	extern "C" {
#	endif
		extern int getpagesize();
#	ifdef __cplusplus
	};
#	endif
#endif

class w_shmem_t : public w_base_t {
public:
    NORET			w_shmem_t();
    NORET			~w_shmem_t();

#ifdef _WINDOWS
	HANDLE 			id()		{ return _id; }
#else
	int 			id()		{ return _id; }
#endif
    
    uint4_t			size()		{ return _size; }
    char* 			base()		{ return _base; }

    // return value of 0 == OK, 1 == FAILURE
    w_rc_t 			create(
	uint4_t 		    sz,
	mode_t			    mode = 0644 );
    w_rc_t			destroy();
#ifdef _WINDOWS
    w_rc_t			attach(HANDLE id);
#else
	w_rc_t			attach(int id);
#endif
    w_rc_t			detach();
    w_rc_t			set(
	uid_t 			    uid = 0,
	gid_t 			    gid = 0, 
	mode_t 			    mode = 0);
private:
    char*			_base;	// attached address
#ifdef _WINDOWS
    HANDLE			_id;
    int				_count;
#else
    int				_id;
#endif
    uint4_t 			_size;
};

inline NORET
w_shmem_t::w_shmem_t()
    : _base(0), _id(0), 
#ifdef _WINDOWS
	_count(0),
#endif
    _size(0)
{
}

inline NORET
w_shmem_t::~w_shmem_t()
{
}

/*<std-footer incl-file-exclusion='W_SHMEM_H'>  -- do not edit anything below this line -- */

#endif          /*</std-footer>*/

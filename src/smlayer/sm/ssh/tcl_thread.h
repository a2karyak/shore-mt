/*<std-header orig-src='shore' incl-file-exclusion='TCL_THREAD_H'>

 $Id: tcl_thread.h,v 1.33 2000/03/02 23:52:31 bolo Exp $

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

#ifndef TCL_THREAD_H
#define TCL_THREAD_H

#include "w_defines.h"

/*  -- do not edit anything above this line --   </std-header>*/

#ifdef __GNUG__
#pragma interface
#endif

class ss_m;
extern ss_m* sm;
extern int t_co_retire(Tcl_Interp* , int , char*[]);
extern void copy_interp(Tcl_Interp *ip, Tcl_Interp *pip);

class tcl_thread_t : public smthread_t  {
public:
    NORET			tcl_thread_t(
	int 			    ac, 
	char* 			    av[], 
	Tcl_Interp* 		    parent,
	bool			    block_immediate = false,		     
	bool			    auto_delete = false);
    NORET			~tcl_thread_t();

    Tcl_Interp*			get_ip() { return ip; }

    static w_list_t<tcl_thread_t> list;
    w_link_t 			link;

#ifdef SSH_SYNC_OLD
    sevsem_t			sync_point;
#else
    bool			isWaiting;
    bool			canProceed;
    bool			hasExited;
    scond_t			quiesced;
#endif
    scond_t			proceed;
    smutex_t			lock;

    static void			initialize(Tcl_Interp* ip, const char* lib_dir);

    static void 		sync_other(unsigned long id);
    static void 		join(unsigned long id);

    void 			sync();

    // This is a pointer to a global variable that all
    // threads can use to pass info between them.
    // It is linked to a TCL variable via the
    // link_to_inter_thread_comm_buffer command.
    static char*		inter_thread_comm_buffer;

protected:
    virtual void	 	run();

protected:
    char*			args;
    Tcl_Interp*			ip;
    smutex_t   			thread_mutex;

    static tcl_thread_t		*find(unsigned long id);

    static int 			count;
public:
    static bool 		allow_remote_command;

private:
    tcl_thread_t(const tcl_thread_t&);
    tcl_thread_t& operator=(const tcl_thread_t&);
};

// for gcc template instantiation
typedef w_list_i<tcl_thread_t>             tcl_thread_t_list_i;

/*<std-footer incl-file-exclusion='TCL_THREAD_H'>  -- do not edit anything below this line -- */

#endif          /*</std-footer>*/

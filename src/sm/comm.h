/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

/*
 *  $Id: comm.h,v 1.10 1995/07/14 22:05:38 nhall Exp $
 */
#ifndef COMM_H
#define COMM_H

/*
 * Communication Manager Facility.
 */

class CommSystem;
class NameService;

#ifdef __GNUG__
#pragma interface
#endif

class comm_m : public smlevel_0 {
public:
    comm_m(bool use /* FALSE => don't use scomm facility */);
    ~comm_m();

    static CommSystem* comm_system()	{ return _comm_system; }
    static NameService* name_service()	{ return _name_service; }
    static bool use_comm()		{return _use_comm; }

private:
    static bool	_use_comm;  	// don't use comm if FALSE
    static smutex_t     _mutex;

    static CommSystem*	_comm_system;
    static NameService*	_name_service;
};

#endif  /* COMM_H */


/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

/*
 *  $Id: comm.c,v 1.15 1995/11/16 17:59:55 nhall Exp $
 */

#define SM_SOURCE
#define COMM_C

#ifdef __GNUG__
#   pragma implementation
#endif

#include "sm_int.h"

bool       comm_m::_use_comm = FALSE; 
smutex_t     comm_m::_mutex("comm_m");
CommSystem*  comm_m::_comm_system = NULL;
NameService* comm_m::_name_service = NULL;


comm_m::comm_m(bool use /* FALSE => don't use scomm facility */)
{
    _use_comm = use;
    if (!_use_comm) return;

#ifdef MULTI_SERVER
    rc_t err;

    if (!_comm_system) {
	err = CommSystem::startup(_comm_system);
	w_assert1(err == RCOK && _comm_system);
    }

    // get the file holding the id of the nameserver.
    char* ns_file = getenv("NAME_SERVER");
    if(!ns_file) {
	cerr << "Fatal error: " <<
	    " You must identify a name server (environment vbl NAME_SERVER)"
	    << endl;
    }
    w_assert1(ns_file);
    ifstream ns(ns_file, ios::in);

    err = NameService::startup(*_comm_system, ns, _name_service);
    w_assert1(err == RCOK && _name_service);

#endif /* MULTI_SERVER */
}


comm_m::~comm_m()
{
    if (!_use_comm) return;
#ifdef MULTI_SERVER
    if (_name_service) {
	delete _name_service;
	_name_service = NULL;
    }
    if (_comm_system) {
	_comm_system->shutdown();
	_comm_system = NULL;
    }

#endif /* MULTI_SERVER */
}


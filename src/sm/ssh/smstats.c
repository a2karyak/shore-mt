/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

/*
 *  $Id: smstats.c,v 1.3 1996/03/18 17:22:11 bolo Exp $
 */
/* 
 * smstats.c   --  a program to attach to a running 
 *   server and gather its statistics.  Uses ptrace so it's 
 *   not a good thing to use on a regular basis.
 *
 * Copyright 1987-1991 Regents of the University of California
 * All rights reserved.
 *
 * Permission to use, copy, modify, and distribute this
 * software and its documentation for any purpose and without
 * fee is hereby granted, provided that the above copyright
 * notice appears in all copies.  The University of California
 * makes no representations about the suitability of this
 * software for any purpose.  It is provided "as is" without
 * express or implied warranty.
 */

#include <stdlib.h>
#include <limits.h>
#include <iostream.h>
#include <strstream.h>
#include <unistd.h>
#include <string.h>
#include <tcl.h>
#include <signal.h>
#include <sys/ptrace.h>
#include <sys/wait.h>
#include <nlist.h>

#include "w.h"
#include "sthread.h"
#include "sm_vas.h"
#include "debug.h"

#if defined(__GNUG__) && __GNUC_MINOR__ < 6 && defined(Sparc)
    extern "C" int getopt(int argc, char** argv, char* optstring);
#endif
#if defined(__GNUG__) && defined(Sparc)
    extern "C" int ptrace(int, pid_t, char*, int, char *);
    extern "C" int nlist(char *, struct nlist *);
#endif

#if defined(__GNUG__) && defined(Sparc)
    extern char *optarg;
    extern int optind, opterr;
#endif

void 
print_usage(ostream& err_stream, char* prog_name)
{
    err_stream << "usage: " << prog_name << endl;
    err_stream << "switches:\n"
	<< "\t-h: print help message and exit\n"
	<< "\t-i  interval in seconds\n"
	<< "\t-p  process id of process to trace\n"
	<< "\t-e  path name of executable file\n"
    << endl;
    return;
}

void
bye(const char *str, pid_t pid)
{
    perror(str);
    if(pid>0) {
	if(ptrace(PTRACE_DETACH, pid, 0, 0, 0) < 0) {
	    perror("detach");
	}
    }
    exit(1);
}

#include "smstats.h"
class sm_stats_info_t my_sm_stats_info_t;
// sm_stats_info_t::compute(){}

#include "sthread_stats.h"
class sthread_stats my_sthread_stats;


main(int argc, char* argv[])
{
    char *prog_name = argv[0];
    pid_t attached = 0;

    /* 
     * Assuming there has been no error so far, the command line
     * is processed for any ssh specific flags.
     */
    int option;
    char *_pid=0,  *_interval=0, *_execfile=0;

    {  // do even if error so that ssh -h can be recognized
	while ((option = getopt(argc, argv, "he:i:p:")) != -1) {
	    DBG(<<"option=" << option << " optarg=" << optarg);
	    switch (option) {
	    case 'i':
		_interval = optarg;
		break;

	    case 'e':
		_execfile = optarg;
		break;

	    case 'p':
		_pid = optarg;
		break;

	    case 'h':
		print_usage(cout, prog_name);
		// free rc structure to avoid complaints on exit
		return 0;
		break;

	    default:
		cout << "unknown flag: " << option << endl;
		return 0;
	    }
	}

    }

    int interval = -1;
    if(_interval) {
       interval = atoi(_interval);
       if(interval < 10) {
	  cout << "Interval " << interval << " seconds is too small." << endl;
	  return 1;
       }
    }

    pid_t pid;
    if(!_pid) {
	cout << "missing -p pid" <<endl;
	print_usage(cout, prog_name);
	return 1;
    }
    pid = atoi(_pid);
    if(pid<0) {
       cout << "Bad process id " << pid <<endl;
       return 1;
    }

    if(!_execfile) {
	cout << "missing -e execfile" <<endl;
	print_usage(cout, prog_name);
	return 1;
    }


    /* do a namelist on the executable */

#define NMMAX 2
    typedef struct nlist _nlist;
    _nlist nmlist[] = {
	{ "_SthreadStats", 0, 0, 0, 0 }, // class sthread_stats
	{ "___stats__", 0, 0, 0, 0 },	 // class sm_stats_info_t
	{ 0, 0, 0, 0, 0 }
    };

    DBG(<<"doing nmlist");
    int nentries;
    if((nentries = nlist(_execfile, &nmlist[0]))<0) {
	bye("nlist", attached);
    }
    DBG(<< "returned " << nentries );
    { 	int i;
	int errs=0;
	for(i=0; i<NMMAX; i++) {
	    if(nmlist[i].n_type != (N_DATA|N_EXT)) {
		cout << "unexpeced value for nmlist entry " << i 
		<< " : " << (int) nmlist[i].n_type << endl;
		errs++;
	    }
	}
	if(errs>0) {
	    bye("nlist", attached);
	}
    }

    struct _myinfo {
	char *n_localplace;
	int  n_datalen;

    } myinfo[] = {
	{ (char *)&my_sthread_stats,   sizeof(my_sthread_stats) },
	{ (char *)&my_sm_stats_info_t, sizeof(my_sm_stats_info_t) },
	{ 0, 0}
    };


    /* Ok ... attach to the process */
    char *addr=0, *addr2=0;
    int data=0;
    int status, *statusp=&status;
    int result=0;

#define PTRACEP(x,p) \
    DBG(<<"invoking " << #x << " pid " << p << ", addr " << addr\
     << ", data " << data << ", addr2 " << addr2); \
    if((result = ptrace(PTRACE_##x, p, addr, data, addr2)) < 0) { \
	bye(#x, attached);\
    }

#define PTRACE(x) PTRACEP(x,attached)

    PTRACEP(ATTACH,pid);
    attached = pid;
    DBG(<<"awaiting " << attached);
    if((waitpid(attached,statusp,0)<0) || (!WIFSTOPPED(status))) {
	attached = 0;
	bye("wait", attached);
    }

    while(1) {
	
	/* read the data for each offset */
	{ 	
	    int i;
	    for(i=0; i<NMMAX; i++) {
		addr = (char *)nmlist[i].n_value;
		data = myinfo[i].n_datalen;
		addr2 = myinfo[i].n_localplace;
		DBG(<<"entry " << i
			<< " address " << (int) addr
			<< " size " << (int) data
			<< " location " << (int) addr2
		    );
		PTRACE(READDATA);
	    }

	    DBG(<< "GOT DATA" );
	    cout << my_sm_stats_info_t << endl;
	    cout << my_sthread_stats << endl;
	}

	if(ptrace(PTRACE_CONT, attached, (char *)1, 0, 0) < 0) {
	    perror("*** detach!");
	    W_FATAL(fcOS);
	}
	/* sleep or exit */
	if(interval>0) {
  	    sleep(interval);
	} else {
	    bye("one-time", attached);
	}

	if(kill(attached, SIGSTOP) < 0) {
	    bye("kill SIGSTOP", attached);
	}
	if((waitpid(attached,statusp,0)<0) || (!WIFSTOPPED(status))) {
	    bye("wait", attached);
	}
    }
}

/*<std-header orig-src='shore'>

 $Id: ssh.cpp,v 1.135 2003/12/01 20:54:40 bolo Exp $

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

/* 
 * tclTest.c --
 *
 *	Test driver for TCL.
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
#include "w_stream.h"
#ifndef _WINDOWS
#include <unistd.h>
#else
#include <io.h>
#endif
#include <string.h>
#include "w_debug.h"
#include "shell.h"

#ifndef __GNU_LIBRARY__
#define __GNU_LIBRARY__
#endif
#include "getopt.h"

#include <unix_stats.h>

/* tcl */
#ifdef EXTERN
#undef EXTERN
#endif
#include <tcl.h>
#include "tcl_workaround.h"
#include "tcl_thread.h"
#include "ssh.h"
#include <sthread_stats.h>


#ifdef NOTDEF
const char* tcl_init_cmd = "if [info library] then { if [file exists [info library]/init.tcl] then { source [info library]/init.tcl }}";
#else
const char* tcl_init_cmd = 0;
#endif

const char* Logical_id_flag_tcl = "Use_logical_id";


Tcl_Interp* global_ip = 0;
bool instrument = true;
bool verbose = false;
bool verbose2 = false;
bool force_compress = false;
bool log_warn_callback = false;
bool start_client_support = false;

// Error codes for ssh
enum {	SSH_MIN_ERROR = 50000,
	SSH_SCOMM,
	SSH_OS,
	SSH_FAILURE,
	SSH_COMMAND_LINE,
	SSH_MAX_ERROR};

static w_error_info_t ssh_error_list[]=
{
    {SSH_SCOMM,	"communication system error"},
    {SSH_OS,	"operating system error"},
    {SSH_FAILURE,"general ssh program failure"},
    {SSH_COMMAND_LINE,"problem with command line"},
};

/*
 * Function print_usage print ssh usage information to
 * err_stream.  The long_form parameter indicates whether
 * a longer and more detailed description should be printed.
 * The options parameter is the list of all options used
 * by ssh and the layers is calls.
 */
void print_usage(ostream& err_stream, const char* prog_name,
		 bool long_form, option_group_t& options)
{
    if (!long_form) {
	err_stream << "usage: " << prog_name
	     << " [-h] [-l] [-v] [-c] [-f script_file]" 
	     << " [-t assignment]"
	     << endl;
	options.print_usage(long_form, err_stream);
    } else {
	err_stream << "usage: " << prog_name << endl;
	err_stream << "switches:\n"
            << "\t-h: print help message and exit\n"
	    << "\t-l: use logical oid\n"
	    << "\t-v: verbose printing of all option values\n"
	    << "\t-V: verbose printing of test results\n"
	    << "\t-c: allow clients to connect\n"
	    << "\t-f script_file: specify script to run\n"
	    << "\t-t assignment is of the form variablename=value\n"
	<< "options:" << endl;
	options.print_usage(true, err_stream);
    }
    return;
}

struct linked_vars linked;

/* An smthread is needed to create a storage manager */
class ssh_smthread_t : public smthread_t {
private:
	char	*f_arg;

public:
	ssh_smthread_t(char *f_arg);
	~ssh_smthread_t() { }
	void	run();
};

unix_stats U;
#ifndef SOLARIS2
unix_stats C(RUSAGE_CHILDREN);
#endif

int main(int argc, const char** argv)
{
#if 0	/* Can't use with tools that want ssh output */
#if defined(W_TRACE)
    char *c = getenv("DEBUG_FLAGS");
    if(c!=NULL) cout << "DEBUG_FLAGS =|" << c << "|" << endl;
    else cout << "DEBUG_FLAGS is not set. " <<endl;
    c = getenv("DEBUG_FILE");
    if(c!=NULL) cout << "DEBUG_FILE =|" << c << "|" << endl;
    else cout << "DEBUG_FILE is not set. " <<endl;

    const char *cc = fname_debug__;
    if(cc!=NULL) cout << "fname_debug__ =|" << cc << "|" << endl;
    else cout << "fname_debug__ is not set. " <<endl;
#else
    cout << "Debugging not configured." << endl;
#endif
#endif

    bool print_stats = false;


    U.start();
#ifndef SOLARIS2
    C.start();
#endif

    // Set up ssh related error codes
    if (! (w_error_t::insert(
		"ss_m shell",
		ssh_error_list, SSH_MAX_ERROR - SSH_MIN_ERROR - 1))) {
	abort();
    }

    // Create tcl interpreter
    global_ip = Tcl_CreateInterp();
    if (!global_ip)
	    W_FATAL(fcOUTOFMEMORY);

    // default is to not use logical IDs
    Tcl_SetVar(global_ip, TCL_CVBUG Logical_id_flag_tcl, 
	TCL_CVBUG "0", TCL_GLOBAL_ONLY);

    /*
     * The following section of code sets up all the various options
     * for the program.  The following steps are performed:
	- determine the name of the program
	- setup an option group for the program
	- initialize the ssm options
	- scan default option configuration files ($HOME/.shoreconfig .shoreconfig)
	- process any options found on the command line
	- use getopt() to process ssh specific flags on the command line
	- check that all required options are set before initializing sm
     */	 

    // set prog_name to the file name of the program
    const char* prog_name = strrchr(argv[0], '/');
    if (prog_name == NULL) {
	    prog_name = argv[0];
    } else {
	    prog_name += 1; /* skip the '/' */
	    if (prog_name[0] == '\0')  {
		    prog_name = argv[0];
	    }
    }

    /*
     * Set up and option group (list of options) for use by
     * all layers of the system.  Level "ssh" indicates
     * that the program is a a part to the ssh test suite.
     * Level "server" indicates
     * the type of program (the ssh server program).  The third
     * level is the program name itself.
     */
    option_group_t options(3);
    W_COERCE(options.add_class_level("ssh"));
    W_COERCE(options.add_class_level("server"));
    W_COERCE(options.add_class_level(prog_name));

    /*
     * Set up and ssh option for the name of the tcl library directory
     * and the name of the .sshrc file.
     */
    option_t* ssh_libdir;
    option_t* ssh_sshrc;
    option_t* ssh_auditing;
    W_COERCE(options.add_option("ssh_libdir", "directory name", NULL,
		"directory for ssh tcl libraries",
		true, option_t::set_value_charstr, ssh_libdir));
    W_COERCE(options.add_option("ssh_sshrc", "rc file name", ".sshrc",
		"full path name of the .sshrc file",
		false, option_t::set_value_charstr, ssh_sshrc));
    W_COERCE(options.add_option("ssh_auditing", "yes/no", "yes",
		"turn on/off auditing of operations using gdbm",
		false, option_t::set_value_bool, ssh_auditing));

    // have the sm add its options to the group
    W_COERCE(ss_m::setup_options(&options));


    /*
     * Scan the default configuration files: $HOME/.shoreconfig, .shoreconfig.  Note
     * That OS errors are ignored since it is not an error
     * for this file to not be found.
     */
    rc_t	rc;
    {
    char		opt_file[ss_m::max_devname+1];
    for(int file_num = 0; file_num < 2 && !rc.is_error(); file_num++) {
	// scan default option files
	ostrstream	err_stream;
	const char*	config = ".shoreconfig";
	if (file_num == 0) {
	    if (!getenv("HOME")) {
		// ignore it ...
		// cerr << "Error: environment variable $HOME is not set" << endl;
		// rc = RC(SSH_FAILURE);
		break;
	    }
	    if (sizeof(opt_file) <= strlen(getenv("HOME")) + strlen("/") + strlen(config) + 1) {
		cerr << "Error: environment variable $HOME is too long" << endl;
		rc = RC(SSH_FAILURE);
		break;
	    }
	    strcpy(opt_file, getenv("HOME"));
	    strcat(opt_file, "/");
	    strcat(opt_file, config);
	} else {
	    w_assert3(file_num == 1);
	    strcpy(opt_file, "./");
	    strcat(opt_file, config);
	}
	{
	    option_file_scan_t opt_scan(opt_file, &options);
	    rc = opt_scan.scan(true, err_stream);
	    err_stream << ends;
	    char* errmsg = err_stream.str();
	    if (rc) {
		// ignore OS error messages
		if (rc.err_num() == fcOS) {
		    rc = RCOK;
		} else {
		    // this error message is kind of gross but is
		    // sufficient for now
		    cerr << "Error in reading option file: " << opt_file << endl;
		    //cerr << "\t" << w_error_t::error_string(rc.err_num()) << endl;
		    cerr << "\t" << errmsg << endl;
		}
	    }
	    if (errmsg) delete[] errmsg;
	}
    }
    }

    /* 
     * Assuming there has been no error so far, the command line
     * is processed for any options in the option group "options".
     */
    if (!rc) {
	// parse command line
	ostrstream	err_stream;
	rc = options.parse_command_line(argv, argc, 2, &err_stream);
	err_stream << ends;
	char* errmsg = err_stream.str();
	if (rc) {
	    cerr << "Error on command line " << endl;
	    cerr << "\t" << w_error_t::error_string(rc.err_num()) << endl;
	    cerr << "\t" << errmsg << endl;
	    print_usage(cerr, prog_name, false, options);
	}
	if (errmsg) delete[] errmsg;
    } 

    /* 
     * Assuming there has been no error so far, the command line
     * is processed for any ssh specific flags.
     */
    int option;
    char* f_arg = 0;
    //if (!rc) 
    {  // do even if error so that ssh -h can be recognized
	bool verbose_opt = false; // print verbose option values
	while ((option = getopt(argc, (char * const*) argv, "nhlLOvsVcyCf:t:")) != -1) {
	    switch (option) {
	    case 'n':
		// null - nothing 
		break;

	    case 'O':
	    	// Force use of old sort
		cout << "Force use of old sort implementation." <<endl;
		newsort = false;
		break;

	    case 'C':
		// force compression of btrees
		force_compress = true;
		break;

	    case 'y':
		// turn yield on
#ifdef USE_SSMTEST
		simulate_preemption(true);
		cout 
		    << "Simulate preemption (USE_SSMTEST) turned ON " 
		    << endl;
#else /* USE_SSMTEST*/
		cerr 
		    << "Simulate preemption (USE_SSMTEST) not configured " 
		    << endl;
		    exit(1);
#endif /*USE_SSMTEST*/
		break;

	    case 't':
		// tcl argument
		// form is variablename=value
		{
		    char *var = strtok(optarg, "=");
		    char *val = strtok(0, "=");
		    if(!var || !val) {
			// error
			cerr << "bad assignment to tcl var"  << endl;
			print_usage(cerr, prog_name, true, options);
			return 0;
		    }
		    Tcl_SetVar(global_ip, var, val , TCL_GLOBAL_ONLY);
	        }

		break;
	    case 's':
		print_stats = true;
		break;
	    case 'f':
		f_arg = optarg;
		break;
	    case 'L':
		// use log warning callback
		log_warn_callback = true;
		break;
	    case 'l':
		// use logical IDs
		Tcl_SetVar(global_ip, TCL_CVBUG Logical_id_flag_tcl, 
			TCL_CVBUG "1", TCL_GLOBAL_ONLY);
		break;
	    case 'h':
		// print a help message describing options and flags
		print_usage(cerr, prog_name, true, options);
		// free rc structure to avoid complaints on exit
		W_IGNORE(rc);
		return 0;
		break;
	    case 'v':
		verbose_opt = true;
		break;
	    case 'V':
		verbose = true;
		break;
	    case 'c':
		start_client_support = true;
		break;
	    default:
		cerr << "unknown flag: " << option << endl;
		rc = RC(SSH_COMMAND_LINE);
	    }
	}

	if (verbose_opt) {
	    options.print_values(false, cerr);
	}
    }

    /*
     * Assuming no error so far, check that all required options
     * in option_group_t options are set.  
     */
    if (!rc) {
	// check required options
	ostrstream	err_stream;
	rc = options.check_required(&err_stream);
	err_stream << ends;
	char* errmsg = err_stream.str();
	if (rc) {
	    cerr << "These required options are not set:" << endl;
	    cerr << errmsg << endl;
	    print_usage(cerr, prog_name, false, options);
	}
	if (errmsg) delete[] errmsg;
    } 

    /* 
     * If there have been any problems so far, then exit
     */
    if (rc) {
	// free the rc error structure to avoid complaints on exit
	W_IGNORE(rc);
	Tcl_DeleteInterp(global_ip);
	return 1;
    }

    /*
     * At this point, all options and flags have been properly
     * set.  What follows is initialization for the rest of
     * the program.  The ssm will be started by a tcl_thread.
     */


    // start remote_listen thread (if start_client_support == true)
    if (start_client_support) {
		rc = start_comm();
	if (rc) {
	    cerr << "error in ssh start_comm" << endl << rc << endl;
	    return 1;
	}
    }

    tcl_thread_t::initialize(global_ip, ssh_libdir->value());

    // read .sshrc
    Tcl_DString buf;
    char* rcfilename = Tcl_TildeSubst(global_ip,
		(char*)(ssh_sshrc->value()), &buf);
    if (rcfilename)  {
	FILE* f;
	f = fopen(rcfilename, "r");
	if (f) {
	    if (Tcl_EvalFile(global_ip, rcfilename) != TCL_OK) {
		cerr << global_ip->result << endl;
		return 1;
	    }
	    fclose(f);
	} else {
	    cerr << "WARNING: could not open rc file: " << rcfilename << endl;
	}
    }
    Tcl_DStringFree(&buf);

    // setup table of sm commands
    dispatch_init();

    Tcl_SetVar(global_ip, TCL_CVBUG "log_warn_callback_flag",
		TCL_SETV2 (log_warn_callback ? "1" : "0"), TCL_GLOBAL_ONLY);

    Tcl_SetVar(global_ip, TCL_CVBUG "compress_flag",
		TCL_SETV2 (force_compress ? "1" : "0"), TCL_GLOBAL_ONLY);

    Tcl_SetVar(global_ip, TCL_CVBUG "instrument_flag",
	       TCL_SETV2 (instrument ? "1" : "0"), TCL_GLOBAL_ONLY);

    Tcl_SetVar(global_ip, TCL_CVBUG "verbose_flag",
	       TCL_SETV2 (verbose ? "1" : "0"), TCL_GLOBAL_ONLY);
    Tcl_SetVar(global_ip, TCL_CVBUG "verbose2_flag",
	       TCL_SETV2 (verbose2 ? "1" : "0"), TCL_GLOBAL_ONLY);

    char* args = Tcl_Merge(argc, (char **) argv);
    if(args) {
	Tcl_SetVar(global_ip, TCL_CVBUG "argv", args, TCL_GLOBAL_ONLY);
	ostrstream s1(args, strlen(args)+1);
	s1 << argc-1;
	Tcl_SetVar(global_ip, TCL_CVBUG "argc", args, TCL_GLOBAL_ONLY);
	Tcl_SetVar(global_ip, TCL_CVBUG "argv0", TCL_CVBUG (f_arg ? f_arg : argv[0]),
	   TCL_GLOBAL_ONLY);
	ckfree(args);
    } else {
	cerr << "Tcl_Merge failed." <<endl; exit(1); 
    }

#ifdef _WINDOWS
    int tty = _isatty(_fileno(stdin));
#else
    int tty = isatty(0);
#endif
    Tcl_SetVar(global_ip, TCL_CVBUG "tcl_interactive",
	       TCL_CVBUG ((f_arg && tty) ? "1" : "0"), TCL_GLOBAL_ONLY);

    linked.sm_page_sz = ss_m::page_sz;
    linked.sm_max_exts = ss_m::max_exts;
    linked.sm_max_vols = ss_m::max_vols;
    linked.sm_max_servers = ss_m::max_servers;
    linked.sm_max_keycomp = ss_m::max_keycomp;
    linked.sm_max_dir_cache = ss_m::max_dir_cache;
    linked.sm_max_rec_len = ss_m::max_rec_len;
    linked.sm_srvid_map_sz = ss_m::srvid_map_sz;
    linked.verbose_flag = verbose?1:0;
    linked.verbose2_flag = verbose2?1:0;
    linked.instrument_flag = instrument?1:0;
    linked.compress_flag = force_compress?1:0;
    linked.log_warn_callback_flag = log_warn_callback?1:0;

    (void) Tcl_LinkVar(global_ip, TCL_CVBUG "sm_page_sz", (char*)&linked.sm_page_sz, TCL_LINK_INT);
    (void) Tcl_LinkVar(global_ip, TCL_CVBUG "sm_max_exts", (char*)&linked.sm_max_exts, TCL_LINK_INT);
    (void) Tcl_LinkVar(global_ip, TCL_CVBUG "sm_max_vols", (char*)&linked.sm_max_vols, TCL_LINK_INT);
    (void) Tcl_LinkVar(global_ip, TCL_CVBUG "sm_max_servers", (char*)&linked.sm_max_servers, TCL_LINK_INT);
    (void) Tcl_LinkVar(global_ip, TCL_CVBUG "sm_max_keycomp", (char*)&linked.sm_max_keycomp, TCL_LINK_INT);
    (void) Tcl_LinkVar(global_ip, TCL_CVBUG "sm_max_dir_cache", (char*)&linked.sm_max_dir_cache, TCL_LINK_INT);
    (void) Tcl_LinkVar(global_ip, TCL_CVBUG "sm_max_rec_len", (char*)&linked.sm_max_rec_len, TCL_LINK_INT);
    (void) Tcl_LinkVar(global_ip, TCL_CVBUG "sm_srvid_map_sz", (char*)&linked.sm_srvid_map_sz, TCL_LINK_INT);

    (void) Tcl_LinkVar(global_ip, TCL_CVBUG "instrument_flag", (char*)&linked.instrument_flag, TCL_LINK_INT);
    (void) Tcl_LinkVar(global_ip, TCL_CVBUG "verbose_flag", (char*)&linked.verbose_flag, TCL_LINK_INT);
    (void) Tcl_LinkVar(global_ip, TCL_CVBUG "verbose2_flag", (char*)&linked.verbose2_flag, TCL_LINK_INT);
    (void) Tcl_LinkVar(global_ip, TCL_CVBUG "compress_flag", (char*)&linked.compress_flag, TCL_LINK_INT);
    /*
    if (Tcl_AppInit(global_ip) != TCL_OK)  {
	cerr << "Tcl_AppInit failed: " << global_ip->result << endl;
    }
    */

    /* We need a storage manager thread to CREATE a storage manager. */
    smthread_t *doit = new ssh_smthread_t(f_arg);
    if (!doit)
	W_FATAL(fcOUTOFMEMORY);
    W_COERCE(doit->fork());
    W_COERCE(doit->wait());
    delete doit;

    Tcl_DeleteInterp(global_ip);

    // stop remote_listen thread
    if (start_client_support) {
		rc = stop_comm();
	if (rc) {
	    cerr << "error in ssh stop_comm" << endl << rc << endl;
	    W_COERCE(rc);
	}
    }

    U.stop(1); // 1 iteration
#ifndef SOLARIS2
    C.stop(1); // 1 iteration
#endif

    if(print_stats) {
	cout << "Thread stats" <<endl;
	sthread_t::dump_stats(cout);
	cout << endl;

	cout << "Unix stats for parent:" <<endl;
	cout << U << endl << endl;

#ifndef SOLARIS2
	cout << "Unix stats for children:" <<endl;
	cout << C << endl << endl;
#endif
    }
    cout << flush;

    return 0;
}


ssh_smthread_t::ssh_smthread_t(char *arg)
: smthread_t(t_regular, "ssh_smthread", WAIT_FOREVER, 2*default_stack ),
  f_arg(arg)
{
}


void ssh_smthread_t::run()
{
    tcl_thread_t* tcl_thread;

    if (f_arg) {
	TCL_AV char* av[2];
	av[0] = TCL_AV1 "source";
	av[1] = f_arg;
	tcl_thread = new tcl_thread_t(2, av, global_ip);
    } else {
	tcl_thread = new tcl_thread_t(0, 0, global_ip);
    }
    assert(tcl_thread);
    W_COERCE( tcl_thread->fork() );
    W_COERCE( tcl_thread->wait() );
    delete tcl_thread;
}


rc_t start_comm()
{
    FUNC(start_comm);

    return RCOK;
}

rc_t stop_comm()
{
    FUNC(stop_comm);

    DBG( << "remote_listen thread has been stopped" );

    return RCOK;
}


extern "C" void ip_eval(void *ip, char *c, char *const&, int, int&);
void
ip_eval(void *vp, char *c, char *const&buf, int buflen, int &error)
{
    Tcl_Interp *ip = (Tcl_Interp *)vp;
    (void) Tcl_Eval(ip, c);
    error = 0;
    int resultlen = strlen (ip->result) + 1;

    if(resultlen > buflen) {
	error = fcFULL;
	resultlen = buflen-1;
    }
    // GROT too many memory copies!
    memcpy(buf, ip->result, resultlen);
    buf[resultlen] = '\0';
    return;
}


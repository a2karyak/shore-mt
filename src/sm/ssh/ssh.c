/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

/*
 *  $Id: ssh.c,v 1.88 1996/04/24 14:30:18 nhall Exp $
 */
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
#include <iostream.h>
#include <strstream.h>
#include <unistd.h>
#include <string.h>
#include <tcl.h>
#ifdef MULTI_SERVER
#include "w.h"
#include "sthread.h"
#include "scomm.hh"
#include "ns_client.hh"
#endif
#include "sm_vas.h"
#include "debug.h"
#include "tcl_thread.h"
#include "ssh.h"
#ifndef SOLARIS2
#include <unix_stats.h>
#else
#include <solaris_stats.h>
#endif
#include <sthread_stats.h>

#if defined(__GNUG__) && __GNUC_MINOR__ < 6 && defined(Sparc)
    extern "C" int getopt(int argc, char** argv, char* optstring);
#endif

#if defined(__GNUG__) && defined(Sparc)
    extern char *optarg;
    extern int optind, opterr;
#endif

char* tcl_init_cmd = "if [file exists [info library]/init.tcl] then { source [info library]/init.tcl }";

char* Logical_id_flag_tcl = "Use_logical_id";

#ifdef USE_VERIFY
extern ovt_init(Tcl_Interp* ip);
extern ovt_final();
#endif

extern sm_dispatch_init();
#if defined(MULTI_SERVER) && defined(MARKOS_TEST)
extern test_dispatch_init();
#endif

Tcl_Interp* global_ip = 0;
int verbose = FALSE;
bool start_client_support = FALSE;

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
void print_usage(ostream& err_stream, char* prog_name,
		 bool long_form, option_group_t& options)
{
    if (!long_form) {
	err_stream << "usage: " << prog_name
	     << " [-h] [-l] [-v] [-c] [-f script_file]" << endl;
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
	<< "options:" << endl;
	options.print_usage(TRUE, err_stream);
    }
    return;
}

static struct {
    int sm_page_sz;
    int sm_max_exts;
    int sm_max_vols;
    int sm_max_xcts;
    int sm_max_servers;
    int sm_max_keycomp;
    int sm_max_dir_cache;
    int sm_max_rec_len;
    int sm_srvid_map_sz;
} linked;

int rseed=1;
char rstate[32]= {
    0x76, 0x4, 0x24, 0x2c,
    0x03, 0xab, 0x38, 0xd0,
    0xab, 0xed, 0xf1, 0x23,
    0x03, 0x00, 0x08, 0xd0,
    0x76, 0x40, 0x24, 0x2c,
    0x03, 0xab, 0x38, 0xd0,
    0xab, 0xed, 0xf1, 0x23,
    0x01, 0x00, 0x38, 0xd0
};


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

main(int argc, char* argv[])
{
    bool print_stats = false;
#ifdef MARKOS_TEST
    srand(gethostid());
#endif

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

    // default is to not use logical IDs
    Tcl_SetVar(global_ip, Logical_id_flag_tcl, "0", TCL_GLOBAL_ONLY);

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
    char* prog_name = strrchr(argv[0], '/');
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
		TRUE, option_t::set_value_charstr, ssh_libdir));
    W_COERCE(options.add_option("ssh_sshrc", "rc file name", ".sshrc",
		"full path name of the .sshrc file",
		FALSE, option_t::set_value_charstr, ssh_sshrc));
    W_COERCE(options.add_option("ssh_auditing", "yes/no", "yes",
		"turn on/off auditing of operations using gdbm",
		FALSE, option_t::set_value_bool, ssh_auditing));

    // have the sm add its options to the group
    W_COERCE(ss_m::setup_options(&options));

    /*
     * Scan the default configuration files: $HOME/.shoreconfig, .shoreconfig.  Note
     * That OS errors are ignored since it is not an error
     * for this file to not be found.
     */
    rc_t	rc;
    for(int file_num = 0; file_num < 2 && !rc.is_error(); file_num++) {
	// scan default option files
	ostrstream	err_stream;
	char		opt_file[_POSIX_PATH_MAX+1];
	char*		config = ".shoreconfig";
	if (file_num == 0) {
	    if (!getenv("HOME")) {
		cerr << "Error: environment variable $HOME is not set" << endl;
		rc = RC(SSH_FAILURE);
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
	option_file_scan_t opt_scan(opt_file, &options);
	rc = opt_scan.scan(TRUE, err_stream);
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
	if (errmsg) delete errmsg;
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
	    print_usage(cerr, prog_name, FALSE, options);
	}
	if (errmsg) delete errmsg;
    } 

    /* 
     * Assuming there has been no error so far, the command line
     * is processed for any ssh specific flags.
     */
    int option;
    char* f_arg = 0;
    //if (!rc) 
    {  // do even if error so that ssh -h can be recognized
	bool verbose_opt = FALSE; // print verbose option values
	while ((option = getopt(argc, argv, "hlvsVcf:")) != -1) {
	    switch (option) {
	    case 's':
		print_stats = true;
		break;
	    case 'f':
		f_arg = optarg;
		break;
	    case 'l':
		// use logical IDs
		Tcl_SetVar(global_ip, Logical_id_flag_tcl, "1", TCL_GLOBAL_ONLY);
		break;
	    case 'h':
		// print a help message describing options and flags
		print_usage(cerr, prog_name, TRUE, options);
		// free rc structure to avoid complaints on exit
		rc = RCOK;
		return 0;
		break;
	    case 'v':
		verbose_opt = TRUE;
		break;
	    case 'V':
		verbose = TRUE;
		break;
	    case 'c':
		start_client_support = TRUE;
		break;
	    default:
		cerr << "unknown flag: " << option << endl;
		rc = RC(SSH_COMMAND_LINE);
	    }
	}

	if (verbose_opt) {
	    options.print_values(FALSE, cerr);
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
	    print_usage(cerr, prog_name, FALSE, options);
	}
	if (errmsg) delete errmsg;
    } 

    /* 
     * If there have been any problems so far, then exit
     */
    if (rc) {
	// free the rc error structure to avoid complaints on exit
	rc = RCOK;
	Tcl_DeleteInterp(global_ip);
	return 1;
    }

    /*
     * At this point, all options and flags have been properly
     * set.  What follows is initialization for the rest of
     * the program.  The ssm will be started by a tcl_thread.
     */

    /* 
     * initialize random number generator
     */
    (void) initstate(rseed, rstate, sizeof(rstate));

#ifdef USE_VERIFY
    bool badVal;
    bool do_auditing = option_t::str_to_bool(ssh_auditing->value(), badVal);
    w_assert3(!badVal);
    if (do_auditing) {
	ovt_init(global_ip);
    }
#endif USE_VERIFY

    // start remote_listen thread (if start_client_support == true)
    if (start_client_support) {
	if (rc = start_comm()) {
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
    sm_dispatch_init();

#if defined(MULTI_SERVER) && defined(MARKOS_TEST)
    // setup table of test commands (for markos' experiments)
    test_dispatch_init();
#endif

    Tcl_SetVar(global_ip, "verbose_flag",
	       verbose ? "1" : "0", TCL_GLOBAL_ONLY);

    char* args = Tcl_Merge(argc, argv);
    Tcl_SetVar(global_ip, "argv", args, TCL_GLOBAL_ONLY);
    ostrstream s1(args, strlen(args)+1);
    s1 << argc-1;
    Tcl_SetVar(global_ip, "argc", args, TCL_GLOBAL_ONLY);
    Tcl_SetVar(global_ip, "argv0", (f_arg ? f_arg : argv[0]),
	       TCL_GLOBAL_ONLY);
    free(args);

    int tty = isatty(0);
    Tcl_SetVar(global_ip, "tcl_interactive",
	       ((f_arg && tty) ? "1" : "0"), TCL_GLOBAL_ONLY);

    linked.sm_page_sz = ss_m::page_sz;
    linked.sm_max_exts = ss_m::max_exts;
    linked.sm_max_vols = ss_m::max_vols;
    linked.sm_max_xcts = ss_m::max_xcts;
    linked.sm_max_servers = ss_m::max_servers;
    linked.sm_max_keycomp = ss_m::max_keycomp;
    linked.sm_max_dir_cache = ss_m::max_dir_cache;
    linked.sm_max_rec_len = ss_m::max_rec_len;
    linked.sm_srvid_map_sz = ss_m::srvid_map_sz;

    (void) Tcl_LinkVar(global_ip, "sm_page_sz", (char*)&linked.sm_page_sz, TCL_LINK_INT);
    (void) Tcl_LinkVar(global_ip, "sm_max_exts", (char*)&linked.sm_max_exts, TCL_LINK_INT);
    (void) Tcl_LinkVar(global_ip, "sm_max_vols", (char*)&linked.sm_max_vols, TCL_LINK_INT);
    (void) Tcl_LinkVar(global_ip, "sm_max_xcts", (char*)&linked.sm_max_xcts, TCL_LINK_INT);
    (void) Tcl_LinkVar(global_ip, "sm_max_servers", (char*)&linked.sm_max_servers, TCL_LINK_INT);
    (void) Tcl_LinkVar(global_ip, "sm_max_keycomp", (char*)&linked.sm_max_keycomp, TCL_LINK_INT);
    (void) Tcl_LinkVar(global_ip, "sm_max_dir_cache", (char*)&linked.sm_max_dir_cache, TCL_LINK_INT);
    (void) Tcl_LinkVar(global_ip, "sm_max_rec_len", (char*)&linked.sm_max_rec_len, TCL_LINK_INT);
    (void) Tcl_LinkVar(global_ip, "sm_srvid_map_sz", (char*)&linked.sm_srvid_map_sz, TCL_LINK_INT);

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

#ifdef USE_VERIFY
    if (do_auditing) {
	ovt_final(); 
    }
#endif

    Tcl_DeleteInterp(global_ip);

    // stop remote_listen thread
    if (start_client_support) {
	if (rc = stop_comm()) {
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
	cout << SthreadStats << endl;

	cout << "Unix stats for parent:" <<endl;
	cout << U << endl << endl;

#ifndef SOLARIS2
	cout << "Unix stats for children:" <<endl;
	cout << C << endl << endl;
#endif
    }
}


ssh_smthread_t::ssh_smthread_t(char *arg)
: smthread_t(t_regular, false, false, "doit" ),
  f_arg(arg)
{
}


void ssh_smthread_t::run()
{
    tcl_thread_t* tcl_thread;

    if (f_arg) {
	char* av[2];
	av[0] = "source";
	av[1] = f_arg;
	tcl_thread = new tcl_thread_t(2, av, global_ip);
    } else {
#if 0
	if (tcl_RcFileName)  {
	    Tcl_DString buf;
	    char* fullname = Tcl_TildeSubst(global_ip, tcl_RcFileName,
					    &buf);
	    if (! fullname)  {
		cerr << global_ip->result << endl;
	    } else {
		FILE* f;
		if (f = fopen(fullname, "r"))  {
		    if (Tcl_EvalFile(global_ip, fullname) != TCL_OK) {
			cerr << global_ip->result << endl;
		    }
		    fclose(f);
		}
	    }
	    Tcl_DStringFree(&buf);
	}
#endif
	tcl_thread = new tcl_thread_t(0, 0, global_ip);
    }
    assert(tcl_thread);
    W_COERCE( tcl_thread->fork() );
    W_COERCE( tcl_thread->wait() );
    delete tcl_thread;
}


#ifdef MULTI_SERVER
#ifdef undef

class remote_listen_thread_t;
Port*           rctrl_port = NULL;
NameService*	rctrl_ns = NULL;
char*   	host_rctrl_name = NULL;
remote_listen_thread_t*  	remote_listen_thread = NULL;

static void remote_listen(void*);

class remote_listen_thread_t : public smthread_t {
public:
    NORET	remote_listen_thread_t();
    NORET	~remote_listen_thread_t() {};
private:
    // disabled
    NORET	remote_listen_thread_t(const remote_listen_thread_t&);
    remote_listen_thread_t&  operator=(const remote_listen_thread_t&);
};

remote_listen_thread_t::remote_listen_thread_t()
	: smthread_t(remote_listen, NULL, t_regular, false, false, "remote_listen_thread")
{
}

#endif /* undef */
#endif /* MULTI_SERVER */

rc_t start_comm()
{
    FUNC(start_comm);

#ifdef MULTI_SERVER
#ifdef undef

    // init comm facility if necessary. This is now done in comm.c
    // if (!Node::init_done()) Node::Init();

    char    hostname[100];
    rctrl_port = Port::get();
    w_assert3(rctrl_port);

    if (gethostname(hostname, sizeof(hostname)-1)) {
	W_FATAL(SSH_OS);
    }
    host_rctrl_name = new char[strlen(hostname) + strlen(".") + strlen(rctrl_name) + 1];
    assert(host_rctrl_name);
    strcpy(host_rctrl_name, hostname);
    strcat(host_rctrl_name, ".");
    strcat(host_rctrl_name, rctrl_name);

    rctrl_ns = new NameService();
    w_assert1(rctrl_ns);

    W_DO_PUSH(rctrl_ns->enter(host_rctrl_name, rctrl_port), SSH_SCOMM);

    remote_listen_thread = new remote_listen_thread_t();
    w_assert1(remote_listen_thread);
    W_COERCE(remote_listen_thread->fork());
    DBG( << "forked remote_listen thread");
#endif /* undef */
#endif /* MULTI_SERVER */

    return RCOK;
}

rc_t stop_comm()
{
    FUNC(stop_comm);
#ifdef MULTI_SERVER
#ifdef undef
    Buffer buf(sizeof(rctrl_msg_t));
    rctrl_msg_t& msg = *(rctrl_msg_t*) buf.start();

    // send message to stop remote_listen thread
    msg.set_type(rctrl_msg_t::sm_shutdown_msg);
    W_DO_PUSH(Node::me()->send(buf, rctrl_port->id()), SSH_SCOMM);

    // wait for thread to end
    DBG( << "waiting for remote_listen thread to end" );
    remote_listen_thread->wait();
    delete remote_listen_thread;
#endif /* undef */
#endif /* MULTI_SERVER */

    DBG( << "remote_listen thread has been stopped" );

    return RCOK;
}

#ifdef MULTI_SERVER
#ifdef undef

// listen for connections
static void remote_listen(void* dummy)
{
    FUNC(remote_listen);
    rctrl_msg_t* msg;
    Buffer buf(sizeof(*msg));
    msg = (rctrl_msg_t*) buf.start();

    Node	*node;
    int		reply_port;

    while(1) {
	W_COERCE(rctrl_port->receive(buf, &node));

	if (msg->type() == rctrl_msg_t::sm_shutdown_msg) {
	    break;   // end of this thread;
	}

	DBG( << "received connection request");
	w_assert1(msg->type() == rctrl_msg_t::connect_msg);
	Port* p = Port::get(); 	// get new port for this connection

	// have death notification for node sent to p
	node->notify(p);

	// respond to client with session port
	reply_port = msg->reply_port();
	msg->content.port = p->id();

	W_COERCE(node->send(buf, reply_port));
	DBG( << "sent connection reply, forking thread");
	// fork thread to handle this client session
	tcl_client_thread_t* t = new tcl_client_thread_t(node, p, global_ip);
	w_assert1(t);
	W_COERCE(t->fork());
    }

}

#endif /* undef */
#endif /* MULTI_SERVER */

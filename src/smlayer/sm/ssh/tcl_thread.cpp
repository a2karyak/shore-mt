/*<std-header orig-src='shore'>

 $Id: tcl_thread.cpp,v 1.125 2000/03/14 18:28:48 bolo Exp $

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

#define TCL_THREAD_C

#ifdef __GNUG__
#   pragma implementation
#endif


extern "C" void dumpthreads();

#ifdef _WIN32
#    undef EXTERN
#include <io.h>
#ifdef _MT
#include <process.h>
#endif
#endif

#include "shell.h"

#if defined(_WIN32) && defined(NEW_IO)
#include <win32_events.h>
#elif !defined(_WIN32)
#include <sfile_handler.h>
#endif


#include <crash.h>

#ifdef USE_COORD
#include <sm_global_deadlock.h>
extern CentralizedGlobalDeadlockServer* globalDeadlockServer;
#endif


#ifdef EXPLICIT_TEMPLATE
template class w_list_i<tcl_thread_t>;
template class w_list_t<tcl_thread_t>;
#endif

// an unlikely naturally occuring error string which causes the interpreter loop to
// exit cleanly.
static const char *TCL_EXIT_ERROR_STRING = "!@#EXIT#@!";


bool tcl_thread_t::allow_remote_command = false;
int tcl_thread_t::count = 0;
char* tcl_thread_t::inter_thread_comm_buffer = 0;

ss_m* sm = 0;

extern int vtable_dispatch(ClientData, Tcl_Interp* ip, int ac, char* av[]);
extern int sm_dispatch(ClientData, Tcl_Interp* ip, int ac, char* av[]);
#ifdef USE_COORD
extern int co_dispatch(ClientData, Tcl_Interp* ip, int ac, char* av[]);
#endif
extern int st_dispatch(ClientData, Tcl_Interp *, int, char **);

extern const char* tcl_init_cmd;

w_list_t<tcl_thread_t> tcl_thread_t::list(offsetof(tcl_thread_t, link));
class xct_i;
extern w_rc_t out_of_log_space (xct_i*, xct_t *&,
	smlevel_0::fileoff_t, smlevel_0::fileoff_t);

// For debugging
extern "C" void tcl_assert_failed();
void tcl_assert_failed() {}

static int
t_debugflags(ClientData, Tcl_Interp* ip, int ac, char** W_IFTRACE(av))
{
    if (ac != 2 && ac != 1) {
	Tcl_AppendResult(ip, "usage: debugflags [arg]", 0);
	return TCL_ERROR;
    }
#ifdef W_TRACE
    char *f;
    f = getenv("DEBUG_FILE");
    if(ac>1) {
        if(strcmp(av[1],"off")==0) {
            av[1] = (char *)"";
        }
         _debug.setflags(av[1]);
    } else {
        Tcl_AppendResult(ip,  _debug.flags(), 0);
    }
    if(f) {
	Tcl_AppendResult(ip,  "NB: written to file: ",f,".", 0);
    }
#endif
    return TCL_OK;
}
static int
t_assert(ClientData, Tcl_Interp* ip, int ac, char* av[])
{
    if (ac != 2) {
	Tcl_AppendResult(ip, "usage: assert [arg]", 0);
	return TCL_ERROR;
    }
    int boo = 0;
    int	res = Tcl_ExprBoolean(ip, av[1], &boo);
    if(res == TCL_OK && boo==0) {
	// assertion failure
	// A place to put a gdb breakpoint!!!
	tcl_assert_failed();
    }
    Tcl_AppendResult(ip, boo?"1":"0", 0);
    cout << flush;
    return res;
}


static int
t_timeofday(ClientData, Tcl_Interp* ip, int ac, char* /*av*/[])
{
    if (ac > 1) {
	Tcl_AppendResult(ip, "usage: timeofday", 0);
	return TCL_ERROR;
    }

    stime_t now = stime_t::now();

    char tmp[100];
    ostrstream otmp(tmp, sizeof(tmp));
    /* XXX should use stime_t operator<<, but format differs */
    W_FORM2(otmp, ("%d %d", now.secs(), now.usecs()));
    otmp << ends;
    Tcl_AppendResult(ip, otmp.str(), 0);

    return TCL_OK;
}
static int
t_allow_remote_command(ClientData, Tcl_Interp* ip, int ac, char* av[])
{
    if (ac != 2) {
	Tcl_AppendResult(ip, "usage: allow_remote_command [off|no|false|on|yes|true]", 
		0);
	return TCL_ERROR;
    }
    const char *c = av[1];

    if(strcmp(c, "off")==0 ||
	strcmp(c, "no")==0 ||
        strcmp(c, "false")==0) {
	tcl_thread_t::allow_remote_command = false;
    } else if(strcmp(c, "on")==0 ||
	strcmp(c, "yes")==0 ||
        strcmp(c, "true")==0) {
	tcl_thread_t::allow_remote_command = true;
    }
    return TCL_OK;
}

#if !defined(USE_SSMTEST) 
#define av
#endif
static int
t_debuginfo(ClientData, Tcl_Interp* ip, int ac, char* av[])
#if !defined(USE_SSMTEST) 
#undef av
#endif
{
    if (ac != 4) {
	Tcl_AppendResult(ip, "usage: debuginfo category v1 v2", 0);
	return TCL_ERROR;
    }
#if defined(USE_SSMTEST) 
    const char *v1 = av[2];
    int v2 = atoi(av[3]);
    debuginfo_enum d = debug_delay;
    /*
     * categories: delay, crash, abort, yield
     * v1  is string
     * v2  is int
     *
     * Same effect as setting environment variables 
     * (e.g.)CRASHTEST CRASHTESTVAL
     */
    if(strcmp(av[1], "delay")==0) {
	// v1: where, v2: time
	d = debug_delay;
    } else
    if(strcmp(av[1], "crash")==0) {
	if(! tcl_thread_t::allow_remote_command) return TCL_OK;

	// v1: where, v2: nth-time-through
	d = debug_crash;
    } else
    if(strcmp(av[1], "none")==0) {
	v1 = "none";
	v2 = 0;
	setdebuginfo(debug_delay, v1, v2);
	d = debug_crash;
    }
    setdebuginfo(d, v1, v2);
    return TCL_OK;
#else
    Tcl_AppendResult(ip, "simulate_preemption (USE_SSMTEST) not configured", 0);
    return TCL_ERROR;
#endif
}

static int
t_write_random(ClientData, Tcl_Interp* ip, int ac, char* av[])
{
    if (ac != 2) {
	Tcl_AppendResult(ip, "usage: write_random filename", 0);
	return TCL_ERROR;
    }
    if(ac == 2) {
    	random_generator::generator.write(av[1]);
    }
    return TCL_OK;
}

static int
t_read_random(ClientData, Tcl_Interp* ip, int ac, char* av[])
{
    if (ac != 2) {
	Tcl_AppendResult(ip, "usage: read_random filename", 0);
	return TCL_ERROR;
    }
    if(ac == 2) {
    	random_generator::generator.read(av[1]);
    }
    return TCL_OK;
}

static int
t_random(ClientData, Tcl_Interp* ip, int ac, char* av[])
{
    if (ac > 2) {
	Tcl_AppendResult(ip, "usage: random [modulus]", 0);
	return TCL_ERROR;
    }
    int mod;
    if(ac == 2) {
	mod = atoi(av[1]);
    } else {
	mod = -1;
    }
    // long res = random_generator::generator.mrand();
    unsigned int res = random_generator::generator.lrand(); // return only unsigned
    if(mod==0) {
	/* initialize to a given, known, state */
	random_generator::generator.srand(0);
    } else if(mod>0) {
	res %= mod;
    }
    {	char tmp[40];
	ostrstream otmp(tmp,sizeof(tmp));
	W_FORM2(otmp, ("%#d",res));
	otmp << ends;
	Tcl_AppendResult(ip, otmp.str(), 0);
    }

    return TCL_OK;
}

static int
t_fork_thread(ClientData, Tcl_Interp* ip, int ac, char* av[])
{
    if (ac < 3)  {
	Tcl_AppendResult(ip, "usage: ", av[0], " proc args", 0);
	return TCL_ERROR;
    }

    char* old_result = strdup(Tcl_GetStringResult(ip));
    w_assert1(old_result);

    tcl_thread_t* p = 0;
    p = new tcl_thread_t(ac - 1, av + 1, ip, false);
    if (!p) {
	Tcl_AppendResult(ip, "cannot create thread", 0);
	return TCL_ERROR;
    }

    rc_t e = p->fork();
    if (e != RCOK) {
	    delete p;
	    Tcl_AppendResult(ip, "cannnot start thread", 0);
	    return TCL_ERROR;
    }
    
    Tcl_SetResult(ip, old_result, TCL_DYNAMIC);

    char buf[20];
    ostrstream s(buf, sizeof(buf));
    s << p->id << '\0';
    Tcl_AppendResult(ip, buf, 0);
    
    return TCL_OK;
}

static int
t_sync(ClientData, Tcl_Interp* ip, int ac, char* av[])
{
    if (ac != 1) {
	Tcl_AppendResult(ip, "usage: ", av[0], 0);
	return TCL_ERROR;
    }

    ((tcl_thread_t *)sthread_t::me())->sync();

    return TCL_OK;
}

static int
t_sync_thread(ClientData, Tcl_Interp* ip, int ac, char* av[])
{
    if (ac == 1)  {
	Tcl_AppendResult(ip, "usage: ", av[0],
			 " thread_id1 thread_id2 ...", 0);
	return TCL_ERROR;
    }

    for (int i = 1; i < ac; i++)  {
	tcl_thread_t::sync_other(strtol(av[i], 0, 10));
    }

    return TCL_OK;
}

static int
t_join_thread(ClientData, Tcl_Interp* ip, int ac, char* av[])
{
    if (ac == 1)  {
	Tcl_AppendResult(ip, "usage: ", av[0],
			 " thread_id1 thread_id2 ...", 0);
	return TCL_ERROR;
    }

    for (int i = 1; i < ac; i++)  {
	tcl_thread_t::join(strtol(av[i], 0, 10));
    }

    return TCL_OK;
}

static int
t_yield(ClientData, Tcl_Interp* ip, int ac, char* av[])
{
    if (ac != 1)  {
	Tcl_AppendResult(ip, "usage: ", av[0], 0);
	return TCL_ERROR;
    }

    me()->yield();

    return TCL_OK;
}

static int
t_link_to_inter_thread_comm_buffer(ClientData, Tcl_Interp* ip, int ac, char* av[])
{
    if (ac != 2)  {
	Tcl_AppendResult(ip, "usage: ", av[0], "variable", 0);
	return TCL_ERROR;
    }

    return Tcl_LinkVar(ip, av[1], (char*)&tcl_thread_t::inter_thread_comm_buffer, TCL_LINK_STRING);

    //return TCL_OK;
}

static int
t_exit(ClientData, Tcl_Interp *ip, int ac, char* av[])
{
    int e = (ac == 2 ? atoi(av[1]) : 0);
    cout << flush;
    if (e == 0)  {
	Tcl_SetResult(ip, TCL_CVBUG TCL_EXIT_ERROR_STRING, TCL_STATIC);
	return TCL_ERROR;  // interpreter loop will catch this and exit
    }  else  {
	_exit(e);
    }
    return TCL_ERROR;
}


/*
 * This is a hacked version of the tcl7.4 'time' command.
 * It uses the shore-native 'stime' support to print
 * "friendly" times, instead of
 *      16636737373 microseconds per iteration
 */

static int 
t_time(ClientData, Tcl_Interp *interp,int argc, char **argv)
{
    int count, i, result;
    stime_t start, stop;

    if (argc == 2) {
	count = 1;
    } else if (argc == 3) {
	if (Tcl_GetInt(interp, argv[2], &count) != TCL_OK) {
	    return TCL_ERROR;
	}
    } else {
	Tcl_AppendResult(interp, "wrong # args: should be \"", argv[0],
		" command ?count?\"", (char *) NULL);
	return TCL_ERROR;
    }
    start = stime_t::now();

    for (i = count ; i > 0; i--) {
	result = Tcl_Eval(interp, argv[1]);
	if (result != TCL_OK) {
	    if (result == TCL_ERROR) {
		    char msg[60];
		    ostrstream msgstr(msg, sizeof(msg));
		    W_FORM2(msgstr, ("\n    (\"time\" body line %d)",
				     interp->errorLine));
		    msgstr << ends;
		    Tcl_AddErrorInfo(interp, msg);
	    }
	    return result;
	}
    }

    stop = stime_t::now();

    Tcl_ResetResult(interp);	/* The tcl version does this. ??? */
    char	buf[128];
    ostrstream s(buf, sizeof(buf));

    if (count > 0) {
	sinterval_t timePer(stop - start);
	s << timePer << " seconds";

	if (count > 1) {
	    sinterval_t timeEach(timePer / count);;
	    s << " (" << timeEach << " seconds per iteration)";
	}
    }
    else
	    s << "0 iterations";
    s << ends;

    Tcl_AppendResult(interp, buf, 0);

    return TCL_OK;
}
static int
t_pecho(ClientData, Tcl_Interp* ip, int ac, char* av[])
{
    for (int i = 1; i < ac; i++) {
        cout << ((i > 1) ? " " : "") << av[i];
#ifdef PURIFY
        if(purify_is_running()) {
            if(i>1) purify_printf(" ");
            purify_printf(av[i]);
        }
#endif
        Tcl_AppendResult(ip, (i > 1) ? " " : "", av[i], 0);
    }
#ifdef PURIFY
    if(purify_is_running()) { purify_printf("\n"); }
#endif
    cout << endl << flush;

    return TCL_OK;
}


static int
t_echo(ClientData, Tcl_Interp* ip, int ac, char* av[])
{
    for (int i = 1; i < ac; i++) {
	cout << ((i > 1) ? " " : "") << av[i];
	Tcl_AppendResult(ip, (i > 1) ? " " : "", av[i], 0);
    }
    cout << endl << flush;

    return TCL_OK;
}

#ifdef UNDEF
static int
t_verbose(ClientData, Tcl_Interp* ip, int ac, char* av[])
{
    extern int verbose;
    
    if (!verbose)
	return TCL_OK;
    for (int i = 1; i < ac; i++) {
	cout << ((i > 1) ? " " : "") << av[i];
	Tcl_AppendResult(ip, (i > 1) ? " " : "", av[i], 0);
    }
    cout << endl << flush;

    return TCL_OK;
}
#endif /*UNDEF*/


/*
 * This is from Henry Spencer's Portable string library, which
 * is in the public domain.  Bolo changed it to have a per-thread
 * context, so safe_strtok() can be used safely in a multi-threaded
 * environment.
 *
 * To use safe_strtok(), set with *scanpoint = NULL when starting off.
 * After that, it will be maintained normally.
 *
 * Get next token from string s (NULL on 2nd, 3rd, etc. calls),
 * where tokens are nonempty strings separated by runs of
 * chars from delim.  Writes NULs into s to end tokens.  delim need not
 * remain constant from call to call.
 *
 * XXX this may be something to insert into the fc or common
 * directories, as it is a problem wherever threads use strtok.
 */

static char *safe_strtok(char *s, const char *delim, char *&scanpoint)
{
	char *scan;
	char *tok;
	const char *dscan;

	if (s == NULL && scanpoint == NULL)
		return(NULL);
	if (s != NULL)
		scan = s;
	else
		scan = scanpoint;

	/*
	 * Scan leading delimiters.
	 */
	for (; *scan != '\0'; scan++) {
		for (dscan = delim; *dscan != '\0'; dscan++)
			if (*scan == *dscan)
				break;
		if (*dscan == '\0')
			break;
	}
	if (*scan == '\0') {
		scanpoint = NULL;
		return(NULL);
	}

	tok = scan;

	/*
	 * Scan token.
	 */
	for (; *scan != '\0'; scan++) {
		for (dscan = delim; *dscan != '\0';)	/* ++ moved down. */
			if (*scan == *dscan++) {
				scanpoint = scan+1;
				*scan = '\0';
				return(tok);
			}
	}

	/*
	 * Reached end of string.
	 */
	scanpoint = NULL;
	return(tok);
}


static void grab_vars(Tcl_Interp* ip, Tcl_Interp* pip)
{
    Tcl_Eval(pip, TCL_CVBUG "info globals");

    char	*result = strdup(Tcl_GetStringResult(pip));
    w_assert1(result);

    char	*last = result + strlen(result);
    char	*context = 0;
    char	*p = safe_strtok(result, " ", context);

    while (p)  {
	char* v = Tcl_GetVar(pip, p, TCL_GLOBAL_ONLY);
	if (v)  {
	    Tcl_SetVar(ip, p, v, TCL_GLOBAL_ONLY);
	    p = safe_strtok(0, " ", context);
	} else {
	    Tcl_VarEval(pip, "array names ", p, 0);
	    char* s = safe_strtok(Tcl_GetStringResult(pip), " ", context);
	    while (s)  {
		v = Tcl_GetVar2(pip, p, s, TCL_GLOBAL_ONLY);
		if (v)  {
		    Tcl_SetVar2(ip, p, s, v, TCL_GLOBAL_ONLY);
		}
		s = safe_strtok(0, " ", context);
		if (s)  *(s-1) = ' ';
	    }
	    p[strlen(p)] = ' ';
	    *last = '\0';
	    p = safe_strtok(p, " ", context);
	    if (p != result) *(p-1) = ' ';
	    p = safe_strtok(0, " ", context);
	}
	if (p) {
	    assert(*(p-1) == '\0');
	    *(p-1) = ' ';
	}
    }
    free(result);
}

static void grab_procs(Tcl_Interp* ip, Tcl_Interp* pip)
{
    Tcl_DString buf;
    Tcl_DStringInit(&buf);
    
    Tcl_Eval(pip, TCL_CVBUG "info procs");
    Tcl_Eval(ip, TCL_CVBUG "info procs");
    char* procs = strdup(Tcl_GetStringResult(pip));
    if (*procs)  {
        char *context = 0;
	w_assert1(procs);

	for (char* p = safe_strtok(procs, " ", context);
	     p;   p = safe_strtok(0, " ", context))  {
	    char line[1000];
	    {
		ostrstream s(line, sizeof(line));
		s << "info proc " << p << '\0';
		Tcl_Eval(ip, line);
		char *r = Tcl_GetStringResult(ip);
		if (!r || strcmp(r, p) == 0)  {
		    // already have this proc
		    continue;
		}
	    }

	    Tcl_DStringAppend(&buf, "proc ", -1);
	    Tcl_DStringAppend(&buf, p, -1);
	    {
		ostrstream s(line, sizeof(line));
		s << "info args " << p << '\0';
		Tcl_Eval(pip, line);
		Tcl_DStringAppend(&buf, " { ", -1);
		Tcl_DStringAppend(&buf, Tcl_GetStringResult(pip), -1);
		Tcl_DStringAppend(&buf, " } ", -1);
	    }
	    {
		ostrstream s(line, sizeof(line));
		s << "info body " << p << '\0';
		Tcl_Eval(pip, line);
		Tcl_DStringAppend(&buf, " { ", -1);
		Tcl_DStringAppend(&buf, Tcl_GetStringResult(pip), -1);
		p = Tcl_DStringAppend(&buf, " } \n", -1);
		assert(p);
		assert(Tcl_CommandComplete(p));
		
		int result = Tcl_RecordAndEval(ip, p, 0);
		if (result != TCL_OK)  {
		    cerr << "grab_procs(): Error" ;
		    if (result != TCL_ERROR) cerr << " " << result;
		    if (Tcl_GetStringResult(ip)[0]) 
			cerr << ": " <<Tcl_GetStringResult( ip);
		    cerr << endl;
		}

		assert(result == TCL_OK);
	    }
        }
    }
    free(procs);
    Tcl_DStringFree(&buf);
}

static void create_stdcmd(Tcl_Interp* ip)
{
    Tcl_CreateCommand(ip, TCL_CVBUG "debugflags", t_debugflags, 0, 0);
    Tcl_CreateCommand(ip, TCL_CVBUG "__assert", t_assert, 0, 0);
    Tcl_CreateCommand(ip, TCL_CVBUG "timeofday", t_timeofday, 0, 0);
    Tcl_CreateCommand(ip, TCL_CVBUG "random", t_random, 0, 0);
    Tcl_CreateCommand(ip, TCL_CVBUG "read_random", t_read_random, 0, 0);
    Tcl_CreateCommand(ip, TCL_CVBUG "write_random", t_write_random, 0, 0);
    Tcl_CreateCommand(ip, TCL_CVBUG "debuginfo", t_debuginfo, 0, 0);
    Tcl_CreateCommand(ip, TCL_CVBUG "allow_remote_command", t_allow_remote_command, 0, 0);

    Tcl_CreateCommand(ip, TCL_CVBUG "fork_thread", t_fork_thread, 0, 0);
    Tcl_CreateCommand(ip, TCL_CVBUG "join_thread", t_join_thread, 0, 0);
    Tcl_CreateCommand(ip, TCL_CVBUG "sync", t_sync, 0, 0);
    Tcl_CreateCommand(ip, TCL_CVBUG "sync_thread", t_sync_thread, 0, 0);
    Tcl_CreateCommand(ip, TCL_CVBUG "yield", t_yield, 0, 0);

    Tcl_CreateCommand(ip, TCL_CVBUG "link_to_inter_thread_comm_buffer", t_link_to_inter_thread_comm_buffer, 0, 0);
    Tcl_CreateCommand(ip, TCL_CVBUG "echo", t_echo, 0, 0);
    Tcl_CreateCommand(ip, TCL_CVBUG "pecho", t_pecho, 0, 0); // echo also to purify logfile

    // Tcl_CreateCommand(ip, TCL_CVBUG "verbose", t_verbose, 0, 0);
    Tcl_CreateCommand(ip, TCL_CVBUG "exit", t_exit, 0, 0);
    Tcl_CreateCommand(ip, TCL_CVBUG "time", t_time, 0, 0);
}

class  __shared {
public:
	char		line[1000];
#if defined(_WIN32) && defined(NEW_IO)
	win32_compat_event_t	stdin_hdl;
#elif defined(_WIN32)
	sevsem_t	stdin_hdl;
#else
	sfile_read_hdl_t	stdin_hdl;
#endif

#if defined(_WIN32) && !defined(NEW_IO)
	HANDLE		thread;
	HANDLE		thwack;
	bool		done;
#ifdef _MT
	unsigned	threadId;
#else
	DWORD		threadId;
#endif
	static unsigned
#ifndef _MT
	long
#endif
	__stdcall	StdinThreadFunc(void *s);
	w_rc_t		run();
#endif

	__shared(
#if defined(_WIN32) && defined(NEW_IO)
		HANDLE fd
#else
		int fd
#endif
		) :
#if defined(_WIN32) && !defined(NEW_IO)
		stdin_hdl(fd, "stdin_hdl"),
		thread(INVALID_HANDLE_VALUE),
		thwack(INVALID_HANDLE_VALUE),
		done(false),
		threadId(0)
#else
		stdin_hdl(fd)
#endif
	{ line[0] = '\0'; };

	w_rc_t	start();
	w_rc_t	stop();
	w_rc_t	read();
};


#if defined(_WIN32) && !defined(NEW_IO)
unsigned
#ifndef _MT
long
#endif
__stdcall __shared::StdinThreadFunc(void *s)
{
	DBGTHRD(<<"StdinThreadFunc " );

	__shared *shared = (__shared *)s;

	W_IGNORE(shared->run());
	
	DBGTHRD(<<"StdinThreadFunc finished" );
	return 0;
}

w_rc_t	__shared::run()
{
	DWORD	what;
	while (!done) {
		what = WaitForSingleObject(thwack, INFINITE);
		if (done)
			break;
		if (what != WAIT_OBJECT_0) {
			w_rc_t	e = RC(fcWIN32);
			cerr << "WaitForSingleObject:" << endl << e << endl;
			W_COERCE(e);
		}
		DBGTHRD("StdinThreadFunc: getline");
		cin.getline(line, sizeof(line) - 2);
		DBGTHRD("StdinThreadFunc: line is " << line);
		DBGTHRD("StdinThreadFunc: post");
		W_IGNORE(stdin_hdl.post());
	}

	return RCOK;
}
#endif

w_rc_t	__shared::start()
{
#if defined(_WIN32) && !defined(NEW_IO)
	thwack = CreateEvent(NULL, FALSE, FALSE, NULL);
	if (thwack == INVALID_HANDLE_VALUE) {
		w_rc_t e = RC(fcWIN32);
		cerr << "CreateEvent():" << endl << e << endl;
		return e;
	}

#ifdef _MT
	thread = (HANDLE) _beginthreadex(0, 0,
	    StdinThreadFunc, (void*) this, 
	    THREAD_TERMINATE | THREAD_SUSPEND_RESUME | THREAD_QUERY_INFORMATION,
	    &threadId); 
#else
	thread = CreateThread(0, 0,
	    StdinThreadFunc, (void*) this, 
	    THREAD_TERMINATE | THREAD_SUSPEND_RESUME | THREAD_QUERY_INFORMATION,
	    &threadId); 
#endif
	DBGTHRD(<< "Forked StdinThreadFunc");
	if (thread == INVALID_HANDLE_VALUE) {
		w_rc_t e = RC(fcWIN32);
		cerr << "Cannot create stdin thread:" << endl << e << endl;
		CloseHandle(thwack);
		thwack = INVALID_HANDLE_VALUE;
		return e;
	}
#endif
	return  RCOK;
}


rc_t	__shared::stop()
{
#if defined(_WIN32) && !defined(NEW_IO)
	BOOL	ok;
	DWORD	what;

	done = true;
	ok = SetEvent(thwack);
	if (!ok) {
		w_rc_t e = RC(fcWIN32);
		cerr << "SetEvent:" << endl << e << endl;
		W_COERCE(e);
	}
	what = WaitForSingleObject(thread, INFINITE);
	if (what != WAIT_OBJECT_0) {
		w_rc_t e = RC(fcWIN32);
		cerr << "WaitForSingleObject:" << endl << e << endl;
		W_COERCE(e);
	}
	CloseHandle(thwack);
	CloseHandle(thread);
#endif
	return RCOK;
}

w_rc_t	__shared::read()
{
#if defined(_WIN32) && !defined(NEW_IO)
	DBGTHRD("StdinThreadFunc: wait");
	BOOL ok;
	ok = SetEvent(thwack);
	if (!ok) {
		w_rc_t e = RC(fcWIN32);
		cerr << "SetEvent:" << endl << e << endl;
		W_COERCE(e);
	}
#endif
	W_IGNORE(stdin_hdl.wait(-1));
#if !defined(_WIN32) || (defined(_WIN32) && defined(NEW_IO))
	cin.getline(line, sizeof(line) - 2);
#endif
	return RCOK;
}


static void process_stdin(Tcl_Interp* ip)
{
    FUNC(process_stdin);
    int partial = 0;
    Tcl_DString buf;
    Tcl_DStringInit(&buf);
#ifdef _WIN32
    const int tty = _isatty(_fileno(stdin));
#else
    const int tty = isatty(0);
#endif
#if defined(_WIN32) && defined(NEW_IO)
	HANDLE	stdin_hdl;
	stdin_hdl = GetStdHandle(STD_INPUT_HANDLE);
	if (stdin_hdl == INVALID_HANDLE_VALUE) {
		w_rc_t	e = RC(fcWIN32);
		cerr << "tcl thread lacks std input: GetStdHandle(STDIN):"
			<< endl << e << endl;
		return;
	}
	__shared	shared(stdin_hdl);
#else
    __shared shared(0);
#endif

    if (tty)
        W_COERCE(shared.start());

    while (1) {
	cin.clear();
	if (tty) {
	    char* prompt = Tcl_GetVar(ip, TCL_CVBUG (partial ? "tcl_prompt2" :
					   "tcl_prompt1"), TCL_GLOBAL_ONLY);
	    if (! prompt) {
		if (! partial)  cout << "% " << flush;
	    } else {
		if (Tcl_Eval(ip, prompt) != TCL_OK)  {
		    cerr << Tcl_GetStringResult(ip) << endl;
		    Tcl_AddErrorInfo(ip,
			 TCL_CVBUG "\n    (script that generates prompt)");
		    if (! partial) cout << "% " << flush;
		} else {
		    ::fflush(stdout);
		}
	    }

	    // wait for stdin to have data
	    W_COERCE(shared.read());
	    DBG(<< "stdin is ready");
	}
	else
		cin.getline(shared.line, sizeof(shared.line) - 2);

	DBGTHRD("StdinThreadFunc: line is " << shared.line);
	shared.line[sizeof(shared.line)-2] = '\0';
	strcat(shared.line, "\n");
	if ( !cin) {
	    if (! partial)  {
		DBGTHRD("break: !cin && !partial" << shared.line);
		break;
	    }
	    shared.line[0] = '\0';
	}
	char* cmd = Tcl_DStringAppend(&buf, shared.line, -1);
	DBGTHRD("StdinThreadFunc: line is " << shared.line
		<< "calling CommandComplete for " << cmd);
	if (shared.line[0] && ! Tcl_CommandComplete(cmd))  {
	    DBG(<< "is partial");
	    partial = 1;
	    continue;
	}
	DBGTHRD("StdinThreadFunc:  complete: record and eval:" << cmd);
	partial = 0;
	int result = Tcl_RecordAndEval(ip, cmd, 0);
	DBGTHRD("Tcl_RecordAndEval returns:" << result);
	Tcl_DStringFree(&buf);
	if (result == TCL_OK)  {
	    DBGTHRD("TCL_OK:" << Tcl_GetStringResult(ip));
	    char *r = Tcl_GetStringResult(ip);
	    if (*r) {
		cout << r<< endl;
	    }
	} else {
	    char *r = Tcl_GetStringResult(ip);
	    if (result == TCL_ERROR && !strcmp(r, TCL_EXIT_ERROR_STRING))  {
		DBGTHRD("TCL_ERROR:" << r);
		break;
	    }  else  {
	        cerr << "process_stdin(): Error";
	        if (result != TCL_ERROR) cerr << " " << result;
	        if (*r) { cerr << ": " << r; }
	        cerr << endl;
	    }
	}
    }
    if (tty)
        W_COERCE(shared.stop());
}

void tcl_thread_t::run()
{
    W_COERCE( thread_mutex.acquire() );
    if (args) {
	int result = Tcl_Eval(ip, args);
	if (result != TCL_OK)  {
	    cerr << "Error in tcl thread at line " <<  __LINE__  
		<< " file " << __FILE__
	        << ": " << Tcl_GetStringResult(ip) << endl;
	    cerr << "Command is " << args <<endl;
	}
    } else {
	process_stdin(ip);
    }
    if (xct() != NULL) {
	cerr << "Dying thread is running a transaction: aborting ..." << endl;
	w_rc_t rc = sm->abort_xct();
	if(rc) {
	    cerr << "Cannot abort tx : " << rc << endl;
	    if(rc.err_num() == ss_m::eTWOTRANS)  {
		me()->detach_xct(xct());
	    }
	} else {
	    cerr << "Transaction abort complete" << endl;
	}
    }
    thread_mutex.release();
    
    // post this in case someone is waiting for us when we die.  That will
    // unblock them, and we will be dead if they wait for us again.

    // cout << "exiting tcl_thread " << id << " nudging waiters" << endl;

#ifdef SSH_SYNC_OLD
    W_IGNORE(sync_point.post());
#else
    W_COERCE(lock.acquire());
    hasExited = true;
    quiesced.signal();
    lock.release();
#endif
}

void tcl_thread_t::join(unsigned long id)
{
    tcl_thread_t *r = find(id);
    if (r)  {
	W_COERCE( r->wait() );
	delete r;
    }
}

void tcl_thread_t::sync_other(unsigned long id)
{
    tcl_thread_t *r = find(id);
    if (r && r->status() != r->t_defunct)  {
	    
	    // cout << "thread " << me()->id << " waiting for " << id << endl;
	    
#ifdef SSH_SYNC_OLD
	    W_COERCE( r->sync_point.wait() );
	    W_IGNORE( r->sync_point.reset(0) );
#else
	    W_COERCE(r->lock.acquire());
	    while (!(r->isWaiting || r->hasExited))
	    	W_COERCE(r->quiesced.wait(r->lock));
	    r->isWaiting = false;
#endif
		
	    // cout << "thread " << me()->id << " awake " << id << endl;
	    
	    assert(r->proceed.is_hot());

#ifdef SSH_SYNC_OLD
	    W_COERCE( r->lock.acquire() );
#else
	    r->canProceed = true;
#endif
	    r->proceed.signal();
	    r->lock.release();

	    assert(!r->proceed.is_hot());
	    smthread_t::yield();
    }
}

void tcl_thread_t::sync()
{
	// cout << "thread " << me()->id << " wait" << endl;

#ifdef SSH_SYNC_OLD
	W_IGNORE(sync_point.post());
	W_COERCE( lock.acquire() );
	W_COERCE( proceed.wait(lock) );
	lock.release();
#else
	W_COERCE(lock.acquire());
	isWaiting = true;
	quiesced.signal();
	while (!canProceed)
		W_COERCE(proceed.wait(lock));
	canProceed = false;
	lock.release();
#endif
    
	// cout << "thread " << me()->id << " woke" << endl;
}

void
copy_interp(Tcl_Interp *ip, Tcl_Interp *pip)
{
    if(tcl_init_cmd) {
	int result = Tcl_Eval(ip, TCL_CVBUG tcl_init_cmd);

	if (result != TCL_OK)  {
	    cerr << "tcl_thread_t(): Error evaluating command:"
		    << tcl_init_cmd <<endl ;
	    if (result != TCL_ERROR) cerr << "     " << result;
	    char *r = Tcl_GetStringResult(ip);
	    if (*r)  cerr << ": " << r;
	    cerr << endl;
	    w_assert3(0);;
	}
    }

    /* These three are done separately because 
     * they are done at a different time from create_stdcmd() 
     * in the initial (main() ) case
     */
    Tcl_CreateCommand(ip, TCL_CVBUG "vtable", vtable_dispatch, 0, 0);
    Tcl_CreateCommand(ip, TCL_CVBUG "sm", sm_dispatch, 0, 0);
#ifdef USE_COORD
    Tcl_CreateCommand(ip, TCL_CVBUG "co", co_dispatch, 0, 0);
#endif
    Tcl_CreateCommand(ip, TCL_CVBUG "st", st_dispatch, 0, 0);

    create_stdcmd(ip); // creates more commands

    grab_vars(ip, pip);
    grab_procs(ip, pip);
    Tcl_ResetResult(ip);
    Tcl_ResetResult(pip);
}

/* The increased stack size is because TCL is stack hungry */
tcl_thread_t::tcl_thread_t(int ac, char* av[],
			   Tcl_Interp* pip,
			   bool block_immediate, bool auto_delete)
: smthread_t(t_regular, block_immediate, auto_delete, "tcl_thread", WAIT_FOREVER, sthread_t::default_stack * 2),
#ifdef SSH_SYNC_OLD
  sync_point(0,"tcl_th.sync"),
#else
  isWaiting(false),
  canProceed(false),
  hasExited(false),
  quiesced("tcl_th.sync"),
#endif
  proceed("tcl_th.go"),
  args(0),
  ip(Tcl_CreateInterp()),
  thread_mutex("tcl_th") // m:tcl_th
{
    /* give thread & mutex a unique name, not just "tcl_thread" */
    {
	char	buf[40];
	ostrstream	o(buf, sizeof(buf)-1);
	W_FORM2(o,("tcl_thread(%d)", id));
	o << ends;
	rename(o.str());
	W_FORM2(o,("tcl_thread_mutex(%d)", id));
	o << ends;
	thread_mutex.rename(o.str());
    }

    w_assert1(ip);
    
    W_COERCE( thread_mutex.acquire() );

    if (++count == 1)  {
	assert(sm == 0);
	// Try without callback function.
	if(log_warn_callback) {
	   sm = new ss_m(out_of_log_space);
	} else {
	   sm = new ss_m();
	}
	if (! sm)  {
	    cerr << __FILE__ << ':' << __LINE__
		 << " out of memory" << endl;
	    thread_mutex.release();;
	    W_FATAL(fcOUTOFMEMORY);
	}
	W_COERCE( sm->config_info(global_sm_config_info));
    }
    unsigned int len;
    int i;
    for (i = 0, len = 0; i < ac; i++)
	len += strlen(av[i]) + 1;

    if (len) {
	args = new char[len];
	w_assert1(args);

	args[0] = '\0';
	for (i = 0; i < ac; i++) {
		if (i)
			strcat(args, " ");
		strcat(args, av[i]);
	}
	w_assert1(strlen(args)+1 == len);
    }

    copy_interp(ip, pip);

    list.push(this);

    thread_mutex.release();
}

tcl_thread_t::~tcl_thread_t()
{
    W_COERCE( thread_mutex.acquire() );
    if (--count == 0)  {
	//COERCE(sm->dismount_all());
	delete sm;
	sm = 0;
#ifdef USE_COORD
	delete globalDeadlockServer;
	globalDeadlockServer = 0;
#endif
    }

    if (ip) {
	Tcl_DeleteInterp(ip);
	ip = 0;
    }
    
    delete [] args;

    // cout << "thread " << id << " died" << endl;
    
    link.detach();

    thread_mutex.release();
}

void tcl_thread_t::initialize(Tcl_Interp* ip, const char* lib_dir)
{
    static int first_time = 1;
    
    if (first_time)  {
	first_time = 0;
	
	char buf[ss_m::max_devname + 1];
	ostrstream s(buf, sizeof(buf));
	s << lib_dir << "/ssh.tcl" << '\0';
	if (Tcl_EvalFile(ip, buf) != TCL_OK)  {
	    cerr << __FILE__ << ':' << __LINE__ << ':'
		 << " error in \"" << buf << "\" script" << endl;
	    cerr << Tcl_GetStringResult(ip) << endl;
	    W_FATAL(fcINTERNAL);
	}
    }

    create_stdcmd(ip);
}

tcl_thread_t	*tcl_thread_t::find(unsigned long id)
{
	w_list_i<tcl_thread_t>	i(list);
	tcl_thread_t		*r;
	while ((r = i.next()) && (r->id != id))
		;
	return r;
}

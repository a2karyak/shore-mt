/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

#include <sm_int.h>
extern "C" {
#include <rpc/rpc.h>
}
#include <tcl.h>

#define RPC_SVC

extern "C" {
#include "msg.h"
}

/*
 * sysdefs.h defines bzero to be an error message to force users
 * to use memset.  But, the FD_ZERO system macro uses bzero on
 * sunos, so undef it here.
 */
#ifdef Sparc
#undef bzero 
#endif

int tcp_sock = -1;
typedef void XPFUNC(...);

XPFUNC* xp_destroy = 0;

/*
 *  shutting_down condition
 *	svc_run() waits on shutting_down while the system services
 *	client requests. To allow svc_run() to proceed,
 *	signal shutting_down.
 */
scond_t shutting_down("shutting_down");

/*
 *  quiesce_clients condition
 *	when svc_run() is allowed to shutdown, it waits on quiesce_clients
 *	condition until active_client_threads == 0.
 */
int active_client_threads = 0;
scond_t quiesce_clients("quiesce_clients");

/*
 *  buffer reservation for clients
 *	bufsize is the maximum read-write buffer size.
 *	There are maxclient client_buf structures linked up in free_pool.
 *	When a tcp connection is established, a client_buf is reserved for
 *	that client (index on socket id).
 *
 *	bufmutex is used to ensure atomic accesses to free_pool.
 *	bufavail is waited upon (signaled) when buf_pool is empty (not empty).
 */
struct clbuf_t {
    int		sock;
    xct_t*	xd;
    bool	got_partial;
    Tcl_Interp* interp;
    Tcl_CmdBuf	buffer;
    ssh_reply_t	reply;

    clbuf_t(int sock);
    link_t	hash_link;
    int		hash_key()	{ return sock; }
};

char initCmd[] = "if [file exists [info library]/init.tcl] {source [info library]/init.tcl}";

extern void sm_shell_init(Tcl_Interp*);

clbuf_t::clbuf_t(int s)
: sock(s), got_partial(0), xd(0),
  interp(Tcl_CreateInterp()), buffer(Tcl_CreateCmdBuf())
{
    assert1(interp);
    int result = Tcl_Eval(interp, initCmd, 0, 0);
    if (result != TCL_OK) {
	cerr << interp->result << endl;
	SM_FATAL(eFAILURE);
    }
    sm_shell_init(interp);
}

class cltab_t {
public:
    cltab_t() : tab(OPEN_MAX), _mutex("cltab_t::mutex")  {};
    void create(int s)		{ 
	_mutex.acquire();
	_create(s); 
	_mutex.release();
    }
    void destroy(int s)		{
	_mutex.acquire();
	_destroy(s); 
	_mutex.release();
    }
    clbuf_t* find(int s)	{ 
	_mutex.acquire();
	clbuf_t* p = _find(s); 
	_mutex.release();
	return p;
    }
private:
    smutex_t _mutex;
    void _create(int s);
    void _destroy(int s);
    clbuf_t* _find(int s);
    hash_t<clbuf_t, int> tab;
};

void cltab_t::_create(int s)
{
    clbuf_t* p = new clbuf_t(s);
    assert1(p);

    tab.push(p);
}

void cltab_t::_destroy(int s)
{
    delete tab.remove(s);
}

clbuf_t* cltab_t::_find(int s)
{
    return tab.lookup(s);
}

#define VERBOSE(x)   printf x

cltab_t cltab;

/*
 *  hook for callback from rpc layer when a transport is destroyed
 */
static void
rpc_destroy(SVCXPRT* xprt)
{
    VERBOSE(("client %d died\n", xprt->xp_sock));

    cltab.destroy(xprt->xp_sock);

    smthread_t::destroy_filehandler(xprt->xp_sock);
    xp_destroy(xprt);
}

/*
 *  create a tcp host on socket sock
 */
SVCXPRT* tcp_create(int sock)
{
    assert(tcp_sock == -1);
    SVCXPRT* xp = svctcp_create(sock, 0, 0);
    if (xp == 0) {
	fprintf(stderr, "cannot create tcp service\n");
	exit(-1);
    }

    xp_destroy = xp->xp_ops->xp_destroy;
    xp->xp_ops->xp_destroy = (XPFUNC*) rpc_destroy;

    tcp_sock = xp->xp_sock;
    
    return xp;
}

/*
 *  get a request off socket sock
 */
static void
get_request(int sock)
{
    ++active_client_threads;
    fd_set r;
    FD_ZERO(&r);
    FD_SET(sock, &r);

    assert(FD_ISSET(sock, &svc_fdset));
    svc_getreqset(&r);

    if (! (FD_ISSET(sock, &svc_fdset)))  {
	/* socket has been destroyed by rpc */
	cltab.destroy(sock);
	smthread_t::destroy_filehandler(sock);
    }
    
    if (--active_client_threads == 0) {
	/* this is the last active_client_threads */
	quiesce_clients.broadcast();
    }
}

/*
 *  tcp connection is ready for something
 *	-- fork a thread to get a request off the socket
 */
static void
tcp_ready(int sock, int mask, void*)
{
    assert(mask == smthread_t::rd);
    smthread_t* t = new smthread_t((threadproc_t*) get_request,
				 (void*) sock, smthread_t::_delete);
    assert(t);
}

/*
 *  A tcp connection has been requested
 *	--- create a new client
 */
static void
tcp_accept(int sock, int mask, void*)
{
    assert(sock == tcp_sock);
    assert(mask == smthread_t::rd);
    fd_set r;
    FD_ZERO(&r);
    FD_SET(sock, &r);
    assert(FD_ISSET(sock, &svc_fdset));

    svc_getreqset(&r);
    for (register i = OPEN_MAX; i > 0; i--) {
	if ((!smthread_t::watching_filehandler(i)) &&
	    (FD_ISSET(i, &svc_fdset)))  {
	    VERBOSE(("new connection %d\n", i));

	    cltab.create(i);
	    smthread_t::create_filehandler(i, smthread_t::rd, tcp_ready, 0);
	}
    }
}

/*
 *  stdin input
 *	--- print status
 */
static void
stdin_ready(int fd, int, void*)
{
    assert(fd == 0);
    char buf[80];
    int n;
    while ((n = read(fd, buf, sizeof(buf))) == sizeof(buf));
    if (n < 0) {
	perror("read");
	exit(-1);
    }
    smthread_t::dump();
}
    
	
/*
 *  set up (initialize) everything and wait on shutting_down condition
 */
void svc_run()
{
    smutex_t mutex("svc_run");
    mutex.acquire();

    if (tcp_sock >= 0) {
	smthread_t::create_filehandler(tcp_sock, smthread_t::rd,
				      tcp_accept, 0);
    }
    
    if (isatty(0))  {
	/* watch stdin */
	smthread_t::create_filehandler(0, smthread_t::rd, stdin_ready, 0);
    }

    /*
     *  This thread goes to sleep here while the server services requests.
     */
    shutting_down.wait(&mutex);
    
    printf("quesce client...\n");
    /* do not accept any more packets */
    smthread_t::destroy_all_filehandlers(); 
    while (active_client_threads > 0) {
	quiesce_clients.wait(&mutex);
    }

    printf("shutting down...\n");
    mutex.release();
}

    

/*
 *   SERVER STUBS
 */
extern "C"
ssh_reply_t* perform_1(ssh_cmd_t* arg, svc_req* req)
{
    clbuf_t* p = cltab.find(req->rq_xprt->xp_sock);
    
    me()->attach_xct(p->xd);
    
    ssh_reply_t* reply = &p->reply;
    char* cmd = Tcl_AssembleCmd(p->buffer, arg->line.line_val);
    cout << "% " << cmd << endl;
    if (! cmd) {
	reply->got_partial = TRUE;
	return reply;
    }

    reply->got_partial = FALSE;
    reply->result = Tcl_RecordAndEval(p->interp, cmd, 0);
    reply->output.output_len = strlen(p->interp->result);
    reply->output.output_val = p->interp->result;

    p->xd = me()->xct();

    return reply;
}

extern "C" void rsshprog_1(...);
extern "C" {
#include <rpc/pmap_clnt.h>
}

char* Logical_id_flag_tcl = "Use_logical_id";

/*
 *  this function was copied from the msg_svc.c generated by rpcgen
 */
main()
{
    SVCXPRT *transp;
    
    pmap_unset(RSSHPROG, RSSHVERS);

#ifdef UDP_SERVER
    transp = udp_create(RPC_ANYSOCK);
    if (transp == NULL) {
	fprintf(stderr, "cannot create udp service.\n");
	exit(1);
    }
    
    if (!svc_register(transp, RSSHPROG, RSSHVERS, rsshprog_1,
		      IPPROTO_UDP)) {
	fprintf(stderr,
		"unable to register (RSSHPROG, RSSHVERS, udp).\n");
	exit(1);
    }
#endif /*UDP_SERVER*/
    
    transp = tcp_create(RPC_ANYSOCK);
    if (transp == NULL) {
	fprintf(stderr, "cannot create tcp service.\n");
	exit(1);
    }
    
    if (!svc_register(transp, RSSHPROG, RSSHVERS, rsshprog_1,
		      IPPROTO_TCP)) {
	fprintf(stderr,
		"unable to register (RSSHPROG, RSSHVERS, tcp).\n");
	exit(1);
    }

    svc_run();
}


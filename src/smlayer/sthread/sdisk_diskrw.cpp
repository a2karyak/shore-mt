/*<std-header orig-src='shore'>

 $Id: sdisk_diskrw.cpp,v 1.16 2000/03/17 23:54:56 bolo Exp $

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
 *   NewThreads is Copyright 1992, 1993, 1994, 1995, 1996, 1997, 1998 by:
 *
 *	Josef Burger	<bolo@cs.wisc.edu>
 *	Dylan McNamee	<dylan@cse.ogi.edu>
 *      Ed Felten       <felten@cs.princeton.edu>
 *
 *   All Rights Reserved.
 *
 *   NewThreads may be freely used as long as credit is given
 *   to the above authors and the above copyright is maintained.
 */

#define	SDISK_DISKRW_C

#define W_INCL_LIST
#define W_INCL_SHMEM
#define W_INCL_TIMER
#include <w.h>

#include <w_debug.h>
#include <w_stream.h>
#include <w_strstream.h>
#include <w_signal.h>
#include <stdlib.h>
#ifndef _WINDOWS
#include <unistd.h>
#endif
#include <string.h>

#ifdef _WINDOWS
#include <time.h>
#else
#include <sys/time.h>
#endif

#ifndef _WINDOWS
#include <sys/wait.h>
#endif
#include <new.h>

#include <sys/stat.h>
#include <w_rusage.h>

#if defined(Sparc) && !defined(SOLARIS2)
#include <vfork.h>
#endif

#if defined(Mips) && !defined(Irix)
extern "C" int vfork();
#endif

#if defined(Mips) && defined(Irix)
#define vfork fork
#endif

#if defined(AIX32) && !defined(AIX41)
#define	vfork	fork
#endif

#include <w_statistics.h>

#ifdef _WINDOWS 
#include <io.h>
#include <process.h>
#ifndef NEW_IO
#include <w_winsock.h>
#endif
#endif


#include <unix_stats.h>

#include "sthread.h"
#include "sthread_stats.h"
#include "spin.h"
#include "diskrw.h"
#include "ready_q.h"
// #include "st_error_enum_gen.h"	/* XXX */

const int	stOS = fcOS;
const int	stINVAL = sthread_base_t::stINVAL;
const int	stTOOMANYOPEN = sthread_base_t::stTOOMANYOPEN;
const int	stBADFD = sthread_base_t::stBADFD;
const int	stBADADDR = sthread_base_t::stBADADDR;
const int	stBADIOVCNT = sthread_base_t::stBADIOVCNT;
const int	stNOTIMPLEMENTED = sthread_base_t::stNOTIMPLEMENTED;

#if defined(_WIN32) && defined(NEW_IO)
#include <win32_events.h>
#else
#include <sfile_handler.h>
#endif

#include <sdisk.h>
/* Drag this in for convert_flags */
#include <sdisk_unix.h>
#include <sdisk_diskrw.h>

#include <fcntl.h>

extern class sthread_stats SthreadStats;

#if defined(SOLARIS2) || defined(Linux) || defined(__NetBSD__) || defined(OSF1)
#define	EXECVP_AV(x)	((char * const *)(x))
#else
#define	EXECVP_AV(x)	(x)
#endif

#if defined(_WIN32) && !defined(NEW_IO)

class listening_socket {

public:
    NORET listening_socket():
	 listenFd(INVALID_SOCKET),
	 portNum(0) {}

    NORET ~listening_socket() {
	close();
    }

    short port() { return portNum; }
    SOCKET fd() { return listenFd; }
    void stash(SOCKET fd, short p) {
	listenFd = fd;
	portNum = p;
    }
    void close() {
	if(listenFd != INVALID_SOCKET) {
	    ::closesocket(listenFd);
	    listenFd = INVALID_SOCKET;
	    portNum = 0;
	}
    }

private:
    SOCKET 		listenFd;
    short 		portNum;
} listener;

#ifdef _MT
unsigned
#else
DWORD
#endif
__stdcall ConnectThreadFunc(void* port)
{
    struct hostent* 	hostp;
    int* 		filedes = (int*)port;
    short 		portNum = (short)filedes[0];
    struct sockaddr_in 	connAddr;

    memset((char*)&connAddr, 0, sizeof(connAddr));
    connAddr.sin_family = AF_INET;
    connAddr.sin_port = htons(portNum);
    hostp = gethostbyname("localhost");
    if(!hostp) {
#ifdef notyet
	w_rc_t e = RC(fcDNS);
#else
	w_rc_t e = RC(fcWIN32);
#endif
	cerr << "gethostbyname(\"localhost\"):" << endl << e << endl;
	W_COERCE(e);
    }
    memcpy((void*)&connAddr.sin_addr, hostp->h_addr, hostp->h_length);

    SOCKET sockFd = socket(AF_INET, SOCK_STREAM,0) ;
    if (sockFd == INVALID_SOCKET) {
	w_rc_t e = RC(fcWIN32);
	cerr << "socket(AF_INET, SOCK_STREAM):" << endl << e << endl;
#ifdef _MT
	_endthreadex(e.err_num());
#else
	ExitThread(e.err_num());
#endif
    }
    {
	int 	error = 0;
	int	value = 0;
	int 	valuelen = sizeof(value);

	value = 1;
	error = setsockopt(sockFd, IPPROTO_TCP, TCP_NODELAY, 
		(char *)&value, valuelen);
	if (error == -1) {
		w_rc_t e = RC(fcWIN32);
		cerr << "threadFunc: setsockopt(TCP_NODELAY):" 
			<< endl << e <<endl;
#ifdef _MT
		_endthreadex(error);
#else
		ExitThread(error);
#endif
		// return -1;
	}
	value = 0;
	error = getsockopt(sockFd, IPPROTO_TCP, TCP_NODELAY, 
		    (char *)&value, &valuelen);
	if (error == -1) {
		w_rc_t e = RC(fcWIN32);
		cerr << "threadFunc: getsockopt(TCP_NODELAY):" 
			<< endl << e << endl;
		return -1;
	}
	if (!value)
		cerr << "WARNING: threadfunc: TCP_NODELAY not enabled!" << endl;
    }
    int 	error = 0;
    error = connect(sockFd, (struct sockaddr*)&connAddr, sizeof(connAddr));
    if (error == -1) {
	w_rc_t e = RC(fcWIN32);
	cerr << "threadFunc: connect:"  << endl << e << endl;
#ifdef _MT
	_endthreadex(error);
#else
	ExitThread(error);
#endif
	// return error;
    }
#ifdef W_DEBUG
    {
	struct sockaddr_in sa;
	int salen = sizeof(sa);
	error = getsockname(sockFd, (struct sockaddr *)&sa, &salen);
	if (error == -1 ) {
	    w_rc_t e = RC(fcWIN32);
	    cerr << "threadfunc: getsockname:"  << endl << e << endl;
	    cerr << "Cannot create pseudo-pipe. " << endl;
#ifdef _MT
	    _endthreadex(error);
#else
	    ExitThread(error);
#endif
	    // return -1;
	}
	error = getpeername(sockFd, (struct sockaddr *)&sa, &salen);
	if (error == -1 ) {
	    w_rc_t e = RC(fcWIN32);
	    cerr << "threadfunc: getpeername:"  << endl << e << endl;
	    cerr << "Cannot create pseudo-pipe. " << endl;
#ifdef _MT
	    _endthreadex(error);
#else
	    ExitThread(error);
#endif
	    //return -1;
	}
    }
#endif
 
    filedes[PIPE_OUT] = sockFd;
    DBGTHRD(<<"_endthreadex(0)");
#ifdef _MT
    _endthreadex(0);
#else
    ExitThread(0);
#endif
    return 0;
}


int pipe(int* filedes)
{

    int                         sockFd = 0;
    short			portNum = 0;
    struct sockaddr_in          servAddr ;  

    if(listener.fd() == INVALID_SOCKET) {
	/* Get a port from the kernel and listen on it; 
	 * stash the listening fd  in listenFd
	 */
	memset((char*)&servAddr, 0, sizeof(struct sockaddr_in));
	servAddr.sin_family = AF_INET;
	{
	    struct hostent* 	hostp;
	    hostp = gethostbyname("localhost");
	    if(!hostp) {
#ifdef notyet
		w_rc_t e = RC(fcDNS);
#else
		w_rc_t e = RC(fcWIN32);
#endif
		    cerr << "gethostbyname(\"localhost\"):" 
			<< endl << e <<endl;
		W_COERCE(e);
	    }
	    memcpy((void*)&servAddr.sin_addr, hostp->h_addr, hostp->h_length);
	}
	// servAddr.sin_addr.s_addr = htonl(INADDR_ANY);
	servAddr.sin_port = htons((unsigned short) portNum);
	int 	error = 0;
	sockFd = socket(AF_INET, SOCK_STREAM,0) ;
	if (sockFd == INVALID_SOCKET) {
		w_rc_t e = RC(fcWIN32);
		cerr << "listener: socket():" << endl << e << endl;
		W_COERCE(e);
	}
	{
	    /* Make sure REUSEADDR is off */
	    int	        value = 0;
	    int 	valuelen = sizeof(value);

	    value = 0;
	    error = getsockopt(sockFd, SOL_SOCKET, SO_REUSEADDR, 
		    (char *)&value, &valuelen);
	    if (error == -1) {
		    w_rc_t e = RC(fcWIN32);
		    cerr << "threadFunc getsockopt(SO_REUSEADDR):" 
			<< endl << e << endl;
		    cerr << "Cannot create pseudo-pipe. " << endl;
		    return -1;
	    }
            if(value != 0) {
		value = 0;
		error = setsockopt(sockFd, SOL_SOCKET, SO_REUSEADDR, 
		    (char *)&value, valuelen);
		if (error == -1) {
		    w_rc_t e = RC(fcWIN32);
		    cerr << "threadFunc setsockopt(SO_REUSEADDR):" 
			<< endl << e <<endl;
		    cerr << "Cannot create pseudo-pipe. " << endl;
		    return -1;
		}
	    }
	}


	while(1) {
	    servAddr.sin_addr.s_addr = htonl(INADDR_ANY);
	    /*
	     // servAddr.sin_port = htons(portNum);
	     * Let kernel decide port: this is necessary due to a
	     * kernel bug in NT
	     */
	    servAddr.sin_port = 0; 
	    if (bind(sockFd, (struct sockaddr*)&servAddr,
		    sizeof(struct sockaddr))== 0)   {
			int salen = sizeof(servAddr);
			error = getsockname(sockFd, (struct sockaddr *)&servAddr, &salen);
			if (error == -1 ) {
			    w_rc_t e = RC(fcWIN32);
			    cerr << "getsockname():"  << endl << e << endl;
			    cerr << "Cannot create pseudo-pipe. " << endl;
			    return -1;
			}
			portNum = ntohs(servAddr.sin_port);
			break;
	    }
	    int err = WSAGetLastError();

	    if (err != WSAEADDRINUSE) {
		w_rc_t e = RC2(fcWIN32, err);
		cerr << "bind:" << endl << e << endl;
		return -1;    
	    }
	    if (--portNum <= 2000) {
		cerr << "Out of ports!" << endl;
		return -1;      
	    }
	}

	// temporarily put portNum in filedes
	filedes[0] = portNum;
    {
	    /* Make sure TCP_NODELAY is ON */
	    int 	error = 0;
	    int	value = 0;
	    int 	valuelen = sizeof(value);
	    int     tries = 0;

	    value = 1;
	    error = setsockopt(sockFd, IPPROTO_TCP, TCP_NODELAY, 
			(char *)&value, valuelen);
	    if (error == -1) {
		w_rc_t e = RC(fcWIN32);
		cerr << "threadFunc: setsockopt(TCP_NODELAY):" 
			<< endl << e << endl;
#ifdef _MT
		_endthreadex(error);
#else
		ExitThread(error);
#endif
		// return error;
	    }
	    value = 0;
	    error = getsockopt(sockFd, IPPROTO_TCP, TCP_NODELAY, 
		    (char *)&value, &valuelen);
	    if (error == -1) {
		w_rc_t e = RC(fcWIN32);
		cerr << "threadFunc getsockopt(TCP_NODELAY):" 
			<< endl << e << endl;
		return -1;
	    }
	    if (!value)
		cerr << "WARNING: threadfunc: TCP_NODELAY not enabled!" << endl;
	};

	error = listen(sockFd, 5);
	if (error == -1 ) {
	    w_rc_t e = RC(fcWIN32);
	    cerr << "listen():"  << endl << e << endl;
	    cerr << "Cannot create pseudo-pipe. " << endl;
	    return -1;
	}
	listener.stash(sockFd, portNum);
    }
    sockFd = listener.fd();
    filedes[0] = listener.port();

    // create the other thread to connect

    HANDLE threadHandle;
#ifdef _MT
    unsigned thisThreadId;
    threadHandle = (HANDLE) _beginthreadex(0, 0,
         ConnectThreadFunc, 
         (void*) filedes, 
	THREAD_TERMINATE | THREAD_SUSPEND_RESUME | THREAD_QUERY_INFORMATION,
         &thisThreadId); 
#else
    DWORD thisThreadId;
    threadHandle = CreateThread(0, 0,
         ConnectThreadFunc, 
         (void*) filedes, 
	THREAD_TERMINATE | THREAD_SUSPEND_RESUME | THREAD_QUERY_INFORMATION,
         &thisThreadId); 
#endif
    DBGTHRD(<< "Forked ConnectThreadFunc, got handle " << (int)(threadHandle) );
    if( threadHandle == 0 ) {
        cerr << "Cannot create thread. " << endl;
        return -1;
    }

    {
	/* 
  	 * make sure the thread didn't choke immediately
	 */
	DWORD exitcode=0;
	if(GetExitCodeThread(threadHandle, &exitcode) ) {
	    if (exitcode!=0) {
		if(exitcode != STILL_ACTIVE) {
		    cerr << "ConnectThreadFunc exit code " << exitcode <<endl;
		    cerr << "Cannot create pseudo-pipe." << exitcode << endl;
		    return -1;
		}

	    }
	}
    }

    int size = sizeof(struct sockaddr);
    struct sockaddr_in caddr;
    memset((char*)&caddr, 0, sizeof(caddr));
    SOCKET newsock = accept(sockFd, (struct sockaddr*)&caddr, &size);

    if (newsock == -1) {
	w_rc_t e = RC(fcWIN32);
        cerr << "accept:" << endl << e << endl;
        cerr << "Cannot create pseudo-pipe. " << endl;
        return -1;
    }
    {
	/* Make sure TCP_NODELAY is ON */
	int 	error = 0;
	int	value = 0;
	int 	valuelen = sizeof(value);
        int     tries = 0;

	value = 1;
	error = setsockopt(newsock, IPPROTO_TCP, TCP_NODELAY, 
		    (char *)&value, valuelen);
	if (error == -1) {
	    w_rc_t e = RC(fcWIN32);
	    cerr << "threadFunc setsockopt(TCP_NODELAY):"  << endl << e << endl;
#ifdef _MT
	    _endthreadex(error);
#else
	    ExitThread(error);
#endif
	    // return error;
	}
	value = 0;
	error = getsockopt(newsock, IPPROTO_TCP, TCP_NODELAY, 
		(char *)&value, &valuelen);
	if (error == -1) {
	    w_rc_t e = RC(fcWIN32);
	    cerr << "threadFunc: getsockopt(TCP_NODELAY):" << endl << e <<endl;
	    return -1;
	}
        if (!value)
	    cerr << "WARNING: threadfunc: TCP_NODELAY not enabled!" << endl;
    }

    WaitForSingleObject(threadHandle, INFINITE);

    {
	DWORD exitcode=0;
	if(GetExitCodeThread(threadHandle, &exitcode)  && (exitcode!=0) ) {
	    w_assert3(exitcode != STILL_ACTIVE);
	    cerr << "ConnectThreadFunc exit code " << exitcode <<endl;
	    cerr << "Cannot create pseudo-pipe." << exitcode << endl;
	    return -1;
	}
    }
    // thread should have put its socket in filedes[PIPE_OUT]
    filedes[PIPE_IN] = newsock;       

    DBGTHRD(<< "pipe: returning fd " << newsock); 
    return 1;
}
#endif

#if defined(_WIN32) && defined(NEW_IO)
void	pipe_handler::ready()
{
	handler.polldisk();
}


/* Null-action simulator */
static void FD_NODELAY(HANDLE )
{
	return;
}


int	pipe(HANDLE *pipes)
{
	HANDLE	h;

	/* Event is Auto-Reset, and Not Signalled at Creation */
	h = CreateEvent(NULL, FALSE, FALSE, NULL);
	if (h == INVALID_HANDLE_VALUE) {
		w_rc_t	e = RC(fcWIN32);
		cerr << "pipe(): CreateEvent:" << endl << e << endl;
		return -1;
	}
	/*
	 * Duplicate the handle to both pipe halves to simulate
 	 * Pipe behavior in most code.  The thing that
	 * really differs is close -- only 1 needed, not
	 * 1 for each end.
	 */
	pipes[0] = h;
	pipes[1] = h;

	return 0;
}

int	closepipe(HANDLE pipe_half)
{
	BOOL	ok;

	/* When local I/O is turned on, the pipes are not opened,
	   and the close is attempted on FD_NONE.  The error is
	   ignored, but this error-reporting close would make an
	   error message appear.  This hack gets rid of that 
	   error message. */
	if (pipe_half == INVALID_HANDLE_VALUE)
		return -1;

	ok = CloseHandle(pipe_half);
	if (!ok) {
		w_rc_t e = RC(fcWIN32);
		cerr << "closepipe(): CloseHandle:" << endl << e << endl;
	}

	return ok ? 0 : -1;
}

#else

#if 0
static void
FD_CLOSE_ON_EXEC(int fd)
{
	if (fcntl(fd, F_SETFD, FD_CLOEXEC) == -1) {
		w_rc_t e =  RC(fcOS);
		cerr << "fcntl(close_on_exec):" << endl << e << endl;
	}
}
#endif /*0*/

static void 
FD_NODELAY(int fd)
{
	int n;
#ifdef _WINDOWS
	unsigned long x = 1;
	n = ioctlsocket(fd, FIONBIO, &x);
	if (n == SOCKET_ERROR)
		n = -1;
#else
	n = fcntl(fd, F_SETFL, O_NDELAY);
#endif
	if (n == -1) {
		w_rc_t e = RC(fcOS);
		cerr << "fcntl(O_NDELAY):" << endl << e << endl;
	}
}

void	pipe_handler::read_ready()
{
	ChanToken token;
	int	pending = 0;

#ifdef _WINDOWS
	while  (::recv(fd, (char *)&token, sizeof(token), 0) == sizeof(token)) {
#else

	while  (::read(fd, (char *)&token, sizeof(token)) == sizeof(token)) {
#endif
		pending++;
	}
	if (pending)
		handler.polldisk();
}
#endif


#ifdef notyet
w_rc_t	_caught_signal_io(int )
{
#ifdef notyet
	if (! (sig == SIGINT && _dummy_disk != 0))
		return RCOK;

	// cntrl-C, so signal dummy disk process so it can free memory
	cerr << "ctrl-C detected -- killing disk processes" 
		<< endl << flush;

	// signal dummy diskrw process so it can kill the others
	// and free shared memory.
	if (kill(_dummy_disk, SIGINT) == -1) {
		cerr << 
		"error: cannot kill disk process that cleans up shared memory(" 
		 << _dummy_disk << ')' << endl
		 << RC(fcOS) << endl;
		cerr << "You might have to clean up shared memory segments by hand."
		<< endl;
		cerr << "See ipcs(1) and ipcrm(1)."<<endl;
		_exit(1);	/* XXX */
	} else {
		cerr << "ctrl-C detected -- exiting" << endl;
		_exit(0);	/* XXX */
	}
#endif
	return RCOK;
}
#endif



#ifdef notyet
w_rc_t	sdisk_handler_t::shutdown()
{
	/* shutdown disk i/o */

#ifdef notyet
	/* Clean up and exit. */
	/* XXX needs work */
	if (shmem_seg.base()) {
		kill(_dummy_disk, SIGHUP);
		for (int i = 0; i < open_max; i++)  {
			if (diskport[i].pid && diskport[i].pid != -1)
				kill(diskport[i].pid, SIGHUP);
		}
		if (svcport) {
			svcport->~svcport_t();
			svcport = 0;
		}
		shmem_seg.destroy();
	}
	unname();	// clean up to avoid assertion in fastnew
#endif

	return RCOK;
}
#endif



/*
 *  Allocate and return a shared memory segment of (sz + delta) bytes
 *  for asynchronous disk I/O. The extranous delta bytes in the 
 *  shared memory is used to hold the control structures for
 *  communicating between main process and diskrw processes (e.g.
 *  the queues to submit disk requests).
 */

w_rc_t	sdisk_handler_t::finish_shared()
{
	if (!shmem_seg.base())
		return RCOK;

	/*
	 *  Free shared memory previous allocated.
	 */
#ifndef _WINDOWS
	if (kill(_dummy_disk, SIGUSR2) == -1 ||
	    waitpid(_dummy_disk, 0, 0) == -1)  {
		cerr << "warning: error killing dummy disk pid " 
			<< _dummy_disk << endl
			<< RC(fcOS) << endl;
	}
#endif
	for (unsigned i = 0; i < open_max; i++)  {
		if (diskport[i].pid && diskport[i].pid != -1) {
#ifndef _WINDOWS
			if (kill(diskport[i].pid, SIGTERM) == -1 ||
			    waitpid(diskport[i].pid, 0, 0) == -1)  {
				cerr << "warning: error killing disk pid " 
					<< diskport[i].pid << endl
					<< RC(fcOS) << endl;
			}
#endif
			DBGTHRD(<<"closing fd " <<
				diskport[i].rw_chan[PIPE_OUT] );
#if defined(_WIN32) && defined(NEW_IO)
			/* XXX duplicate? */
			closepipe(diskport[i].rw_chan[PIPE_OUT]);
#elif defined(_WIN32)
			::shutdown(diskport[i].rw_chan[PIPE_OUT], SD_SEND);
			::closesocket(diskport[i].rw_chan[PIPE_OUT]);
#else
			::close(diskport[i].rw_chan[PIPE_OUT]);
#endif
		}
	}

	svcport->~svcport_t();
	W_COERCE( shmem_seg.destroy() );

	diskport = 0;
	svcport = 0;
	_iomem = 0;

	return RCOK;
}


w_rc_t sdisk_handler_t::init_shared(unsigned size)
{
	if (shmem_seg.base())  {
		/*
	 	 *  Cannot re-allocate shared memory.
	 	 */
		return RC(fcFOUND);	// disable for now
	}

	w_rc_t	e;


	/*
	 *  Calculate the extra bytes we need for holding control info.
	 *
	 * XXX this calculation, alignment, and the allocation of the 
	 * diskports are wrong.  The aligned size of the svcport_t
	 * should be used, not its actual size.  What can happen is
	 * the svcport may not be the correct size to produce natural
	 * alignment for the elements in the diskport.  I will fix
	 * this soon, but am noting it here if you wonder why the
	 * stupid 'fill' member is in svcport_t.
	 */
	int _extra = sizeof(svcport_t) + sizeof(diskport_t)*open_max;
	if (_extra & 0x7)	/* XXX align */
		_extra += 8 - (_extra & 0x7);
	const int extra = _extra;	// to void gcc vfork warning

	/*
	 *  Allocate the shared memory segment
	 */
	e = shmem_seg.create(size + extra);
	if (e)  {
		cerr << "Cannot create shared memory:" << endl << e << endl;
		return e;
	}

	/*
	 *  Initialize server port and disk ports in the shared memory
	 *  segment allocated.
	 */

	svcport = new (shmem_seg.base()) svcport_t(open_max);

	diskport = (diskport_t*) (shmem_seg.base() + sizeof(svcport_t));
	for (unsigned i = 0; i < open_max; i++)
		new (diskport + i) diskport_t(i);

	svcport->sleep = 1;

#ifndef _WIN32
	/*
	 *  Fork a dummy diskrw process that monitors longevity of
	 *  this process.
	 */
	char arg[20];	/* shared memory ID */
	ostrstream s(arg, sizeof(arg));
	s << shmem_seg.id() << ends;

	char arg2[20];	/* offset of svcport */
	ostrstream s2(arg2, sizeof(arg2));
	s2 << (char*)svcport - (char*)shmem_seg.base()  << ends;

	const char *av[5];
	int	ac = 0;

	av[ac++] = _diskrw;
	av[ac++] = arg;
	av[ac++] = arg2;
	av[ac++] = 0;


	_dummy_disk = ::vfork();
	if (_dummy_disk == -1) {
		w_rc_t e = RC(fcOS);
		cerr << "vfork for diskrw:" << endl << e << endl;
		W_IGNORE( shmem_seg.destroy() );
		W_COERCE(e);
	}
	else if (_dummy_disk == 0) {
		execvp(av[0], EXECVP_AV(av));
		w_rc_t e = RC(fcOS);
		cerr << "could not start disk I/O process " << _diskrw 
		     << ":" << endl << e << endl;
		kill(getppid(), SIGKILL);
		W_IGNORE( shmem_seg.destroy() );
		::_exit(1);
	}
#else
	_dummy_disk = 0;
#endif

	/*
	 *  start of user space in the buffer (above control info space).
	 */
	_iomem = shmem_seg.base() + extra;

#if defined(PURIFY_ZERO)
	/* To prevent "UMR/UMC" from the buffer pool, because the 
	   disk r/w threads are setting that data, NOT the
	   SM process.  */
	memset(_iomem, '\0', size);
#endif

	return RCOK;
}


void	*sdisk_handler_t::ioBuffer()
{
	return	_iomem;
}


sdisk_handler_t::sdisk_handler_t(const char *diskrw)
: _diskrw(diskrw),
  _dummy_disk(0),
  _iomem(0),
  diskport(0),
  diskport_cnt(0),
  svcport(0),
  open_max(default_open_max),
  chan_event(DISKRW_FD_NONE, *this)
{
	master_chan[0] = master_chan[1] = DISKRW_FD_NONE;

	const char *s = getenv("SDISK_DISKRW_OPEN_MAX");
	if (s && *s) {
		int n = atoi(s);
		if (n > 0)
			open_max = (unsigned)n;
	}
}


sdisk_handler_t::~sdisk_handler_t()
{
	W_COERCE(shutdown());
}


w_rc_t	sdisk_handler_t::make(unsigned size, const char *diskrw, 
			      sdisk_handler_t *&hp)
{
	sdisk_handler_t	*h;
	w_rc_t		e;

	hp = 0;

	h = new sdisk_handler_t(diskrw);
	if (!h)
		return RC(fcOUTOFMEMORY);

	e = h->init_shared(size);
	if (e != RCOK) {
		delete h;
		return e;
	}

	hp = h;
	return RCOK;
}


w_rc_t	sdisk_handler_t::shutdown()
{
	return finish_shared();
}


void	sdisk_handler_t::set_diskrw_name(const char *diskrw)
{
	_diskrw = diskrw;
}


bool	sdisk_handler_t::pending()
{
	return svcport ? svcport->peek_incoming_counter() != 0 : false;
}


/*
 *  Poll all open disk ports to check for I/O acknowledgement.
 */

void	sdisk_handler_t::polldisk()
{
	if (!svcport)
		return;

	unsigned count = 0;

	for (unsigned i = 0; i < open_max; i++) {
		if (diskport[i].pid && diskport[i].pid != -1)  {
			while (diskport[i].sthread_recv_ready()) {
				/*
				 * An I/O has completed. Dequeue the ack 
				 * and unblock the thread that initiated 
				 * the I/O.
				 */
				diskmsg_t m;
				diskport[i].sthread_recv(m);
				W_COERCE( m.thread->unblock() );
				++count;
			}
		}
	}

	svcport->decr_incoming_counter(count);
}


void	sdisk_handler_t::polldisk_check()
{
	if (!svcport)
		return;

	w_assert1(svcport->sleep);
	polldisk();
}


void	sdisk_handler_t::ensure_sleep()
{
	if (svcport)
		svcport->sleep = 1;
}


w_rc_t sdisk_handler_t::open(const char *path, int flags, int mode,
			     unsigned &slot)
{
	if (!shmem_seg.base())
		return RC(stBADFD);	/* XXX not ready */

#ifdef _WIN32
	CommandLineArgs* cmdArgs = new CommandLineArgs(8);
	if (!cmdArgs)
		return RC(fcOUTOFMEMORY);
#endif

	/*
	 *  Enter critical section in manipulating diskport
	 */
	W_COERCE( forkdisk.acquire() );

	if (diskport_cnt >= unsigned(open_max)) {
		forkdisk.release();
#ifdef _WIN32
		delete cmdArgs;
#endif
		return RC(stTOOMANYOPEN);
	}

	/*
	 *  Find a free diskport and instantiate an object on it
	 */
	diskport_t * volatile p;
	for (p = diskport; p->pid != 0; p++)
		;
	new (p) diskport_t(p - diskport);
	diskport_cnt++;

	/*
	 *  Set up arguments for diskrw
	 */

	char arg1[100];
	ostrstream s1(arg1, sizeof(arg1));
	s1 << shmem_seg.id() << ends;

	char arg2[20];
	ostrstream s2(arg2, sizeof(arg2));
	s2 << (char*)svcport - (char*)shmem_seg.base()  << ends;

	char arg3[20];
	ostrstream s3(arg3, sizeof(arg3));
	s3 << (char*)p - (char*)shmem_seg.base() << ends;

	char arg4[20];
	ostrstream s4(arg4, sizeof(arg4));
	/* diskrw process takes unix flags for now */
	s4 << sdisk_unix_t::convert_flags(flags) << ends;

	char arg5[20];
	ostrstream s5(arg5, sizeof(arg5));
	s5 << mode << ends;


#ifdef _WIN32
	p->_args = cmdArgs; // keep a ptr around for cleanup in ~diskport_t

	*cmdArgs += _diskrw;
	*cmdArgs += arg1;
	*cmdArgs += arg2;
	*cmdArgs += arg3;
	*cmdArgs += arg4;
	*cmdArgs += arg5;
	*cmdArgs += path;
#else
	const	char *av[8];
	int	ac = 0;
	av[ac++] = _diskrw;
	av[ac++] = arg1;
	av[ac++] = arg2;
	av[ac++] = arg3;
	av[ac++] = arg4;
	av[ac++] = arg5;
	av[ac++] = path;
	av[ac++] = 0;
#endif /* _WINDOWS */

	/* XXX why not initialize this with the handler? */
	if (master_chan[0] == DISKRW_FD_NONE) {
		if (pipe(master_chan) == -1) {
			w_rc_t e = RC(fcOS);
			cerr << "pipe() for master_chan:" << endl << e << endl;
#ifdef notyet
			p->~diskport_t();
			forkdisk.release();
			return e;
#else
			W_COERCE(e);
#endif
		}

		/* CLOSE_ON_EXEC(master_chan[PIPE_IN]);  */
		FD_NODELAY(master_chan[PIPE_IN]);
		W_COERCE(chan_event.change(master_chan[PIPE_IN]));
		W_COERCE(sthread_t::io_start(chan_event));
		chan_event.enable();
	}
	p->sv_chan[PIPE_IN] = master_chan[PIPE_IN];
	p->sv_chan[PIPE_OUT] = master_chan[PIPE_OUT];

	if (pipe(p->rw_chan) == -1) {
		w_rc_t e = RC(fcOS);
		cerr << "pipe() for diskrw_chan:" << endl << e << endl;
#ifdef notyet
		p->~diskport_t();
		forkdisk.release();
		return e;
#else
		W_COERCE(e);
#endif
	}

	/* CLOSE_ON_EXEC(p->rw_chan[PIPE_OUT]); */
	    

#ifdef _WINDOWS
	HANDLE diskrwHandle;
#ifdef _MT
	unsigned pid;
	diskrwHandle = (HANDLE) _beginthreadex(0, 0, 
		DiskRWMain, (void*) cmdArgs, 
		THREAD_TERMINATE | THREAD_SUSPEND_RESUME | THREAD_SET_INFORMATION,
		&pid);
#else
	DWORD pid;
	diskrwHandle = CreateThread(0, 0, 
		DiskRWMain, (void*) cmdArgs, 
		THREAD_TERMINATE | THREAD_SUSPEND_RESUME | THREAD_SET_INFORMATION,
		&pid);
#endif

	if (diskrwHandle == 0) {
		w_rc_t e = RC(fcWIN32);
		delete cmdArgs;
		cerr << "CreateThread:" << e << endl;
		W_COERCE(e);
	}
	p->_thread = diskrwHandle;
	DBGTHRD(<< "Forked DiskRWMain, got handle " 
		<< (int)(p->_thread)
		<< (int) pid
		);
#else

	/*
	 *  Fork 
	 */

	pid_t pid = ::vfork();
	if (pid == -1)  {
		// failure
		w_rc_t e = RC(fcOS);
		cerr << "can't vfork:" << endl << e << endl;
		W_COERCE(e);
	}
	else if (pid == 0) {
		/*
		 *  Child is running here. Exec diskrw.
		 */
		execvp(av[0], EXECVP_AV(av));
		w_rc_t e = RC(fcOS);
		cerr << "could not start disk I/O process, " << _diskrw
			<< ":" << endl << e << endl;
		kill(getppid(), SIGKILL);
		::_exit(1);
	}
#endif /* _WIN32 */

	/*
	 *  Parent is running here. 
	 */
#if defined(_WIN32) && defined(NEW_IO)
	/* nothing to do, see comment for _WIN32. */
#elif defined(_WIN32)
	// NT: share address space && fds - don't do this until close.
	// ::shutdown(diskport[i].rw_chan[PIPE_OUT], SD_SEND);
	// ::closesocket(p->rw_chan[PIPE_IN]);
#else
	::close(p->rw_chan[PIPE_IN]);
#endif
	// *p is in shared memory: we'll clobber diskrw's
	// idea of the fd if we do this:
	// p->rw_chan[PIPE_IN] = DISKRW_FD_NONE;


	p->pid = pid;

	forkdisk.release();

	slot = p - diskport;

	return RCOK;
}



/*
 *  Shutdown the diskrw process for this open file and release
 *  the resources in shared memory used for it.
 */

w_rc_t sdisk_handler_t::close(unsigned slot)
{
	if (slot >= unsigned(open_max))
		return RC(stBADFD);

	diskport_t* p = diskport + slot;

#if 0
	/* XXX belongs in sdisk layer ?? */
	if (p->_openflags & (O_RDWR | O_APPEND | O_WRONLY)) {
		// sync before close
		W_DO(io(slot, diskmsg_t::t_sync, 0, 0));
	}
#endif

#ifdef _WIN32
	// tell the diskrw to shut down cleanly
	W_DO(io(slot, diskmsg_t::t_close, 0, 0));
#endif


#ifndef _WIN32
	/*
	 *  Kill diskrw and wait for it to end.
	 */
	w_assert3(p->outstanding() == 0);
	if (kill(p->pid, SIGHUP) == -1) {
		w_rc_t e = RC(fcOS);
		cerr << "cannot kill pid " << p->pid << endl
			<< e << endl;
		W_COERCE(e);
	}
	if (waitpid(p->pid, 0, 0) == -1) {
		w_rc_t e = RC(fcOS);
		cerr << "cannot waitpid " << p->pid << endl
			<< e << endl;
		W_COERCE(e);
	}
#else
	/* TODO await diskrw thread finish */
	w_assert3(p->rw_chan[PIPE_IN] != DISKRW_FD_NONE);
	w_assert3(p->rw_chan[PIPE_OUT] != DISKRW_FD_NONE);

	DBGTHRD( << "TODO: await diskrw to finish, handle = " 
		<< (int)(p->_thread) 
		<< (int)(p->pid) 
		);

	/* XXX Error not recovered */
	WaitForSingleObject(p->_thread, INFINITE);
#endif

#if defined(_WIN32) && defined(NEW_IO)
	closepipe(p->rw_chan[PIPE_IN]); 
#elif defined(_WIN32)
	::shutdown(p->rw_chan[PIPE_IN], SD_SEND);
        ::closesocket(p->rw_chan[PIPE_IN]);
	::shutdown(p->rw_chan[PIPE_OUT], SD_SEND);
	::closesocket(p->rw_chan[PIPE_OUT]);
#else
	::close(p->rw_chan[PIPE_OUT]);
#endif
	p->rw_chan[PIPE_OUT] = DISKRW_FD_NONE;

	/*
	 *  Call destructor manually.
	 */
	p->diskport_t::~diskport_t();

	W_COERCE(forkdisk.acquire());
	diskport_cnt--;

	if (diskport_cnt == 0) {
		DBGTHRD(<<"NO DISKRW RUNNING ");
		// no diskrw running

		W_COERCE(sthread_t::io_finish(chan_event));
		W_COERCE(chan_event.change(DISKRW_FD_NONE));

#if defined(_WIN32) && defined(NEW_IO)
		closepipe(master_chan[PIPE_IN]);
#elif defined(_WIN32)
		listener.close();
		::shutdown(master_chan[PIPE_IN], SD_SEND);
		::closesocket(master_chan[PIPE_IN]);
#else
		::close(master_chan[PIPE_IN]);
#endif
		master_chan[PIPE_IN] = DISKRW_FD_NONE;

#if defined(_WIN32) && defined(NEW_IO)
		/* only 1 thing to close */
#elif defined(_WIN32)
		::shutdown(master_chan[PIPE_OUT], SD_SEND);
		::closesocket(master_chan[PIPE_OUT]);
#else
		::close(master_chan[PIPE_OUT]);
#endif
		master_chan[PIPE_OUT] = DISKRW_FD_NONE;
	}
	forkdisk.release();

	return RCOK;
}


/*
 * Perform an I/O using the diskrw mechanism to avoid blocking
 * the CPU thread we are running on.
 *
 * The "file descriptor" is the actual file descriptor index,
 * not the externally-visible "psuedo-fd".
 */

w_rc_t sdisk_handler_t::io(unsigned slot,
			   int op,
			   const iovec_t *v, int iovcnt,
			   fileoff_t other)
{
	diskport_t	&p = diskport[slot];
	int		total = 0;
	diskv_t		diskv[iovec_max];
	fileoff_t	pos = p.pos;
	fileoff_t	io_pos = pos;
	bool		pio = false;
	w_rc_t		e;

	DBGTHRD(<<"diskrw _io: " << (diskmsg_t::op_t)op << " slot " << slot );

	switch (op) {
	case diskmsg_t::t_trunc:
		io_pos = other;
		/*FALLTHROUGH*/

	case diskmsg_t::t_close:
	case diskmsg_t::t_sync:
		break;

	case diskmsg_t::t_pread:
	case diskmsg_t::t_pwrite:
		io_pos = other;
		op = (op == diskmsg_t::t_pread) 
			? diskmsg_t::t_read  : diskmsg_t::t_write;
		pio = true;
		/*FALLTHROUGH*/

	case diskmsg_t::t_read:
	case diskmsg_t::t_write:
		e = map_iovec(v, iovcnt, diskv, total);
		if (e)
			return e;
		break;

	default:
		return RC(stBADADDR);
		break;
	}

	SthreadStats.diskrw_io++;

#ifdef EXPENSIVE_STATS
	stime_t start_time(stime_t::now());
#endif

	/*
	 *  Send message to diskrw process to perform the I/O. Current
	 *  thread will block in p.send() until I/O completes.
	 */


	p.sthread_send(diskmsg_t((diskmsg_t::op_t)op, 
				 io_pos,
				 diskv, iovcnt));

#ifdef EXPENSIVE_STATS
	SthreadStats.io_time += (float) (stime_t::now() - start_time);
#endif

	/* XXX If a file is truncated, and the seek pointer is
	   in the truncated region, it remains inside the truncated
	   region after the truncate.   It should be moved to the
	   end-of-file.  */
	if (!pio)
		p.pos = pos + total;

	DBGTHRD(<<" diskrw op= " << op << " done"  );

	return RCOK;
}


w_rc_t	sdisk_handler_t::seek(unsigned slot, fileoff_t offset, int whence,   
			      fileoff_t &ret)
{
	if (slot > unsigned(open_max))
		return RC(stBADFD);	/* XXX */

	switch (whence) {
	case sdisk_t::SEEK_AT_SET:
		diskport[slot].pos = offset;
		break;
	case sdisk_t::SEEK_AT_CUR:
		diskport[slot].pos += offset;
		break;
	case sdisk_t::SEEK_AT_END:
		// cannot support SEEK_AT_END yet; We don't know 
		// how large the file is, and don't track appends.
		cerr << "sdisk_diskrw_t::seek(): SEEK_AT_END not supported"
			<< endl;
		/*FALLTHROUGH*/
	default:
		return RC(stINVAL);
	}

	ret = diskport[slot].pos;
	return RCOK;
}


/*
 * Convert pointers from local memory iovecs to
 * to offset relative to shared memory origin.
 */

w_rc_t	sdisk_handler_t::map_iovec(const iovec_t *iov, int iovcnt,
				   diskv_t *diskv, int &total)
{
	total = 0;

	for (int i = 0; i < iovcnt; i++)  {
		diskv[i].bfoff = ((char *)iov[i].iov_base) - shmem_seg.base();
		diskv[i].nbytes = iov[i].iov_len;
		total += iov[i].iov_len;

		if (diskv[i].bfoff < 0
			|| (uint)(diskv[i].bfoff + diskv[i].nbytes) > shmem_seg.size())   {
#ifdef W_DEBUG
			cerr 
			<<" iov[i].iov_base=" << (int)(iov[i].iov_base)
			<<" iov[i].iov_len=" << (int)(iov[i].iov_len)
			<< endl
			<<" diskv[i].bfoff=" << diskv[i].bfoff
			<<" diskv[i].nbytes=" << diskv[i].nbytes
			<< endl
			<<" shmem_seg.base()=" << (int)(shmem_seg.base())
			<<" shmem_seg.size()=" << shmem_seg.size()
			<<" shmem_seg.last_byte=" 
			<< (((int) shmem_seg.base()) + shmem_seg.size() - 1)
			<< endl;
#endif
			return RC(stBADADDR);
		}
	}
	return RCOK;
}


ostream	&sdisk_handler_t::print(ostream &o)
{
	o << "DUMP DISKRW" << endl;
	o << "open_max=" << open_max
		<< ", diskport_cnt=" << diskport_cnt << endl;

	if (!shmem_seg.base()) {
		o << "NO shared segment" << endl;
		return o;
	}
	svcport_t	&sp = *(svcport_t *) shmem_seg.base();
	o << "svcport_t:";
	if (sp.sleep)
		o << " [sleep]";
	o << " incoming=" << sp.incoming;
	o << " master_chan = {" << master_chan[0]
		<< ", " << master_chan[1] << "}";
	o << endl;

	unsigned	i;

	for (i = 0; i < unsigned(open_max); i++) {
		diskport_t	&p = diskport[i];

		if (p.pid == 0)
			continue;
		if (p.pid == -1) {
			o << "diskrw[" << i << "]: open local" << endl;
			continue;
		}
		/* Name not used incase it isn't labelled */
		o << "diskrw[" << i << "]:";
		if (p.sleep)
			o << " [sleep]";
		o << " pos=" << p.pos;
		o << " pid=" << p.pid;
		o << " rw_chan = { " << p.rw_chan[0]
			<< ", " << p.rw_chan[1] << "}";
		o << " sv_chan = { " << p.sv_chan[0]
			<< ", " << p.sv_chan[1] << "}";
		o << endl;

		o << "	req=" << p._requested
			<< " comp=" << p._completed
			<< " wake=" << p._awakened
			<< endl;

		o << "	q[toDISKRW]:";
		o << " outs=" << p.queue[0]._outstanding;
		o << " head=" << p.queue[0].head
			<< " tail=" << p.queue[0].tail
			<< " cnt=" << p.queue[0].cnt << endl;
		o << "	q[fromDISKRW]:";
		o << " outs=" << p.queue[1]._outstanding;
		o << " head=" << p.queue[1].head
			<< " tail=" << p.queue[1].tail
			<< " cnt=" << p.queue[1].cnt << endl;
	}

	return o;
}


sdisk_diskrw_t::sdisk_diskrw_t(sdisk_handler_t &h)
: handler(h),
  slot(-1)
{
}


sdisk_diskrw_t::~sdisk_diskrw_t()
{
	if (slot >= 0)
		W_COERCE(close());
}


w_rc_t	sdisk_diskrw_t::make(sdisk_handler_t &handler,
			     const char *path, int flags, int mode,
			     sdisk_t *&sdisk)
{
	sdisk_diskrw_t	*sd;
	w_rc_t		e;

	sdisk = 0;

	sd = new sdisk_diskrw_t(handler);
	if (!sd)
		return RC(fcOUTOFMEMORY);

	e = sd->open(path, flags, mode);
	if (e != RCOK) {
		delete sd;
		return e;
	}

	sdisk = sd;
	return RCOK;
}


w_rc_t	sdisk_diskrw_t::open(const char *path, int flags, int mode)
{
	w_rc_t	e;

	if (slot >= 0)
		return RC(stBADFD);		/* XXX already open */

#ifdef SDISK_DISKRW_HACK
	e = sdisk_unix_t::open(path, flags, mode);
	if (e != RCOK)
		return e;
	if (!hasOption(flags, OPEN_KEEP_HACK))
		W_COERCE(sdisk_unix_t::close());
#else
	int	fd;
	fd = ::open(path, sdisk_unix_t::convert_flags(flags), mode);
	if (fd == -1)
		return RC(stOS);
	::close(fd);
#endif

	flags &= ~OPEN_CREATE;

	unsigned aslot;
	e = handler.open(path, flags, mode, aslot);
	if (e == RCOK)
		slot = aslot;

#ifdef SDISK_DISKRW_HACK
	if (e != RCOK && hasOption(flags, OPEN_KEEP_HACK))
		W_COERCE(sdisk_unix_t::close());
#endif
	return e;
}


w_rc_t	sdisk_diskrw_t::close()
{
	if (slot < 0)
		return RC(stBADFD);
	
	w_rc_t e = handler.close(slot);

	if (e == RCOK) {
		slot = -1;
#ifdef SDISK_DISKRW_HACK
		W_IGNORE(sdisk_unix_t::close());
#endif
	}

	return e;
}


/*
 *  Perform an I/O of type "op" on the file "fd" previously
 *  opened with sthread_t::open(). 
 *
 *  This translates from externally visible file descriptors, etc
 *  to the choice of internal I/O operation, and keeps track of
 *  common statistics.
 */

w_rc_t sdisk_diskrw_t::io(int op,
			  const iovec_t *iov, int iovcnt,
			  fileoff_t other)
{
	/* This does the open/not open checking for everything
	   except seek. */
	if (slot == -1)
		return RC(stBADFD);	/* XXX */

	return handler.io(slot, op, iov, iovcnt, other);
}


/* XXXX bytes-written not implemented yet, diskrw still fatals
   on I/O errors */

w_rc_t sdisk_diskrw_t::read(void* buf, int n, int &done)
{
	iovec_t iov(buf, n);
	done = n;	/* XXX system crashes if it doesn't */
	return io(diskmsg_t::t_read, &iov, 1);
}

w_rc_t sdisk_diskrw_t::write(const void* buf, int n, int &done)
{
	iovec_t iov((void *)buf, n);
	done = n;	/* XXX system crashes if it doesn't */
	return io(diskmsg_t::t_write, &iov, 1);
}

w_rc_t sdisk_diskrw_t::readv(const iovec_t *iov, int iovcnt, int &done)
{
	done = vsize(iov, iovcnt);	/* XXX system crashes if it doesn't */
	return io(diskmsg_t::t_read, iov, iovcnt);
}

w_rc_t sdisk_diskrw_t::writev(const iovec_t *iov, int iovcnt, int &done)
{
	done = vsize(iov, iovcnt);	/* XXX system crashes if it doesn't */
	return io(diskmsg_t::t_write, iov, iovcnt);
}

w_rc_t	sdisk_diskrw_t::pread(void *buf, int n, fileoff_t pos, int &done)
{
	iovec_t	iov(buf, n);
	done = n;	/* XXX system crashes if it doesn't */
	return io(diskmsg_t::t_pread, &iov, 1, pos);
}

w_rc_t	sdisk_diskrw_t::pwrite(const void *buf, int n, fileoff_t pos, int &done)
{
	iovec_t	iov((void *)buf, n);
	done = n;	/* XXX system crashes if it doesn't */
	return io(diskmsg_t::t_pwrite, &iov, 1, pos);
}


w_rc_t	sdisk_diskrw_t::sync()
{
	return io(diskmsg_t::t_sync, 0, 0);
}

w_rc_t	sdisk_diskrw_t::truncate(fileoff_t n)
{
	return io(diskmsg_t::t_trunc, 0, 0, n);
}


w_rc_t sdisk_diskrw_t::seek(fileoff_t offset, int whence, fileoff_t& ret)
{
	if (slot < 0)
		return RC(stBADFD);

	return handler.seek(slot, offset, whence, ret);
}

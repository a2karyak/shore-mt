/*<std-header orig-src='shore' incl-file-exclusion='DISKRW_H'>

 $Id: diskrw.h,v 1.67 1999/06/10 23:32:30 bolo Exp $

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

#ifndef DISKRW_H
#define DISKRW_H

#include "w_defines.h"

/*  -- do not edit anything above this line --   </std-header>*/

// define this to volatile if the compiler supports it
#define VOLATILE 


#include <unix_error.h>

#if defined(_WIN32) && defined(NEW_IO)
#define	DISKRW_FD_NONE	INVALID_HANDLE_VALUE
#else
#define	DISKRW_FD_NONE	-1
#endif

typedef	sdisk_t::fileoff_t	diskrw_off_t;

/* 
 * Magic numbers used to verify that the server and diskrw agree
 * on the communication protocol and layout of shared memory.
 * Increment the last part of the magic number if you make
 * any change to the size or shape of any of the data structures:
 */
enum { diskrw_magic = 0x7adbda13 + sizeof(diskrw_off_t) };

class sthread_t;

#include "shmc_stats.h"

const int open_max = 32;
const int max_diskv = 8;

struct diskv_t {
    long	bfoff;
    int		nbytes;
};


/*
   A message between the thread package and I/O processes which
   communicate I/O requests and completions.
 */

#ifndef SIGALRM
#ifndef _WINDOWS
#error SIGALRM should be defined for this architecture
#endif
#define SIGALRM  1
#define SIGUSR1  2
#endif

struct diskmsg_t {
    enum op_t {
	t_inval,
	t_read, t_write,
	t_sync, t_trunc,
	t_close,
	t_pread, t_pwrite,
	t_last
    };
    op_t	op;
    sthread_t*	thread;
    diskrw_off_t	off;
    int		dv_cnt;
    diskv_t	diskv[max_diskv];

    diskmsg_t()	: op(t_inval), thread(0), off(0), dv_cnt(0) {}
    diskmsg_t& operator=(const diskmsg_t& m)  {
	op = m.op;
	thread = m.thread;
	off = m.off;
	dv_cnt = m.dv_cnt;
	if (dv_cnt == 1) 
	    diskv[0] = m.diskv[0];
	else 
	    memcpy(diskv, m.diskv, sizeof(diskv_t) * dv_cnt);
	return *this;
    }

#if defined(SDISK_DISKRW_C)
    diskmsg_t(op_t o, diskrw_off_t offset, const diskv_t* v, int cnt) 
	: op(o), thread(sthread_t::me()), off(offset), dv_cnt(cnt)
    {
	w_assert3(cnt>=0 && cnt <= max_diskv);
	memcpy(diskv, v, sizeof(diskv_t) * cnt);
    }
	
    diskmsg_t(op_t o, diskrw_off_t offset, long bfoffset, int n)
    : op(o), thread(sthread_t::me()), off(offset), dv_cnt(1)
    {
	diskv[0].bfoff = bfoffset;
	diskv[0].nbytes = n;
    }
#endif /* IO_C */
};


extern ostream &operator<<(ostream &o, const diskmsg_t::op_t op);


#if (defined(SDISK_DISKRW_C) || (defined(DISKRW_C) && !defined(_WIN32)))
ostream &operator<<(ostream &o, const diskmsg_t::op_t op)
{
	/* XXX dependency upon enum */
	static const char *names[] = {
		"t_inval",
		"t_read", "t_write", "t_sync", "t_trunc", "t_close",
		"t_pread", "t_pwrite",
		"t_last"

	};

	if (op < 0  ||  op > diskmsg_t::t_last)
		o << "<unknown diskmsg_t::op_t " << (int)op << ">";
	else
		o << names[(int)op];

	return o;	
}
#endif


/*
   A circular message queue is used to pass I/O requests
   between a I/O process and the thread package.

   The message queue synchronizes access with other processes
   (and threads) via a spinlock.

   XXX The '_outstanding' variable is redundant with regards to the
   'cnt' of items in the message queue (should be fixed).

   XXX Placing and removing messages from the queue is disparate
   from verifying queue consistency.  It requires that the user of
   the message queue lock the queue's lock.  This is a potential hazard
   which could be fixed.  
 */  

class msgq_t {
    friend class sdisk_handler_t;

public:
    VOLATILE int		_outstanding;
    VOLATILE spinlock_t	lock;
private:
    enum { qsize = 12 };
    diskmsg_t	msg[qsize];
    int 	head, tail, cnt;
public:
    msgq_t() : _outstanding(0), head(0), tail(0), cnt(0) {};
    ~msgq_t()	{};

    int outstanding() const  { return _outstanding; }

    void put(const diskmsg_t& m, bool getlock=true) {
	w_assert1(cnt < qsize);
	if(getlock) {
	    ACQUIRE(lock);	
	}
	msg[tail]= m, ++cnt;
	if (++tail >= qsize) tail = 0;
	++_outstanding; // not used
	if(getlock) {
	    lock.release();
	}
    }
    void get(diskmsg_t& m,bool getlock=true) {
	if(getlock) {
	    ACQUIRE(lock); // SPUN in server recving reply
	}
	w_assert3(cnt > 0);
	m = msg[head], --cnt;
	if (++head >= qsize) head = 0;
	--_outstanding;
	if(getlock) {
	    lock.release();
	}
    }
    bool is_empty()	{ return cnt == 0; }
    bool is_full()	{ return cnt == qsize; }
};


/*
   A service port is another shared memory data structure. 
   It is multiplexed between all i/o processes, and tells
   the thread package if any I/O compoletions are outstanding.

   In addition, it attempts to retain sanity by verifying that
   the I/O process and the thread package both have the same
   version of the shared-memory data structures compiled in.
 */

class svcport_t {
    friend class sdisk_handler_t;

private:
    VOLATILE int		incoming;	// disk op reply pending
    VOLATILE spinlock_t	lock;

public:
    VOLATILE int		sleep;		// server sleep

private:
    // NOTE: be sure to increment diskrw_magic if you change this
    //       data structure.
    enum	{ magic = diskrw_magic };
    const int	_magic;

public:
    bool check_magic()	{ return magic == _magic; }
    svcport_t() : incoming(0), sleep(0), _magic(magic) {
	    if(!check_magic()) {
		cerr << 
		"Configurations of diskrw and server do not match." 
		<< endl;
	    }
	};

#ifdef DISKRW_C
    void 	incr_incoming_counter()  {
	ACQUIRE(lock);
	++incoming;
	lock.release();
    }
#endif /* DISKRW_C */

#if defined(SDISK_DISKRW_C)
    int		peek_incoming_counter() {
	return incoming;
    }
    void	decr_incoming_counter(int /*count*/) {
	ACQUIRE(lock);
#if 1  /*** uncomment 'count' above if this is set to false (0) ***/
	    // avoid choking on unavoidable race condition 
        incoming = 0;
#else
        incoming -= count;
        w_assert3(incoming >= 0);
#endif
	lock.release();
    }
#endif /* IO_C */

};

#ifdef _WINDOWS
class CommandLineArgs {
public:
	// CommandLineArgs(CString cmdline, int nargs);
	CommandLineArgs(int nargs) : argc(nargs), _filled(0) {
	    argv = new char *[argc];
	    for(int i=0; i < argc; i++) {
	        argv[i] = 0;
            }
	};

	CommandLineArgs& operator+=(const char *c) {
	    w_assert3(_filled < argc);
	    int len = strlen(c);
	    char *tmp = new char[len+1];
	    w_assert1(tmp);
	    strncpy(tmp, c, len);
	    tmp[len]='\0';
	    argv[_filled++] = tmp;
            return *this;
	};

	~CommandLineArgs() {
	    for (int i = 0; i < _filled; i++)
		delete [] argv[i];
	    delete [] argv;
	}
	int argc;
	int _filled;
	char** argv;

        friend ostream& operator<<(ostream &o, const CommandLineArgs&c);
};
inline
ostream& operator<<(ostream &o, const CommandLineArgs&c) {
    for (int i = 0; i < c._filled; i++) {
	o << c.argv[i] << " ";
    }
    return o;
}
#endif /* _WINDOWS */

typedef	char	ChanToken;
enum { PIPE_IN = 0, PIPE_OUT = 1 };


/*
   The disk port is used by the thread package and the I/O process
   to communicate their I/O requests and completions.

   It uses two message queues, one for thread to I/O requests,
   and a second for I/O to thread completions.
 */

class diskport_t {
    friend class sdisk_handler_t;

private:
    // NOTE: be sure to increment diskrw_magic if you change this
    //       data structure.
    enum { magic = diskrw_magic };
    enum { DISKRW_OUT = 1, DISKRW_IN = 0 };
    enum { STHREAD_OUT = 0, STHREAD_IN = 1 };

    VOLATILE msgq_t	queue[2];
    VOLATILE int	sleep;
public:
    diskrw_off_t	pos;
    pid_t	pid;

#ifdef _WINDOWS
    HANDLE	_thread; // thread to await on close
    CommandLineArgs *_args;
#endif
    int		_openflags; // _WINDOWS: can't fsync a file opened RDONLY

#if defined(_WIN32) && defined(NEW_IO)
    HANDLE	rw_chan[2];	/* channel to notify diskrw on 		*/
    HANDLE	sv_chan[2];	/* channel to notify server on 		*/
#else
    int		rw_chan[2];	/* channel to notify diskrw on 		*/
    int		sv_chan[2];	/* channel to notify server on 		*/
#endif

    const int	_magic; /* magic should be last so that data structure
				   size differences are detected 	*/
    int	        _fd;    /* for server's fastpath                        */

    unsigned	_requested;	/* requests queued by a thread */
    unsigned	_completed;	/* operation completed by a diskrw */
    unsigned	_awakened;	/* request threads wakened from sleep */
    char	name[20];	/* label for debug output, blocking */

    diskport_t(int id = -1) : sleep(0), pos(0), pid(0), 
#ifdef _WINDOWS
	 _thread(0),
	 _args(0),
#endif
	 _openflags(0),
	_magic(magic),
	_fd(-1),
	_requested(0),
	_completed(0),
	_awakened(0) {
		/* diskrw[-1] means it wasn't labelled. */
		ostrstream s(name, sizeof(name));
		s << "diskrw[" << id << "]" << ends;
    };

    ~diskport_t()	{
	pos = 0, pid = 0, sleep = 0;

#ifdef _WINDOWS
	delete _args;
#endif
	rw_chan[0] = rw_chan[1] = DISKRW_FD_NONE;
	sv_chan[0] = sv_chan[1] = DISKRW_FD_NONE;
    }

    int outstanding() const  { 
		return queue[0].outstanding()+queue[1].outstanding();
	}

    int diskrw_recv(diskmsg_t& m) {
#ifdef _WINDOWS
			    return _recv(DISKRW_IN, m);
#else
			    if (_recv(DISKRW_IN, m) < 0) ::exit(1);
			    return 0;
#endif /* ! _WINDOWS */
			}

    void diskrw_send(const diskmsg_t& m) {
			    queue[DISKRW_OUT].put(m);
			    _completed++;
			}

    void sthread_send(const diskmsg_t& m) {
			    _send(STHREAD_OUT, m);
			}

    void sthread_recv(diskmsg_t& m) {
			    queue[STHREAD_IN].get(m);
			}

    int diskrw_send_ready() { return !queue[DISKRW_OUT].is_full(); }
    int diskrw_recv_ready() { return !queue[DISKRW_IN].is_empty(); }
    int sthread_send_ready() { return !queue[STHREAD_OUT].is_full(); }
    int sthread_recv_ready() { return !queue[STHREAD_IN].is_empty(); }

    bool check_magic()	{ return magic == _magic; }

private:
    void _send(int _q, const diskmsg_t& m);
    int  _recv(int _q, diskmsg_t& m);
};

#if defined(DISKRW_C)
int
diskport_t::_recv(int _q, diskmsg_t& m)
{
    ACQUIRE(queue[_q].lock);		// SPUN in diskrw recving request

    sleep = 1;  // may sleep, set set this now to avoid race condition
    if (queue[_q].is_empty())  {
	queue[_q].lock.release();
	SET_SHMCSTATS(lastsig, 0);
	ChanToken token = 0;
	int	n;
	void clean_shutdown(int);
	while (queue[_q].is_empty()) {
		errno = 0;
		w_assert3(rw_chan[PIPE_IN] != DISKRW_FD_NONE);
#if defined(_WIN32) && defined(NEW_IO)
		n = WaitForSingleObject(rw_chan[PIPE_IN], INFINITE);
		switch (n) {
		case WAIT_TIMEOUT:	/* XXX ??? */
			continue;
		case WAIT_OBJECT_0:	/* success */
			break;
		case WAIT_ABANDONED:	/* no more writers */
			clean_shutdown(1);
			break;
		default:
			w_rc_t e = RC(fcWIN32);
			cerr << "diskrw: - read channel token:" << endl
				<< e << endl;
			return -1;
		}
#else
#ifdef _WINDOWS
		n = recv(rw_chan[PIPE_IN], (char *)&token, sizeof(token), 0);
#else
		n = read(rw_chan[PIPE_IN], (char *)&token, sizeof(token));
#endif
		if (n == -1 && errno == EINTR)	/* intr syscall */
			continue;
		else if (n == 0) 	/* no more writers */
			clean_shutdown(1);
		else if (n != sizeof(token)) {
			w_rc_t e;
#ifdef _WINDOWS
			e  = RC(fcWIN32);
#else
			e  = RC(fcOS);
#endif
			cerr << "diskrw: - read channel token"
			<< "additional info: n = " << n
			<< " fd = " << rw_chan[PIPE_IN] 
			<< ":" << endl << e << endl;
			return -1;
			// was exit(1);
		}
#endif
		INC_SHMCSTATS(notify)
		SET_SHMCSTATS(lastsig, SIGUSR1) // pretend
	}

#ifndef EXCLUDE_SHMCSTATS
	switch(SHMCSTATS(lastsig)){
	case SIGALRM: INC_SHMCSTATS(falarm) break;
	case SIGUSR1: INC_SHMCSTATS(fnotify) break;
	case 0: INC_SHMCSTATS(found) break;
	default:
		w_assert3(0);
	}
#endif /* EXCLUDE_SHMCSTATS */

	ACQUIRE(queue[_q].lock);
    }
    sleep = 0;
    queue[_q].get(m,false);
    queue[_q].lock.release();
    return 0;
}
#endif /* !IO_C */

#if defined(SDISK_DISKRW_C)
void 
diskport_t::_send(int _q, const diskmsg_t& m)
{
    ACQUIRE(queue[_q].lock);
    while (queue[_q].is_full())  {
	queue[_q].lock.release();
	sthread_t::yield();
	ACQUIRE(queue[_q].lock);
    }
    _requested++;
    queue[_q].put(m,false);
    int kick = sleep;
    queue[_q].lock.release();
    if (kick)  {
	INC_SHMCSTATS(kicks);

        int n=0;
	w_assert3(rw_chan[PIPE_OUT] != DISKRW_FD_NONE);
#if defined(_WIN32) && defined(NEW_IO)
	n = SetEvent(rw_chan[PIPE_OUT]);
	if (!n) {
		w_rc_t e = RC(fcWIN32);
		cerr << "sthread: write channel token:" << endl
			<< e << endl;
		::exit(-1);
	}
#else
	ChanToken token = 0;
#ifdef _WINDOWS
	n = ::send(rw_chan[PIPE_OUT], (char *)&token, sizeof(token), 0);
#else
	n = ::write(rw_chan[PIPE_OUT], (char *)&token, sizeof(token));
#endif
	if(n != sizeof(token)) {
		w_rc_t e;
#ifdef _WINDOWS
		e  = RC(fcWIN32);
#else
		e  = RC(fcOS);
#endif
		cerr << "sthread: write channel token additional info: n = "
			<< n << " fd = " << rw_chan[PIPE_OUT]
			<< ":" << endl << e << endl;
		::exit(-1);
	}
#endif
    }
    
    sthread_t::priority_t oldp = sthread_t::me()->priority();
    W_COERCE( sthread_t::me()->set_priority(sthread_t::t_time_critical) );
    
    int befores = SthreadStats.selects;

    SthreadStats.iowaits++;

    W_COERCE( sthread_t::block(sthread_t::WAIT_FOREVER, 0, name) );
    _awakened++;

    int	afters = SthreadStats.selects;

    switch(afters-befores) {
	case 0: INC_STH_STATS(zero); break;
	case 1: INC_STH_STATS(one); break;
	case 2: INC_STH_STATS(two); break;
	case 3: INC_STH_STATS(three); break;
	default:
		if(afters>befores) {
		    INC_STH_STATS(more);
		} else {
		    INC_STH_STATS(wrapped);
		}
		break;
    }
    
    W_COERCE( sthread_t::me()->set_priority(oldp) );
}
#endif /* !DISKRW_C */


#ifdef _WINDOWS
#ifdef _MT
unsigned
#else
DWORD
#endif
__stdcall DiskRWMain(void* data);

#else /* !_WINDOWS */

#ifdef DISKRW_C
#ifndef EXCLUDE_SHMCSTATS
const char *shmc_stats::stat_names[1]; // not used by diskrw
#endif
#endif /* DISKRW_C */

#if !(defined(HPUX8) && defined(_INCLUDE_XOPEN_SOURCE))
	extern "C" {
		int fsync(int);
		int ftruncate(int, off_t);
	}
#endif /* ! HPUX8 ... */

#endif /* !_WINDOWS */

/*<std-footer incl-file-exclusion='DISKRW_H'>  -- do not edit anything below this line -- */

#endif          /*</std-footer>*/

/*<std-header orig-src='shore'>

 $Id: diskrw.cpp,v 1.119 2000/02/22 21:51:58 bolo Exp $

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

#define DISKRW_C

#include <w_debug.h>

#ifdef _WINDOWS
#undef DBGTHRD
#define DBGTHRD(arg) DBG(<< "th."<< GetCurrentThreadId() << " " arg)
#else
/* Can't use DBGTHRD or DBG in separate-process configuration */
#undef DBGTHRD
#define DBGTHRD(arg) DBG(arg)
#endif

#include <stdio.h>
#include <w_stream.h>
#include <w_strstream.h>
#include <w.h>
#include <w_statistics.h>
#include <w_signal.h>
#include <stdlib.h>

#ifndef _WINDOWS
#include <unistd.h>
#endif /* !_WINDOWS */
#include <sys/stat.h>

#include <string.h>
#include <w_rusage.h>

#ifdef _WINDOWS
#include <io.h>
#include <process.h>
#define	getpid()	_getpid()
#endif


#if	!defined(_SC_OPEN_MAX) && defined(RLIMIT_NOFILE)
extern "C"	int getrlimit(int, struct rlimit *);
#endif

#ifndef RLIMIT_NOFILE
extern "C" int getdtablesize();
#endif

#if defined(_WIN32)
struct iovec {
	void	*iov_base;
	int	iov_len;
};
#elif !defined(AIX41) && !defined(SOLARIS2) && !defined(OSF1)
extern "C" {
	extern int writev(int, const struct iovec *, int);
	extern int readv(int, const struct iovec *, int);
}
#endif

#include <w_base.h>
#include <w_shmem.h>

#include "sdisk.h"

#include "spin.h"
#include "diskrw.h"
#include "st_error_enum_gen.h"
#include "diskstats.h"

#ifndef _WIN32
#include <sys/uio.h>
#endif
#include <fcntl.h>


#include <os_interface.h>

#ifndef O_ACCMODE
#define O_ACCMODE (O_RDONLY|O_WRONLY|O_RDWR)
#endif

#ifndef O_BINARY
#define	O_BINARY	0
#endif


enum {
	stOS = fcOS
};

/*
 *  Local variables
 *
 *	fname		: file name of device or unix file
 *	shmem_seg	: shared memory segment
 *	ppid		: parent (server) process id
 *	kill_siblings	: true if process should kill all siblings
 *			  when it dies (for the monitor process).
 *	fflags		: open flags for file
 *
 *  and, if DISKSTATSDIR is set:
 *	path		: file name in which to write disk stats
 *                        (determined by environment vbl DISKSTATSDIR and
 *                        file name in fname)
 *	statfd		: file descriptor for stats file
 *	u		: unix_stats structure
 *	s		: system call counts structure
 *	smode		: create-mode for stats file
 */

#ifdef _WINDOWS
w_shmem_t*  shmem_seg;
#else
w_shmem_t   _shmem_seg;
w_shmem_t*  shmem_seg=&_shmem_seg;
#endif

pid_t 		ppid;
bool 		kill_siblings = 0;
/* XXX these are not thread safe in the diskrw win32 version */
int 		statfd = -1;
const char	*path = 0;
int 		smode = 0755;
unsigned	open_max = 0;



/*********************************************************************
 *
 *  Function declarations
 *
 *********************************************************************/
void 		caught_signal(int sig);
void 		setup_signal();
void 		writestats(bool closeit=false);



/*********************************************************************
 *
 *  fatal(line)
 *
 *  Print fatal error message and exit.
 *
 *********************************************************************/
void
fatal(int line)
{

#ifdef _WINDOWS
    W_FORM2(cerr,("diskrw: pid=%d, threadid=%d: fatal error at line %d\n", 
	getpid(),  GetCurrentThreadId(), line));
#ifdef W_DEBUG
	// NB: maybe this should be TerminateProcess?
	DBGTHRD( << "FATAL ERROR: _endthreadex(-1)" );
#ifdef _MT
	_endthreadex(-1);
#else
	ExitThread(-1);
#endif
#else
	cerr << "FATAL ERROR: ExitProcess(-1)" <<endl;
	W_FATAL(fcINTERNAL);
	ExitProcess(-1);
#endif
#else


	cerr << "diskrw: pid=" << getpid()
		<< " fatal error at line " << line
		<<  endl;

	kill(ppid, SIGKILL);
	::_exit(1);
#endif /* !_WINDOWS */
    
}



/*********************************************************************
 *
 *  is_raw_device(devname)
 *
 *  Return true if "devname" is a raw device. False otherwise.
 *
 *********************************************************************/
bool
is_raw_device(const char* devname)
{
    bool ret = false;
    os_stat_t	statInfo;
    if (os_stat(devname, &statInfo) < 0) {
	w_rc_t	e = RC(fcOS);
	cerr << "diskrw: cannot stat \"" << devname << "\":" << endl
		<< e << endl;
    } else {
	// if it's a character device, its a raw disk
	ret = ((statInfo.st_mode & S_IFMT) == S_IFCHR);
    }
    return ret;
}

/*********************************************************************
 *
 *  get_disk_info(devname)
 *
 *  get disk block info for "devname"  -- only used for
 *  raw devices
 *
 *********************************************************************/

#ifdef PREFETCH
#include "sdisk.h"

struct  diskinfo_t {
    long nblk;	    // # sectors total
    long blksz;     // sector size
    long nsect; // # sectors per track
    long tracksize; // in bytes
    long lastfulltrack; // in bytes
};

static	diskinfo_t D;
int	sys_pagesize;
int	tracksize_in_pages;

w_rc_t
get_disk_info(const char *devname, diskinfo_t &d)
{
	int fr;
	fr  = ::os_open(devname, O_RDONLY | O_BINARY, 0);

	w_rc_t	e;
	struct	disk_geometry	dg;

	e = sdisk_get_geometry(fr, dg);
	if (e) {
		DBGTHRD(<<"closing fd " << fr );
		close(fr);
		return e;
	}

	d.nsect = dg.sectors;
	d.nblk = dg.blocks;
	d.blksz = dg.block_size;
	d.tracksize = d.nsect * d.blksz; 

	if (tracksize_in_pages > 0)
		d.tracksize = sys_pagesize * tracksize_in_pages;

	d.lastfulltrack = (d.nblk * d.blksz) / d.tracksize;

	/*
	   cerr << "raw device has " <<  d.nblk 
	   << " blocks of size " << d.blksz
	   << " nsect is "  << d.nsect
	   << " tracksize is "  << d.tracksize
	   << " lastfulltrack is "  << d.lastfulltrack
	   <<endl;
	   */

	DBGTHRD(<<"closing fd " << fr );
	os_close(fr);

	return RCOK;
}
#endif

/*********************************************************************
 *
 *  struct wbufdatum_t
 *
 *  A request to write "len" bytes of memory at "ptr" to "offset"
 *  on disk.
 *
 *********************************************************************/
struct wbufdatum_t {
    diskrw_off_t 		offset;	
    char* 			ptr;
    int 			len;

    NORET			wbufdatum_t()
    	: offset(0), ptr(0), len(0)	{};
    NORET			wbufdatum_t(diskrw_off_t off, char* p, int l)
	: offset(off), ptr(p), len(l)   {};

};


/*********************************************************************
 * 
 *  class writebuf_t
 *
 *********************************************************************/
class writebuf_t {
public:
    NORET			writebuf_t() : offset(-1), sz(0)   {};
    NORET			~writebuf_t();

    int 			deposit(const wbufdatum_t& d, int f = 0);
    void 			withdraw_all(wbufdatum_t& d);
    void 			withdrawal_done();
protected:
    enum { 
	bufsz = 64 * 1024
    };
    diskrw_off_t 		offset;
    char 			buf[bufsz];
    int  			sz;
};




/*********************************************************************
 *
 *  writebuf_t::~writebuf_t()
 *
 *  Destructor. Check that bank is empty.
 *
 *********************************************************************/
NORET
writebuf_t::~writebuf_t()
{
    if (sz > 0)  {
	cerr << "writebuf_t::~writebuf_t  :- not empty" << endl;
    } if (sz < 0) {
	w_assert3(sz == -1);
	cerr << "writebuf_t::~writebuf_t  :- withdrawal not done"
	     << endl;
    }
}



/*********************************************************************
 *
 *  writebuf_t::withdraw_all(d)
 *
 *  Withdraw all deposits so far. 
 *
 *********************************************************************/
inline void
writebuf_t::withdraw_all(wbufdatum_t& d)  
{
    w_assert3(sz >= 0);
    d.offset = offset;
    d.ptr = buf;
    d.len = sz;
    sz = -1;
    offset = -1;
}




/*********************************************************************
 *
 *  writebuf_t::withdrawal_done()
 *
 *  Signal that all bytes withdrawn are spent (written to disk).
 *
 *********************************************************************/
inline void
writebuf_t::withdrawal_done()  
{ 
    w_assert3(sz == -1);
    sz = 0;
    offset = -1;
}



/*********************************************************************
 *
 *  writebuf_t::deposit(d, print_error)
 *
 *  Deposit the request "d" into writebuf. "Print_error" indicates
 *  whether to print out an error message if the operation fails
 *  (for debugging).
 *
 *********************************************************************/
int
writebuf_t::deposit(const wbufdatum_t& d, int print_error)
{
    /*
     *  Check if in middle of a withdrawal.
     */
    if (sz == -1) {
	if (print_error)  cerr << "diskrw: withdrawal not done" << endl;
	return -1;	// withdrawal not done
    }

    /*
     *  Check if deposited bytes are not contiguous with 
     *  current holdings.
     */
    w_assert3(sz >= 0);
    if (offset >= 0 && offset + sz != d.offset) {
	if (print_error)  cerr << "diskrw: discontinuity" << endl;
	return -1;	// discontinuity
    }

    /*
     *  Check if buffer will go bust with new deposits.
     */
    if (d.len > bufsz - sz)  {
	if (print_error)  cerr << "diskrw: overflow" << endl;
	return -1; // overflow
    }

    /*
     *  If this is the first deposit (after a full withdrawal) ...
     */
    if (offset < 0)  {
	w_assert3(offset == -1);
	w_assert3(sz == 0);
	offset = d.offset;
    }

    /*
     *  Copy from d 
     */
    memcpy(buf + sz, d.ptr, d.len);
    sz += d.len;

    return 0;
}


/*********************************************************************
 *
 *  Local variable
 *
 *********************************************************************/
static writebuf_t* wbuf = 0;


/*********************************************************************
 *
 *  flush_wbuf(fd)
 *
 *********************************************************************/
void
flush_wbuf(int fd)
{
    wbufdatum_t d;
    wbuf->withdraw_all(d);
    if (d.len)  {
	if (d.len < 0)  fatal(__LINE__);
	if (os_lseek(fd, d.offset, SEEK_SET) == -1) {
	    w_rc_t e = RC(fcOS);
	    cerr << "diskrw: lseek(" << d.offset << "):" 
		<< endl << e << endl;
	    fatal(__LINE__);
	}
	if (os_write(fd, d.ptr, d.len) != d.len)  {
	    w_rc_t e = RC(fcOS);
	    cerr << "diskrw: write(" << d.len << " @ " << d.len << "):"
		<< endl << e << endl;
	    fatal(__LINE__);
	}
	s.writes++;
	s.bwritten += d.len;
    }

    wbuf->withdrawal_done();
}

/*********************************************************************
 *
 *  main(argc, argv) / DiskRWMain(data)
 *
 *  Usage: diskrw shmid [args path]
 *
 *  If [args path] is not supplied, diskrw functions as a clean up 
 *  agent of the shared memory segment. Otherwise, diskrw is 
 *  responsible for I/O to path.
 *
 *********************************************************************/


#ifdef _WINDOWS
    w_shmem_t* global_shmem = 0;
#ifdef _MT
    unsigned
#else
    DWORD
#endif
	__stdcall DiskRWMain(void* data)
#else
int    main(int argc, char* argv[])
#endif /* !_WINDOWS */

{
    void clean_shutdown(int);
    bool do_clean_shutdown = false;

#ifdef _WINDOWS
    shmem_seg = global_shmem;

    CommandLineArgs* cmdline = (CommandLineArgs*) data;
    int argc = cmdline->argc;
    char** argv = cmdline->argv;

#endif  /* _WINDOWS */

    int		i;
    int		mode=0;
    svcport_t	*sp = 0;
    diskport_t	*dp = 0;
    bool	raw_disk = false;
    int		fflags = 0;
    const char	*fname = "<noname-diskrw>";

#if defined(DEBUG_DISKRW)
    cerr << "diskrw: pid=" << getpid() << ':';
    for (i = 0; i < argc; i ++)
	cerr << ' ' << argv[i];
    cerr << endl;
#endif /* DEBUG_DISKRW */


#ifdef _WIN32
    ppid = getpid();
#else
    ppid = getppid();
    if (argc != 7 && argc != 3) {
	cerr << "usage: " << argv[0]
	     << " shmid svcport [diskport fflags mode path]"
	     << endl;
	fatal(__LINE__);
    }
#endif /* !_WINDOWS */

    /*
     *  Attach to shared memory segment.
     */
    const char* arg1 = argv[1];
    istrstream(VCPP_BUG_1 arg1) >> i;

#ifndef _WINDOWS
#define HANDLE int
#endif /* _WINDOWS */

    w_rc_t e = shmem_seg->attach((HANDLE)i);
    if (e)  {
	cerr << "diskrw:- problem with shared memory" << endl;
	cerr << e;
	fatal(__LINE__);
    }

    i = atoi(argv[2]);
    sp = (svcport_t*) (shmem_seg->base() + i);
    open_max = sp->open_max;

    /*
     *  If this is a regular diskrw process, set up pointers to
     *  svcport and diskport.
     */
    if (argc > 3) {
	i = atoi(argv[3]);
	dp = (diskport_t*) (shmem_seg->base() + i);

	if( ! (sp->check_magic() && dp->check_magic()) ) {
	    cerr << 
	    "diskrw:- configurations of diskrw and server do not match" 
	    << endl;
	    fatal(__LINE__);
	}

	fflags = atoi(argv[4]);
	mode = atoi(argv[5]);
    }

    /*
     *  Set up signals/alarm
     */
#ifndef _WINDOWS
    setup_signal();
    alarm(60);
#endif /* !_WINDOWS */

    if (argc == 3)  {
	/*
	 *  This is the dummy diskrw-- the one that cleans up 
	 *  the shared memory. Loop forever.
	 */
	kill_siblings = 1;

#ifndef _WINDOWS
	while (1)  pause();
	exit(0);

#else /* _WINDOWS */
#ifdef _MT
	_endthreadex(0);
#else
	ExitThread(0);
#endif
#endif /* !_WINDOWS */
    }

#ifndef _WINDOWS
    /* close the pipe ends that we don't use */
    DBGTHRD(<<"closing fd " << dp->rw_chan[PIPE_OUT]);
     ::close(dp->rw_chan[PIPE_OUT]);
     // *dp is in shared memory: don't clobber fd!
     // dp->rw_chan[PIPE_OUT] = DISKRW_FD_NONE;

    DBGTHRD(<<"closing fd " << dp->sv_chan[PIPE_IN]);
     ::close(dp->sv_chan[PIPE_IN]);
     // *dp is in shared memory: don't clobber fd!
     // dp->sv_chan[PIPE_IN] = DISKRW_FD_NONE;
#endif /* !_WINDOWS */

    /*
     * Close all file descriptors up to the limit. 
     */
    int fd;
    {
	int maxfds;

#ifdef _WINDOWS
	maxfds = 20; // ????
#else

#if defined(_SC_OPEN_MAX)
	maxfds = (int) sysconf(_SC_OPEN_MAX);
#elif defined(RLIMIT_NOFILE)
	struct rlimit buf;
	if(getrlimit(RLIMIT_NOFILE, &buf)<0) {
		w_rc_t e = RC(fcOS);
		cerr << "getrlimit(RLIMIT_NOFILE) fails:" << endl << e << endl;
	}
	maxfds = buf.rlim_cur; // SOFT limit
#else  
	maxfds = getdtablesize();
#endif

#endif /* _WINDOWS */

#ifdef notdef
	// could also try:
#    if defined(NOFILE)
	maxfds = NOFILE;
#    endif
#endif /* notdef */

#ifndef _WINDOWS
	for (fd = 3; fd < maxfds; fd++)  {
	    if (fd == dp->rw_chan[PIPE_IN] || fd == dp->sv_chan[PIPE_OUT])
		continue;
	    DBGTHRD(<<"closing fd " << fd);
	    (void)::close(fd);
	}
#endif /* !_WINDOWS */
    }
    
    /*
     *  Open the file given by the path argument.
     */
    fname = argv[6];


    /* Always do binary I/O */
    fflags |= O_BINARY;

    fd = os_open(fname, fflags, mode);
    if (fd < 0) {
	w_rc_t e = RC(fcOS);
	W_FORM2(cerr,("diskrw: open(%s):\n", fname));
	cerr << e << endl;
	fatal(__LINE__);
    }
    DBGTHRD(<<"open " << fname 
	<< " flags " << fflags
	<< " mode " << mode
	<< " returns fd= " << fd);

    /*
     * Grab advisory lock 
     * TODO: equiv for WINDOWS
     */
#if defined(SOLARIS2) 
    /* 
     * Unfortunately, using the lockf() library function,
     * we're stuck with write locks in all cases, and 
     * we can't get a write lock on a file opened for read,
     * believe it or not...  But for now, this is ok
     * because we happen never to open files read-only without 
     * also * opening them for write.
     */
    if((fd >= 0) && (fflags != O_RDONLY)) {
	int is_fatal=0;
	int e = os_lockf(fd, F_TLOCK, 0);
	if(e<0) {
	    w_rc_t e1 = RC(fcOS);
	    if( errno == EBADF) {
		/* try fstat - see what it says */
		os_stat_t	bf;
		e=os_fstat(fd, &bf);
		if ((bf.st_mode & S_IFMT) == S_IFREG) {
			cerr << "REG FILE" <<endl;
			is_fatal++;
		} else if ((bf.st_mode & S_IFMT) == S_IFCHR) {
			cerr << "CHR SPECIAL" <<endl;
			is_fatal++;
		} else if ((bf.st_mode & S_IFMT) == S_IFBLK) {
			cerr << "BLK SPECIAL" <<endl;
			is_fatal++;
		}
		cerr << "diskrw: lockf(fd=" << fd << "):" << endl << e1 << endl;
	    } else  if(errno == EAGAIN) {
		cerr << "***********************************************"
		    <<endl;
		cerr << "diskrw: file "
			<< fname
			<< " is locked." <<endl;
		cerr << "Is the server already running?" <<endl
			<< endl;
		is_fatal++;
	    }
	    if(is_fatal) {
		fatal(__LINE__);
	    }
	}
    }
#endif /* SOLARIS2 */

    /*
     *  If file is raw, then set up writebuf to cache writes
     *  for performance.
     */

    if (is_raw_device(fname)) {

	wbuf = new writebuf_t;
	if (! wbuf)  {
	    w_rc_t e = RC(fcOUTOFMEMORY);
	    // we will make do without the buffer
	    W_FORM2(cerr, ("diskrw: %s: Warning: no write buffer:\n", fname));
	    cerr << e << endl;
	}

	raw_disk = true;
    }
    /*
     *   If we are monitoring performance of diskrw processes,
     *   get the directory name in which to write the stats
     */

    {
	const char *c = getenv("DISKSTATSDIR");
	if(c) {
	    // const char *fname = "./volumes/dev1";
	    const char *z = strrchr(fname,'/');
	    
	    if(!z) {
	     	z = fname;
	    }
	    char *b = new char[strlen(c) + strlen(z) + 1];
	    if(b) {
		strcpy(b,c);
		strcat(b,z);
		path = b;
	    } // else skip writing stats
	} // else skip writing stats

	// open & read it into s, u
	if(path) {
	    statfd = os_open(path, O_WRONLY | O_CREAT | O_BINARY, smode);
	    if(statfd<0) {
		w_rc_t e = RC(fcOS);
		cerr << "open(" << path << ") fails:" << endl << e << endl;
		fatal(__LINE__);
	    }
	}
	u.start();
    }


    /*
     *  Loop forever and process disk I/O request.
     */
    while ( 1 )  {
	
	/*
	 *  Dequeue a message
	 */
	diskmsg_t m;
        if (dp->diskrw_recv(m) == -1) {
            // delete dshmem_seg;
	    DBGTHRD(<<"err in diskrw_recv: calling _endthreadex()");
	    /*
	     * normal (non-fatal) return
	     */
#ifdef _WINDOWS
#ifdef _MT
	    _endthreadex(0);
#else
	    ExitThread(0);
#endif
#else
            return 0;
#endif
        }
	diskv_t* diskv = m.diskv;

	/*
	 *  Convert shared memory offsets back to addresses
	 */
	struct	iovec iov[8];	// XXX magic number
	int	iovcnt;
	int	totalcc;
	int	cc;
	iovcnt = 0;
	totalcc = 0;
	switch (m.op) {
	case m.t_read:
	case m.t_write:
		iovcnt = m.dv_cnt;
		for (i = 0; i < m.dv_cnt; i++) {
		    w_assert3((uint)diskv->bfoff < shmem_seg->size());
		    iov[i].iov_base = shmem_seg->base() + diskv[i].bfoff;
		    iov[i].iov_len = diskv[i].nbytes;
		    totalcc += iov[i].iov_len;
		}
		break;
	default:
		break;
	}

	/*
	 *  Process message
	 */
	DBGTHRD(<<"processing " << int(m.op) << " on fd " << fd );

	switch (m.op)  {
	case m.t_trunc:
	    /* the wbuf contents might overlap the truncated area */	
	    if (wbuf)
		flush_wbuf(fd);
	
	    s.ftruncs++;
	    if (os_ftruncate(fd, m.off) == -1) {
		w_rc_t e = RC(fcOS);
		cerr << "diskrw: " << fname << ": ftruncate("
			<< fd << ", " << m.off << "):" << endl
			<< e << endl;
		cerr << e << endl;
		fatal(__LINE__);
	    } 
	    break;

	case m.t_close:
	    if (wbuf)
		flush_wbuf(fd);

	    if (os_close(fd) == -1) {
		w_rc_t e = RC(fcOS);
		W_FORM2(cerr,("diskrw: %s: close(%d):\n", fname, fd));
		cerr << e << endl;
		fatal(__LINE__);
	    }
	    do_clean_shutdown = true;
	    break;

	case m.t_sync:
	    w_assert3(m.dv_cnt == 0);
	    if (wbuf) flush_wbuf(fd);

#if defined(AIX32) || defined(AIX41)
	    /* AIX doesn't allow fsync on devices.  To be precise,
	     it doesn't allow it on character devices.  I haven't
	     checked block devices --bolo */
	    if (raw_disk) {
		s.fsyncs++;
		break;
	    }
#endif	    

	    /* don't fsync() RO files */
	    if ((fflags & O_ACCMODE) == O_RDONLY) {
		s.fsyncs++;
		break;
	    }
		
	    if (os_fsync(fd) == -1) {
		w_rc_t e = RC(fcOS);
		cerr << "diskrw: " << fname << ": fsync(" << fd
			<< "), mode=" << fflags << ":"
			<< endl << e << endl;
		fatal(__LINE__);
	    } 
	    s.fsyncs++;
	    break;

	case m.t_read:
	    w_assert3(m.dv_cnt > 0);
	    if (wbuf) flush_wbuf(fd);
	    {
		bool ok = false;

		if(!ok) {
		    if (os_lseek(fd, m.off, SEEK_SET) == -1)  {
			w_rc_t e = RC(fcOS);
			cerr << "diskrw: " << fname << ": lseek("
				<< m.off << "):" << endl
				<< e << endl;
			fatal(__LINE__);
		    }
#if !defined(_WIN32)
		    /* the real thing */
		    cc = os_readv(fd, iov, iovcnt);
		    if (cc != totalcc) {
			w_rc_t e = RC(fcOS);
			cerr << "diskrw: " << fname << ": read("
				<< totalcc << " @ " << m.off
				<< ")==" << cc << ":" << endl
				<< e << endl;
			fatal(__LINE__);
		    }
		    s.bread += totalcc;
#else
		    /* XXX left for debugging */
		    for (i = 0; i < iovcnt; i++) {
			cc = os_read(fd, iov[i].iov_base, iov[i].iov_len);
			if (cc != iov[i].iov_len) {
			    w_rc_t e = RC(fcOS);
			    cerr << "diskrw: " << fname << ": read("
				<< iov[i].iov_len << " @ " << m.off
				<< ")==" << cc << ":" << endl
				<< e << endl;
			    fatal(__LINE__);
			}
			s.bread += iov[i].iov_len;
		    }
#endif
		    s.reads++;
		}
	    }
	    break;

	case m.t_write:

	    if (wbuf && iovcnt > 1)  {
		diskrw_off_t off = m.off;
		for (i = 0; i < iovcnt; i++) {
		     wbufdatum_t d(off, (char*)iov[i].iov_base, iov[i].iov_len);
		
		     if (wbuf->deposit(d))  {
		        flush_wbuf(fd);
		        if (wbuf->deposit(d)) {
			    /* it's probably too big; just do it */
			    if (os_lseek(fd, d.offset, SEEK_SET) == -1)  {
				w_rc_t e = RC(fcOS);
			        cerr << "diskrw: " << fname << ": lseek("
					<< d.offset << "):" << endl
					<< e << endl;
				fatal(__LINE__);
			    }
			    
			    cc = os_write(fd, d.ptr, d.len);
			    if (cc != d.len) {
				w_rc_t e = RC(fcOS);
				cerr << "diskrw: " << fname << ": write("
					<< d.len << " @ " << d.offset
					<< ")==" << cc << ":" << endl
					<< e << endl;
				fatal(__LINE__);
			    }
		        }
		    }
		    off += iov[i].iov_len;
		}
	    } else {

		if (wbuf) flush_wbuf(fd);
	    
		if (os_lseek(fd, m.off, SEEK_SET) == -1)  {
		    w_rc_t e = RC(fcOS);
		    cerr << "diskrw: " << fname << ": lseek("
			<< m.off << "):" << endl
			<< e << endl;
		    fatal(__LINE__);
		}
#if !defined(_WIN32)
		/* do the real thing */
		cc = os_writev(fd, iov, iovcnt);
		if (cc != totalcc) {
			w_rc_t e = RC(fcOS);
			cerr << "diskrw: " << fname << ": write("
				<< totalcc << " @ " << m.off
				<< ")==" << cc << ":" << endl
				<< e << endl;
			fatal(__LINE__);
		}
		s.bwritten += totalcc;
#else
		for (i = 0; i < iovcnt; i++) {
		    cc = os_write(fd, iov[i].iov_base, iov[i].iov_len);
		    if (cc != iov[i].iov_len) {
			cerr << "diskrw: " << fname << ": write("
				<< iov[i].iov_len << " @ " << m.off
				<< ")==" << cc << ":" << endl
				<< e << endl;
			fatal(__LINE__);
		    }
		    s.bwritten += iov[i].iov_len;
	        }
#endif
		s.writes++;
	    }
	    break;
	default:
	    W_FORM2(cerr,("diskrw: %s: bad disk message op=%d\n", fname, m.op));
	    fatal(__LINE__);
	}

	DBGTHRD(<<"sending response for op " << int(m.op) << " on fd " << fd );

	/*
	 *  Begin atomic.
	 */
	dp->diskrw_send(m);  // BOLO: switch these two statements!??
	sp->incr_incoming_counter(); 
	/*
	 *  End atomic.
	 */


	if (sp->sleep)  {
	    DBGTHRD(<<"kicking server for " << int(m.op) << " on fd " << fd);

	    INC_SHMCSTATS(kicks)
	    /*
	     *  Kick server
	     */

#if defined(_WIN32) && defined(NEW_IO)
	    BOOL ok;
	    ok = SetEvent(dp->sv_chan[PIPE_OUT]);
	    if (!ok) {
		w_rc_t	e = RC(fcWIN32);
		cerr << "diskrw: " << fname << ": write token:" 
			<< endl << e << endl;
		fatal(__LINE__);
	    }
#else
	    ChanToken	token = 0;
	    int		n;
#ifdef _WINDOWS
	    n =::send(dp->sv_chan[PIPE_OUT], (char *)&token, sizeof(token), 0);
#else
	    n =::write(dp->sv_chan[PIPE_OUT], (char *)&token, sizeof(token));
#endif
	    if (n != sizeof(token))
		{
		w_rc_t e = RC(fcOS);
		W_FORM2(cerr, ("diskrw: %s: write token:\n", fname));
		cerr << e << endl;
		fatal(__LINE__);
	    }
#endif /* !_WINDOWS */
	}
        if(do_clean_shutdown) {
	    clean_shutdown(2);
        }
    }

    fatal(__LINE__);

#ifdef _WINDOWS
#ifdef _MT
    _endthreadex(0);
#else
    ExitThread(0);
#endif
#endif /* _WINDOWS */
    return 0;
}



/*********************************************************************
 *
 *  clean_shutdown(int)
 *
 *********************************************************************/

void 
clean_shutdown(int DBG_ONLY(why))
{

    DBGTHRD(<<"clean_shutdown, reason=" << why
	<< " kill_siblings=" << kill_siblings
    );

    if (kill_siblings)  {
	// presumably siblings destroy their
	// shared memory segments
	diskport_t* dp = (diskport_t*) (shmem_seg->base() +
					sizeof(svcport_t));
	for (unsigned i = 0; i < open_max; i++, dp++)  {
	    if (dp->pid && dp->pid != -1)  {
#ifdef W_DEBUG
		cerr << "diskrw (pid="
			<< getpid()
			<< ") killing sibling process " << dp->pid
			<<endl;
#endif /* W_DEBUG */
#ifdef _WINDOWS
		DBGTHRD(<<"unconditionally terminate the process " 
			<< dp->pid );
		/* unconditionally terminate the process */
		TerminateProcess((HANDLE)dp->pid, 0);
#else
		kill(dp->pid, SIGTERM);
#endif /* !_WINDOWS */
	    }
	}
    }

    w_rc_t e = shmem_seg->destroy();

    if (e)  {
	cerr << "diskrw:- problem with shared memroy" << endl;
	cerr << e;
    }

#ifndef _WINDOWS
    // clean up all the server's left-over shm segments
    //
    FILE *res;
    bool failed=false;
    res = popen("ipcs -m -p | grep \"^m\"","r");
    if(res) {
	int n=7, id;
	pid_t owner;
	int last;
	char perm[20]; 
	char person[40];
	char group[40];
	char key[40];

	while(n==7) {
#if defined(Linux) || defined(__NetBSD__) || defined(OSF1)	/* pid_t is a int */
	    n = fscanf(res, "m %d %s %s %s %s %d %d\n",
		&id, key, perm,  person, group, &owner, &last);
#else
	    n = fscanf(res, "m %d %s %s %s %s %ld %d\n",
		&id, key, perm,  person, group, &owner, &last);
#endif

	    if(n == 7) {
		if(owner == ppid) {
		    cerr << "Removing segment " << id 
			<< " created by " << owner << endl;

		    if(shmctl(id,IPC_RMID,0) < 0) {
			w_rc_t e = RC(fcOS);
			cerr << "shmctl(" << id << ", IPC_RMID) fails:"
				<< endl << e << endl;
			failed = true;
		    }
		}
#if defined(DEBUG_DISKRW)
		else
		    cerr << "Ignoring segment " << id 
			<< " created by " << owner << endl;
#endif /* DEBUG_DISKRW */
	    } else if(n!=EOF) {
		failed = true;
	    }
	}
	pclose(res);
    } else {
	failed = true;
    }
    if(failed) {
	cerr << "diskrw: Cannot clean up server's shared memory segments."
	<<endl
	<< "diskrw: You might have to do so by hand, with ipcs(1) and ipcrm(1)."
	<< endl;
    }
#endif /* !_WINDOWS */

#ifdef DEBUG_DISKRW
    cerr << "diskrw: pid=" << getpid() << ": " << fname << ": normal exit."
	<< endl;
#endif /* DEBUG_DISKRW */

#ifdef _WINDOWS
    DBGTHRD(<<"exiting thread");
    ExitThread(0);
#else
    exit(0);
#endif /* !_WINDOWS */
}


#ifndef _WINDOWS

/*********************************************************************
 *
 *  caught_signal(sig)
 *
 *********************************************************************/
void 
caught_signal(int sig)
{
#ifndef EXCLUDE_SHMCSTATS
    SET_SHMCSTATS(lastsig, sig)
    switch (sig)  {
    case SIGALRM: 
	INC_SHMCSTATS(alarm) break;
    case SIGUSR1:
	INC_SHMCSTATS(notify) break;
    default:
	break;
    }
#endif /* EXCLUDE_SHMCSTATS */
    switch (sig)  {
    case SIGALRM: 
    case SIGINT:
	if (getppid() != ppid || sig == SIGINT)
	    clean_shutdown(3);
	if(statfd > 0) writestats();
	alarm(60);
	break;
    case SIGTERM:
	    if (kill_siblings)
		    clean_shutdown(4);
	    /*FALLTHROUGH*/
    case SIGUSR2: {
	    w_rc_t e = shmem_seg->destroy();
	    if (e) {
		cerr << e << endl;
	    }
	    if (statfd > 0) writestats(true);
#if defined(DEBUG_DISKRW)
	    W_FORM2(cerr, ("diskrw: pid=%d: %s: SIGTERM ination\n", getpid(), fname));
#endif /* DEBUG_DISKRW */
	    exit(-1);
	    break;
    }
    case SIGHUP:
	if(statfd > 0) writestats();

#ifdef DEBUG_DISKRW
	cerr << "diskrw: pid=" << getpid() << ": " << fname << ": SIGHUP."
		<< endl;
#endif
	exit(0);
    case SIGUSR1:
	break;
    }
}
#endif /* !_WINDOWS */


#ifndef _WINDOWS

/*********************************************************************
 *
 *  setup_signal()
 *
 *********************************************************************/
void 
setup_signal()
{
    // allow all signals to be caught
    sigset_t all_mask;
    sigset_t proc_mask;

    sigemptyset(&all_mask);
    sigemptyset(&proc_mask);

    if (sigprocmask(SIG_SETMASK, &proc_mask, NULL) == -1)  {
	W_FATAL(stOS);
    }

#ifdef POSIX_SIGNALS
    // specify specific actions for some signals
    struct sigaction sact;
    sact.sa_handler = W_POSIX_HANDLER caught_signal;
    sact.sa_flags = 0;
#ifdef SA_RESTART
    sact.sa_flags |= SA_RESTART;
#endif
    if (sigaddset(&all_mask, SIGUSR1) == -1)  {
	W_FATAL(stOS);
    }
    if (sigaddset(&all_mask, SIGUSR2) == -1)  {
	W_FATAL(stOS);
    }

    if (sigaddset(&all_mask, SIGTERM) == -1) {
	W_FATAL(stOS);
    }

    if (sigaddset(&all_mask, SIGALRM) == -1)  {
	W_FATAL(stOS);
    }
#if 0
    if (sigaddset(&all_mask, SIGINT) == -1)  {
	W_FATAL(stOS);
    }
#endif

    if (sigaddset(&all_mask, SIGHUP) == -1)  {
	W_FATAL(stOS);
    }

    sact.sa_mask = all_mask;

    if (sigaction(SIGUSR1, &sact, 0) == -1) {
	W_FATAL(stOS);
    }
    if (sigaction(SIGUSR2, &sact, 0) == -1) {
	W_FATAL(stOS);
    }
    if (sigaction(SIGTERM, &sact, 0) == -1) {
	W_FATAL(stOS);
    }
    if (sigaction(SIGHUP, &sact, 0) == -1) {
	W_FATAL(stOS);
    }
#if 0
    if (sigaction(SIGINT, &sact, 0) == -1) { 
	// catch SIGINT from parent
	W_FATAL(stOS);
    }
#endif

    sact.sa_flags = 0;
#ifdef SA_INTERRUPT
    sact.sa_flags |= SA_INTERRUPT;
#endif
    if (sigaction(SIGALRM, &sact, 0) == -1) {
	W_FATAL(stOS);
    }

#if 1
    sact.sa_handler = W_POSIX_HANDLER SIG_IGN;
    sigemptyset(&sact.sa_mask);
    sact.sa_flags = 0;
    sigaction(SIGINT, &sact, 0);
#endif

/*
    sact.sa_flags = 0;
    sact.sa_handler = SIG_IGN;
    sigaction(SIGINT, &sact, 0);
*/
#else
    /* ANSI STANDARD C way -- allows int arg on handler */

#   define SETSIG(x)       {						\
    if (signal(x, W_ANSI_C_HANDLER caught_signal) == 			\
					W_ANSI_C_HANDLER SIG_ERR) {	\
	W_FATAL(stOS);							\
    }									\
}

    SETSIG(SIGUSR1);
    SETSIG(SIGTERM);
    SETSIG(SIGHUP);
    SETSIG(SIGINT);
    SETSIG(SIGALRM);
#endif /* !POSIX_SIGNALS*/

}

#endif /* !_WINDOWS */

void
writestats(bool closeit)
{
    int cc;
    u.stop(1); // iters don't matter

    (void) lseek(statfd, 0, SEEK_SET);
    if((cc = write(statfd, &s, sizeof s)) != sizeof s ) {
	    w_rc_t e = RC(fcOS);
	    cerr << "write stats: s:" << endl << e << endl;
	    return;
    }

    if((cc = write(statfd, &u, sizeof u)) != sizeof u ) {
	    w_rc_t e = RC(fcOS);
	    cerr << "write stats: u:" << endl << e << endl;
	    return;
    }
    if(closeit) {
	DBGTHRD(<<"closing fd " << statfd);
	(void) close(statfd);
    }
}

#if defined(_WINDOWS)&& defined(NOTDEF)

CommandLineArgs::CommandLineArgs(CString cmdline, int nargs)
{
    int idx = -1;
    CString temp;
    int i = 0;
    argv = new char * [nargs];
    argc = nargs;
    while((idx = cmdline.Find(' ')) != -1) {
	temp = cmdline.Left(idx+1);
	argv[i] = new char[temp.GetLength() + 1];
	memcpy(argv[i], temp.GetBuffer(1), temp.GetLength());
	argv[i][temp.GetLength() - 1] = '\0';
	i++;
	cmdline = cmdline.Mid(idx+1);
    }
    argv[i] = new char[cmdline.GetLength() + 1];
    memcpy(argv[i], cmdline.GetBuffer(1), cmdline.GetLength());
    argv[i][cmdline.GetLength()] = '\0';
}

#endif /* _WINDOWS */


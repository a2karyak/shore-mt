/*<std-header orig-src='shore'>

 $Id: thread2.cpp,v 1.52 2003/02/03 16:11:04 bolo Exp $

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

#include <w_stream.h>
#include <os_types.h>
#include <os_fcntl.h>
#ifdef _WIN32
#include <io.h>
#include <process.h>
#else
#include <unistd.h>
#endif
#include <memory.h>

#include <w.h>
#include <w_statistics.h>
#include <sthread.h>
#include <sthread_stats.h>

#include <vtable_info.h>
#include <vtable_enum.h>

#include <getopt.h>

#ifdef _WIN32
#define	IO_DIR	"."
#define getpid() _getpid()
#endif

#ifndef IO_DIR
#define	IO_DIR	"/var/tmp"
#endif


class io_thread_t : public sthread_t {
public:
	io_thread_t(int i, char *bf);

protected:
	void	run();

private:
	int	idx;
	char	*buf;
};


class cpu_thread_t : public sthread_t {
public:
	cpu_thread_t(int i);

protected:
	void	run();

private:
	int	idx;
};



io_thread_t 	**ioThread;
cpu_thread_t	**cpuThread;

int	NumIOs = 100;
int	NumThreads = 5;
int	BufSize = 1024;
int	vec_size = 0;		// i/o vector slots for an i/o operation
bool	local_io = false;
bool	keepopen_io = false;
bool	fastpath_io = false;
bool	raw_io = false;
bool	histograms = false;
bool	verbose = false;

const char* io_dir = IO_DIR;

/* build an i/o vector for an I/O operation, to test write buffer. */
int make_vec(char *buf, int size, int vec_size, 
		sthread_t::iovec_t *vec, const int iovec_max)
{
	int	slot = 0;

	/* Default I/O vector size is the entire i/o */
	if (vec_size == 0)
		vec_size = size;

	while (size > vec_size && slot < iovec_max-1) {
		vec[slot].iov_len = vec_size;
		vec[slot].iov_base = buf;
		buf += vec_size;
		size -= vec_size;
		slot++;
	}
	if (size) {
		vec[slot].iov_len = size;
		vec[slot].iov_base = buf;
		slot++;
	}

	return slot;
}



io_thread_t::io_thread_t(int i, char *bf)
:
  sthread_t(t_regular),
  idx(i),
  buf(bf)
{
	char buf[40];
	ostrstream s(buf, sizeof(buf));

	s << "io[" << idx << "]" << ends;
	rename(buf);
}


void io_thread_t::run()
{
	cout << name() << ": started: " 
		<< NumIOs << " I/Os "
		<< endl;

	char fname[40];
	ostrstream f(fname, sizeof(fname));

	f << io_dir << '/' << "sthread." << getpid() << '.' << idx << ends;
    
	int fd;
	w_rc_t rc;
	int flags = OPEN_RDWR | OPEN_SYNC | OPEN_CREATE;
	if (local_io)
		flags |= OPEN_LOCAL;
	else if (fastpath_io)
		flags |= OPEN_FAST;
	else if (keepopen_io)
		flags |= OPEN_KEEP;
	
	if (raw_io)
		flags |= OPEN_RAW;

	rc = sthread_t::open(fname, flags, 0666, fd);
	if (rc) {
	    cerr << "open " << fname << ":" << endl << rc << endl;
	    W_COERCE(rc);
	}

	int i; 
	for (i = 0; i < NumIOs; i++)  {
		sthread_t::iovec_t	vec[iovec_max];	/*XXX magic number */
		int	cnt;
		for (register int j = 0; j < BufSize; j++)
			buf[j] = (unsigned char) i;

		cnt = make_vec(buf, BufSize, vec_size, vec, iovec_max);

		// cout  << name() << endl ;
		rc = sthread_t::writev(fd, vec, cnt);
		if (rc) {
			cerr << "write:" << endl << rc << endl;
			W_COERCE(rc);
		}
	}

	// cout << name() << ": finished writing" << endl;

	rc = sthread_t::lseek(fd, 0, SEEK_AT_SET);
	if (rc)  {
		cerr << "lseek:" << endl << rc << endl;
		W_COERCE(rc);
	}
    
	// cout << name() << ": finished seeking" << endl;
	for (i = 0; i < NumIOs; i++) {
		rc = sthread_t::read(fd, buf, BufSize);
		if (rc)  {
			cerr << "read:" << endl << rc << endl;
			W_COERCE(rc);
		}
		for (register int j = 0; j < BufSize; j++) {
			if ((unsigned char)buf[j] != (unsigned char)i) {
				cout << name() << ": read bad data";
				W_FORM2(cout,(" (page %d  expected %d got %d\n",
					  i, i, buf[j]));
				W_FATAL(fcINTERNAL);
			}
		}
	}
	// cout << name() << ": finished reading" << endl;
	
	W_COERCE( sthread_t::fsync(fd) );

	W_COERCE( sthread_t::close(fd) );

	// cleanup after ourself
	(void) ::unlink(fname);
}

void print_histograms(ostream &o)
{
	vtable_info_array_t  t;
	if (sthread_t::collect(t)<0)
		o << "THREADS: error" << endl;
	else {

		o << "THREADS" <<endl;
		t.operator<<(o);
	}

	vtable_info_array_t  f;
	if (w_factory_t::collect(f)<0)
		o << "FACTORIES: error" << endl;
	else {
		o << "FACTORIES" <<endl;
		f.operator<<(o);
	}

	vtable_info_array_t  h;
	if (w_factory_t::collect_histogram(h)<0)
		o << "FACTORIES:THREAD HISTOGRAM: error" << endl;
	else {
		o << "FACTORY:THREAD HISTOGRAM" <<endl;
		h.operator<<(o);
	}
}


int main(int argc, char **argv)
{
    int c;
    int errors = 0;
    const	char *s;
    const	char *diskrw = "./diskrw";	/* don't need "." in path. */ 

#ifdef _WIN32
    /* Don't know /tmp on Win32 until runtime */
    s = getenv("TEMP");
    if (s && *s)
    	io_dir = s;
#endif
    s = getenv("TMPDIR");
    if (s && *s)
    	io_dir = s;

    while ((c = getopt(argc, argv, "i:n:b:t:d:klfvV:hRD")) != EOF) {
	   switch (c) {
	   case 'i':
	   case 'n':
		   NumIOs = atoi(optarg);
		   break;
	   case 'b':
		   BufSize = atoi(optarg);
		   break;
	   case 't':
		   NumThreads = atoi(optarg);
		   break;
	   case 'd':
		   io_dir = optarg;
		   break;
	   case 'k':
		   keepopen_io = true;
		   break;
	   case 'l':
		   local_io = true;
		   break;
	   case 'f':
		   fastpath_io = true;
		   break;
	   case 'R':
	   	   raw_io = true;
		   break;
	   case 'V':
		   vec_size = atoi(optarg);
		   break;
	   case 'v':
	   	   verbose = true;
		   break;
	   case	'h':
		    histograms = true;
		    break;
	   case 'D':
		   diskrw = optarg;
		   break;
	   default:
		   errors++;
		   break;
	   }
    }
    if (errors) {
	   cerr << "usage: "
		   << argv[0]
		   << " [-i num_ios]"
		   << " [-b bufsize]"
		   << " [-t num_threads]"
		   << " [-d directory]"
		   << " [-v vectors]"
		   << " [-l]"
		   << " [-h]"
		   << endl;

	   return 1;
    }

    sthread_t::set_diskrw_name(diskrw);

    w_rc_t	e;
    char	*buf;
    e = sthread_t::set_bufsize(BufSize * NumThreads, buf);
    W_COERCE(e);

    cout << "Num Threads = " << NumThreads << endl;

    ioThread = new io_thread_t *[NumThreads];
    if (!ioThread)
	W_FATAL(fcOUTOFMEMORY);

    cout << "Using "
	    << (local_io ? "local" : (fastpath_io ? "fastpath" : "diskrw"))
	    << " io." << endl;

    int i;
    for (i = 0; i < NumThreads; i++)  {
	ioThread[i] = new io_thread_t(i, buf + i*BufSize);
	if (!ioThread[i])
		W_FATAL(fcOUTOFMEMORY);
    }

    /* Start them after they are allocated, so we get some parallelism */
    for (i = 0; i < NumThreads; i++)
	W_COERCE(ioThread[i]->fork());

    for (i = 0; i < NumThreads; i++)
	W_COERCE( ioThread[i]->wait() );

    if (histograms)
    	print_histograms(cout);

    for (i = 0; i < NumThreads; i++)
	delete ioThread[i];

    delete [] ioThread;
    
    sthread_t::dump_stats(cout);
    
    if (verbose)
	sthread_t::dump_event(cout);

    W_COERCE(sthread_t::set_bufsize(0, buf));
    
    return 0;
}


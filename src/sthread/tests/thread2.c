/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

/*
 *  $Id: thread2.c,v 1.16 1996/07/15 22:51:03 bolo Exp $
 */
#include <iostream.h>
#include <strstream.h>
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>
#include <assert.h>
#include <memory.h>

#include <w.h>
#include <w_statistics.h>
#include <sthread.h>
#include <sthread_stats.h>

#ifndef IO_DIR
#define	IO_DIR	"/var/tmp"
#endif

extern "C" char *optarg; 

class io_thread_t : public sthread_t {
public:
    io_thread_t(int i, char *bf) : idx(i), buf(bf), sthread_t(t_regular)   {};
protected:
    virtual void run();
private:
    int		idx;
    char	*buf;
};

io_thread_t **ioThread;

int	NumIOs = 100;
int	NumThreads = 5;
int	BufSize = 1024;

char	*io_dir = IO_DIR;


void io_thread_t::run()
{
    cout << "io[" << idx << "]: started" << endl;
    char fname[40];
    ostrstream f(fname, sizeof(fname));

    f.form("%s/sthread.%d.%d", io_dir, getpid(), idx);
    f << ends;
    
    int fd;
    w_rc_t rc;
    if (rc = sthread_t::open(fname, O_RDWR|O_SYNC|O_CREAT, 0666, fd))	{
	cerr << "open:" << endl << rc << endl;
	W_COERCE(rc);
    }

    int i; 
    for (i = 0; i < NumIOs; i++)  {
	for (register j = 0; j < BufSize; j++)
	    buf[j] = (unsigned char) i;
	if (rc = sthread_t::write(fd, buf, BufSize))  {
	    cerr << "write:" << endl << rc << endl;
	    W_COERCE(rc);
	}
    }

    cout << "io[" << idx << "]: finished writing" << endl;

    off_t pos;
    if (rc = sthread_t::lseek(fd, 0, SEEK_SET, pos))  {
	cerr << "lseek:" << endl << rc << endl;
	W_COERCE(rc);
    }
    
    cout << "io[" << idx << "]: finished seeking" << endl;
    for (i = 0; i < NumIOs; i++) {
	if (rc = sthread_t::read(fd, buf, BufSize))  {
	    cerr << "read:" << endl << rc << endl;
	    W_COERCE(rc);
	}
	for (register j = 0; j < BufSize; j++) {
	    if ((unsigned char)buf[j] != (unsigned char)i) {
		cout << "io[" << idx << "]: read bad data";
		cout.form(" (page %d  expected %d got %d\n",
			i, i, buf[j]);
		W_FATAL(fcINTERNAL);
	    }
	}
    }
    cout << "io[" << idx << "]: finished reading" << endl;

    W_COERCE( sthread_t::fsync(fd) );

    W_COERCE( sthread_t::close(fd) );

    // cleanup after ourself
    (void) ::unlink(fname);
}

main(int argc, char **argv)
{
    int c;
    int errors = 0;

    while ((c = getopt(argc, argv, "i:b:t:d:")) != EOF) {
	   switch (c) {
	   case 'i':
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
		   << endl;	   

	   return 1;
    }

    char *buf = (char*) sthread_t::set_bufsize(BufSize * NumThreads);
    assert(buf);

    ioThread = new io_thread_t *[NumThreads];
    assert(ioThread);

    int i;
    for (i = 0; i < NumThreads; i++)  {
	ioThread[i] = new io_thread_t(i, buf + i*BufSize);
	w_assert1(ioThread[i]);
	W_COERCE(ioThread[i]->fork());
    }
    for (i = 0; i < NumThreads; i++)  {
	W_COERCE( ioThread[i]->wait() );
	delete ioThread[i];
    }
    delete [] ioThread;
    
    cout << SthreadStats << endl;

    (void) sthread_t::set_bufsize(0);

    return 0;
}


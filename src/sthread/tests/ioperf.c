/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

/*
 *  $Id: ioperf.c,v 1.11 1996/07/15 22:51:00 bolo Exp $
 */
#include <iostream.h>
#include <strstream.h>
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>
#include <assert.h>
#include <memory.h>
#include <time.h>
#ifdef SOLARIS2
#include <solaris_stats.h>
#else
#include <unix_stats.h>
#endif
#include <sys/time.h>
#include <sys/stat.h>

#include <w.h>
#include <w_statistics.h>
#include <sthread.h>
#include <sthread_stats.h>

#define SECTOR_SIZE	512
#define BUFSIZE		1024

char* buf = 0;

class io_thread_t : public sthread_t {
public:

    io_thread_t(
	char	rw_flag,
	const	char* file,
	size_t	block_size,
	int	block_cnt,
	char*	buf);

    ~io_thread_t();

protected:

    virtual void run();

private:

    char	_rw_flag;
    const char*	_fname;
    size_t	_block_size;
    int	 	_block_cnt;
    char*	_buf;
};

io_thread_t::io_thread_t(
    char rw_flag,
    const char* file,
    size_t block_size,
    int block_cnt,
    char* buf)
: _rw_flag(rw_flag),
  _fname(file),
  _block_size(block_size), _block_cnt(block_cnt),
  _buf(buf),
  sthread_t(t_regular)
{
}

io_thread_t::~io_thread_t()
{
}

void
io_thread_t::run()
{
    stime_t timeBegin;
    stime_t timeEnd;
    struct stat stbuf;
    bool is_special;
    int oflag = O_RDONLY;
    const char* op_name = 0;
    int fd;
    w_rc_t rc;

    // see if it's a raw or character device
    if(stat(_fname, &stbuf))
	is_special = false;
    else if((stbuf.st_mode & S_IFMT) == S_IFCHR ||
	    (stbuf.st_mode & S_IFMT) == S_IFBLK)
	is_special = true;
    else
	is_special = false;

    // figure out what the open flags should be
    if(_rw_flag == 'r'){
	op_name = "read";
	oflag = O_RDONLY;
    }
    else if(_rw_flag == 'w') {
	op_name = "write";
	oflag = O_RDWR;
    }
    else if(_rw_flag == 'b') {
	op_name = "read/write";
	oflag = O_RDWR;
    }
    else
	cerr << "internal error at " << __LINE__ << endl;

    // some systems don't like O_CREAT with device files
    if(!is_special)
	oflag |= O_CREAT;

    // open the file
    rc = sthread_t::open(_fname, oflag, 0666, fd);
    if(rc){
	cerr << "open:" << endl << rc << endl;
	W_COERCE(rc);
    }

    // for device files, skip over the first 2 sectors so we don't trash
    // the disk label
    if(is_special){
	off_t ret;
	rc = sthread_t::lseek(fd, 2 * SECTOR_SIZE, SEEK_SET, ret);
	if(rc){
	    cerr << "lseek:" << endl << rc << endl;
	    W_COERCE(rc);
	}
    }

    cout << "starting: " << _block_cnt << " "
	 << op_name << " ops of " << _block_size
	 << " bytes each." << endl;

    timeBegin = stime_t::now();

    for (int i = 0; i < _block_cnt; i++)  {
	if (_rw_flag == 'r' || _rw_flag == 'b') {
	    if (rc = sthread_t::read(fd, _buf, _block_size))  {
		cerr << "read:" << endl << rc << endl;
		W_COERCE(rc);
	    }
	}
	if (_rw_flag == 'w' || _rw_flag == 'b') {
	    if (rc = sthread_t::write(fd, _buf, _block_size))  {
		cerr << "write:" << endl << rc << endl;
		W_COERCE(rc);
	    }
	}
    }

    W_COERCE( sthread_t::fsync(fd) );

    timeEnd = stime_t::now();
    
    cout << "finished: " << _block_cnt << " "
	 << op_name << " ops of " << _block_size
	 << " bytes each." << endl;

    if (_rw_flag == 'b') {
	_block_cnt <<= 1; // times 2
    }

    sinterval_t delta(timeEnd - timeBegin);

    cout << "Total time: " << delta << endl;

    cout << "MB/sec: "
	 << (_block_size*_block_cnt)/(1024.0*1024.0) / (double)delta
	 << endl;

    W_COERCE( sthread_t::close(fd) );
}

static unix_stats U1;
#ifndef SOLARIS2
static unix_stats U3(RUSAGE_CHILDREN);
#else
static unix_stats U3; // wasted
#endif

int
main(int argc, char** argv)
{

    U1.start();
    U3.start();
    if (argc != 5) {
	printf("usage: %s r(ead)/w(rite)/b(oth) filename block_size block_cnt\n", argv[0]);
	exit(1);
    }

    char rw_flag = argv[1][0];
    switch (rw_flag) {
    case 'r': case 'w': case 'b':
	break;
    default:
	cerr << "first parameter must be read write flag either r or w"
	     << endl;
	return 1;
    }

    const char* fname = argv[2];
    size_t 	block_size = atoi(argv[3]);
    int  	block_cnt = atoi(argv[4]);

    char* 	buf = sthread_t::set_bufsize(block_size);


    io_thread_t thr(rw_flag, fname, block_size, block_cnt, buf);
    W_COERCE( thr.fork() );
    W_COERCE( thr.wait() );

    cout << SthreadStats << endl;

    U1.stop();
    U3.stop();
    cout << "Unix stats for parent:" << endl ;
    cout << U1 << endl <<endl;
#ifndef SOLARIS2
    cout << "Unix stats for children:" << endl;
    cout << U3 << endl <<endl;
#endif
    return 0;
}

/*<std-header orig-src='shore'>

 $Id: readstats.cpp,v 1.17 1999/06/07 19:06:01 kupsch Exp $

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

#include <stdio.h>
#include <w_stream.h>
#include <w_statistics.h>
#include <w_signal.h>
#include <stdlib.h>
#include <os_fcntl.h>
#include <os_interface.h>

#include <unix_stats.h>

#ifdef _WINDOWS
#include <io.h>
#else
#include <unistd.h>
#endif
#include <sys/stat.h>

#if defined(Sparc) || defined(Mips) || defined(I860)
	extern "C" int fsync(int);
#endif
#if defined(HPUX8) && !defined(_INCLUDE_XOPEN_SOURCE)
	extern "C" int fsync(int);
#endif

#include "diskstats.h"

main(int argc, char *argv[])
{

    int statfd = -1;
    const char *path=0;
    int flags;
    int mode = 0755;

    // get filename from argv
    if(argc < 2) {
	    cerr << "usage: " << argv[0] 
		    << " <filename> " << endl;
	    exit(1);
    }
    flags = O_RDONLY;
    path = argv[1];
    // open & read it into s, u
    if(path) {
	statfd = os_open(path, flags, mode);
	if(statfd<0) {
		w_rc_t e = RC(fcOS);
		cerr << "open(" << path << ") for stats:" << endl
			<< e << endl;
		return(1);
	}
    }

    int cc;
    if((cc = os_read(statfd, &s, sizeof s)) != sizeof s ) {
	    w_rc_t e = RC(fcOS);
	    cerr << "read stats: s:" << endl << e << endl;
	    return(1);
    }

    if((cc = os_read(statfd, &u, sizeof u)) != sizeof u ) {
	    w_rc_t e = RC(fcOS);
	    cerr << "read stats: u:" << endl << e << endl;
	    return(1);
    }

    {
	U r;
	r.copy(s.reads, u);

	float secs = r.compute_time();

	if(secs == 0.0) {
		cout <<"No time transpired." <<endl;
		exit(0);
	}
	cout 
		<< "Non-zero rusages in "
		<< (float)secs << " seconds :" << endl;

	{
	    float usr = r.usertime() / (float)1000000;
	    float sys = r.systime() / (float)1000000;
	    float clk = r.clocktime() / (float)1000000;
	    cout 
		    << "secs( "
		    << (float)usr << "usr " 
		    << (float)sys << "sys " 
		    << (float)clk << "clk ) " 
		    << endl;
	}

	cout
		<< r << endl;
	cout << endl;


	r.copy(s.reads, u);
	cout << "READ CALLS       : " << s.reads << endl;
	if(secs != 0) {
	    cout << "READ CALLS/sec   : " << (float)s.reads/secs << endl;
	}
	r.copy(s.bread, u);
	cout << "BYTES READ       : " << s.bread << endl;
	if(secs != 0) {
	    cout << "BYTES READ/sec   : "  << (float)s.bread/secs << endl;
	}
	if(s.reads != 0) {
	    cout << "BYTES/CALL avg   : " << (int)(s.bread/s.reads) << endl;
	}
	cout << endl;

	r.copy(s.discards, u);
	cout << "DISCARDED RBUF   : " << s.discards << endl;
	r.copy(s.skipreads, u);
	cout << "READS SKIPPED    : " << s.skipreads << endl;

	r.copy(s.writes, u);
	cout << "WRITE CALLS      : " << s.writes << endl;
	if(secs != 0) {
	    cout << "WRITE CALLS/sec  : " << (float)s.writes/secs << endl;
	}

	r.copy(s.bwritten, u);
	cout << "BYTES WRITTEN    : " << s.bwritten << endl;
	if(secs != 0) {
	    cout << "BYTES WRITTEN/sec: "  << (float)s.bwritten/secs << endl;
	}
	if(s.writes != 0) {
	    cout << "BYTES/CALL avg   : " << (int)(s.bwritten/s.writes) << endl;
	}
	cout << endl;

	r.copy(s.fsyncs, u);
	cout << "FSYNC CALLS      : " << s.fsyncs << endl;
	cout << "FSYNC CALLS/sec  : " << (float)s.fsyncs/secs << endl;

	r.copy(s.fsyncs, u);
	cout << "FTRUNCS CALLS    : " << s.ftruncs << endl;

    }
    return 0;
}


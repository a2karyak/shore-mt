/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

//
// shls.C
//

#include <iostream.h>
#include <string.h>
#include <ShoreApp.h>

void do_ls(char *path, bool iflag);
void listdir(char *name, bool iflag);

void
usage(ostream &os, char *progname)
{
    os << "Usage: " << progname << " [-i] [path...]" << endl;
}

int
main(int argc, char *argv[])
{
    char *progname = argv[0];
    option_group_t *options;
    int i;
    bool iflag;
    shrc rc;

    // Establish a connection with the vas and initialize the object
    // cache.
    SH_DO(Shore::init(argc, argv, argv[0], getenv("SHORE_RC")));

    // Begin a transaction.
    SH_BEGIN_TRANSACTION(rc);

    // If that failed, or if a transaction aborted...
    if(rc){
	cerr << rc << endl;
	return 1;
    }

    // The main body of the transaction goes here.
    else{

	// -i is off by default
	iflag = false;

	// check flags
	if(argc > 1){
	    if(argv[1][0] == '-'){
		switch(argv[1][1]){
		case 'i':
		    iflag = true;
		    --argc;
		    ++argv;
		    break;

		default:
		    usage(cerr, progname);
		    exit(1);
		}
	    }
	}

	// With no args, do "shls /".
	if(argc == 1)
	    do_ls("/", iflag);

	// Otherwise, list each arg separately.
	else{
	    for(i = 1; i < argc; ++i)
		do_ls(argv[i], iflag);
	}

	// Commit the transaction.
	SH_DO(SH_COMMIT_TRANSACTION);
    }

    return 0;
}

void
do_ls(char *path, bool iflag)
{
    OStat osp;
    shrc rc;

    // Stat the object.
    rc = Shore::stat(path, &osp);
    if(rc){
	if(rc.err_num() == SH_NotFound)
	    cerr << path << ": No such file or directory" << endl;
	else
	    cerr << path << ": " << rc << endl;
	return;
    }

    // If it's a directory, then list its contents.
    if(osp.type_loid == ReservedOid::_Directory)
	listdir(path, iflag);

    // For everything else, we just list the name (and possibly the oid).
    else{
	if(iflag)
	    cout << osp.loid << " ";
	cout << path << endl;
    }
}

void
listdir(char *name, bool iflag)
{
    // Open a scan over the given directory
    DirScan *scan = new DirScan(name);
    DirEntry entry;

    if(scan->rc() != RCOK){
	cerr << "Error listing directory " << name << endl;
	cerr << scan->rc() << endl;
	delete scan;
	return;
    }

    cout << name << ":" << endl;

    // Scan until end-of-scan, or until an error is encountered.
    while(scan->next(&entry) == RCOK){

	// Print out each entry.
	cout << "\t";
	if(iflag)
	    cout << entry.loid << " ";
	cout << entry.name << endl;
    }

    // Rc indicates why we exited the loop.

    // If rc is anything except SH_EndOfScan, then there was a problem.
    if(scan->rc() != RCOK && scan->rc().err_num() != SH_EndOfScan){
	cerr << "Error listing directory " << name << endl;
	cerr << scan->rc() << endl;
    }

    // The DirScan destructor will close the scan.
    delete scan;
}

/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

/* 
 * tclTest.c --
 *
 *	Test driver for TCL.
 *
 * Copyright 1987-1991 Regents of the University of California
 * All rights reserved.
 *
 * Permission to use, copy, modify, and distribute this
 * software and its documentation for any purpose and without
 * fee is hereby granted, provided that the above copyright
 * notice appears in all copies.  The University of California
 * makes no representations about the suitability of this
 * software for any purpose.  It is provided "as is" without
 * express or implied warranty.
 */

#ifdef DEBUG
static char rcsid[] = "$Header: /p/shore/shore_cvs/src/sm/rssh/rssh.C,v 1.10 1995/07/14 22:08:00 nhall Exp $";
#endif /*DEBUG*/

#define RPC_CLNT
#include <sysdefs.h>
extern "C" {
#include <rpc/rpc.h>
#include <msg.h>
}
#include <tcl.h>

CLIENT* cl;

main(int argc, char* argv[])
{
    bool 		use_logical_id = FALSE;
    struct timeval 	tv;
    cl = clnt_create("caseus", RSSHPROG, 1, "tcp");
    if (! cl) {
	clnt_pcreateerror("caseus");
    }

    tv.tv_sec = 30;
    tv.tv_usec = 0;
    clnt_control(cl, CLSET_TIMEOUT, &tv);
    tv.tv_sec = 5;
    clnt_control(cl, CLSET_RETRY_TIMEOUT, &tv);

    int option;
    char* f_arg = 0;
    while ((option = getopt(argc, argv, "lf:")) != -1) {
	switch (option) {
	case 'f':
	    f_arg = optarg;
	    break;
	case 'l':
	    use_logical_id = TRUE;
	    break;
	default:
	    cerr << "usage: " << argv[0] << " [-f file] [-l (use logical IDs]" << endl;
	}
    }

    FILE* inf = stdin;
    if (f_arg) {
	if ((inf = fopen(f_arg, "r")) == 0) {
	    perror("fopen");
	    exit(-1);
	}
	
    }

    /*
     * start running tcl commands
     */
    char buf[1000];
    ssh_cmd_t cmd;

    /*
     *	Initialize logical ID stuff
     */
    if (use_logical_id) {
	strcpy(buf, "set Use_logical_id 1");
    } else {
	strcpy(buf, "set Use_logical_id 0");
    }
    cmd.line.line_val = buf;
    cmd.line.line_len = strlen(buf) + 1;
    ssh_reply_t* reply = perform_1(&cmd, cl);
    if (! reply) {
	cerr << "Fatal RPC failure in perform_1()" << endl;
	exit(-1);
    }

    /*
     *	Run the commands in the file
     */
    int gotPartial = 0;
    while (1) {
	clearerr(inf);
	if (! gotPartial) {
	    cout << "% " << flush;
	}
	if (fgets(buf, sizeof(buf), inf) == 0) {
	    if (! gotPartial) {
		exit(0);
	    }
	    buf[0] = 0;
	}
	cmd.line.line_val = buf;
	cmd.line.line_len = strlen(buf) + 1;
	ssh_reply_t* reply = perform_1(&cmd, cl);
	if (! reply) {
	    cerr << "Fatal RPC failure in perform_1()" << endl;
	    exit(-1);
	}
	gotPartial = reply->got_partial;
	if (gotPartial) continue;
	
	if (reply->result == TCL_OK) {
	    if (reply->output.output_len != 0) {
		cout << reply->output.output_val << endl;
	    }
	} else {
	    if (reply->result == TCL_ERROR) {
		cout << "Error";
	    } else {
		cout << "Error " << reply->result;
	    }
	    if (reply->output.output_len != 0) {
		cout << ": " << reply->output.output_val;
	    }
	    cout << endl;
	}
    }
}


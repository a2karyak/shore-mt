/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

/*
 *  $Id: ssh.h,v 1.11 1996/03/18 17:22:15 bolo Exp $
 */
#ifndef SSH_H
#define SSH_H

#define SSH_ERROR(err) \
    {                                                           \
        cerr << "ssh error (" << err << ") at file " << __FILE__\
             << ", line " << __LINE__ << endl;                  \
        W_FATAL(fcINTERNAL);                                    \
    }

#undef DO

#define DO(x)                                                   \
    {                                                           \
        int err = x;                                            \
        if (err)  { SSH_ERROR(err); }                           \
    }

#define COMM_ERROR(err) \
        cerr << "communication error: code " << (err) \
             << ", file " << __FILE__ << ", line " << __LINE__

#define COMM_FATAL(err) { COMM_ERROR(err); W_FATAL(fcINTERNAL); }

//
// Start and stop client communication listening thread
//
extern rc_t stop_comm();
extern rc_t start_comm();

#if !defined(Linux) && !defined(AIX41)
extern "C" {
	long random();
	int  srandom(int);
	char *initstate(unsigned long, char *,int);
	char *setstate(char *);
}
#endif

extern int rseed;
extern char rstate[32];

#endif /* SSH_H */

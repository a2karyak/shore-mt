/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

/*
 *  $Id: sysdefs.h,v 1.22 1996/02/28 22:15:11 nhall Exp $
 */
#ifndef SYSDEFS_H
#define SYSDEFS_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <limits.h>
#ifdef Decstation
#include <sysent.h>
#endif
#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/signal.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <netdb.h>
#include <setjmp.h>
#include <new.h>
#include <stream.h>
#include <strstream.h>
#include <fstream.h>
#include <limits.h>
//#include <rpc/rpc.h>

#define W_INCL_LIST
#include <w.h>
#include <sthread.h>

/* Some system include files use bcopy or bzero, so define these */

#define bcopy(a, b, c)   memcpy(b,a,c)
#define bzero(a, b)      memset(a,'\0',b)
#define bcmp(a, b, c)    memcmp(a,b,c)

#ifdef __GNUG__
    extern "C" {
	extern char *strerror (int);
	extern int fsync(int);
#if !defined(Linux) && !defined(AIX32) && !defined(AIX41)
	extern int truncate(const char *, off_t);
#endif
	extern int ftruncate(int, off_t);
    }
#endif

#endif /* SYSDEFS_H */

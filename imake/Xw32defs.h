/*<std-header orig-src='shore' incl-file-exclusion='XW32DEFS_H' no-defines='true'>

 $Id: Xw32defs.h,v 1.7 1999/06/07 20:32:55 kupsch Exp $

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

#ifndef XW32DEFS_H
#define XW32DEFS_H

/*  -- do not edit anything above this line --   </std-header>*/

/* $XConsortium: Xw32defs.h /main/5 1996/11/13 14:43:44 lehors $ */
/* -------------------------------------------------------
	    see Xosdefs.h for copyright notice        
------------------------------------------------------- */

#ifndef _XW32DEFS_H
#define  _XW32DEFS_H

typedef char *caddr_t;

#define access	   _access
#define alloca	   _alloca
#define chdir	_chdir
#define chmod	   _chmod
#define close	   _close
#define creat	   _creat
#define dup	   _dup
#define dup2	   _dup2
#define environ     _environ
#define execl	 _execl
#define execle	 _execle
#define execlp	 _execlp
#define execlpe  _execlpe
#define execv	 _execv
#define execve	 _execve
#define execvp	 _execvp
#define execvpe  _execvpe
#define fdopen	  _fdopen
#define fileno	  _fileno
#define fstat	 _fstat
#define getcwd	_getcwd
#define getpid	 _getpid
#define hypot		_hypot
#define isascii __isascii
#define isatty	   _isatty
#define lseek	   _lseek
#define mkdir	_mkdir
#define mktemp	   _mktemp
#define open	   _open
#define putenv	    _putenv
#define read	   _read
#define rmdir	_rmdir
#define sleep(x) _sleep((x) * 1000)
#define stat	 _stat
#define sys_errlist _sys_errlist
#define sys_nerr    _sys_nerr
#define umask	   _umask
#define unlink	   _unlink
#define write	   _write
#define random   rand
#define srandom  srand

#define O_RDONLY    _O_RDONLY
#define O_WRONLY    _O_WRONLY
#define O_RDWR	    _O_RDWR
#define O_APPEND    _O_APPEND
#define O_CREAT     _O_CREAT
#define O_TRUNC     _O_TRUNC
#define O_EXCL	    _O_EXCL
#define O_TEXT	    _O_TEXT
#define O_BINARY    _O_BINARY
#define O_RAW	    _O_BINARY

#define S_IFMT	 _S_IFMT
#define S_IFDIR  _S_IFDIR
#define S_IFCHR  _S_IFCHR
#define S_IFREG  _S_IFREG
#define S_IREAD  _S_IREAD
#define S_IWRITE _S_IWRITE
#define S_IEXEC  _S_IEXEC

#define	F_OK	0
#define	X_OK	1
#define	W_OK	2
#define	R_OK	4

#endif

/*<std-footer incl-file-exclusion='XW32DEFS_H'>  -- do not edit anything below this line -- */

#endif          /*</std-footer>*/

/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

/*
 *  $Id: regex.posix.h,v 1.3 1995/04/24 19:28:27 zwilling Exp $
 */
#ifndef _REGEX_POSIX_H_
#define _REGEX_POSIX_H_


/*
 * hpux does not have re_comp and re_exec.  Instead there is
 * regcomp() and regexec().
 */

#    define _INCLUDE_XOPEN_SOURCE
#    include <regex.h>
#    include <assert.h>

#    define re_comp re_comp_posix
#    define re_exec re_exec_posix

extern "C" {
	char* re_comp_posix(const char* pattern);
	int	re_exec_posix(const char* string);
}

#endif/* _REGEX_POSIX_H_ */

/*<std-header orig-src='regex' incl-file-exclusion='REGEX_POSIX_H'>

 $Id: regex_posix.h,v 1.5 2006/01/29 22:27:28 bolo Exp $


*/

#ifndef REGEX_POSIX_H
#define REGEX_POSIX_H

#include "w_defines.h"

/*  -- do not edit anything above this line --   </std-header>*/

/*
 * hpux does not have re_comp and re_exec.  Instead there is
 * regcomp() and regexec().
 */

#    define _INCLUDE_XOPEN_SOURCE
#ifndef _WINDOWS
#    include "regex.h"
#else
extern "C" {
#include "regex.h"
}
#endif /*!_WINDOWS */

#include <cassert>

#    define re_comp re_comp_posix
#    define re_exec re_exec_posix

#ifdef __cplusplus
extern "C" {
#endif
	char* re_comp_posix(const char* pattern);
	int	re_exec_posix(const char* string);
#ifdef __cplusplus
}
#endif

/*<std-footer incl-file-exclusion='REGEX_POSIX_H'>  -- do not edit anything below this line -- */

#endif          /*</std-footer>*/

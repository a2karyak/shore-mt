#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ(2.59)
AC_INIT(Shore-Storage-Manager, 5.0.2, Noone._Sadly-_this_is_unsupported_software.)
AC_CONFIG_HEADERS([config/shore-config.h:config/shore-config-h.in])
AC_CONFIG_SRCDIR([config/shore.def])
AC_COPYRIGHT(COPYRIGHT-NOTICE)
AC_CONFIG_AUX_DIR(config)
AM_INIT_AUTOMAKE([-Wall -Werror foreign])

AM_PROG_AS

# Checks for programs.

AC_PROG_CXXCPP
AC_REQUIRE_CPP
AC_PROG_INSTALL 
## AC_PROG_LN_S 
## AC_PROG_MAKE_SET 
AC_PROG_RANLIB 
AC_PATH_PROG([PERL], [perl], [],[]) 


# AC_CANONICAL_BUILD
# AC_CANONICAL_HOST

AC_CHECK_TOOL(RMM, rm, /bin/rm)


# Checks for header files.
## AC_HEADER_STDBOOL 
## AC_HEADER_STDC 
## AC_HEADER_SYS_WAIT 
## AC_HEADER_TIME 
## HAVE_DIRENT_H : src/sthread/os_interface.h
AC_HEADER_DIRENT 
## HAVE_FCNTL_H : src/sthread/os_fcntl.h
AC_CHECK_HEADERS([fcntl.h]) 
## AC_CHECK_HEADERS([limits.h]) 

## used in src/fc/tests/ms_perf.cpp - on windows only, not
## made by "make check"
## AC_CHECK_HEADERS([malloc.h]) 

## used in os_memory.h and tests/
AC_CHECK_HEADERS([memory.h]) 
AC_CHECK_HEADERS([string.h]) 

## used in lid.cpp (logical-id manager)  for volume ids
AC_CHECK_HEADERS([netdb.h]) 

## used in sthread lib
AC_CHECK_HEADERS([netinet/in.h]) 

## AC_CHECK_HEADERS([stdlib.h]) 
## used in solaris_stats only
AC_CHECK_HEADERS([sys/ioctl.h]) 

## used in fc/w_minmax.h
AC_CHECK_HEADERS([sys/param.h]) 

## used in sthread/sfile.cpp, hpp
AC_CHECK_HEADERS([sys/socket.h]) 

## in fc/w_gettimeofday.h, windows 
AC_CHECK_HEADERS([sys/timeb.h]) 

AC_CHECK_HEADERS([unistd.h]) 
## 
## Checks for typedefs, structures, and compiler characteristics.
##
## used in sthread for diskrw
AC_FUNC_FORK 

## redefined malloc/realloc if malloc/realloc doesn't return 
## at least 1 byte of heap-allocated space when given argument of zero,
## that is to say, doesn't return a valid pointer in this case.
## NOTE: haven't adapted code to this, so I'm taking the check out.
## AC_FUNC_MALLOC 
## AC_FUNC_REALLOC 

## used in error log and w_form
AC_FUNC_VPRINTF 

## these are not available in autoconf 2.59
## AC_TYPE_INT8_T 
## AC_TYPE_INT16_T
## AC_TYPE_INT32_T 
## AC_TYPE_INT64_T 
## 
## AC_TYPE_UINT8_T 
## AC_TYPE_UINT16_T
## AC_TYPE_UINT32_T 
## When the definition of [u]int64_t becomes available,
## we should remove the references to long long and -Wno-long-long
## AC_TYPE_UINT64_T 
## 
## AC_TYPE_LONG_LONG_INT 
## AC_TYPE_UNSIGNED_LONG_LONG_INT 


## leave in the arcane existing handling of signals in
## src/fc/w_signal.h
## AC_TYPE_SIGNAL 
AC_TYPE_SIZE_T 
AC_TYPE_UID_T 
AC_TYPE_PID_T 
AC_TYPE_MODE_T 
AC_TYPE_OFF_T 

## AC_CHECK_DECLS([sys_siglist]) 
AC_CHECK_MEMBERS([struct stat.st_blksize]) 
## can't build any of this if wedon't have this type.
## AC_CHECK_TYPES([ptrdiff_t]) 


AH_TEMPLATE(Alpha,	[architecture])
AH_TEMPLATE(I860,	[architecture])
AH_TEMPLATE(I386,	[architecture])
AH_TEMPLATE(i386,	[architecture - not always defined by compiler])
AH_TEMPLATE(IA64,	[architecture])
AH_TEMPLATE(ARCH_LP64,	[64 bit architecture])
AH_TEMPLATE(Snake, 	[architecture])
AH_TEMPLATE(Sparc, 	[architecture])
AH_TEMPLATE(PowerPC,	[architecture])
AH_TEMPLATE(Rs6000,	[architecture])
AH_TEMPLATE(Mips,	[architecture])

AH_TEMPLATE(AIX32, 	[operating system])
AH_TEMPLATE(AIX41, 	[operating system])
AH_TEMPLATE(HPUX8, 	[operating system])
AH_TEMPLATE(Irix,	[operating system])
AH_TEMPLATE(Linux,	[operating system])
AH_TEMPLATE(MacOSX,	[operating system])
AH_TEMPLATE(OSF1,	[operating system])
AH_TEMPLATE(OSF1AD,	[operating system])
AH_TEMPLATE(SOLARIS2,	[operating system])
AH_TEMPLATE(SUNOS41,	[operating system])
AH_TEMPLATE(Ultrix42,	[operating system])
AH_TEMPLATE(__MacOSX__,	[operating system])
AH_TEMPLATE(w_NetBSD,	[operating system])

AH_TEMPLATE(IS_AIX,	[operating system])
AH_TEMPLATE(IS_AIX3,	[operating system])
AH_TEMPLATE(IS_AIX4,	[operating system])
AH_TEMPLATE(IS_HPUX8,	[operating system])
AH_TEMPLATE(IS_LINUX,	[operating system])
AH_TEMPLATE(IS_IRIX,	[operating system])
AH_TEMPLATE(IS_MACOSX,	[operating system])
AH_TEMPLATE(IS_NETBSD,	[operating system])
AH_TEMPLATE(IS_OSF1,	[operating system])
AH_TEMPLATE(IS_OSF1AD,	[operating system])
AH_TEMPLATE(IS_SOLARIS2,[operating system])
AH_TEMPLATE(IS_ULTRIX42,[operating system])
AH_TEMPLATE(IS_SUNOS41,	[operating system])
AH_TEMPLATE(win32,	[operating system])


AH_TEMPLATE(IS_ALPHA_OSF1,	[combo])
AH_TEMPLATE(IS_I386_CYGWIN,	[combo])
AH_TEMPLATE(IS_I386_LINUX,	[combo])
AH_TEMPLATE(IS_I386_NETBSD,	[combo])
AH_TEMPLATE(IS_I386_SOLARIS2,	[combo])
AH_TEMPLATE(IS_I860_OSF1AD,	[combo])
AH_TEMPLATE(IS_IA64_LINUX,	[combo])
AH_TEMPLATE(IS_IX86_NT,		[combo])
AH_TEMPLATE(IS_MIPS_IRIX,	[combo])
AH_TEMPLATE(IS_MIPS_ULTRIX42,	[combo])
AH_TEMPLATE(IS_POWERPC_MACOSX,	[combo])
AH_TEMPLATE(IS_RS6000_AIX32,	[combo])
AH_TEMPLATE(IS_RS6000_AIX41,	[combo])
AH_TEMPLATE(IS_SNAKE_HPUX8,	[combo])
AH_TEMPLATE(IS_SPARC_SOLARIS2,	[combo])
AH_TEMPLATE(IS_SPARC_SUNOS41,	[combo])
AH_TEMPLATE(IS_IX86_NT,		[combo])
AH_TEMPLATE(IS_I386_CYGWIN,	[combo])

AH_TEMPLATE(IS_UNIX,		[windows or unix?])
AH_TEMPLATE(IS_NT,		[opposite of IS_UNIX])
AH_TEMPLATE(IS_DOSDEV,		[dos-based])
AH_TEMPLATE(IS_CYGWIN,		[windows])

# AH_TEMPLATE(DEBUG,		[debug code included])
# AH_TEMPLATE(W_DEBUG,		[debug code included])
# AH_TEMPLATE(NDEBUG,		[no debug code included])
AH_TEMPLATE(W_TRACE,		[tracing debugging code included])

## Checks for library functions.
## if any of these don't exist, we have a real problem.
## For now, don't bother checking, because we don't have any
## alternatives at the moment, and we're not trying to build this
## on windows..
## AC_CHECK_FUNCS([clock_gettime]) 
## AC_CHECK_FUNCS([gethostbyname]) 
## AC_CHECK_FUNCS([gethostname]) 
## AC_CHECK_FUNCS([getpagesize]) 
## AC_CHECK_FUNCS([gettimeofday]) 
## AC_CHECK_FUNCS([isascii]) 
## AC_CHECK_FUNCS([localtime_r]) 
## AC_CHECK_FUNCS([lstat]) 
## AC_CHECK_FUNCS([memchr]) 
## AC_CHECK_FUNCS([memmove]) 
## AC_CHECK_FUNCS([memset]) 
## AC_CHECK_FUNCS([re_comp]) 
## AC_CHECK_FUNCS([regcomp]) 
## AC_CHECK_FUNCS([select]) 
## AC_CHECK_FUNCS([sqrt]) 
## AC_CHECK_FUNCS([strcasecmp]) 
## AC_CHECK_FUNCS([strchr]) 
## AC_CHECK_FUNCS([strdup]) 
## AC_CHECK_FUNCS([strrchr]) 
## AC_CHECK_FUNCS([strstr]) 
## AC_CHECK_FUNCS([strtol]) 
## AC_CHECK_FUNCS([strtoul]) 
## AC_CHECK_FUNCS([strtoull]) 
## AC_CHECK_FUNCS([uname]) 

AC_CHECK_FUNCS([strerror]) 

##  Checks for libraries.
##  For folks who have Tcl installed in a "normal" place..
AC_CHECK_LIB([tcl], [tclCreateCommand])

AC_CHECK_LIB([c], [malloc])
## AC_CHECK_LIB([c], [realloc])
## AC_CHECK_LIB([c], [memcmp])

## ---------------------------------------------------------------
## 
## -------------- Architecture
##
## NOTE: all these IS_xxx, including IS_NT and IS_UNIX
## are not used in the sources and can be removed. For now, they
## are left in for developers' convenience.
##
## ---------------------------------------------------------------

AC_LANG_PUSH(C++)

AC_MSG_CHECKING([architecture])
AC_PREPROC_IFELSE([AC_LANG_PROGRAM(
[[#if defined(_WIN32) && defined(_M_IX86) && defined(_MSC_VER)
#else
#error not this architecture
#endif
]])], 
[AC_DEFINE(I386,,[])
AC_DEFINE(i386)
AC_DEFINE(win32,,[])
AC_DEFINE(IS_IX86_NT,1,[x86 nt])
AC_DEFINE(IS_NT,1,[is nt])
AC_DEFINE(IS_DOSDEV,1,[dos-based])
AC_MSG_RESULT([dos-based nt i386])
],
[])
AC_PREPROC_IFELSE([AC_LANG_PROGRAM(
[[#if defined(_WIN32) && defined(__GNUC__) && defined(i386)
#else
#error not this architecture
#endif
]])], 
[AC_DEFINE(I386,,[])
AC_DEFINE(win32,,[])
AC_DEFINE(IS_NT,1,[is nt])
AC_DEFINE(IS_CYGWIN,1,[nt cygwin])
AC_DEFINE(IS_I386_CYGWIN,1,[x86 nt cygwin])
AC_MSG_RESULT([cygwin nt i386])
],
[])

AC_PREPROC_IFELSE([AC_LANG_PROGRAM(
[[#if defined(sun) && defined(sparc) && (defined(__svr4__)||defined(FORCE_SOLARIS))
#else
#error not this architecture
#endif
]])], 
[AC_DEFINE(Sparc,,[])
AC_DEFINE(SOLARIS2,,[])
AC_DEFINE(IS_SPARC_SOLARIS2,1,[is solaris])
AC_DEFINE(IS_SOLARIS2,1,[sparc solaris])
AC_MSG_RESULT([solaris sparc])
],
[])

AC_PREPROC_IFELSE([AC_LANG_PROGRAM(
[[#if defined(sun) && defined(sparc) && !defined(__svr4__) && !defined(FORCE_SOLARIS)
#else
#error not this architecture
#endif
]])], 
[AC_DEFINE(Sparc,,[])
AC_DEFINE(SUNOS41,,[])
AC_DEFINE(IS_SPARC_SUNOS41,1,[is not solaris])
AC_DEFINE(IS_SUNOS41,1,[not solaris])
AC_MSG_RESULT([sunos  sparc])
],
[])

AC_PREPROC_IFELSE([AC_LANG_PROGRAM(
[[#if defined(sun) && defined(i386) && defined(FORCE_SOLARIS)
#else
#error not this architecture
#endif
]])], 
[AC_DEFINE(I386,,[])
AC_DEFINE(SOLARIS2,,[])
AC_DEFINE(IS_I386_SOLARIS2,1,[is solaris])
AC_DEFINE(IS_SOLARIS2,1,[sparc solaris])
AC_MSG_RESULT([solaris  sparc])
],
[])

AH_VERBATIM([sun],[/*turn off sun and sparc*/
#if defined(sun) && defined(sparc)
#undef sun
#undef sparc
#endif
])

AC_PREPROC_IFELSE([AC_LANG_PROGRAM(
[[#if defined(hpux) && defined(hp9000s800) 
#else
#error not this architecture
#endif
]])], 
[AC_DEFINE(Snake,,[])
AC_DEFINE(HPUX8,,[])
AC_DEFINE(IS_SNAKE_HPUX8,1,[hp unix])
AC_DEFINE(IS_HPUX8,1,[hp unix])
AC_MSG_RESULT([hpux  hp])
],
[])
AH_VERBATIM([hpux],[/*turn off hpux and hp9000s800*/
#if defined(hpux) && defined(hp9000s800)
#undef hpux
#undef hp9000s800
#endif
])

AC_PREPROC_IFELSE([AC_LANG_PROGRAM(
[[#if defined(__OSF1__) && defined(__PARAGON__)
#else
#error not this architecture
#endif
]])], 
[AC_DEFINE(I860,,[])
AC_DEFINE(OSF1AD,,[])
AC_DEFINE(IS_I860_OSF1AD,1,[osf1 paragon])
AC_DEFINE(IS_OSF1AD,1,[osf1])
AC_MSG_RESULT([osf1 paragon])
],
[])

AC_PREPROC_IFELSE([AC_LANG_PROGRAM(
[[#if defined(__osf__) && defined(__alpha)
#else
#error not this architecture
#endif
]])], 
[AC_DEFINE(Alpha,,[])
AC_DEFINE(OSF1,,[])
AC_DEFINE(IS_ALPHA_OSF1,1,[osf1 alpha])
AC_DEFINE(IS_OSF1,1,[osf1])
AC_DEFINE(ARCH_LP64,1,[64 bit])
AC_MSG_RESULT([osf1 alpha 64 bit])
],
[])

AC_PREPROC_IFELSE([AC_LANG_PROGRAM(
[[#if defined(mips) && defined(ultrix)
#else
#error not this architecture
#endif
]])], 
[AC_DEFINE(Mips,,[])
AC_DEFINE(Ultrix42,,[])
AC_DEFINE(IS_MIPS_ULTRIX42,1,[mips ultrix])
AC_DEFINE(IS_ULTRIX42,1,[ultrix])
AC_MSG_RESULT([ultrix mips])
],
[])
AH_VERBATIM([mips],[/*turn off mips and ultrix*/
#if defined(mips) && defined(ultrix)
#undef mips
#undef ultrix
#endif
])


AC_PREPROC_IFELSE([AC_LANG_PROGRAM(
[[#if defined(__NetBSD__) && defined(i386)
#else
#error not this architecture
#endif
]])], 
[AC_DEFINE(I386,,[])
AC_DEFINE(Ultrix42,,[])
AC_DEFINE(IS_I386_NETBSD,1,[i386 netbsd])
AC_DEFINE(w_NetBSD,1,[ultrix])
AC_DEFINE(IS_NETBSD, 1, [netbsd])
AC_MSG_RESULT([netbsd i386])
],
[])

AC_PREPROC_IFELSE([AC_LANG_PROGRAM(
[[#if defined(sgi) 
#else
#error not this architecture
#endif
]])], 
[AC_DEFINE(Mips,,[])
AC_DEFINE(Irix,,[])
AC_DEFINE(IS_MIPS_IRIX,1,[sgi mips irix])
AC_DEFINE(IS_IRIX,1,[irix])
AC_MSG_RESULT([irix mips])
],
[])
AH_VERBATIM([sgi],[/*turn off sgi */
#if defined(sgi) 
#undef sgi
#endif
])

AC_PREPROC_IFELSE([AC_LANG_PROGRAM(
[[#if defined(linux) && defined(i386)
#else
#error not this architecture
#endif
]])], 
[AC_DEFINE(I386,,[])
AC_DEFINE(i386)
AC_DEFINE(Linux,,[])
AC_DEFINE(IS_I386_LINUX,1,[i386 linux])
AC_DEFINE(IS_LINUX,1,[linux])
AC_MSG_RESULT([linux i386])
],
[])

AC_PREPROC_IFELSE([AC_LANG_PROGRAM(
[[#if defined(linux) && defined(__ia64)
#else
#error not this architecture
#endif
]])], 
[AC_DEFINE(IA64,,[])
AC_DEFINE(Linux,,[])
AC_DEFINE(IS_IA64_LINUX,1,[ia64 linux])
AC_DEFINE(ARCH_LP64,1,[for ia64 only])
AC_DEFINE(IS_LINUX,1,[linux])
AC_MSG_RESULT([linux 64 bit])
],
[])

AC_PREPROC_IFELSE([AC_LANG_PROGRAM(
[[#if defined(aix)  && defined(FORCE_AIX4)
#else
#error not this architecture
#endif
]])], 
[AC_DEFINE(Rs6000,,[])
AC_DEFINE(AIX41,,[])
AC_DEFINE(IS_RS6000_AIX41,1,[Rs6000 aix41])
AC_DEFINE(IS_AIX4,1,[aix41])
AC_DEFINE(IS_AIX,1,[aix])
AC_MSG_RESULT([aix4 Rs6000])
],
[])

AC_PREPROC_IFELSE([AC_LANG_PROGRAM(
[[#if defined(aix)  && !defined(FORCE_AIX4)
#else
#error not this architecture
#endif
]])], 
[AC_DEFINE(Rs6000,,[])
AC_DEFINE(AIX32,,[])
AC_DEFINE(IS_RS6000_AIX32,1,[Rs6000 aix32])
AC_DEFINE(IS_AIX3,1,[aix3])
AC_DEFINE(IS_AIX,1,[aix])
AC_MSG_RESULT([aix3 Rs6000])
],
[])


AC_PREPROC_IFELSE([AC_LANG_PROGRAM(
[[#if defined(__ppc__) && defined(__APPLE__)
#else
#error not this architecture
#endif
]])], 
[AC_DEFINE(PowerPC,,[])
AC_DEFINE(MacOSX,,[])
AC_DEFINE(__MacOSX__)
AC_DEFINE(IS_POWERPC_MACOSX,1,[powerpc macosx])
AC_DEFINE(IS_MACOSX,1,[apple mac osx])
AC_MSG_RESULT([mac osx powerpc])
],
[])

AC_PREPROC_IFELSE([AC_LANG_PROGRAM(
[[#if defined(__i386__) && defined(__APPLE__)
#else
#error not this architecture
#endif
]])], 
[AC_DEFINE(I386,,[])
AC_DEFINE(MacOSX,,[])
AC_DEFINE(__MacOSX__)
AC_DEFINE(IS_I386_MACOSX,1,[i386 macosx])
AC_DEFINE(IS_MACOSX,1,[apple mac osx])
AC_MSG_RESULT([mac osx i386])
],
[])

## -------------- IS NT OR UNIX

AH_VERBATIM([IS_UNIX],[/*simply !IS_NT*/
#ifndef IS_NT 
#define IS_UNIX 1
#endif
])
AC_LANG_POP(C++)

## -------------- Check for some specific gcc versions

AC_LANG_PUSH(C++)
# -------------------------------------------------------------------------
#
# USE_IOS_COMPAT
# This simply causes configure/make to add the compat/ directory to
# the include list for compiles.
#
# -------------------------------------------------------------------------
# NOTE: must use AC_PREPROC_IFELSE here because at least one
# compiler version (4.1.2) produces bad assembly, which causes this
# test to fail. Cannot use 4.1.2
# -------------------------------------------------------------------------
AC_MSG_CHECKING([whether the ios headers exist without .h suffix])
AC_PREPROC_IFELSE([AC_LANG_PROGRAM(
[[#include <sstream>
]])], 
[AM_CONDITIONAL([USE_IOS_COMPAT],[test -z "1"]) AC_MSG_RESULT([yes])],
[AM_CONDITIONAL([USE_IOS_COMPAT],[test -n "1"]) AC_MSG_RESULT([no])]
)
# -------------------------------------------------------------------------
#
# W_USE_COMPAT_STRSTREAM
# Unrelated to USE_IOS_COMPAT
#
# unconditional #define W_USE_COMPAT_STRSTREAM to get in-mem stream bufs
#
# At one time we made this optional, but with the newer compilers, it
# is mandatory
#
# -------------------------------------------------------------------------
AH_VERBATIM([W_USE_COMPAT_STRSTREAM],[/*for in-memory str streams*/
#define W_USE_COMPAT_STRSTREAM
])

# -------------------------------------------------------------------------
#
# GCC_BROKEN_WARNINGS
# Needed at config time to tell make what -W flags to put on the compiles.
# Needed at compile time  for smsh/sm_sizeof.cpp
#
# -------------------------------------------------------------------------

AH_TEMPLATE(GCC_BROKEN_WARNINGS,	[/*is gcc 2.96*/])
AH_VERBATIM([GCC_BROKEN_WARNINGS],[/*if is gcc2.96*/
#if defined(__GNUC__) && __GNUC__==2
# if defined(__GNUC_MINOR__) && __GNUC_MINOR__==96
# define GCC_BROKEN_WARNINGS
#endif
#endif
])

AC_MSG_CHECKING([whether the compiler is gcc 2.96])
AC_PREPROC_IFELSE([AC_LANG_PROGRAM(
[[
#if defined(__GNUC__) && (__GNUC__==2) && defined(__GNUC_MINOR__) && (__GNUC_MINOR__==96)
#else
#error not broken warnings
#endif
]])], 
[AM_CONDITIONAL([GCC_BROKEN_WARNINGS], [test -n "1"]) AC_MSG_RESULT([yes])],
[AM_CONDITIONAL([GCC_BROKEN_WARNINGS], [test -z "1"]) AC_MSG_RESULT([no])]
)

# -------------------------------------------------------------------------
#
# NOPEDANTIC: determine if this is a compiler with known problems in
# its include files that prevent -pedantic from being used.
# Needed at config time to tell make whether to use -pedantic
# We find this to be a problem with 3.0 through 3.2
#
# We find this to be a problem with different Linices. Just rip it out....
# -------------------------------------------------------------------------

AH_TEMPLATE(NOPEDANTIC,	[does compiler have header-file pedantic-errors])
# AC_MSG_CHECKING([if compiler has header-file errors precluding -pedantic])
# AC_COMPILE_IFELSE([AC_LANG_PROGRAM(
# [[
# if defined(__GNUC__) && (__GNUC__==3) && defined(__GNUC_MINOR__) && (__GNUC_MINOR__<=2)
# #else
# #   error 
# #endif
# ]])], 
# [AM_CONDITIONAL([NOPEDANTIC], [test -n "1"]) AC_MSG_RESULT([yes])],
# [AM_CONDITIONAL([NOPEDANTIC], [test -z "1"]) AC_MSG_RESULT([no])]
# )
AM_CONDITIONAL([NOPEDANTIC], [test -n "1"])


# -------------------------------------------------------------------------
#
# GCC_VER_34_WARNINGS
# Needed at config time to tell make what -W flags to put on the compiles.
#
# -------------------------------------------------------------------------

AH_TEMPLATE(GCC_VER_34_WARNINGS,	[hack for gcc3.4 and later])
AC_MSG_CHECKING([whether the compiler is gcc 3.4 or later ])
AC_PREPROC_IFELSE([AC_LANG_PROGRAM(
[[
#if defined(__GNUC__) && (__GNUC__==3) && defined(__GNUC_MINOR__) && (__GNUC_MINOR__>=4)
#else
#   if defined(__GNUC__) && (__GNUC__>3) 
#   else
#   error version 3.4
#   endif
#endif
]])], 
[AM_CONDITIONAL([GCC_VER_34_WARNINGS], [test -n "1"]) AC_MSG_RESULT([yes])],
[AM_CONDITIONAL([GCC_VER_34_WARNINGS], [test -z "1"]) AC_MSG_RESULT([no])]
)


# -------------------------------------------------------------------------
#
# GCC_VER_3_WARNINGS
# Needed at config time to tell make what -W flags to put on the compiles.
# Needed for smsh/sm_sizeof.cpp
#
# -------------------------------------------------------------------------

AH_TEMPLATE(GCC_VER_3_WARNINGS,	[hack for gcc3.x ])
AC_MSG_CHECKING([whether the compiler is gcc 3.x ])
AC_PREPROC_IFELSE([AC_LANG_PROGRAM(
[[#if defined(__GNUC__) && (__GNUC__==3) 
#else
#error not version 3
#endif
]])], 
[ AM_CONDITIONAL([GCC_VER_3_WARNINGS],[test -n "1"]) AC_MSG_RESULT([yes])],
[ AM_CONDITIONAL([GCC_VER_3_WARNINGS],[test -z "1"]) AC_MSG_RESULT([no])]
)
AH_TEMPLATE(GCC_VER_3_WARNINGS,	[/*is gcc 3.x, x < 4*/])
AH_VERBATIM([GCC_VER_3_WARNINGS],[/*if is gcc3.x, x < 4*/
#if defined(__GNUC__) && (__GNUC__==3) 
# define GCC_VER_3_WARNINGS
#endif
])


# -------------------------------------------------------------------------
#
# No macros defined.
# Needed at config time to tell if the chosen compiler version is known
#   to fail at build time.
#
# -------------------------------------------------------------------------

AC_MSG_CHECKING([whether the gcc version is known broken: 4.1.x, x <= 2 or 2.95])
AC_PREPROC_IFELSE([AC_LANG_PROGRAM(
[[
#if defined(__GNUC__) && (__GNUC__==4) && defined(__GNUC_MINOR__) && (__GNUC_MINOR__==1) && defined(__GNUC_PATCHLEVEL__) && (__GNUC_PATCHLEVEL__<=2)
#else

#if defined(__GNUC__) && (__GNUC__==2) && defined(__GNUC_MINOR__) && (__GNUC_MINOR__==95) 
#else
#error not version 2.95 or 4.1.2
#endif

#endif
]])], 
[ 
  AC_MSG_RESULT([yes: build will fail])
  AC_MSG_ERROR([
*****************************************************************************
   You are using broken version of gcc. Please try another release.
*****************************************************************************
  ])
  ],
[ AC_MSG_RESULT([no])]
)

AC_MSG_CHECKING([whether the gcc version has known issues: 4.0.1])
AC_PREPROC_IFELSE([AC_LANG_PROGRAM(
[[
#if defined(__GNUC__) && (__GNUC__==4) && defined(__GNUC_MINOR__) && (__GNUC_MINOR__==0) && defined(__GNUC_PATCHLEVEL__) && (__GNUC_PATCHLEVEL__==1)
#else
#error not version 4.0.1
#endif
]])], 
[ 
  AC_MSG_RESULT([yes: build will have warnings])
  AC_MSG_WARN([
*****************************************************************************
   You are using version of gcc that generates questionale assembly. 
   Please try another release, such as 4.0.4.
*****************************************************************************
  ])
  ],
[ AC_MSG_RESULT([no])]
)
AC_LANG_POP(C++)


# -------------------------------------------------------------------------
#
# SM_PAGESIZE
# Needed at compile time. Default is 8K. 
# User can override in shore.def
#
# -------------------------------------------------------------------------
AH_VERBATIM([SM_PAGESIZE],[/*1024 to 32768 inclusive*/
#ifndef SM_PAGESIZE
#define SM_PAGESIZE 8192
#endif
])

# add a bogus "feature" simply for the purpose of getting nicer --help 
AC_ARG_ENABLE([bogus],
[
SHORE-specific Features:], [], [])

# -------------------------------------------------------------------------
#
# handle --enable-debug --disable-debug
# Needed at config time to tell make what flags to put on the compiles,
# and what symbols, -DDEBUG -DW_DEBUG, -UNDEBUG
# We need to put the macro definitions into the shore-config.h
# and take them out of the compile command line.
#
# enabling this feature is equiv to DEBUGCODE ON
#
# -------------------------------------------------------------------------

AC_ARG_ENABLE([debug],
[  --enable-debug	default:no	Turn on full debugging: symbols & code],
[case "${enableval}" in
 yes) debug=true ;;
 no)  debug=false ;;
 *) AC_MSG_ERROR([bad value ${enableval} for --enable-debug]) ;;
esac], [debug=false])
AM_CONDITIONAL([DEBUG], [test x$debug = xtrue])

AH_VERBATIM([EXPENSIVE_STATS],[/*for thread statistics*/
#define EXPENSIVE_STATS
])

# -------------------------------------------------------------------------
#
# We need to have a default stack check level, and 
# when debugging is on, we need to raise the stack check level.
# Note that an environment variable can be used to change the check level,
# and the user can override this (set it to 0, for example) in shore.def
#
# -------------------------------------------------------------------------
AH_VERBATIM([STHREAD_STACK_CHECK],[/*0, 1 or 2*/
#ifndef STHREAD_STACK_CHECK
#ifdef W_DEBUG
#define STHREAD_STACK_CHECK 2
#else
#define STHREAD_STACK_CHECK 1
#endif
#endif
])

# -------------------------------------------------------------------------
#
# handle --enable-dbgsymbols --disable-dbgsymbols
# Needed at config time to tell make what flags to put on the compiles,
#
# enabling this feature is equiv to DEBUGGERSYMBOLS  ON
#
# -------------------------------------------------------------------------
AC_ARG_ENABLE([dbgsymbols],
[  --enable-dbgsymbols	default:no	Turn on debugger symbols],
[case "${enableval}" in
 yes) dbgsymbols=true ;;
 no)  dbgsymbols=false ;;
 *) AC_MSG_ERROR([bad value ${enableval} for --enable-dbgsymbols]) ;;
esac], [dbgsymbols=false])
AM_CONDITIONAL([DEBUGGERSYMBOLS], [test x$dbgsymbols = xtrue])


# -------------------------------------------------------------------------
#
# handle --enable-lgfileaware  --disable-lgfileaware
# Needed at config time to tell make what macros to define
#   -DLARGEFILE_AWARE -D_FILE_OFFSET_BITS=64 
#
# TODO: put these into shore-config.h and not on the compile cmd line
# I spent a day trying to make this work, but configure doesn't
# put the ON/OFF macro into the shore-config.h file. config.log gets
# the definitino right in confdefs.h, but somehow it gets eliminated
# during all the screwing around with sed.
# The AH_VERBATIM and the AC_ARG_ENABLE work, but the AC_DEFINE_UNQUOTED
# does not.
#
# -------------------------------------------------------------------------
AC_ARG_ENABLE([lgfileaware],
[  --enable-lgfileaware	default:yes	Compile to use 64 bit file offsets],
 [case "${enableval}" in
 yes) lgfileaware=ON ;;
 no)  lgfileaware=OFF ;;
 *) AC_MSG_ERROR([bad value ${enableval} for --enable-lgfileaware]) ;;
esac]
 , 
[lgfileaware=ON])
AM_CONDITIONAL([W_LARGEFILE], [test x$lgfileaware = xON])
# AC_DEFINE_UNQUOTED(W_LARGEFILE, $lgfileaware,[use large files?])

AH_VERBATIM([W_LARGEFILE],[#ifdef W_LARGEFILE
#define _FILE_OFFSET_BITS 64
#define LARGEFILE_AWARE
#endif
])
# -------------------------------------------------------------------------
#
# handle --enable-trace --disable-trace
# Needed at config time to tell make what flags to put on the compiles,
# and what symbols, -DW_TRACE
#
# enabling this feature is equiv to TRACECODE  ON
#
# -------------------------------------------------------------------------
# p. 198
AC_ARG_ENABLE([trace],
[  --enable-trace	default:no	Include tracing code],
 [case "${enableval}" in
 yes) trace=1 ;;
 no)  trace=0 ;;
 *) AC_MSG_ERROR([bad value ${enableval} for --enable-trace]) ;;
esac]
 , 
[trace=0])
# If we want to do something different in the makefile again,
# re-install this conditional:
# AM_CONDITIONAL([TRACE], [test x$trace = x1])

AC_DEFINE_UNQUOTED(TRACECODE, $trace,[include tracing code?])
AH_VERBATIM([W_TRACE],[#if TRACECODE==1
#define W_TRACE
#endif
])


# -------------------------------------------------------------------------
#
# handle --enable-largelogpart  --disable-largelogpart
# Needed at config time to tell make what macros to define
#   -DSM_DISKADDR_LARGE
#
#
# -------------------------------------------------------------------------
AC_ARG_ENABLE([largelogpart],
[  --enable-largelogpart	default:yes	Use 64-bit disk addresses in the
 					on-disk structures; allows >2GB 
					log partitions],
[case "${enableval}" in
 yes) lgdisk=1 ;;
 no)  lgdisk=0 ;;
 *) AC_MSG_ERROR([bad value ${enableval} for --enable-largelogpart]) ;;
esac], [lgdisk=1])
# If we want to do something different in the makefile again,
# re-install this conditional:
# AM_CONDITIONAL([LARGELOGPART], [test x$lgdisk = x1])
AC_DEFINE_UNQUOTED(LARGELOGPART, $lgdisk,[use large log partitions?])
AH_VERBATIM([SM_DISKADDR_LARGE],[#if LARGELOGPART==1
#define SM_DISKADDR_LARGE
#endif
])

# -------------------------------------------------------------------------
#
# handle --enable-explicit --disable-explicit
# Needed at config time to tell make what flags to put on the compiles.
#
# -------------------------------------------------------------------------
AC_ARG_ENABLE([explicit],
[  --enable-explicit	default:no	Compile with explicit templates],
[case "${enableval}" in
 yes) explicit=true ;;
 no)  explicit=false ;;
 *) AC_MSG_ERROR([bad value ${enableval} for --enable-explicit]) ;;
esac], [explicit=false])
AM_CONDITIONAL([EXPLICIT_TEMPLATE], [test x$explicit = xtrue])

# -------------------------------------------------------------------------
#
# handle --enable-purify, --enable-quantify, --enable-purecov
# Needed at config time to tell make what include directories to add
# and what libs to add.
#
# -------------------------------------------------------------------------
AC_ARG_ENABLE([purify],
[  --enable-purify	default:no	Enable build of <prog>.pure],
[case "${enableval}" in
 yes) purify=true ;;
 no)  purify=false ;;
 *) AC_MSG_ERROR([bad value ${enableval} for --enable-purify]) ;;
esac], [purify=false])
AM_CONDITIONAL([PURIFY], [test x$purify = xtrue])

AC_ARG_ENABLE([quantify],
[  --enable-quantify	default:no	Enable build of <prog>.quant],
[case "${enableval}" in
 yes) quantify=true ;;
 no)  quantify=false ;;
 *) AC_MSG_ERROR([bad value ${enableval} for --enable-quantify]) ;;
esac], [quantify=false])
AM_CONDITIONAL([QUANTIFY], [test x$quantify = xtrue])

AC_ARG_ENABLE([purecov],
[  --enable-purecov	default:no	Enable build of <prog>.purecov],
[case "${enableval}" in
 yes) purecov=true ;;
 no)  purecov=false ;;
 *) AC_MSG_ERROR([bad value ${enableval} for --enable-purecov]) ;;
esac], [purecov=false])
AM_CONDITIONAL([PURECOV], [test x$purecov = xtrue])


AC_CONFIG_FILES([Makefile])
AC_CONFIG_FILES([config/Makefile])
AC_CONFIG_FILES([tools/Makefile])

AC_CONFIG_FILES([src/Makefile])

AC_CONFIG_FILES([src/fc/Makefile])
AC_CONFIG_FILES([src/fc/tests/Makefile])

AC_CONFIG_FILES([src/sthread/Makefile])
AC_CONFIG_FILES([src/sthread/tests/Makefile])

AC_CONFIG_FILES([src/common/Makefile])
AC_CONFIG_FILES([src/common/tests/Makefile])

AC_CONFIG_FILES([src/sm/Makefile])
AC_CONFIG_FILES([src/sm/tests/Makefile])
AC_CONFIG_FILES([src/sm/smsh/Makefile])
AC_CONFIG_FILES([src/sm/smsh/scripts/Makefile])
AC_CONFIG_FILES([src/sm/smsh/lib/Makefile])

AC_CONFIG_FILES([doc/Makefile])
AC_CONFIG_FILES([doc/ssmapi/Makefile])
AC_CONFIG_FILES([doc/ssmarch/Makefile])
AC_CONFIG_FILES([doc/man/Makefile])
AC_CONFIG_FILES([doc/man/manfc/Makefile])
AC_CONFIG_FILES([doc/man/mansthread/Makefile])
AC_CONFIG_FILES([doc/man/mancommon/Makefile])
AC_CONFIG_FILES([doc/man/manssm/Makefile])
AC_CONFIG_FILES([doc/ssmvas/Makefile])
AC_CONFIG_FILES([doc/ssmvas/hello/Makefile])


AC_OUTPUT

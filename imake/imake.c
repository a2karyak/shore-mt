/*<std-header orig-src='shore' no-defines='true'>

 $Id: imake.c,v 1.11 2000/01/03 15:25:15 kupsch Exp $

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

  -- do not edit anything above this line --   </std-header>*/

/* $XConsortium: imake.c /main/90 1996/11/13 14:43:23 lehors $ */

/***************************************************************************
 *                                                                         *
 *                                Porting Note                             *
 *                                                                         *
 * Add the value of BOOTSTRAPCFLAGS to the cpp_argv table so that it will  *
 * be passed to the template file.                                         *
 *                                                                         *
 ***************************************************************************/

/*
 * 
Copyright (c) 1985, 1986, 1987  X Consortium

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
X CONSORTIUM BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN
AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

Except as contained in this notice, the name of the X Consortium shall not be
used in advertising or otherwise to promote the sale, use or other dealings
in this Software without prior written authorization from the X Consortium.
 * 
 * Original Author:
 *	Todd Brunhoff
 *	Tektronix, inc.
 *	While a guest engineer at Project Athena, MIT
 *
 * imake: the include-make program.
 *
 * Usage: imake [-Idir] [-Ddefine] [-T template] [-f imakefile ] [-C Imakefile.c ] [-s] [-e] [-v] [make flags]
 *
 * Imake takes a template file (Imake.tmpl) and a prototype (Imakefile)
 * and runs cpp on them producing a Makefile.  It then optionally runs make
 * on the Makefile.
 * Options:
 *		-D	define.  Same as cpp -D argument.
 *		-I	Include directory.  Same as cpp -I argument.
 *		-T	template.  Designate a template other
 *			than Imake.tmpl
 *		-f	specify the Imakefile file
 *		-C	specify the name to use instead of Imakefile.c
 *		-s[F]	show.  Show the produced makefile on the standard
 *			output.  Make is not run is this case.  If a file
 *			argument is provided, the output is placed there.
 *              -e[F]   execute instead of show; optionally name Makefile F
 *		-v	verbose.  Show the make command line executed.
 *
 * Environment variables:
 *		
 *		IMAKEINCLUDE	Include directory to use in addition to "."
 *		IMAKECPP	Cpp to use instead of /lib/cpp
 *		IMAKEMAKE	make program to use other than what is
 *				found by searching the $PATH variable.
 * Other features:
 *	imake reads the entire cpp output into memory and then scans it
 *	for occurences of "@@".  If it encounters them, it replaces it with
 *	a newline.  It also trims any trailing white space on output lines
 *	(because make gets upset at them).  This helps when cpp expands
 *	multi-line macros but you want them to appear on multiple lines.
 *	It also changes occurences of "XCOMM" to "#", to avoid problems
 *	with treating commands as invalid preprocessor commands.
 *
 *	The macros MAKEFILE and MAKE are provided as macros
 *	to make.  MAKEFILE is set to imake's makefile (not the constructed,
 *	preprocessed one) and MAKE is set to argv[0], i.e. the name of
 *	the imake program.
 *
 * Theory of operation:
 *   1. Determine the name of the imakefile from the command line (-f)
 *	or from the content of the current directory (Imakefile or imakefile).
 *	Call this <imakefile>.  This gets added to the arguments for
 *	make as MAKEFILE=<imakefile>.
 *   2. Determine the name of the template from the command line (-T)
 *	or the default, Imake.tmpl.  Call this <template>
 *   3. Determine the name of the imakeCfile from the command line (-C)
 *	or the default, Imakefile.c.  Call this <imakeCfile>
 *   4. Store lines of input into <imakeCfile>:
 *	- A c-style comment header (see ImakefileCHeader below), used
 *	  to recognize temporary files generated by imake.
 *	- If DEFAULT_OS_NAME is defined, format the utsname struct and
 *	  call the result <defaultOsName>.  Add:
 *		#define DefaultOSName <defaultOsName>
 *	- If DEFAULT_OS_MAJOR_REV is defined, format the utsname struct
 *	  and call the result <defaultOsMajorVersion>.  Add:
 *		#define DefaultOSMajorVersion <defaultOsMajorVersion>
 *	- If DEFAULT_OS_MINOR_REV is defined, format the utsname struct
 *	  and call the result <defaultOsMinorVersion>.  Add:
 *		#define DefaultOSMinorVersion <defaultOsMinorVersion>
 *	- If DEFAULT_OS_TEENY_REV is defined, format the utsname struct
 *	  and call the result <defaultOsTeenyVersion>.  Add:
 *		#define DefaultOSTeenyVersion <defaultOsTeenyVersion>
 *	- If the file "localdefines" is readable in the current
 *	  directory, print a warning message to stderr and add: 
 *		#define IMAKE_LOCAL_DEFINES	"localdefines"
 *		#include IMAKE_LOCAL_DEFINES
 *	- If the file "admindefines" is readable in the current
 *	  directory, print a warning message to stderr and add: 
 *		#define IMAKE_ADMIN_DEFINES	"admindefines"
 *		#include IMAKE_ADMIN_DEFINES
 *	- The following lines:
 *		#define INCLUDE_IMAKEFILE	< <imakefile> >
 *		#define IMAKE_TEMPLATE		" <template> "
 *		#include IMAKE_TEMPLATE
 *	- If the file "adminmacros" is readable in the current
 *	  directory, print a warning message to stderr and add: 
 *		#define IMAKE_ADMIN_MACROS	"adminmacros"
 *		#include IMAKE_ADMIN_MACROS
 *	- If the file "localmacros" is readable in the current
 *	  directory, print a warning message to stderr and add: 
 *		#define IMAKE_LOCAL_MACROS	"localmacros"
 *		#include IMAKE_LOCAL_MACROS
 *   5. Start up cpp and provide it with this file.
 *	Note that the define for INCLUDE_IMAKEFILE is intended for
 *	use in the template file.  This implies that the imake is
 *	useless unless the template file contains at least the line
 *		#include INCLUDE_IMAKEFILE
 *   6. Gather the output from cpp, and clean it up, expanding @@ to
 *	newlines, stripping trailing white space, cpp control lines,
 *	and extra blank lines, and changing XCOMM to #.  This cleaned
 *	output is placed in a new file, default "Makefile", but can
 *	be specified with -s or -e options.
 *   7. Optionally start up make on the resulting file.
 *
 * The design of the template makefile should therefore be:
 *	<set global macros like CFLAGS, etc.>
 *	<include machine dependent additions>
 *	#include INCLUDE_IMAKEFILE
 *	<add any global targets like 'clean' and long dependencies>
 */

#include <stdio.h>
#include <ctype.h>
#include "Xosdefs.h"
#ifdef WIN32
# include "Xw32defs.h"
#endif
#ifndef X_NOT_POSIX
# ifndef _POSIX_SOURCE
#  define _POSIX_SOURCE
# endif
#endif
#include <sys/types.h>
#include <fcntl.h>
#ifdef X_NOT_POSIX
# ifndef WIN32
#  include <sys/file.h>
# endif
#else
# include <unistd.h>
#endif
#if defined(X_NOT_POSIX) || defined(_POSIX_SOURCE)
# include <signal.h>
#else
# define _POSIX_SOURCE
# include <signal.h>
# undef _POSIX_SOURCE
#endif
#include <sys/stat.h>
#ifndef X_NOT_POSIX
# ifdef _POSIX_SOURCE
#  include <sys/wait.h>
# else
#  define _POSIX_SOURCE
#  include <sys/wait.h>
#  undef _POSIX_SOURCE
# endif
# define waitCode(w)	WEXITSTATUS(w)
# define waitSig(w)	WTERMSIG(w)
typedef int		waitType;
#else /* X_NOT_POSIX */
# ifdef SYSV
#  define waitCode(w)	(((w) >> 8) & 0x7f)
#  define waitSig(w)	((w) & 0xff)
typedef int		waitType;
# else /* SYSV */
#  ifdef WIN32
#   include <process.h>
typedef int		waitType;
#  else
#   include <sys/wait.h>
#   define waitCode(w)	((w).w_T.w_Retcode)
#   define waitSig(w)	((w).w_T.w_Termsig)
typedef union wait	waitType;
#  endif
# endif
# ifndef WIFSIGNALED
#  define WIFSIGNALED(w) waitSig(w)
# endif
# ifndef WIFEXITED
#  define WIFEXITED(w) waitCode(w)
# endif
#endif /* X_NOT_POSIX */
#ifndef X_NOT_STDC_ENV
# include <stdlib.h>
#else
char *malloc(), *realloc();
void exit();
#endif
#if defined(macII) && !defined(__STDC__)  /* stdlib.h fails to define these */
char *malloc(), *realloc();
#endif /* macII */
#ifdef X_NOT_STDC_ENV
extern char	*getenv();
#endif
#include <errno.h>
#ifdef X_NOT_STDC_ENV
extern int	errno;
#endif
#ifndef WIN32
#include <sys/utsname.h>
#endif
#ifndef SYS_NMLN
# ifdef _SYS_NMLN
#  define SYS_NMLN _SYS_NMLN
# else
#  define SYS_NMLN 257
# endif
#endif

#include "imakemdep.h"

/*
 * This define of strerror is copied from (and should be identical to)
 * Xos.h, which we don't want to include here for bootstrapping reasons.
 */
#if defined(X_NOT_STDC_ENV) || (defined(sun) && !defined(SVR4)) || defined(macII)
# ifndef strerror
extern char *sys_errlist[];
extern int sys_nerr;
#  define strerror(n) \
    (((n) >= 0 && (n) < sys_nerr) ? sys_errlist[n] : "unknown error")
# endif
#endif

#define	TRUE		1
#define	FALSE		0

#ifdef FIXUP_CPP_WHITESPACE
int	InRule = FALSE;
# ifdef INLINE_SYNTAX
int	InInline = 0;
# endif
#endif
#ifdef MAGIC_MAKE_VARS
int xvariable = 0;
int xvariables[10];
#endif

/*
 * Some versions of cpp reduce all tabs in macro expansion to a single
 * space.  In addition, the escaped newline may be replaced with a
 * space instead of being deleted.  Blech.
 */
#ifdef FIXUP_CPP_WHITESPACE
void KludgeOutputLine(), KludgeResetRule();
#else
# define KludgeOutputLine(arg)
# define KludgeResetRule()
#endif

typedef	unsigned char	boolean;

#ifdef USE_CC_E
# ifndef DEFAULT_CC
#  define DEFAULT_CC "cc"
# endif
#else
# ifndef DEFAULT_CPP
#  ifdef CPP_PROGRAM
#   define DEFAULT_CPP CPP_PROGRAM
#  else
#   define DEFAULT_CPP "/lib/cpp"
#  endif
# endif
#endif

char *cpp = NULL;

/* XXX should be $TEMP */
#ifdef WIN32
char	*tmpMakefile    = "C:/tmp/Imf.XXXXXX";
char	*tmpImakefile    = "C:/tmp/IIf.XXXXXX";
#else
char	*tmpMakefile    = "/tmp/Imf.XXXXXX";
char	*tmpImakefile    = "/tmp/IIf.XXXXXX";
#endif
char	*make_argv[ ARGUMENTS ] = {
#ifdef WIN32
    "nmake"
#else
    "make"
#endif
};

int	make_argindex;
int	cpp_argindex;
char	*Imakefile = NULL;
char	*Makefile = "Makefile";
char	*Template = "Imake.tmpl";
char	*ImakefileC = "Imakefile.c";
boolean haveImakefileC = FALSE;
char	*cleanedImakefile = NULL;
char	*program;
char	*FindImakefile();
char	*ReadLine();
char	*CleanCppInput();
char	*Strdup();
char	*Emalloc();
void	LogFatalI(), LogFatal(), LogMsg();

void	showit();
void	wrapup();
void	init();
void	AddMakeArg();
void	AddCppArg();
void	SetOpts();
void	CheckImakefileC();
void	cppit();
void	makeit();
void	CleanCppOutput();
boolean	isempty();
void	writetmpfile();

boolean	verbose = FALSE;
boolean	show = TRUE;

int
main(argc, argv)
	int	argc;
	char	**argv;
{
	FILE	*tmpfd;
	char	makeMacro[ BUFSIZ ];
	char	makefileMacro[ BUFSIZ ];

	program = argv[0];
	init();
	SetOpts(argc, argv);

	Imakefile = FindImakefile(Imakefile);
	CheckImakefileC(ImakefileC);

	if (Makefile) {
		tmpMakefile = Makefile;
	} else {
		tmpMakefile = Strdup(tmpMakefile);
		(void) mktemp(tmpMakefile);
	}
	AddMakeArg("-f");
	AddMakeArg( tmpMakefile );
	sprintf(makeMacro, "MAKE=%s", program);
	AddMakeArg( makeMacro );
	sprintf(makefileMacro, "MAKEFILE=%s", Imakefile);
	AddMakeArg( makefileMacro );

if(verbose) { fprintf(stderr, "%d: fopen w+ %s\n" , __LINE__, tmpMakefile); }
	if ((tmpfd = fopen(tmpMakefile, "w+")) == NULL)
		LogFatal("Cannot create temporary file %s.", tmpMakefile);

if(verbose) { fprintf(stderr, "%d: ImakefileC = %s\n" , __LINE__, ImakefileC); }
	cleanedImakefile = CleanCppInput(Imakefile);
if(verbose) { fprintf(stderr, "%d: ImakefileC = %s\n" , __LINE__, ImakefileC); }
	cppit(cleanedImakefile, Template, ImakefileC, tmpfd, tmpMakefile);

if(verbose) { fprintf(stderr, "%d: done with cppit \n" , __LINE__); }

	if (show) { 
		if(verbose) { fprintf(stderr, "%d\n" , __LINE__); }
		if (Makefile == NULL) {
			if(verbose) { fprintf(stderr, "%d\n" , __LINE__); }
			showit(tmpfd);
		    }
	} else {
		if(verbose) { fprintf(stderr, "%d\n" , __LINE__); }
		makeit();
	}
	wrapup();
	exit(0);
}

void
showit(fd)
	FILE	*fd;
{
	char	buf[ BUFSIZ ];
	int	red;

	fseek(fd, 0, 0);
	while ((red = fread(buf, 1, BUFSIZ, fd)) > 0)
		writetmpfile(stdout, buf, red, "stdout");
	if (red < 0)
	    LogFatal("Cannot read %s.", tmpMakefile);
}

void
wrapup()
{
	if (tmpMakefile != Makefile) {
	    if(verbose) {
		    fprintf(stderr, "Not unlinking %s\n", tmpMakefile);
	    } else {
		    unlink(tmpMakefile);
	    }
	}
	if (cleanedImakefile && cleanedImakefile != Imakefile) {
	    if(verbose) {
		    fprintf(stderr, "Not unlinking %s\n", cleanedImakefile);
	    } else {
		    unlink(cleanedImakefile);
	    }
	}
	if (haveImakefileC) {
	    if(verbose) {
		    fprintf(stderr, "Not unlinking %s\n", ImakefileC);
	    } else {
		    unlink(ImakefileC);
	    }
	}
}

#define  INTSIGCATCH

#ifdef SIGNALRETURNSINT
    #define RETVALUE int
#else
    #define RETVALUE void
#endif

#ifdef _WIN32
RETVALUE __cdecl catch(int);
#endif

#ifdef INTSIGCATCH
    RETVALUE catch(sig) int	sig;
#else
    RETVALUE catch() 
#endif
{
	errno = 0;
#ifdef INTSIGCATCH
	LogFatalI("Signal %d.", (int)sig);
#else
	LogFatalI("Unknown signal.");
#endif
}


#ifdef USE_THIS_FOR_CPP
/* USE_THIS_FOR_CPP if set, needs to be set to the string used for the
   c preprocessor, including any arguments.  only the -D's and -U's are
   kept from the stuff set in imakemdep.h.  the macro's value should
   not be quoted.
*/

#undef USE_CC_E
#undef DEFAULT_CPP
#define DEFAULT_CPP forced_cpp_string

#ifdef STRINGIZE_USE_THIS_FOR_CPP
#define XX_STRINGIZE(x) XX_STRINGIZE2(x)
#define XX_STRINGIZE2(x) #x
char forced_cpp_string[] = XX_STRINGIZE(USE_THIS_FOR_CPP);
#undef XX_STRINGIZE
#undef XX_STRINGIZE2
#else
char forced_cpp_string[] = USE_THIS_FOR_CPP;
#endif

void fixup_for_forced_cpp()
{
    char* defines[ARGUMENTS];
    char* s = forced_cpp_string;
    int i;
    int num_defines = 0;
    int cpp_argc = 0;

    /* find -I's, -U's and -D's and keep these */
    for (i = 0; cpp_argv[i] != 0; ++i)  {
        if (cpp_argv[i][0] == '-' && (cpp_argv[i][1] == 'I' || cpp_argv[i][1] == 'D' || cpp_argv[i][1] == 'U'))  {
	    defines[num_defines++] = cpp_argv[i];
	}
    }

    while (*s)  {
	while (*s == ' ')  {		/* skip whitespace */
	    ++s;
	}

	if (*s)  {
	    /* at start of new word */
	    cpp_argv[cpp_argc++] = s;

	    /* find end of word and null terminate */
	    while (*s != ' ' && *s != 0)  {
	        ++s;
	    }
	    if (*s == ' ')  {
		*s++ = 0;
	    }
	}
    }

    /* add -U's and -D's found in original string */
    for (i = 0; i < num_defines; ++i)  {
        cpp_argv[cpp_argc++] = defines[i];
    }

    cpp_argv[cpp_argc] = 0;
}

#else
#define fixup_for_forced_cpp()  /* do nothing if USE_THIS_FOR_CPP not set */
#endif


/*
 * Initialize some variables.
 */
void
init()
{
	register char	*p;
	fixup_for_forced_cpp();  /* may do nothing, see above */

	make_argindex=0;
	while (make_argv[ make_argindex ] != NULL)
		make_argindex++;
	cpp_argindex = 0;
	while (cpp_argv[ cpp_argindex ] != NULL)
		cpp_argindex++;

	/*
	 * See if the standard include directory is different than
	 * the default.  Or if cpp is not the default.  Or if the make
	 * found by the PATH variable is not the default.
	 */
	if (p = getenv("IMAKEINCLUDE")) {
		if (*p != '-' || *(p+1) != 'I')
			LogFatal("Environment var IMAKEINCLUDE %s",
				"must begin with -I");
		AddCppArg(p);
		for (; *p; p++)
			if (*p == ' ') {
				*p++ = '\0';
				AddCppArg(p);
			}
	}
	if (p = getenv("IMAKECPP"))
		cpp = p;
	if (p = getenv("IMAKEMAKE"))
		make_argv[0] = p;

	if (signal(SIGINT, SIG_IGN) != SIG_IGN)
		signal(SIGINT, catch);
if(verbose) { fprintf(stderr, "init done\n"); }
}

void
AddMakeArg(arg)
	char	*arg;
{
	errno = 0;
	if (make_argindex >= ARGUMENTS-1)
		LogFatal("Out of internal storage.", "");
	make_argv[ make_argindex++ ] = arg;
	make_argv[ make_argindex ] = NULL;
}

void
AddCppArg(arg)
	char	*arg;
{
	errno = 0;
	if (cpp_argindex >= ARGUMENTS-1)
		LogFatal("Out of internal storage.", "");
	cpp_argv[ cpp_argindex++ ] = arg;
	cpp_argv[ cpp_argindex ] = NULL;
}

void
SetOpts(argc, argv)
	int	argc;
	char	**argv;
{
	errno = 0;
	/*
	 * Now gather the arguments for make
	 */
	for(argc--, argv++; argc; argc--, argv++) {
if(verbose) { fprintf(stderr, "argc=%d, argv[0]=%s done\n", argc, argv[0]); }
	    /*
	     * We intercept these flags.
	     */
	    if (argv[0][0] == '-') {
		if (argv[0][1] == 'D') {
		    AddCppArg(argv[0]);
		} else if (argv[0][1] == 'I') {
		    AddCppArg(argv[0]);
		} else if (argv[0][1] == 'f') {
		    if (argv[0][2])
			Imakefile = argv[0]+2;
		    else {
			argc--, argv++;
			if (! argc)
			    LogFatal("No description arg after -f flag", "");
			Imakefile = argv[0];
		    }
		} else if (argv[0][1] == 's') {
		    if (argv[0][2])
			Makefile = ((argv[0][2] == '-') && !argv[0][3]) ?
			    NULL : argv[0]+2;
		    else {
			argc--, argv++;
			if (!argc)
			    LogFatal("No description arg after -s flag", "");
			Makefile = ((argv[0][0] == '-') && !argv[0][1]) ?
			    NULL : argv[0];
		    }
if(verbose) { fprintf(stderr, "SHOW <-- true \n"); }
		    show = TRUE;
		} else if (argv[0][1] == 'e') {
		   Makefile = (argv[0][2] ? argv[0]+2 : NULL);
		   show = FALSE;
		} else if (argv[0][1] == 'T') {
		    if (argv[0][2])
			Template = argv[0]+2;
		    else {
			argc--, argv++;
			if (! argc)
			    LogFatal("No description arg after -T flag", "");
			Template = argv[0];
		    }
		} else if (argv[0][1] == 'C') {
		    if (argv[0][2])
			ImakefileC = argv[0]+2;
		    else {
			argc--, argv++;
			if (! argc)
			    LogFatal("No imakeCfile arg after -C flag", "");
			ImakefileC = argv[0];
		    }
		} else if (argv[0][1] == 'v') {
		    verbose = TRUE;
		} else
		    AddMakeArg(argv[0]);
	    } else
		AddMakeArg(argv[0]);
	}
#ifdef USE_CC_E
	if (!cpp)
	{
		AddCppArg("-E");
		cpp = DEFAULT_CC;
	}
#else
	if (!cpp)
		cpp = DEFAULT_CPP;
#endif
	cpp_argv[0] = cpp;
	AddCppArg(ImakefileC);

if(verbose) {
    fprintf(stderr, "%d: cpp = %s\n", __LINE__, cpp);

#ifdef FIXUP_CPP_WHITESPACE
    fprintf(stderr, "FIXUP_CPP_WHITESPACE.\n");
#else
    fprintf(stderr, "-no- FIXUP_CPP_WHITESPACE.\n");
#endif
#ifdef REMOVE_CPP_LEADSPACE
    fprintf(stderr, "REMOVE_CPP_LEADSPACE.\n");
#else
    fprintf(stderr, "-no- REMOVE_CPP_LEADSPACE.\n");
#endif
#ifdef _WIN32
    fprintf(stderr, "_WIN32.\n");
#else
    fprintf(stderr, "UNIX.\n");
#endif
#ifdef INLINE_SYNTAX
    fprintf(stderr, "INLINE_SYNTAX.\n");
#else
    fprintf(stderr, "-no- INLINE_SYNTAX.\n");
#endif
#ifdef MAGIC_MAKE_VARS
    fprintf(stderr, "MAGIC_MAKE_VARS.\n");
#else
    fprintf(stderr, "-no- MAGIC_MAKE_VARS.\n");
#endif
}
}

char *
FindImakefile(Imakefile)
	char	*Imakefile;
{
	if (Imakefile) {
		if (access(Imakefile, R_OK) < 0)
			LogFatal("Cannot find %s.", Imakefile);
	} else {
		if (access("Imakefile", R_OK) < 0)
			if (access("imakefile", R_OK) < 0)
				LogFatal("No description file.", "");
			else
				Imakefile = "imakefile";
		else
			Imakefile = "Imakefile";
	}
	return(Imakefile);
}

void
LogFatalI(s, i)
	char *s;
	int i;
{
	/*NOSTRICT*/
	LogFatal(s, (char *)i);
}

void
LogFatal(x0,x1)
	char *x0, *x1;
{
	static boolean	entered = FALSE;

	if (entered)
		return;
	entered = TRUE;

	LogMsg(x0, x1);
	fprintf(stderr, "  Stop.\n");
	wrapup();
	exit(1);
}

void
LogMsg(x0,x1)
	char *x0, *x1;
{
	int error_number = errno;

	if (error_number) {
		fprintf(stderr, "%s: ", program);
		fprintf(stderr, "%s\n", strerror(error_number));
	}
	fprintf(stderr, "%s: ", program);
	fprintf(stderr, x0, x1);
	fprintf(stderr, "\n");
}

void
showargs(argv)
	char	**argv;
{
	if(verbose) { fprintf(stderr, "showargs:"); }
	for (; *argv; argv++)
		fprintf(stderr, "%s ", *argv);
	fprintf(stderr, "\n");
}

#define ImakefileCHeader "/* imake - temporary file */"

void
CheckImakefileC(masterc)
	char *masterc;
{
	char mkcbuf[1024];
	FILE *inFile;
if(verbose) { fprintf(stderr, "%d: checkImakefileC %s\n" , __LINE__, masterc); }

	if (access(masterc, F_OK) == 0) {
if(verbose) { fprintf(stderr, "%d: fopen r %s\n" , __LINE__, masterc); }
		inFile = fopen(masterc, "r");
		if (inFile == NULL)
			LogFatal("Refuse to overwrite: %s", masterc);
		if ((fgets(mkcbuf, sizeof(mkcbuf), inFile) &&
		     strncmp(mkcbuf, ImakefileCHeader, 
			     sizeof(ImakefileCHeader)-1)))
		{
if(verbose) { fprintf(stderr, "%d: fclose %s\n" , __LINE__, masterc); }
			fclose(inFile);
			LogFatal("Refuse to overwrite: %s", masterc);
		}
if(verbose) { fprintf(stderr, "%d: fclose %s\n" , __LINE__, masterc); }
		fclose(inFile);
	}
}

#define LocalDefineFmt	"#define %s \"%s\"\n"
#define IncludeFmt	"#include %s\n"
#define ImakeDefSym	"INCLUDE_IMAKEFILE"
#define ImakeTmplSym	"IMAKE_TEMPLATE"
#define OverrideWarning	"Warning: local file \"%s\" overrides global macros."

boolean
optional_include(inFile, defsym, fname)
        FILE	*inFile;
        char    *defsym;
        char    *fname;
{
	errno = 0;
	if (access(fname, R_OK) == 0) {
		LogMsg(OverrideWarning, fname);
		return (fprintf(inFile, LocalDefineFmt, defsym, fname) < 0 ||
			fprintf(inFile, IncludeFmt, defsym) < 0);
	}
	return FALSE;
}

void
doit(outfd, cmd, argv)
	FILE	*outfd;
	char    *cmd;
	char	**argv;
{
	int	pid;
	waitType	status;

	/*
	 * Fork and exec the command.
	 */
#ifdef WIN32
	if (outfd)
		dup2(fileno(outfd), 1);
	status = _spawnvp(_P_WAIT, cmd, argv);
	if (status < 0)
		LogFatal("Cannot spawn %s.", cmd);
	if (status > 0)
		LogFatalI("Exit code %d.", status);
#else
	pid = fork();
	if (pid < 0)
		LogFatal("Cannot fork.", "");
	if (pid) {	/* parent... simply wait */
		while (wait(&status) > 0) {
			errno = 0;
			if (WIFSIGNALED(status))
				LogFatalI("Signal %d.", waitSig(status));
			if (WIFEXITED(status) && waitCode(status))
				LogFatalI("Exit code %d.", waitCode(status));
		}
	}
	else {	/* child... dup and exec cmd */
		if (verbose)
			showargs(argv);
		if (outfd)
			dup2(fileno(outfd), 1);
		execvp(cmd, argv);
		LogFatal("Cannot exec %s.", cmd);
	}
#endif
}

#ifndef WIN32
static void
parse_utsname(name, fmt, result, msg)
     struct utsname *name;
     char *fmt;
     char *result;
     char *msg;
{
  char buf[SYS_NMLN * 5 + 1];
  char *ptr = buf;
  int arg;

  /* Assemble all the pieces into a buffer. */
  for (arg = 0; fmt[arg] != ' '; arg++)
    {
      /* Our buffer is only guaranteed to hold 5 arguments. */
      if (arg >= 5)
	LogFatal(msg, fmt);

      switch (fmt[arg])
	{
	case 's':
	  if (arg > 0)
	    *ptr++ = ' ';
	  strcpy(ptr, name->sysname);
	  ptr += strlen(ptr);
	  break;

	case 'n':
	  if (arg > 0)
	    *ptr++ = ' ';
	  strcpy(ptr, name->nodename);
	  ptr += strlen(ptr);
	  break;

	case 'r':
	  if (arg > 0)
	    *ptr++ = ' ';
	  strcpy(ptr, name->release);
	  ptr += strlen(ptr);
	  break;

	case 'v':
	  if (arg > 0)
	    *ptr++ = ' ';
	  strcpy(ptr, name->version);
	  ptr += strlen(ptr);
	  break;

	case 'm':
	  if (arg > 0)
	    *ptr++ = ' ';
	  strcpy(ptr, name->machine);
	  ptr += strlen(ptr);
	  break;

	default:
	  LogFatal(msg, fmt);
	}
    }

  /* Just in case... */
  if (strlen(buf) >= sizeof(buf))
    LogFatal("Buffer overflow parsing uname.", "");

  /* Parse the buffer.  The sscanf() return value is rarely correct. */
  *result = '\0';
  (void) sscanf(buf, fmt + arg + 1, result);
}
#endif

boolean
define_os_defaults(inFile)
	FILE	*inFile;
{
#ifndef WIN32
#if (defined(DEFAULT_OS_NAME) || defined(DEFAULT_OS_MAJOR_REV) || \
     defined(DEFAULT_OS_MINOR_REV) || defined(DEFAUL_OS_TEENY_REV))
	struct utsname name;
	char buf[SYS_NMLN * 5 + 1];

	/* Obtain the system information. */
	if (uname(&name) < 0)
		LogFatal("Cannot invoke uname", "");

# ifdef DEFAULT_OS_NAME
	parse_utsname(&name, DEFAULT_OS_NAME, buf, 
		      "Bad DEFAULT_OS_NAME syntax %s");
	if (buf[0] != '\0')
		fprintf(inFile, "#define DefaultOSName %s\n", buf);
# endif

# ifdef DEFAULT_OS_MAJOR_REV
	parse_utsname(&name, DEFAULT_OS_MAJOR_REV, buf,
		      "Bad DEFAULT_OS_MAJOR_REV syntax %s");
	fprintf(inFile, "#define DefaultOSMajorVersion %s\n", *buf ? buf : "0");
# endif

# ifdef DEFAULT_OS_MINOR_REV
	parse_utsname(&name, DEFAULT_OS_MINOR_REV, buf,
		      "Bad DEFAULT_OS_MINOR_REV syntax %s");
	fprintf(inFile, "#define DefaultOSMinorVersion %s\n", *buf ? buf : "0");
# endif

# ifdef DEFAULT_OS_TEENY_REV
	parse_utsname(&name, DEFAULT_OS_TEENY_REV, buf,
		      "Bad DEFAULT_OS_TEENY_REV syntax %s");
	fprintf(inFile, "#define DefaultOSTeenyVersion %s\n", *buf ? buf : "0");
# endif
#endif
#endif /* WIN32 */
	return FALSE;
}

void
cppit(imakefile, template, masterc, outfd, outfname)
	char	*imakefile;
	char	*template;
	char	*masterc;
	FILE	*outfd;
	char	*outfname;
{
	FILE	*inFile;

	haveImakefileC = TRUE;
if(verbose) { fprintf(stderr, "%d: cppit fopen w %s\n" , __LINE__, masterc); }
	inFile = fopen(masterc, "w");
	if (inFile == NULL)
		LogFatal("Cannot open %s for output.", masterc);
if(verbose) { fprintf(stderr, "%d: fclose %s\n" , __LINE__, masterc); }
	if (fprintf(inFile, "%s\n", ImakefileCHeader) < 0 ||
	    define_os_defaults(inFile) ||
	    optional_include(inFile, "IMAKE_LOCAL_DEFINES", "localdefines") ||
	    optional_include(inFile, "IMAKE_ADMIN_DEFINES", "admindefines") ||
	    fprintf(inFile, "#define %s <%s>\n", ImakeDefSym, imakefile) < 0 ||
	    fprintf(inFile, LocalDefineFmt, ImakeTmplSym, template) < 0 ||
	    fprintf(inFile, IncludeFmt, ImakeTmplSym) < 0 ||
	    optional_include(inFile, "IMAKE_ADMIN_MACROS", "adminmacros") ||
	    optional_include(inFile, "IMAKE_LOCAL_MACROS", "localmacros") ||
	    fflush(inFile) || 
	    fclose(inFile))
		LogFatal("Cannot write to %s.", masterc);
	/*
	 * Fork and exec cpp
	 */
if(verbose) { 
		int i=0;
		fprintf(stderr, "%d: doit: %s \n" , __LINE__, cpp); 
		for (i=0; cpp_argv[i]; i++) {
		    fprintf(stderr, "argv[%d]= %s\n" , i, cpp_argv[i]);
		}
	    }
	doit(outfd, cpp, cpp_argv);
	CleanCppOutput(outfd, outfname);
}

void
makeit()
{
	doit(NULL, make_argv[0], make_argv);
}

char *
CleanCppInput(imakefile)
	char	*imakefile;
{
	FILE	*outFile = NULL;
	FILE	*inFile;
	char	*buf,		/* buffer for file content */
		*pbuf,		/* walking pointer to buf */
		*punwritten,	/* pointer to unwritten portion of buf */
		*ptoken,	/* pointer to # token */
		*pend,		/* pointer to end of # token */
		savec;		/* temporary character holder */
	int	count;
	struct stat	st;

	/*
	 * grab the entire file.
	 */
if(verbose) { fprintf(stderr, "%d: fopen r %s\n" , __LINE__, imakefile); }
	if (!(inFile = fopen(imakefile, "r")))
		LogFatal("Cannot open %s for input.", imakefile);
	if (fstat(fileno(inFile), &st) < 0)
		LogFatal("Cannot stat %s for size.", imakefile);
	buf = Emalloc((int)st.st_size+3);
	count = fread(buf + 2, 1, st.st_size, inFile);
	if (count == 0  &&  st.st_size != 0)
		LogFatal("Cannot read %s:", imakefile);
if(verbose) { fprintf(stderr, "%d: fclose %s\n" , __LINE__, imakefile); }
	fclose(inFile);
	buf[0] = '\n';
	buf[1] = '\n';
	buf[count + 2] = '\0';

	punwritten = pbuf = buf + 2;
if(verbose) { fprintf(stderr, "%d: pbuf  %s\n" , __LINE__, pbuf); }
	while (*pbuf) {
	    /* for compatibility, replace make comments for cpp */
	    if (*pbuf == '#' && pbuf[-1] == '\n' && pbuf[-2] != '\\') {
		ptoken = pbuf+1;
		while (*ptoken == ' ' || *ptoken == '\t')
			ptoken++;
		pend = ptoken;
		while (*pend && *pend != ' ' && *pend != '\t' && *pend != '\n')
			pend++;
		savec = *pend;
		*pend = '\0';
if(verbose) { fprintf(stderr, "%d: ptoken %s\n" , __LINE__, ptoken); }
if(verbose) { fprintf(stderr, "%d: punwritten  %s ptoken-punwritten=%d\n" , 
	__LINE__, punwritten, (int)(ptoken-punwritten)); }
		if (strcmp(ptoken, "define") &&
		    strcmp(ptoken, "if") &&
		    strcmp(ptoken, "ifdef") &&
		    strcmp(ptoken, "ifndef") &&
		    strcmp(ptoken, "include") &&
		    strcmp(ptoken, "line") &&
		    strcmp(ptoken, "else") &&
		    strcmp(ptoken, "elif") &&
		    strcmp(ptoken, "endif") &&
		    strcmp(ptoken, "error") &&
		    strcmp(ptoken, "pragma") &&
		    strcmp(ptoken, "undef")) {
		    if (outFile == NULL) {
			tmpImakefile = Strdup(tmpImakefile);
			(void) mktemp(tmpImakefile);
if(verbose) { fprintf(stderr, "%d: fopen w %s\n" , __LINE__, tmpImakefile); }
			outFile = fopen(tmpImakefile, "w");
			if (outFile == NULL)
			    LogFatal("Cannot open %s for write.",
				tmpImakefile);
		    }
		    writetmpfile(outFile, punwritten, pbuf-punwritten,
				 tmpImakefile);
		    if (ptoken > pbuf + 1)
			writetmpfile(outFile, "XCOMM", 5, tmpImakefile);
		    else
			writetmpfile(outFile, "XCOMM ", 6, tmpImakefile);
		    punwritten = pbuf + 1;
		}
		*pend = savec;
	    }
	    pbuf++;
	}
	if (outFile) {
	    writetmpfile(outFile, punwritten, pbuf-punwritten, tmpImakefile);
if(verbose) { fprintf(stderr, "%d: fclose %s\n" , __LINE__, tmpImakefile); }
	    fclose(outFile);
	    return tmpImakefile;
	}

	return(imakefile);
}

void
CleanCppOutput(tmpfd, tmpfname)
	FILE	*tmpfd;
	char	*tmpfname;
{
	char	*input;
	int	blankline = 0;

	while(input = ReadLine(tmpfd, tmpfname)) {
		if (isempty(input)) {
			if (blankline++)
				continue;
			KludgeResetRule();
		} else {
			blankline = 0;
			KludgeOutputLine(&input);
			writetmpfile(tmpfd, input, strlen(input), tmpfname);
		}
		writetmpfile(tmpfd, "\n", 1, tmpfname);
	}
	fflush(tmpfd);
#ifdef NFS_STDOUT_BUG
	/*
	 * On some systems, NFS seems to leave a large number of nulls at
	 * the end of the file.  Ralph Swick says that this kludge makes the
	 * problem go away.
	 */
	ftruncate (fileno(tmpfd), (off_t)ftell(tmpfd));
#endif
}

/*
 * Determine if a line has nothing in it.  As a side effect, we trim white
 * space from the end of the line.  Cpp magic cookies are also thrown away.
 * "XCOMM" token is transformed to "#".
 */
boolean
isempty(line)
	register char	*line;
{
	register char	*pend;

	/*
	 * Check for lines of the form
	 *	# n "...
	 * or
	 *	# line n "...
	 */
	if (*line == '#') {
		pend = line+1;
		if (*pend == ' ')
			pend++;
		if (*pend == 'l' && pend[1] == 'i' && pend[2] == 'n' &&
		    pend[3] == 'e' && pend[4] == ' ')
			pend += 5;
		if (isdigit(*pend)) {
		    	do {
			    pend++;
			} while (isdigit(*pend));
			if (*pend == '\n' || *pend == '\0')
				return(TRUE);
			if (*pend++ == ' ' && *pend == '"')
				return(TRUE);
		}
		while (*pend)
		    pend++;
	} else {
	    for (pend = line; *pend; pend++) {
		if (*pend == 'X' && pend[1] == 'C' && pend[2] == 'O' &&
		    pend[3] == 'M' && pend[4] == 'M' &&
		    (pend == line || pend[-1] == ' ' || pend[-1] == '\t') &&
		    (pend[5] == ' ' || pend[5] == '\t' || pend[5] == '\0'))
		{
		    *pend = '#';
		    strcpy(pend+1, pend+5);
		}
#ifdef MAGIC_MAKE_VARS
		if (*pend == 'X' && pend[1] == 'V' && pend[2] == 'A' &&
		    pend[3] == 'R')
		{
		    char varbuf[5];
		    int i;

		    if (pend[4] == 'd' && pend[5] == 'e' && pend[6] == 'f' &&
			pend[7] >= '0' && pend[7] <= '9')
		    {
			i = pend[7] - '0';
			sprintf(varbuf, "%0.4d", xvariable);
			strncpy(pend+4, varbuf, 4);
			xvariables[i] = xvariable;
			xvariable = (xvariable + 1) % 10000;
		    }
		    else if (pend[4] == 'u' && pend[5] == 's' &&
			     pend[6] == 'e' && pend[7] >= '0' &&
			     pend[7] <= '9')
		    {
			i = pend[7] - '0';
			sprintf(varbuf, "%0.4d", xvariables[i]);
			strncpy(pend+4, varbuf, 4);
		    }
		}
#endif
	    }
	}
	while (--pend >= line && (*pend == ' ' || *pend == '\t')) ;
	pend[1] = '\0';
	return (*line == '\0');
}

/*ARGSUSED*/
char *
ReadLine(tmpfd, tmpfname)
	FILE	*tmpfd;
	char	*tmpfname;
{
	static boolean	initialized = FALSE;
	static char	*buf, *pline, *end;
	register char	*p1, *p2;

	if (! initialized) {
#ifdef WIN32
		FILE *fp = tmpfd;
#endif
		int	total_red;
		struct stat	st;

		/*
		 * Slurp it all up.
		 */
		fseek(tmpfd, 0, 0);
		if (fstat(fileno(tmpfd), &st) < 0)
			LogFatal("cannot stat %s for size", tmpMakefile);
		pline = buf = Emalloc((int)st.st_size+1);
		total_red = fread(buf, 1, st.st_size, tmpfd);
		if (total_red == 0  &&  st.st_size != 0)
			LogFatal("cannot read %s", tmpMakefile);
		end = buf + total_red;
		*end = '\0';
		fseek(tmpfd, 0, 0);
#if defined(SYSV) || defined(WIN32)
		tmpfd = freopen(tmpfname, "w+", tmpfd);
#ifdef WIN32
		if (! tmpfd) /* if failed try again */
			tmpfd = freopen(tmpfname, "w+", fp);
#endif
		if (! tmpfd)
			LogFatal("cannot reopen %s\n", tmpfname);
#else	/* !SYSV */
		ftruncate(fileno(tmpfd), (off_t) 0);
#endif	/* !SYSV */
		initialized = TRUE;
	    fprintf (tmpfd, "# Makefile generated by imake - do not edit!\n");
	    fprintf (tmpfd, "# %s\n",
		"$XConsortium: imake.c /main/90 1996/11/13 14:43:23 lehors $");
	}

	for (p1 = pline; p1 < end; p1++) {
		if (*p1 == '@' && *(p1+1) == '@'
		    /* ignore ClearCase version-extended pathnames */
		    && !(p1 != pline && !isspace(*(p1-1)) && *(p1+2) == '/'))
		{ /* soft EOL */
			*p1++ = '\0';
			p1++; /* skip over second @ */
			break;
		}
		else if (*p1 == '\n') { /* real EOL */
#if defined(WIN32) || defined(_WIN32)
			if (p1 > pline && p1[-1] == '\r')
				p1[-1] = '\0';
#endif
			*p1++ = '\0';
			break;
		}
	}

	/*
	 * return NULL at the end of the file.
	 */
	p2 = (pline == p1 ? NULL : pline);
	pline = p1;
if(verbose) { 
	fprintf(stderr, "ReadLine returns: %s\n", p2); 
	    fprintf(stderr, "first 2 chars of next line are : %c, %c.\n", 
	    	*pline, *(pline+1)); 
	}
	return(p2);
}

void
writetmpfile(fd, buf, cnt, fname)
	FILE	*fd;
	int	cnt;
	char	*buf;
	char	*fname;
{
	if (fwrite(buf, sizeof(char), cnt, fd) == -1)
		LogFatal("Cannot write to %s.", fname);
}

char *
Emalloc(size)
	int	size;
{
	char	*p;

	if ((p = malloc(size)) == NULL)
		LogFatalI("Cannot allocate %d bytes", size);
	return(p);
}

#ifdef FIXUP_CPP_WHITESPACE
void
KludgeOutputLine(pline)
	char	**pline;
{
	char	*p = *pline;
	char	quotechar = '\0';

if(verbose) { fprintf(stderr, "KludgeOutputLine: %s\n", *pline); }

	switch (*p) {
	    case '#':	/*Comment - ignore*/
		break;
	    case '\t':	/*Already tabbed - ignore it*/
	    	break;
	    case ' ':	/*May need a tab*/
		if (p[1] == '@' || p[1] == '-') { /* ignore-error or
						    don't echo flags */
		    InRule = TRUE;
		    goto breakfor;
		} /* fall through */
	    default:
# ifdef INLINE_SYNTAX
		if (*p == '<' && p[1] == '<') { /* inline file close */
		    InInline--;
		    InRule = TRUE;
		    break;
		}
# endif
		/*
		 * The following cases should not be treated as beginning of 
		 * rules:
		 * variable := name	(GNU make)
		 * variable = .*:.*	(':' should be allowed as value)
		 *	sed 's:/a:/b:'	(: used in quoted values)
		 */
		for (; *p; p++) {
if(verbose) { fprintf(stderr, "*p=%c\n", *p ); }
		    if (quotechar) {
if(verbose) { fprintf(stderr, "%d if(quotechar) yields true : %d\n", __LINE__,
		quotechar ); }
			if (quotechar == '\\' ||
			    (*p == quotechar &&
#ifdef NOTDEF
# ifdef WIN32
			     quotechar != ')' &&
# endif
#endif
			     p[-1] != '\\'))
			    quotechar = '\0';
			continue;
		    } else if(quotechar != '\0') {
if(verbose) { fprintf(stderr, "%d if(quotechar != '\0') yields true\n", __LINE__ ); }
		    }
if(verbose) { fprintf(stderr, "quotechar=%c, *p=%c \n", quotechar, *p ); }
		    switch (*p) {
		    case '\\':
		    case '"':
		    case '\'':
			quotechar = *p;
			break;
		    case '(':
			quotechar = ')';
			break;
		    case '{':
			quotechar = '}';
			break;
		    case '[':
			quotechar = ']';
			break;
		    case '=':
# ifdef REMOVE_CPP_LEADSPACE
			if (!InRule && **pline == ' ') {
			    while (**pline == ' ')
				(*pline)++;
			}
# endif
if(verbose) { fprintf(stderr, "%d goto breakfor \n", __LINE__); }
			goto breakfor;
# ifdef INLINE_SYNTAX
		    case '<':
			if (p[1] == '<') /* inline file start */
			    InInline++;
			break;
# endif
		    case ':':
if(verbose) { fprintf(stderr, "%d\n", __LINE__ ); }
			if (p[1] == '=') {
if(verbose) { fprintf(stderr, "%d goto breakfor \n", __LINE__); }
			    goto breakfor;
			}
if(verbose) { fprintf(stderr, "%d **pline=%c\n", __LINE__, **pline ); }
			while (**pline == ' ') {
			    (*pline)++;
if(verbose) { fprintf(stderr, "%d **pline=%c\n", __LINE__, **pline ); }
			}
			InRule = TRUE;
if(verbose) { fprintf(stderr, "InRule = TRUE\n"); }
			return;
		    }
		}
breakfor:
if(verbose) { fprintf(stderr, "breakfor test: InRule: %d %c\n", (int)(InRule), **pline ); }
		if (InRule && **pline == ' ')
		    **pline = '\t';
		break;
	}
}

void
KludgeResetRule()
{
	InRule = FALSE;
}
#endif /* FIXUP_CPP_WHITESPACE */

char *
Strdup(cp)
	register char *cp;
{
	register char *new = Emalloc(strlen(cp) + 1);

	strcpy(new, cp);
	return new;
}


/*<std-header orig-src='shore' incl-file-exclusion='W_WORKAROUND_H'>

 $Id: w_workaround.h,v 1.51 2000/03/17 20:15:41 bolo Exp $

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

#ifndef W_WORKAROUND_H
#define W_WORKAROUND_H

#include "w_defines.h"

/*  -- do not edit anything above this line --   </std-header>*/


/* see below for more info on GNUG_BUG_12 */
#define GNUG_BUG_12(arg) arg

#if 	defined(__GNUC__) && (__GNUC_MINOR__ < 5)
#error	This software requires gcc 2.5.x or a later release.
#error  Gcc 2.6.0 is preferred.
#endif

#ifdef __GNUC__

#   if (__GNUC_MINOR__ < 6)

    /*
     * G++ also has a bug in calling the destructor of a template
     */
#   define GNUG_BUG_2 1

    /*
     * G++ seems to have a problem calling ::operator delete 
     */
#   define GNUG_BUG_3 1

    /*
     * G++ version 2.4.5 has problems with templates that don't have
     * destructors explicitly defined.  It also seems to have problems
     * with classes used to instantiate templates if those classes
     * do not have destructors.
     */
#   define GNUG_BUG_7 1

    /* bug #8:
     * gcc include files don't define signal() as in ANSI C.
     * we need to get around that
     */
#   define GNUG_BUG_8 1

#endif /* __GNUC_MINOR__ < 6 */

    /*
     * #12
     * This is a bug in parsing specific to gcc 2.6.0.
     * The compiler misinterprets:
     *	int(j)
     * to be a declaration of j as an int rather than the correct
     * interpretation as a cast of j to an int.  This shows up in
     * statements like:
     * 	istrstream(c) >> i;
    */

#   if __GNUC_MINOR__ > 5
#	undef GNUG_BUG_12	
#   	define GNUG_BUG_12(arg) (arg)
#   endif

#   if __GNUC_MINOR__ > 5
/*
 * 	GNU 2.6.0  : template functions that are 
 *  not member functions don't get exported from the
 *  implementation file.
 */
#define 	GNUG_BUG_13 1

/*
 * Cannot explicitly instantiate function templates.
 */
#define 	GNUG_BUG_14 1
#	endif

#	if __GNUC_MINOR__ > 6
/* gcc 2.7.2 has bogus warning messages; it doesn't inherit pointer
   properties correctly */
#define		GNUG_BUG_15  1
#	endif

/* Gcc 64 bit integer math incorrectly optimizes range comparisons such as
   if (i64 < X || i64 > Y)
	zzz;
   This should be re-examined when we upgrade to 2.8, perhaps it is fixed and
   we can make this a 2.7 dependency.
 */
#define	GNUG_BUG_16

/******************************************************************************
 *
 * Migration to standard C++
 *
 ******************************************************************************/
#if (__GNUC__ == 2) && (__GNUC_MINOR__>=90)
/*
 * EGCS is 2.90 (which really screws up any attempt to fix 
 * things based on __GNUC_MINOR__ and __GNUC__
 * and egcs does not define any different macro to identify itself.
 */
#endif

#if (__GNUC__ == 2) && (__GNUC_MINOR__>=90)
#define EGCS_BUG_1
    /*  Certain statements cause egc 1.1.1 to croak. */
#define EGCS_BUG_2
    /*  egcs 1.1.1 gives inappropriate warning about 
     *  uninitialized variable
     */
#endif

#   if (__GNUC_MINOR__ < 8) 
#   define BIND_FRIEND_OPERATOR_PART_1(TYP,TMPL) /**/
#   define BIND_FRIEND_OPERATOR_PART_1B(TYP1,TYP2,TMPLa,TMPLb) /**/
#   define BIND_FRIEND_OPERATOR_PART_2(TYP) /**/
#   define BIND_FRIEND_OPERATOR_PART_2B(TYP1,TYP2) /**/
#   else
#   define BIND_FRIEND_OPERATOR_PART_1(TYP,TMPL) \
	template <class TYP> ostream & operator<<(ostream&o, const TMPL& l);
#   define BIND_FRIEND_OPERATOR_PART_1B(TYP1,TYP2,TMPLa,TMPLb) \
	template <class TYP1, class TYP2> \
	ostream & operator<<(ostream&o, const TMPLa,TMPLb& l);
#   define BIND_FRIEND_OPERATOR_PART_2(TYP)\
	<TYP>
#   define BIND_FRIEND_OPERATOR_PART_2B(TYP1,TYP2)\
	<TYP1, TYP2>
#   endif

#endif /* __GNUC__ */

/******************************************************************************
 *
 * gcc complains about big literals which don't have the LL suffix.
 * don't know if egcs or gcc 2.95 have this problem, but it shouldn't hurt.
 *
 ******************************************************************************/
#ifdef __GNUC__
#define INT64_LITERAL_BUG(x) x ## LL
#else
#define INT64_LITERAL_BUG(x) x
#endif

/* There is a newer version of the STL out there, which is standard with
   gcc-2.95.  It requires different template instantiations, but most
   other use of it is seamless from before.  Specifiying the newer STL
   can either be enabled explicitly in shore.def, or automgically here
   for the appropriate gcc versions.  XXX probably needs an updated
   expression if gcc changes major numbers again. */
#if defined(__GNUC__) && __GNUC__ == 2 && __GNUC_MINOR__ > 90
#define W_NEWER_STL
#endif

/******************************************************************************
 *
 * C string bug
 *
 ******************************************************************************/

/*
 * a C string constant is of type (const char*).  some routines assume that
 * they are (char*).  this is to cast away the const in those cases.
 */
#define C_STRING_BUG (char *)


/******************************************************************************
 *
 * TCL bugs
 *
 ******************************************************************************/

/*
 *tcl doesn't use const char* anywhere, and "string" is a const char*.
 */
#define TCL_CVBUG (char *)


/******************************************************************************
 *
 * Perl library  bugs
 *
 ******************************************************************************/

/*
 * perl doesn't use const char* anywhere, and "string" is a const char*.
 */
#define PERL_CVBUG (char *)

/*
 * perl creates variable through macro expansions which aren't always used.
 */
#define PERL_UNUSED_VAR(x) (void)x


/******************************************************************************
 *
 * HP CC bugs
 *
 ******************************************************************************/
#if defined(Snake) && !defined(__GNUC__)

    /*
     * HP CC does not like some enums with the high order bit set.
     */
#   define HP_CC_BUG_1 1

    /*
     * HP CC does not implement labels within blocks with destructors
     */
#   define HP_CC_BUG_2 1

    /*
     * HP CC does not support having a nested class as as a
     * parameter type to a template class
     */
#   define HP_CC_BUG_3 1

#endif /* defined(Snake) && !defined(__GNUC__) */


/* This is really a library problem; stream.form() and stream.scan()
   aren't standard, but GNUisms.  On the other hand, they should
   be in the standard, because they save us from static form() buffers.
   Using the W_FORM() and W_FORM2() macros instead of
   stream.form() or stream << form() encapsulates this use, so the
   optimal solution can be used on each platform.
   If a portable scan() equivalent is written, a similar set
   of W_SCAN macros could encapuslate input scanning too.
 */  
#if defined(__GNUG__)
#define	W_FORM(stream)		stream . form
#else
#define	W_FORM(stream)		stream << form	
#endif

/* Grab our form if needed.  */
#if defined(_MSC_VER)
extern const char *form(const char *, ...);
#endif

#if defined(_MSC_VER) && defined(USE_CL_MFC)
/* MFC is a pig.  Don't use it except in extremis */
#include <afx.h>
#define W_FORM2(stream, args) \
	do { \
		CString strBuf; strBuf.Format args; stream << strBuf; \
	} while (0)
#else
#define	W_FORM2(stream,args)	W_FORM(stream) args
#endif


#ifdef _MSC_VER

/******************************************************************************
 *
 * Migration to standard C++
 *
 ******************************************************************************/
#   define TEMPLATE_PREFIX(TYP) /**/
#   define BIND_FRIEND_OPERATOR_PART_1(TYP,TMPL) /**/
#   define BIND_FRIEND_OPERATOR_PART_1B(TYP1,TYP2,TMPLa,TMPLb) /**/
#   define BIND_FRIEND_OPERATOR_PART_2(TYP) /**/
#   define BIND_FRIEND_OPERATOR_PART_2B(TYP1,TYP2) /**/

/******************************************************************************
 *
 * Visual C++ bugs
 *
 ******************************************************************************/

	/*
	 * VC++ complains about legitimate uses of this in object
	 * constructors.  This pragma disables the warnings so our
	 * code compiles without generating error messages, and so
	 * we can force the compiler to die on other errors.
	 */

#pragma warning(disable: 4355)

	/*
	 * VC++ complains about converting integers to booleans.
	 * These are valid c++ conversions.
	 */
#pragma warning(disable: 4800)
     
	/*
	 * When performing template expansion, symbols longer than
	 * the 255 character maximum for the MS tools may be generated.
	 * This disables the warnings about those long symbols.
	 */
#pragma warning(disable: 4786)

	/*
	 * Stop warning about no matching delete for a new in 
	 * same function
	 */
#pragma warning(disable: 4291)

    /*
     * VC++ 5.0 thinks that istrstream's first argument is a char *,
     * not a const char *
     */
#define VCPP_BUG_1 (char *)

    /*
     * VC++ 5.0 does not recognize the const void * operator for 
     * w_rc_t in the following context:
     * if (rc = c.m()) ....
     *
     * You must do one of the following:
     * rc = c.m();
     * if (rc) ...
     * 
     * OR
     * if ( (rc = c.m()) != 0) ....
     * You must do one of the following:
     */
#define VCPP_BUG_2  1

    /*
     * Apparently VC++ 5.0 does not instantiate non-member
     * template functions
     * _MSC_VER is 1200 for VC++ 6.0
     */
#if defined(_MSC_VER) && (_MSC_VER < 1200)
#define VCPP_BUG_3 1
#endif

    /*  TODO: try on VCPP and remove */
#define VCPP_BUG_4 0

/* Same as GNUG_BUG_15 */
#define	VCPP_BUG_5  1

/* Setting ios::badbit, etc and base io manipulators */
#define STD_NAMESPACE std

/* Visual C++ does not understand  T x(parameters) initializers
   which are the same as T x = T(parameters) initializers without
   the extra cruft. */
#define	VCPP_BUG_6	1

/*
 * Visual C++ compiler generates bogus calls to 
   class-member operator delete[].
   If T* p was created with
	p = new T[n],
   the complier is supposed to generate code for
	delete[] p
   by calling
	 T::operator delete[](p, s)
   where s =  delta + sizeof(T)*n 

   Unfortunately, VC++ compiler calls 
	 T::operator delete[](p, sizeof(T))
   (whether or not T has a destructor).
   Interestingly enough, if T has a destructor, delta is non-zero
   but the 2nd argument to T::operator delete[]() is still wrong.
 */
#define VCPP_BUG_7 

/*
 * Visual C++ does allow the conversion of a const char* to a void*
 * consequently the following fails without a cast to (char*):
 *
 * const char* p = new char[n];
 * delete[] p;  // only works if written as delete[] (char*)p;
 */
#define VCPP_BUG_DELETE_CONST (char*)

#else

#define VCPP_BUG_DELETE_CONST

#endif /* _MSC_VER */

/*
 * Default definitions of macros used everywhere:
 * NB: re: VC++/g++ differences in treatments of sizeof:
 * whereas sizeof(empty class) == 1 in both cases,
 * classes inheriting from an empty class don't get the 1 byte
 * added in by VC++, but in g++, those classes DO get an extra 1(4)
 * bytes(s) added in by the inheritance. 
 */

#ifndef VCPP_BUG_1
#define VCPP_BUG_1 
#endif

/*<std-footer incl-file-exclusion='W_WORKAROUND_H'>  -- do not edit anything below this line -- */

#endif          /*</std-footer>*/

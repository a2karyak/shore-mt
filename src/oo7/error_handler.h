#ifndef _ERROR_HANDLER_h
#define _ERROR_HANDLER_h

/*$Header: /p/shore/shore_cvs/src/oo7/error_handler.h,v 1.2 1994/02/04 19:21:26 nhall Exp $*/
/********************************************************/
/*                                                      */
/*               OO7 Benchmark                          */
/*                                                      */
/*              COPYRIGHT (C) 1993                      */
/*                                                      */
/*                Michael J. Carey 		        */
/*                David J. DeWitt 		        */
/*                Jeffrey Naughton 		        */
/*               Madison, WI U.S.A.                     */
/*                                                      */
/*	         ALL RIGHTS RESERVED                    */
/*                                                      */
/********************************************************/

extern int handle_error(int, char*, int);

#define HANDLE_ERROR(_code) handle_error((_code), __FILE__, __LINE__)

#endif /* _ERROR_HANDLER_h */


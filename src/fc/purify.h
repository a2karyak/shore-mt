/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

/*
 * Public declarations.
 */

#ifndef PURIFY_H
#define PURIFY_H

/*
 * Provide prototypes for ANSI C and C++, but skip them for KR C.
 */

#if defined (__cplusplus) || defined (c_plusplus)
#define PROTO(a)	a
extern "C" {
#else
#if defined (__STDC__)
#define PROTO(a)	a
#else
#define PROTO(a)	()
#endif /*__STDC__*/
#endif /*c++*/

/*
 * Take advantage of GNU gcc/g++ printf format checking.
 * These macros may be appended to printf-like functions to invoke checking.
 */

#if (__GNUC__ == 2 && ! defined(__cplusplus))
/* fmt is no (starting at 1) of format string, arg1 is no of first arg */
#define		PRINTF_FORMAT(fmt, arg1)				\
  __attribute__ ((format(printf, fmt, arg1)))
#else
#define		PRINTF_FORMAT(fmt, arg1)
#endif

/*
 * Use these variables for setting conditional breakpoints etc.
 * They contain interesting values when purify_stop_here is called.
 */

extern unsigned long	purify_report_address;
extern unsigned long	purify_report_number;
extern unsigned long	purify_report_type;
extern unsigned long	purify_report_result;

/*
 * Functions to be called to find and print memory leaks and allocations.
 */

int			purify_new_leaks PROTO((void));
int			purify_all_leaks PROTO((void));
int			purify_clear_leaks PROTO((void));

int			purify_new_inuse PROTO((void));
int			purify_all_inuse PROTO((void));
int			purify_clear_inuse PROTO((void));

/* still supported, but will go away in the future */
int			purify_newallocated PROTO((void)); /* OBSOLETE */
int			purify_allallocated PROTO((void)); /* OBSOLETE */
int			purify_clear_newallocated PROTO((void));/* OBSOLETE */
int			purify_newleaks PROTO((void)); /* OBSOLETE */
int			purify_allleaks PROTO((void)); /* OBSOLETE */
int			purify_clear_newleaks PROTO((void)); /* OBSOLETE */

/*
 * Report batching mode control.
 */

int			purify_new_reports PROTO((void));
int			purify_all_reports PROTO((void));
int			purify_clear_new_reports PROTO((void));

int			purify_start_batch PROTO((void));
int			purify_start_batch_show_first PROTO((void));
int			purify_stop_batch PROTO((void));

/*
 * User-customizable error reporting.
 * All these functions take printf style arguments,
 * and are no-ops in the non-purify case handled
 * by purify_stubs.a
 */

/* Print into logfile if that is set, else bit bucket */
int			purify_logfile_printf PROTO((const char *fmt, ...))
			  PRINTF_FORMAT(1, 2);

/* Print into logfile if that is set, else stderr */
int			purify_printf PROTO((const char *fmt, ...))
			  PRINTF_FORMAT(1, 2);

/* Print into logfile if that is set, else stderr, with call chain afterwards */
int			purify_printf_with_call_chain
			  PROTO((const char *fmt, ...))
			  PRINTF_FORMAT(1, 2);
/*
 * Debugging aid functions.
 */

char *			purify_describe PROTO((char*)); /* used to be called purify_print */
int			purify_what_colors PROTO((char*, unsigned int));

/*
 * Watchpoints.
 */

int			purify_watch PROTO((char *));

int			purify_watch_1 PROTO((char *));
int			purify_watch_2 PROTO((char *));
int			purify_watch_4 PROTO((char *));
int			purify_watch_8 PROTO((char *));

int			purify_watch_w_1 PROTO((char *));
int			purify_watch_w_2 PROTO((char *));
int			purify_watch_w_4 PROTO((char *));
int			purify_watch_w_8 PROTO((char *));

int			purify_watch_rw_1 PROTO((char *));
int			purify_watch_rw_2 PROTO((char *));
int			purify_watch_rw_4 PROTO((char *));
int			purify_watch_rw_8 PROTO((char *));

int			purify_watch_r_1 PROTO((char *));
int			purify_watch_r_2 PROTO((char *));
int			purify_watch_r_4 PROTO((char *));
int			purify_watch_r_8 PROTO((char *));

int			purify_watch_n PROTO((char *where,
					      int how_many, /* bytes */
					      char *type    /* "r", "w", "rw" */
					      ));

int			purify_watch_info PROTO((void));
int			purify_watch_remove PROTO((int watchno));
int			purify_watch_remove_all PROTO((void));

/*
 * The function purify_stop_here is not really a function,
 * but a label attached to some very hairy code which serves to
 * convince different debuggers that you are in a real function,
 * even if the user's code is in a windowless leaf function.
 * So don't call purify_stop_here() and expect to see a return!
 *
 * If you really want to stop_here, call the following function,
 * which does some setup first.
 */

int			purify_stop_here_internal PROTO((void));

/*
 * Undocumented function that Purify's the file 'in' and puts the name of
 * the purify'd file in 'out'. This can be used with custom dynamic
 * loaders.
 * The Sun dynamic library loader (dlopen) is already handled
 * automatically by purify.
 */

int			purify_process_file PROTO((const char in[1024],
						   char out[1024]));

/*
 * Tell they symbol table lookup routines what files are mapped where.
 */

int			purify_note_loaded_file PROTO((char* filename,
			  unsigned int where_loaded));

/*
 * return TRUE if purify is running
 */

int			purify_is_running PROTO((void));

/*
 * Call exit(status).
 * If errors were detected, OR in 0x40 to status word.
 * If leaks were detected, OR in 0x20.
 * If potential leaks were detected, OR in 0x10.
 * The original status word (if non-zero) may be seen
 *   in the purify summary report.
 */

int			purify_exit PROTO((int status));

/*
 * Handle to permit a purify_override function to call the original
 * overridden function.  Offers wrapper functionality.
 */

#if defined (__cplusplus) || defined (c_plusplus)
void *			purify_original PROTO((...));
#else
void *			purify_original PROTO(());
#endif

/* Read/write protected memory without triggering access errors. */
char *			purify_unsafe_memcpy PROTO((char *dst, const char *src,
						    int n));

/* Asserts memory readable/writable, or prints error and calls stop_here. */
int			purify_assert_is_readable PROTO((const char *src,
							 int n));
int			purify_assert_is_writable PROTO((const char *src,
							 int n));

/*
 * Pool allocator functionality.
 */

void			purify_set_pool_id PROTO((char *mem, int id));
int			purify_get_pool_id PROTO((char *mem));
void			purify_set_user_data PROTO((char *mem, void* data));
void *			purify_get_user_data PROTO((char *mem));
void			purify_map_pool PROTO((int id,
					       void(*fn)(char *, int, void *)));
  /* e.g. map_pool_func(char *user_mem, int user_size, void *user_aux_dat) */
void			purify_map_pool_id PROTO((void(*fn)(int)));
  /* e.g. map_id_func(int pool_id) */
char *			purify_start_of_block PROTO((char *mem));
int			purify_size_of_block PROTO((char *mem));

#if defined (__cplusplus) || defined (c_plusplus)
}
#endif

#endif /*PURIFY_H*/

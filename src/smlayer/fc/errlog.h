/*<std-header orig-src='shore' incl-file-exclusion='ERRLOG_H'>

 $Id: errlog.h,v 1.11 1999/06/07 19:02:42 kupsch Exp $

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

#ifndef ERRLOG_H
#define ERRLOG_H

#include "w_defines.h"

/*  -- do not edit anything above this line --   </std-header>*/

#include <assert.h>
#include <stdlib.h>
#include <stddef.h>
#include <w.h>
#include <w_stream.h>
#include <stdio.h>



/* errlog.h -- facilities for logging errors */

#ifdef __GNUG__
#pragma interface
#endif

class ErrLog; // forward
class logstream; // forward

#ifndef	_SYSLOG_H
#define LOG_EMERG 0 
#define LOG_ALERT 1
#define LOG_CRIT  2
#define LOG_ERR   3
#define LOG_WARNING 4
#define LOG_NOTICE  5
#define LOG_INFO  6
#define LOG_DEBUG 7
#define LOG_USER  8
#endif

enum LogPriority {
	log_none = -1,	// none (for global variable logging_level only)
	log_emerg = LOG_EMERG,		// no point in continuing (syslog's LOG_EMERG)
	log_fatal = LOG_ALERT,		// no point in continuing (syslog's LOG_ALERT)
	log_alert = log_fatal,		// alias
	log_internal = LOG_CRIT,	// internal error 
	log_error = LOG_ERR,		// client error 
	log_warning = LOG_WARNING,	// client error 
	log_info = LOG_INFO,		// just for yucks 
	log_debug=LOG_DEBUG,		// for debugging gory details 
	log_all
};
#define default_prio  log_error

extern ostream& flushl(ostream& o);
extern ostream& emerg_prio(ostream& o);
extern ostream& fatal_prio(ostream& o);
extern ostream& internal_prio(ostream& o);
extern ostream& error_prio(ostream& o);
extern ostream& warning_prio(ostream& o);
extern ostream& info_prio(ostream& o);
extern ostream& debug_prio(ostream& o);
extern	void setprio(ostream&, LogPriority);
extern logstream *is_logstream(ostream &);

#define LOGSTREAM__MAGIC 0xad12bc45
#define ERRORLOG__MAGIC  0xa2d29754

class logstream : public ostrstream {
	friend class ErrLog;
	friend ostream &flush_and_setprio(ostream& o, LogPriority p);
	friend ostream& emerg_prio(ostream& o);
	friend ostream& fatal_prio(ostream& o);
	friend ostream& internal_prio(ostream& o);
	friend ostream& error_prio(ostream& o);
	friend ostream& warning_prio(ostream& o);
	friend ostream& info_prio(ostream& o);
	friend ostream& debug_prio(ostream& o);

	unsigned int 	__magic1;
	LogPriority 	_prio;
	ErrLog*		_log;
	unsigned int 	__magic2;

public:
	friend logstream *is_logstream(ostream &);

private:
	static ostrstream static_stream;
public:
	logstream(char *buf, size_t bufsize = 1000) : 
		ostrstream(buf, bufsize), __magic1(LOGSTREAM__MAGIC),
		_prio(log_none), 
		__magic2(LOGSTREAM__MAGIC)
		{
			// tie this to a hidden stream; 
			tie(&static_stream);
			assert(tie() == &static_stream) ;
			assert(__magic1==LOGSTREAM__MAGIC);
		}

protected:
	void  init_errlog(ErrLog* mine) { _log = mine; }
};

enum LoggingDestination {
	log_to_ether, // no logging
	log_to_unix_file, 
	log_to_open_file, 
	log_to_stderr
}; 

typedef void (*ErrLogFunc)(ErrLog *, void *);

class ErrLog {
	friend class logstream;
	friend logstream *is_logstream(ostream &);
	friend ostream &flush_and_setprio(ostream& o, LogPriority p);

	LoggingDestination	_destination;
	LogPriority 		_level;
	FILE* 			_file;		// if local file logging is used
	const char * 		_ident; // identity for syslog & local use
	char *			_buffer; // default is static buffer
	size_t			_bufsize; 
public:
	ErrLog(
		const char *ident,
		LoggingDestination dest, // required
		const char *filename = 0, 			
		LogPriority level =  default_prio,
		char *ownbuf = 0,
		int  ownbufsz = 0  // length of ownbuf, if ownbuf is given
	);
	ErrLog(
		const char *ident,
		LoggingDestination dest, // required
		FILE *file = 0, 			
		LogPriority level =  default_prio,
		char *ownbuf = 0,
		int  ownbufsz = 0  // length of ownbuf, if ownbuf is given
	);
	~ErrLog();

	static LogPriority parse(const char *arg, bool *ok=0);

	// same name
	logstream	clog;
	void log(enum LogPriority prio, const char *format, ...);

	const char * ident() { 
		return _ident;
	}
	LoggingDestination	destination() { return _destination; };

	LogPriority getloglevel() { return _level; }
	const char * getloglevelname() { 
		switch(_level) {
			case log_none:
				return "log_none";
			case log_emerg:
				return "log_emerg";
			case log_fatal:
				return "log_fatal";
			case log_internal:
				return "log_internal";
			case log_error:
				return "log_error";
			case log_warning:
				return "log_warning";
			case log_info:
				return "log_info";
			case log_debug:
				return "log_debug";
			case log_all:
				return "log_all";
			default:
				return "error: unknown";
				// w_assert1(0);
		}
	}

	LogPriority setloglevel( LogPriority prio) {
		LogPriority old = _level;
		_level =  prio;
		return old;
	}

	static ErrLog *find(const char *id);
	static void apply(ErrLogFunc func, void *arg);

private:
	void _init1();
	void _init2();
	void _flush(bool in_crit);
	void _openlogfile( const char *filename );
	void _closelogfile();
	NORET ErrLog(const ErrLog &); // disabled
	
	unsigned int _magic;
#if defined(_WIN32) && defined(FC_ERRLOG_WIN32_LOCK)
	CRITICAL_SECTION _crit;
#endif

} /* ErrLog */;

/*<std-footer incl-file-exclusion='ERRLOG_H'>  -- do not edit anything below this line -- */

#endif          /*</std-footer>*/

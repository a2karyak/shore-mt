/*<std-header orig-src='shore' incl-file-exclusion='W_ERROR_H'>

 $Id: w_error.h,v 1.55 1999/06/07 19:02:52 kupsch Exp $

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

#ifndef W_ERROR_H
#define W_ERROR_H

#include "w_defines.h"

/*  -- do not edit anything above this line --   </std-header>*/

#ifdef __GNUG__
#pragma interface
#endif

struct w_error_info_t {
    w_base_t::uint4_t	err_num;
    const char*		errstr;
};

class w_error_t : public w_base_t {
public:
    typedef w_error_info_t info_t;

#include "fc_error_enum_gen.h"

    // kludge: make err_num come first:
    const uint4_t		err_num;

    const char* const		file;
    const uint4_t		line;
    const int4_t		sys_err_num;

    w_error_t*			next() { return _next; }

    w_error_t&			add_trace_info(
	const char* const 	    filename,
	uint4_t			    line_num);

    ostream			&print_error(ostream &o) const;

    static w_error_t*		make(
	const char* const 	    filename,
	uint4_t			    line_num,
	uint4_t			    err_num,
	w_error_t*		    list = 0);
    static w_error_t*		make(
	const char* const 	    filename,
	uint4_t			    line_num,
	uint4_t			    err_num,
	uint4_t			    sys_err,
	w_error_t*		    list = 0);
    static bool			insert(
	const char		    *modulename,
	const info_t		    info[],
	uint4_t			    count);

    NORET			W_FASTNEW_CLASS_DECL(w_error_t);

    static w_error_t*		no_error;
    static const char* const	error_string(uint4_t err_num);
    static const char* const	module_name(uint4_t err_num);

    NORET			~w_error_t();

#ifdef __BORLANDC__
public:
#else
private:
#endif /* __BORLANDC__ */
    enum { max_range = 10, max_trace = 3 };
    

private:
    friend class w_rc_t;
    void			_incr_ref();
    void			_decr_ref();
    void			_dangle();
    uint4_t			_ref_count;
				     
    uint4_t			_trace_cnt;
    w_error_t*			_next;
    const char*			_trace_file[max_trace];
    uint4_t			_trace_line[max_trace];

    NORET			w_error_t(
	const char* const 	    filename,
	uint4_t			    line_num,
	uint4_t			    err_num,
	w_error_t*		    list);
    NORET			w_error_t(
	const char* const 	    filename,
	uint4_t			    line_num,
	uint4_t			    err_num,
	uint4_t			    sys_err,
	w_error_t*		    list);
    NORET			w_error_t(const w_error_t&);
    w_error_t&			operator=(const w_error_t&);

    static const info_t*	_range_start[max_range];
    static uint4_t		_range_cnt[max_range];
    static const char *		_range_name[max_range];
    static uint4_t		_nreg;

    static inline uint4_t	classify(int err_num);
public:
	// make public so it  can be exported to client side
    static const info_t		error_info[];
    static ostream & 		print(ostream &out);
private:
	// disabled
	static void init_errorcodes(); 
};

extern ostream  &operator<<(ostream &o, const w_error_t &obj);

inline void
w_error_t::_incr_ref()
{
    ++_ref_count;
}

inline void
w_error_t::_decr_ref()
{
    if (--_ref_count == 0 && this != no_error)  _dangle();
}

inline NORET
w_error_t::~w_error_t()
{
}

/* XXX automagic generation of this would be nice */
const w_base_t::uint4_t fcINTERNAL	= w_error_t::fcINTERNAL;
const w_base_t::uint4_t fcOS		= w_error_t::fcOS;
const w_base_t::uint4_t fcFULL		= w_error_t::fcFULL;
const w_base_t::uint4_t fcEMPTY		= w_error_t::fcEMPTY;
const w_base_t::uint4_t fcOUTOFMEMORY	= w_error_t::fcOUTOFMEMORY;
const w_base_t::uint4_t fcNOTFOUND	= w_error_t::fcNOTFOUND;
const w_base_t::uint4_t fcNOTIMPLEMENTED= w_error_t::fcNOTIMPLEMENTED;
const w_base_t::uint4_t fcREADONLY	= w_error_t::fcREADONLY;
const w_base_t::uint4_t fcMIXED		= w_error_t::fcMIXED;
const w_base_t::uint4_t fcFOUND		= w_error_t::fcFOUND;
const w_base_t::uint4_t fcNOSUCHERROR	= w_error_t::fcNOSUCHERROR;
const w_base_t::uint4_t fcWIN32		= w_error_t::fcWIN32;
const w_base_t::uint4_t fcASSERT	= w_error_t::fcASSERT;

/*<std-footer incl-file-exclusion='W_ERROR_H'>  -- do not edit anything below this line -- */

#endif          /*</std-footer>*/

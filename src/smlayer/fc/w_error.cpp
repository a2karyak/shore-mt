/*<std-header orig-src='shore'>

 $Id: w_error.cpp,v 1.56 1999/12/11 03:04:10 bolo Exp $

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

#include "w_defines.h"

/*  -- do not edit anything above this line --   </std-header>*/

#ifdef __GNUC__
#pragma implementation "w_error.h"
#endif

#include <unix_error.h>
#ifdef _WIN32
#include <win32_error.h>
#endif

#include <string.h>

#define W_SOURCE
#include <w_base.h>

W_FASTNEW_STATIC_DECL(w_error_t, 32);

const
#include <fc_einfo_gen.h>

//
// Static equivalent of insert(..., error_info, ...)
//
const 
w_error_t::info_t*	w_error_t::_range_start[w_error_t::max_range] = {
    w_error_t::error_info, 0 
};
w_base_t::uint4_t	w_error_t::_range_cnt[w_error_t::max_range] = {
	fcERRMAX - fcERRMIN + 1, 0
};

const char *w_error_t::_range_name[w_error_t::max_range]= { 
	"Foundation Classes",
	0
};
w_base_t::uint4_t	w_error_t::_nreg = 1;

static char		no_space[sizeof(w_error_t)] = {0};
w_error_t*		w_error_t::no_error = (w_error_t*) no_space;

/*** not used ***
static void w_error_t_error_not_checked()
{
}
*/

static void w_error_t_no_error_code()
{
}

#ifdef W_DEBUG
#define CHECKIT do {\
    w_error_t*	my = _next; \
    w_error_t*	p = my; \
    while(p) { \
	if (p == p->_next || my == p->_next) { \
		cerr << "Recursive error detected:" << endl << *this << endl;\
		W_FATAL(fcINTERNAL); \
	} \
	p = p->_next; \
    } \
  } while(0)

#else
#define CHECKIT
#endif

void 
w_error_t::_dangle()
{
    if (this == no_error)
    	return;

    register w_error_t* pp = 0;
    for (register w_error_t* p = _next; p; p = pp)  {
	pp = p->_next;
	if (pp == p || pp == this) {
		cerr << "Recursive error detected:" << endl << *this << endl;
		W_FATAL(fcINTERNAL);
	}
	delete p;
    }

    delete this;
}

w_error_t&
w_error_t::add_trace_info(
    const char* const	filename,
    uint4_t		line_num)
{
    if (_trace_cnt < max_trace)  {
	_trace_file[_trace_cnt] = filename;
	_trace_line[_trace_cnt] = line_num;
	++_trace_cnt;
    }

    return *this;
}

w_error_t&
w_error_t::set_more_info_msg(const char* more_info)
{
    delete[] VCPP_BUG_DELETE_CONST more_info_msg;
    more_info_msg = more_info;
    return *this;
}

w_error_t&
w_error_t::append_more_info_msg(const char* more_info)
{
    if (more_info)  {
	if (more_info_msg)  {
	    int more_info_len = strlen(more_info);
	    int more_info_msg_len = strlen(more_info_msg);
	    char* new_more_info_msg = new char[more_info_len + more_info_msg_len + 2];
	    strcpy(new_more_info_msg, more_info_msg);
	    new_more_info_msg[more_info_msg_len] = '\n';
	    strcpy(new_more_info_msg + more_info_msg_len + 1, more_info);
	    new_more_info_msg[more_info_msg_len + more_info_len + 1] = 0;
	    delete[] VCPP_BUG_DELETE_CONST more_info_msg;
	    more_info_msg = new_more_info_msg;
	}  else  {
	    set_more_info_msg(more_info);
	}
    }

    return *this;
}

const char*
w_error_t::get_more_info_msg()
{
    return more_info_msg;
}

/* automagically generate a sys_err_num from an errcode */
inline w_base_t::uint4_t w_error_t::classify(int er)
{
	uint4_t	sys = 0;
	switch (er) {
	case fcOS:
		sys = last_unix_error();
		break;
#ifdef _WIN32
	case fcWIN32:
		sys = last_win32_error();
		break;
#endif
	}
	return sys;
}

inline
w_error_t::w_error_t(const char* const	fi,
		     uint4_t		li,
		     uint4_t		er,
		     w_error_t*		list,
		     const char*	more_info)
: err_num(er),
  file(fi),
  line(li), 
  sys_err_num(classify(er)),
  more_info_msg(more_info),
  _ref_count(0),
  _trace_cnt(0),
  _next(list)
{
    CHECKIT;
}

w_error_t*
w_error_t::make(
    const char* const	filename,
    uint4_t		line_num,
    uint4_t		err_num,
    w_error_t*		list,
    const char*		more_info)
{
    return new w_error_t(filename, line_num, err_num, list, more_info);
}

inline NORET
w_error_t::w_error_t(
    const char* const	fi,
    uint4_t		li,
    uint4_t		er,
    uint4_t		sys_er,
    w_error_t*		list,
    const char*		more_info)
    : err_num(er),
      file(fi), line(li), 
      sys_err_num(sys_er),
      more_info_msg(more_info),
      _ref_count(0),
      _trace_cnt(0),
      _next(list)
{
    CHECKIT;
}

w_error_t*
w_error_t::make(
    const char* const	filename,
    uint4_t		line_num,
    uint4_t		err_num,
    uint4_t		sys_err,
    w_error_t*		list,
    const char*		more_info)
{
    return new w_error_t(filename, line_num, err_num, sys_err, list, more_info);
}

bool
w_error_t::insert(
    const char *        modulename,
    const info_t	info[],
    uint4_t		count)
{
    if (_nreg >= max_range)
	return false;

    uint4_t start = info[0].err_num;

    for (uint4_t i = 0; i < _nreg; i++)  {
	if (start >= _range_start[i]->err_num && start < _range_cnt[i])
	    return false;
	uint4_t end = start + count;
	if (end >= _range_start[i]->err_num && end < _range_cnt[i])
	    return false;
    }
    _range_start[_nreg] = info;
    _range_cnt[_nreg] = count;
    _range_name[_nreg] = modulename;

    ++_nreg;
    return true;
}

const char* const
w_error_t::error_string(uint4_t err_num)
{
    if(err_num ==  w_error_t::no_error->err_num ) {
	return "no error";
    }
    uint4_t i;
    for (i = 0; i < _nreg; i++)  {
	if (err_num >= _range_start[i]->err_num && 
	    err_num <= _range_start[i]->err_num + _range_cnt[i]) {
	    break;
	}
    }
    
    if (i == _nreg)  {
	w_error_t_no_error_code();
	return error_string( fcNOSUCHERROR );
	// return "unknown error code";
    }

    const uint4_t j = CAST(int, err_num - _range_start[i]->err_num);
    return _range_start[i][j].errstr;
}

const char* const
w_error_t::module_name(uint4_t err_num)
{
    if(err_num ==  w_error_t::no_error->err_num ) {
	return "all modules";
    }
    uint4_t i;
    for (i = 0; i < _nreg; i++)  {
	if (err_num >= _range_start[i]->err_num && 
	    err_num <= _range_start[i]->err_num + _range_cnt[i]) {
	    break;
	}
    }
    
    if (i == _nreg)  {
	return "unknown module";
    }
    return _range_name[i];
}


ostream& w_error_t::print_error(ostream &o) const
{
    if (this == w_error_t::no_error) {
	return o << "no error";
    }

    int cnt = 1;
    for (const w_error_t* p = this; p; p = p->_next, ++cnt)  {

	const char* f = strrchr(p->file, '/');
	f ? ++f : f = p->file;
	o << cnt << ". error in " << f << ':' << p->line << " ";
	if(cnt > 1) {
	    if(p == this) {
	    	o << "Error recurses, stopping" << endl;
	    	break;
	    } 
	    if(p->_next == p) {
	    	o << "Error next is same, stopping" << endl;
		break;
	    }
	}
	if(cnt > 20) {
	    o << "Error chain >20, stopping" << endl;
	    break;
	}
	o << p->error_string(p->err_num);
	o << " [0x" << hex << p->err_num << dec << "]";

	/* Eventually error subsystems will have their own interfaces
	   here. */
	switch (p->err_num) {
	case fcOS: {
    	    char buf[1024];
	    format_unix_error(p->sys_err_num, buf, sizeof(buf));
	    o << " --- " << buf;
	    break;
        } 
#ifdef _WIN32
	case fcWIN32: {
    	    char buf[1024];
	    format_win32_error(p->sys_err_num, buf, sizeof(buf));
	    o << " --- " << buf;
	    break;
        }
#endif
	}

	o << endl;

	if (more_info_msg)  {
	    o << "\tadditional information: " << more_info_msg << endl;
	}

	if (p->_trace_cnt)  {
	    o << "\tcalled from:" << endl;
	    for (unsigned i = 0; i < p->_trace_cnt; i++)  {
		f = strrchr(p->_trace_file[i], '/');
		f ? ++f : f = p->_trace_file[i];
		o << "\t" << i << ") " << f << ':' 
		  << p->_trace_line[i] << endl;
	    }
	}
    }

    return o;
}

ostream &operator<<(ostream &o, const w_error_t &obj)
{
	return obj.print_error(o);
}

ostream &
w_error_t::print(ostream &out)
{
    for (unsigned i = 0; i < _nreg; i++)  {
	unsigned int first	= _range_start[i]->err_num;
	unsigned int last	= first + _range_cnt[i] - 1;

	for (unsigned j = first; j <= last; j++)  {
		const char *c = module_name(j);
		const char *s = error_string(j);

	    out <<  c << ":" << j << ":" << s << endl;
	}
    }
    
    return out;
}


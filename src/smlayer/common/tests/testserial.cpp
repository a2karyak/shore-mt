/*<std-header orig-src='shore'>

 $Id: testserial.cpp,v 1.11 1999/06/07 19:02:38 kupsch Exp $

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

#undef BITS64 

#define MaxTestInc 5
#define MXDIFF (MaxTestInc+3)

#include <w_strstream.h>
#include <basics.h>
#include <serial_t.h>
#include <assert.h>

#define P(z)  pp(#z,z); 

#define PLN(z)  pp(#z,z); cout << endl;

void
pp(const char *str, const serial_t &z) 
{
	cout << "["  << str << "=" << z ;
	if(z.is_null()) {
		cout << ",null" ;
	}
	if(z.is_remote()) {
		cout << ",remote" ;
	}
	if(z.is_local()) {
		cout << ",local" ;
	}
	assert(z.is_remote() == !(z.is_local()));
	if(z.is_on_disk()) {
		cout << ",on-disk" ;
	}
	if(z.is_in_memory()) {
		cout << ",in_memory" ;
	}
	assert(z.is_in_memory() == !(z.is_on_disk()));
	cout << "]";
}

#define EQUIV(a,b)\
	if(a.equiv(b)) {\
		pp(#a,a); cout<<"equiv"; pp(#b,b);cout<<endl; \
	} else {\
		pp(#a,a); cout<<" !equiv "; pp(#b,b);cout<<endl; \
	}

#define CMP(a,op,b)\
	if(a op b) {\
		pp(#a,a); cout << #op; pp(#b,b); cout << endl;\
	} else {\
		pp(#a,a); cout << "!" << #op; pp(#b,b); cout << endl;\
	}

void
compare(
	const serial_t &a,
	const serial_t &b
)
{
	EQUIV(a,b);
	EQUIV(b,a);
	CMP(a,==,b);
	CMP(a,!=,b);
	if(a.is_remote() ) return;
	if(b.is_remote() ) return;
	CMP(a,<=,b);
	CMP(a,>=,b);
	CMP(a,>,b);
	CMP(a,<,b);
}
void
compare(
	const serial_t_data &a,
	const serial_t &b
)
{
	CMP(a,==,b);
	CMP(a,!=,b);
	if(b.is_remote() ) return;
	CMP(a,<=,b);
	CMP(a,>=,b);
	CMP(a,>,b);
	CMP(a,<,b);
}
void
compare(
	const serial_t &a,
	const serial_t_data &b
)
{
	CMP(a,==,b);
	CMP(a,!=,b);
	if(a.is_remote() ) return;

	CMP(a,<=,b);
	CMP(a,>=,b);
	CMP(a,>,b);
	CMP(a,<,b);
}
#define COMPARE(a,b) {\
	cout << "BEGIN { comparisons at "<< __LINE__ << " :" << __FILE__ << endl;\
	compare(a,b);\
	cout << "}" << endl;\
}

#ifdef _WINDOWS
unsigned long hex (long j)  { return (unsigned long) j; }
int dec (long j)  { return (int) j; }
#endif

int
main()
{
	int i;

	cout << "other_bits          " << hex((long)serial_t::other_bits) << endl;
	cout << "max_any             " << hex((long)serial_t::max_any) << endl;
	cout << "overflow_shift_bits " << dec((long)serial_t::overflow_shift_bits) << endl;
	cout << "max_inc             " << hex((long)serial_t::max_inc) << endl;
	cout << "mask_remote         " << hex((long)serial_t::mask_remote) << endl;
	cout << "expect overflow after " << dec((long)(serial_t::max_any<<1)+1) << endl;

#ifdef notdef
	serial_t xnull;

	PLN(xnull);

	serial_t xstartwith8(8,false);
	PLN(xstartwith8);
	COMPARE(xnull,xstartwith8);

	serial_t x8remote(8,true);
	PLN(x8remote);
	COMPARE(x8remote,xstartwith8);

	serial_t x8local(8,false);
	PLN(x8local);
	COMPARE(x8remote,x8local);
	COMPARE(x8local,xstartwith8);

	serial_t x8(8,false);
	serial_t x8copy(x8);
	PLN(x8);
	PLN(x8copy);
	COMPARE(x8,x8copy);
	COMPARE(x8local,x8copy);
	COMPARE(x8remote,x8copy);

	serial_t_data	g8  = {  8
#ifdef BITS64
		, 8
#endif
	};
	serial_t xg(g8);
	PLN(xg);
	COMPARE(xg,g8);
	COMPARE(g8,xg);
#endif /* notdef */

	// g-l-over should overflow into the high word but not overflow
	// completely
	static serial_t_data	glover  = { 
#ifdef BITS64
		serial_t::max_any-MXDIFF, // don't overflow the whole thing
		~0 - MXDIFF
#else
		(serial_t::max_any<<1) - MXDIFF
#endif
	};
	serial_t	lowover(glover);
	PLN(lowover);
	for(i=0; i<MaxTestInc; i++) {
		cout << "inc lowover by " << i << " ";
		if(lowover.increment((w_base_t::uint4_t)i)) {
			cout << "OVERFLOWED";
		} else {
			cout << "did not overflow";
		}
		cout << "; new value = ";
		PLN(lowover);
	}

	// g-h-over should overflow the high word 

	static serial_t_data	ghover  = {
#ifdef BITS64
		serial_t::max_any, 
		~0 - MXDIFF
#else
		(serial_t::max_any<<1) - MXDIFF
#endif
	};
	serial_t	highover(ghover);
	serial_t	hosave(highover);
	PLN(highover);
	for(i=0; i<MaxTestInc; i++) {
		cout << "inc highover by " << i << " ";
		if(highover.increment((w_base_t::uint4_t)i)) {
			cout << "OVERFLOWED";
		} else {
			cout << "no overflow";
		}
		cout << ":";
		PLN(highover);
	}
#ifdef notdef
	COMPARE(xg,glover);
	COMPARE(glover,xg);

	COMPARE(xg,ghover);
	COMPARE(ghover,xg);
#endif

	COMPARE(highover,hosave);
	COMPARE(highover,lowover);


	serial_t hi_local(serial_t::max_local);

	if( ! hi_local.increment((w_base_t::uint4_t)1)) {
		cout << "at line " << __LINE__ << " :" << __FILE__ 
		<< " ERROR - hi-local did not overflow" << endl;
	} 

	serial_t hi_remote(serial_t::max_remote);

	if( ! hi_remote.increment((w_base_t::uint4_t)1)) {
		cout << "at line " << __LINE__ << " :" << __FILE__ 
		<< " ERROR - hi-remote did not overflow" << endl;
	}

	return 0;
}


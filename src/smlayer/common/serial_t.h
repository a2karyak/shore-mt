/*<std-header orig-src='shore' incl-file-exclusion='SERIAL_T_H'>

 $Id: serial_t.h,v 1.60 2001/06/26 16:48:35 bolo Exp $

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

#ifndef SERIAL_T_H
#define SERIAL_T_H

#include "w_defines.h"

/*  -- do not edit anything above this line --   </std-header>*/

#ifdef __GNUG__
#pragma interface
#endif

/*
 * NB -- THIS FILE MUST BE LEGITIMATE INPUT TO cc and RPCGEN !!!!
 * Please do as follows:
 * a) keep all comments in traditional C style.
 * b) If you put something c++-specific make sure it's 
 * 	  got ifdefs around it
 */
#ifndef DUAL_ASSERT_H
#include "dual_assert.h"
#endif
#ifndef SERIAL_T_DATA_H
#include "serial_t_data.h"
#endif

/*
//short logical record ID (serial number)
*/
#define SERIAL_T
struct serial_t {
    
	/* The type of the only data member of serial_t is defined elsewhere */
	/* so that each level of the Shore software can wrap the data member */
	/* with its own definition, be it a class, struct, or union. 		 */

    /* NB: when serial_t expands to 8 bytes, rather than doing this:
		serial_t_data low, high
	 * do this:
		expand the type "serial_t_data"
	 */
    serial_t_data data;

#define serial_t_guts serial_t_data
#define guts data

#ifdef __cplusplus

#  	ifdef HP_CC_BUG_1
        static const uint4_t mask_remote;
	static const uint4_t max_inc;
	static const uint4_t max_any;
	static const uint4_t other_bits;
#	endif /* HP_CC_BUG_1 */

    enum {
		mask_zero = 0x0, 		   // no bits set
		mask_disk = 0x1, 		   // set --> on-disk form; this bit is 0 if
								   // in-memory reference

#  	ifndef HP_CC_BUG_1
		mask_remote = 0x80000000,  // set --> off-volume-reference
#	endif /* !HP_CC_BUG_1 */

#ifdef BITS64
#	define OVERFLOWBITS 31
#else
#	define OVERFLOWBITS 30
#endif

		overflow_shift_bits = OVERFLOWBITS
#  	ifndef HP_CC_BUG_1
		,
		max_inc = ((1<<overflow_shift_bits)-1),
		max_any = max_inc,
		other_bits = ~max_inc
#	endif /* !HP_CC_BUG_1 */
	};

// note: due to redefinition of OCRef, we should not need OBJECT_CACHE
// ifdef any more.

    /*
     * Warning: do not create a constructor that only takes one
     * basic C type as a parameter.  Otherwise, the compiler
     * may generate unwanted type casting (using temps) which can cause
     * major problems with functions having non-const reference
     * parameters of type serial_t
     */

		/* makes a null serial_t */
    serial_t( bool ondisk=true)
		{ 
#			ifdef BITS64
			data._high = 0;
#			endif
			data._low = ondisk?mask_disk:mask_zero;
		} 

	/* NB: the next constructor is for use by the storage manager 
	 * -- it shifts the given value around and sets bits indicating
	 * on/off-volume-reference, on-disk or in-memory, etc.
	 */
    serial_t(uint4_t start, bool remote)
        /* use middle 30 bits, low bit to 1, high bit to remote */
        {	
			/*
			// carefully written so that it works whether data
			// has _low only or _low and _high...
			*/
#			ifdef BITS64
			data._high = 0;
#			endif

			data._low = (start << 1) | mask_disk;

#			ifdef BITS64
			data._high |= remote ? mask_remote : mask_zero; 
#			else
			data._low |= remote ? mask_remote : mask_zero; 
#			endif
		}

    serial_t(const serial_t& s) 	{ *this = s; } 

    serial_t(const serial_t_data& g) 	{ 
		data = g; // struct copy
	} 

private:
	bool _incr(
		uint4_t		*what,
		uint4_t		amt,
		uint4_t		*overflow
	);

public:
	// return value true indicates overflow
    bool increment(uint4_t amount); // also decrements 

    bool is_remote() 	const {return (data.high() & mask_remote)==mask_remote;} 
    bool is_local()  	const {return (data.high() & mask_remote)==0; }
    bool is_on_disk()	const {return (data._low  & mask_disk)==mask_disk; }
    bool is_in_memory() const {return (data._low  & mask_disk)==0; }

	// This is a little flaky, but there *are* places where
	// we need to ignore the lowest bit (on-disk), e.g,.
	// to tell if the value is otherwise nil.
    bool equiv(const serial_t &s) const {
		// remote is never == local
		return
#ifdef BITS64
			(data._high==s.data._high) &&
#endif
		((data._low  & ~mask_disk)==(s.data._low & ~mask_disk));
	}
    bool is_null()    const { return this->equiv(serial_t::null); }

/**************  ASSIGNMENT  *****************************************/

#ifdef __GNUC__
		 // serial_t  =  serial_t 
    serial_t& operator=(const serial_t& t)    {
		data = t.data; // struct copy
        return *this;
    }
#endif
		 // serial_t  =  data 
    serial_t& operator=(const serial_t_data &g) {
	    this->data = g; // struct copy
		return  *this;
	}

	void set(const serial_t& t)
	{ data = t.data; }

	void set(const serial_t_data &g)
	{ data = g; }

/**************  COMPARISONS *****************************************/

/*  
 *  NB: if you want to compare a serial# in on-disk form
 *  with one in in-memory form, use the method equiv() rather
 *  than the operator ==
 *  (equiv ignores the lowest bit)
 */

// == 

		 // compare 2 serial_ts 
    bool operator==(const serial_t& s) const { 
		return (data._low == s.data._low)
#ifdef BITS64
			&& (data._high==s.data._high) 
#endif
		;
	}
		 // compare a serial_t  with a data 
    bool operator==(const serial_t_data& s) const { 
		return (data._low == s._low)
#ifdef BITS64
		 	&& (data._high==s._high) 
#endif
		;
	}
		// and... to make the above operator commutative:
    inline friend bool operator==(const serial_t_data& g, const serial_t&s) { 
		return s == g;
	}

// != 

		 // compare 2 serial_ts 
    bool operator!=(const serial_t& s) const { 
		return !(*this==s);
	}
		 // compare a serial_t  with a data 
    bool operator!=(const serial_t_data& s) const { 
		return !(*this==s);
	}
		// and... to make it commutative:
    inline friend bool operator!=(const serial_t_data& g, const serial_t&s) {  
		return !(s==g); 
	}

	/*
	// Noone should ever compare remote refs with local refs
	// except for equality comparisons, so we include an assertion
	// in the following operators.
	//
	// First we have operators for 2 serial_ts, then operators
	// for comparing a serial_t and a data.
	*/

// <= 
		 // compare 2 serial_ts 
    bool operator<=(const serial_t& s) const { 	
			//
			// using asserts in here creates problem with
			// HP CC when this file is run through rpcgen.
			//
			// dual_assert3(is_remote() == s.is_remote()); 
			return  
#ifdef BITS64
			(data._high < s.data._high) || ((data._high==s.data._high) && 
#endif
				(data._low <= s.data._low)
#ifdef BITS64
				)
#endif
			;
		}

		 // compare a serial_t  with a data 
    bool operator<=(const serial_t_data& g) const { 	
			return
#ifdef BITS64
			(data._high < g._high) || ((data._high==g._high) && 
#endif
				(data._low <= g._low)
#ifdef BITS64
				)
#endif
			;
		}

// < 
		 // compare 2 serial_ts 
    bool operator<(const serial_t& s) const { 
			//
			// using asserts in here creates problem with
			// HP CC when this file is used in a template
			// (at least, it happens in vas template code)
			//
			// dual_assert3(is_remote() == s.is_remote()); 
			return 
#ifdef BITS64
			(data._high < s.data._high) || ((data._high==s.data._high) && 
#endif
				(data._low < s.data._low)
#ifdef BITS64
				)
#endif
			;
		}
		 // compare a serial_t  with a data 
    bool operator<(const serial_t_data& g) const { 	
			return
#ifdef BITS64
			(data._high < g._high) || ((data._high==g._high) && 
#endif
				(data._low < g._low)
#ifdef BITS64
				)
#endif
			;
		}

// >= 
		 // compare 2 serial_ts 
    bool operator>=(const serial_t& s) const { 
			return ! ((*this) < s);
		}
		 // compare a serial_t  with a data 
    bool operator>=(const serial_t_data& g) const { 	
			return ! ((*this) < g);
		}
// > 
		 // compare 2 serial_ts 
    bool operator>(const serial_t& s) const { 
			return ! ((*this) <= s);
		}
		 // compare a serial_t  with a data 
    bool operator>(const serial_t_data& g) const { 	
			return ! ((*this <= g));
		}

		// and... to make the above 4 serial_t/data operators commutative:
    inline friend bool operator<=(const serial_t_data& g, const serial_t&s) 
		{  return (s >= g); }
    inline friend bool operator>=(const serial_t_data& g, const serial_t&s) 
		{  return (s <= g); }
    inline friend bool operator<(const serial_t_data& g, const serial_t&s) 
		{  return (s > g); }
    inline friend bool operator>(const serial_t_data& g, const serial_t&s) 
		{  return (s < g); }

/* INPUT and OUTPUT */
    friend ostream& operator<<(ostream&, const serial_t& s);
    friend istream& operator>>(istream&, serial_t& s);

    friend istream& operator>>(istream&, serial_t_data& g);
    friend ostream& operator<<(ostream&, const serial_t_data& g);

/* CONSTANTS */
    /* defined in serial_t.cpp: */
	/* all of the following are in on-disk form: */
	static const serial_t max_local;
	static const serial_t max_remote;
    static const serial_t null;

    /* this is the key descriptor for using serial_t's as btree keys */
    static const char* key_descr; 
#endif /* __cplusplus */
};

#ifdef __cplusplus
inline w_base_t::uint4_t w_hash(const serial_t& s)
{
#ifdef BITS64
	return s.data._low;  // this is reasonable since _low changes a lot
#else
	return s.data._low;
#endif
}
#endif /* __cplusplus */

/*<std-footer incl-file-exclusion='SERIAL_T_H'>  -- do not edit anything below this line -- */

#endif          /*</std-footer>*/

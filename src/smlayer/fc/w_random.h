/*<std-header orig-src='shore' incl-file-exclusion='W_RANDOM_H'>

 $Id: w_random.h,v 1.11 2000/02/01 23:33:11 bolo Exp $

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

#ifndef W_RANDOM_H
#define W_RANDOM_H

#include "w_defines.h"

/*  -- do not edit anything above this line --   </std-header>*/

#ifdef __GNUG__
#pragma interface 
#endif 

#include <w.h>

class istream;
class ostream;

class random_generator {

private:
    unsigned short *	get_seed() const;
    static unsigned short _original_seed[3];
    static bool 	_constructed;
#ifdef _WINDOWS
    static unsigned short  ncalls; // for seed maintenance
#endif

public:
    int 	mrand_choice(int lower, int higher) const; // return an
			// int in the range [lower, higher)

    double 	drand() const;
    w_base_t::uint8_t	qrand() const;
    unsigned int lrand() const;
    int 	mrand() const;

    /* some compatibility methods */
    void 	srand(int seed);
    int 	rand() const; // may be negative

    friend ostream& operator<<(ostream&, const random_generator&);
    friend istream& operator>>(istream&, random_generator&);

public:
    NORET
    random_generator()  
    { 
	// Disallow a 2nd random-number generator
	if(!_constructed) {
	    *_original_seed = *get_seed(); 
	    _constructed = true;
	}
#ifdef _WINDOWS
        ncalls = 0;
#endif
    }
    // For writing the current seed to a file
    // and picking it up from a file 
    void read(const char *fname);
    void write(const char *fname) const;

    static random_generator generator; // in w_random.cpp
};

/*<std-footer incl-file-exclusion='W_RANDOM_H'>  -- do not edit anything below this line -- */

#endif          /*</std-footer>*/

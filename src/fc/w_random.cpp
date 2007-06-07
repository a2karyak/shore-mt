/*<std-header orig-src='shore'>

 $Id: w_random.cpp,v 1.12 2007/05/18 21:38:25 nhall Exp $

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

#ifdef __GNUG__
#pragma implementation "w_random.h"
#endif 
#include "w_random.h"
#include <w_debug.h>

random_generator random_generator::generator; 

bool random_generator::_constructed=false;
unsigned short random_generator::_original_seed[3];
#ifdef _WINDOWS
unsigned short random_generator::ncalls = 0;
#endif

#ifndef _WINDOWS
extern "C" {
/* from the man page:
     drand48() returns non-negative double-precision
     floating-point values uniformly distributed over the inter-
     val [0.0, 1.0).

     lrand48() returns non-negative long integers
     uniformly distributed over the interval (0, ~2**31).

     mrand48() returns signed long integers uni-
     formly distributed over the interval [-2**31 ~2**31).
*/
// stdlib declares these (with different exceptions, depending on compiler)
    // double drand48();
    // long lrand48(); // non-negative
    // long mrand48(); // may be negative

    // void srand48(long ); // compatibility with ::srand(int)
			// to change the seed

    // unsigned short *seed48( unsigned short arg[3] );

}
#endif
#ifdef _WINDOWS
#define SRAND48(arg) ::srand((unsigned int)(arg))
#define LRAND48() (long) ::rand()
#define MRAND48() (long) ::rand()
#define SEED48(arg)  w_assert1(0);
#else
#define SRAND48(arg) ::srand48((long) arg)
#define LRAND48() ::lrand48()
#define MRAND48() ::mrand48()
#define DRAND48() ::drand48()
#define SEED48(arg)  ::seed48(arg)
#endif

unsigned short *
random_generator::get_seed() const
{
#ifdef _WINDOWS
    return &random_generator::ncalls;
#else
    unsigned short dummy[3] = {0x0,0x0,0x0};
    unsigned short *result = SEED48(dummy);
    (void) SEED48(result);
    return result;
#endif
}

ostream& 
operator<<(ostream& o, const random_generator& )
{
#ifdef _WINDOWS
    o << random_generator::ncalls;
#else
    /*
     * expect "........,........,........"
     * no spaces
     */

    unsigned short dummy[3] = {0x0,0x0,0x0};
    unsigned short *result = SEED48(dummy);

    o << 
	result[0] << "," << 
	result[1] << "," << 
	result[2] << endl;
    (void) SEED48(result);
#endif
    return o;
}

istream&
operator>>(istream& i, random_generator& )
{
#ifdef _WINDOWS
    i >> random_generator::ncalls;
#else
    /*
     * print "0x........,0x........,0x........"
     */
    unsigned short dummy[3];
    char	comma = ',';
    unsigned 	j=0;

// DBG(<<"rdstate: " << i.rdstate() <<" bad_bit: " << i.bad() <<" fail_bit: " << i.fail());
    while( (comma == ',') && 
	(j < sizeof(dummy)/sizeof(unsigned short)) &&
	(i >>  dummy[j])
	) {
// DBG( <<"rdstate: " << i.rdstate() <<" bad_bit: " << i.bad() <<" fail_bit: " << i.fail());
	if(i.peek() == ',') i >> comma;
// DBG( <<"rdstate: " << i.rdstate() <<" bad_bit: " << i.bad() <<" fail_bit: " << i.fail());
	j++;
    }
    if(j < sizeof(dummy)/sizeof(unsigned short) ) {
	// This actually sets the badbit:
	i.clear(ios::badbit|i.rdstate());
    }
    (void) SEED48(dummy);
#endif
    return i;
}

void
random_generator::srand(int seed)
{
#ifdef _WINDOWS
    ::srand(1);
    ncalls = 0;
    if(seed != 0) {
        while (ncalls++ < seed) {
            (void) rand();
	}
    }
#else
    if(seed==0) {
	// reset  it to the state at the
	// time the constructor was called
	(void) SEED48(_original_seed);
    }
    SRAND48(seed);
#endif
}

double
random_generator::drand() const
{
#ifdef _WINDOWS
    random_generator::ncalls++;
    int r = lrand();
    double d  = double (r);
    while(d > 1.0) d = d/10.0;
    return d;
#else
    return DRAND48();
#endif
}

int 
random_generator::mrand() const
{
#ifdef _WINDOWS
    random_generator::ncalls++;
#endif
    return MRAND48();
}

w_base_t::uint8_t
random_generator::qrand() const
{
	w_base_t::uint8_t	r = 0;
#ifdef _WINDOWS
	random_generator::ncalls++;
	r = (w_base_t::uint8_t) rand() << 32;
	r |= (long) rand();
#else
	r = (w_base_t::uint8_t)LRAND48() << 32;
	r |= LRAND48();
#endif
	return r;
}

unsigned int 
random_generator::lrand() const
{
#ifdef _WINDOWS
    random_generator::ncalls++;
    return (long) rand();
#else
    return LRAND48();
#endif
}

int 
random_generator::rand() const
{
    return mrand();
}


#include <fstream>

void
random_generator::read(const char *fname)
{
    ifstream f(fname);
    if (!f) {
      cerr << "Cannot read random seed file "
	    << fname
	    << endl;
    }
    f >> *this;
    f.close();
}

void
random_generator::write(const char *fname) const
{
    ofstream f(fname, ios::out);
    if (!f) {
      cerr << "Cannot write to file "
	    << fname
	    << endl;
    }
    f << *this;
    f.close();
}


int 	
random_generator::mrand_choice(int lower, int higher) const 
{
    // return an int in the range [lower, higher)
    double d  = drand();
    int m = higher - lower;
    m  = int(m * d);
    m += lower;
    return m;
}


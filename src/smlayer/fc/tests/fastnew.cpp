/*<std-header orig-src='shore'>

 $Id: fastnew.cpp,v 1.19 1999/06/07 19:03:03 kupsch Exp $

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

#include <w.h>

#include <w_random.h>
#include <unix_stats.h>

enum e_selection { randomnum, fast, regular };

class initializer {
public:
    void*	loc;
    NORET
    initializer() {
#ifdef SOLARIS2
	loc = sbrk(0);
#ifdef SOLARIS2
    W_FORM2(cout, 
    ("INIT: SBRK %#x, delta=%#d\n", (int)loc, (int)loc));
#endif
#endif
    }
    ~initializer() {
#ifdef SOLARIS2
    W_FORM2(cout, 
    ("END: SBRK %#x, delta=%#d\n", (int)sbrk(0), ((int)sbrk(0)-(int)loc)));
#endif
    }
};
static initializer __init;

class telem_t: public w_own_heap_base_t {
public:
    double d;	// use double to verify alignment
    char c[13];
private:
};
class txelem_t : public w_own_heap_base_t {
public:
    double d[300];	// > 1K < 65K
    char c[13];
private:
};

class tnelem_t : public w_own_heap_base_t {
public:
    double d[8192+10];	// > 65K
    char c[13];
private:
};

class felem_t {
public:
    double d;	// use double to verify alignment
    char c[13];
    W_FASTNEW_CLASS_PTR_DECL(felem_t);
private:
};
W_FASTNEW_STATIC_PTR_DECL(felem_t);

class fxelem_t {
public:
    double d[300];	// > 1K < 65K
    char c[13];
    W_FASTNEW_CLASS_PTR_DECL(fxelem_t);
private:
};
W_FASTNEW_STATIC_PTR_DECL(fxelem_t);

class fnelem_t {
public:
    double d[8192+10];	// > 65K
    char c[13];
    W_FASTNEW_CLASS_PTR_DECL(fnelem_t);
private:
};
W_FASTNEW_STATIC_PTR_DECL(fnelem_t);

class elem_t {
public:
    double d;	// use double to verify alignment
    char c[13];
private:
};
class xelem_t {
public:
    double d[300];	// > 1K < 65K
    char c[13];
private:
};

class nelem_t {
public:
    double d[8192+10];	// > 65K
    char c[13];
private:
};

unix_stats U;
int hiwat_i=0;
int hiwat_xi=0;
int hiwat_ni=0;
int flip=0;
int news=0;
int frees=0;

unsigned int randfunc() {
    unsigned int r;
    r = random_generator::generator.lrand();
    if(r & 0xa0000000) r = 0; else 
    if((r & 0x00f00000) == 0x00f00000) r = 2; else 
    if((r & 0x00030000) == 0x00030000) r = 4; else 
    if((r & 0x0000f000) == 0x0000f000) r = 1; else 
    if((r & 0x00000f00) == 0x00000f00) r = 3; else 
    r = 5;

    return r;
}
void
func(int numallocs, int iter, int  selection) // 0=do nothing; 1=fast, 2=regular
{
    random_generator::generator.srand(0); // reinitialize

#ifdef SOLARIS2
    void *start = sbrk(0);
#endif

    int i=0;
    int xi=0;
    int ni=0;


    U.start();
    while(--iter > 0) {

    w_assert1(i >= 0);
    w_assert1(ni >= 0);
    w_assert1(xi >= 0);

    switch(selection) {
    case (int) fast: {
	felem_t **pf = new felem_t* [hiwat_i+1]; 
	fxelem_t **pfx = new fxelem_t*[hiwat_xi+1];
	fnelem_t **pfn = new fnelem_t*[hiwat_ni+1];

	while(numallocs-- > 0) {
	    w_assert1(i >= 0);
	    w_assert1(ni >= 0);
	    w_assert1(xi >= 0);

	    switch(randfunc()) {
	    case 1:
		if(i > 0) {
		    delete pf[--i];
		    break;
		}
		// fall through
	    case 0:
		pf[i++] = new felem_t;
		break;

	    case 3:
		if(xi > 0) { 
		    delete pfx[--xi];
		    break;
		}
		// fall through
	    case 2:
		pfx[xi++] = new fxelem_t;
		break;

	    case 5:
		if(ni > 0) { 
		    delete pfn[--ni];
		    break;
		}
		// fall through
	    case 4:
		pfn[ni++] = new fnelem_t;
		break;
	    }
	}

	// w_factory_t::print_all(cout);

	while(ni > 0) delete pfn[--ni]; 
	while(xi > 0) delete pfx[--xi]; 
	while(i > 0) delete pf[--i]; 
        delete[] pfn;
        delete[] pfx;
        delete[] pf;
    } break;
    case (int) regular: {
	elem_t **p = new elem_t*[hiwat_i+1];
	xelem_t **px = new xelem_t*[hiwat_xi+1];
	nelem_t **pn = new nelem_t*[hiwat_ni+1];

	while(numallocs-- > 0) {
	    w_assert1(i >= 0);
	    w_assert1(ni >= 0);
	    w_assert1(xi >= 0);

	    switch(randfunc()) {
	    case 1:
		if(i > 0) { 
		    delete p[--i];
		    break; 
		}
		// fall through
	    case 0:
		p[i++] = new elem_t;
		break;

	    case 3:
		if(xi > 0) { 
		    delete px[--xi];
		    break;
		}
		// fall through
	    case 2:
		px[xi++] = new xelem_t;
		break;

	    case 5:
		if(ni > 0) {
		    delete pn[--ni];
		    break;
		}
		// fall through
	    case 4:
		pn[ni++] = new nelem_t;
		break;
	    }
	}
	while(ni > 0) delete pn[--ni]; 
	while(xi > 0) delete px[--xi]; 
	while(i > 0) delete p[--i]; 
	delete[] pn;
	delete[] px;
	delete[] p;
    } break;
    case (int) randomnum: {
	// try to factor out the time for random number generation
	while(numallocs-- > 0) {
	    w_assert1(i >= 0);
	    w_assert1(ni >= 0);
	    w_assert1(xi >= 0);

	    switch(randfunc()) {
#define HIWAT(i) hiwat_##i = i > hiwat_##i? i : hiwat_##i;
	    case 1:
		if(i > 0) { 
		    --i;
		    frees++;
		    break;
		}
		// drop through
	    case 0:
		i++; HIWAT(i);
		news++;
		break;

	    case 3:
		if(xi > 0) { 
		    --xi;
		    frees++;
		    break;
		}
		// drop through
	    case 2:
		xi++; HIWAT(xi);
		news++;
		break;

	    case 5:
		if(ni > 0) { 
		    --ni;
		    frees++;
		    break; 
		}
		// drop through
	    case 4:
		ni++; HIWAT(ni);
		news++;
		break;
	    
	    }
	}
	while(ni > 0) { --ni; frees++;}
	while(xi > 0) { --xi; frees++; }
	while(i > 0) { --i; frees++;}

    } break;
    case 4:
    case 200:
    case 500:
    case 345: {
	cerr << "force switch if you can" <<endl;
    } break;
    case 5:
    case 201:
    case 501:
    case 341: {
	cerr << "these dummy numbers are meant to trick"
		<< "the compiler into generating a switch"
		<< "statement" <<endl;
    } break;
	
    default: {
	cerr << "bad 2nd argument" <<endl;
    } break;
    }
    }
    U.stop(1); // 1 iteration

    cout << "Unix stats :" <<endl;
    cout << U << endl << endl;
    cout << flush;

#ifdef SOLARIS2
    { void *x = sbrk(0);
    W_FORM2(cout, ("FUNC: SBRK %#x, delta=%#d\n", x, ((int)x-(int)start)));
    }
#endif

}

int
main(int argc, const char *argv[])
{
#ifdef SOLARIS2
    W_FORM2(cout, 
    ("MAIN: SBRK %#x, delta=%#d\n", (int)sbrk(0), ((int)sbrk(0)-(int)__init.loc)));
#endif
    W_FASTNEW_STATIC_PTR_DECL_INIT(felem_t, 1000);
    W_FASTNEW_STATIC_PTR_DECL_INIT(fxelem_t, 100);
    W_FASTNEW_STATIC_PTR_DECL_INIT(fnelem_t, 10);
#ifdef SOLARIS2
    W_FORM2(cout, 
    ("MAIN-2: SBRK %#x, delta=%#d\n", (int)sbrk(0), ((int)sbrk(0)-(int)__init.loc)));
#endif

#ifndef NOTDEF
// test allocation of array of size 0
{
    int i;
    tnelem_t* p;
    for (i = 0; i < 10; i++)  {
	p = new tnelem_t[0];
	delete p;
    }
    cout << "done with test new[0]" <<endl;
}
#endif
#ifdef NOTDEF
{
    int i;
    tnelem_t* p;
    for (i = 0; i < 10; i++)  {
	p = new tnelem_t;
	p->d[0] = i;
	delete p;
    }
    cout << "done with tnelem_t test" <<endl;
}
#endif
#ifdef NOTDEF
    int i;
    fnelem_t* p;
    for (i = 0; i < 10; i++)  {
	p = new fnelem_t;
	p->d[0] = i;
	delete p;
    }

    fnelem_t* np[10];
    for (i = 0; i < 10; i++)  {
	np[i] = new fnelem_t;
    }

    for (i = 1; i < 10; i+= 2)  {
	delete np[i];
	np[i] = 0;
    }

    for (i = 0; i < 10; i+=2)  {
	delete np[i];
	np[i] = 0;
    }

    fnelem_t* pa = new fnelem_t[10];
    for (i = 0; i < 10; i++)  {
	pa[i].d[0] = i;
    }
    delete[] pa;
#endif


    int numallocs = 10000; // inner
    int iter = 10000; //outer
    int selection = 0;
    bool err = false;
    if(argc != 4) {
	err = true;
    } else {
	const char *a = argv[1];
	if(strcmp(a, "fast")==0) selection = fast;
	else if(strcmp(a, "regular")==0) selection = regular;
	else {
	    cerr << " bad first argument" <<endl; 
	    err = true;
	}
    }
    if(!err) {
	iter = atoi(argv[2]);
	if(iter <= 0) {
	    cerr << " <#iterations> must be > 0" << endl;
	    err = true;
	}
    }
    if(!err) {
	numallocs = atoi(argv[3]);
	if(numallocs <= 0) {
	    cerr << " <bound on #allocs per iteration> must be > 0" << endl;
	    err = true;
	}
    }
    if(err) {
	cerr << "usage: " << argv[0] 
	    << " fast|regular" 
	    << " <#iterations>" 
	    << " <bound on #allocs per iteration>"
	    <<endl;
	exit(1);
    }
    cout << "random number generation : " << numallocs << "*" << iter <<endl;
    func(numallocs, iter, 0);

    cout 
	<< "Start. "
	<< (const char *)(selection==fast?"fast":"regular")
	<< " : (max"
	<< numallocs 
	<< " alloc) * "
	<< iter 
	<< " iterations "
	<<endl;
    func(numallocs, iter, selection);

    cout << "Done. " <<endl;

    if (sizeof(felem_t) != sizeof(elem_t) ) {
	cout 
	    << "sizeof(felem_t) = " << sizeof(felem_t)
	    << "; sizeof(elem_t) = " << sizeof(elem_t)
	    << endl;
	cout 
	    << "sizeof(fxelem_t) = " << sizeof(fxelem_t)
	    << "; sizeof(xelem_t) = " << sizeof(xelem_t)
	    << endl;

	cout 
	    << "sizeof(fnelem_t) = " << sizeof(fnelem_t)
	    << "; sizeof(nelem_t) = " << sizeof(nelem_t)
	    << endl;
    }
    cout << "initial limit " << __init.loc <<endl;

    cout << "hiwat_i " << hiwat_i 
	<< "; size= " << sizeof(felem_t)
	<< "; SPACE= " << hiwat_i * sizeof(felem_t)
	<<endl;
    cout << "hiwat_xi " << hiwat_xi 
	<< "; size= " << sizeof(fxelem_t)
	<< "; SPACE= " << hiwat_xi * sizeof(fxelem_t)
	<<endl;
    cout << "hiwat_ni " << hiwat_ni 
	<< "; size= " << sizeof(fnelem_t)
	<< "; SPACE= " << hiwat_ni * sizeof(fnelem_t)
	<<endl;
    cout 
	<< "TOTAL SPACE= " << 
		(hiwat_i * sizeof(felem_t)) +
		(hiwat_xi * sizeof(fxelem_t)) +
		(hiwat_ni * sizeof(fnelem_t)) 
	<<endl;

    cout << news << "*" <<  iter << " NEW" <<endl;
    cout << frees << "*" << iter << " DELETE" <<endl;

    {
	vtable_info_array_t A;
	(void) w_factory_t::collect_histogram( A );
	A.operator<<(cout);
    }

    return 0;
}


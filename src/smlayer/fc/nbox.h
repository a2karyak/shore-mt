/*<std-header orig-src='shore' incl-file-exclusion='NBOX_H'>

 $Id: nbox.h,v 1.17 2001/10/13 17:46:21 bolo Exp $

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

#ifndef NBOX_H
#define NBOX_H

#include "w_defines.h"

/*  -- do not edit anything above this line --   </std-header>*/

#include <w_base.h>

class ostream;

//
// spatial object class: n-dimensional box 
//	represented as: xlow, ylow, zlow, xhigh, yhigh, zhigh
// 


#ifdef __GNUG__
#pragma interface
#endif

class nbox_t {
	friend ostream& operator<<(ostream& os, const nbox_t& box);

public:
        typedef w_base_t::int4_t int4_t;

	enum { max_dimension = 4 }; 
	static const int4_t	max_int4;
	static const int4_t	min_int4;

	// special marker for queries
	// This is not a box with area 0, but rather, a
	// non-box i.e., not a legit value
	static nbox_t& 	Null;


public:
	enum sob_cmp_t { t_exact = 1, t_overlap, t_cover, t_inside, t_bad };
	bool is_Null() const { return (dim == 0)?true:false; }

protected:
	int4_t	array[2*max_dimension];	// boundary points
	int	dim; 	 		// dimension
private:
	fill4	filler; 		// 8 byte alignment

	int4_t* box() { return array; }	// reveal internal storage

public:
	nbox_t(int dimension = max_dimension);
	nbox_t(int dimension, int box[]);
	nbox_t(const nbox_t& nbox);
	nbox_t(const char* s, int len);	// for conversion from tuple key 

	virtual ~nbox_t() {}

	int dimension() const	 { return dim; }
	int bound(int n) const	 { return array[n]; }
	int side(int n) const 	 { return array[n+dim]-array[n]; }
	int center(int n) const { return (array[n+dim]-array[n])/2+array[n]; }

	bool    empty() const;	// test if box is empty
	void    squared();	// make the box squared
	void    nullify();	// make the box empty (poor name)

	void    canonize();	// make it orthodox: 
				// first point is low in all
				// dimensions, 2nd is high in all dim.

	int hvalue(const nbox_t& universe, int level=0) const; // hilbert value
	int hcmp(const nbox_t& other, const nbox_t& universe, 
			int level=0) const; // hilbert value comparison

	void print(ostream &, int level) const;
	void draw(int level, ostream &DrawFile, const nbox_t& CoverAll) const;

	//
	// area of a box :
	//	>0 : valid box
	//	=0 : a point
	//	<0 : empty box 
	//
	double area() const;

	//
	// margine of a Rectangle
	//
	int margin();

	//
	// some binary operations:
	//	^: intersection  ->  box
	//	+: bounding box  ->  box (result of addition)
	//	+=: enlarge by adding the new box  
	//	==: exact match  ->  boolean
	//	/: containment   ->  boolean
	//	||: overlap	 ->  boolean
	//	>: bigger (comapre low values) -> boolean
	//	<: smaller (comapre low values) -> boolean
	//	*: square of distance between centers of two boxes 
	//
	nbox_t operator^(const nbox_t& other) const;
	nbox_t operator+(const nbox_t& other) const;

	nbox_t& operator+=(const nbox_t& other);
	nbox_t& operator=(const nbox_t& other);
	bool operator==(const nbox_t& other) const;
	bool operator/(const nbox_t& other) const;
    bool contains(const nbox_t& other) const { return *this / other; }
	bool operator||(const nbox_t& other) const;
	bool operator>(const nbox_t& other) const;
	bool operator<(const nbox_t& other) const;
	double operator*(const nbox_t& other) const;

	//
	// for tcl use only
	//
	nbox_t(const char* s);		// for conversion from ascii for tcl
	operator char*();	// conversion to ascii for tcl
	void put(const char*);  // conversion from ascii for tcl

	//
	// conversion between key and box
	//
	void  bytes2box(const char* key, int klen);
	const void* kval() const { return (void *) array; }
	int   klen() const { return 2*sizeof(int)*dim; }

};

inline nbox_t& nbox_t::operator=(const nbox_t &r)
{
	if (this != &r) {
		int i;
		dim = r.dim;
		for (i = 0; i < dim*2; i++)
			array[i] = r.array[i];
	}
	return *this;
}

/*<std-footer incl-file-exclusion='NBOX_H'>  -- do not edit anything below this line -- */

#endif          /*</std-footer>*/

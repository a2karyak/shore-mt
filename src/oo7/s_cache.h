#ifndef _S_CACHE_H_
#define _S_CACHE_H_

/*$Header: /p/shore/shore_cvs/src/oo7/s_cache.h,v 1.2 1994/02/04 19:21:43 nhall Exp $*/

struct cache_elt {
	void * obj_pt;
	Type * obj_type;
	void * save_objpt;
	// int filler; // to make it 16 bytes.
	int pin_count;
};

const MAX_OIDS = 100000;
// int last_oid = 1; //increment as allocated.

extern cache_elt cobjs[MAX_OIDS];

#endif

/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

//
// sdl_string.C
//

#ifdef __GNUG__
#pragma implementation
#endif


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ShoreApp.h"
#include "sdl_string.h"


void 
sdl_heap_base::__extend_space(size_t s)
// extend the existing heap space to size s; copy the existing contents.
// we (with some danger) use realloc.
{
	char * new_sp;
	assert(s>cur_size);
	if (free_it && space != 0)
	{
		new_sp = (char *)::realloc(space,s);
	}
	else
	{
		new_sp = (char *)malloc(s);
		memcpy(new_sp,space,cur_size);
#ifdef PURIFY
		if(purify_is_running()) {
			memset(new_sp, '\0', s);
		}
#endif
	}
	space = new_sp;
	cur_size = s;
	free_it = 1;
}

// constructors, except for default sdl_string::sdl_string(), defined
// in sdl_support.C.
sdl_string::sdl_string(const sdl_string &istring)
{
	sdl_heap_base::init();
	set(istring.string(),0,istring.blen());
}

sdl_string::sdl_string(const char *s)
{
	sdl_heap_base::init();
	set(s);
}

// note: everywhere __reset_space is called below, we could probably
// do something with realloc in sdl_heap_base.
char sdl_string::get(size_t n) const
{
	if(n >= strlen())
		return '\0';
	else
		return string()[n];
}


void sdl_string::get(char *s) const
{
	if (strlen())
		::memcpy(s, string(), strlen()+1);
	else
		*s = 0;
}


void sdl_string::get(char *s, size_t from, size_t len) const
{
	if (from >= strlen())
		*s = 0;
	else {
		// Allow copying of null at the end.  This is necessary
		// in order to make get(x,0,strlen()+1) equiv to get(x)

		if (from + len > strlen()+1) len = (strlen()+1) - from;
		::memcpy(s, string() + from, len);
	}
}


void sdl_string::set(size_t n, char c)
{
	char *tmp;

	check_size(n);
	string()[n] = c;
}


void sdl_string::set(const char *s)
{
	kill(); // null outthe previous string.
	if (s == 0) {
		return; // already nulled
	}
	set(s,0,::strlen(s)+1);
}


void sdl_string::set(const char *s, size_t from, size_t len)
{
	if (s == 0) {
		return;
	}
	if (len < 0) return;

	check_size(from+len); // puts in the null at the end
							// if extend was called
	if (len>0)::memcpy(string() + from, s, len);
}




int sdl_string::countc(const char c) const
{
	// strlen/blen confusion makes this questionable...
	size_t i, count;

	count = 0;
	for(i = 0; i < strlen(); ++i)
		if(string()[i] == c)
			++count;

	return count;
}


int sdl_string::strcmp(const char *s) const
{
	//return memcmp(string(), s, ::strlen(s)+1);
	// why memcmp? null termination is guaranteed?
	return ::strcmp(string(),s);
}


int sdl_string::strcmp(const sdl_string &s) const
{
	//return memcmp(string(), s.string(), s.strlen()+1);
	return ::strcmp(string(),s.string());
}


int sdl_string::strncmp(const char *s, size_t len) const
{
	// if (len > strlen()) len = strlen();
	// again, null termination is guaranteed.?
	return ::strncmp(string(), s, len);
}


int sdl_string::strncmp(const sdl_string &s, size_t len) const
{
	//if (len > strlen()) len = strlen();
	//if (len > s.strlen()) len = s.strlen();
	return ::strncmp(string(), s.string(), len);
}


sdl_string &
sdl_string::operator=(const sdl_string & s)
// do a literal copy, including nulls, of the entire space of s.
{
	kill();
	set(s.string(),0,s.blen());
	return *this;
}

ostream& operator<<(ostream& os, const sdl_string& x)
{
	os << (char *)x;
}
	
template class Apply<sdl_string>;
// I'm not sure about this.

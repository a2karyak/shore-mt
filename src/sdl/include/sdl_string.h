/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

//
// sdl_string.h
//


#ifndef _STRING_H_
#define _STRING_H_

#ifdef __GNUG__
#pragma interface
#endif
#if (__GNUC_MINOR__ < 7)
#   include <memory.h>
#else
#   include <std/cstring.h>
#endif /* __GNUC_MINOR__ < 7 */

// try to unify all heap object handling with the following struct.
// formerly, we had separate instances of this for string, sequence,
// set, and union.  The main thing here is to properly derive element
// counts from raw size infomation.
struct sdl_heap_base { 
	unsigned int  free_it:1;
	unsigned int cur_size:30;
	char *space;
	sdl_heap_base::sdl_heap_base();
	// this is defined in sdl_support.C, and is context dependent.
	void init() { cur_size=0; free_it = 0; space=0; }
	void __apply(HeapOps op);
	void Apply(class app_class *) const;
	void text__apply(HeapOps op); // use this heap elt as text field.
	void __free_space() // zero the space  ptr, dealocate if we can.
	{	if (free_it &&  space) delete space;
		space = 0;
	}
	void __set_space(size_t s) 
	{	space = new char[s];
		cur_size = s;
		memset(space,0,s);
		free_it = 1;
	}
	void __extend_space(size_t s);
	// extend existing space, preserving contents.
	void __reset_space(size_t s, void *spt)
	{
		space = (char *)spt;
		cur_size = s;
	}
	void  __trim_space(size_t s) // reset the size; can only make smaller.
	{
		if (s<cur_size)
			cur_size = s;
	}
	void __clear() // free  space & zero len.
	{
		__free_space();
		cur_size = 0;
	}

};

class sdl_string : protected sdl_heap_base
{
	// make sure string()[n] is a valid address; string[n+1] must be avail.for
	// null.
	void check_size(size_t n) { 
		if (cur_size< n+1) { 	
			__extend_space(n+1); 
			string()[blen()-1] = 0;
		}
	}
 public:
	friend ostream& operator<<(ostream&, const sdl_string& x);
	sdl_string(){}; // sdl_heap_base ctor does everything.
	sdl_string(const sdl_string &);
	sdl_string(const char *);

    void __apply(HeapOps op) { sdl_heap_base::__apply(op);  }
    // FIX: This will almost certainly get us in trouble later!
    // indeed.
    // sdl_string(void) : length(0), string(NULL) {}

    char get(size_t n) const;
    void get(char *s) const;
    void get(char *s, size_t from, size_t len) const;

    void set(size_t n, char c);
    void set(const char *s); // s is null-terminated
    void set(const char *s, size_t from, size_t len);

	// now, always return
	size_t strlen() const { return cur_size? ::strlen(string()):0;};
	// available space as binary data.
	size_t blen() const { return cur_size? cur_size :0;};
    int countc(const char c) const;

    int strcmp(const char *s) const; // s is null-terminated
	bool operator==(const char *s) { return strcmp(s)==0;}
    int strcmp(const sdl_string &string) const;
	bool operator==(const sdl_string & s) { return strcmp(s)==0;}

    int strncmp(const char *s, size_t len) const;
    int strncmp(const sdl_string &string, size_t len) const;

    // add familiar names
    const char *memcpy(const char *s, size_t len) { set(s,0,len); return string();}
    void	bcopy(const char *s, size_t len)  { set(s,0,len);}
    const char *strcpy(const char *s)	{ set(s); return string();}
    const char *strcat(const char *s)	
    { if (s!=0) set(s,strlen(),::strlen(s)+1); return string();}
    // for other things, just use the proper cast.

    void kill()  { sdl_heap_base::__clear(); }
	char *string()  const { return (char *)space; };

    // extras for better C++ usability
    // conversion to C string.
    operator const char *() const  { return (const char  *)space; }
    operator char *() const  { return (char  *)space; }
    // also add const void * conversion, for use with mem/bcopy routines.
    operator const void *() const  { return (const void *) space; }
    const char * operator=(const char *s ) { set(s); return string(); }
	// conversion from C string
    sdl_string &  operator=(const sdl_string &); // conversion from C string



protected:
    // Invariants:
    //   length >= 0
    //   length==0 iff string==NULL,
    //   if length > 0 then { size of string = length+1 and string[length] = 0 }
    // we dubiously hard code 32 bits here.
    // unsigned int free_it:1; // set if we need to free the string space.
    // unsigned int binary:1;  // do we need this? if set by memcpy.
    // unsigned int length:30;
    // char *string;
};

// for now, text looks exactly like a string; only differnce is in its
// own _apply fuct.
class sdl_text: public sdl_string 
{
public:
    void __apply(HeapOps op) { sdl_heap_base::text__apply(op); } 
    const char * operator=(const char *arg) { return sdl_string::operator=(arg);}// conversion from C string
};


#endif



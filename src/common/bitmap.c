/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

/*
 *  $Header: /p/shore/shore_cvs/src/common/bitmap.c,v 1.13 1996/03/18 17:21:23 bolo Exp $
 */

static char rcsid[] = "$Header: /p/shore/shore_cvs/src/common/bitmap.c,v 1.13 1996/03/18 17:21:23 bolo Exp $";

#define BITMAP_C

#ifdef __GNUC__
#pragma implementation "bitmap.h"
#endif

#include <stdlib.h> 
#include <stream.h> 
#include <sys/types.h> 
#include "basics.h" 
#include "bitmap.h" 

inline int div8(long x)         { return x >> 3; }
inline int mod8(long x)         { return x & 7; }
inline int div32(long x)        { return x >> 5; }
inline int mod32(long x)        { return x & 31; }

void bm_zero(u_char* bm, int size)
{
    int n = div8(size - 1) + 1;
    for (int i = 0; i < n; i++, bm++)   *bm = 0;
}

void bm_fill(u_char* bm, int size)
{
    int n = div8(size - 1) + 1;
    for (int i = 0; i < n; i++, bm++)	*bm = ~0;
}

bool bm_is_set(const u_char* bm, long offset)
{
    return bm[div8(offset)] & (1 << mod8(offset));
}

void bm_set(u_char* bm, long offset)
{
    bm[div8(offset)] |= (1 << mod8(offset));
}

void bm_clr(u_char* bm, long offset)
{
    bm[div8(offset)] &= ~(1 << mod8(offset));
}

int bm_first_set(const u_char* bm, int size, int start)
{
#ifdef DEBUG
    const u_char* tmp = bm;
#endif
    register int mask;
    
    dual_assert3(start >= 0 && start < size);
    
    bm += div8(start);
    mask = 1 << mod8(start);
    
    for (size -= start; size; start++, size--)  {
	if (*bm & mask)  {
	    dual_assert3(bm_is_set(tmp, start));
	    return start;
	}
	if ((mask <<= 1) == 0x100)  {
	    mask = 1;
	    bm++;
	}
    }
    
    return -1;
}

int bm_first_clr(const u_char* bm, int size, int start)
{
    dual_assert3(start >= 0 && start < size);
    register int mask;
    
    bm += div8(start);
    mask = 1 << mod8(start);
    
    for (size -= start; size; start++, size--) {
	if ((*bm & mask) == 0)	{
	    return start;
	}
	if ((mask <<= 1) == 0x100)  {
	    mask = 1;
	    bm++;
	}
    }
    
    return -1;
}

int bm_num_set(const u_char* bm, int size)
{
    int count;
    int mask;
    for (count = 0, mask = 1; size; size--)  {
	if (*bm & mask)	count++;
	if ((mask <<= 1) == 0x100)  {
	    mask = 1;
	    bm++;
	}
    }
    return count;
}

void bm_print(u_char* bm, int size)
{
    for (int i = 0; i < size; i++)  {
	cout << (bm_is_set(bm, i) != 0);
    }
}
 
 
#if 0
const numBits = 71;
const numBytes = (numBits - 1) / 8 + 1;
 
main()
{
    u_char map[numBytes];
 
    for (int i = 0; i < numBytes; i++)   map[i] = rand();
 
    cout << "Testing Clear Map    ";
    BmapClearMap(map, numBits);
    for (i = 0; i < numBits; i++)  {
		if (BmapIsSet(map, i))  {
			cout << "failed\n" << flush;
			exit(-1);
			}
		}
    cout << "passed\n" << flush;
 
    cout << "Testing Set Map:     ";
    BmapSetMap(map, numBits);
    for (i = 0; i < numBits; i++)  {
		if ( ! BmapIsSet(map, i))  {
			cout << "failed\n" << flush;
			exit(-1);
			}
		}
    cout << "passed\n" << flush;
 
    cout << "Testing Random Set:  ";
    BmapClearMap(map, numBits);
    for (i = 0; i < 10000; i++)  {
		int bit = rand() % numBits;
		BmapSetBit(map, bit);
		if ( ! BmapIsSet(map, bit) )  {
			cout << "failed\n" << flush;
			exit(-1);
			}
		}
    cout << "passed\n" << flush;
 
    cout << "Testing Random Clear:";
    BmapSetMap(map, numBits);
    for (i = 0; i < 10000; i++)  {
		int bit = rand() % numBits;
		BmapClearBit(map, bit);
		if (BmapIsSet(map, bit))  {
			cout << "failed\n" << flush;
			exit(-1);
			}
		}
    cout << "passed\n" << flush;
 
    cout << "Testing First Set   : ";
    for (i = 0; i < 10000; i++)  {
        BmapClearMap(map, numBits);
 
	int j = rand() % numBits;
	BmapSetBit(map, j);
 
	if (BmapFirstSet(map, numBits, 0) != j)  {
	    cout << "failed\n" << flush;
	    exit(-1);
	    }
	
	if (BmapFirstSet(map, numBits, j) != j)  {
	    cout << "failed\n" << flush;
	    exit(-1);
	    }
	}
    cout << "passed\n" << flush;
 
    cout << "Testing First Clear : ";
    for (i = 0; i < 10000; i++)  {
		BmapSetMap(map, numBits);
		int bit = rand() % numBits;
		BmapClearBit(map, bit);
		if (BmapFirstClear(map, numBits, 0) != bit)  {
			cout << "failed\n" << flush;
			exit(-1);
			}
 
		if (BmapFirstClear(map, numBits, bit) != bit)  {
			cout << "failed\n" << flush;
			exit(-1);
			}
		BmapPrint(map, numBits);
		cout << "\n";
		}
    cout << "passed\n" << flush;
 
    cout << flush;
}
#endif


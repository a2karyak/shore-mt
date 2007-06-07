/*<std-header orig-src='shore'>

 $Id: ms_perf.cpp,v 1.7 1999/06/07 19:03:05 kupsch Exp $

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

#ifndef _WIN32
#include <w_stream.h> 
int main() 
{
    cerr << "Not implemented for non-Windows architectures." <<endl;
}
	
#else
/*
 * This code is taken DIRECTLY from the VC++ help example
 * on the page
 * "Displaying Object, Instance, and Counter Names"
 */
#include <windows.h> 
#include <stdio.h> 
#include <stdlib.h> 
#include <malloc.h> 
#include <string.h> 
#define TOTALBYTES 8192 
#define BYTEINCREMENT 1024 
LPSTR lpNameStrings; 
LPSTR *lpNamesArray; 
DWORD lpNamesArraySize=0;
/***************************************************************** 
* * 
* Functions used to navigate through the performance data. * 
* * 
*****************************************************************/ 
PPERF_OBJECT_TYPE FirstObject( PPERF_DATA_BLOCK PerfData ) 
{ 
    return( (PPERF_OBJECT_TYPE)((PBYTE)PerfData + 
    PerfData->HeaderLength) ); 
} 
PPERF_OBJECT_TYPE NextObject( PPERF_OBJECT_TYPE PerfObj ) 
{ 
    return( (PPERF_OBJECT_TYPE)((PBYTE)PerfObj + 
    PerfObj->TotalByteLength) ); 
} 
PPERF_INSTANCE_DEFINITION FirstInstance( PPERF_OBJECT_TYPE PerfObj ) 
{ 
    return( (PPERF_INSTANCE_DEFINITION)((PBYTE)PerfObj + 
    PerfObj->DefinitionLength) ); 
} 
PPERF_INSTANCE_DEFINITION NextInstance( 
PPERF_INSTANCE_DEFINITION PerfInst ) 
{ 
    PPERF_COUNTER_BLOCK PerfCntrBlk; 
    PerfCntrBlk = (PPERF_COUNTER_BLOCK)((PBYTE)PerfInst + 
    PerfInst->ByteLength); 
    return( (PPERF_INSTANCE_DEFINITION)((PBYTE)PerfCntrBlk + 
    PerfCntrBlk->ByteLength) ); 
} 
PPERF_COUNTER_DEFINITION FirstCounter( PPERF_OBJECT_TYPE PerfObj ) 
{ 
    return( (PPERF_COUNTER_DEFINITION) ((PBYTE)PerfObj + 
    PerfObj->HeaderLength) ); 
} 
PPERF_COUNTER_DEFINITION NextCounter( 
PPERF_COUNTER_DEFINITION PerfCntr ) 
{ 
    return( (PPERF_COUNTER_DEFINITION)((PBYTE)PerfCntr + 
    PerfCntr->ByteLength) ); 
} 
__int64 PerfTime;
__int64 Time100nSec;
__int64 PerfFreq;
/***************************************************************** 
* * 
* Load the counter and object names from the registry to the * 
* global variable lpNamesArray. * 
* * 
*****************************************************************/ 
const char *unknown = "Unknown name";
void GetNameStrings( ) 
{ 
    HKEY hKeyPerflib; // handle to registry key 
    HKEY hKeyPerflib009; // handle to registry key 
    DWORD dwMaxValueLen; // maximum size of key values 
    DWORD dwBuffer; // bytes to allocate for buffers 
    DWORD dwBufferSize; // size of dwBuffer 
    LPSTR lpCurrentString; // pointer for enumerating data strings 
    DWORD dwCounter; // current counter index 
    // Get the number of Counter items. 
    RegOpenKeyEx( HKEY_LOCAL_MACHINE, 
    "SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\Perflib", 
    0, 
    KEY_READ, 
    &hKeyPerflib); 
    dwBufferSize = sizeof(dwBuffer); 
	RegQueryValueEx( hKeyPerflib, 
	"Last Counter", 
	NULL, 
	NULL, 
	(LPBYTE) &dwBuffer, 
	&dwBufferSize ); 
    RegCloseKey( hKeyPerflib ); 

    // Allocate memory for the names array. 
    lpNamesArraySize = (dwBuffer+1)*sizeof(LPSTR);
    printf("lpNamesArraySize is %d\n", lpNamesArraySize);
    lpNamesArray = (char **)malloc( (dwBuffer+1) * sizeof(LPSTR) ); 

    // initialize to "unknown";
    for(unsigned int i=0; i< (dwBuffer+1); i++) {
	lpNamesArray[i] = (char *)unknown;
    }

    // Open key containing counter and object names. 
    RegOpenKeyEx( HKEY_LOCAL_MACHINE, 
    "SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\Perflib\\009", 
    0, 
    KEY_READ, 
    &hKeyPerflib009); 
    // Get the size of the largest value in the key (Counter or Help). 
    RegQueryInfoKey( hKeyPerflib009, 
    NULL, 
    NULL, 
    NULL, 
    NULL, 
    NULL, 
    NULL, 
    NULL, 
    NULL, 
    &dwMaxValueLen, 
    NULL, 
    NULL); 
    // Allocate memory for the counter and object names. 
    dwBuffer = dwMaxValueLen + 1; 
    lpNameStrings = (char *)malloc( dwBuffer * sizeof(CHAR) ); 
    // Read Counter value. 
    RegQueryValueEx( hKeyPerflib009, 
	"Counter", 
	NULL, 
	NULL, 
	(unsigned char *)lpNameStrings, &dwBuffer ); 

    // Load names into an array, by index. 
    for( lpCurrentString = lpNameStrings; *lpCurrentString; 
	lpCurrentString += (lstrlen(lpCurrentString)+1) ) {
	dwCounter = atol( lpCurrentString ); 
	lpCurrentString += (lstrlen(lpCurrentString)+1); 
        if(dwCounter > lpNamesArraySize) {
	    printf("%d %s %d", __LINE__ ,"bad index: ",
			dwCounter);
	    abort();
	}
	lpNamesArray[dwCounter] = (LPSTR) lpCurrentString; 
/*
	if(dwCounter >= 0) {
	    printf("index  %d at address %d, string is %s\n", 
		dwCounter, lpCurrentString,
		lpCurrentString);
	}
*/
    } 
} 

void
filetime2timeval(FILETIME *f, timeval &tv)
{
	LARGE_INTEGER li;
        memcpy(&li, f, sizeof(*f));
	__int64 i = li.QuadPart;
	i /= 10;
#define MILLION 1000000;
	tv.tv_usec =  i % MILLION;
	tv.tv_sec =  i / MILLION;
}

void
RetrieveAllCounters(
    PPERF_OBJECT_TYPE PerfObj, 
    PPERF_COUNTER_DEFINITION PerfCntr,
    PPERF_COUNTER_BLOCK PtrToCntr)
{
    PPERF_COUNTER_DEFINITION CurCntr = PerfCntr;
    DWORD j;
    int known = 1;

    // Retrieve all counters. 
    for( j=0; j < PerfObj->NumCounters; j++ ) {
	// Display the counter by index and name. 
	known = 1; 
	printf("\t\t%d-Counter %ld: %s\n", 
	    j,
	    CurCntr->CounterNameTitleIndex,
	    lpNamesArray[CurCntr->CounterNameTitleIndex]
	    );

	printf("\t\t\tCounterType=0x%x, CounterSize=%d, Kind=", 
	    CurCntr->CounterType,
	    CurCntr->CounterSize
	    ); 
    
	union __value {
	    char 		value1[8];
	    unsigned short 	value2[4];
	    unsigned int 	value4[2];
	    unsigned __int64 value8[1];
	} value;
	timeval tv;

	memcpy(&value, 
		((PBYTE)PtrToCntr) + CurCntr->CounterOffset,
		CurCntr->CounterSize);

        char *value_out = new char[20];

	switch(CurCntr->CounterType) {
	case PERF_COUNTER_COUNTER: {
	    printf(" PERF_COUNTER_COUNTER\n");

	    __int64 g;
	    memcpy(&g, &value, sizeof(g));  

	    /* compute the diff */
	    __int64 f = PerfTime / PerfFreq;
	    f = g / f;
	    (void) _i64toa(f, value_out, 10);
	    printf("\t\t\tValue=%s\n", value_out);
	    filetime2timeval((FILETIME *)&f, tv);
	    printf("\t\t\tValue=%d sec %d usec\n", 
		    tv.tv_sec, tv.tv_usec);
	    } break;

	case PERF_COUNTER_MULTI_TIMER:
	    printf(" PERF_COUNTER_MULTI_TIMER\n");
		known = 0;
	    break;

	case PERF_100NSEC_TIMER: {
	    printf(" PERF_100NSEC_TIMER\n");
	    __int64 g;
	    memcpy(&g, &value, sizeof(g));

	    __int64 f = g/ Time100nSec ;
	    (void) _i64toa(f, value_out, 10);
	    printf("\t\t\tValue=%s\n", value_out);

	    filetime2timeval((FILETIME *)&f, tv);
	    printf("\t\t\tValue=%d sec %d usec\n", 
		    tv.tv_sec, tv.tv_usec);
	    } break;

	case PERF_COUNTER_TIMER: {
	    printf(" PERF_COUNTER_TIMER\n");
	    __int64 g;
	    memcpy(&g, &value, sizeof(g));
	    __int64 f = g/PerfTime;
	    (void) _i64toa(f, value_out, 10);
	    printf("\t\t\tValue=%s\n", value_out);
	    filetime2timeval((FILETIME *)&f, tv);
	    printf("\t\t\tValue=%d sec %d usec\n", 
		    tv.tv_sec, tv.tv_usec);
	    } break;

	case PERF_COUNTER_RAWCOUNT:
	    printf(" PERF_COUNTER_RAWCOUNT\n");
	    printf("\t\t\tValue=%d\n", value.value4[0]);
	    break;

	case PERF_COUNTER_LARGE_RAWCOUNT:
	    printf(" PERF_COUNTER_LARGE_RAWCOUNT\n");
	    printf("\t\t\tValue=%d/%d\n", 
		value.value4[0], value.value4[1]);
	    break;

	case PERF_ELAPSED_TIME: {
	    printf(" PERF_ELAPSED_TIME\n");
	    /* value given is the start time */

	    /* Get the OBJECT's sampling time */
	    memcpy(&PerfTime, &PerfObj->PerfTime, sizeof(PerfTime));
	    memcpy(&PerfFreq, &(PerfObj->PerfFreq), sizeof(PerfFreq));

	    __int64 g;
	    memcpy(&g, &value, sizeof(g));

	    /* compute the diff */
	    __int64 f = PerfTime - g;
	    f = f / PerfFreq;
	    (void) _i64toa(f, value_out, 10);
	    printf("\t\t\tValue=%s\n", value_out);

	    filetime2timeval((FILETIME *)&f, tv);
	    printf("\t\t\tValue=%d sec %d usec\n", 
		    tv.tv_sec, tv.tv_usec);
	    } break;

	default:
	    /* don't know what to do with it */
	    printf(" unknown\n");
	    known = 0;
	    break;
	}
	delete[] value_out;

	if(!known) switch(CurCntr->CounterSize) {
	case 1: {
	    printf("\t\t\tValue=0x%x\n", value.value1[0]);
	    } break;
	case 2: {
	    printf("\t\t\tValue=0x%x\n", value.value2[0]);
	    } break;
	case 4: {
	    printf("\t\t\tValue=0x%x\n", value.value4[0]);
	    } break;
	case 8: {
	    printf("\t\t\tValue=0x%x\n", value.value8[0]);
	    } break;
	}

	// Get the next counter. 
	CurCntr = NextCounter( CurCntr ); 
    } 
}
/***************************************************************** 
* * 
* Display the indices and/or names for all performance objects, * 
* instances, and counters. * 
* * 
*****************************************************************/ 
void main(int argc, char *argv[]) 
{ 
    printf("argv[0]=%s\n", argv[0]);
    (void) setbuf(stdout, NULL);

    PPERF_DATA_BLOCK PerfData = NULL; 
    PPERF_OBJECT_TYPE PerfObj; 
    PPERF_INSTANCE_DEFINITION PerfInst; 
    PPERF_COUNTER_DEFINITION PerfCntr;
    PPERF_COUNTER_BLOCK PtrToCntr; 
    DWORD BufferSize = TOTALBYTES; 
    DWORD i, k; 

    // Get the name strings through the registry. 
    GetNameStrings( ); 

    // Allocate the buffer for the performance data. 
    PerfData = (PPERF_DATA_BLOCK) malloc( BufferSize ); 
    while( RegQueryValueEx( HKEY_PERFORMANCE_DATA, 
	"Global", 
	NULL, 
	NULL, 
	(LPBYTE) PerfData, 
	&BufferSize ) == ERROR_MORE_DATA ) 
    { 
	// Get a buffer that is big enough. 
	BufferSize += BYTEINCREMENT; 
	PerfData = (PPERF_DATA_BLOCK) realloc( PerfData, BufferSize ); 
    } 
    printf( "\n%d objects\n", PerfData->NumObjectTypes);

    memcpy(&PerfTime, &PerfData->PerfTime, sizeof(PerfTime));
    memcpy(&PerfFreq, &(PerfData->PerfFreq), sizeof(PerfFreq));
    memcpy(&Time100nSec, &(PerfData->PerfTime100nSec), sizeof(Time100nSec));

    // Get the first object type. 
    PerfObj = FirstObject( PerfData ); 
    // Process all objects. 
    for( i=0; i < PerfData->NumObjectTypes; i++ ) {
	// Display the object by index and name. 
        if(PerfObj->ObjectNameTitleIndex > lpNamesArraySize) {
	    printf("%d %s %d", __LINE__ ,"bad index: ",
			PerfObj->ObjectNameTitleIndex);
	    abort();
	}
	printf( "\n%d-Object %ld: %s %d instances\n", 
		i,
		PerfObj->ObjectNameTitleIndex, 
		lpNamesArray[PerfObj->ObjectNameTitleIndex],
		PerfObj->NumInstances ); 

	// Get the first counter. 
	PerfCntr = FirstCounter( PerfObj ); 

	if( PerfObj->NumInstances > 0 ) {
	    // Get the first instance. 
	    PerfInst = FirstInstance( PerfObj ); 
	    // Retrieve all instances. 
	    for( k=0; k < (unsigned int) PerfObj->NumInstances; k++ ) {
		// Display the instance by name. 
		printf( "\n\t%d-Instance Name=%S/UniqueID=%d (parent=%s %d): %d counters \n", 
		    k,
		    (char *)((PBYTE)PerfInst + PerfInst->NameOffset),
		    PerfInst->UniqueID,
			lpNamesArray[PerfInst->ParentObjectTitleIndex],
			PerfInst->ParentObjectInstance,
		    PerfObj->NumCounters); 

		// Get the counter block for the instance. 
		PtrToCntr = (PPERF_COUNTER_BLOCK) ((PBYTE)PerfInst + 
				PerfInst->ByteLength ); 
		RetrieveAllCounters(PerfObj, PerfCntr, PtrToCntr) ;

		// Get the next instance. 
		PerfInst = NextInstance( PerfInst ); 
	    } 
	} else {
	    // Get the counter block. 
	    PtrToCntr = (PPERF_COUNTER_BLOCK) ((PBYTE)PerfObj + 
				PerfObj->DefinitionLength ); 

	    RetrieveAllCounters(PerfObj, PerfCntr, PtrToCntr) ;

	} 
	// Get the next object type. 
	PerfObj = NextObject( PerfObj ); 
    } 
    free(PerfData);
} 
#endif _WIN32


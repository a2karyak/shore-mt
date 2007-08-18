#ifndef E_ERROR_ENUM_GEN_H
#define E_ERROR_ENUM_GEN_H

/* DO NOT EDIT --- generated by ../../tools/errors.pl from e_error.dat
                   on Sat Aug 18 14:26:29 2007 

<std-header orig-src='shore' genfile='true'>

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
enum {
    eASSERT                   = 0x80000,
    eUSERABORT                = 0x80001,
    eCRASH                    = 0x80002,
    eCOMM                     = 0x80003,
    eOUTOFSPACE               = 0x80004,
    eALIGNPARM                = 0x80005,
    eBADSTID                  = 0x80006,
    e1PAGESTORELIMIT          = 0x80007,
    eBADPID                   = 0x80008,
    eBADVOL                   = 0x80009,
    eVOLTOOLARGE              = 0x8000a,
    eDEVTOOLARGE              = 0x8000b,
    eDEVICEVOLFULL            = 0x8000c,
    eDEVNOTMOUNTED            = 0x8000d,
    eALREADYMOUNTED           = 0x8000e,
    eVOLEXISTS                = 0x8000f,
    eBADFORMAT                = 0x80010,
    eNVOL                     = 0x80011,
    eEOF                      = 0x80012,
    eDUPLICATE                = 0x80013,
    eBADSTORETYPE             = 0x80014,
    eBADSTOREFLAGS            = 0x80015,
    eBADNDXTYPE               = 0x80016,
    eBADSCAN                  = 0x80017,
    eWRONGKEYTYPE             = 0x80018,
    eNDXNOTEMPTY              = 0x80019,
    eBADKEYTYPESTR            = 0x8001a,
    eBADKEY                   = 0x8001b,
    eBADCMPOP                 = 0x8001c,
    eOUTOFLOGSPACE            = 0x8001d,
    eRECWONTFIT               = 0x8001e,
    eBADSLOTNUMBER            = 0x8001f,
    eRECUPDATESIZE            = 0x80020,
    eBADSTART                 = 0x80021,
    eBADAPPEND                = 0x80022,
    eBADLENGTH                = 0x80023,
    eBADSAVEPOINT             = 0x80024,
    eTOOMANYLOADONCEFILES     = 0x80025,
    ePAGECHANGED              = 0x80026,
    eINSUFFICIENTMEM          = 0x80027,
    eBADARGUMENT              = 0x80028,
    eNOTSORTED                = 0x80029,
    eTWOTRANS                 = 0x8002a,
    eTWOTHREAD                = 0x8002b,
    eNOTRANS                  = 0x8002c,
    eINTRANS                  = 0x8002d,
    eTWOQUARK                 = 0x8002e,
    eNOQUARK                  = 0x8002f,
    eINQUARK                  = 0x80030,
    eNOABORT                  = 0x80031,
    eNOTPREPARED              = 0x80032,
    eISPREPARED               = 0x80033,
    eEXTERN2PCTHREAD          = 0x80034,
    eNOTEXTERN2PC             = 0x80035,
    eNOSUCHPTRANS             = 0x80036,
    eBADLOCKMODE              = 0x80037,
    eLOCKTIMEOUT              = 0x80038,
    eMAYBLOCK                 = 0x80039,
    eDEADLOCK                 = 0x8003a,
    eREMOTEDEADLOCK           = 0x8003b,
    ePREEMPTED                = 0x8003c,
    eBADCCLEVEL               = 0x8003d,
    eRETRY                    = 0x8003e,
    eFOUNDEXTTOFREE           = 0x8003f,
    eNOGLOBALDEADLOCKCLIENT   = 0x80040,
    eNOCOPIES                 = 0x80041,
    eCOPYNOTEXISTS            = 0x80042,
    eBADCBREPLY               = 0x80043,
    eCBEXISTS                 = 0x80044,
    eSERVERNOTCONNECTED       = 0x80045,
    eBADMSGTYPE               = 0x80046,
    eNOFREESERVER             = 0x80047,
    eVOTENO                   = 0x80048,
    eVOTEREADONLY             = 0x80049,
    eBADLOGICALID             = 0x8004a,
    eBADLOGICALIDTYPE         = 0x8004b,
    eLOGICALIDOVERFLOW        = 0x8004c,
    eNOLOGICALTEMPFILES       = 0x8004d,
    eCANTWHILEACTIVEXCTS      = 0x8004e,
    eINTRANSIT                = 0x8004f,
    eNOTONREMOTEVOL           = 0x80050,
    eNOHANDLE                 = 0x80051,
    ePROTOCOL                 = 0x80052,
    eMIXED                    = 0x80053,
    eLOGVERSIONTOONEW         = 0x80054,
    eLOGVERSIONTOOOLD         = 0x80055,
    eBADMASTERCHKPTFORMAT     = 0x80056,
    eABORTED                  = 0x80057,
    eLOGSPACEWARN             = 0x80058,
    eOK                       = 0x0,
    eERRMIN                   = 0x80000,
    eERRMAX                   = 0x80058
};

#endif

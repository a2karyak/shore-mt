#ifndef E_ERRMSG_GEN_H
#define E_ERRMSG_GEN_H

/* DO NOT EDIT --- generated by ../../tools/errors.pl from e_error.dat
                   on Fri May 30 23:57:48 2008 

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
static char* e_errmsg[] = {
/* eASSERT                   */ "Assertion failed",
/* eUSERABORT                */ "User initiated abort",
/* eCRASH                    */ "Server told to crash or shutdown immediately",
/* eCOMM                     */ "Error from shore communication facility",
/* eOUTOFSPACE               */ "out of disk space",
/* eALIGNPARM                */ "parameter is not aligned",
/* eBADSTID                  */ "Malformed or invalid store id",
/* e1PAGESTORELIMIT          */ "1-page store needs to grown beyond 1 page",
/* eBADPID                   */ "invalid page id",
/* eBADVOL                   */ "invalid volume id",
/* eVOLTOOLARGE              */ "volume is too large for device",
/* eDEVTOOLARGE              */ "device is too large for OS file interface",
/* eDEVICEVOLFULL            */ "device cannot hold anymore volumes",
/* eDEVNOTMOUNTED            */ "device is not mounted",
/* eALREADYMOUNTED           */ "device already mounted",
/* eVOLEXISTS                */ "volume already exists",
/* eBADFORMAT                */ "volume has bad format",
/* eNVOL                     */ "too many volumes",
/* eEOF                      */ "end of scan/record reached",
/* eDUPLICATE                */ "duplicate entries found",
/* eBADSTORETYPE             */ "bad store type",
/* eBADSTOREFLAGS            */ "bad store flags",
/* eBADNDXTYPE               */ "bad index type",
/* eBADSCAN                  */ "bad scan arguments",
/* eWRONGKEYTYPE             */ "key type unsupported for current index",
/* eNDXNOTEMPTY              */ "index is not empty",
/* eBADKEYTYPESTR            */ "bad key type descriptor",
/* eBADKEY                   */ "bad key value",
/* eBADCMPOP                 */ "bad compare operators",
/* eOUTOFLOGSPACE            */ "out of log space ",
/* eRECWONTFIT               */ "record will not fit",
/* eBADSLOTNUMBER            */ "record ID slot number is bad",
/* eRECUPDATESIZE            */ "record update request is too large",
/* eBADSTART                 */ "start parameter larger than record size",
/* eBADAPPEND                */ "append size too large for the record",
/* eBADLENGTH                */ "bad length parameter",
/* eBADSAVEPOINT             */ "bad save point ",
/* eTOOMANYLOADONCEFILES     */ "too many load-once files",
/* ePAGECHANGED              */ "page has changed",
/* eINSUFFICIENTMEM          */ "memory limit imposed on function was insufficient",
/* eBADARGUMENT              */ "bad argument or combination of arguments to function",
/* eNOTSORTED                */ "input file for bulk load was not sorted",
/* eTWOTRANS                 */ "second transaction in one thread",
/* eTWOTHREAD                */ "multiple threads not allowed for this operation",
/* eNOTRANS                  */ "no active transaction",
/* eINTRANS                  */ "in active transaction (not allowed)",
/* eTWOQUARK                 */ "second quark in a single transaction is not allowed",
/* eNOQUARK                  */ "no quark is open",
/* eINQUARK                  */ "a quark is open",
/* eNOABORT                  */ "logging is turned off -- cannot roll back",
/* eNOTPREPARED              */ "transaction thread is not prepared -- cannot commit or abort",
/* eISPREPARED               */ "transaction thread is prepared  -- cannot do this operaton",
/* eEXTERN2PCTHREAD          */ "transaction is (already) participating in external 2-phase commit ",
/* eNOTEXTERN2PC             */ "transaction is not participating in external 2-phase commit ",
/* eNOSUCHPTRANS             */ "could not find a prepared transaction with given global transaction id",
/* eBADLOCKMODE              */ "invalid lock mode",
/* eLOCKTIMEOUT              */ "lock timeout",
/* eMAYBLOCK                 */ "lock req will block locally or may block remotely",
/* eDEADLOCK                 */ "deadlock detected",
/* eREMOTEDEADLOCK           */ "callback req received deadlock notification from server",
/* ePREEMPTED                */ "lock req cancelled by another xact",
/* eBADCCLEVEL               */ "unsupported concurrency control level",
/* eRETRY                    */ "retry the request (used internally)",
/* eFOUNDEXTTOFREE           */ "unlock duration found an ext to free (used internally)",
/* eNOGLOBALDEADLOCKCLIENT   */ "no global deadlock client installed",
/* eNOCOPIES                 */ "no copies in the copy table",
/* eCOPYNOTEXISTS            */ "copy does not exist",
/* eBADCBREPLY               */ "invalid reply to callback request",
/* eCBEXISTS                 */ "duplicate callback proxy",
/* eSERVERNOTCONNECTED       */ "not connected to server",
/* eBADMSGTYPE               */ "unknown or unsupported message type",
/* eNOFREESERVER             */ "no free slot in the server table",
/* eVOTENO                   */ "prepare resulted in vote to abort (abort done)",
/* eVOTEREADONLY             */ "prepare resulted in vote readonly (commit done)",
/* eCANTWHILEACTIVEXCTS      */ "can't do to volume while there are active transactions",
/* eINTRANSIT                */ "had to wait for pending remote read",
/* eNOTONREMOTEVOL           */ "this volume operation disallowed on remote volume ",
/* eNOHANDLE                 */ "Missing coordinator handle",
/* ePROTOCOL                 */ "Two-phase commit protocol error",
/* eMIXED                    */ "Aborted two-phase commit got mixed results",
/* eLOGVERSIONTOONEW         */ "Log created with newer incompatible server",
/* eLOGVERSIONTOOOLD         */ "Log created with older incompatible server",
/* eBADMASTERCHKPTFORMAT     */ "Bad format for master checkpoint",
/* eABORTED                  */ "Transaction aborted by another thread",
/* eLOGSPACEWARN             */ "log space warning (not necessarily out)",
	"dummy error code"
};

const e_msg_size = 84;

#endif

#ifndef  _stats_server
#define  _stats_server

/*$Header: /p/shore/shore_cvs/src/oo7/stats_server.h,v 1.2 1994/02/04 19:21:56 nhall Exp $*/
/********************************************************/
/*                                                      */
/*               OO7 Benchmark                          */
/*                                                      */
/*              COPYRIGHT (C) 1993                      */
/*                                                      */
/*                Michael J. Carey 		        */
/*                David J. DeWitt 		        */
/*                Jeffrey Naughton 		        */
/*               Madison, WI U.S.A.                     */
/*                                                      */
/*	         ALL RIGHTS RESERVED                    */
/*                                                      */
/********************************************************/

/*
 * stats_server.h
 *
 * Class definition for the server stats class.
 * Member functions are defined in stats_server.c
 */

#include "stats.h"

#ifndef _sm_client
#	include "E/sm_client.h"
#	define _sm_client
#endif

/*
 * Stats objects are meant to be used as follows:
 *
 *        statsObj.Start();
 *        section of code for which stats are being gathered
 *        statsObj.Stop();
 *        examine gathered statistics
 *
 * Note that member functions marked as unimplmented return zero.
 * They are unimplemented because Mach has yet to support the 
 * gathering of the required statistics.
 * Mach will supposedly provide support in the near future, however.
 */


/*
 * Stats for The server process.
 */
class ServerStats : public ProcessStats {

	SERVERSTATS		serverStats1;
	SERVERSTATS		serverStats2;

public:
    void   Start();			/* start gathering stats  */
    void   Stop();			/* stop gathering stats  */
};


#endif _stats_server


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

#include <libc.h>
#include <stdio.h>
#include "MultiStats.h"


// initialize statistics

MultiStats::MultiStats(int numXacts)
{
    count = 0;
    limit = numXacts;
    times = new TimingRec[limit];
}


// accumulate statistics

void MultiStats::AddToStats(double wall, double user, double system)
{
    if (count == limit)
    {
	fprintf(stderr, "Multi-user measurement overflow!!!\n");
	exit(1);
    }
    times[count].wall = wall;
    times[count].user = user;
    times[count].system = system;
    count++;
}


// print out statistics

void MultiStats::PrintStats()
{
    // first print the results on a per-transaction basis

    int i;
    for (i = 0; i < count; i++)
    {
	printf("Transaction number %d:\n", i+1);
	printf("\tresponse time = %f seconds\n", times[i].wall);
	printf("\tcpu time = %f seconds (%f user, %f system)\n",
		  times[i].user+times[i].system,
		  times[i].user,
		  times[i].system);
    }

    // now compute the totals/averages (note that this code will
    // be changed later as needed to toss out some of the initial
    // and/or final iteration times)

    double wallAvg = 0.0, userAvg = 0.0, systemAvg = 0.0;
    for (i = 0; i < count; i++)
    {
	wallAvg += times[i].wall;
	userAvg += times[i].user;
	systemAvg += times[i].system;
    }
    wallAvg /= double(count);
    userAvg /= double(count);
    systemAvg /= double(count);

    // finally, print the total/average results

    printf("Average over all %d transactions\n", count);
    printf("\tavg response time = %f seconds\n", wallAvg);
    printf("\tavg cpu time = %f seconds (%f user, %f system)\n",
    	      (userAvg + systemAvg), userAvg, systemAvg);
}


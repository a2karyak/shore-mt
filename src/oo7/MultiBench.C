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
#include <stdlib.h>
#include "getrusage.h"
#include "random.h"

#include "oo7.h"
#include "globals.h"
#include "GenParams.h"
#include "VarParams.h"
#include "BenchParams.h"
//#include "IntBtree.h" 
//#include "StringBtree.h"
#include "stats.h"
//#include "stats_server.h"
#include "baidlist.h"

#include "MultiStats.h"
#define MAXCLIENTS 16

extern int RealWork; // set to one to make atomicpart::DoNothing
                     // actually do some work
extern int WorkAmount;   // controls how much work DoNothings do


extern int SetParams(char* configFileName);


int overFlowCnt;

extern void Purge();
extern int mquery1(int clientNum);
extern int mquery2(int clientNum);

extern double ComputeWallClockTime(struct timeval *startWallTime, 
			           struct timeval *endWallTime);

extern double ComputeUserTime(struct rusage *startUsage, 
	        	      struct rusage *endUsage);

extern double ComputeSystemTime(struct rusage *startUsage, 
	 	                struct rusage *endUsage);


//////////////////////////////////////////////////////////////////
//
// Global Variables, etc., for Benchmarking ODB Operations
//
//////////////////////////////////////////////////////////////////


int	nextAtomicId=0; 
int	nextCompositeId=0;
int	nextComplexAssemblyId=0;
int	nextBaseAssemblyId=0;
int	nextModuleId = TotalModules;

struct rusage startUsage, endUsage;
struct timeval startWallTime;
struct timeval endWallTime;
struct timeval startWarmTime;


char    *types[NumTypes] = {
		"type000", "type001", "type002", "type003", "type004",
		"type005", "type006", "type007", "type008", "type009"
	};

// here only to keep the make happy. used in gendb
BAIdList* private_cp;
BAIdList* shared_cp;

///////////////////////////////////////////////////////////////////////////
// ParseCommandLine parses the original shell call to "mbench", determining 
// which operation to run and how many times to run it.
//////////////////////////////////////////////////////////////////////////

void NewParseCommandLine(int argc, 
		      char**argv, 
		      int& numXacts,
		      int& numOps,
		      int& RP,
		      int& RS,
		      int& WP,
		      int& WS,
	              int& clientNum) 
{
	printf("Call was: ");
	for (int foo = 0; foo < argc; foo++) {
		printf("%s  ", argv[foo]);
	}
	printf("\n");

	if (strcmp(argv[argc-1], "-d") == 0) debugMode = TRUE;
        else debugMode = FALSE;
        if (argc < 8)
        {
	    fprintf(stderr, "Usage: %s <configFileName> <xcount> <ocount> <rp> <rs> <wp> <ws> <client> [-d]\n", argv[0]);
	    fprintf(stderr, "xcount = number of transactions to run.\n");
	    fprintf(stderr, "ocount = number of ops per transaction.\n");
	    fprintf(stderr, "client = unique integer client number.\n");
	    exit(1);
	}

	if ( ((numXacts  = atoi(argv[2])) <= 0) ||
	     ((numOps    = atoi(argv[3])) <= 0) ||
	     ((RP        = atoi(argv[4])) <  0) ||
	     ((RS        = atoi(argv[5])) <  0) ||
	     ((WP        = atoi(argv[6])) <  0) ||
	     ((WS        = atoi(argv[7])) <  0) ||
	     ((clientNum = atoi(argv[8])) <= 0) ||
	     (clientNum > TotalModules) )
	{
	    fprintf(stderr, "Usage: %s <configFileName> <xcount> <ocount> <op> <client> [-d]\n", argv[0]);
	    fprintf(stderr, "xcount = %d, ocount = %d, clientNum = %d\n", numXacts, numOps, clientNum);
	    fprintf(stderr, "xcount and ocount must be greater than 0,\n");
	    fprintf(stderr, "and client must not exceed the number of modules!\n");
	    exit(1);
        } 
}



main(int argc, char** argv)
{

	REF(Module)  moduleH;
	char moduleName[40];
	// ProcessStats    totalTime;
	// ServerStats     totalSrvTime;
	//char*	purgeVar;

// NEW VARS
	int err;
	int numRestarts=0;
	int  backupCompId;
	int backupAtomId;
	char  resultText[200];              // buffer to hold result of operation for
	                                    // printing outside of timing region.
	BenchmarkOp whichOp = PrivateRead;

	char *vashost;


	vashost = getenv(VASHOST);
	if(vashost == NULL)
	    vashost = DEF_VASHOST;
	if(oc.init(vashost)){
	    cerr << "can't initialize object cache" << endl;
	    return 1;
	}



	// Compute structural info needed by the update operations,
        // since these operations need to know which id's should
        // be used next.


	if(oc.begin_transaction(3)){
	    cerr << "can't begin transaction" << endl;
	    return 1;
	}
	// initialize parameters for benchmark.
	if (SetParams(argv[1]) == -1)
	{
	    fprintf(stderr, "Usage: %s <configFileName> <xcount> <ocount> <op> <client> [-d]\n", argv[0]);
	    exit(1);
	  }

	if(oc.commit_transaction()){
	    cerr << "can't commit transaction" << endl;
	    return 1;
	  }


 	// see which multi-user test to run, how many transactions to
	// run on each client, and whether or not debug mode is desired
	int numXacts = 1;
	int numOps = 1;
	int numAtomic;
	int RP,RS,WP,WS;
	int clientNo = 0;
        NewParseCommandLine(argc, argv, numXacts, numOps,
	                    RP,RS,WP,WS, clientNo); 
	// compute structural info needed by some operations
	int baseCnt = NumAssmPerAssm;
	int complexCnt = 1;
	for (int i = 1; i < NumAssmLevels-1; i++) {
            baseCnt = baseCnt * NumAssmPerAssm;
            complexCnt += complexCnt * NumAssmPerAssm;
	}
	nextBaseAssemblyId = TotalModules*baseCnt + 1;
	nextComplexAssemblyId = TotalModules*complexCnt + 1;
	nextCompositeId = TotalCompParts + numXacts*((clientNo-1)*NumNewCompParts) + 1;

	printf("CLIENT=%d,nextCompId=%d\n",clientNo,nextCompositeId );

        nextAtomicId = (nextCompositeId-1)*NumAtomicPerComp+1;
	 // needed for insert and delete tests
	 shared_cp = new BAIdList[TotalCompParts+numXacts*(MAXCLIENTS*NumNewCompParts)+1];
	 private_cp = new BAIdList[TotalCompParts+numXacts*(MAXCLIENTS*NumNewCompParts)+1];

	// purge the caches if so desired
//	purgeVar = getenv ("PURGE");
//	if (purgeVar != NULL)
//	{
//		if (atoi(purgeVar) == 1)
//		{
//			if ((err = BeginTransaction()) != 0 )
//	    		{
//	    		    fprintf(stderr, "Xact failure (on purge)!\n");
//	    		    exit(1);
//	    		}
//			printf("purginging db \n");
//			Purge();
//			printf("done purging db\n");
//			Commit();
//		}
//		else printf("not purging db\n");
//	}
//	else printf("not purging db\n");

	// initialize the random number generator in a way that will
	// avoid lockstep access behavior if N of these run concurrently
	pid_t pid;
	char state[8];
	pid = getpid();
	initstate((int) pid, state, 8);

	// initialize timing info
	MultiStats timingData(numXacts);
//	totalTime.Start();
//	totalSrvTime.Start();


        // finally, repeatedly run the darn thing
	for (int xactNo = 0; xactNo < numXacts; xactNo++) {
	    int rp=0;
	    int rs=0;
	    int wp=0;
	    int ws=0;

  	    // get starting wall clock time and usage values
            gettimeofday(&startWallTime, IGNOREZONE &ignoreZone);
	    getrusage(RUSAGE_SELF, &startUsage);
            backupCompId=nextCompositeId;
            backupAtomId=nextAtomicId;
	    // start a new transaction
begin_xact:
//	    printf("DEBUG: calling begin transaction\n");
//	    fflush(stdout);
	    if (oc.begin_transaction(3)){

//	        printf("DEBUG: system-induced abort occurred\n");
//	        fflush(stdout);
		numRestarts++;
		goto begin_xact;
	    }

	    nextCompositeId=backupCompId;
	    nextAtomicId=backupAtomId;

// this backup thing is to take care of restart during insert/delete
//	    printf("DEBUG: begin transaction done, err = %d\n", err);
//          fflush(stdout);


	    // seed the random number generator in a repeatable way (in
	    // case the transaction aborts and has to be executed again)
	    initstate((int) pid + xactNo, state, 8);

//	    printf("DEBUG:  random() = %d\n", random());
//	    fflush(stdout);

	    // choose a module to run the transaction on, based on the
	    // number of this client
//	    int moduleId = (int) (random() % TotalModules) + 1;
	    int moduleId = clientNo;
#ifdef USE_MODULE_INDEX
            sprintf(moduleName, "Module %08d", moduleId);
            /* printf("just checking1\n");fflush(stdout);*/
	    moduleH = (Module*) ModuleIdx->lookup(moduleName);
#else
            sprintf(moduleName, "Module%d", moduleId);
	    printf("moduleName=%s, moduleId=%d\n", moduleName, moduleId);
	    moduleH = REF(Module)::lookup(moduleName);
#endif
	    if (moduleH == NULL)
	    {
	        fprintf(stderr, "ERROR: Cannot access %s.\n", moduleName);
		exit(1);
	    }
    
	    // Perform the requested operation(s) on the chosen module
	    for (int opNo = 0; opNo < numOps; opNo++){
	    
	        int count = 0;
	        int docCount = 0;
	        int charCount = 0;
	        int replaceCount = 0;

                int selectOp;

		selectOp=random() % 100;
		// printf("op : %d\n",selectOp);

		if (selectOp < RP)
		  whichOp=PrivateRead;
                else
		if (selectOp < (RP+RS))
		  whichOp=SharedRead;
                else
		if (selectOp < (RP+RS+WP))
		   whichOp=PrivateWrite;
                else 
		   whichOp=SharedWrite;

	        switch (whichOp) {
                    case PrivateRead:
                        count = moduleH->newtraverse(whichOp);
			rp++;
                        sprintf(resultText, "xactNo = %d, opNo = %d: PrivateRead touched %d atomic parts.\n", xactNo, opNo, count);
                        break;
                    case SharedRead:
                        count = moduleH->newtraverse(whichOp);
			rs++;
                        sprintf(resultText, "xactNo = %d, opNo = %d: Shared touched %d atomic parts.\n", xactNo, opNo, count);
                        break;
                    case PrivateWrite:
	//		printf("here");
                        count = moduleH->newtraverse(whichOp);
			wp++;
                        sprintf(resultText, "xactNo = %d, opNo = %d: PrivateWrite touched %d atomic parts.\n", xactNo, opNo, count);
                        break;
                    case SharedWrite:
                        count = moduleH->newtraverse(whichOp);
			ws++;
                        sprintf(resultText, "xactNo = %d, opNo = %d: SharedWrite touched %d atomic parts.\n", xactNo, opNo, count);
                        break;
	            default:
	                fprintf(stderr, "Operation not available yet.\n");
		        exit(1);
	        }
		usleep(MultiSleepTime);

	        // DEBUG:  print out result text to show ops
                // fprintf(stdout, resultText);
	      }
    

//	    printf("DEBUG: about to commit\n");
//	    fflush(stdout);
            printf("rp=%d, rs=%d, wp=%d, ws=%d\n",rp,rs,wp,ws);


	    // Commit the current transaction if we are running the 
	    // last iteration or running a multitransaction test
	if(oc.commit_transaction()){
	        printf("Error in Deadlock detection\n");
			exit(1);
		numRestarts++;
		goto begin_xact;
	}
//       printf("DEBUG: done committing\n");
//	fflush(stdout);

            // compute and record the wall clock time and CPU times
            gettimeofday(&endWallTime, IGNOREZONE &ignoreZone);
	    getrusage(RUSAGE_SELF, &endUsage);
	    timingData.AddToStats(
               ComputeWallClockTime(&startWallTime, &endWallTime),
	       ComputeUserTime(&startUsage, &endUsage),
	       ComputeSystemTime(&startUsage, &endUsage));

	  }
	//////////////////////////////////////////////////////////////////
	//
	// Shutdown 
	//
	//////////////////////////////////////////////////////////////////

//	totalTime.Stop();
//	totalSrvTime.Stop();

	printf("SUMMARY OF MULTI-USER TIMING RESULTS:\n");
	timingData.PrintStats();
	printf("(Number of transaction restarts: %d)\n", numRestarts);

//	fprintf(stdout, "Total stats (client,server):\n");
//	totalTime.PrintStatsHeader(stdout);
//	totalTime.PrintStats(stdout, "TotalCli");
//	totalSrvTime.PrintStats(stdout, "TotalSrv");

	// Exit
	exit(0);
      }


/*$Header: /p/shore/shore_cvs/src/oo7/GenBench.C,v 1.7 1996/07/23 19:33:23 nhall Exp $*/
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

#include "getrusage.h"

//#include <E/collection.h>

/* new */
#include <stdlib.h> 

#include "oo7.h"
#include "globals.h"
#include "GenParams.h"
#include "VarParams.h"
// #include "IntBtree.h"
// #include "StringBtree.h"
// #include "stats.h"
// #include "stats_server.h"
#include "baidlist.h"
#include "random.h"

int overFlowCnt;
int NoLogging = 1;
// this should be a command line flag.
// this should also be integerated into the E runtime.
// well maybe.

//////////////////////////////////////////////////////////////////
//
// Global Variables, etc., for Benchmark Database Generation
//
//////////////////////////////////////////////////////////////////

struct rusage startUsage, endUsage;
struct timeval startWallTime;
struct timeval endWallTime;

//extern void SwizzleCache();
//extern void UnSwizzleCache();
extern SetParams(char* configFileName);

extern double ComputeWallClockTime(struct timeval *startWallTime, 
	           struct timeval *endWallTime);

extern double ComputeUserTime(struct rusage *startUsage, 
	           struct rusage *endUsage);

extern double ComputeSystemTime(struct rusage *startUsage, 
	           struct rusage *endUsage);

int	nextAtomicId=1;
int	nextCompositeId=1;
int	nextComplexAssemblyId=1;
int	nextBaseAssemblyId=1;
int	nextModuleId=1;

char    *types[NumTypes] = {
		"type000", "type001", "type002", "type003", "type004",
		"type005", "type006", "type007", "type008", "type009"
	};

extern void createDummy(int);

// data structures to keep track of which composite parts each base
// assemblies uses.  This is done so that we can generate the base
// assemblies first and then the composite parts.  Why bother?
// In particular, if you did the composite parts first and the base
// assemblies, there would be no need for these data structures.
// This second approach worked fine for the small and medium databases
// but not for the large as the program seeked all over the disk.

BAIdList* private_cp;
BAIdList* shared_cp;

//////////////////////////////////////////////////////////////////
//
// Do Benchmark Data Generation
//
//////////////////////////////////////////////////////////////////

REF(Module) g_module;
void gendb()
{
        //int 		partDate;
        int 		id /* , typeNo, docLen */ ;
	char 		moduleName[40];
	REF(Module)		moduleH;
	REF(CompositePart)	cp;

	// create all the indices needed
	//AtomicPartIdx = new (CompPartSet) IntBtIndex;
	// AtomicPartIdx = AtomicPartIdx.new_persistent(CompPartSet);
	AtomicPartIdx = new (CompPartSet) IntBtIndex;
	AtomicPartIdx.update()->init();

	// CompPartIdx = new (CompPartSet) IntBtIndex;
	// CompPartIdx = CompPartIdx.new_persistent(CompPartSet);
	CompPartIdx = new (CompPartSet) IntBtIndex;
	CompPartIdx.update()->init();

	// DocumentIdx = new (DocumentSet) StringBtIndex;
	// DocumentIdx= DocumentIdx.new_persistent(DocumentSet);
	DocumentIdx = new (DocumentSet) StringBtIndex;
	DocumentIdx.update()->init();

	DocumentIdIdx = new (DocumentSet) IntBtIndex;
	// DocumentIdIdx = DocumentIdIdx.new_persistent(DocumentSet);
	DocumentIdIdx.update()->init();

	ModuleIdx = new (ModuleSet) StringBtIndex;
	// ModuleIdx = ModuleIdx.new_persistent(ModuleSet);
	ModuleIdx.update()->init();

	BaseAssemblyIdx = new (ModuleSet) IntBtIndex;
	// BaseAssemblyIdx = BaseAssemblyIdx.new_persistent(ModuleSet);
	BaseAssemblyIdx.update()->init();

	printf("TotalCompParts = %d\n",TotalCompParts);

	shared_cp = new BAIdList[TotalCompParts+1];
	private_cp = new BAIdList[TotalCompParts+1];

	// First generate the desired number of modules
	while (nextModuleId <= TotalModules) 
	{
	    int id = nextModuleId++;
            sprintf(moduleName, "Module %08d", id);
	    // moduleH = new (ModuleSet) Module (id);
	    // moduleH = moduleH.new_persistent(ModuleSet);
	    moduleH = new (ModuleSet) Module ;
	    moduleH.update()->init(id);

	    ModuleIdx.update()->insert(moduleName, moduleH);  // add module to index
	    g_module = moduleH;
	    // temporary hack: don't have indexes implemented.
	    AllModules.update()->add(moduleH); // add module to extent
	    // E_CommitTransaction();
	    // E_BeginTransaction();
	    // if (NoLogging)
		// sm_SetLogLevel(LOG_SPACE, 0, 0);
	}

	printf("generated all the modules, now generated the comp parts\n");
	// now generate the composite parts
	while (nextCompositeId <= TotalCompParts) 
	{
	    id = nextCompositeId++;
	    // cp = new (CompPartSet) CompositePart(id);
	    // cp = cp.new_persistent(CompPartSet);
	    cp = new (CompPartSet) CompositePart;
	    cp.update()->init(id);

	    CompPartIdx.update()->insert(id, cp);  // add composite part to its index
	    // dts 3-11-93 AllCompParts is not used, so don't maintain it.
	    // (used in Reorg code though, but that's not used currently)
	    // AllCompParts.add(cp);  // add composite part to its extent 
	    if ((nextCompositeId % 25)==0)
	    {
		printf("progress : nextCompositeId=%d\n", nextCompositeId);
		if ((nextCompositeId%100)==0)
		{
			// E_CommitTransaction();
			// E_BeginTransaction();
			// if (NoLogging)
			    // sm_SetLogLevel(LOG_SPACE, 0, 0);
			printf("intermediate commmit done\n");
		}
	    }
        }
	// E_CommitTransaction();

	// Print out some useful information about what happened

	printf("=== DONE CREATING DATABASE, TOTALS WERE ===\n");
	printf("    # atomic parts\t\t%d\n", nextAtomicId-1);
	printf("    # composite parts\t\t%d\n", nextCompositeId-1);
	printf("    # complex assemblies\t%d\n", nextComplexAssemblyId-1);
	printf("    # base assemblies\t\t%d\n", nextBaseAssemblyId-1);
	printf("    # modules\t\t\t%d\n", nextModuleId-1);
	printf("    # overFlowCnt\t\t\t%d\n", overFlowCnt);

	//////////////////////////////////////////////////////////////////
	//
	// Shutdown DB
	//
	//////////////////////////////////////////////////////////////////

        // compute and report wall clock time
        gettimeofday(&endWallTime, IGNOREZONE &ignoreZone);
        printf("Wall-Clock time to generate database: %f seconds.\n", 
	             ComputeWallClockTime(&startWallTime, &endWallTime));

	getrusage(RUSAGE_SELF, &endUsage);
	fprintf(stdout, "(CPU: %f seconds.)\n", 
	                ComputeUserTime(&startUsage, &endUsage) +
			ComputeSystemTime(&startUsage, &endUsage));

}


extern w_rc_t InitGlobals(); // for SDL, a dummy routing.

extern int RealWork; // set to one to make atomicpart::DoNothing
		// actually do some work
extern int WorkAmount;   // controls how much work DoNothings do


extern int  traverse7();
extern int  query1();
extern int  query2();
extern int  query3();
extern int  query4();
extern int  query5();
extern int  query6();
extern int  query7();
extern int  query8();
extern void insert1();
extern void delete1();
extern int  reorg1();
extern int  reorg2();
extern void Purge();


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

#include "BenchParams.h"
struct timeval startWarmTime;






///////////////////////////////////////////////////////////////////////////
// ParseCommandLine parses the original shell call to "bench", determining 
// which operation to run, how many times to run it, and whether the
// individual runs are distinct transactions or are lumped together
// into one large transaction.
//////////////////////////////////////////////////////////////////////////
void ParseCommandLine(int argc, 
		      char**argv, 
		      int& opIndex,
		      int& repeatCount,
                      BenchmarkOp& whichOp,
			  int& manyXACTS) 
{
	printf("Call was: ");
	for (int foo = 0; foo < argc; foo++) {
		printf("%s  ", argv[foo]);
	}
	printf("\n");
	opIndex = 3;

	if (strcmp(argv[argc-1], "-d") == 0) debugMode = 1;
        else debugMode = 0;
        if (argc < 5)
        {
	    fprintf(stderr, "Usage: %s <configFileName> <count> <op> <xacts> [-d]\n", 
		argv[0]);
	    fprintf(stderr, "count = number of times to repeat op.\n");
	    fprintf(stderr, "Currently supported <op>s: t1, t1ww, t2a, t2b, t2c, t3a, t3b, t3c\n");
	    fprintf(stderr, "\t t4, t5do, t5undo, t6, t7, q1, q1ww, q2, q3, q4, q5, q6, q7, q8\n");
	    fprintf(stderr, "\t i, d, r1, r2, wu, m1, m2, m3, m4, m5, m6.\n");
	    fprintf(stderr, "<xacts> = single (all repetitions done in one xact)\n");
	    fprintf(stderr, "or many (each repetition a distinct xact.)\n");
	    exit(1);
	}

	if ((repeatCount = atoi(argv[2])) <= 0) 
	{
	    fprintf(stderr, "Usage: %s <configFileName> <count> <op> <xacts> [-d]\n", argv[0]);
	    exit(1);
        } 
	
	if (strcmp(argv[4], "many") == 0) {
            manyXACTS = 1;
        } 
	if (strcmp(argv[opIndex], "t1") == 0) {
	    whichOp = Trav1;
	} else if (strcmp(argv[opIndex], "t1ww") == 0) {
	    whichOp = Trav1WW;
	    if (debugMode ) WorkAmount = atoi(argv[argc-2]);
            else WorkAmount = atoi(argv[argc-1]);
            printf("WorkAmount = %d\n", WorkAmount);
	} else if (strcmp(argv[opIndex], "t2a") == 0) {
	    whichOp = Trav2a;
	} else if (strcmp(argv[opIndex], "t2b") == 0) {
	    whichOp = Trav2b;
	} else if (strcmp(argv[opIndex], "t2c") == 0) {
	    whichOp = Trav2c;
	} else if (strcmp(argv[opIndex], "t3a") == 0) {
	    whichOp = Trav3a;
	} else if (strcmp(argv[opIndex], "t3b") == 0) {
	    whichOp = Trav3b;
	} else if (strcmp(argv[opIndex], "t3c") == 0) {
	    whichOp = Trav3c;
	} else if (strcmp(argv[opIndex], "t4") == 0) {
	    whichOp = Trav4;
	} else if (strcmp(argv[opIndex], "t5do") == 0) {
	    whichOp = Trav5do;
	} else if (strcmp(argv[opIndex], "t5undo") == 0) {
	    whichOp = Trav5undo;
	} else if (strcmp(argv[opIndex], "t6") == 0) {
	    whichOp = Trav6;
	} else if (strcmp(argv[opIndex], "t7") == 0) {
	    whichOp = Trav7;
	} else if (strcmp(argv[opIndex], "t8") == 0) {
	    whichOp = Trav8;
	} else if (strcmp(argv[opIndex], "t9") == 0) {
	    whichOp = Trav9;
	} else if (strcmp(argv[opIndex], "t10") == 0) {
	    whichOp = Trav10;
	} else if (strcmp(argv[opIndex], "q1") == 0) {
	    whichOp = Query1;
	} else if (strcmp(argv[opIndex], "q2") == 0) {
	    whichOp = Query2;
	} else if (strcmp(argv[opIndex], "q3") == 0) {
	    whichOp = Query3;
	} else if (strcmp(argv[opIndex], "q4") == 0) {
	    whichOp = Query4;
	} else if (strcmp(argv[opIndex], "q5") == 0) {
	    whichOp = Query5;
	} else if (strcmp(argv[opIndex], "q6") == 0) {
	    whichOp = Query6;
	} else if (strcmp(argv[opIndex], "q7") == 0) {
	    whichOp = Query7;
	} else if (strcmp(argv[opIndex], "q8") == 0) {
	    whichOp = Query8;
	} else if (strcmp(argv[opIndex], "i") == 0) {
	    whichOp = Insert;
	} else if (strcmp(argv[opIndex], "d") == 0) {
	    whichOp = Delete;
	} else if (strcmp(argv[opIndex], "r1") == 0) {
	    whichOp = Reorg1;
	} else if (strcmp(argv[opIndex], "r2") == 0) {
	    whichOp = Reorg2;
// NEW
	} else if (strcmp(argv[opIndex], "wu") == 0) {
	    whichOp = WarmUpdate;
	} else {
	    fprintf(stderr, "ERROR: Illegal OO7 operation specified.\n");
	    fprintf(stderr, "Currently supported <op>s: t1, t1ww, t2a, t2b, t2c, t3a, t3b, t3c\n");
	    fprintf(stderr, "\t t4, t5do, t5undo, t6, t7, t8, t9, q1, q2, q3, q4, q5, q6, q7, q8\n");
	    fprintf(stderr, "\t i, d, r1, r2 wu\n");
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

        char  resultText[200];  // buffer to hold result of operation for
                                // printing outside of timing region.
	w_rc_t rc;

	// initialize parameters for benchmark.
	SetParams(argv[1]);
	rc = InitGlobals();
	if(rc){
	    cerr << "Error in InitGlobals: " << rc << endl;
	    exit(1);
	}

	gendb(); // generate the database

	// try to swizzle/unswizzle cache, just for testing.
	// UnSwizzleCache();
	// SwizzleCache();
	nextAtomicId  = TotalAtomicParts + 1;
	nextCompositeId = TotalCompParts + 1;

	// Compute structural info needed by the update operations,
        // since these operations need to know which id's should
        // be used next.

	int baseCnt = NumAssmPerAssm;
	int complexCnt = 1;
	for (int i = 1; i < NumAssmLevels-1; i++) {
            baseCnt = baseCnt * NumAssmPerAssm;
            complexCnt += complexCnt * NumAssmPerAssm;
	}
	nextBaseAssemblyId = TotalModules*baseCnt + 1;
	nextComplexAssemblyId = TotalModules*complexCnt + 1;
	nextAtomicId = TotalAtomicParts + 1;
	nextCompositeId = TotalCompParts + 1;

	int opIndex = 2;
	int repeatCount = 1;
	BenchmarkOp whichOp = Trav1;
	int manyXACTS = 0;

	// needed for insert and delete tests
	shared_cp = new BAIdList[TotalCompParts+NumNewCompParts+1];
	private_cp = new BAIdList[TotalCompParts+NumNewCompParts+1];


	// See if debug mode is desired, see which operation to run,
	// and how many times to run it.
	ParseCommandLine(argc, argv, opIndex, repeatCount, whichOp, manyXACTS); 


	// totalTime.Start();
	// totalSrvTime.Start();

        // Actually run the darn thing.
	for (int iter = 0; iter < repeatCount; iter++) 
	{
	    //////////////////////////////////////////////////////////////////
	    // Run an OO7 Benchmark Operation
	    //
	    //////////////////////////////////////////////////////////////////

	    printf("RUNNING OO7 BENCHMARK OPERATION %s, iteration = %d.\n", 
	           argv[opIndex], iter);

  	    // get wall clock time
            gettimeofday(&startWallTime, IGNOREZONE &ignoreZone);

	    // get starting usage values.
	    getrusage(RUSAGE_SELF, &startUsage);

	    // Start a new transaction if either this is the first iteration
	    // of a multioperation transaction or we we are running each
	    // operate as a separate transaction

	    if ((iter == 0) || (manyXACTS)) ;// E_BeginTransaction();

            // set random seed so "hot" runs are truly hot
            srandom(1);

	    // Use random module for the operation
            int moduleId = (int) (random() % TotalModules) + 1;
            sprintf(moduleName, "Module %08d", moduleId);
//	    printf("moduleName=%s\n",moduleName);
	    moduleH =  ModuleIdx.update()->lookup(moduleName);
	    // SDL BAD temporary hack: don't have indexes implemented.
	    moduleH = g_module;
	    if (moduleH == NULL)
	    {
	        fprintf(stderr, "ERROR: Unable to access %s.\n", moduleName);
		// E_AbortTransaction();
	        exit(1);
	    }

	    // Perform the requested operation on the chosen module
	    long count = 0;
	    int docCount = 0;
	    int charCount = 0;
	    int replaceCount = 0;

	    switch (whichOp) {
	        case Trav1:
	            count = moduleH->traverse(whichOp);
	            sprintf(resultText, "Traversal 1 DFS visited %d atomic parts.\n",
			             count);
  		    break;
		 case Trav1WW:
                    RealWork = 1;
                    whichOp = Trav1;  // so traverse methods don't complain
                    count = moduleH->traverse(whichOp);
                    whichOp = Trav1WW;  // for next (hot) traversal
                    sprintf(resultText, "Traversal 1WW DFS visited %d atomic parts.\n",
                                     count);
                    break;
	        case Trav2a:
	            count = moduleH->traverse(whichOp);
	            sprintf(resultText, "Traversal 2A swapped %d pairs of (X,Y) coordinates.\n",
 			         count);
		    break;
	        case Trav2b:
	            count = moduleH->traverse(whichOp);
	            sprintf(resultText, "Traversal 2B swapped %d pairs of (X,Y) coordinates.\n",
			             count);
		    break;
	        case Trav2c:
	            count = moduleH->traverse(whichOp);
	            sprintf(resultText, "Traversal 2C swapped %d pairs of (X,Y) coordinates.\n",
			             count);
		    break;
	        case Trav3a:
	            count = moduleH->traverse(whichOp);
	            sprintf(resultText, "Traversal 3A toggled %d dates.\n",
			             count);
		    break;
	        case Trav3b:
	            count = moduleH->traverse(whichOp);
	            sprintf(resultText, "Traversal 3B toggled %d dates.\n",
			             count);
		    break;
	        case Trav3c:
	            count = moduleH->traverse(whichOp);
	            sprintf(resultText, "Traversal 3C toggled %d dates.\n",
			            count);
		    break;
	        case Trav4:
	            count = moduleH->traverse(whichOp);
	            sprintf(resultText, "Traversal 4: %d instances of the character found\n",
			             count);
		    break;
	        case Trav5do:
	            count = moduleH->traverse(whichOp);
	            sprintf(resultText, "Traversal 5(DO): %d string replacements performed\n",
			             count);
		    break;
	        case Trav5undo:
	            count = moduleH->traverse(whichOp);
	            sprintf(resultText, "Traversal 5(UNDO): %d string replacements performed\n",
			         count);
		    break;
	        case Trav6:
	            count = moduleH->traverse(whichOp);
	            sprintf(resultText, "Traversal 6: visited %d atomic part roots.\n",
			             count);
		    break;
	        case Trav7:
	            count = traverse7();
		    sprintf(resultText, "Traversal 7: found %d assemblies using rand om atomic part.\n", 
			count);
		    break;
	        case Trav8:
	            count = moduleH->scanManual();
		    sprintf(resultText, "Traversal 8: found %d occurrences of character in manual.\n", 
			count);
		    break;
	        case Trav9:
	            count = moduleH->firstLast();
		    sprintf(resultText, "Traversal 9: match was %d.\n", 
			count);
		    break;

                case Trav10:
                    // run traversal #1 on every module.
                    count = 0;
                    whichOp = Trav1;  // so object methods don't complain
                    for (moduleId = 1; moduleId <= TotalModules; moduleId++) {
                        sprintf(moduleName, "Module %08d", moduleId);
	  	        moduleH =  ModuleIdx.update()->lookup(moduleName);
   	                if (moduleH == NULL) {
                                fprintf(stderr,
                                        "ERROR: t10 Unable to access %s.\n",
                                         moduleName);
                                // E_AbortTransaction();
                                exit(1);
                        }
                        count += moduleH->traverse(whichOp);
                    }
                    sprintf(resultText,
                           "Traversal 10 visited %d atomic parts in %d modules.\\n",
                                     count, TotalModules);
                    whichOp = Trav10;  // for next time around
                    break;           

	        case Query1:
	            count = query1();
	            sprintf(resultText, "Query one retrieved %d atomic parts.\n",
			             count);
		    break;
	        case Query2:
	            count = query2();
	            sprintf(resultText, "Query two retrieved %d qualifying atomic parts.\n",
			         count);
		    break;
	        case Query3:
	            count = query3();
	            sprintf(resultText, "Query three retrieved %d qualifying atomic parts.\n",
			         count);
		    break;
	        case Query4:
	            count = query4();
	            sprintf(resultText, "Query four retrieved %d (document, base assembly) pairs.\n",
			         count);
		    break;
	        case Query5:
	            count = query5();
	            sprintf(resultText, "Query five retrieved %d out-of-date base assemblies.\n",
			             count);
		    break;
	        case Query6:
	            count = query6();
	            sprintf(resultText, "Query six retrieved %d out-of-date assemblies.\n",
			         count);
		    break;
	        case Query7:
	            count = query7();
		    sprintf(resultText, "Query seven iterated through %d atomic part s.\n",
			             count);
		    break;
	        case Query8:
	            count = query8();
		    sprintf(resultText, "Query eight found %d atomic part/document m atches.\n",
			 count);
		    break;
	        case Insert:
	            insert1();
	            sprintf(resultText, "Inserted %d composite parts (a total of %d atomic parts.)\n",
		      NumNewCompParts, NumNewCompParts*NumAtomicPerComp);
		    break;
	        case Delete:
	            delete1();
	            sprintf(resultText, "Deleted %d composite parts (a total of %d atomic parts.)\n",
	             NumNewCompParts, NumNewCompParts*NumAtomicPerComp);
		    break;

		 case Reorg1:
		     count = reorg1();
		     sprintf(resultText, "Reorg1 replaced %d atomic parts.\n", 
			count);
		     break;

	    	 case Reorg2:
		     count = reorg2();
		     sprintf(resultText, "Reorg2 replaced %d atomic parts.\n", 
			count);
		     break;
// NEW
	        case WarmUpdate:
		    // first do the t1 traversal to warm the cache
	            count = moduleH->traverse(Trav1);
		    // then call T2 to do the update
	            count = moduleH->traverse(Trav2a);
	            sprintf(resultText, 
			"Warm update swapped %d pairs of (X,Y) coordinates.\n",
 			         count);
		     break;
	        default:
	            fprintf(stderr, "Sorry, that operation isn't available yet.\n");
		    // E_AbortTransaction();
	            exit(1);
	    }

	    // Commit the current transaction if we are running the 
	    // last iteration or running a multitransaction test
	    if ((iter == repeatCount-1) || (manyXACTS)) ;//E_CommitTransaction();

            // compute and report wall clock time
            gettimeofday(&endWallTime, IGNOREZONE &ignoreZone);
	    printf("EXODUS, operation=%s, iteration=%d, elapsedTime=%f seconds\n",
               argv[opIndex], iter,
               ComputeWallClockTime(&startWallTime, &endWallTime));
            if (iter == 1) startWarmTime = startWallTime;

            // Compute and report CPU time.
	    getrusage(RUSAGE_SELF, &endUsage);
            fprintf(stdout, resultText);
	    fprintf(stdout, "CPU time: %f seconds.\n", 
	                ComputeUserTime(&startUsage, &endUsage) +
			ComputeSystemTime(&startUsage, &endUsage));
	    fprintf(stdout, "(%f seconds user, %f seconds system.)\n", 
	                ComputeUserTime(&startUsage, &endUsage),
			ComputeSystemTime(&startUsage, &endUsage));

	    if ((repeatCount > 2) && (iter == repeatCount-2)) 
	    {
	       // compute average hot time for 2nd through n-1 th iterations
               printf("EXODUS, operation=%s, average hot elapsedTime=%f seconds\n",
	       	  argv[opIndex], 
	          ComputeWallClockTime(&startWarmTime, &endWallTime)/(repeatCount-2)); 
	    }
	}

	//////////////////////////////////////////////////////////////////
	//
	// Shutdown 
	//
	//////////////////////////////////////////////////////////////////

	// totalTime.Stop();
	// totalSrvTime.Stop();
	// fprintf(stdout, "Total stats (client,server):\n");
	// totalTime.PrintStatsHeader(stdout);
	// totalTime.PrintStats(stdout, "TotalCli");
	// totalSrvTime.PrintStats(stdout, "TotalSrv");

	// Exit
	exit(0);
}


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
#include <string.h>
#include <stdio.h>
#include <unistd.h>

#include "oo7.h"
#include "BenchParams.h"
#include "GenParams.h"
#include "VarParams.h"
#include "globals.h"
#include "random.h"
#include "AssocItr.h"
#include "PartIdSet.h"

//#include "IntBtree.h"
//#include "StringBtree.h"

//////////////////////////////////////////////////////////////////
//
// Traverse  - DFS traverse module.  Upon reaching a base assembly,
// visit all referenced composite parts.  At each composite part,
// take an action that depends on which traversal variant has been
// requested.
//
///////////////////////////////////////////////////////////////////



extern void PrintOp(BenchmarkOp op);

//////////////////////////////////////////////////////////////////
//
// Module Method for Traversal 
//
//////////////////////////////////////////////////////////////////


int Module::newtraverse(BenchmarkOp op) const
{
    if (debugMode) {
        printf("Module::newtraverse(id = %d, op = ", id);
	PrintOp(op);
	printf(")\n");
    }

    // now traverse the assembly hierarchy
    return designRoot->newtraverse(op);
}

//////////////////////////////////////////////////////////////////
//
// ComplexAssembly Method for Traversal
//
//////////////////////////////////////////////////////////////////


int ComplexAssembly::newtraverse(BenchmarkOp op) const
{
    AssocItr* assmI;  // iterator for traversing subassemblies
    REF(Assembly) assm;

    if (debugMode) {
        printf("\tComplexAssembly::newtraverse(id = %d, op = ", id);
	PrintOp(op);
	printf(")\n");
    }

    // traverse each of the assembly's subassemblies
    int count = 0;

	// randomly walk down the assembly hierarchy
	int curPath = 0;
	int randPath = (int) (random() % NumAssmPerAssm);
        assmI = new AssocItr(subAssemblies);  // establish subassembly iterator
        assm = assmI->next(); 
        while (assm != NULL)
        {
	    if (curPath++ == randPath)
	    {
	        count += assm->newtraverse(op);
		break;
	    }
            assm =  assmI->next();
        }
        delete assmI;

    // lastly, return the result
    return count;
}


//////////////////////////////////////////////////////////////////
//
// BaseAssembly Method for Traversal 
//
//////////////////////////////////////////////////////////////////


int BaseAssembly::newtraverse(BenchmarkOp op) const
{
    AssocItr* compI;  // iterator for traversing composite parts
    REF(CompositePart) cp;
    REF(CompositePart) privatePart;
    REF(CompositePart) sharedPart;

    if (debugMode) {
        printf("\t\tBaseAssembly::newtraverse(id = %d, op = ", id);
	PrintOp(op);
	printf(")\n");
    }

    // prepare for and execute the traversal
    int count = 0;


	// do random traversal(s) of (a) selected composite part(s)

	int curPath;
	int randPath;

        // first, traverse a private and/or shared composite part

        if (op == PrivateRead)
	{
	    curPath = 0;
	    randPath = (int) (random() % NumCompPerAssm);
            compI = new AssocItr(componentsPriv);
            cp =  compI->next(); 
            while (cp != NULL)
            {
                if (curPath++ == randPath) 
		{
		    privatePart = cp;
		    count += cp->traverse(Trav1);
		    break;
		}
                cp =  compI->next(); 
            }
            delete compI;
        }
	else
        if (op == SharedRead)
	{
	    curPath = 0;
	    randPath = (int) (random() % NumCompPerAssm);
            compI = new AssocItr(componentsShar);
            cp = compI->next(); 
            while (cp != NULL)
            {
                if (curPath++ == randPath) 
		{
		    sharedPart = cp;
		    count += cp->traverse(Trav1);
		    break;
		}
                cp =  compI->next(); 
            }
            delete compI;
        }
	else
        if (op == PrivateWrite)
	{
	    curPath = 0;
	    randPath = (int) (random() % NumCompPerAssm);
            compI = new AssocItr(componentsPriv);
            cp =  compI->next(); 
            while (cp != NULL)
            {
                if (curPath++ == randPath) 
		{
		    privatePart = cp;
		    count += cp->traverse(Trav2b);
		    break;
		}
                cp =  compI->next(); 
            }
            delete compI;
        }
	else
        if (op == SharedWrite)
	{
	    curPath = 0;
	    randPath = (int) (random() % NumCompPerAssm);
            compI = new AssocItr(componentsShar);
            cp =  compI->next(); 
            while (cp != NULL)
            {
                if (curPath++ == randPath) 
		{
		    sharedPart = cp;
		    count += cp->traverse(Trav2b);
		    break;
		}
                cp =  compI->next(); 
            }
            delete compI;
        }
        else
        if (op == PrivateWrite_D)
	{
	    curPath = 0;
	    randPath = (int) (random() % NumCompPerAssm);
            compI = new AssocItr(componentsPriv);
            cp =  compI->next(); 
            while (cp != NULL)
            {
                if (curPath++ == randPath) 
		{
		    privatePart = cp;
		    count += cp->traverse(Trav2d);
		    break;
		}
                cp =  compI->next(); 
            }
            delete compI;
        }
	else
        if (op == SharedWrite_D)
	{
	    curPath = 0;
	    randPath = (int) (random() % NumCompPerAssm);
            compI = new AssocItr(componentsShar);
            cp =  compI->next(); 
            while (cp != NULL)
            {
                if (curPath++ == randPath) 
		{
		    sharedPart = cp; 
		    count += cp->traverse(Trav2d);
		    break;
		}
                cp =  compI->next(); 
            }
            delete compI;
         }

        if (op == MultiTrav3b)
	{
	    curPath = 0;
	    randPath = (int) (random() % NumCompPerAssm);
            compI = new AssocItr(componentsShar);
            cp =  compI->next(); 
            while (cp != NULL)
            {
                if (curPath++ == randPath) 
		{
		    sharedPart = cp;
		    count += cp->traverse(Trav3b);
		    break;
		}
                cp =  compI->next(); 
            }
            delete compI;
         }

    // lastly, return the result
    return count;
}



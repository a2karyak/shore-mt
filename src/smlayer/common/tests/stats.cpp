/*<std-header orig-src='shore'>

 $Id: stats.cpp,v 1.14 2003/06/19 18:43:51 bolo Exp $

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

#include <stdio.h>
#include <stdlib.h>
#include <w_strstream.h>
#include <iostream.h>
#include <stddef.h>
#include <limits.h>
#include <w.h>
#include <w_base.h>
#include <option.h>
#include <w_debug.h>

#include "w_statistics.h"
#include "test_stat.h"

void statstest();

int main()
{

	DBG(<<"app_test: main");
	statstest();
	return 0;
}

#ifdef __GNUC__
typedef w_auto_delete_array_t<char> gcc_kludge_1;
typedef w_list_i<option_t> 			gcc_kludge_0;
#endif /* __GNUC__*/


void
statstest()
{
	/*********w_statistics_t ***********/

	class w_statistics_t ST; // generic stats class
	class test_stat TST; // my test class that uses stats
	w_rc_t	e;

	// server side: implementor of RPC request does this:
	TST.compute();

	ST << TST;
	ST << TST;
	ST << TST;

	// then ST gets sent across the wire and re-instantiated
	// on client side

	// Various ways the contents of ST can be printed:
	{
		cout << "ST.printf():" << endl;
		ST.printf();
		cout << endl;

		cout << "cout << ST:" << endl;
		cout << ST;
		cout << endl;

		/*  
		 * Format some output my own way.
		 * This is a way to do it by calling
		 * on the w_statistics_t structure to provide
		 * the string name.
		 */
		cout << "My own pretty formatting for module : " << ST.module(TEST_i) << endl;
		W_FORM2(cout,("\t%-30.30s %10.10d", 
				ST.string(TEST_i), ST.int_val(TEST_i)) );
			cout << endl;
		W_FORM2(cout,("\t%-30.30s %10.10u", 
				ST.string(TEST_j), ST.uint_val(TEST_j)) );
			cout << endl;
		W_FORM2(cout,("\t%-30.30s %10.6f", 
				ST.string(TEST_k), ST.float_val(TEST_k)) );
			cout << endl;
#ifdef notyet
		W_FORM2(cout,("\t%-30.30s %10.10d", 
				ST.string(TEST_l), ST.float_val(TEST_l)) );
			cout << endl;
#endif
		W_FORM2(cout,("\t%-30.30s %10.10u", 
				ST.string(TEST_v), ST.ulong_val(TEST_v)) );
			cout << endl;
		cout << endl;

		/*
		 * Error cases:
		 */
		cout << "Expect some unknowns-- these are error cases:" <<endl;
		cout <<  ST.typechar(TEST_i) << ST.typestring(TEST_STATMIN) << endl;
		cout 	// <<  ST.typechar(-1,1) 
			<< ST.typestring(-1) 
				<< ST.string(-1,-1)<<  ST.float_val(-1,-1)
		<< endl;
	}
	{
		w_statistics_t *firstp = ST.copy_brief();
		w_statistics_t &first = *firstp;

		if(!firstp) {
			cout << "Could not copy_brief ST." << endl;
		
		} else {
			const w_statistics_t &CUR = ST;

			cout << __LINE__ <<  " :******* CUR.copy_brief(true): " << endl;
			first.printf();
			cout << endl;

			cout << __LINE__ << endl;
			TST.inc();
			cout << __LINE__<< " :******* TST.inc(): " << endl;
			CUR.printf();
			cout << endl;
		}
		{
			w_statistics_t *lastp = ST.copy_all();
			w_statistics_t &last = *lastp;

			if(!lastp) {
				cout << "Could not copy_all ST." << endl;
			
			} else {
				cout << __LINE__ << endl;

				cout << __LINE__ << endl;
				last -= first;
				cout << __LINE__ 
					<< " :******* last -= first: :" << endl;
				cout << last << endl;
				cout << endl;

				cout << __LINE__ << endl;
				last += first;
				cout << __LINE__ 
					<< " :******* last += first: :" << endl;
				cout << last << endl;
				cout << endl;

				cout << __LINE__ << endl;
				last.zero();
				cout << __LINE__ 
					<< " :******* last.zero(): :" << endl;
				cout << last << endl;
				cout << endl;

				cout << "Expect error:" << endl;

				ST -= first;
			}
			delete lastp;
		}
		delete firstp;
	}
	/********** end w_statistics_t tests ********/
}


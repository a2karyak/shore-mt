/*<std-header orig-src='shore' incl-file-exclusion='DTID_T_H'>

 $Id: dtid_t.h,v 1.13 1999/06/07 19:04:02 kupsch Exp $

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

#ifndef DTID_T_H
#define DTID_T_H

#include "w_defines.h"

/*  -- do not edit anything above this line --   </std-header>*/

#include <w.h>
#include <basics.h>
#include <memory.h>
#include <tid_t.h>

// distributed transaction type
// fixed size for us

class DTID_T {

public:
    NORET 	DTID_T();
    void 	update();
    void 	convert_to_gtid(gtid_t &g) const;
    void 	convert_from_gtid(const gtid_t &g);
	
private:
    static uint4_t unique();

    uint4_t	location;
    uint4_t	relative;
    char	date[26]; // Magic Number ... length of ctime() output
    char	nulls[2]; // pad to mpl of 4, 
	// and terminate the string with nulls.
};

inline void
DTID_T::convert_to_gtid(gtid_t &g) const
{
    w_assert3(sizeof(DTID_T) < max_gtid_len);
    g.zero();
    g.append(&this->location, sizeof(this->location));
    g.append(&this->relative, sizeof(this->relative));
    g.append(&this->date, sizeof(this->date));
}

inline void        
DTID_T::convert_from_gtid(const gtid_t &g)
{
    w_assert3(sizeof(DTID_T) < max_gtid_len);

    w_assert3(g.length() == sizeof(DTID_T));
    int i=0;

#define GETX(field) \
    memcpy(&this->field, g.data_at_offset(i), sizeof(this->field)); \
    i += sizeof(this->field);

    GETX(location);
    GETX(relative);
    GETX(date);
#undef GETX

}

/*<std-footer incl-file-exclusion='DTID_T_H'>  -- do not edit anything below this line -- */

#endif          /*</std-footer>*/

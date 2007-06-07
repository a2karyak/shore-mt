
/*<std-header orig-src='shore' incl-file-exclusion='SCOPED_RENAME_H'>

 $Id: scoped_rename.h,v 1.1 1999/11/11 21:02:12 bolo Exp $

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

#ifndef SCOPED_RENAME_H
#define SCOPED_RENAME_H

#include "w_defines.h"

/*  -- do not edit anything above this line --   </std-header>*/

class ScopedRename {
private:
	sthread_named_base_t	&me;
	char			*old_name;
	char			_default_name[64];

public:
	ScopedRename(sthread_named_base_t &_me, const char *new_name)
	: me(_me), old_name(_default_name)
	{
		const char	*name = _me.name();
		size_t	need = strlen(name) + 1;
		if (need > sizeof(_default_name)) {
			old_name = new char[need];
			TOR_CHECK_ALLOC(old_name);
		}
		strcpy(old_name, name);

		me.rename(new_name);
	}

	~ScopedRename()
	{
		me.rename(old_name);
		if (old_name != _default_name)
			delete [] old_name;
		old_name = 0;
	}
};

/*<std-footer incl-file-exclusion='SCOPED_RENAME_H'>  -- do not edit anything below this line -- */

#endif          /*</std-footer>*/

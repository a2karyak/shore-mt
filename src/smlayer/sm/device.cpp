/*<std-header orig-src='shore'>

 $Id: device.cpp,v 1.23 2003/06/19 22:39:34 bolo Exp $

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

#define SM_SOURCE
#define DEVICE_C

#ifdef __GNUG__
#   pragma implementation
#endif

#include "sm_int_0.h"
#include "device.h"

#ifdef EXPLICIT_TEMPLATE
template class w_list_t<device_s>;
template class w_list_i<device_s>;
template class w_list_const_i<device_s>;
#endif

device_m::device_m()
: _tab(W_LIST_ARG(device_s, link))
{
}

device_m::~device_m()
{
    w_assert1(_tab.is_empty());
}

w_rc_t device_m::mount(const char* dev_name, const device_hdr_s& dev_hdr, u_int& vol_cnt)
{
    device_s* dev = _find(dev_name);
    if (!dev) {
	devid_t devid(dev_name);
	dev = _find(devid);
	if (dev) {
	    // device is already mounted under a different name
	    return RC(eALREADYMOUNTED);
	}
	dev = new device_s;
	if (!dev) return RC(eOUTOFMEMORY);
	strncpy(dev->name, dev_name, sizeof(dev->name)-1);
	/* XXX possible loss of bits */
	dev->quota_pages = shpid_t(dev_hdr.quota_KB/(page_sz/1024));
	dev->id = devid;
	_tab.append(dev);
    }

    // count the number of volume on the device (for now max is 1)
    vol_cnt = 0;
    if (dev_hdr.lvid != lvid_t::null) vol_cnt++;

    return RCOK;
}

w_rc_t device_m::dismount(const char* dev_name)
{

    device_s* dev = _find(dev_name);
    if (!dev) return RC(eDEVNOTMOUNTED);
    dev->link.detach();
    delete dev;
    return RCOK;
}

w_rc_t device_m::dismount_all()
{
    w_list_i<device_s> scan(_tab);
    while(scan.next()) {
	scan.curr()->link.detach();
	delete scan.curr();
    }
    w_assert1(_tab.is_empty());
    return RCOK;
}

bool device_m::is_mounted(const char* dev_name)
{
    device_s* dev = _find(devid_t(dev_name));
    if (!dev) return false;
    return true;
}

rc_t device_m::quota(const char* dev_name, smksize_t& quota_KB)
{

    device_s* dev = _find(dev_name);
    if (!dev) return RC(eDEVNOTMOUNTED);
    quota_KB = dev->quota_pages*(page_sz/1024);
    return RCOK;
}

rc_t device_m::list_devices(const char**& dev_list, devid_t*& devid_list, u_int& dev_cnt)
{
    dev_cnt = _tab.num_members();
    if (dev_cnt == 0) {
	dev_list = NULL;
	return RCOK;
    }
    dev_list = new const char*[dev_cnt];
    if (!dev_list) {
	dev_cnt = 0;
	return RC(eOUTOFMEMORY);
    }
    devid_list = new devid_t[dev_cnt];
    if (!devid_list) {
	dev_cnt = 0;
	return RC(eOUTOFMEMORY);
    }
    w_list_i<device_s> scan(_tab);
    for (int i = 0; scan.next(); i++) {
        dev_list[i] = scan.curr()->name;
        devid_list[i] = scan.curr()->id;
    }
    return RCOK;
}

void device_m::dump() const
{
    cout << "DEVICE TABLE: " << endl;
    w_list_const_i<device_s> scan(_tab);
    while(scan.next()) {
	cout << scan.curr()->name << "  id:" << scan.curr()->id << "  quota = "  << scan.curr()->quota_pages << endl;
    }
}

device_s* device_m::_find(const char* dev_name)
{
    w_list_i<device_s> scan(_tab);
    while(scan.next() && strcmp(dev_name, scan.curr()->name));
    return scan.curr();
}

device_s* device_m::_find(const devid_t& devid)
{
    w_list_i<device_s> scan(_tab);
    while(scan.next() && devid != scan.curr()->id);
    return scan.curr();
}


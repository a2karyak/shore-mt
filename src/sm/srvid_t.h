/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

/*
 *  $Id: srvid_t.h,v 1.1
 */

#ifndef SRVID_H
#define SRVID_H

#ifdef __GNUG__
#pragma interface
#endif

struct srvid_t {
    enum { ME = 0 };

                srvid_t(uint2 s): id(s) {}
		srvid_t(const srvid_t& s): id(s.id) {}
                srvid_t(): id(smlevel_0::max_servers) {}

    bool      is_valid() const   { return (id < smlevel_0::max_servers); }
    bool      is_me() const      { return id == ME; }

    uint2       id;

    static srvid_t null;

    bool operator==(const srvid_t& s) const;
    bool operator!=(const srvid_t& s) const;
    friend inline ostream& operator<<(ostream&, const srvid_t& s);
};


inline bool
srvid_t::operator==(const srvid_t& s) const
{
    return id == s.id;
}

inline bool 
srvid_t::operator!=(const srvid_t& s) const
{
    return id != s.id;
}

inline ostream& operator<<(ostream& o, const srvid_t& s)
{
    return o << s.id;
}


inline u_long hash(const srvid_t& srv) {
    return srv.id;
}

#endif /* SRVID_H */

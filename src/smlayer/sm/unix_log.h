/*<std-header orig-src='shore' incl-file-exclusion='UNIX_LOG_H'>

 $Id: unix_log.h,v 1.17 1999/06/15 15:11:57 nhall Exp $

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

#ifndef UNIX_LOG_H
#define UNIX_LOG_H

#include "w_defines.h"

/*  -- do not edit anything above this line --   </std-header>*/

#ifdef __GNUG__
#pragma interface
#endif


class unix_partition : public partition_t {
public:
	// these are abstract in the parent class
	void			_clear();
	void			_init(srv_log *o);
	FHDL			fhdl_rd() const;
	FHDL			fhdl_app() const;
	FHDL			seekend_app();
	void			open_for_append(partition_number_t n);
	void			open_for_read(partition_number_t n, bool err=true);
	int			seeklsn_rd(uint4_t offset);
#ifdef OLD
	w_rc_t                  write(const logrec_t &r, const lsn_t &ll);
	void			flush(bool force = false);
#endif
	// w_rc_t                  read(logrec_t *&r, lsn_t &ll);
	void			close(bool both);
#ifdef OLD
	bool			exists() const;
	bool			is_open_for_read() const;
	bool			is_open_for_append() const;
	bool			flushed() const;
	bool			is_current() const; 
#endif
	void			peek(partition_number_t n, 
					const lsn_t& end_hint, 
					bool, int *fd = 0);
	void			destroy();
	void			sanity_check() const;
	void			set_fhdl_app(int fd);
	void			_flush(int fd);

private:
	FHDL			_fhdl_rd;
	FHDL			_fhdl_app;
};

class unix_log : public srv_log {
    friend class unix_partition;

    NORET			unix_log(const char* logdir,
				    int rdbufsize, 
				    int wrbufsize, 
				    char *shmbase,
				    bool reformat);

public:
    static rc_t			new_unix_log(unix_log *&the_log,
					     const char *logdir,
					     int	rdbufsize,
					     int	wrbufsize,
					     char	*shmbase,
					     bool	reformat);
    NORET			~unix_log();

    void			_write_master(const lsn_t& l, const lsn_t& min);
    w_rc_t                      _read_master( 
				    const char *fname,
				    int prefix_len,
				    lsn_t &tmp,
				    lsn_t& tmp1,
				    lsn_t* lsnlist,
				    int&   listlength,
				    bool&  old_style
				    );
    partition_t *		i_partition(partition_index_t i) const;

    static void			_make_log_name(
	partition_number_t	    idx,
	char*			    buf,
	int			    bufsz);

				// bool e==true-->msg if can't be unlinked
    static void			destroy_file(partition_number_t n, bool e);

private:
    unix_partition 	_part[max_open_log];

    ////////////////////////////////////////////////
    // just for unix files:
    ///////////////////////////////////////////////

    void			_make_master_name(
	const lsn_t&		    master_lsn, 
	const lsn_t&		    min_chkpt_rec_lsn,
	char*			    buf,
	int 			    bufsz,
	bool			    old_style = false);

    static const char 		master_prefix[];
    static const char 		log_prefix[];

};

/*<std-footer incl-file-exclusion='UNIX_LOG_H'>  -- do not edit anything below this line -- */

#endif          /*</std-footer>*/

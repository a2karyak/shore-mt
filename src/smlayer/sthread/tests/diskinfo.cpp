/*<std-header orig-src='shore'>

 $Id: diskinfo.cpp,v 1.16 2000/01/07 07:17:29 bolo Exp $

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

#include <w_stream.h>
#include <os_types.h>
#include <memory.h>
#include <getopt.h>

#include <w.h>
#include <w_statistics.h>
#include <sthread.h>
#include <sthread_stats.h>


static int parse_args(int, char **);


typedef sthread_base_t::disk_geometry_t disk_geometry_t;
typedef	sthread_base_t::filestat_t	filestat_t;

ostream &operator<<(ostream &o, const disk_geometry_t &dg)
{
	o 	<< "   " << setw(8) << dg.blocks
		<< "  " << setw(10) << dg.block_size
		<< "   " << setw(10) << dg.cylinders
		<< " " << setw(10) << dg.tracks
		<< " " << setw(10) << dg.sectors;
	return o;
}

ostream	&operator<<(ostream &o, const filestat_t &st)
{
	o 	<< "   " << setw(8) << (st.st_size / st.st_block_size)
		<< "  " << setw(10) << st.st_block_size
		<< "    " << "FILE: size= " << st.st_size;
	return o;
}

void print_dg(const char *fname, const disk_geometry_t &dg)
{
	W_FORM(cout)("%-20s", fname);
	cout << dg << endl;
}

void	print_st(const char *fname, const filestat_t &st)
{
	W_FORM(cout)("%-20s", fname);
	cout << st << endl;
}

void label_dg()
{
	W_FORM(cout)("%-20s   %8s  %10s   %10s %10s %10s\n",
		     "Device", "Blocks", "BlockSize",
		     "Cylinders", "Tracks", "Sectors");
}


bool	check_last = false;
char	*io_buf;	/* Sthread I/O buffer */


w_rc_t test_diskinfo(const char *fname, bool stamp, disk_geometry_t *all)
	
{
	int	fd;
	w_rc_t	rc;
	int	flags = sthread_t::OPEN_RDONLY | sthread_t::OPEN_LOCAL;
	disk_geometry_t		dg;
	filestat_t		st;

	rc = sthread_t::open(fname, flags, 0666, fd);
	if (rc) {
		cerr << "Can't open '" << fname << "':" << endl << rc << endl;
		return rc;
	}

	rc = sthread_t::fstat(fd, st);
	if (rc) {
		cerr << "Can't stat '" << fname << "':" << endl << rc << endl;
		sthread_t::close(fd);
		return rc;
	}

	if (st.is_file) {
		print_st(fname, st);
		W_COERCE( sthread_t::close(fd) );
		all->blocks += (st.st_size / st.st_block_size);
		return RCOK;
	}

	rc = sthread_t::fgetgeometry(fd, dg);
	if (rc) {
		cerr << "Can't get disk geometry:" << endl << rc << endl;
		sthread_t::close(fd);
		return rc;
	}

	if (check_last) {

		rc = sthread_t::lseek(fd, (dg.blocks-1) * dg.block_size,
					sthread_t::SEEK_AT_SET);
		if (rc)
			cerr << "Can't seek to last block:" 
				<< endl << rc << endl;
		else {
			rc = sthread_t::read(fd, io_buf, dg.block_size);
			if (rc)
				cerr << "Can't read last block:" 
					<< endl << rc << endl;
		}
	}


	if (stamp)
		*all = dg;
	else {
		all->cylinders += dg.cylinders;
		all->blocks += dg.blocks;
	}

	print_dg(fname, dg);

	W_COERCE( sthread_t::close(fd) );

	return RCOK;
}

int main(int argc, char **argv)
{
	int	i;
	int	arg;

	arg = parse_args(argc, argv);
	if (arg < 0)
		return 1;

	w_rc_t	e;
	e = sthread_t::set_bufsize(1024*1024, io_buf);
	if (e != RCOK)
		W_COERCE(e);

	disk_geometry_t all;
	memset(&all, 0, sizeof(all));	/* XXX soon be gone */

	label_dg();
	for (i = arg; i < argc; i++)
		W_IGNORE(test_diskinfo(argv[i], i == arg, &all));

	if (argc - arg > 2)
		print_dg("All", all);

	e = sthread_t::set_bufsize(0, io_buf);
	if (e != RCOK)
		cerr << "Warning: can't release buffer:" << endl << e << endl;

	return 0;
}


static	int	parse_args(int argc, char **argv)
{
	int	errors = 0;
	int	c;

	while ((c = getopt(argc, argv, "c")) != EOF) {
		switch (c) {
		case 'c':
			check_last = true;
			break;
		default:
			errors++;
			break;
		}
	}

	if (optind == argc)
		errors++;

	if (errors)
		cout << "Usage: " << argv[0]
			<< " [-c]"
			<< "path ..."
			<< endl;

	return errors ? -1 : optind;
}

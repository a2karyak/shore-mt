# <std-header style='tcl' orig-src='shore'>
#
#  $Id: append.st,v 1.15 1999/06/07 19:05:09 kupsch Exp $
#
# SHORE -- Scalable Heterogeneous Object REpository
#
# Copyright (c) 1994-99 Computer Sciences Department, University of
#                       Wisconsin -- Madison
# All Rights Reserved.
#
# Permission to use, copy, modify and distribute this software and its
# documentation is hereby granted, provided that both the copyright
# notice and this permission notice appear in all copies of the
# software, derivative works or modified versions, and any portions
# thereof, and that both notices appear in supporting documentation.
#
# THE AUTHORS AND THE COMPUTER SCIENCES DEPARTMENT OF THE UNIVERSITY
# OF WISCONSIN - MADISON ALLOW FREE USE OF THIS SOFTWARE IN ITS
# "AS IS" CONDITION, AND THEY DISCLAIM ANY LIABILITY OF ANY KIND
# FOR ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
#
# This software was developed with support by the Advanced Research
# Project Agency, ARPA order number 018 (formerly 8230), monitored by
# the U.S. Army Research Laboratory under contract DAAB07-91-C-Q518.
# Further funding for this work was provided by DARPA through
# Rome Research Laboratory Contract No. F30602-97-2-0247.
#
#   -- do not edit anything above this line --   </std-header>

#
#  This script tests record append.
#
#	record:  	small, medium, large
#	append size:	small, medium, large
#	transaction:	abort, commit
#
if { ! [is_set RESTART] } {
    set RESTART	0
}
set_config_info
set PG_SZ $lg_rec_page_space

proc append_rec { fid rec_size app_size } {
    set rec_size [format "%.0f" $rec_size]
    set app_size [format "%.0f" $app_size]
    puts stdout "$rec_size  $app_size" nonewline
    set r1 [ st create_rec $fid $rec_size ]
    set r2 [ st create_rec $fid $rec_size ]

    for { set i 1 } { $i <= 5 } { incr i } {
	st append_rec $r1 $app_size			
	st append_rec $r2 $app_size			
	puts stdout "."	nonewline
    }
    verbose ""
}

#####  main  #####

source $script_dir/vol.init

st begin_xct
set f [sm create_file $volid]
verbose "\nCreate File $f\n"
st commit_xct

foreach rec_sz { 0.1 0.9 1.0 2.0 10.0 50.0 100.0 } {
    foreach app_sz { 0.1 0.9 1.0 2.0 10.0 50.0 100.0 0.9 0.1 } {
	st begin_xct
	append_rec $f [expr { $PG_SZ * $rec_sz }] [expr { $PG_SZ * $app_sz }]
	st abort_xct

	if { $RESTART } { st restart }

	st begin_xct
	append_rec $f [expr { $PG_SZ * $rec_sz }] [expr { $PG_SZ * $app_sz }]
	st commit_xct

	if { $RESTART } {
	    st restart
	    st begin_xct
	    append_rec $f [expr { $PG_SZ * $rec_sz }] [expr {$PG_SZ * $app_sz}]
	    st restart
 	}
    }
    st begin_xct
    st validate_file $f
    st commit_xct
}

verbose "\n====================================="
verbose "Permovt Info"
verbose "-------------------------------------"
st show_permovt $f
verbose "=====================================\n"

verbose "\nFinal File Validation"
st begin_xct
st validate_file $f
st commit_xct


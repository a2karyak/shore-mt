# <std-header style='tcl' orig-src='shore'>
#
#  $Id: truncate.st,v 1.13 1999/06/07 19:05:49 kupsch Exp $
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
#  This script tests record truncate. 
#
#	record:  		small, medium, large
#	truncate size:		small, medium, large
#	transaction:		abort, commit
#
if { ! [is_set RESTART] } {
    set RESTART 1
}
set_config_info
set PG_SZ $lg_rec_page_space

proc truncate_rec { fid rec_size tr_size } {
    set rec_size [format "%.0f" $rec_size]
    set tr_size [format "%.0f" $tr_size]
    puts stdout "$rec_size  $tr_size" nonewline
    set r1 [ st create_rec $fid $rec_size ]
    set r2 [ st create_rec $fid $rec_size ]

    #sm print_rec $r1 0 0 
    for { set i 1 } { $i <= 10 } { incr i } {
	set rec_size [expr {$rec_size - $tr_size}]
	if { $rec_size < 0 } { break }
	st truncate_rec $r1 $tr_size			
	st truncate_rec $r2 $tr_size			
	puts stdout "."	nonewline
    }
    verbose ""
}

source $script_dir/vol.init

st begin_xct
set f [sm create_file $volid]
verbose "\nCreate File $f\n"
st commit_xct

#
#  for records of different sizes (with respect to PG_SZ),
#  truncate different amount (with respect to PG_SZ).
#
foreach rec_sz { 0.1 0.9 0.99 1.0 2.0 10.0 50.0 100.0 } {
    foreach tr_sz { 0.01 0.1 0.5 0.9 0.99 1.0 2.0 10.0 50.0 99.0 } {
	if { $rec_sz >= $tr_sz } {
	    st begin_xct
	    truncate_rec $f [expr {$PG_SZ * $rec_sz}] [expr {$PG_SZ * $tr_sz}]
	    st abort_xct
	    if { $RESTART } { st restart }

	    st begin_xct
	    truncate_rec $f [expr {$PG_SZ * $rec_sz}] [expr {$PG_SZ * $tr_sz}]
	    st commit_xct

	    if { $RESTART } {
	      st restart
	      st begin_xct
	      truncate_rec $f [expr {$PG_SZ * $rec_sz}] [expr {$PG_SZ * $tr_sz}]
	      st restart	
	    }
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


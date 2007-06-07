# <std-header style='tcl' orig-src='shore'>
#
#  $Id: update1.st,v 1.10 1999/06/07 19:05:50 kupsch Exp $
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
#  This script demonstrates record update.
#
source $script_dir/vol.init

st begin_xct

set f [sm create_file $volid]
verbose "\nCreate File $f\n"

for { set i 1 } { $i <= 10 } { incr i } {
    set r($i) [st create_rec $f $i]
}

#
#  Create 10 records.
#
verbose "\nCreate 10 Records\n"
sm scan_recs $f

verbose "\nTmp OVT Contains\n"
st show_tmpovt $f

verbose "\nValidate File $f\n"
st validate_file $f

#
#  Update records with length 4-10
#
verbose "\nUpdate Records with length 4-10 \n"
for { set i 4; set start 1; set len 2 } { $i <= 10 } { incr i; incr start } {
   	 st update_rec $r($i) $start $len
}

verbose "\nScan File $f\n"
sm scan_recs $f

verbose "\nTmp OVT Contains\n"
st show_tmpovt $f

verbose "\nValidate File $f\n"
st validate_file $f

st commit_xct


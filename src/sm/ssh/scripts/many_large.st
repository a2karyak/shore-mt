# <std-header style='tcl' orig-src='shore'>
#
#  $Id: many_large.st,v 1.10 1999/06/07 19:05:37 kupsch Exp $
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
#	Test large record creates 
#
source $script_dir/vol.init

set num_rec 200
set rec_size 3000

#echo "num_rec = " $num_rec
puts stdout "num_rec = $num_rec "

st begin_xct
set fid [st create_file $volid]

set rid(0) $null_rid
echo creating $num_rec of size $rec_size
for {set i 1} {$i <= $num_rec} {incr i} {
    set rid($i) [ st create_rec $fid 3000 ]
    puts stdout "created $rid($i)"
}

puts stdout "committing $num_rec creates of size $rec_size"
st commit_xct

st begin_xct
puts stdout "validating"
st validate_file $fid
st commit_xct


unset num_rec
unset rec_size
unset rid
unset fid


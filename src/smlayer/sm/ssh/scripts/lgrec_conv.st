# <std-header style='tcl' orig-src='shore'>
#
#  $Id: lgrec_conv.st,v 1.7 1999/06/07 19:05:32 kupsch Exp $
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

source $script_dir/vol.init
if {$volid == 0} {
    set volid 10
    st create_vol $volid force
    set volid [sm init_vol_logical_id $volid 100]
    sm mount
}

#
#  begin transaction
#
st begin_xct
set fid [sm create_file $volid]
verbose "\nFile $fid created\n"

set r1 [st create_rec $fid 96]
verbose "Record $r1 created\n"

set r2 [st create_rec $fid 96]
verbose "Record $r2 created\n"

for { set i 1 } { $i <= 200 } { incr i } {
        st append_rec $r1 96 
        st append_rec $r2 96 
        verbose "\n$i ---------------------------------------"
        st validate_file $fid
        verbose "---------------------------------------"
        #st print_scan_recs $fid
}

st show_permovt $fid

st commit_xct


# --------------------------------------------------------------- #
# -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- #
# -- University of Wisconsin-Madison, subject to the terms     -- #
# -- and conditions given in the file COPYRIGHT.  All Rights   -- #
# -- Reserved.                                                 -- #
# --------------------------------------------------------------- #

#
#  This script demonstrates record append and truncate.
#
source $script_dir/vol.init

proc check_file { fid } {
    echo "\nScan File $fid\n"
    st print_scan_recs $fid

    echo "\nTmp OVT Contains\n"
    st show_tmpovt $fid

    echo "\nValidate File $fid\n"
    st validate_file $fid
}

st begin_xct

set f [sm create_file $volid]
echo "\nCreate File $f\n"

#
#  Create 10 records.
#
echo "\nCreate 10 Records\n"
for { set i 1 } { $i <= 10 } { incr i } {
    set r($i) [st create_rec $f $i]
}
check_file $f

#
#  Append records so they're all 15-byte long.
#
echo "\nAppend Records so they're all 15-byte long\n"
for { set i 1; set len 14 } { $i <= 10 } { incr i; incr len -1 } {
         st append_rec $r($i) $len
}
check_file $f

#
#  Truncate records
#
echo "\nTruncate Records\n"
for { set i 1; set len 1 } { $i <= 10 } { incr i; incr len 1 } {
         st truncate_rec $r($i) $len
}
check_file $f

st commit_xct

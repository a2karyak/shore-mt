# --------------------------------------------------------------- #
# -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- #
# -- University of Wisconsin-Madison, subject to the terms     -- #
# -- and conditions given in the file COPYRIGHT.  All Rights   -- #
# -- Reserved.                                                 -- #
# --------------------------------------------------------------- #

#
#  This script demonstrates record update.
#
source $script_dir/vol.init

st begin_xct

set f [sm create_file $volid]
echo "\nCreate File $f\n"

for { set i 1 } { $i <= 10 } { incr i } {
    set r($i) [st create_rec $f $i]
}

#
#  Create 10 records.
#
echo "\nCreate 10 Records\n"
sm scan_recs $f

echo "\nTmp OVT Contains\n"
st show_tmpovt $f

echo "\nValidate File $f\n"
st validate_file $f

#
#  Update records with length 4-10
#
echo "\nUpdate Records with length 4-10 \n"
for { set i 4; set start 1; set len 2 } { $i <= 10 } { incr i; incr start } {
   	 st update_rec $r($i) $start $len
}

echo "\nScan File $f\n"
sm scan_recs $f

echo "\nTmp OVT Contains\n"
st show_tmpovt $f

echo "\nValidate File $f\n"
st validate_file $f

st commit_xct

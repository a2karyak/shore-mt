#!/bin/sh -- # do not change this line, it mentions perl and prevents looping

# <std-header style='perl' orig-src='shore'>
#
#  $Id: runtests.pl,v 1.19 1999/06/07 19:04:56 kupsch Exp $
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

# Script to run individual crash tests with ssh
# ssh must have been built with USE_SSMTEST

eval 'exec perl -s $0 ${1+"$@"}'
	if 0;

$v=1;
$w=1;

$flags = "-f";
if($n) {
  # ignore
}
if($C) {
   $flags = $flags ."C";
}
if($l) {
   $flags = $flags ."l";
}
if($y) {
   $flags = $flags ."y";
}

print STDOUT "runtests.pl with flags= $flags\n";

use FileHandle;


# Complete list of the tests:

# Set $kind to one of : crash abort yield delay

# NB: below this, the list gets reset if you
# want to run a subset of the tests.
# Also note: they are run in reverse order.

$kind = crash;

$tests_ = <<EOF;
    extent.3/1/alloc.2
    store.1/1/file.undo.1
    store.1/1/vol.2

    rtree.1/1/rtree.1
    rtree.2/1/rtree.1
    rtree.3/1/rtree.1
    rtree.4/1/rtree.1
    rtree.5/1/rtree.3

    enter.2pc.1/1/trans.1
    recover.2pc.1/1/trans.2
    recover.2pc.2/1/trans.2
    extern2pc.commit.1/1/trans.1

    prepare.readonly.1/1/trans.1
    prepare.readonly.2/1/trans.1
    prepare.abort.1/1/trans.1
    prepare.abort.2/1/trans.1

    prepare.unfinished.0/1/trans.1
    prepare.unfinished.1/1/trans.1
    prepare.unfinished.2/1/trans.1
    prepare.unfinished.3/1/trans.1
    prepare.unfinished.4/1/trans.1
    prepare.unfinished.5/1/trans.1

    commit.1/1/trans.1
    commit.2/1/trans.1
    commit.3/1/trans.1

    io_m::_mount.1/1/vol.init
    io_m::_dismount.1/1/vol.init

    commit.1/1/btree.6
    commit.2/1/btree.6
    commit.3/1/btree.6
    extent.3/1/btree.6
    io_m::_dismount.1/1/btree.6
    io_m::_mount.1/1/btree.6

    compensate/1/btree.1
    compensate/1/bt.remove.1

    commit.1/1/bt.remove.1
    commit.2/1/bt.remove.1
    commit.3/1/bt.remove.1
    extent.3/1/bt.remove.1
    btree.bulk.1/1/btree.3
    btree.bulk.2/1/btree.3
    btree.bulk.3/1/btree.3

    btree.remove.9/1/bt.remove.1
    btree.unlink.4/1/bt.remove.1
    btree.unlink.3/1/bt.remove.1
    btree.unlink.1/1/bt.remove.1
    btree.unlink.2/1/bt.remove.1

    btree.shrink.1/1/btree.5
    btree.shrink.2/1/btree.5
    btree.shrink.3/1/btree.5
    btree.create.1/1/btree.6
    btree.create.2/1/btree.6
    btree.distribute.1/1/btree.6
    btree.grow.1/1/btree.6
    btree.grow.2/1/btree.6
    btree.grow.3/1/btree.6
    btree.insert.1/1/btree.6
    btree.insert.2/1/btree.6
    btree.insert.3/1/btree.6
    btree.insert.4/1/btree.6
    btree.insert.5/1/btree.6

    btree.propagate.s.10/1/btree.6
    btree.propagate.s.1/1/btree.6
    btree.propagate.s.2/1/btree.6
    btree.propagate.s.3/1/btree.6
    btree.propagate.s.4/1/btree.6
    btree.propagate.s.5/1/btree.6
    btree.propagate.s.6/1/btree.6
    btree.propagate.s.7/1/btree.6
    btree.propagate.s.8/1/btree.6
    btree.propagate.s.9/1/btree.6

    btree.remove.6/1/btree.33

    btree.1page.1/1/btree.convert.1
    btree.1page.2/1/btree.convert.1

    btree.propagate.d.1/1/bt.remove.1
    btree.propagate.d.3/1/bt.remove.1
    btree.propagate.d.4/1/bt.remove.1
    btree.propagate.d.5/1/bt.remove.1
    btree.propagate.d.6/1/bt.remove.1
    btree.remove.1/1/bt.remove.1
    btree.remove.2/1/bt.remove.1
    btree.remove.3/1/bt.remove.1
    btree.remove.4/1/bt.remove.1
    btree.remove.5/1/bt.remove.1
    btree.remove.7/1/bt.remove.1
    btree.remove.10/1/bt.remove.1


EOF

# reset $tests_ to a subset:


@tests = split(/\s+/, $tests_);

NEXT: foreach $file (@ARGV) {
    # $file is test name
    
    $j=0;
    while($j++ < $#tests) {
	$t = @tests[$j];
	($tst,$val,$script) = split(/[\/]/, $t);

	# print STDOUT "$j($#tests): trying to match $tst, $s\n";

	$tst =~ m/^($file)$/o  && do {
	   $res = &onetest($tst,$val,$script,$flags);
	   next NEXT;
	};

    }
    print STDERR "Cannot find  $s in list.\n";
    exit 1;
}
if($i > 0) { exit 0; };

while($#tests>0) {
    $t = pop(@tests);

    ($tst,$val,$script) = split(/[\/]/, $t);

    $res = &onetest($tst,$val,$script,$flags);

}

sub onetest {
    my($test,$val,$script,$flags)=@_;

    print STDOUT "\n*** $test\n";
    $status = `/bin/rm -f log/* volumes/*`;
    if($status != 0) { return 1; }

    $system="./runtest $test $val $kind crashtest $script $flags";
    print STDOUT "    $system\n";
    system $system;
    $status = $?;
    # print STDOUT "    STATUS $status\n";

    if($status == 2) {
       print STDOUT "    exiting STATUS $status\n";
       exit $status;
    }

    $status = $status/256;
    # print STDOUT "    STATUS $status\n";
    if($status != 44) {
       print STDOUT "    FAILED TO CRASH $test $val\n";
       return 2;
    }

    print STDOUT "    RECOVERY ... ";

    $ENV{"DEBUG_FLAGS"}="restart.cpp xct_impl.cpp log_m::insert";
    $ENV{"SSMTEST"}="";
    $ENV{"SSMTEST_ITER"}="";
    $ENV{"SSMTEST_KIND"}="";

    $system="./runtest \"\" "
	.  $val
	.  " $kind"
	.  " \"restart_m::redo_pass xct_t::rollback log_m::insert\" "
	.  " empty"
	.  " $flags"
	;

    # print STDOUT "    $system\n";
    system $system;
    $status = $?;

    print STDOUT "    STATUS $status\n";
    if($status >= 32768) {
       exit $status;
    }

    # print STDOUT "    STATUS $status\n";
    $status = $status/256;
    if($? != 0) {
       print STDOUT " FAILED \n";
       exit;
       return 3;
    }
    print STDOUT "    WORKED \n";
    return 0;
}

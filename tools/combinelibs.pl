#!/s/std/bin/perl -w

# <std-header style='perl' orig-src='shore'>
#
#  $Id: combinelibs.pl,v 1.9 1999/06/07 19:09:12 kupsch Exp $
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
# combine many archives into one 
#
# usage: perl combinelibs.pl <$(AR)> <target> <otherlibs>
# <$(AR)> is the archive executable to use
# <target> is the name of the generated library
# <otherlibs> is a list of libraries to combine
#
# utterly unix-oriented, the following system utilities: 
# basename dirname rm mkdir pwd  ranlib 
# and some form of ar, passed in as an argument

# $v = 1;
$v = 0;

if ($v) {
    print "$#ARGV\n";
    print "AR=$ARGV[0] target=$ARGV[1] otherlibs=$ARGV[2],=$ARGV[3]\n";
}
if ( $#ARGV < 3 ) {
    print "usage: $pn ar-program target-archive archive [ archive ... ]\n";
    exit 1;
}

$pn=`basename $0`;
chop $pn;
$t="/tmp/$pn.".$$;

if ($v) {
    print "T = $t\n";
}

if (-e $t && -d $t) {
    `rm -rf $t`;
    if ($v) {
	print "removed $t\n";
    }
}
mkdir $t,0777;
if ($v) {
    print "made dir $t\n";
}


$AR=$ARGV[0];
$target=$ARGV[1];
$cur=`pwd`;
chop $cur;
if ($v) {
    print "cur = $cur\n";
}

$i = 2;
while ( $i <= $#ARGV ) {

#	get proper path of $i
	if ($v) {
	    print "get proper path of $i\n";
	}

	$one = $ARGV[$i];
	$dir = `dirname $one`; 
	chop $dir;
	if ($v) {
	    print "cd $dir|\n";
	}
	chdir $dir;

	$p=`pwd`; 
	chop $p;
	if ($v) {
	    print "pwd= $p|\n";
	}
	chdir $cur;

	$lib="$p/".`basename $one`;
	chop $lib;
	if ($v) {
	    print "Trying lib $lib (from $one)\n";
	}

	#
	# size > 0? (if -s ...)
	#
	if ( -s $lib ) {
		print "extracting $lib ...\n";

		chdir $t;
		`$AR x $lib`; 
		if ( -s "__.SYMDEF" ) {
		    unlink __.SYMDEF;
		}
		chdir $cur;

		foreach $j (`$AR t $lib`) {
			chop $j;
			if ($v) {
			    print "j = $j|\n";
			}
			$s=`basename $j.o`;
			chop $s;
			if ($v) {
			    print "s = $s\n";
			}
			if ( $s.o != $j && $j != "__.SYMDEF" ) {
			print "Warning: file name truncated in $one. \n";
			print "$j is being changed to $j.o \n";
			rename $t/$j,$t/$j.o;
			}
		}
		print " done with $lib \n";
	}
	$i++;
}


print "The following .o files will be in  $target: ";
foreach $j (`ls $t`) {
    chop $j;
    print "$j\n";
}

if ( -f $target ) {
    print "removing $target\n";
    unlink $target;
}

print "creating $target\n";
`$AR q $target $t/*.o`;

print "ranlib $target\n";
`ranlib $target`;

`rm -rf $t`;
print "$pn is done.\n";
exit 0


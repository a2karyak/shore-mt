#!/s/std/bin/perl -w

# <std-header style='perl' orig-src='shore'>
#
#  $Id: ntclean.pl,v 1.8 1999/06/07 19:09:14 kupsch Exp $
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


# Usage: perl <this file>  <file-list>
# Usage: <this file> <file-list>
# 	removes ^M from the ends of lines
#	of files that contain them; prints the
#	names of files that it munged.
#
# Usage: perl -s <this file> -n <file-list>
# Usage: <this file> -n <file-list>
# 	counts ^M on the ends of lines
#
$the_following_copyright_applies_to_this_script = <<ENDOFNOTICE;
/*
Copyright (c) 1994-1997 
Computer Sciences Department
University of Wisconsin -- Madison
All Rights Reserved.

Permission to use, copy, modify and distribute this software and its
documentation is hereby granted, provided that both the copyright
notice and this permission notice appear in all copies of the
software, derivative works or modified versions, and any portions
thereof, and that both notices appear in supporting documentation.

THE COMPUTER SCIENCES DEPARTMENT OF THE UNIVERSITY OF WISCONSIN --
MADISON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS" CONDITION.
THE DEPARTMENT DISCLAIMS ANY LIABILITY OF ANY KIND FOR ANY DAMAGES
WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.

This software was developed with support by the Advanced Research 
Project Agency, ARPA order number 018 (formerly 8230), monitored by 
the U.S. Army Research Laboratory under contract DAAB07-91-C-Q518.
*/
ENDOFNOTICE

foreach $file (@ARGV) {
    $count = 0;
    open(FROM, "<$file") || die "Cannot open $file.\n";
    while(<FROM>) {
	/
$/  && $count++;
    }
    close FROM;
    if($count > 0) {
	@doit{$file} = $count;
    }
}
if ($n) {
    while (($file,$count) = each %doit) {
	print STDOUT "$file: $count\n";
    }
} else {
    while(($file,$count) = each %doit) {
	if($v) { print STDOUT "$file ...\n";}
	open(FROM, "<$file") || die "Cannot open $file.\n";
	open(TO, ">$file.munged") || die "Cannot open $file.munged.\n";
	while(<FROM>) {
	    s/
$// ;
	    print TO $_;
	}
	# for testing purposes
	# print TO "garbage\n";
	close FROM;
	close TO;

	$res = system("diff -w $file $file.munged");
	if($res == 0) {
	    # print STDERR "renaming $file.munged -> $file \n";
	    $res = rename "$file.munged",$file;
	    if($res != 1) {
		print STDERR "rename $file.munged -> $file failed : $res\n" ;
		print STDERR "You must clean up by hand.\n";
	    }
	} else {
	    print STDERR "ERROR: $file and $file.munged differ in more than whitespace\n";
	    exit -1;
	}
    }
}

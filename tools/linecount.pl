#!/s/std/bin/perl -w

# <std-header style='perl' orig-src='shore'>
#
#  $Id: linecount.pl,v 1.8 1999/06/07 19:09:14 kupsch Exp $
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


# to call: perl <this script> <directory>
#
# no more: for NT
# #!/bin/sh -- # A comment mentioning perl to prevent looping.
# eval 'exec perl -s $0 ${1+"$@"}'
# if 0;

# A poor attempt to do this in perl

foreach $file (@ARGV) {
    $cmd = "find $file -type f -name \"*.[chC]\" -exec cat {} \\; | wc -l";
    # print "$cmd\n";

    $f = `$cmd`;
    print STDERR "Raw lines of text: $f\n";


    $cmd ="find $file -type f -name ";
    $cmd .= '"*.[chC]"';
    $cmd .= " -exec egrep ";
    $cmd .= '";|{"';
    $cmd .= " {} \\; | wc -l";
    # print "$cmd\n";
    $f = `$cmd`;
    print STDERR "Real lines of code (those with ; and {) ... : $f\n";
    exit 0
}

print "\nUsage linecount dirs ...\n";
exit 1

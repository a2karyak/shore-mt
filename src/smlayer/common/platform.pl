#!/s/std/bin/perl -w

# <std-header style=PERL orig-src=SHORE>
#
#  $Id: platform.pl,v 1.3 1999/05/17 18:54:48 kupsch Exp $
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


use strict;
use Getopt::Long;

sub Usage
{
    my $progname = $0;
    $progname =~ s/.*[\\\/]//;
    print STDERR <<EOF;
Usage: $progname [--help] filename
Generates a file containing the current time and date.

    --arch         processor architecture to be defined
    --opsys        operating system to be defined
    --outfile      name of output file
    --help|h       prints this message and exits
EOF
}

my %options = (arch => '', opsys => '', outfile => '', help => 0);
my @options = ("arch=s", "opsys=s", "outfile=s", "help|h!");
my $ok = GetOptions(\%options, @options);
$ok = 0 if !$options{arch} || !$options{opsys} || !$options{outfile} || $#ARGV != -1;
if (!$ok || $options{help})  {
    Usage();
    exit(!$ok);
}

my $outfile = $options{outfile};

open OUTFILE, ">$outfile" or die "open $outfile";
print OUTFILE <<EOF;
/*<std-header orig-src=SHORE genfile>

SHORE -- Scalable Heterogeneous Object REpository

Copyright (c) 1994-99 Computer Sciences Department, University of
                      Wisconsin -- Madison
All Rights Reserved.

Permission to use, copy, modify and distribute this software and its
documentation is hereby granted, provided that both the copyright
notice and this permission notice appear in all copies of the
software, derivative works or modified versions, and any portions
thereof, and that both notices appear in supporting documentation.

THE AUTHORS AND THE COMPUTER SCIENCES DEPARTMENT OF THE UNIVERSITY
OF WISCONSIN - MADISON ALLOW FREE USE OF THIS SOFTWARE IN ITS
"AS IS" CONDITION, AND THEY DISCLAIM ANY LIABILITY OF ANY KIND
FOR ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.

This software was developed with support by the Advanced Research
Project Agency, ARPA order number 018 (formerly 8230), monitored by
the U.S. Army Research Laboratory under contract DAAB07-91-C-Q518.
Further funding for this work was provided by DARPA through
Rome Research Laboratory Contract No. F30602-97-2-0247.

  -- do not edit anything above this line --   </std-header>*/

#define $options{arch} 1
#define $options{opsys} 1
EOF

close OUTFILE or die "close $outfile";

#!/s/std/bin/perl -w

# <std-header style='perl' orig-src='shore'>
#
#  $Id: GenDefines.pl,v 1.1 1999/06/07 19:09:11 kupsch Exp $
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

my %undefs;
my %defines;
for (my $i = 0; $i <= $#ARGV; $i++)  {
    if ($ARGV[$i] =~ /^-D(.*)/)  {
	my $value = $1;
	if ($value =~ /(.*?)=(.*)/)  {
	    $defines{$1} = " $2";
	}  else  {
	    $defines{$1} = '';
	}
    }  elsif ($ARGV[$i] =~ /^-U(.*)/)  {
        $undefs{$1} = '';
    }  else  {
        next;
    }
    splice @ARGV, $i, 1;
    redo;
}

sub Usage
{
    my $progname = $0;
    $progname =~ s/.*[\\\/]//;
    print STDERR <<EOF;
Usage: $progname [--help] [--outfile=filename] (-Dname=value | -Uname)...
Generates a file containing c preprocessor statement defining and undefining
variables.

    -Dname[=value] #define name [to be value]
    -Uname         #undef name
    --outfile      name of output file
    --help|h       prints this message and exits
EOF
}

my %options = (outfile => '-', help => 0);
my @options = ("outfile=s", "help|h!");
my $ok = GetOptions(\%options, @options);
$ok = 0 if $#ARGV != -1;
if (!$ok || $options{help})  {
    Usage();
    exit(!$ok);
}

my $outfile = $options{outfile};
my $inclFileExclusion = uc($outfile);
$inclFileExclusion =~ s/.*\///;
$inclFileExclusion =~ tr/A-Z0-9/_/c;

open OUTFILE, ">$outfile" or die "open $outfile";
my $curTime = localtime;
print OUTFILE <<EOF;
/* DO NOT EDIT --- generated by $0 on $curTime

<std-header orig-src='shore' genfile='true'>

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

*/

#include "w_defines.h"

/*  -- do not edit anything above this line --   </std-header>*/

#ifndef $inclFileExclusion
#define $inclFileExclusion


EOF


foreach my $key (sort keys %defines)  {
    print OUTFILE "#define $key$defines{$key}\n";
}
foreach my $key (sort keys %undefs)  {
    print OUTFILE "#undef  $key\n";
}

print OUTFILE <<EOF;


#endif
EOF

close OUTFILE or die "close $outfile";

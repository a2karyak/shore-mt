#!/s/perl/bin/perl -w

# <std-header style='perl' orig-src='shore'>
#
#  $Id: edit_config.pl,v 1.11 1999/06/07 19:09:12 kupsch Exp $
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
# converts a configuration file consisting of C preprocessor macro
# define's and undef's to have new values.
#
# arguments:
#   --input=filename    input file name
#   --output=filename   output file name
#   --createDirs        create output file directory if not existing
#   [--help]            print help message
#   NAME=VALUE          define macro name to have value VALUE
#   NAME                define macro name to have the null value
#   undef-NAME          undefine macro name
#


use strict;
use Getopt::Long;

sub Usage
{
    my $progname = $0;
    $progname = s/.*[\\\/]//;
    print STDERR <<EOF;
Usage: $progname [--input=infile] [--output=outfile] [--createDirs] [--help] changes...
Copy the C preprocessor file to the outfile, modifying #define and #undef lines
according to changes.  changes can be one of the following 3 formats:

  NAME=value    define NAME to be value
  NAME          define NAME to have an empty value
  undef-NAME    undef the NAME

Only lines in the infile which start with #define or #undef are changed and only if
a change line is present.  All other lines are output verbatim.

    --input           name of input file
    --output          name of output file
    --createDirs      create non-existent output directories
    --help|h          print this message and exit
EOF
}

my %options = (input => '-', output => '-', createDirs => 0, help => 0);
my @options = ("input=s", "output=s", "createDirs!", "help|h!");
my $ok = GetOptions(\%options, @options);
if (!$ok || $options{help})  {
    Usage();
    exit(!$ok);
}


sub CreateDirs
{
    my $dir = shift;

    return if -e $dir;

    my $parentDir;
    ($parentDir) = ($dir =~ /(.*)\//);
    $parentDir = '.' if !$parentDir;

    CreateDirs($parentDir) unless -e $parentDir;

    my $mode = (stat $parentDir)[2] | 0700;
    mkdir $dir, $mode or die "mkdir $dir";
}


my (%map);
my (%isdef);

#
# read arguments (-input and -output are handled by -s flag)
#
foreach my $arg (@ARGV)  {
    if ($arg =~ /^(\w*)=(.*)$/)  {
	$map{$1} = $2;
	$isdef{$1} = 1;
    }  elsif ($arg =~ /^undef-(\w*)$/)  {
	$map{$1} = '';
	$isdef{$1} = 0;
    }  elsif ($arg =~ /^(\w*)$/)  {
	$map{$1} = '';
	$isdef{$1} = 1;
    }  else  {
	die "$0: invalid arguement format \"$arg\"\n";
    }
}


my $output = $options{output};
my $input  = $options{input};

CreateDirs($output =~ /(.*)\//) if $options{createDirs} && $output ne '-';

open(INFILE, "<$input") or die "$0: couldn't open file '$output'";
my (@lines) = <INFILE>;
close INFILE or die "$0: couldn't close file '$input'";

open(OUTFILE, ">$output") or die "$0: couldn't open file '$output'";

my (@macroname) = ("undef", "define");
foreach my $line (@lines)  {
    if ($line =~ /^(\s*#\s*)(undef|define)(\s*)(\w*)(\s*)(.*)\n/ && defined $isdef{$4})  {
	my ($sep);
	if ($isdef{$4})  {
	    $sep = $5;
	    $sep = ' ' if $sep eq '';
	}  else  {
	    $sep = '';
	}
	print OUTFILE "$1$macroname[$isdef{$4}]$3$4$sep$map{$4}\n";
    }  else  {
	print OUTFILE $line;
    }
}

close OUTFILE or die "$0: couldn't close file '$output'";

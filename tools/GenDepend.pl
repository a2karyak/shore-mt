#!/s/std/bin/perl -w

# <std-header style='perl' orig-src='shore'>
#
#  $Id: GenDepend.pl,v 1.10 2003/06/24 15:38:46 bolo Exp $
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
# This is an alternative first 3 lines that will make the
# script into a program (has problems on linux).
#
# #!/bin/sh -- # A comment mentioning perl to prevent looping.
# eval 'exec perl -S $0 ${1+"$@"}'
#    if 0;
#

use strict;
use Getopt::Long;

sub Usage
{
    my $progname = $0;
    $progname =~ s/.*[\\\/]//;
    print STDERR <<EOF;
Usage: $progname [--outFile=file] [--autoUpdating] [--verbose] [--help] --cppCmd=cmd [--objExtension=objExt] [--dosPaths] files...
Create file which contains a list of dependencies used by make.

    --outFile            name of output file defaults to stdout
    --autoUpdating       if set, makes output file depend on dependencies
    --verbose|v          verbose debugging output
    --help|h             prints this message and exits
    --cppCmd             C preprocessor command and options
    --objExtension       object file extension (ex. .obj or .o)
    --dosPaths		 grok DOS 'drive-letter:' pathnames
    file...              files to include as targets in the output file
EOF
}


# Configuration parameters.

my %options = (outFile => '-', autoUpdating => 0, verbose => 0, help => 0, objExtension => '', dosPaths => 0);
my @options = ("outFile=s", "autoUpdating!", "verbose|v!", "help|h!", "cppCmd=s", "objExtension=s", "dosPaths!");
my $ok = GetOptions(\%options, @options);
if (!$ok || $options{help} || !defined($options{cppCmd}))  {
    Usage();
    exit($ok == 0);
}

my $isDOS = $options{dosPaths};
## If one isn't provided, try guessing based on unix or DOS
if (!$options{objExtension}) {
	$options{objExtension} = $isDOS ? '.obj' : '.o';
}

## new cygwin mounts drives as /cygdrive/D, previous mounted as //D
## XXX convert to option
my $isNewCygwin = 1;

my $v = $options{verbose};

if ($v)  {
    my $key;
    foreach $key (keys %options)  {
        print "\$options{$key} = $options{$key}\n";
    }
}


my $outfile = $options{outFile};
open OUTFILE, ">$outfile" or die "open >$outfile";

my $file;
foreach $file (@ARGV) {
    next if !($file=~/\.(C|c|cc|cpp|cxx)$/);
    my $objFile = $file;
    $objFile =~ s/^.*\///;
    $objFile =~ s/\.[^.]*$/$options{objExtension}/o;
    my $command = "$options{cppCmd} $file";
    if ($isDOS)  {
	$command =~ s/'/"/g; # NT can't handle '
	$command = "sh -c '$command'" if $command =~ /^"/;
    }
    open(CMDOUT, "$command|") or die "Failure: $command\n";

    # Scan output for line directives (#line num "file"  or  # num file)

    my %dependency = ();
    while (<CMDOUT>) {
	if (/^\s*#\s*line\s*\d+\s+"(.*)"\s*$/ || /^\s*#\s*\d+\s+"(.*)"/)  {		# line num file
	    my $depFile = $1;
	    if ($depFile eq "<built-in>") {
	    	next;
	    }
	    $depFile =~ tr/\\/\//;
	    $depFile =~ s/\/+/\//g;
	    if ($isDOS) {
	    	## map drive letters into cygwin paths
	    	if ($isNewCygwin) {
		    $depFile =~ s|([a-zA-Z]):|/cygdrive/$1|;
		}
		else {
		    $depFile =~ s|([a-zA-Z]):|//$1|;
		}
	    }
	    if ($depFile !~ /[: ]/)  {
	        $dependency{$depFile}++;
	    }
	}
    }
    close CMDOUT or warn "close $command";

    my $target = $objFile;
    $target .= " $outfile" if $options{autoUpdating};
    my $key;
    print OUTFILE "$target:";
    foreach $key (sort keys(%dependency)) {
	print OUTFILE " \\\n $key";
    }
    print OUTFILE "\n";
}

close OUTFILE or die "close $outfile";

#!/s/std/bin/perl -w

# <std-header style='perl' orig-src='shore' no-defines='true'>
#
#  $Id: bootstrap.pl,v 1.13 1999/06/07 20:32:55 kupsch Exp $
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
    print <<EOF;
Usage: $progname [--imake_dir=imakedir] [--build_dir=builddir] [--createDirs=createdirs] [--help] [--make=make]
Make imake.

    --imake_dir    directory which contains imake files
    --build_dir    directory into which to build imake
    --createDirs   create build_dir if true
    --help|h       print this message and exit
    --make	   name of make executable
EOF
}


my %options = (imake_dir => '.', build_dir => '.', createDirs => 0, help => 0, make => 'make');
my @options = ("imake_dir=s", "build_dir=s", "createDirs!", "help|h!", "make=s");
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


use Cwd;
my $cwdDir = cwd();
$cwdDir =~ s/\\/\//g;

my $build_dir = $options{build_dir};
$build_dir = "$cwdDir/$build_dir" if $build_dir !~ /^\//;
$build_dir .= "/$options{imake_dir}";

CreateDirs($build_dir) if $options{createDirs} && $build_dir ne '-';

my $isNT = ($^O =~ /Win32/);

my $target_build_dir = $build_dir;
$target_build_dir =~ s/^(.):\/?/\/\/$1\// if $isNT;

my $makeflags = "-C $options{imake_dir} BUILD_DIR='$build_dir' TARGET_BUILD_DIR='$target_build_dir'";
$makeflags .= " WINDOWS=1" if $isNT;
$makeflags =~ s/'//g if $isNT;
my $makefile = "makefile.boot";
my $makeCmd = "$options{make} -f $makefile $makeflags";

my $cmd = "$makeCmd clean";
print "$cmd\n";
print `$cmd`;
die "$0: '$cmd' failed" if $?;

$cmd = "$makeCmd $target_build_dir/imake";
$cmd .= '.exe' if $isNT;
print "If following fails, execute the following from the shell:\n";
print "$cmd\n";
print `$cmd`;
die "$0: '$cmd' failed" if $?;

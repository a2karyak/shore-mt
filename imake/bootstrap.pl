#!/s/std/bin/perl -w

# <std-header style='perl' orig-src='shore' no-defines='true'>
#
#  $Id: bootstrap.pl,v 1.17 2002/05/30 14:56:51 bolo Exp $
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
Usage: $progname [--imake_dir=imakedir] [--build_dir=builddir] [--createDirs=createdirs] [--help] [--make=make] [--imake_exec_name=imake_prog] [--compile_c_to_exec='c compile string'] [--use_this_for_cpp="cpp string to use"] [--dosPaths]
Make imake.

    --imake_dir          directory which contains imake files
    --build_dir          directory into which to build imake
    --createDirs         create build_dir if true
    --help|h             print this message and exit
    --make	         name of make executable
    --imake_exec_name    name of imake program
    --compile_c_to_exec  string to compile c to exec (special tokens: EXEC_FILE SOURCE_FILE IMAKE_DEFINES)
                           eg. if using gcc  "gcc -O IMAKE_DEFINES -o EXEC_FILE SOURCE_FILE"
    --use_this_for_cpp   program and options to use for a cpp program
                           eg. if using gcc  "gcc -E -traditional"
    --dosPaths		 Grok DOS drive-letter: pathnames
EOF
}


my %options = (imake_dir => '.', build_dir => '.', createDirs => 0, help => 0, make => 'make', imake_exec_name => '', compile_c_to_exec => '', use_this_for_cpp => '', dosPaths => 0);
my @options = ("imake_dir=s", "build_dir=s", "createDirs!", "help|h!", "make=s", "imake_exec_name=s", "compile_c_to_exec=s", "use_this_for_cpp=s", "dosPaths!");
my $ok = GetOptions(\%options, @options);
if (!$ok || $options{help})  {
    Usage();
    exit(!$ok);
}

my $isDOS = $options{dosPaths};

## "new" /cygdrive/d versus "old" //d handling of cygnus drive paths 
my $isNewCygwin = 1;

sub DosName
{
    my $filename = shift;

#    printf("DosName(%s)", $filename);

    ## convert either style of cygwin path
    ## old style cygnus //d path
    $filename =~ s|^//([a-zA-Z])|$1:|;
    ## new style cygnus //d path
    $filename =~ s|^/cygdrive/([a-zA-Z])|$1:|;

#    printf(" -> %s\n", $filename);

    return $filename;
}

sub CygwinName
{
    my $filename = shift;

#    printf("CygwinName(%s)", $filename);

    if ($isNewCygwin) {
	$filename =~ s|^([a-zA-Z]):/?|/cygdrive/$1/|;
    }
    else {
	$filename =~ s|^([a-zA-Z]):/?|//$1/|;
    }

#    printf(" -> %s\n", $filename);

    return $filename;
}

sub AbsolutePath
{
    my $filename = shift;
    my $atroot = 0;

#    printf("AbsolutePath(%s)", $filename);

    $atroot = $filename =~ /^\//;
    if (!$atroot && $isDOS) {
    	$atroot = 1 if $filename =~ /^[a-zA-Z]:/;
    }

#    printf(" -> %d\n", $atroot);

    return $atroot;
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
$cwdDir =~ s|\\|/|g if $isDOS;


my $build_dir = $options{build_dir};
$build_dir = "$cwdDir/$build_dir" if !AbsolutePath($build_dir);
$build_dir .= "/$options{imake_dir}";
$build_dir = DosName($build_dir) if $isDOS;

CreateDirs($build_dir) if $options{createDirs} && $build_dir ne '-';

if (!$options{imake_exec_name})  {
    $options{imake_exec_name} = 'imake';
    $options{imake_exec_name} .= '.exe' if $isDOS;
}

my $target_build_dir = $build_dir;
$target_build_dir = CygwinName($target_build_dir) if $isDOS;

my $makeflags = "-C $options{imake_dir} BUILD_DIR='$build_dir' TARGET_BUILD_DIR='$target_build_dir'";
$makeflags .= " WINDOWS=1" if $isDOS;
$makeflags =~ s/'//g if $isDOS;
$makeflags .= " IMAKE_EXEC_NAME=$options{imake_exec_name}";
$makeflags .= " COMPILE_C_TO_EXEC='$options{compile_c_to_exec}'" if $options{compile_c_to_exec};
if ($options{use_this_for_cpp})  {
    $makeflags .= " USE_THIS_FOR_CPP='$options{use_this_for_cpp}'";
    $makeflags .= " STRINGIZE_USE_THIS_FOR_CPP=1" if ($options{use_this_for_cpp} =~ /^cl/);
}
my $makefile = "makefile.boot";
my $makeCmd = "$options{make} -f $makefile $makeflags";

my $cmd = "$makeCmd $target_build_dir/$options{imake_exec_name}";
print "$cmd\n";
print `$cmd`;
die "$0: '$cmd' failed" if $?;

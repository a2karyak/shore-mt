#!/s/std/bin/perl -w

# <std-header style='perl' orig-src='shore'>
#
#  $Id: makemake.pl,v 1.29 2000/01/14 05:27:55 bolo Exp $
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


my $imakeDir = 'imake';
my $imakeExec = "$imakeDir/imake";
my $imakefileName = 'Imakefile';
my $tmpMakefileName = 'Makefile.new';
my $makefileName = 'Makefile';
my $defFile = 'config/shore.def';
my $isDOS = 0;


sub CanonicalFilename
{
    my $filename = shift;
    my @filename = split(/\//, $filename);
    foreach my $i (reverse 0 .. $#filename)  {
	splice(@filename, $i, 1) if $filename[$i] eq '.' || $i != 0 && $filename[$i] eq '';
    }
    my $i = 1;
    while ($i <= $#filename)  {
	if ($filename[$i] eq '..' && $filename[$i - 1] ne '..')  { 
	    splice(@filename, $i - 1, 2);
	    --$i if $i > 1;
	}  else  {
	    ++$i;
	}
    }
    $filename[0] = '.' if $#filename == -1;
    join('/', @filename);
}

sub CygwinName
{
    my $filename = shift;
    $filename =~ s|^([a-zA-Z]):|\$(SLASH)/$1| if $isDOS;
    return $filename;
}


# generate the Makefile in dir from the Imakefile.
# first generate it into a tmp Makefile then copy
# the tmp Makefile to the final one.

sub DoImake
{
    my ($path, $includeDirs, $topDirPath, $buildTopDirPath, $topDir, $buildTopDir) = @_;
    my $fullDir = CanonicalFilename("$topDir/$path");
    my $fullBuildDir = CanonicalFilename("$buildTopDir/$path");
    my $tmpMakefile = CanonicalFilename("$buildTopDir/$path/$tmpMakefileName");
    my $tmpMakefileC = CanonicalFilename("$buildTopDir/$path/Makefile.c");
    my $finalMakefile = CanonicalFilename("$buildTopDir/$path/$makefileName");
    my $top = CanonicalFilename($topDirPath);
    my $buildTop = CanonicalFilename($buildTopDirPath);
    my $thisPath = CanonicalFilename($path);
    my $extTop = $top;
    my $extBuildTop = $buildTop;
    $top = CygwinName($top) if $isDOS;
    $buildTop = CygwinName($buildTop) if $isDOS;

    # do unlink since previous makemake made these read-only.
    unlink $tmpMakefile;

    my $defines = "-DTop='$top' -DBuildTop='$buildTop' -DThisPath='$thisPath'";
    $defines .= " -DExtTop='$extTop'" if $top ne $extTop;
    $defines .= " -DExtBuildTop='$extBuildTop'" if $buildTop ne $extBuildTop;
    $defines =~ s/'//g if $isDOS;	# NT SHELL doesn't do quotes!!
    my $cmd = "$buildTopDir/$imakeExec $includeDirs $defines -C$tmpMakefileC -f$fullDir/$imakefileName -s$tmpMakefile";
    print "$cmd\n";

    `$cmd`;
    if ($?)  {
	# perl bug, die is supposed to exit($? >> 8), but doesn't
	# setting $? to 0 makes it exit with 255
	$? = 0;
	die "$0: $cmd failed";
    }

    # copy tmp Makefile to Makefile
    local $/;
    open INFILE, "<$tmpMakefile" or die "open $tmpMakefile";
    my $contents = <INFILE>;
    close INFILE or die "close $tmpMakefile";

    unlink $finalMakefile;
    open OUTFILE, ">$finalMakefile" or die "open $finalMakefile";
    print OUTFILE $contents;
    close OUTFILE or die "close $finalMakefile";

    unlink $tmpMakefile;
}

sub CreateDirs
{
    my ($dir, $useBuildDir) = @_;
    return if !$useBuildDir || -d $dir;
    my $parentDir = $dir;
    $parentDir =~ s/\/[^\/]*$//;
    CreateDirs($parentDir, $useBuildDir);
    my $mode = (stat "$parentDir")[2] | 0700;
    mkdir $dir, $mode or die "mkdir $dir";
}


# generate all the Makefile's in the current directory
# and all of its subdirectories, doing subdirectories
# before the current directory.

sub DoDir
{
    my ($path, $includeDirs, $relativeTopDir, $relativeBuildTopDir, $useRelativeDirs, $useBuildDir, $topDir, $buildTopDir) = @_;

    my $fullDir = CanonicalFilename("$topDir/$path");
    return if $fullDir eq $buildTopDir && $useBuildDir;

    opendir(DIR, $fullDir) or die "opendir $fullDir";
    my @files = readdir(DIR);
    closedir(DIR) or die "closedir $fullDir";

    my $newIncludeDirs = " -I$fullDir$includeDirs";
    my $filename;
    foreach $filename (grep {-d "$fullDir/$_"} @files)  {
	next if $filename eq '.' || $filename eq '..';
	DoDir("$path/$filename", $newIncludeDirs, "$relativeTopDir/..",
		"$relativeBuildTopDir/..", $useRelativeDirs, $useBuildDir, $topDir, $buildTopDir);
    }
    if (-e "$fullDir/$imakefileName")  {
	CreateDirs("$buildTopDir/$path", $useBuildDir);
	if ($useRelativeDirs)  {
	    DoImake($path, $newIncludeDirs, $relativeTopDir, $relativeBuildTopDir, $topDir, $buildTopDir)
	}  else  {
	    DoImake($path, $newIncludeDirs, $topDir, $buildTopDir, $topDir, $buildTopDir)
	}
    }
}

sub Usage
{
    my $progname = $0;
    $progname =~ s/.*[\\\/]//;
    print STDERR <<EOF;
Usage: $progname [--topDir=repository_dir] [--buildTopDir=build_dir] [--curPath=path] [--help] [--imakeExecName=imake_program_name] [--dosPaths] [repository_dir]
Call imake on each Imakefile found in repository_dir and generate Makefile's in build_dir.

    --topDir         set directory to start search for Imakefiles
    --buildTopDir    set parallel directory to generate Makefiles
    --curPath        set current path from top dir to this directory
    --imakeExecName  name of imake program
    --dosPaths	     mangle and recognize 'drive-letter:' pathnames
    --help|h         print this message and exit
EOF
}

sub IsRelativePath
{
    my $filename = shift;
    return !($filename =~ m|^/| || $isDOS && $filename =~ m|^[a-zA-Z]:|);
}

use Getopt::Long;
use Cwd;
my $cwdDir = cwd();
$cwdDir =~ s/\\/\//g;

my %options = (topDir => '', buildTopDir => '', curPath => '', help => 0, imakeExecName => '', dosPaths => 0);
my @options = ("topDir=s", "buildTopDir=s", "curPath=s", "help|h!", "imakeExecName=s", "dosPaths!");
my $ok = GetOptions(\%options, @options);

$ok = 0 if $#ARGV > 0;
if ($#ARGV == 0)  {
    $ok = 0 if $options{topDir};
    $options{topDir} = $ARGV[0];
}
$options{topDir} = '.' if !$options{topDir};
$options{buildTopDir} = $options{topDir} if !$options{buildTopDir};
$options{curPath} = $cwdDir if !$options{curPath};

$imakeExec = "$imakeDir/$options{imakeExecName}" if ($options{imakeExecName});

my $topDir = $options{topDir};
my $buildTopDir = $options{buildTopDir};
$buildTopDir =~ s|^//([a-zA-Z])|$1:|;
my $curPath = $options{curPath};
$isDOS = $options{dosPaths};

if (!$ok || $options{help})  {
    Usage();
    die(!$ok);
}

## This may not be needed, it should find the non .exe filename
## in the search, as the name is looked for with a generated
## extension.   This would also allow substitution of imake with
## a front-end script, or other normal unix-like things.
if ($isDOS) {
    $imakeExec .= ".exe";
}

$topDir = "$cwdDir/$topDir" if IsRelativePath($topDir);
$buildTopDir = "$cwdDir/$buildTopDir" if IsRelativePath($buildTopDir);

$topDir = CanonicalFilename($topDir);
$buildTopDir = CanonicalFilename($buildTopDir);
my $useBuildDir = ($topDir ne $buildTopDir);

$curPath =~ s/^$buildTopDir\/?//;
$curPath = CanonicalFilename($curPath);
my $curPathToTop = $curPath eq '.' ? '.' : CanonicalFilename("../" x (($curPath =~ tr|/||) + 1));

die "$0: error curPath ($curPath) must be contained in buildTopDir ($buildTopDir)." if !IsRelativePath($curPath) || $curPath =~ /^\.\./;
die "$0: error curPath must be contained in '.'" if $curPath =~ /^\.\./;

my $relativeBuildTopDir = '';
my $relativeTopDir = '';
if ($buildTopDir =~ /$topDir\/?(.*)/)  {
    $relativeBuildTopDir = $curPathToTop;
    my $x = $1 || '.';
    my $buildTopToTop = $1 ? ("../" x (($1 =~ /\//) + 1)) : '';
    $relativeTopDir = CanonicalFilename("$curPathToTop/" . ($x eq '.' ? '.' : "../" x (($x =~ tr|/||) + 1)));
}


# make list of directories to search (all dirs from current dir to
# top plus top/config) which is common to all Imakesfile from this
# directory down.

my $commonIncludeDirs = '';
my $currentDir = CanonicalFilename("$topDir/$curPath");
while ($currentDir ne $topDir)  {
    $currentDir = CanonicalFilename("$currentDir/..");
    $commonIncludeDirs .= " -I$currentDir";
}
$commonIncludeDirs .= " -I$buildTopDir/config";
$commonIncludeDirs .= " -I$topDir/config" if $topDir ne $buildTopDir;


# check to make sure shore.def and imake exist

die <<EOF if !-e "$buildTopDir/$defFile";
Error ($0): $buildTopDir/$defFile does not exist. 
    Copy $topDir/$defFile.example to $buildTopDir/$defFile,
    and edit the values contained within as necessary.
EOF

die <<EOF if !-e "$buildTopDir/$imakeExec";
Error ($0): $buildTopDir/$imakeExec does not exist.
    cd to $buildTopDir/$imakeExec and type 'perl bootstrap.pl'
EOF


# print values defined in shore.def for informational purposes

print "Configuring Makefiles with these definitions in $defFile:\n";
my (@undefs);
open INFILE, "<$buildTopDir/$defFile" or die "open $buildTopDir/$defFile";
my $line;
while (defined($line = <INFILE>))  {
    print "\t$line" if $line =~ /^\s*#\s*define/;
    $undefs[$#undefs + 1] = "\t$line" if $line =~ /^\s*#\s*undef/;
}
print join('', @undefs);
close INFILE or die "close $buildTopDir/$defFile";


# do the real work.

DoDir($curPath, $commonIncludeDirs, $relativeTopDir, $relativeBuildTopDir, $relativeTopDir ne '', $useBuildDir, $topDir, $buildTopDir);

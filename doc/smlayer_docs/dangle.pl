#!/s/std/bin/perl  -s

# <std-header style='perl' orig-src='shore'>
#
#  $Id: dangle.pl,v 1.8 1999/06/07 19:09:25 kupsch Exp $
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
# NB: YOU MUST RUN THIS IN THE directory betadoc/
#

$usg = 'usage: dangle.pl';


$debug = 0;

if ($d) {
	$debug = 1;
};

$out = 0;

sub compress_href {
	local($hr)=@_[0];
	$hr =~ s'\/([^\/]*)\/\.\.'';
	return $hr;
}

sub dump {
	if($v) {
		print "print $#ancref refs at start\n";
		foreach $a (%ancref) {
			print "2 end-REF contents: $a\n";
		}
		print "print $#ancref refs at start\n";
		foreach $a (%ancref) {
			print "2 end-DEF contents: $a\n";
		}
	}
}

sub getlist {
	local($fname)=@_[0];
	local($FILELIST)=@_[1];

	$pid = fork;
	if($pid == 0) {
		exec 'find docs -name "*.html" -print '."> $fname";
		if($v) { print "execed ... \n";}
	} else {
		$w = waitpid($pid,0);
		if($v) { print "awaited $w ... \n";}
		if($w==-1) {
			if($v) { print "exec failed\n";}
			exit(1);
		}
		if($w == $pid) {
			open(FILELIST,$fname) || die("couldn't open $fname");
			if($v) { print "opened ... \n";}
		}
	}
}

&getlist("/tmp/tmp$$", FILELIST);

while (<FILELIST>) {
		chop;
		$FILE = $_;
        open(FILE,$FILE) || die "Can't open $FILE\n";

		if($v) { print "$FILE ... \n";}

		@dirpart = split('/',$FILE);
		$dir = @dirpart[0].'/'.@dirpart[1].'/';
		print "======= inspecting file= $FILE...\n";
		# print "======= dir = $dir...\n";

        # translate:
		$out = 0;
		$/ = "<";
        while(<FILE>) { 
			&doit($dir); 
		};
		$/ = "\n";
		if ($out > 0) {
			if($v) { print "\n";}
		}
}
# &dump;
#


# see if any anchor refs have no definitions

print "\n================= Looking for dangling refs.===============\n";

@keys = keys %ancref;
@values = values %ancref;
$totalkeys = $#keys;
while ($#keys >= 0) {
	$x = pop(@values);
	$y = pop(@keys);
	if($x) {
		print "UNDEFINED section anchor --",$y,"-- \n\tis referenced:",
		$x, "\n";
	}
}
print "$totalkeys different hrefs found.\n";

if($v) { print "done\n";}


sub doit {
	local($dir)=@_;
	local($nodup) = 0;
	if($v) { print "\tdoit\tFILE=$FILE at line $. ...\n"; }

	# locate Anchors
	/\s*[Aa]\s+[Nn][Aa][Mm][Ee]\s*=\s*([^>]*)>/ && do {
		$hr = $FILE;
		$anchor = $1;
		if($anchor =~ m/^(\S*)\sHREF=.*$/) {
			$anchor = $1;
			$nodup = 1;
		}
		if($v) { print "\t1\tANCHOR=$anchor= at line $. ...\n"; }

		# strip off # if it's at the beginning
		# because it's about to be put back in
		# and we don't want 2 of them
		$anchor =~ s/^\#//;
		if($v) { print "\t1.5\tdefine $hr#$anchor= at line $. ...\n"; }

		$key = "$hr#$anchor";
		$key =~ &compress_href($key);
		if(@ancdef{$key}) {
			print "$FILE, line $. : --$anchor-- DUPLICATED ...\n", 
				"\t\t last occurence was at line @ancdef{$key} \n"
				unless $nodup;
		} 
		@ancdef{$key} = $.;
		# wipe out any unresolved references 
		@ancref{$key} = "";
	};
	# locate HREFS
	/[Hh][Rr][Ee][Ff]=([^>]*)>/ && do {
		# clean up the href string
		$hr = $1;
		$anchor = "";

		if($v) { print "\t1\tHREF=$hr= at line $. ... \n"; }
		$out++;

		# strip off leading whitepace and quote
		if ($hr =~ m/^\s*"\s*(.*)/) {
			$hr = $1;
		}
		# strip off trailing whitepace and quote
		if ($hr =~ m/(.*)\s*"\s*$/) {
			$hr = $1;
		}

		if($v) { print "\t3\tHREF=$hr= at line $. ... \n"; }
		$orighr = $1;

		# strip off part after #
		if ($hr =~ m/^(.*)#(.*)$/) {
			$hr = $1;
			$anchor = $2;
		}
		if($v) { print "\t4\tHREF=$hr= at line $. ... \n"; }
		# strip off whitepace 
		# if ($hr =~ m/^\S*([^\S]*)\S*$/) {
# 			$hr = $1;
# 		}
		if($v) { print "\t5\tHREF=$hr= at line $. ... \n"; }

		if (length($hr) == 0) {
			if($v) { print "\tfile is self \n"; }
			$hr = $FILE;
			if($v) { print "\tself is $FILE\n"; }
		} else {
			$hr = $dir.$hr;
		}

		# convert remove x/..
		$hr = &compress_href($hr);

		# yet another way...
		if ($hr =~ m?^(.*)\/([^/]*)\/\.\.[\/](\S*)$?) {
			print "\tconverted $hr to $1/$3\n";
			if($v) { print "\tconverted $hr to $1/$3\n"; }
 			$hr = "$1/$3";
 		}

		if ($hr =~ m/(\S*\.html)\#(\S*)/) {
			$hrfile = $1;
		} else {
			$hrfile = $hr;
		}
		if($v) { print "\thrfile is $hrfile\n";}

		if(-e $hrfile) {
			if($anchor) {
				# strip off trailing whitepace and quote
				if ($anchor =~ m/(.*)\s*"\s*$/) {
					$anchor = $1;
				}
				$anchor =~ y/#//;

				$key = "$hr#$anchor";
				$key = &compress_href($key);

				if(@ancdef{$key}) {
					# anchor exists 
					if ($v) 
					{ print "\t--$anchor-- already defined by line $.\n" }
				} else {
					# it's not yet been seen 
					if ($v) 
					{ print 
					"\t--$anchor-- ref used before defined at line $.\n" }
					$temp = @ancref{$key};
					if ($temp) {
						$temp = "$temp,";
					}
					$temp = "$temp\n\t\t$FILE:$.";

					@ancref{$key} =  $temp;
				}
				if ($v) { print "\tREFS to --$anchor-- at $temp\n" }
			}
		} else {
			if($v) { print "\t6 $hr is BAD\n";} else {
				if ($orighr =~ m.^ftp://. ) {
					print "$FILE, line $. : remote $orighr.\n"; 
				} elsif($orighr =~ m.^http://.) {
					print "$FILE, line $. : remote $orighr.\n"; 
				} elsif($orighr =~ m.\S\S*ftp://.) {
					print "$FILE, line $. : BAD REMOTE $orighr.\n"; 
				} elsif($orighr =~ m.\S\S*http://.) {
					print "$FILE, line $. : BAD REMOTE $orighr.\n"; 
				} else {
					print "UNDEFINED file anchor --$orighr--\n";
					print "\tis referenced:\n\t\t$FILE, line $.\n";
				}
			}
		}
	};
}

__END__

# --------------------------------------------------------------- #
# -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- #
# -- University of Wisconsin-Madison, subject to the terms     -- #
# -- and conditions given in the file COPYRIGHT.  All Rights   -- #
# -- Reserved.                                                 -- #
# --------------------------------------------------------------- #

#
# use switches
# -n for nfs
# -m for mount
# -o for other
#
# -v for values
# -s for strings
#
$notdef=0;

#check
$i = 0;
if($v) { $i++; }
if($s) { $i++; }
if($i != 1) {
	printf(STDERR "Must choose one of: -s -v\n");
}
$i=0;
if($r) { $i++; }
if($c) { $i++; }
if($n) { $i++; }
if($m) { $i++; }
if($o) { $i++; }
if($i != 1) {
	printf(STDERR "Must choose one of: -m -n -c -r\n");
}

if($r || $c) {
	$started = 0;
} else {
	$started = 1;
}

while(<>) {
	if($n) {
		/^#define (NFSPROC\w+).*unsigned long.\s*(\d+)/ && 
			&doprint($2,$1);
		/^#define (NFSPROC\w+).*u_long.\s*(\d+)/ && &doprint($2,$1);
	} 
	if($m) {
		/^#define (MOUNTPROC\w+).*unsigned long.\s*(\d+)/ && 
			&doprint($2,$1);
		/^#define (MOUNTPROC\w+).*u_long.\s*(\d+)/ && &doprint($2,$1);
	} 
	if($c || $r) {
		/^#define ([a-z_01-9]+).*unsigned long.\s*(\d+)/ && &doprint($2,$1);
		/^#define ([a-z_01-9]+).*u_long.\s*(\d+)/ && &doprint($2,$1);
	}
}
sub doprint {
	local($val)=@_[0];
	local($str)=@_[1];
	if($str =~ m/czero/) {
		if($c) {
			$started = 1;
		}
		if($r) {
			$started = 0;
		}
	} elsif($str =~ m/vzero/) {
		if($r) {
			$started = 1;
		}
		if($c) {
			$started = 0;
		}
	} elsif($started==1)  {
		if($s) {
			# string
			print "/* ".$val." */ \"".$str."\",\n" ;
		} else {
			# value
			print "$val,\n";
		}
	}
}

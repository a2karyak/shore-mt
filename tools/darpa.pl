#! /s/perl/bin/perl  -s
#
# flags : -d to search for DARPA notices
#       : -c to search for COPYRIGHT notices
#       : -v for verbose (debugging)
#
# NB: YOU MUST RUN THIS IN THE DIRECTORY WITH THE .html files!
# tells you which files don't have darpa sponsorship
#

$usg = 'darpa -d|-c [files]';


($#ARGV >= 0) || die($usg);
($d || $c) || die($usg); 

foreach $FILE (@ARGV) {
        open(FILE,$FILE) || die "Can't open $FILE\n";

		if($v) { print "$FILE ... \n";}
		$n=0;
		$m=0;
		if($d) {
			while(<FILE>) { 
				$n = $n + &darpa;
			};
			if($n < 7) {
				print "$FILE does not have a complete DARPA notice: $n.\n"
			}
		};
		if($c) {
			while(<FILE>) { 
				$m = $m + &copyright;
			};
			if($FILE =~ m/\.\w*\.html/) {
				if($m < 3) {
				print 
				"man page $FILE does not have a complete COPYRIGHT notice: $m.\n"
				}
			} elsif($m < 5) {
				print 
				"TeX document $FILE does not have a complete COPYRIGHT notice: $m.\n"
			}
		};
}

sub copyright {
	$y=0;
	/subject to/ && $y++;
	/terms/ && $y++;
	/Copyright/ && $y++;
	/Computer Sciences Department/ && $y++;
	/199[4-6]/ && $y++;
	return $y;
}
sub darpa {
	$y=0;
	/sponsored/ && $y++;
	/Advanced/ && $y++;
	/Research/ && $y++;
	/Project/ && $y++;
	/Agency/ && $y++;
	/Army/ && $y++;
	/DAAB07[-]92[-]C[-]Q508/ && $y++;
	return $y;
}

__END__

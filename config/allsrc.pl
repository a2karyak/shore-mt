#!/s/std/bin/perl -w
# Use this little script to generate files allconf, allimake, allmake, and
# allsrc.  See also the shell file setup.
use strict 'subs';

%suf = (
	"C" => 1,
	"c" => 1,
	"cc" => 1,
	"h" => 1,
	"hh" => 1,
	"i" => 1,
	"l" => 1,
	"x" => 1,
	"y" => 1,
);

chomp($cwd = `pwd`);
$src = $cwd;
die unless $src =~ s/config$/src/;

die "allconf: $!\n" unless open(OUT,">allconf");
foreach (<Imake.*>) {
	print OUT "$cwd/$_\n";
}
print OUT "$cwd/config.h\n";

die "allimake: $!\n" unless open(IMAKE,">allimake");
die "allmake: $!\n" unless open(MAKE,">allmake");
die "allsrc: $!\n" unless open(SRC,">allsrc");
for $_ (`find ../src -print`) {
	chomp();
	print STDERR "$_\n" unless s|\.\./src||;
	print SRC "$src$_\n"
		if m|\.([^.]+)$| && $suf{$1};
	print IMAKE "$src$_\n"
		if m|/Imakefile$|;
	print MAKE "$src$_\n"
		if m|/Makefile$|;
}

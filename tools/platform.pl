#!perl -w

use strict;
use Config;

my $arch = $Config{archname};
$arch = "nt-$arch" if $arch !~ /nt-/ && $arch =~ /win32/i;

print $arch;

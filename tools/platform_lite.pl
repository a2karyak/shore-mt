#!perl -w

use strict;
use Config;

my $arch = $Config{archname};
$arch = "nt-i386" if $arch !~ /nt-/ && $arch =~ /win32/i;

## real kludge here, but it works for now
$arch = "hpux-parisc" if $arch =~ /PA-RISC2.0/;

print $arch;

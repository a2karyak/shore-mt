#!/s/std/bin/perl -w
use strict 'subs';
#
#  $Header: /p/shore/shore_cvs/tools/addtags.pl,v 1.1 1997/06/13 22:22:15 solomon Exp $
#

$cpysrc = <<EOF;
/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */
EOF
$cpysrc = '';

# Usage:  addtags.pl ctags tags-file [ files ]
# A thin wrapper around ctags;
# Does 'ctags paths', where 'paths' is produced by prefixing each name in
# 'files' with the current directory, merges tags-file with the result,
# and makes the local name 'tags' be a symlink to the resulting tags-file.
# In the special case that 'files' is empty, simply make the link.

$ctags = shift;
$tags = shift;
exit unless @ARGV;
$args = '';
$cwd = `pwd`;
chomp($cwd);
for $arg (@ARGV) {
	$args .=  " $cwd/$arg";
}
dosys("rm -f tags");
dosys("$ctags -dTCaw $args");
if (-s "tags") {
	if (-s $tags) {
		dosys("sort -u -o $tags $tags tags");
	}
	else {
		dosys("sort -u -o $tags tags");
	}
}
dosys("rm -f tags");
dosys("ln -s $tags tags");

sub dosys {
	local ($cmd) = @_;
	# print "$cmd\n";
	system($cmd);
}
__END__

#!/bin/sh -- # A comment mentioning perl to prevent looping.
eval 'exec perl $0 ${1+"$@"}'
    if 0;

# --------------------------------------------------------------- #
# -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- #
# -- University of Wisconsin-Madison, subject to the terms     -- #
# -- and conditions given in the file COPYRIGHT.  All Rights   -- #
# -- Reserved.                                                 -- #
# --------------------------------------------------------------- #

#
#  Perl script for generating server-server messages.
#
#  $Id: remote_msg.pl,v 1.8 1995/05/07 02:04:56 markos Exp $
#

open(DATA, "<remote_msg.dat") || die "cannot open remote_msg.dat: $!\n";
open(ENUM, ">remote_msg_enum.i") || die "cannot open remote_msg_enum.i: $!\n";

$warning = "\n/* DO NOT EDIT --- GENERATED FROM remote_msg.dat */\n\n";
print ENUM $warning;

$base = 0;
$enum = $base;

printf(ENUM "enum msg_type_t  {\n");
LINE:
while (<DATA>)  {
    next LINE if (/^#/);
    ($def, $msg) = split(/\s+/, $_, 2);
    next LINE unless $def;
    printf(ENUM "    %s_msg = %d,\n", $def, $enum);
    ++$cnt;
    ++$enum;
    chop $msg;
}

printf(ENUM "};\n\n");
printf(ENUM "enum {\n");
printf(ENUM "    %-20s = %d,\n", 'min_msg', $base);
printf(ENUM "    %-20s = %d\n", 'max_msg', $enum - 1);
printf(ENUM "};\n");

close DATA;
close ENUM;


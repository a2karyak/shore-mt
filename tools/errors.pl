#
#  $Header: /p/shore/shore_cvs/tools/errors.pl,v 1.13 1995/09/08 16:05:26 nhall Exp $
#
# *************************************************************
#
# usage: <this-script> [-e] [-d] [-v] filename [filename]*
#
# -v verbose
# -e generate enums
# -d generate #defines 
# 
# (both -e and -d can be used)
#
# *************************************************************
#
# INPUT: any number of sets of error codes for software
#	layers called "name", with (unique) masks as follows:
#
#	name = mask {
#	ERRNAME	Error string
#	ERRNAME	Error string
#	 ...
#	ERRNAME	Error string
#	}
#
#  (mask can be in octal (0777777), hex (0xabcdeff) or decimal 
#   notation)
#
#	(Error string can be Unix:ECONSTANT, in which case the string
#	for Unix's ECONSTANT is used)
#
#	If you want the error_info[] structure to be part of a class,
#	put the class name after the mask and groupname and before the open "{"
#	for the group of error messages; in that case the <name>_einfo.i
#	file will look like:
#		w_error_info_t <class>::error_info[] = ...
#	If you don't do that, the name of the error_info structure will
#	have the <name> prepended, e.g.:
#		w_error_info_t <name>_error_info[] = ...
#
# *************************************************************
#
# OUTPUT:
#  for each software layer ("name"), this script creates:
#	STR:  <name>_error.i
#	INFO: <name>_einfo.i  
#	BINFO: <name>_einfo_bakw.h (if -d option is used)
#	HDRE: <name>_error.h (if -e option is used)
#	HDRD: <name>_error_def.h (if -d option is used)
#
#	name_error.i  contains a static char * array, each element is
#		the error message associated with an error code
#	name_einfo.i  contains a definition of a w_error_info_t array
#		for use with the w_error package.
#	name_error.h  contains an enumeration for the
#		error codes , and an enum containing eERRMIN & eERRMAX
#	name_error_def.h contains the #defined constants for the error 
#		codes, and for minimum and maximum error codes
#
# *************************************************************
#
#

if(!$d && !$e) {
	die "You must specify one of -d, -e.";
}
$both = 0;
if($d && $e) {
	$both = 1;
}

$cpysrc = <<EOF;
/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */
EOF


open(ERRS, "gcc -dM -E /usr/include/sys/errno.h |") || die "cannot open /usr/include/sys/errno.h: $!\n";


# set up Unix error codes 
while (<ERRS>)  {
    /^\#define\s+([E]\w*)\s+([1234567890]+)/ && do {
        @errs{$1}=$2;
    }
}
@errkeys = keys %errs;
@errvalues = values %errs;



$_error = '_error';
$_error_def = '_error_def';
$_einfo = '_einfo';
$_info = '_info';
$_errmsg = '_errmsg';
$_msg_size = '_msg_size';
$_OK	= 'OK';

foreach $FILE (@ARGV) {

	&translate($FILE);
	if($v) { printf(STDERR "done\n");}
}

sub translate {
	local($file)=@_[0];
	local($warning)="";
	local($base)=\e;

	open(FILE,$file) || die "Cannot open $file\n";
	if($v) { printf (STDERR "translating $file ...\n"); }
	$warning = 
			"\n/* DO NOT EDIT --- GENERATED from $file by errors.pl */\n\n";

	LINE: while (<>)  {
		next LINE if (/^#/);
		# {
		s/\s*[}]// && do {
			if($v) { 
				printf(STDERR "END OF PACKAGE: ".$basename.",".$BaseName." = 0x%x\n", $base);
			}

			# {
			print INFO "};\n\n";
			if($class) {
				print(INFO 'void '.$class.'::init_errorcodes(){'."\n");
				print(INFO "\tif (! (w_error_t::insert(\n\t\t");
				print(INFO $groupname.', '.$class.'::error_info,',"\n\t\t");
				print(INFO $basename.'ERRMAX - '.$basename.'ERRMIN + 1)) ) {');
				print(INFO "\n\t\t\t W_FATAL(fcINTERNAL);\n\t}\n}\n");
			}
			if($d) {
				print BINFO "};\n\n";
			}

			if($e) {
				#{
				printf(HDRE "};\n\n");
				printf(HDRE "enum {\n");
				printf(HDRE "    %s%s = 0x%x,\n", $basename, 'ERRMIN', $base);
				printf(HDRE "    %s%s = 0x%x\n", $basename, 'ERRMAX', $highest);
				printf(HDRE "};\n\n");
			}
			if($d) {
				printf(HDRD "#define $BaseName%s%-20s  0x%x\n", '_','ERRMIN', $base);
				printf(HDRD "#define $BaseName%s%-20s  0x%x\n", '_','ERRMAX', $highest);
				printf(HDRD "\n");
			}

			# {
			print STR "\t\"dummy error code\"\n};\n";
			print STR "const ".$basename.$_msg_size." = $cnt;\n\n";

			if($e) {
				printf(HDRE "#endif /*__".$basename."_error_h__*/\n");
				close HDRE;
			}
			if($d) {
				printf(HDRD "#endif /*__".$basename."_error_def_h__*/\n");
				close HDRD;
			}
			printf(STR "#endif /*__".$basename."_error_i__*/\n");
			close STR;
			printf(INFO "#endif /*__".$basename."_einfo_i__*/\n");
			close INFO;
			if($d) {
				printf(BINFO "#endif /*__".$basename."_einfo_bakw_i__*/\n");
				close BINFO;
			}

			$basename = "";
			$BaseName = "";
			$base = \e;
		};

		s/\s*(\S+)\s*[=]\s*([0xabcdef123456789]+)\s*(["].*["])\s*(.*)[{]// && do {
			$basename = $1;
			$base = $2;
			$groupname = $3;
			$class = $4;

			$BaseName = $basename;
			$BaseName =~ y/a-z/A-Z/;
			$base = oct($base) if $base =~ /^0/;
			if($class){
				if($v) {
					printf(STDERR "CLASS=$class\n");
				}
			}

			$cnt = -1;
			$highest = 0;

			if($v) { 
				printf(STDERR "PACKAGE: $basename,$BaseName = 0x%x\n", $base);
			}


			if($d) {
				if($v) {printf(STDERR "trying to open ".$basename.$_error_def.".h\n");}
				open(HDRD, ">$basename$_error_def.h") || 
					die "cannot open $basename$_error_def.h: $!\n";

				printf(HDRD "#ifndef __$basename%serror_def_h__\n", '_');
				printf(HDRD "#define __$basename%serror_def_h__\n", '_');
				printf(HDRD $cpysrc);
			}
			if($d) {
				if($v) {printf(STDERR "trying to open ".$basename."_einfo_bakw.h\n");}
				open(BINFO, ">$basename"."_einfo_bakw.i") || 
					die "cannot open $basename"."_einfo_bakw.i: $!\n";

				printf(BINFO "#ifndef __$basename"."_einfo_bakw_i__\n");
				printf(BINFO "#define __$basename"."_einfo_bakw_i__\n");
				printf(BINFO $cpysrc);
			}
			if($e) {
				if($v) {printf(STDERR "trying to open $basename$_error.h\n");}
				open(HDRE, ">$basename$_error.h") || 
					die "cannot open $basename$_error.h: $!\n";
				printf(HDRE "#ifndef __$basename%serror_h__\n", '_');
				printf(HDRE "#define __$basename%serror_h__\n", '_');
				printf(HDRE $cpysrc);
			}

			if($v) {printf(STDERR "trying to open $basename$_error.i\n");}
			open(STR, ">$basename$_error.i") || 
				die "cannot open $basename$_error.i: $!\n";

			printf(STR "#ifndef __".$basename."_error_i__\n");
			printf(STR "#define __".$basename."_error_i__\n");
			printf(STR $cpysrc);

			if($v) {printf(STDERR "trying to open $basename$_einfo.i\n");}
			open(INFO, ">$basename$_einfo.i") || 
				die "cannot open $basename$_einfo.i: $!\n";
			printf(INFO "#ifndef __".$basename."_einfo_i__\n");
			printf(INFO "#define __".$basename."_einfo_i__\n");
			printf(INFO $cpysrc);

			if($e) {
				print HDRE $warning;
			} 
			if($d) {
				print HDRD $warning;
				print BINFO $warning;
			}
			print STR $warning;
			print INFO $warning;

			if($class) {
				print INFO 
					"w_error_info_t ".$class."::error".$_info."[] = {\n"; #}
			} else {
				print INFO 
					"w_error_info_t ".$basename.$_error.$_info."[] = {\n"; #}
			}
			if($e) { printf(HDRE "enum { \n"); } #}
			if($d) { printf(HDRD "#define $BaseName%s%-20s 0\n", '_','OK');}
			printf(STR "static char* ".$basename.$_errmsg."[] = {\n"); #}

			if($d) {
				print BINFO  
					"w_error_info_t ".$basename.$_error.$_info."_bakw[] = {\n"; #}
			}
		};  # } to match the one in the pattern
		

		next LINE if $base =~ \e;
		($def, $msg) = split(/\s+/, $_, 2);
		next LINE unless $def;
		chop $msg;
		++$cnt;
		if($msg =~ m/^Unix:(.*)/) {
			# 		$def is unchanged
			$val = $errs{$1};
			$!= $val;
			$msg = qq/$!/; # force it to be a string
			$val = $val + $base;
		} else {
			$msg = qq/$msg/;
			$val = $cnt + $base;
		}
		if($highest < $val) { $highest = $val; }

		if($d) {
			printf(HDRD "#define $BaseName%s%-20s 0x%x\n", '_', $def, $val);
		}
		if($e) {
			printf(HDRE "    $basename%-20s = 0x%x,\n", $def, $val);
		}

		if($e) {
			printf(STR "/* $basename%-18s */ \"%s\",\n",  $def, $msg);
		} else {
			printf(STR "/* $BaseName%s%-18s */ \"%s\",\n",  '_', $def, $msg);
		}

		if($e) {
			printf(INFO " { $basename%-18s, \"%s\" },\n", $def, $msg);
		} else {
			printf(INFO " { $BaseName%s%-18s, \"%s\" },\n", '_', $def, $msg);
		}

		if($d) {
			printf(BINFO " { $BaseName%s%s, \"$BaseName%s%s\" },\n",
				'_', $def, '_', $def);
		}
	} # LINE: while

	if($v) { printf(STDERR "translated $file\n");}

	close FILE;
}


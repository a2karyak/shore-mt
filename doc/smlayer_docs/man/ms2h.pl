#! /s/perl/bin/perl -s

# <std-header style='perl' orig-src='shore'>
#
#  $Id: ms2h.pl,v 1.9 1999/06/07 19:09:27 kupsch Exp $
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
# NB: THIS IS A LARGELY HACKED VERSION OF ms2h.pl --
# DON'T COUNT ON ANY OF THE ms MACROS WORKING ANYMORE.
# I HACKED THIS TO CONVERT man MACROS, at the expense of
# ms MACROS.  I DIDN'T BOTHER TO REMOVE THE ms MACROS THOUGH.
#	-Nancy
#
#
# ms2html       --- convert (pseudo) troff -ms text to HTML
#
# see:
# http://cui_www.unige.ch/ftp/PUBLIC/oscar/scripts/README.html
#
# Converts an annotated text file into HTML
# The annotations are based on troff -ms macros,
# but may be generated in a variety of ways.
# For example, a Framemaker template (doc2ms.fm)
# can be applied to an existing document to
# insert the annotations for every paragraph type.
#
# A Framemaker template that implements the annotations is in:
# cui.unige.ch:PUBLIC/oscar/doc2ms.fm.Z
#
# A Table of Contents and links to references and numbered sections
# are automatically generated.
#
# Each of the following commands should appear on a line by itself
# (although arbitrary white space is tolerated, to facilitate
# translation from Framemaker files):
#
# Standard troff -ms:
#       .TL     Title
#       .ST     Subtitle
#       .AU     Author
#       .AI     Author's Institution
#       .AB     Abstract
#       .NH1    Numbered Section
#       .NH2    Numbered Subsection
#       .NH3    Numbered Subsubsection
#       .NH4    Numbered Subsubsubsection
#       .SH1    Unnumbered Section
#       .SH2    Unnumbered Subsection
#       .SH3    Unnumbered Subsubsection
#       .SH4    Unnumbered Subsubsubsection
#       .LP     Left Paragraph
#       .PP     Indented Paragraph
#       .IP     Indented Paragraph
#       .QP     Quotation
#       .FS     Footnote
#       .DS     Display Start
#       .\"     Comment
# Non-standard troff -man:
#
#       .TH     title
#       .SH     section header
#       .B     bold
#       .BI     bold-italic
#       .BR     bold-roman
#       .I     italic
#       .IB     italic-bold
#       .IR     italic-roman
#       .R     roman
#       .RB     roman-bold
#       .RI     roman-italic
#       .EX     example (verbatim) 
#       .EE     end example
# Non-standard:
#       .BC     Block - Centred
#       .BH     Block - Hang Indented
#       .BU1    Bullet Item (level 1)
#       .BU2    Bullet Item (level 2)
#       .BU3    Bullet Item (level 3)
#       .BU4    Bullet Item (level 4)
#       .LL     Left Label (Bold .LP)
#       .NS     Start Numbered Paragraph
#       .NN     Next Numbered Paragraph
#       .MD     Math Definition
#       .ML     Math Lemma
#       .MP     Math Proposition
#       .MT     Math Theorem
#       .PR     Proof
#       .UR     Unnumbered Reference
#       .RF[1]  Reference
#
# In addition, the following are understood:
#       [RF:1]  A cross reference:
#       \fB     start bold text
#       \fI     start italic text
#       \fC     start typewriter text
#       \fR     return to Roman text (also \fP)
#       \.      dot (at beginning of line)
#
# NB: note that .B and .I are not valid commands since they would
# start a new "paragraph" (this script splits text at command lines).
# Just convert ".B text" to "\fBtext\fR".
#
# Author: Oscar Nierstrasz -- oscar@cui.unige.ch -- June 1993
#
# This script and friends can be found at:
# http://cui_www.unige.ch/ftp/PUBLIC/oscar/scripts/README.html
# or ftp: cui.unige.ch:/PUBLIC/oscar/scripts/
#
#v = "ms2html v1.1"; # 6.93
#v = "ms2html v1.2"; # 1.7.93   -- changed .DA to .BU3
#                               -- tweaked file URL recognition
#v = "ms2html v1.3"; # 2.7.93   -- fixed separate numbering for NH and SH
#                               -- added named TOC anchor
#v = "ms2html v1.4"; # 8.7.93   -- added .LL
#v = "ms2html v1.5";
#v = "ms2html v1.6"; # 4.8.93   -- made url'href into a library
#v = "ms2html v1.7"; # 25.8.93  -- added \. escape at beginning of line
#                               -- changed TOC to use <UL>
$v = "ms2html v1.8"; # 31.3.94  -- url package split; ms2html uses html.pl

# Fix this path to wherever button.pl and html.pl are installed:
unshift(@INC,"..");

require("button.pl");
require("html.pl");

$usg = 'Usage: ms2html [-<options>] <msfile.ms> ...
        -b/-nob -- create a single body page only (default is b)
        -t/-not -- create table of contents (default is not)
        -p/-nop -- use plain text instead of button to navigate (default is p)
';

$debug = $d;

($#ARGV >= 0) || die($usg);

$longdate  = "";
$date = "";
&getdate;



# The record separator is a carriage return followed by a dot:
# $/ = "\n\." ;

# The signature for each page.  Change this to whatever is appropriate ...
#omn = '<A HREF="http://cui_www.unige.ch/OSG/omn.html"><I>OMN</I></A><P>';
%sig = "<P><I>This document was translated by a descendent of $v on $date.</I>\n$omn<P></BODY>\n";

$ftp = '<A HREF="http://cui_www.unige.ch/ftp/PUBLIC/oscar/scripts/README.html">';
# $sig = "<I>This document was translated by $ftp$v</A> on $date.</I><P>\n";

$nbodies=0;



foreach $FILE (@ARGV) {
        $TL = 0; # TOC level (starts at 0)
        $toc = $refs = "";
        open(FILE,$FILE) || die "Can't open $FILE\n";
        ($BASE = $FILE) =~ s/\.ms$//;   # drop the .ms suffix
        $TOC = $BASE . "-toc.html";     # the title page
        $CURR = $PREV = $TOC;           # current and previous pages


        &newpage($TOC); $inbody = 0;

		# fix arguments
		$b=1;
		if ($nob) {$b=0};
		$t=0;
		if ($not) {$t=0};
		$p=1;
		if ($nop) {$p=0};
        if ($b) { $TOTOC = ""; }
        else { $TOTOC = $TOC; }

        # some useful strings:
		# unfortunately, brain-damaged mosaic
		# can't handle &#60a to mean "<a"
        $REFS = $BASE . "-refs.html";
        $totoc = "<I>To <A HREF=\"$TOTOC#TOC\">Table of Contents</A></I><P>\n";
        $torefs = "<I>To <A HREF=\"$REFS\">References</A></I><P>\n";

        # translate:
        while(<FILE>) { 
			&ms2html; 
		}

        # gracefully close the last body page:
        &lastbody;

        # put the collected table of contents at the end
        # of the title page:
        if ($t) {
			while ($TL > 0) { $toc .= "</UL>\n"; $TL--; }
			open(TOC, ">>$TOC");
			print TOC "<H1><A NAME=\"TOC\">Table of Contents</H1>\n$toc\n";
			if ($refs =~ /./) { print TOC $torefs; }
			print TOC $sig;
			close(TOC);
		}

        # if there are references, print them out:
        if ($refs =~ /./) {
                &newpage($REFS);
                &printtitle("References");
                print "<H2>References</H2>\n";
                print "<OL>\n$refs\n</OL>\n";
                &up; print "<P>\n";
                print $sig;
                close(STDOUT);
        }
}

# convert some standard sequences to HTML:
sub accent2html {
        # escape & < and >:
        s/\&/\&amp;/g;
        s/</\&lt;/g;
        s/>/\&gt;/g;

        # convert dead-key accents to HTML
        s/\\AE/\&AElig;/g;
        s/\\'([AEIOUYaeiouy])/\&$1acute;/g;
        s/\\[<^]([AEIOUaeiou])/\&$1circ;/g;
        s/\\`([AEIOUaeiou])/\&$1grave;/g;
        s/\\o([Aa])/\&$1ring;/g;
        s/\\~([ANOano])/\&$1tilde;/g;
        s/\\[:"]([AEIOUYaeiouy])/\&$1uml;/g;
        s/\\,([Cc])/\&$1cedil;/g;
        s/\\\/([Oo])/\&$1slash;/g;
        s/\\ss/\&szlig;/g;
}

# translate the next line:
sub ms2html {

        # pass through anything that doesn't begin with "."
        /^[^.]/ && do { 
			# convert < and >
			s/</\&lt;/g;
			s/>/\&gt;/g;
			# $_ =~ s/</$lt/g; 
			# $_ =~ s/>/$gt/g; 
			print "$_"; return; 
		};        

		# delete initial "." (only needed for first record)
        s/^\.//;        # delete initial "." (only needed for first record)
        s/\n+\./\n./;      # delete record separaters
        s/\s+\n/\n/g;   # delete trailing white space
        s/\n\\\./\n./g; # unescape leading dots
        &accent2html;   # expand accents
        s/\\f([IB])([^\\]*)\\f[RP]/<$1>$2<\/$1>/g;      # italics & bold
        s/\\fC([^\\]*)\\f[RP]/<CODE>$1<\/CODE>/g;       # code

#        &html'href;

        # expand references into HTML links:
        s/\[RF:(\d*)\]/<A HREF="${REFS}#RF:$1">[$1]<\/A>/g;

        if (/^$/) { return; }   # blank record!?

        # separate the text from the command:
        $text = "";

		# replace $_ with the command, $text with the balance
        s/^(\S+[ \t]+)// && do { $text = $_; $_ = $1; };

		# strip white space from end of command
        s/^(\S+)[ \t]+/$1/;


		# ignore .so files
        /^so$/ && do { return; };

        &popall unless
                /^[LIP][PL]/ || /^N[SN]$/ || /^BU[1234]/ 
				|| /^HR/ 
				|| /^[BIRC]/ 
				|| /^[SV]A/ 
				|| /^FN/
				;

        /^PD$/ && do { return; };

        # don't distinguish LL & PP for nesting purposes:
        /^LP$/ && do { &listitem("LP"); print "$text\n"; return; };
        /^PP$/ && do { &listitem("PP"); print "$text\n"; return; };

        # don't distinguish LL & IP for nesting purposes:
        /^LL$/ && do { &listitem("IP"); print "<DT><B>$text</B>\n"; return; };

		# restore default tabs -- ignore
        /^DT$/ && do { return; };
		# index entry -- ignore
        /^IX$/ && do { return; };
		# vertical paragraph distance-- ignore
		# relative indent start & end -- ignore
        /^RE$/ && do { return; };
        /^RS$/ && do { return; };
		# reduce font size -- ignore
        /^SM$/ && do { return; };
		# tagged paragraph w/ tag on next line -- ignore
        /^TP$/ && do { return; };
		# resolve title abbrev - ignore
        /^TX$/ && do { return; };
		# hanging indent - ignore
        /^HP$/ && do { return; };

        /^IP$/ && do { 
			&listitem("IP");
			if(split(/"/,$text,3)==1)  {
				split(/[ \t\n]+/,$text,4);
				$first=0;
				$second=1;
				$third=2;
			} else {
				$first=1;
				$second=2;
				$third=3;
			};
			if (@_[$second] =~ /\d+/) {
				print "<DT> @_[$first] <DD>@_[$third]";
			} else {
				print "<DT> @_[$first] <DD>@_[$second]";
			};
			return;
		};
		# .HR underlined href rest
        /^HR$/ && do {
			if(split(/"/,$text,3)==1)  {
				split(/[ \t\n]+/,$text,4);
				$first=0;
				$second=1;
				$third=2;
			} else {
				$first=1;
				$second=2;
				$third=3;
			};
			print 
			"<A HREF=@_[$second]><B> @_[$first] </B></A>\n@_[$third]";
			return;
		};
        /^SA$/ && do {
			if (split(/[()]/,$text)>0) {
				print "<A HREF=@_[0].@_[1].html><B> @_[0](@_[1]) </B></A>\n @_[2]";
				return;
			};
			print "\n<!-- unknown SA: $text >\n";
			return;
		};
        /^\\"$/ && do { 
			return; 
		};
        # these are ignored:
        (/^AE$/ || /^FE$/ || /^DE$/) && do { return; };


		#
		# .TH page section date footer middle
		#	ignores date, footer, middle
		#   inserts today for date
        /^TH$/ && do {
			split(/[" ]+/,$text,3);
			$title = "@_[0](@_[1])";
			&printtitle("<em>Shore Programmer's Manual</em> - $longdate");
			return;
		};

		# darpa sponsorship
        /^DA$/ && do {
			print "\n<H2>VERSION</H2>\n";
			print  "This manual page applies to Version 1.0 of the";
			print  "Shore software.";

			print "\n<H2>SPONSORSHIP</H2>\n";
			print " The Shore project ";
			print "is sponsored by the Advanced Research Project Agency, ";
			print "ARPA order number 018 (formerly 8230), monitored by the ";
			print "U.S. Army Research Laboratory under contract ";
			print "DAAB07-92-C-Q508.";

			print "\n<H2>COPYRIGHT</H2>\n";
			print "Copyright (c) 1994, 1995, 1996 Computer Sciences Department, ";
			print "University of Wisconsin -- Madison. All Rights Reserved.";
			return;
		};

		# section heading
		# .SH text
        /^S([SH])$/ && do {
			$name = "SH-$text";         # unique anchor name
			$num = "";

			# get header size
			if($1=~/S/) { $H = 3; }
			else { $H = 2; }

			if(split(/"/,$text,3)==1)  {
				print "<H$H>$text</H$H>\n";
			} else {
				print "<H$H>@_[1]</H$H>\n"; # the rest is ignored
			}
			return;
		};


        /^R$/ && do {	# roman
			print "$text"; return;
		};

		# HTML doesn't have a roman font, and italics 
		# should really be <em>, but we use <i> and <r> anyway,
		# and a good browser will ignore them
		#
		# fonts

        /^([BIRC])([BIRC])$/ && do {	# font, font
			$one=$1;
			$two=$2;
			if (m/^B/) { $one = "STRONG";};
			if (m/^.B/) { $two = "STRONG";};
			if (m/^I/) { $one = "EM";};
			if (m/^.I/) { $two = "EM";};
			if (m/^C/) { $one = "TT";};
			if (m/^.C/) { $two = "TT";};
			if (m/^R/) { $one = "R";}; # roman- ignored by html
			if (m/^.R/) { $two = "R";};
			if(split(/"/,$text,3)==1)  {
				split(/[ \t\n]+/,$text,2);
				$first=0;
				$second=1;
			} else {
				$first=1;
				$second=2;
			};
			print "<$one>@_[$first]</$one><$two>@_[$second]</$two>\n";
			return;
		};

		#    fonts
		#    treat .SB as .B
        /^SB/ && do {
			$one="STRONG";
			if(split(/"/,$text,3)==1)  {
				split(/[ \t\n]+/,$text,2);
				$first=0;
				$second=1;
			} else {
				$first=1;
				$second=2;
			};
			print "<$one>@_[$first]</$one>@_[$second]\n";
			return;
		};
        /^([BIRC])$/ && do {	# [bold,italic,roman,constant]
			$one=$1;
			if (m/^I/) { $one = "EM";};
			if (m/^C/) { $one = "TT";};
			if (m/^R/) { $one = "R";}; # ignored 
			if (m/^B/) { $one = "STRONG";};
			if(split(/"/,$text,3)==1)  {
				split(/[ \t\n]+/,$text,2);
				$first=0;
				$second=1;
			} else {
				$first=1;
				$second=2;
			};
			print "<$one>@_[$first]</$one>@_[$second]\n";
			return;
		};
        /^IN$/ && do {	# include
			if(split(/"/,$text,3)==1)  {
				split(/[ \t\n]+/,$text,2);
				$first=0;
				$second=1;
			} else {
				$first=1;
				$second=2;
			};
			print "#include &#60;@_[$first]&#62;@_[$second]\n";
			return;
		};
    #  .FN and .VA
    # NB: if you change either of these, be sure to change
    # the nroff definitions in tmac.man.local
        /^([FV][NA])$/ && do {  # function name or variable name
								# in the midst of a paragraph
			if(split(/"/,$text,3)==1)  {
				split(/[ \t\n]+/,$text,2);
				$first=0;
				$second=1;
			} else {
				$first=1;
				$second=2;
			};
			if($1=~/FN/) {
				print "<STRONG> @_[$first]</STRONG>\n@_[$second]\n";
			} else {
				print "<EM> @_[$first]</EM>\n@_[$second]\n";
			}
			return;
		};
        /^EX$/ && do { # example code
			print "<PRE>\n$text"; return;
		}; 
        /^EE$/ && do { # end example
			print "</PRE>\n"; return;
		};

		# unknown command?
		# consider it untreated text:
        print "$_$text";
}

# close the current body page and open a new one:
sub newbody {
		$nbodies ++;
        local($NEXT) = @_;
		if ($debug>0) { print STDERR "$. newbody ($nbodies) \n"};
        &popall;
        if ($inbody) {
                &left; &up; &right; print "<P>\n";
                print $sig;
        }
        close(STDOUT);
        $PREV = $CURR; $CURR = $NEXT;
        &newpage($CURR);
}

# terminate the last body page:
sub lastbody {
		if ($debug>0) { print STDERR "$. lastbody ($nbodies) \n"};
        local($NEXT);
        &popall;
		if($nbodies >1) {
			&left; &up;
		}
        # pointer to next only if references exist:
        if ($refs =~ /./) { $NEXT = $REFS; &right; };
        print "<P>\n";
        # clean up:
        print $sig;
        close(STDOUT);
}

# open a new page:
sub newpage {
        local($PAGE) = @_;
        open(STDOUT, ">$PAGE") || die "Can't create $PAGE";
        print STDERR "Created $PAGE\n";
}

# check if need to push or pop a list level
# when a new list item appears:
sub listitem {
        local($ltype) = @_;     # this list item type
		if ($debug>0) { 
			print STDERR "li1: listitem $ltype\n";
		}
		local($top) = $lstack[$#lstack];
		local($topm1) = "--empty--";
		if  ($#lstack > 0)  {
			local($topm1) = $lstack[$#lstack-1];
		}
        if ($#lstack < 0) {
			&newlist($ltype);
			return;
		}

		if ($ltype =~ /^IP/) {
			if ($top ne $ltype) {
				&newlist($ltype);
			}
		} elsif ($ltype =~ /^LP/) {
			if ($top =~ /^IP/) {
				if ($debug>0) { 
				print STDERR "li3: Popping from $top to $topm1\n";
				};
				&poplist;
			}
			print "<P>";
		} elsif ($ltype =~ /^PP/) {
			print "<P>";
		} else {
			print "<P>";
			&newlist($ltype);
		};
}

# start a new list:
sub newlist {
        local($ltype) = @_;     # this list item type
		if ($debug>0) { print STDERR "$. newlist of $ltype \n";};

        if ($ltype eq "NS") { print "<OL>\n"; $ltype = "NN"; }
        elsif ($ltype eq "NN") { print "<OL>\n"; }
        elsif ($ltype =~ /BU[1234]/) { print "<UL>\n"; }
       	elsif ($ltype =~ /IP/) { print "<DL>\n"; }
       	elsif ($ltype =~ /PP/) { print "<P>\n"; }
       	elsif ($ltype =~ /LP/) { print "<P>\n"; }
        push(@lstack,$ltype);
	if ($debug>0) { print STDERR "$. pushed ($#lstack) $ltype \n";};
}

# pop the current list:
sub poplist {
	local($ltype);

	if ($debug>0) { print STDERR "$. poplist ($#lstack)\n";};

	$ltype = pop(@lstack);
	if ($debug>0) { print STDERR "$. popped $ltype to ($#lstack) $lstack[$#lstack-1]\n";};

	if ($ltype eq "NN") { print "</OL>\n"; }
	elsif ($ltype =~ /BU[1234]/) { print "</UL>\n"; }
	elsif ($ltype =~ /LP/) { print "<P>"; }
	elsif ($ltype =~ /PP/) { print "<P>"; }
	elsif ($ltype =~ /IP/) { print "</DL>\n"; }
	else { print STDERR "poplist error: unknown list type \"$ltype\"\n"; }
                # should never happen!
}

# pop out of all remaining lists:
sub popall {
	if ($debug>0) { print STDERR "$. popall ($#lstack) \n"};
        while ($#lstack >= 0) {
                &poplist;
        }
}

# yep, you guessed it!
sub printtitle {
        local($name) = @_;
        print "<HEAD><TITLE>$title -- $name</TITLE></HEAD>\n" ;
        print "<BODY><H1>$title</H1><H2>$name</H2>";
}

# standard buttons:
sub up {
		if (!$t) { return;}
        if ($p) { print $totoc; }
        else { &button("up","$TOC#TOC"); }
}
sub left {
        if ($p) { print "<I>To <A HREF=\"$PREV\">Previous Page</A></I><P>\n"; }
        else { &button("left","$PREV"); }
}
sub right {
        if ($p) { print "<I>To <A HREF=\"$NEXT\">Next Page</A></I><P>\n"; }
        else { &button("right","$NEXT"); }
}

sub getdate {
	local(%months) = (0,"January",1,"February",
		2,"March", 3,"April", 4,"May", 5,"June",
		6,"July",7,"August",8,"September",9,"October",
		10,"November",11,"December");
	local($s,$m,$h,$mday,$mon,$year,$wday,$yday,$isdst) = localtime(time);

	$longdate = "$mday $months{$mon} $year";
	chop($date = `date +%d.%m.%y`);
}

__END__

#! /s/perl/bin/perl

# <std-header style='perl' orig-src='shore'>
#
#  $Id: html.pl,v 1.7 1999/06/07 19:09:27 kupsch Exp $
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
# html.pl       --- extract, normalize and hypertextify URLs in HTML files
#
# This package contains:
#
# html'href:    identify URLs and turn them into hypertext links
# html'abs:     convert relative URLs to absolute ones
# html'parse:   parse an URL and return ($type,$host,$port,$path,$request)
# html'hrefs:   return all hrefs in a page
#
# Oscar Nierstrasz 26/8/93 oscar@cui.unige.ch
#
# 15/01/94 -- fixed html'abs to handle HREFs without surrounding quotes
# 09/02/94 -- fixed html'abs to handle images as well
# 24/3/94 -- added hrefs (from `explore')
# 25/3/94 -- fixed hrefs to handle malformed HREFs (missing or extra quotes)
# 25/3/94 -- fixed abs to leave internal refs alone!
# 25/3/94 -- moved to separate package
# 13/4/94 -- repaired abs() to handle HREFs with missing quotes
#
# BUGS: Craig Allen <ccount@mit.edu> points out that binary files
# that contain "<a" will be damaged by html'abs ...

package html;

# Try to recognize URLs and ftp file indentifiers and convert them into HREFs:
# This routine is evolving.  The patterns are not perfect.
# This is really a parsing problem, and not a job for perl ...
# It is also generally impossible to distinguish ftp site names
# from newsgroup names if the ":<directory>" is missing.
# An arbitrary file name ("runtime.pl") can also be confused.
sub href {
        # study; # doesn't speed things up ...

        # to avoid special cases for beginning & end of line
        s|^|#|; s|$|#|;

        # URLS: <serice>:<rest-of-url>
        s|(news:[\w.]+)|<A HREF="$&">$&</A>|g;
        s|(http:[\w/.:+\-~]+)|<A HREF="$&">$&</A>|g;
        s|(file:[\w/.:+\-]+)|<A HREF="$&">$&</A>|g;
        s|(ftp:[\w/.:+\-]+)|<A HREF="$&">$&</A>|g;
        s|(wais:[\w/.:+\-]+)|<A HREF="$&">$&</A>|g;
        s|(gopher:[\w/.:+\-]+)|<A HREF="$&">$&</A>|g;
        s|(telnet:[\w/.:+\-]+)|<A HREF="$&">$&</A>|g;
        # s|(\w+://[\w/.:+\-]+)|<A HREF="$&">$&</A>|g;

        # catch some newsgroups to avoid confusion with sites:
        s|([^\w\-/.:@>])(alt\.[\w.+\-]+[\w+\-]+)|$1<A HREF="news:$2">$2</A>|g;
        s|([^\w\-/.:@>])(bionet\.[\w.+\-]+[\w+\-]+)|$1<A HREF="news:$2">$2</A>|g;
        s|([^\w\-/.:@>])(bit\.[\w.+\-]+[\w+\-]+)|$1<A HREF="news:$2">$2</A>|g;
        s|([^\w\-/.:@>])(comp\.[\w.+\-]+[\w+\-]+)|$1<A HREF="news:$2">$2</A>|g;
        s|([^\w\-/.:@>])(gnu\.[\w.+\-]+[\w+\-]+)|$1<A HREF="news:$2">$2</A>|g;
        s|([^\w\-/.:@>])(misc\.[\w.+\-]+[\w+\-]+)|$1<A HREF="news:$2">$2</A>|g;
        s|([^\w\-/.:@>])(news\.[\w.+\-]+[\w+\-]+)|$1<A HREF="news:$2">$2</A>|g;
        s|([^\w\-/.:@>])(rec\.[\w.+\-]+[\w+\-]+)|$1<A HREF="news:$2">$2</A>|g;

        # FTP locations (with directory):
        # anonymous@<site>:<path>
        s|(anonymous@)([a-zA-Z][\w.+\-]+\.[a-zA-Z]{2,}):(\s*)([\w\d+\-/.]+)|$1<A HREF="file://$2/$4">$2:$4</A>$3|g;
        # ftp@<site>:<path>
        s|(ftp@)([a-zA-Z][\w.+\-]+\.[a-zA-Z]{2,}):(\s*)([\w\d+\-/.]+)|$1<A HREF="file://$2/$4">$2:$4</A>$3|g;
        # <site>:<path>
        s|([^\w\-/.:@>])([a-zA-Z][\w.+\-]+\.[a-zA-Z]{2,}):(\s*)([\w\d+\-/.]+)|$1<A HREF="file://$2/$4">$2:$4</A>$3|g;
        # NB: don't confuse an http server with a port number for
        # an FTP location!
        # internet number version: <internet-num>:<path>
        s|([^\w\-/.:@])(\d{2,}\.\d{2,}\.\d+\.\d+):([\w\d+\-/.]+)|$1<A HREF="file://$2/$3">$2:$3</A>|g;

        # just the site name (assume two dots): <site>
        s|([^\w\-/.:@>])([a-zA-Z][\w+\-]+\.[\w.+\-]+\.[a-zA-Z]{2,})([^\w\d\-/.:!])|$1<A HREF="file://$2">$2</A>$3|g;
        # NB: can be confused with newsgroup names!
        # <site>.com has only one dot:
        s|([^\w\-/.:@>])([a-zA-Z][\w.+\-]+\.com)([^\w\-/.:])|$1<A HREF="file://$2">$2</A>$3|g;

        # just internet numbers:
        s|([^\w\-/.:@])(\d+\.\d+\.\d+\.\d+)([^\w\-/.:])|$1<A HREF="file://$2">$2</A>$3|g;
        # unfortunately inet numbers can easily be confused with
        # european telephone numbers ...

        s|^#||; s|#$||;
}

# convert relative http URLs to absolute ones:
# BUG: minor problem with binary files containing "<a" ...
sub abs {
        local($url,$page) = @_;
        ($type,$host,$port,$path,$request) = &parse(undef,undef,undef,undef,$url);
        $root = "http://$host:$port";
        @hrefs = split(/<[Aa]/,$page);
        $n = $[;
        while (++$n <= $#hrefs) {
                # absolute URLs ok:
                ($hrefs[$n] =~ m|href\s*=\s*"?http://|i) && next;
                ($hrefs[$n] =~ m|href\s*=\s*"?\w+:|i) && next;
                # internal refs ok:
                ($hrefs[$n] =~ m|href\s*=\s*"?#|i) && next;
                # relative URL from root:
                ($hrefs[$n] =~ s|href\s*=\s*"?/([^">]*)"?|HREF="$root/$1"|i) && next;
                # relative from $path:
                $hrefs[$n] =~ s|href\s*=\s*"?([^/"][^">]*)"?|HREF="$root$path$1"|i;
                # collapse relative paths:
                $hrefs[$n] =~ s|/\./|/|g;
                while ($hrefs[$n] =~ m|/\.\./|) {
                        $hrefs[$n] =~ s|[^/]*/\.\./||;
                }
        }
        # Actually, this causes problems for binary files
        # that just happen to include the sequence "<a"!!!
        $page = join("<A",@hrefs);
        # duplicate code could be merged into a subroutine ...
        @hrefs = split(/<IMG/i,$page);
        $n = $[;
        while (++$n <= $#hrefs) {
                # absolute URLs ok:
                ($hrefs[$n] =~ m|src\s*=\s*"?http://|i) && next;
                ($hrefs[$n] =~ m|src\s*=\s*"?\w+:|i) && next;
                # relative URL from root:
                ($hrefs[$n] =~ s|src\s*=\s*"?/([^">]*)"?|SRC="$root/$1"|i) && next;
                # relative from $path:
                $hrefs[$n] =~ s|src\s*=\s*"?([^/"][^">]*)"?|SRC="$root$path$1"|i;
                # collapse relative paths:
                $hrefs[$n] =~ s|/\./|/|g;
                while ($hrefs[$n] =~ m|/\.\./|) {
                        $hrefs[$n] =~ s|[^/]*/\.\./||;
                }
        }
        join("<IMG",@hrefs);
}

# convert an URL to ($type,host,port,path,request)
# given previous type, host, port and path, will handle relative URLs
# NB: May need special processing for different service types (e.g., news)
sub parse {
        local($type,$host,$port,$path,$url) = @_;
        if ($url =~ m|^(\w+)://(.*)|) {
                $type = $1;
                $host = $2;
                $port = &defport($type);
                $request = "/"; # default
                ($host =~ s|^([^/]+)(/.*)$|$1|) && ($request = $2);
                ($host =~ s/:(\d+)$//) && ($port = $1);
                ($path = $request) =~ s|[^/]*$||;
        }
        else {
                # relative URL of form "<type>:<request>"
                if ($url =~ /^(\w+):(.*)/) {
                        $type = $1;
                        $request = $2;
                }
                # relative URL of form "<request>"
                else { $request = $url; }
                $request =~ s|^$|/|;
                $request =~ s|^([^/])|$path$1|; # relative path
                $request =~ s|/\./|/|g;
                while ($request =~ m|/\.\./|) {
                        $request =~ s|[^/]*/\.\./||;
                }
                # assume previous host & port:
                unless ($host) {
                        # $! = "html'parse: no host for $url\n";
                        print STDERR "html'parse: no host for $url\n";
                        return (undef,undef,undef,undef,undef);
                }
        }
        ($type,$host,$port,$path,$request);
}

# default ports
sub defport {
        local($type) = @_;
        if ($type eq "http") { 80; }
        elsif ($type eq "gopher") { 70; }
        else { undef; }
}

# return a list of all the hrefs in a page
sub hrefs {
        local($page) = @_;
        $page =~ s/^[^<]+</</;
        $page =~ s/>[^<]*</></g;
        $page =~ s/>[^<]+$/>/;
        $page =~ s/<a[^>]*href\s*=\s*"?([^">]+)[^>]*>/$1\n/gi;
        $page =~ s/<img[^>]*src\s*=\s*"?([^">]+)[^>]*>/$1\n/gi;
        $page =~ s/<[^>]*>//g;
        $page =~ s/#.*//g;
        $page =~ s/\n+/\n/g;
        split(/\n/,$page);
}

1;

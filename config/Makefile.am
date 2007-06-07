## needed for "make dist" also
EXTRA_DIST = shore.def
DISTCLEANFILES = Makefile.in shore-config.h
MAINTAINERCLEANFILES = Makefile.in shore-config.h

installdir           = $(top_builddir)/installed
includedir           = $(installdir)/include

all:
	cp shore.def $(includedir)/
	cp shore-config.h $(includedir)/


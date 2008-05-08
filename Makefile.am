## Process this file with automake to produce Makefile.in
##  Trouble with 'make install' makes me put all the installed stuff here:
## and let user copy that wherever he chooses.`
include $(top_srcdir)/Makefile.generic
MAINTAINERCLEANFILES = \
	$(installdir)/include/*.h \
	$(installdir)/lib/*.a \
	$(installdir)/bin/* \
	$(installdir)/doc
SUBDIRS = src doc config
DIST_SUBDIRS = tools $(SUBDIRS)
EXTRA_DIST = bootstrap Makefile.local Makefile.generic COPYRIGHT-NOTICE README ChangeLog

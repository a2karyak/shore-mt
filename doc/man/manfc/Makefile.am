include $(top_srcdir)/Makefile.generic
include $(top_srcdir)/doc/man/Makefile.man.generic
distdir = $(top_distdir)/doc
localpdfdir = $(localdocdir)/man/manfc
CLEANFILES = $(ALL_TARGETS)
EXTRA_DIST = $(DIST_TARGETS)

SOURCES = error.fc intro.fc list.fc rc.fc statistics.fc

HTML_TARGETS = $(SOURCES:fc=html)
DVI_TARGETS = $(SOURCES:fc=dvi)
PDF_TARGETS = $(SOURCES:fc=pdf)

SUFFIXES: .fc 

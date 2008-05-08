include $(top_srcdir)/Makefile.generic
include $(top_srcdir)/doc/man/Makefile.man.generic
distdir = $(top_distdir)/doc
localpdfdir = $(localdocdir)/man/mancommon
CLEANFILES = $(ALL_TARGETS)
EXTRA_DIST = $(DIST_TARGETS)

SOURCES = errlog.common intro.common latch_t.common lid_t.common nbox_t.common options.common rsrc.common vec_t.common
HTML_TARGETS = $(SOURCES:common=html)
DVI_TARGETS = $(SOURCES:common=dvi)
PDF_TARGETS = $(SOURCES:common=pdf)

SUFFIXES: .common 


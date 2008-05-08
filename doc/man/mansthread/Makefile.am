include $(top_srcdir)/Makefile.generic
include $(top_srcdir)/doc/man/Makefile.man.generic
distdir = $(top_distdir)/doc
localpdfdir = $(localdocdir)/man/mansthread
CLEANFILES = $(ALL_TARGETS)
EXTRA_DIST = $(DIST_TARGETS)

SOURCES = errors.sthread file_handlers.sthread intro.sthread scond_t.sthread sevsem_t.sthread smutex_t.sthread sthread_t.sthread

HTML_TARGETS = $(SOURCES:sthread=html)
DVI_TARGETS = $(SOURCES:sthread=dvi)
PDF_TARGETS = $(SOURCES:sthread=pdf)

SUFFIXES: .sthread

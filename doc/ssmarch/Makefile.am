## Process this file with automake to produce Makefile.in
##
include $(top_srcdir)/Makefile.generic
include $(top_srcdir)/doc/Makefile.doc.generic
distdir = $(top_distdir)/doc
localpdfdir = $(localdocdir)/ssmarch
SUBDIRS = 
CLEANFILES = $(ALL_TARGETS) *.dvi *.aux *.toc *.log *.idx
EXTRA_DIST = $(DIST_TARGETS) $(DOC_PDF_TARGETS)
SOURCES = 
#
## For now: use .doc as the source.
## Might eventally use TeX source.
DOC_PDF_TARGETS = ssmarch.pdf

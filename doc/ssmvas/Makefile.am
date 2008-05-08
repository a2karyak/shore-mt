## Process this file with automake to produce Makefile.in
include $(top_srcdir)/Makefile.generic
include $(top_srcdir)/doc/Makefile.doc.generic
distdir = $(top_distdir)/doc
localpdfdir = $(localdocdir)/ssmvas
SUBDIRS = hello
DIST_SUBDIRS = $(SUBDIRS)
CLEANFILES = $(ALL_TARGETS) *.dvi *.aux *.toc *.log *.idx
EXTRA_DIST = $(DIST_TARGETS)

SOURCES = ssmvas.tex

HTML_TARGETS = $(SOURCES:tex=html)
DVI_TARGETS = $(SOURCES:tex=dvi)
PDF_TARGETS = $(SOURCES:tex=pdf)

SUFFIXES: .tex 


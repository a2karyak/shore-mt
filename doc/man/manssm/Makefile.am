include $(top_srcdir)/Makefile.generic
include $(top_srcdir)/doc/man/Makefile.man.generic
distdir = $(top_distdir)/doc
localpdfdir = $(localdocdir)/man/manssm
CLEANFILES = $(ALL_TARGETS)
EXTRA_DIST = $(DIST_TARGETS)

## automake bug in some earlier versions doesn't deal with the continuation character here:

SOURCES = btree.ssm debug.ssm device.ssm enum.ssm errors.ssm file.ssm init.ssm intro.ssm id.ssm lock.ssm newsort.ssm pin_i.ssm rtree.ssm scan_file_i.ssm scan_index_i.ssm scan_rt_i.ssm smthread_t.ssm sort.ssm sort_stream_i.ssm ssm_options.ssm statistics.ssm transaction.ssm volume.ssm

HTML_TARGETS = $(SOURCES:ssm=html)
DVI_TARGETS = $(SOURCES:ssm=dvi)
PDF_TARGETS = $(SOURCES:ssm=pdf)

SUFFIXES: .ssm 

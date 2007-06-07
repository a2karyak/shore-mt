## Process this file with automake to produce Makefile.in
include $(top_srcdir)/Makefile.generic
EXTRA_DIST = testall EXAMPLE_SHORECONFIG

AM_CXXFLAGS          +=  \
		 -I$(top_srcdir)/src/fc \
		 -I$(top_srcdir)/src/sthread \
		 -I$(top_srcdir)/src/common  \
		 -I$(top_srcdir)/src/sm 

check_PROGRAMS     = file_scan$(EXEEXT)

TESTS = testall

file_scan_SOURCES      = file_scan.cpp options.cpp 

LDADD      = \
	$(top_builddir)/src/sm/libsm.a  \
	$(top_builddir)/src/common/libcommon.a  \
	$(top_builddir)/src/sthread/libsthread.a  \
	$(top_builddir)/src/fc/libfc.a  \
	$(LOCALLDADD)

file_scan.pure:  
	$(PURIFY_BIN)/purify $(PURIFY_FLAGS)  -collector=$(PURIFY_COLLECTOR)  $(CXX) -o $@ $(file_scan_SOURCES:.cpp=.o) $(LDADD) $(LIBS)



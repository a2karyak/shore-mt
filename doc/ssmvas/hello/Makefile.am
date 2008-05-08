## Process this file with automake to produce Makefile.in
include $(top_srcdir)/Makefile.generic 
include $(top_srcdir)/doc/Makefile.doc.generic 
SUBDIRS =
EXTRA_DIST = $(hello_SOURCES) $(hello_HEADERS) exampleconfig README hello.sh  hello.makefile.end
hellodir = $(top_srcdir)/doc/ssmvas/hello

CLEANFILES = ./log.hello/* device.hello

INDEPENDENT_MAKEFLAGS   = $(AM_CXXFLAGS)
hello_CXXFLAGS          = $(AM_CXXFLAGS)
hello_CXXFLAGS          += -I$(top_srcdir)/src/fc \
		       -I$(top_srcdir)/src/sthread \
		       -I$(top_srcdir)/src/common \
		       -I$(top_srcdir)/src/sm 

LDADD      = \
	$(top_builddir)/src/sm/libsm.a \
	$(top_builddir)/src/common/libcommon.a  \
	$(top_builddir)/src/sthread/libsthread.a  \
	$(top_builddir)/src/fc/libfc.a  \
	$(TCL_LIB)  $(LOCALLDADD)

hello_SOURCES     = hello.cpp

check_PROGRAMS     = hello$(EXEEXT) 

	
TESTS = hello.sh

instdir = $(localdocdir)/ssmvas/hello

install-data-local:
## make sure the target dir exists
	mkdir -p $(instdir)
## copy over sources
	cp README $(hello_SOURCES) $(hello_HEADERS) $(instdir)
## create the example config and make it use the
## installed diskrw
	sed -e "s=../../../src/sthread=$(bindir)=" exampleconfig > $(instdir)/exampleconfig
## copy the test script
	cp hello.sh $(instdir)
## create the sample makefile that uses the same build args
## as the built and installed ssm
	echo "INCLUDES =-I" $(includedir) > $(instdir)/hello.make
	echo "FLAGS =" $(INDEPENDENT_MAKEFLAGS) >> $(instdir)/hello.make
	echo "libdir =" $(libdir) >> $(instdir)/hello.make
	cat hello.makefile.end >> $(instdir)/hello.make

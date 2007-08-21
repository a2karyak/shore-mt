## Process this file with automake to produce Makefile.in
include $(top_srcdir)/Makefile.generic
EXTRA_DIST = testall
CLEANFILES = diskrw
AM_CXXFLAGS    += -I$(top_srcdir)/src/sthread \
		-I$(top_srcdir)/src/fc

check_PROGRAMS     = thread1$(EXEEXT) thread2$(EXEEXT)\
		     thread3$(EXEEXT) thread4$(EXEEXT) \
		   ioperf$(EXEEXT) diskinfo$(EXEEXT) except$(EXEEXT)

TESTS = testall

LDADD = ../libsthread.a ../../fc/libfc.a $(LOCALLDADD)

thread1_SOURCES      = thread1.cpp

thread2_SOURCES      = thread2.cpp

thread3_SOURCES      = thread3.cpp

thread4_SOURCES      = thread4.cpp

ioperf_SOURCES      = ioperf.cpp

diskinfo_SOURCES      = diskinfo.cpp

except_SOURCES      = except.cpp

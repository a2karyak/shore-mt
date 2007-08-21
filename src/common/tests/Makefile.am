## Process this file with automake to produce Makefile.in
include $(top_srcdir)/Makefile.generic
AM_CXXFLAGS    += -I$(top_srcdir)/src/common \
		  -I$(top_srcdir)/src/fc 

EXTRA_DIST = test_stat.dat testall

check_HEADERS     = test_stat.h

check_PROGRAMS     = bitmap$(EXEEXT)  kvl$(EXEEXT) opaque$(EXEEXT) \
		   stats$(EXEEXT) vectors$(EXEEXT)\
		   errcodes$(EXEEXT) 
TESTS = testall

LDADD = ../libcommon.a ../../sthread/libsthread.a ../../fc/libfc.a $(LOCALLDADD)

bitmap_SOURCES      = bitmap.cpp

errcodes_SOURCES      = errcodes.cpp

kvl_SOURCES      = kvl.cpp

opaque_SOURCES      = opaque.cpp

stats_SOURCES      = test_stat.cpp stats.cpp

vectors_SOURCES      = vectors.cpp


TEST_STAT_GENFILES = test_stat_def_gen.h test_stat_struct_gen.h 

GENFILES = $(TEST_STAT_GENFILES)

test_stat.$(OBJEXT): $(TEST_STAT_GENFILES)
stats.$(OBJEXT):  $(TEST_STAT_GENFILES)

$(TEST_STAT_GENFILES): test_stat.dat
	$(PERL) $(top_srcdir)/tools/stats.pl $<

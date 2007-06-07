## Process this file with automake to produce Makefile.in
include $(top_srcdir)/Makefile.generic

EXTRA_DIST = testall

AM_CXXFLAGS    += -I$(top_srcdir)/src/fc

check_PROGRAMS     = list1$(EXEEXT) list2$(EXEEXT) list3$(EXEEXT) \
	       	fastnew$(EXEEXT) \
		hash1$(EXEEXT) hash2$(EXEEXT) \
	       	cq1$(EXEEXT) cq2$(EXEEXT) \
	       	bit$(EXEEXT) \
	       	timer$(EXEEXT) ntime$(EXEEXT) vector$(EXEEXT) \
	        heap$(EXEEXT) \
	       	rc$(EXEEXT) errlogtest$(EXEEXT) \
	       	random$(EXEEXT) \
	       	stats$(EXEEXT) \
	       	box$(EXEEXT) \
	       	io$(EXEEXT) \
	       	ss$(EXEEXT) \
	       	basic_sizes$(EXEEXT)


TESTS = testall
LDADD = ../libfc.a  $(LOCALLDADD)

basic_sizes_SOURCES      = basic_sizes.cpp

ss_SOURCES      = ss.cpp

list1_SOURCES      = list1.cpp

list2_SOURCES      = list2.cpp

list3_SOURCES      = list3.cpp

fastnew_SOURCES      = fastnew.cpp

hash1_SOURCES      = hash1.cpp

hash2_SOURCES      = hash2.cpp

cq1_SOURCES      = cq1.cpp

cq2_SOURCES      = cq2.cpp

bit_SOURCES      = bit.cpp

timer_SOURCES      = timer.cpp

ntime_SOURCES      = ntime.cpp

vector_SOURCES      = vector.cpp

heap_SOURCES      = heap.cpp

rc_SOURCES      = rc.cpp

errlogtest_SOURCES      = errlogtest.cpp

random_SOURCES      = random.cpp

stats_SOURCES      = stats.cpp

box_SOURCES      = box.cpp

io_SOURCES      = io.cpp

TEST = testall
testall:  $(check_PROGRAMS)



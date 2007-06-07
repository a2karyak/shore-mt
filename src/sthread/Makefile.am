## Process this file with automake to produce Makefile.in
SUBDIRS = tests
include $(top_srcdir)/Makefile.generic
EXTRA_DIST = st_error.dat sthread_stats.dat shmc_stats.dat

installdir           = $(top_builddir)/installed

libdir               = $(installdir)/lib
bindir               = $(installdir)/bin
includedir           = $(installdir)/include

AM_CXXFLAGS          += -I. \
		       -I$(top_srcdir)/config \
		       -I$(top_srcdir)/src/fc

AM_CXXFLAGS += -W -Wall -Wpointer-arith -Wwrite-strings -Werror -fno-inline

lib_LIBRARIES     = libsthread.a
bin_PROGRAMS     =  diskrw
diskrw_LDADD      = libsthread.a ../fc/libfc.a $(LOCALLDADD)


ST_ERROR_GENFILES  = st_errmsg_gen.h\
		     st_einfo_gen.h\
		     st_error_def_gen.h\
		     st_error_enum_gen.h\
		     st_einfo_bakw_gen.h

STHREAD_GENFILES_H  = sthread_stats_collect_enum_gen.h \
	sthread_stats_def_gen.h \
	sthread_stats_msg_gen.h \
	sthread_stats_struct_gen.h

STHREAD_GENFILES_CPP = \
	sthread_stats_collect_gen.cpp \
	sthread_stats_dec_gen.cpp \
	sthread_stats_inc_gen.cpp \
	sthread_stats_out_gen.cpp \
	sthread_stats_stat_gen.cpp

SHMC_GENFILES_H  = shmc_stats_collect_enum_gen.h \
	shmc_stats_def_gen.h \
	shmc_stats_msg_gen.h \
	shmc_stats_struct_gen.h

SHMC_GENFILES_CPP = \
	shmc_stats_collect_gen.cpp \
	shmc_stats_dec_gen.cpp \
	shmc_stats_inc_gen.cpp \
	shmc_stats_out_gen.cpp \
	shmc_stats_stat_gen.cpp

GENFILES_H = $(SHMC_GENFILES_H)  $(STHREAD_GENFILES_H) $(ST_ERROR_GENFILES)

GENFILES_CPP = $(SHMC_GENFILES_CPP)  $(STHREAD_GENFILES_CPP) 

GENFILES = $(GENFILES_H) $(GENFILES_CPP)

include_HEADERS = \
	$(GENFILES_H) \
	diskrw.h diskstats.h \
	os_fcntl.h os_interface.h \
	pure_threads.h ready_q.h \
	scoped_rename.h \
	sdisk.h sdisk_diskrw.h sdisk_unix.h \
	sfile.h sfile_handler.h sfile_handler_poll.h sfile_handler_select.h \
	shmc_stats.h spin.h \
	stcore.h stcore_pthread.h \
	sthread.h sthread_gcc.h sthread_gcc3.h \
	sthread_stats.h sthread_vtable_enum.h strace.h \
	tsl.h w_wsa.h os_memory.h

diskrw_SOURCES   = diskrw.cpp tsl.S

#  pthread_tsl.cpp  only if STHREAD_TSL_PTHREAD in shore.def
# for pthread-based sthreads to work with C++ exception-handling
libsthread_a_SOURCES      = \
	alloc_sthread.cpp diskrw.cpp event.cpp fake_tsl.cpp \
	io.cpp readstats.cpp \
	ready_q.cpp  \
	sdisk.cpp sdisk_diskrw.cpp \
	sdisk_unix.cpp sdisk_unix_sun.cpp \
       	sfile.cpp  \
	sfile_handler.cpp sfile_handler_poll.cpp sfile_handler_select.cpp \
	shmc_stats.cpp \
	shore_threads.cpp \
	stcore.cpp \
	sthread.cpp \
	sthread_core.cpp \
	sthread_core_pthread.cpp \
	sthread_cpu.cpp \
	sthread_stats.cpp \
	strace.cpp \
	vtable_sthread.cpp \
	w_wsa.cpp


# libsthread_a_LIBADD       = @LIBOBJS@

MOSTLYCLEANFILES = $(GENFILES_H) $(GENFILES_CPP)

## sthread.h: st_error_enum_gen.h
## sthread_stats.h: sthread_stats_struct_gen.h

alloc_sthread.$(OBJEXT): $(ST_ERROR_GENFILES)
$(ST_ERROR_GENFILES): st_error.dat
	$(PERL) $(top_srcdir)/tools/errors.pl --d --e $<


event.$(OBJEXT): $(STHREAD_GENFILES_H)
sthread_stats.$(OBJEXT): $(STHREAD_GENFILES_CPP)
## vtable_sthread.$(OBJEXT): $(STHREAD_GENFILES_H)
$(STHREAD_GENFILES_CPP) $(STHREAD_GENFILES_H): sthread_stats.dat
	$(PERL) $(top_srcdir)/tools/stats.pl $<

shmc_stats.$(OBJEXT): $(SHMC_GENFILES_CPP)
diskrw.$(OBJEXT): $(SHMC_GENFILES_H)
$(SHMC_GENFILES_CPP) $(SHMC_GENFILES_H): shmc_stats.dat
	$(PERL) $(top_srcdir)/tools/stats.pl $<


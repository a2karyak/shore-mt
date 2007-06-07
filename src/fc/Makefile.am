## Process this file with automake to produce Makefile.in
SUBDIRS = tests
include $(top_srcdir)/Makefile.generic
EXTRA_DIST = fc_error.dat

installdir           = $(top_builddir)/installed
libdir               = $(installdir)/lib
bindir               = $(installdir)/bin
includedir           = $(installdir)/include

lib_LIBRARIES     = libfc.a

FC_ERROR_GENFILES_H  = fc_errmsg_gen.h \
		     fc_einfo_gen.h \
		     fc_error_def_gen.h \
		     fc_error_enum_gen.h \
		     fc_einfo_bakw_gen.h

GENFILES = $(FC_ERROR_GENFILES_H)

include_HEADERS = \
	cat.h errlog.h errlog_s.h \
	extTraceEvents.h \
	getopt.h nbox.h \
	ncrTraceEvents.h os_types.h \
	solaris_stats.h \
	stime.h tcl_workaround.h \
	unix_error.h unix_stats.h \
	vtable_info.h \
	w.h w_autodel.h w_base.h w_bitmap.h w_bitmap_space.h \
	w_cirqueue.h w_compat_strstream.h w_debug.h w_defines.h \
	w_dynarray.h w_endian.h w_error.h w_factory.h \
	w_factory_fast.h w_factory_thread.h \
	w_factory_vtable_enum.h \
	w_form.h \
	w_gettimeofday.h \
	w_hash.h w_heap.h w_list.h w_minmax.h w_random.h \
	w_rc.h w_rusage.h w_shmem.h w_shore_alloc.h w_signal.h \
	w_sptr.h w_statistics.h \
	w_stream.h w_strstream.h \
	w_timer.h w_uio.h w_vector.h \
	w_workaround.h \
	w_windows.h w_winsock.h \
	largefile_aware.h \
	$(GENFILES)

libfc_a_SOURCES      = \
        unix_error.cpp \
        w_uio.cpp \
        unix_stats.cpp \
        w_compat_strstream.cpp \
        w_form.cpp w_form_base.cpp \
        common_tmpl.cpp debug.cpp errlog.cpp getopt.cpp nbox.cpp \
        stime.cpp vtable_info.cpp w_base.cpp w_bitmap.cpp w_error.cpp \
        w_factory.cpp w_factory_fast.cpp w_factory_thread.cpp \
        w_gettimeofday.cpp w_listm.cpp w_random.cpp w_rc.cpp \
        w_shmem.cpp w_shore_alloc.cpp w_statistics.cpp \
        w_timer.cpp w_input.cpp


MOSTLYCLEANFILES = $(GENFILES)

debug.$(OBJEXT): $(FC_ERROR_GENFILES_H)
$(FC_ERROR_GENFILES_H): fc_error.dat
	$(PERL) $(top_srcdir)/tools/errors.pl -d -e $<


## Process this file with automake to produce Makefile.in
SUBDIRS = smsh tests
include $(top_srcdir)/Makefile.generic
EXTRA_DIST = e_error.dat\
	     smthread_stats.dat sm_stats.dat logdef.dat sm2pc_stats.dat

installdir           = $(top_builddir)/installed
libdir               = $(installdir)/lib
bindir               = $(installdir)/bin
includedir           = $(installdir)/include

AM_CXXFLAGS          += -I$(top_srcdir)/src/fc \
		       -I$(top_srcdir)/src/sthread \
		       -I$(top_srcdir)/src/common 

lib_LIBRARIES     = libsm.a

E_ERROR_GENFILES_H = \
	e_errmsg_gen.h \
	e_einfo_gen.h \
	e_error_def_gen.h \
	e_error_enum_gen.h \
	e_einfo_bakw_gen.h

SM_STATS_GENFILES_H = \
	sm_stats_t_collect_enum_gen.h \
	sm_stats_t_def_gen.h \
	sm_stats_t_msg_gen.h \
	sm_stats_t_struct_gen.h 

SM_STATS_GENFILES_CPP = \
	sm_stats_t_collect_gen.cpp \
	sm_stats_t_dec_gen.cpp \
	sm_stats_t_inc_gen.cpp \
	sm_stats_t_out_gen.cpp \
	sm_stats_t_stat_gen.cpp

SMTHREAD_STATS_GENFILES_H = \
	smthread_stats_t_collect_enum_gen.h \
	smthread_stats_t_def_gen.h \
	smthread_stats_t_msg_gen.h \
	smthread_stats_t_struct_gen.h

SMTHREAD_STATS_GENFILES_CPP =  \
	smthread_stats_t_collect_gen.cpp \
	smthread_stats_t_dec_gen.cpp \
	smthread_stats_t_inc_gen.cpp \
	smthread_stats_t_out_gen.cpp \
	smthread_stats_t_stat_gen.cpp

SM2PC_STATS_GENFILES_H = \
	sm2pc_stats_t_collect_enum_gen.h \
	sm2pc_stats_t_def_gen.h \
	sm2pc_stats_t_msg_gen.h \
	sm2pc_stats_t_struct_gen.h 

SM2PC_STATS_GENFILES_CPP = \
	sm2pc_stats_t_collect_gen.cpp \
	sm2pc_stats_t_dec_gen.cpp \
	sm2pc_stats_t_inc_gen.cpp \
	sm2pc_stats_t_out_gen.cpp \
	sm2pc_stats_t_stat_gen.cpp

LOGDEF_GENFILES_H = \
	logfunc_gen.h \
	logtype_gen.h

LOGDEF_GENFILES_CPP = \
	undo_gen.cpp \
	redo_gen.cpp \
	logdef_gen.cpp \
	logstr_gen.cpp \
	logstub_gen.cpp

GENFILES_CPP = $(SM_STATS_GENFILES_CPP) \
	       $(SMTHREAD_STATS_GENFILES_CPP) \
	       $(SM2PC_STATS_GENFILES_CPP) \
	       $(LOGDEF_GENFILES_CPP)

GENFILES_H = $(E_ERROR_GENFILES_H) \
	   $(SM_STATS_GENFILES_H) \
	   $(SM2PC_STATS_GENFILES_H) \
	   $(SMTHREAD_STATS_GENFILES_H) \
	   $(LOGDEF_GENFILES_H)

 
GENFILES = $(GENFILES_H) $(GENFILES_CPP)

include_HEADERS = \
	$(GENFILES_H) \
	app_support.h \
	bf.h bf_core.h bf_prefetch.h bf_s.h \
	bitmapvector.h \
	btcursor.h btree.h btree_impl.h btree_p.h \
	chkpt.h chkpt_serial.h \
	crash.h \
	device.h dir.h dtid_t.h \
	deadlock_events.h \
	extent.h \
	file.h file_s.h \
	global_deadlock.h \
	histo.h keyed.h lexify.h \
	lgrec.h lid.h lid_s.h \
	lock.h lock_core.h lock_s.h lock_s_inline.h lock_x.h \
	log.h log_buf.h logrec.h \
	mappings.h \
	page.h page_alias.h page_h.h page_s.h \
	pin.h \
	pmap.h \
	prologue.h \
	raw_log.h \
	restart.h restart_s.h \
	rtree.h rtree_p.h \
	scan.h sdesc.h \
	sm.h sm_base.h sm_du_stats.h sm_escalation.h \
	sm_int_0.h sm_int_1.h sm_int_2.h sm_int_3.h sm_int_4.h \
	sm_io.h sm_s.h \
	sm_vtable_enum.h \
	smstats.h \
	smthread.h \
	sort.h sort_s.h \
	srv_log.h \
	sysdefs.h \
	unix_log.h \
	vol.h \
	xct.h xct_dependent.h xct_impl.h \
	zkeyed.h 

libsm_a_SOURCES      =  \
	bf.cpp bf_core.cpp bf_prefetch.cpp \
	bitmapvector.cpp \
	btcursor.cpp btree.cpp btree_bl.cpp btree_impl.cpp btree_p.cpp \
	chkpt.cpp chkpt_serial.cpp \
	common_templates.cpp \
	crash.cpp \
	device.cpp \
	deadlock_events.cpp \
	dir.cpp \
	dtid_t.cpp \
	file.cpp \
	histo.cpp \
	keyed.cpp zkeyed.cpp \
	lexify.cpp \
	lgrec.cpp \
	lid.cpp \
	lock.cpp lock_core.cpp \
	log.cpp log_base.cpp log_buf.cpp logrec.cpp logstub.cpp \
	raw_log.cpp unix_log.cpp srv_log.cpp \
	sort.cpp newsort.cpp \
	page.cpp \
	pin.cpp \
	pmap.cpp \
	restart.cpp \
	rtree.cpp \
	scan.cpp \
	sm.cpp \
	sm_du_stats.cpp \
	sm_io.cpp \
	sm_s.cpp \
	smfile.cpp smindex.cpp \
	smstats.cpp \
	sm2pcstats.cpp \
	smthread.cpp smthreadstats.cpp \
	vol.cpp \
	xct.cpp xct_impl.cpp

MOSTLYCLEANFILES = $(GENFILES) e_error.timestamp smthread_stats.timestamp logdef.timestamp \
	sm2pc_stats.timestamp sm_stats.timestamp

bf.$(OBJEXT): $(E_ERROR_GENFILES_H)
$(E_ERROR_GENFILES_H): e_error.dat
	$(PERL) $(top_srcdir)/tools/errors.pl --d --e $<

bf.$(OBJEXT): $(SMTHREAD_STATS_GENFILES_H)
$(SMTHREAD_STATS_GENFILES_CPP) $(SMTHREAD_STATS_GENFILES_H): smthread_stats.dat
	$(PERL) $(top_srcdir)/tools/stats.pl $<

bf.$(OBJEXT): $(SM_STATS_GENFILES_H)
sm.$(OBJEXT): $(SM_STATS_GENFILES_CPP)
$(SM_STATS_GENFILES_CPP) $(SM_STATS_GENFILES_H): sm_stats.dat
	$(PERL) $(top_srcdir)/tools/stats.pl $<

bf.$(OBJEXT): $(SM2PC_STATS_GENFILES_H)
sm.$(OBJEXT): $(SM2PC_STATS_GENFILES_CPP)
sm2pcstats.$(OBJEXT): $(SM2PC_STATS_GENFILES_CPP)
$(SM2PC_STATS_GENFILES_CPP) $(SM2PC_STATS_GENFILES_H): sm2pc_stats.dat
	$(PERL) $(top_srcdir)/tools/stats.pl $<

bitmapvector.$(OBJEXT): $(LOGDEF_GENFILES_H)
chkpt.$(OBJEXT): $(LOGDEF_GENFILES_CPP)
$(LOGDEF_GENFILES_H) $(LOGDEF_GENFILES_CPP): logdef.dat
	$(PERL) -w $(top_srcdir)/tools/logdef.pl $<


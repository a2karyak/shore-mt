# <std-header style='make' orig-src='shore' no-defines='true'>
#
#  $Id: makefile.boot,v 1.11 2000/01/03 15:25:15 kupsch Exp $
#
# SHORE -- Scalable Heterogeneous Object REpository
#
# Copyright (c) 1994-99 Computer Sciences Department, University of
#                       Wisconsin -- Madison
# All Rights Reserved.
#
# Permission to use, copy, modify and distribute this software and its
# documentation is hereby granted, provided that both the copyright
# notice and this permission notice appear in all copies of the
# software, derivative works or modified versions, and any portions
# thereof, and that both notices appear in supporting documentation.
#
# THE AUTHORS AND THE COMPUTER SCIENCES DEPARTMENT OF THE UNIVERSITY
# OF WISCONSIN - MADISON ALLOW FREE USE OF THIS SOFTWARE IN ITS
# "AS IS" CONDITION, AND THEY DISCLAIM ANY LIABILITY OF ANY KIND
# FOR ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
#
# This software was developed with support by the Advanced Research
# Project Agency, ARPA order number 018 (formerly 8230), monitored by
# the U.S. Army Research Laboratory under contract DAAB07-91-C-Q518.
# Further funding for this work was provided by DARPA through
# Rome Research Laboratory Contract No. F30602-97-2-0247.
#
#   -- do not edit anything above this line --   </std-header>

# $XConsortium: Makefile.ini /main/24 1996/09/28 16:14:20 rws $
#  -------------------------------------------------------
#              see Xosdefs.h for copyright notice        
#  -------------------------------------------------------
#
#   WARNING    WARNING    WARNING    WARNING    WARNING    WARNING    WARNING
#
# This is NOT an automatically generated Makefile!  It is hand-crafted as a 
# bootstrap, may need editing for your system.  The BOOTSTRAPCFLAGS variable
# may be given at the top of the build tree for systems that do not define
# any machine-specific preprocessor symbols.
#
#
# usage: make -f Makefile.ini 


ifndef TARGET_BUILD_DIR
TARGET_BUILD_DIR = $(BUILD_DIR)
endif

ifndef WINDOWS
WINDOWS = 0
endif

ifndef IMAKE_DEFINES
IMAKE_DEFINES =
endif

ifdef USE_THIS_FOR_CPP
ifdef STRINGIZE_USE_THIS_FOR_CPP
IMAKE_DEFINES += -DUSE_THIS_FOR_CPP='$(USE_THIS_FOR_CPP)' -DSTRINGIZE_USE_THIS_FOR_CPP
else
IMAKE_DEFINES += -DUSE_THIS_FOR_CPP='"$(USE_THIS_FOR_CPP)"'
endif
endif

ifdef STRINGIZE_USE_THIS_FOR_CPP
IMAKE_DEFINES += -DSTRINGIZE_USE_THIS_FOR_CPP
endif

ifndef COMPILE_C_TO_EXEC
ifeq ($(WINDOWS),1)
COMPILE_C_TO_EXEC = cl -nologo -D__STDC__ -D_WIN32 -DWIN32 -I. -Ox IMAKE_DEFINES -FeEXEC_FILE SOURCE_FILE
else
COMPILE_C_TO_EXEC = gcc -O IMAKE_DEFINES -o EXEC_FILE SOURCE_FILE
endif
endif

IMAKE_EXEC = $(BUILD_DIR)/$(IMAKE_EXEC_NAME)
TARGET_IMAKE_EXEC = $(TARGET_BUILD_DIR)/$(IMAKE_EXEC_NAME)

$(TARGET_IMAKE_EXEC): imake.c imakemdep.h Xosdefs.h Xw32defs.h
	$(subst EXEC_FILE,$(IMAKE_EXEC),$(subst SOURCE_FILE,$<,$(subst IMAKE_DEFINES,$(IMAKE_DEFINES),$(COMPILE_C_TO_EXEC))))

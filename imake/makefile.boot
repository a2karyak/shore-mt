# <std-header style='make' orig-src='shore' no-defines='true'>
#
#  $Id: makefile.boot,v 1.10 1999/06/07 20:32:57 kupsch Exp $
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

ifeq ($(WINDOWS),1)
	BOOTSTRAPCFLAGS =  -nologo -D__STDC__  -D_WIN32 -DWIN32
	CC = cl
	INCLUDES = -I. 
	OPTIMIZE  = -Ox
	OBJEXT = obj
	EXEC = .exe
	RM = rm -f
	MV = mv
	MAKE = make
	LIBS = libc.lib kernel32.lib
	OBJ_OUTPUT=  -Fo
	EXE_OUTPUT=  -Fe
#	SHELL=tcsh
else
	BOOTSTRAPCFLAGS =  
	CC = gcc
	INCLUDES = -I. 
	OPTIMIZE  = -O
	OBJEXT = o
	EXEC = 
	RM = rm -f
	MV = mv
	MAKE = make
	LIBS = 
	OBJ_OUTPUT = -o 
	EXE_OUTPUT = -o 
# 	SHELL = /bin/sh
endif

CDEBUGFLAGS = $(OPTIMIZE) 

CFLAGS = $(BOOTSTRAPCFLAGS) $(CDEBUGFLAGS) $(INCLUDES)

IMAKE_EXEC = $(BUILD_DIR)/imake$(EXEC)
IMAKE_OBJ = $(BUILD_DIR)/imake.$(OBJEXT)
TARGET_IMAKE_EXEC = $(TARGET_BUILD_DIR)/imake$(EXEC)
TARGET_IMAKE_OBJ = $(TARGET_BUILD_DIR)/imake.$(OBJEXT)

print_vars::
	@echo making $(IMAKE_EXEC) with BOOTSTRAPCFLAGS=$(BOOTSTRAPCFLAGS)
	@echo and IMAKE_OBJ=$(IMAKE_OBJ)
	@echo and CC=$(CC)
	@echo and OUTPUT=$(OUTPUT)
	@echo and INCLUDES=$(INCLUDESCC)
	@echo and OPTIMIZE=$(OPTIMIZE)
	@echo and EXEC=$(EXEC)
	@echo and RM=$(RM)
	@echo and MV=$(MV)
	@echo and MAKE=$(MAKE)
	@echo and LIBS=$(LIBS)
	@echo and SHELL=$(SHELL)

$(TARGET_IMAKE_EXEC): $(TARGET_IMAKE_OBJ)
	$(CC) $(EXE_OUTPUT)$(IMAKE_EXEC) $(CFLAGS) $(IMAKE_OBJ) $(LIBS)

$(TARGET_IMAKE_OBJ): imake.c
	$(CC) -c $(CFLAGS) $(OBJ_OUTPUT)$(IMAKE_OBJ) imake.c

clean:
	$(RM) $(IMAKE_EXEC) $(IMAKE_OBJ)

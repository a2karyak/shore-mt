# <std-header style='make' orig-src='shore'>
#
#  $Id: GNUmakefile,v 1.27 1999/06/07 18:56:44 kupsch Exp $
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

# Force --print-directories unless --silent
# --print-directories is not the default in cygwin
MAKEFLAGS := -r --warn-undefined-variable $(MAKEFLAGS)
ifneq 's' '$(findstring s, $(filter-out --%, $(MAKEFLAGS)))'
MAKEFLAGS := -w $(MAKEFLAGS)
endif

TOP = .
CONFIG_DIR = config

default:

ifdef DIR_SCHEME_FILE
ifneq (,$(wildcard $(DIR_SCHEME_FILE)))
include $(DIR_SCHEME_FILE)
$(DIR_SCHEME_FILE): ;
else
include $(CONFIG_DIR)/$(DIR_SCHEME_FILE)
$(CONFIG_DIR)/$(DIR_SCHEME_FILE): ;
endif
ifeq (,$(findstring USE_DIR_SCHEME_FILE, $(CONFIG_OPTIONS)))
CONFIG_OPTIONS += USE_DIR_SCHEME_FILE=\"$(DIR_SCHEME_FILE)\"
endif
export PERL_DIR
endif

.PHONY: default

ifndef BOOTSTRAPPED1
BOOTSTRAPPED1 = 0
endif
ifndef BOOTSTRAPPED2
BOOTSTRAPPED2 = 0
endif

ifeq ($(BOOTSTRAPPED1),0)
#
# add targets here which need to set the DIR_SCHEME_FILE
#

default:
	$(MAKE) BOOTSTRAPPED1=1 $@

in_clearcase_%:
	$(MAKE) BOOTSTRAPPED1=1 DIR_SCHEME_FILE=nt_standard.def $(patsubst in_clearcase_%, %, $@)

for_scm_%:
	$(MAKE) BOOTSTRAPPED1=0 FOR_SCM=1 $(patsubst for_scm_%, in_clearcase_%, $@)

%:
	$(MAKE) BOOTSTRAPPED1=1 $@

else
ifeq ($(BOOTSTRAPPED2),0)
#
# this is section is here, so that if there is a DIR_SCHEME_FILE
# defined, PERL_DIR is set before it is used.  otherwise make
# read the whole file and tries to process conditional statement
# which use perl, and then does the includes which define PERL_DIR,
# and then reprocesses everything again.
#

export BOOTSTRAPPED2 = 1

default:
	$(MAKE) $@

%:
	$(MAKE) $@

else
#
# all bootstrapping is done
#

ifndef PERL_DIR
PERL = perl
else
PERL = $(PERL_DIR)/bin/perl
endif


PLATFORM := $(strip $(shell $(PERL) -x tools/platform.pl -q))
DEFAULT_BUILD_DIR = $(PLATFORM)

# comment out next line to build in PLATFORM dir by default
DEFAULT_BUILD_DIR = .

ifndef BUILD_DIR
BUILD_DIR = $(DEFAULT_BUILD_DIR)
endif
ifeq ($(strip $(BUILD_DIR)),)
BUILD_DIR = .
endif

TOOLS_DIR = tools

EDIT_CONFIG = $(PERL) $(TOOLS_DIR)/edit_config.pl
GEN_SRCDIRINFO = $(PERL) $(TOOLS_DIR)/GenSrcDirInfo.pl

CONFIG_BUILD_DIR = $(BUILD_DIR)/$(CONFIG_DIR)
SHORE_DEF = $(CONFIG_BUILD_DIR)/shore.def
SHORE_DEF_EXAMPLE = $(CONFIG_DIR)/shore.def.example
SRCDIRINFO_TMPL = $(CONFIG_BUILD_DIR)/SrcDirInfo.tmpl
SRCDIRINFO_DAT = $(CONFIG_DIR)/SrcDirInfo.dat

IMAKE_DIR = imake
IMAKE_BUILD_DIR = $(BUILD_DIR)/$(IMAKE_DIR)
ifeq (,$(findstring nt-,$(PLATFORM)))
IMAKE = $(IMAKE_BUILD_DIR)/imake
IMAKE_OBJ = $(IMAKE_BUILD_DIR)/imake.o
else
IMAKE = $(IMAKE_BUILD_DIR)/imake.exe
IMAKE_OBJ = $(IMAKE_BUILD_DIR)/imake.obj
endif
MAKEFILE = $(BUILD_DIR)/Makefile
IMAKE_BOOTSTRAP = $(PERL) $(IMAKE_DIR)/bootstrap.pl

MAKEMAKE = $(PERL) $(TOOLS_DIR)/makemake.pl

MAKE_FORWARD = $(MAKE) -C $(BUILD_DIR) -f Makefile

DEFAULT_CONFIG_OPTIONS = DEBUGCODE=OFF

PLAT_CONFIG_OPTIONS =
ifneq (,$(findstring nt-,$(PLATFORM)))
PLAT_CONFIG_OPTIONS += INSTRUMENT_MEM_ALLOC
endif

.DELETE_ON_ERROR:


### default rule

default: $(MAKEFILE)
	$(MAKE_FORWARD)


### rules to bootstrap generation of Makefiles

$(MAKEFILE): $(IMAKE) $(SHORE_DEF) $(SRCDIRINFO_TMPL)
	$(MAKEMAKE) --topDir=. --buildTopDir=$(BUILD_DIR) --curPath=.

$(IMAKE):
	$(IMAKE_BOOTSTRAP) --build_dir=$(BUILD_DIR) --imake_dir=$(IMAKE_DIR) --createDirs

$(SHORE_DEF):
	$(EDIT_CONFIG) --input=$(SHORE_DEF_EXAMPLE) --output=$(SHORE_DEF) --createDirs $(DEFAULT_CONFIG_OPTIONS) $(PLAT_CONFIG_OPTIONS) $(CONFIG_OPTIONS)

$(SRCDIRINFO_TMPL):
	$(GEN_SRCDIRINFO) --input=$(SRCDIRINFO_DAT) --output=$(SRCDIRINFO_TMPL) --createDirs

pristine:: $(MAKEFILE)
	$(MAKE_FORWARD) pristine
	rm -f $(IMAKE) $(IMAKE_OBJ) $(SHORE_DEF) $(SRCDIRINFO_TMPL)


### forward unknown targets to $(BUILD_DIR)/Makefile

DUMMY_CMD_TO_FORCE_FORWARDING = dummy_cmd_to_force_forwarding

%: $(MAKEFILE) $(DUMMY_CMD_TO_FORCE_FORWARDING)
	$(MAKE_FORWARD) $@

.PHONY: $(DUMMY_CMD_TO_FORCE_FORWARDING)

$(DUMMY_CMD_TO_FORCE_FORWARDING):  ;


### rule to build files needed by visual studio, forces BUILD_DIR to . for now

.PHONY: for_vstudio

for_vstudio: automatic_for_vstudio
%_for_vstudio:
	$(MAKE) BUILD_DIR=. $(patsubst %_for_vstudio, %, $@)

### special targets to set things in shore.def easily

.PHONY: for_release build_smlayer_only build_shore_only demo_version

for_release:
	$(MAKE) CONFIG_OPTIONS='$(CONFIG_OPTIONS) OPTIMIZE=ON SM_PAGESIZE=32768' 

build_smlayer_only:
	$(MAKE) CONFIG_OPTIONS='$(CONFIG_OPTIONS) BUILD_SMLAYER_ONLY'

build_shore_only:
	$(MAKE) CONFIG_OPTIONS='$(CONFIG_OPTIONS) BUILD_SMLAYER_ONLY undef-USE_COORD undef-USE_OCOMM'

demo_version:
	$(MAKE) CONFIG_OPTIONS='$(CONFIG_OPTIONS) PARA_MAX_PARATHREADS=5' for_release

### these targets build without dependencies being generated

.PHONY: fast

fast: default_fast
%_fast:
	$(MAKE) INCLUDE_DEPEND=no $(patsubst %_fast, %, $@)

endif
endif


### don't do anything to make GNUmakefile. prevent doing a MAKE_FORWARD.

GNUmakefile: ;

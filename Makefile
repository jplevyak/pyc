# Makefile for PYC

all: defaulttarget

USE_PLIB=1
MODULE=pyc
DEBUG=1
#OPTIMIZE=1
#PROFILE=1
USE_GC=1  # required
#LEAK_DETECT=1
#VALGRIND=1
#USE_LLVM=1  # incomplete
#USE_SS=1  # incomplete

MAJOR=0
MINOR=0

CXX ?= clang++
AR ?= llvm-ar
PREFIX ?= /usr/local

.PHONY: all install $(IFALIB)

OS_TYPE = $(shell uname -s | \
  awk '{ split($$1,a,"_"); printf("%s", a[1]);  }')
OS_VERSION = $(shell uname -r | \
  awk '{ split($$1,a,"."); sub("V","",a[1]); \
  printf("%d%d%d",a[1],a[2],a[3]); }')
ARCH = $(shell uname -m)
ifeq ($(ARCH),i386)
  ARCH = x86
endif
ifeq ($(ARCH),i486)
  ARCH = x86
endif
ifeq ($(ARCH),i586)
  ARCH = x86
endif
ifeq ($(ARCH),i686)
  ARCH = x86
endif

ifeq ($(OS_TYPE),Darwin)
  AR_FLAGS = crvs
else
  AR_FLAGS = crv
endif

ifeq ($(OS_TYPE),CYGWIN)
else
ifeq ($(OS_TYPE),Darwin)
GC_CFLAGS += -I/usr/local/include
else
GC_CFLAGS += -I/usr/local/include
LIBS += -lrt -lpthread
endif
endif

ifdef USE_GC
CFLAGS += -DUSE_GC ${GC_CFLAGS}
endif

CFLAGS += -Wall -std=c++23
ifdef DEBUG
CFLAGS += -g -DDEBUG=1
endif
ifdef OPTIMIZE
CFLAGS += -O3 -march=native
endif
ifdef PROFILE
CFLAGS += -pg
endif
ifdef VALGRIND
CFLAGS += -DVALGRIND_TEST
endif

CPPFLAGS += $(CFLAGS)
LIBS += -lm

BUILD_VERSION = $(shell git show-ref 2> /dev/null | head -1 | cut -d ' ' -f 1)
ifeq ($(BUILD_VERSION),)
  BUILD_VERSION = $(shell cat BUILD_VERSION)
endif
VERSIONCFLAGS += -DMAJOR_VERSION=$(MAJOR) -DMINOR_VERSION=$(MINOR) -DBUILD_VERSION=\"$(BUILD_VERSION)\"

IFA_DIR=ifa
PLIB_DIR=$(IFA_DIR)/common

CFLAGS += -std=c++23 -D__PYC__=1 -I$(PLIB_DIR) -I$(IFA_DIR) -I$(IFA_DIR)/if1 -I$(IFA_DIR)/frontend -I$(IFA_DIR)/analysis -I$(IFA_DIR)/codegen -I$(IFA_DIR)/optimize
ifdef USE_SS
CFLAGS += -Ilib -Ilib/os
endif
LIBS += -lpcre
ifdef USE_LLVM
LLVM_INCLUDE_DIR=$(shell llvm-config --includedir)
CFLAGS += -I$(LLVM_INCLUDE_DIR) -fno-exceptions -funwind-tables -D_GNU_SOURCE -D__STDC_CONSTANT_MACROS -D__STDC_FORMAT_MACROS -D__STDC_LIMIT_MACROS
LLVM_LIBDIR=$(shell llvm-config --libdir)
LLVM_LIBS=$(shell llvm-config --libs)
LIBS += -L$(LLVM_LIBDIR) $(LLVM_LIBS)
CFLAGS += -DUSE_LLVM=1
endif
ifdef USE_GC
IFA_LIB_DIR=ifa
LIBS += -L$(IFA_LIB_DIR) -lifa_gc -lgc -lgccpp -ldparse_gc
IFALIB = $(IFA_LIB_DIR)/libifa_gc.a
else
LIBS += -L$(IFA_LIB_DIR) -lifa -L../plib -lplib -ldparse
IFALIB = $(IFA_LIB_DIR)/libifa.a
endif

ifdef USE_SS
CFLAGS += -DSSLIB="../shedskin/shedskin/lib"
endif

CFLAGS += -Wno-deprecated-register

AUX_FILES = $(MODULE)/index.html $(MODULE)/manual.html $(MODULE)/faq.html $(MODULE)/pyc.1 $(MODULE)/pyc.cat

PYC_DEPEND_SRCS = pyc.cc python_ifa_util.cc python_ifa_sym.cc python_ifa_build_syms.cc python_ifa_build_if1.cc python_ifa_main.cc python_parse.cc version.cc
PYC_SRCS = $(PYC_DEPEND_SRCS) gnuc.g.d_parser.cc python.g.d_parser.cc
ifdef USE_SS
PYC_SRCS += shedskin.cc
endif
ifdef USE_LLVM
PYC_SRCS += llvm.cc
endif
PYC_OBJS = $(PYC_SRCS:%.cc=%.o)

EXECUTABLE_FILES = pyc
INSTALL_LIBRARIES =
#INCLUDES =
MANPAGES = pyc.1

CLEAN_FILES += *.cat

ifeq ($(OS_TYPE),CYGWIN)
EXECUTABLES = $(EXECUTABLE_FILES:%=%.exe)
PYC = pyc.exe
else
EXECUTABLES = $(EXECUTABLE_FILES)
PYC = pyc
endif

DEPEND_SRCS = $(PYC_DEPEND_SRCS)

defaulttarget: $(EXECUTABLES) pyc.cat

install:
	cp $(EXECUTABLES) $(PREFIX)/bin
	cp $(MANPAGES) $(PREFIX)/man/man1
#	cp $(INCLUDES) $(PREFIX)/include
#	cp $(INSTALL_LIBRARIES) $(PREFIX)/lib

deinstall:
	rm $(EXECUTABLES:%=$(PREFIX)/bin/%)
	rm $(MANPAGES:%=$(PREFIX)/man/man1/%)
#	rm $(INCLUDES:%=$(PREFIX)/include/%)
#	rm $(INSTALL_LIBRARIES:%=$(PREFIX)/lib/%)

%.g.d_parser.cc: %.g
	make_dparser -v -Xcc -I $<

$(IFALIB):
	$(MAKE) -C $(IFA_DIR)

$(PYC): $(PYC_OBJS) $(IFALIB)
	$(CXX) $(CFLAGS) $(LDFLAGS) -o $@ $^ $(LIBS)

pyc.cat: pyc.1
	rm -f pyc.cat
	nroff -man pyc.1 | sed -e 's/.//g' > pyc.cat

LICENSE.i: LICENSE
	rm -f LICENSE.i
	cat $< | sed s/\"/\\\\\"/g | sed s/\^/\"/g | sed s/$$/\\\\n\"/g | sed 's/%/%%/g' > $@

COPYRIGHT.i: LICENSE
	rm -f COPYRIGHT.i
	head -1 LICENSE | sed s/\"/\\\\\"/g | sed s/\^/\"/g | sed s/$$/\\\\n\"/g > $@

pyc.o: LICENSE.i COPYRIGHT.i

version.o: version.cc Makefile
	$(CXX) $(CFLAGS) $(VERSIONCFLAGS) -c version.cc

test: test_pyc
	./test_pyc

test_dparse: $(PYC)
	@echo "--- DParser parse validation ---"; \
	failed=0; \
	for f in tests/*.py; do \
	  if ./$(PYC) --dparse_only "$$f" 2>/dev/null; then \
	    echo "$$f OK"; \
	  else \
	    ./$(PYC) --dparse_only "$$f" 2>&1 | head -3; \
	    echo "$$f FAILED"; \
	    failed=$$((failed + 1)); \
	  fi; \
	done; \
	if [ "$$failed" -eq 0 ]; then \
	  echo "--- ALL DParser tests PASSED ---"; \
	else \
	  echo "--- $$failed DParser test(s) FAILED ---"; exit 1; \
	fi

clean:
	\rm -f *.o core *.core *.gmon $(EXECUTABLES) LICENSE.i COPYRIGHT.i $(CLEAN_FILES)
	$(MAKE) -C $(IFA_DIR) clean

realclean: clean
	\rm -f *.a *.orig *.rej
	$(MAKE) -C $(IFA_DIR) realclean

depend:
	./mkdep $(CFLAGS) $(DEPEND_SRCS)

-include .depend
# DO NOT DELETE THIS LINE -- mkdep uses it.
# DO NOT PUT ANYTHING AFTER THIS LINE, IT WILL GO AWAY.

# IF YOU PUT ANYTHING HERE IT WILL GO AWAY

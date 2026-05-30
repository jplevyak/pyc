# Makefile for PYC

# Build configuration ---------------------------------------------------------
#
# USE_GC is required — the runtime and IFA library both assume Boehm GC.
# The other flags are optional; uncomment to enable.

USE_GC=1          # required
DEBUG=1
#OPTIMIZE=1
#PROFILE=1
#LEAK_DETECT=1
#VALGRIND=1
#USE_LLVM=1       # LLVM backend (work in progress; see ifa/CODEGEN_LLVM.md)
#USE_SS=1         # Shedskin backend (vestigial)

MAJOR=0
MINOR=0

CXX    ?= clang++
AR     ?= llvm-ar
PREFIX ?= /usr/local

# Platform detection ----------------------------------------------------------

OS_TYPE = $(shell uname -s | awk '{ split($$1,a,"_"); printf("%s", a[1]); }')

ifeq ($(OS_TYPE),Darwin)
  AR_FLAGS = crvs
else
  AR_FLAGS = crv
endif

# Compiler flags --------------------------------------------------------------

CFLAGS += -std=c++23 -Wall -MMD -MP -D__PYC__=1

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

# The implicit `%.o: %.cc` rule uses CPPFLAGS+CXXFLAGS, not CFLAGS.
# Bridge CFLAGS into CPPFLAGS so the implicit rule sees them.
CPPFLAGS += $(CFLAGS)

LIBS += -lm -lpcre

ifneq ($(OS_TYPE),Darwin)
ifneq ($(OS_TYPE),CYGWIN)
  LIBS += -lrt -lpthread
endif
endif

# IFA library + plib headers --------------------------------------------------

IFA_DIR      = ifa
IFA_LIB_DIR  = $(IFA_DIR)
PLIB_DIR     = $(IFA_DIR)/common

CFLAGS += -I$(PLIB_DIR) -I$(IFA_DIR) -I$(IFA_DIR)/if1 -I$(IFA_DIR)/frontend \
          -I$(IFA_DIR)/analysis -I$(IFA_DIR)/codegen -I$(IFA_DIR)/optimize \
          -I/usr/local/include

ifdef USE_GC
  CFLAGS += -DUSE_GC
  LIBS   += -L$(IFA_LIB_DIR) -lifa_gc -lgc -lgccpp -ldparse_gc
  IFALIB  = $(IFA_LIB_DIR)/libifa_gc.a
else
  LIBS   += -L$(IFA_LIB_DIR) -lifa -L../plib -lplib -ldparse
  IFALIB  = $(IFA_LIB_DIR)/libifa.a
endif

# Optional backends -----------------------------------------------------------

ifdef USE_LLVM
  LLVM_INCLUDE_DIR = $(shell llvm-config --includedir)
  LLVM_LIBDIR      = $(shell llvm-config --libdir)
  LLVM_LIBS        = $(shell llvm-config --libs)
  CFLAGS += -I$(LLVM_INCLUDE_DIR) -DUSE_LLVM=1 -fno-exceptions -funwind-tables \
            -D_GNU_SOURCE -D__STDC_CONSTANT_MACROS -D__STDC_FORMAT_MACROS \
            -D__STDC_LIMIT_MACROS
  LIBS   += -L$(LLVM_LIBDIR) $(LLVM_LIBS)
endif

ifdef USE_SS
  CFLAGS += -Ilib -Ilib/os -DSSLIB="../shedskin/shedskin/lib"
endif

# Version stamp ---------------------------------------------------------------

BUILD_VERSION = $(shell git show-ref 2> /dev/null | head -1 | cut -d ' ' -f 1)
ifeq ($(BUILD_VERSION),)
  BUILD_VERSION = $(shell cat BUILD_VERSION)
endif
VERSIONCFLAGS = -DMAJOR_VERSION=$(MAJOR) -DMINOR_VERSION=$(MINOR) \
                -DBUILD_VERSION=\"$(BUILD_VERSION)\"

# Sources / objects -----------------------------------------------------------

PYC_DEPEND_SRCS = pyc.cc python_ifa_util.cc python_ifa_sym.cc \
                  python_ifa_build_syms.cc python_ifa_build_if1.cc \
                  python_ifa_main.cc python_parse.cc version.cc
PYC_SRCS = $(PYC_DEPEND_SRCS) gnuc.g.d_parser.cc python.g.d_parser.cc
ifdef USE_SS
  PYC_SRCS += shedskin.cc
endif
ifdef USE_LLVM
  PYC_SRCS += llvm.cc
endif
PYC_OBJS = $(PYC_SRCS:%.cc=%.o)

# Output / install ------------------------------------------------------------

ifeq ($(OS_TYPE),CYGWIN)
  EXECUTABLES = pyc.exe
  PYC         = pyc.exe
else
  EXECUTABLES = pyc
  PYC         = pyc
endif

MANPAGES    = pyc.1
CLEAN_FILES = *.cat $(PYC_OBJS:.o=.d)

# Targets ---------------------------------------------------------------------

.PHONY: all defaulttarget install deinstall clean realclean \
        test test_dparse $(IFALIB) pullifa pushifa diffifa

all: defaulttarget

defaulttarget: $(EXECUTABLES) pyc.cat

$(PYC): $(PYC_OBJS) $(IFALIB)
	$(CXX) $(CFLAGS) $(LDFLAGS) -o $@ $^ $(LIBS)

# Per-target override (uses CPPFLAGS because that's what the implicit rule
# sees); keeps -MMD -MP dependency generation active.
version.o: CPPFLAGS += $(VERSIONCFLAGS)
version.o: Makefile

# pyc.cc embeds these via #include.
pyc.o: LICENSE.i COPYRIGHT.i

# Generated parsers.
%.g.d_parser.cc: %.g
	make_dparser -v -Xcc -I $<

# IFA library is built by its own makefile; always recurse.
$(IFALIB):
	$(MAKE) -C $(IFA_DIR)

# Manpage as cat-page (strip backspace overstrike from nroff output).
pyc.cat: pyc.1
	rm -f pyc.cat
	nroff -man pyc.1 | sed -e 's/.//g' > pyc.cat

# Generated headers from LICENSE.
LICENSE.i: LICENSE
	rm -f LICENSE.i
	sed -e 's/"/\\"/g' -e 's/^/"/' -e 's/$$/\\n"/' -e 's/%/%%/g' $< > $@

COPYRIGHT.i: LICENSE
	rm -f COPYRIGHT.i
	head -1 LICENSE | sed -e 's/"/\\"/g' -e 's/^/"/' -e 's/$$/\\n"/' > $@

install: $(EXECUTABLES)
	cp $(EXECUTABLES) $(PREFIX)/bin
	cp $(MANPAGES) $(PREFIX)/man/man1

deinstall:
	rm -f $(EXECUTABLES:%=$(PREFIX)/bin/%)
	rm -f $(MANPAGES:%=$(PREFIX)/man/man1/%)

# Tests -----------------------------------------------------------------------

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

# IFA subtree management ------------------------------------------------------

pullifa:
	git subtree pull --prefix=ifa ifa-remote main --squash

pushifa:
	./pushifa.sh

diffifa:
	git diff ifa-remote/main HEAD:ifa

# Clean -----------------------------------------------------------------------

clean:
	rm -f *.o *.d core *.core *.gmon $(EXECUTABLES) LICENSE.i COPYRIGHT.i $(CLEAN_FILES)
	$(MAKE) -C $(IFA_DIR) clean

realclean: clean
	rm -f *.a *.orig *.rej
	$(MAKE) -C $(IFA_DIR) realclean

-include $(PYC_OBJS:.o=.d)

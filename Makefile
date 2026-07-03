# Makefile for PYC

# Build configuration ---------------------------------------------------------
#
# Boehm GC is a hard dependency of the runtime and IFA library; the
# build no longer carries a non-GC branch. The flags below are
# optional; uncomment to enable.

DEBUG=1
#OPTIMIZE=1
#PROFILE=1
#LEAK_DETECT=1
#VALGRIND=1
#USE_LLVM=1       # LLVM backend; uncomment, or invoke as `make USE_LLVM=1`, to
                  # compile the LLVM code path into pyc. The C backend is the
                  # default + production path. `pyc -b` (PYC_LLVM=1, or
                  # `PYC_FLAGS=-b ./test_pyc`) selects LLVM at runtime once
                  # USE_LLVM=1 is built. The LLVM backend doesn't yet pass
                  # most pyc tests — see CODEGEN_PLAN §3.5 and CODEGEN_LLVM.md.
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

CFLAGS += -std=c++23 -Wall -MMD -MP -D__PYC__=1 -Wno-register

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
# "plib" lives vendored under ifa/common/; the top-level /plib/
# directory that older configurations linked against is gone.
PLIB_DIR     = $(IFA_DIR)/common

CFLAGS += -I$(PLIB_DIR) -I$(IFA_DIR) -I$(IFA_DIR)/if1 -I$(IFA_DIR)/frontend \
          -I$(IFA_DIR)/analysis -I$(IFA_DIR)/codegen -I$(IFA_DIR)/optimize \
          -I/usr/local/include -I/opt/homebrew/include

CFLAGS += -I./Python-2.7.18/Include -I./Python-2.7.18/PC

CFLAGS += -DUSE_GC
LIBS   += -L/opt/homebrew/lib -L/usr/local/lib -L$(IFA_LIB_DIR) -lifa_gc -lgc -lgccpp -ldparse_gc
IFALIB  = $(IFA_LIB_DIR)/libifa_gc.a

# Optional backends -----------------------------------------------------------

USE_LLVM = 1
ifdef USE_LLVM
  LLVM_CONFIG ?= $(shell command -v llvm-config 2>/dev/null || echo /opt/homebrew/opt/llvm/bin/llvm-config)
  LLVM_INCLUDE_DIR = $(shell $(LLVM_CONFIG) --includedir)
  LLVM_LIBDIR      = $(shell $(LLVM_CONFIG) --libdir)
  LLVM_LIBS        = $(shell $(LLVM_CONFIG) --libs)
  CFLAGS  += -I$(LLVM_INCLUDE_DIR) -DUSE_LLVM=1 -fno-exceptions -funwind-tables \
             -D_GNU_SOURCE -D__STDC_CONSTANT_MACROS -D__STDC_FORMAT_MACROS \
             -D__STDC_LIMIT_MACROS
  LIBS    += -L$(LLVM_LIBDIR) $(LLVM_LIBS)
  # JIT path: pyc_runtime.o must be in the pyc binary so that
  # DynamicLibrarySearchGenerator::GetForCurrentProcess finds _CG_string_alloc
  # and friends.  -rdynamic exports the main binary's symbols to dlsym/dlopen.
  LDFLAGS += -rdynamic
  JIT_RUNTIME_OBJ = pyc_runtime.o
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

PYC_DEPEND_SRCS = pyc.cc repl.cc python_ifa_util.cc python_ifa_sym.cc \
                  python_ifa_build_syms.cc python_ifa_build_if1.cc \
                  python_ifa_main.cc python_parse.cc version.cc
PYC_SRCS = $(PYC_DEPEND_SRCS) gnuc.g.d_parser.cc python.g.d_parser.cc
ifdef USE_SS
  PYC_SRCS += shedskin.cc
endif
# Note: under USE_LLVM, the LLVM codegen sources (codegen/llvm.cc,
# codegen/llvm_codegen.cc, codegen/llvm_primitives.cc) are already
# compiled into ifa/libifa_gc.a — no top-level source to add. The
# ifdef on the link side (above) still applies for the LLVM libs.
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
CLEAN_FILES = *.cat $(PYC_OBJS:.o=.d) pyc_runtime.o pyc_runtime.d libpyc_runtime.a

# Targets ---------------------------------------------------------------------

.PHONY: all defaulttarget install deinstall clean realclean clean-tests \
        test test-e2e test-unit test-ir test-dparse test_dparse \
        $(IFALIB) pullifa pushifa diffifa

all: defaulttarget

defaulttarget: $(EXECUTABLES) libpyc_runtime.a pyc.cat

$(PYC): $(PYC_OBJS) $(IFALIB) $(JIT_RUNTIME_OBJ)
	$(CXX) $(CFLAGS) $(LDFLAGS) -o $@ $^ $(LIBS)

# Phase D.3.5: runtime library for the v2 LLVM backend's link
# step. The C backend continues to get inline-derived helpers via
# the static-inline copies in pyc_c_runtime.h. The LLVM backend
# produces a single .o that references runtime helpers as plain
# external symbols; libpyc_runtime.a satisfies them.
CC     ?= clang
pyc_runtime.o: pyc_runtime.c pyc_c_runtime.h
	$(CC) -O2 -g -Wall -fPIC -I/usr/local/include -I/opt/homebrew/include -c -o $@ $<

# Rebuild the archive from scratch each time so stale members
# (from sources removed from the build) don't linger.  `ar crs`
# is additive — it never removes members on its own.  Single-
# source archive today but the hygiene matters as soon as the
# archive grows.  Mirrors ifa/Makefile's $(LIBRARY) rule.
libpyc_runtime.a: pyc_runtime.o Makefile
	$(RM) $@
	$(AR) crs $@ pyc_runtime.o

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

install: $(EXECUTABLES) libpyc_runtime.a
	cp $(EXECUTABLES) $(PREFIX)/bin
	cp libpyc_runtime.a $(PREFIX)/lib
	cp $(MANPAGES) $(PREFIX)/man/man1

deinstall:
	rm -f $(EXECUTABLES:%=$(PREFIX)/bin/%)
	rm -f $(PREFIX)/lib/libpyc_runtime.a
	rm -f $(MANPAGES:%=$(PREFIX)/man/man1/%)

# Tests -----------------------------------------------------------------------
#
# `make test`         — everything (unit + ir + e2e). Fails on first category that fails.
# `make test-unit`    — IFA unit tests via the UnitTest framework (`ifa --test`).
# `make test-ir`      — IF1-level golden-file phase tests (`ifa-test`).
# `make test-e2e`     — end-to-end pyc tests (parse, compile, execute, CPython diff).
# `make test-dparse`  — parse-only validation of every tests/*.py.
# `make clean-tests`  — remove tests/build/ and any in-tree leftovers.
#
# See tests/README.md and ifa/testing/TEST_RUNNER.md for adding / debugging tests.

test: test-unit test-ir test-e2e

test-e2e: $(PYC)
	@echo "--- Testing C Backend ---"
	./test_pyc
	@echo "--- Testing LLVM Backend ---"
	PYC_FLAGS=-b ./test_pyc

test-unit: $(IFALIB)
	@if [ -x $(IFA_DIR)/ifa ]; then \
	  $(IFA_DIR)/ifa --test; \
	else \
	  echo "skipping unit tests: $(IFA_DIR)/ifa not built"; \
	fi

test-ir:
	$(MAKE) -C $(IFA_DIR) test-ir

test-dparse test_dparse: $(PYC)
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

clean-tests:
	rm -rf tests/build tests/__pycache__ tests/pyc_compat.py
	rm -f  tests/*.out tests/*.py.c tests/*.py.s
	@# Stale binaries from old in-place test runs: any executable with a .py twin.
	@for f in tests/*; do \
	  if [ -f "$$f" ] && [ -x "$$f" ] && [ -e "$$f.py" ]; then rm -f "$$f"; fi; \
	done

# IFA subtree management ------------------------------------------------------

pullifa:
	git subtree pull --prefix=ifa ifa-remote main --squash

pushifa:
	./pushifa.sh

diffifa:
	git diff ifa-remote/main HEAD:ifa

# Clean -----------------------------------------------------------------------

clean: clean-tests
	rm -f *.o *.d core *.core *.gmon $(EXECUTABLES) LICENSE.i COPYRIGHT.i $(CLEAN_FILES)
	$(MAKE) -C $(IFA_DIR) clean

realclean: clean
	rm -f *.a *.orig *.rej
	$(MAKE) -C $(IFA_DIR) realclean

-include $(PYC_OBJS:.o=.d)

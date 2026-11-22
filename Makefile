# Makefile for PYC

# Build configuration ---------------------------------------------------------
#
# Boehm GC is a hard dependency of the runtime and IFA library; the
# build no longer carries a non-GC branch. The flags below are
# optional; uncomment to enable.

DEBUG=1
# On by default: an -O0 pyc spends about twice as long in flow analysis
# (plcfrs 156 s -> 80 s, identical result). DEBUG still adds -g. For a
# build to step through in gdb, use `make clean && make OPTIMIZE=`.
OPTIMIZE=1
#PROFILE=1
#LEAK_DETECT=1
#VALGRIND=1
#USE_LLVM=1       # LLVM backend; uncomment, or invoke as `make USE_LLVM=1`, to
                  # compile the LLVM code path into pyc. The C backend is the
                  # default + production path. `pyc -b` (PYC_LLVM=1, or
                  # `PYC_FLAGS=-b ./test_pyc`) selects LLVM at runtime once
                  # USE_LLVM=1 is built. The LLVM backend doesn't yet pass
                  # most pyc tests — see CODEGEN_PLAN §3.5 and CODEGEN_LLVM.md.

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
# PORTABLE=1 (a release build, `make dist`): no -march=native, so the
# binary runs on any CPU of its architecture, not only the build host's.
# Passed down to ifa/ too. Changing it needs `make clean` (make does not
# track flags).
ifdef PORTABLE
  MARCH =
  export PORTABLE
else
  MARCH = -march=native
endif
ifdef OPTIMIZE
  CFLAGS += -O3 $(MARCH)
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

LIBS += -lm

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


# Static link (`pyc-static`, which `make dist` installs) -------------------------
#
# A pyc to hand to someone else: LLVM, Boehm GC, dparser and the C++
# runtime linked in, so it needs only the platform's C library (and a C++
# compiler, to compile the programs it writes). Each archive is found by
# name in the usual library directories and the compiler's own; a missing
# one is an error here rather than a silent fallback to the shared library.
# LLVM needs zlib and zstd (its compression support; the libxml2 it also
# names is never referenced). Each is linked statically when an archive
# exists (CI installs zlib1g-dev and libzstd-dev), otherwise dynamically,
# by its soname when there is no development symlink.
LIB_SEARCH = /opt/homebrew/lib /usr/local/lib \
             $(subst :, ,$(shell $(CXX) -print-search-dirs 2>/dev/null | sed -n 's/^libraries: =//p'))
static_lib = $(firstword $(wildcard $(addsuffix /lib$(1).a,$(LIB_SEARCH))))
# The dynamic fallback: -l<name>, or on Linux, when there is no development
# symlink, the soname itself (-l:libzstd.so.1). macOS links its own
# libraries by name (-lz is the SDK's zlib).
ifeq ($(OS_TYPE),Darwin)
  shared_lib = -l$(1)
else
  shared_lib = $(if $(wildcard $(addsuffix /lib$(1).so,$(LIB_SEARCH))),-l$(1),-l:lib$(1).so.1)
endif
need_static_lib = $(or $(call static_lib,$(1)),$(error no static lib$(1).a in $(LIB_SEARCH)))
ifneq ($(OS_TYPE),Darwin)
  STATIC_GROUP_BEGIN = -Wl,--start-group
  STATIC_GROUP_END   = -Wl,--end-group
  STATIC_LDFLAGS     = -static-libstdc++ -static-libgcc -Wl,--dynamic-list=pyc_jit_exports.list
  STATIC_NO_RDYNAMIC = -rdynamic
endif
# The Boehm GC pyc links, and the one `make dist BUNDLE_GC=1` ships for the
# programs pyc builds: its headers are taken from the same prefix as the
# archive (/usr/lib/<multiarch>/libgc.a -> /usr/include), never mixed.
GC_STATIC_LIB = $(call static_lib,gc)
GC_PREFIX     = $(shell echo $(realpath $(GC_STATIC_LIB)) | sed 's|/lib/.*||')
GC_INCLUDE   ?= $(GC_PREFIX)/include
# libffi, which `make dist` ships beside Boehm's for programs that declare a
# ctypes CFUNCTYPE (issues/188): the static archive, and the directory the
# compiler finds ffi.h in (Debian's is multiarch; Homebrew's libffi is
# keg-only, so pass FFI_STATIC_LIB and FFI_INCLUDE there).
FFI_STATIC_LIB ?= $(or $(call static_lib,ffi),$(wildcard $(shell brew --prefix libffi 2>/dev/null)/lib/libffi.a))
FFI_INCLUDE    ?= $(or $(patsubst %/ffi.h,%,$(firstword $(filter %/ffi.h,$(shell printf '\043include <ffi.h>\n' | \
                    $(CXX) -E -M -x c - 2>/dev/null)))),$(wildcard $(shell brew --prefix libffi 2>/dev/null)/include))

# Version stamp ---------------------------------------------------------------

# `pyc --version`, and the name of a `make dist` tarball. A tagged commit
# (vX.Y.Z) is X.Y.Z; past a tag, `git describe` says how far
# (0.1.0-3-g1da76bb1); with no tag yet, MAJOR.MINOR and the commit
# (0.0-g1da76bb1). `-dirty` when the tree has uncommitted changes. Outside a
# git checkout, the BUILD_VERSION file. version.stamp holds it, rewritten
# only when it changes, so version.o follows it (make does not track flags).
GIT_DESCRIBE := $(shell git describe --tags --match 'v[0-9]*' --dirty 2>/dev/null)
GIT_COMMIT   := $(shell git describe --always --dirty 2>/dev/null)
ifneq ($(GIT_DESCRIBE),)
  PYC_VERSION := $(patsubst v%,%,$(GIT_DESCRIBE))
else ifneq ($(GIT_COMMIT),)
  PYC_VERSION := $(MAJOR).$(MINOR)-g$(GIT_COMMIT)
else
  PYC_VERSION := $(MAJOR).$(MINOR)-$(shell cat BUILD_VERSION 2>/dev/null)
endif
VERSIONCFLAGS = -DPYC_VERSION=\"$(PYC_VERSION)\"


# Sources / objects -----------------------------------------------------------

PYC_DEPEND_SRCS = pyc.cc repl.cc python_ifa_util.cc python_ifa_sym.cc \
                  python_ifa_build_syms.cc python_ifa_build_if1.cc \
                  python_ifa_main.cc python_ifa_ctypes.cc python_ifa_dataclass.cc python_parse.cc version.cc
PYC_SRCS = $(PYC_DEPEND_SRCS) gnuc.g.d_parser.cc python.g.d_parser.cc
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
CLEAN_FILES = *.cat $(PYC_OBJS:.o=.d) pyc_runtime.o pyc_runtime.d libpyc_runtime.a pyc_runtime_lto.o pyc-static \
              version.stamp

# Targets ---------------------------------------------------------------------

.PHONY: all defaulttarget install deinstall dist clean realclean clean-tests \
        test test-core test-e2e test-unit test-ir test-dparse test_dparse \
        test-links test_links test-explorer test_explorer test_llvm test-ifa-llvm test_ifa_llvm \
        $(IFALIB) pullifa pushifa diffifa show-sweep

all: defaulttarget

defaulttarget: $(EXECUTABLES) libpyc_runtime.a pyc_runtime_lto.o pyc.cat

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
# The same runtime as LLVM bitcode (-flto), for the LLVM backend: pyc links
# it into the program's module before optimizing (llvm_emit_object), so the
# runtime helpers inline as the C backend's do from pyc_c_runtime.h, and
# falls back to libpyc_runtime.a when it is missing or unreadable. Measured
# 2026-10-18: LLVM-built binaries 17% faster (geomean of 8 corpus programs;
# sha 47%, sieve 30%). Must be built by a clang NO NEWER than the LLVM pyc
# links (CC and LLVM_CONFIG of one version), or pyc cannot read it.
pyc_runtime_lto.o: pyc_runtime.c pyc_c_runtime.h
	$(CC) -O2 -flto -fPIC -I/usr/local/include -I/opt/homebrew/include -c -o $@ $<

libpyc_runtime.a: pyc_runtime.o Makefile
	$(RM) $@
	$(AR) crs $@ pyc_runtime.o

# Per-target override (uses CPPFLAGS because that's what the implicit rule
# sees); keeps -MMD -MP dependency generation active.
version.o: CPPFLAGS += $(VERSIONCFLAGS)
version.o: Makefile version.stamp
.PHONY: FORCE
version.stamp: FORCE
	@echo '$(PYC_VERSION)' | cmp -s - $@ || echo '$(PYC_VERSION)' > $@

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

# The static pyc (see "Static link" above).
pyc-static: $(PYC_OBJS) $(IFALIB) $(JIT_RUNTIME_OBJ) pyc_jit_exports.list
	$(CXX) $(CFLAGS) $(filter-out $(STATIC_NO_RDYNAMIC),$(LDFLAGS)) $(STATIC_LDFLAGS) -o $@ $(PYC_OBJS) $(JIT_RUNTIME_OBJ) \
	  $(STATIC_GROUP_BEGIN) $(IFALIB) $(call need_static_lib,dparse_gc) $(call need_static_lib,gccpp) \
	  $(call need_static_lib,gc) $(wildcard $(shell $(LLVM_CONFIG) --link-static --libfiles)) $(STATIC_GROUP_END) \
	  $(foreach l,z zstd,$(or $(call static_lib,$(l)),$(call shared_lib,$(l)))) -lpthread -ldl -lm \
	  $(if $(filter Darwin,$(OS_TYPE)),,-lrt)

# Install ---------------------------------------------------------------------
#
#   <prefix>/bin/pyc
#   <prefix>/lib/pyc/        pyc's library directory (pyc finds it from its
#                            own location: <exe>/../lib/pyc), holding
#     __pyc__/  pyc_lib/     the builtin module and the Python library
#     pyc_c_runtime.h  Makefile.cg  pyc_runtime.o      the C backend's
#     libpyc_runtime.a  pyc_runtime_lto.o              the LLVM backend's
#     gc/include  gc/lib     with BUNDLE_GC=1: a static Boehm GC that the
#                            programs pyc builds link, so they need no libgc
#     ffi/include ffi/lib    and a static libffi, for ctypes callbacks
#   <prefix>/share/man/man1/pyc.1
#
# PYC_INSTALL_BIN picks the binary (pyc, or pyc-static for `make dist`);
# INSTALL_STRIP=-s strips it (dist does: the debug info is most of its size).
# DESTDIR is honoured, for packagers.
PYC_INSTALL_BIN ?= $(PYC)
INSTALL_LIBDIR   = $(DESTDIR)$(PREFIX)/lib/pyc

install: $(PYC_INSTALL_BIN) libpyc_runtime.a pyc_runtime_lto.o pyc_runtime.o
	install -d $(DESTDIR)$(PREFIX)/bin $(INSTALL_LIBDIR) $(DESTDIR)$(PREFIX)/share/man/man1
	install -m 755 $(INSTALL_STRIP) $(PYC_INSTALL_BIN) $(DESTDIR)$(PREFIX)/bin/$(PYC)
	install -m 644 pyc_c_runtime.h Makefile.cg pyc_runtime.o libpyc_runtime.a pyc_runtime_lto.o $(INSTALL_LIBDIR)
	rm -rf $(INSTALL_LIBDIR)/__pyc__ $(INSTALL_LIBDIR)/pyc_lib
	tar -cf - --exclude=__pycache__ --exclude='*.pyc' __pyc__ pyc_lib | tar -xf - -C $(INSTALL_LIBDIR)
	install -m 644 $(MANPAGES) $(DESTDIR)$(PREFIX)/share/man/man1
ifdef BUNDLE_GC
	@test -n "$(GC_STATIC_LIB)" || { echo "BUNDLE_GC: no static libgc.a in $(LIB_SEARCH)"; exit 1; }
	@test -f "$(GC_INCLUDE)/gc.h" || { echo "BUNDLE_GC: no gc.h in $(GC_INCLUDE) (set GC_INCLUDE)"; exit 1; }
	install -d $(INSTALL_LIBDIR)/gc/lib $(INSTALL_LIBDIR)/gc/include
	install -m 644 $(GC_STATIC_LIB) $(INSTALL_LIBDIR)/gc/lib
	$(if $(call static_lib,gccpp),install -m 644 $(call static_lib,gccpp) $(INSTALL_LIBDIR)/gc/lib)
	install -m 644 $(GC_INCLUDE)/gc.h $(INSTALL_LIBDIR)/gc/include
	cp -R $(GC_INCLUDE)/gc $(INSTALL_LIBDIR)/gc/include/
	@test -n "$(FFI_STATIC_LIB)" || { echo "BUNDLE_GC: no static libffi.a (set FFI_STATIC_LIB)"; exit 1; }
	@test -f "$(FFI_INCLUDE)/ffi.h" || { echo "BUNDLE_GC: no ffi.h in '$(FFI_INCLUDE)' (set FFI_INCLUDE)"; exit 1; }
	install -d $(INSTALL_LIBDIR)/ffi/lib $(INSTALL_LIBDIR)/ffi/include
	install -m 644 $(FFI_STATIC_LIB) $(INSTALL_LIBDIR)/ffi/lib/libffi.a
	install -m 644 $(FFI_INCLUDE)/ffi.h $(FFI_INCLUDE)/ffitarget.h $(INSTALL_LIBDIR)/ffi/include
endif

deinstall:
	rm -f $(DESTDIR)$(PREFIX)/bin/$(PYC) $(DESTDIR)$(PREFIX)/share/man/man1/pyc.1
	rm -rf $(INSTALL_LIBDIR)

# Distribution tarball (`make PORTABLE=1 dist` in a clean tree) ---------------
#
# A relocatable install of pyc-static with the GC bundled, under
# dist/pyc-<version>-<os>-<arch>/, and its .tar.gz. Unpack it anywhere: pyc
# finds its library beside itself. Needs PORTABLE=1 so no object was built
# with -march=native; since make does not track flags, build it in a clean
# tree (`make clean`).
DIST_VERSION ?= $(PYC_VERSION)
DIST_NAME    ?= pyc-$(DIST_VERSION)-$(shell uname -s | tr A-Z a-z)-$(shell uname -m)
DIST_ROOT     = dist/$(DIST_NAME)

# Strip the installed pyc, but keep the symbols it exports: the JIT finds the
# runtime's (_CG_*, GC_*) in the running pyc by name. On Linux `install -s`
# keeps the dynamic symbol table (pyc_jit_exports.list). On macOS a full
# strip removes the exported globals too, and the stripped pyc segfaulted in
# its static initializers before main; `strip -x` removes local symbols only.
ifeq ($(OS_TYPE),Darwin)
  DIST_INSTALL_STRIP =
  DIST_STRIP_AFTER   = strip -x $(DIST_ROOT)/bin/$(PYC)
else
  DIST_INSTALL_STRIP = -s
  DIST_STRIP_AFTER   =
endif

dist:
	@test -n "$(PORTABLE)" || { echo "make dist needs PORTABLE=1 (and a clean tree): see the Makefile"; exit 1; }
	$(MAKE) pyc-static libpyc_runtime.a pyc_runtime_lto.o pyc_runtime.o pyc.cat
	rm -rf $(DIST_ROOT) dist/$(DIST_NAME).tar.gz
	$(MAKE) install PYC_INSTALL_BIN=pyc-static INSTALL_STRIP=$(DIST_INSTALL_STRIP) BUNDLE_GC=1 \
	  PREFIX=$(abspath $(DIST_ROOT)) DESTDIR=
	$(DIST_STRIP_AFTER)
	tar -czf dist/$(DIST_NAME).tar.gz -C dist $(DIST_NAME)
	@echo "dist/$(DIST_NAME).tar.gz"

# Tests -----------------------------------------------------------------------
#
# `make test`         — THE GATE: everything CI runs, in CI's order. Both
#                       backends. Fails on the first category that fails.
# `make test-core`    — unit + ir + e2e only (what `make test` was before
#                       2026-09-20). Still covers both backends, via test-e2e.
# `make test-unit`    — IFA unit tests via the UnitTest framework (`ifa --test`).
# `make test-ir`      — IF1-level golden-file phase tests (`ifa-test`).
# `make test-e2e`     — end-to-end pyc tests (parse, compile, execute, CPython
#                       diff), run TWICE: C backend, then LLVM backend.
# `make test-ifa-llvm`— the ifa V-language LLVM smoke, i.e. `make -C ifa
#                       test_llvm`. NOT the same thing as this directory's
#                       `test_llvm`, which is the pyc LLVM e2e; the two
#                       Makefiles have long disagreed on that name.
# `make test-dparse`  — parse-only validation of every tests/*.py.
# `make test-links`   — every relative Markdown link resolves. Needs no build.
# `make clean-tests`  — remove tests/build/ and any in-tree leftovers.
#
# `test` mirrors .github/workflows/ci.yml step for step so a green local run
# means a green CI run. CI's own LLVM-backend pyc e2e is a separate step only
# because it applies an LLVM_BASELINE_PASS floor instead of a hard pass/fail;
# test-e2e below runs the same thing strictly, so `test` does not repeat it.
# Touched a header? `make clean` first — header deps are incomplete and stale
# objects produce failures that read like real bugs.
#
# See tests/README.md and ifa/testing/TEST_RUNNER.md for adding / debugging tests.

test: test-core test-ifa-llvm test-dparse test-links test-explorer

test-core: test-unit test-ir test-e2e

test-e2e: $(PYC) libpyc_runtime.a
	@echo "--- Testing C Backend ---"
	./test_pyc.py
	@echo "--- Testing LLVM Backend ---"
	PYC_FLAGS=-b ./test_pyc.py

test_llvm: pyc
	PYC_FLAGS=-b ./test_pyc.py

test-ifa-llvm test_ifa_llvm:
	$(MAKE) -C $(IFA_DIR) test_llvm



test-unit: $(IFALIB)
	@if [ -x $(IFA_DIR)/ifa ]; then \
	  $(IFA_DIR)/ifa --test; \
	else \
	  echo "skipping unit tests: $(IFA_DIR)/ifa not built"; \
	fi

test-ir:
	$(MAKE) -C $(IFA_DIR) test-ir

# `make test-links` — every relative Markdown link in a tracked *.md
# resolves. Needs no build. Exists because a 2026-09-02 sweep found 95 of
# 1100 issue-tree links broken (plus 61 elsewhere): archiving an issue into
# `closed/` never updated the links pointing at it, and nothing checked.
test-links test_links:
	@python3 check_doc_links.py

# The compilation explorer (--html / --dump-json): telemetry must not change
# codegen, the delta-encoded passes must rebuild, the FA hooks must fire,
# and pyc -x must keep the source view (test_explorer.py says why each).
test-explorer test_explorer: $(PYC) libpyc_runtime.a
	@echo "--- Compilation explorer ---"
	python3 test_explorer.py

# pyc's parser must agree with CPython's on every test: accept what
# `ast.parse` accepts and reject what it rejects (issues/106 -- pyc used to
# accept an `if:` with no body, which CPython rejects).
test-dparse test_dparse: $(PYC)
	@echo "--- DParser parse validation ---"; \
	rejects=$$(printf '%s\n' 'import ast, sys' 'for f in sys.argv[1:]:' \
	  '  try: ast.parse(open(f, "rb").read(), f)' '  except SyntaxError: print(f)' \
	  | python3 - tests/*.py) || { echo "CPython parse oracle failed"; exit 1; }; \
	failed=0; \
	for f in tests/*.py; do \
	  want=0; case " $$(echo $$rejects) " in *" $$f "*) want=1;; esac; \
	  if ./$(PYC) --dparse-only "$$f" >/dev/null 2>&1; then got=0; else got=1; fi; \
	  if [ $$got -eq $$want ]; then \
	    if [ $$want -eq 0 ]; then echo "$$f OK"; else echo "$$f OK (rejected, as CPython does)"; fi; \
	  else \
	    ./$(PYC) --dparse-only "$$f" 2>&1 | head -3; \
	    if [ $$want -eq 0 ]; then echo "$$f FAILED"; else echo "$$f FAILED (accepted; CPython rejects it)"; fi; \
	    failed=$$((failed + 1)); \
	  fi; \
	done; \
	if [ "$$failed" -eq 0 ]; then \
	  echo "--- ALL DParser tests PASSED ---"; \
	else \
	  echo "--- $$failed DParser test(s) FAILED ---"; exit 1; \
	fi

clean-tests:
	rm -rf tests/build tests/__pycache__ tests/pyc_compat.py tests/*.dSYM
	rm -f  tests/*.out tests/*.py.c tests/*.py.s tests/*.ll tests/*.bak tests/*.o tests/*.d
	@# Stale binaries from old in-place test runs: any executable with a .py twin.
	@for f in tests/*; do \
	  if [ -f "$$f" ] && [ -x "$$f" ] && [ -e "$$f.py" ]; then rm -f "$$f"; fi; \
	done

# compile_commands.json (clangd / standalone-linter support) -----------------
#
# Without this, clangd (and other tools that don't understand this
# Makefile's -I/-D flags) can't find headers like common.h/ifadefs.h and
# reports false-positive "file not found" / "unknown type" diagnostics on
# every file under ifa/. Regenerate whenever source files are added/removed
# or build flags change; not checked in (absolute paths, machine-specific --
# see .gitignore).
compile_commands:
	$(MAKE) clean
	bear -- $(MAKE) -j4

# IFA subtree management ------------------------------------------------------

pullifa:
	git subtree pull --prefix=ifa ifa-remote main --squash

pushifa:
	./pushifa.sh

diffifa:
	git diff ifa-remote/main HEAD:ifa

show-sweep:
	less sweeps/`ls -t sweeps/ | head -2 | tail -1`

# Clean -----------------------------------------------------------------------

clean: clean-tests
	rm -f *.o *.d core *.core *.gmon $(EXECUTABLES) LICENSE.i COPYRIGHT.i $(CLEAN_FILES)
	rm -f *.py.c *.py.s *.out *.bak *.ll *.txt *.log *.ir out_class out_dyn out_mc baseline
	rm -f *.g.d_parser.cc *.g.d_parser.h *.g.d_parser.d
	rm -rf dist
	rm -rf *.dSYM
	@# Stale binaries from one-off runs in root and shedskin: any executable with a .py twin.
	@for f in * shedskin_examples/*/*; do \
	  if [ -f "$$f" ] && [ -x "$$f" ] && [ -e "$$f.py" ] && [ "$$f" != "pyc" ]; then rm -f "$$f"; fi; \
	done
	@for f in shedskin_examples/*/*.dSYM; do \
	  if [ -d "$$f" ]; then rm -rf "$$f"; fi; \
	done
	$(MAKE) -C $(IFA_DIR) clean

realclean: clean
	rm -f *.a *.orig *.rej
	$(MAKE) -C $(IFA_DIR) realclean

-include $(PYC_OBJS:.o=.d)

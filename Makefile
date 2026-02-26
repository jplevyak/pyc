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

PYTHON=python2.7
LDFLAGS += -L./Python-2.7.18 -l$(PYTHON)

MAJOR=0
MINOR=0

CXX ?= clang++
AR ?= llvm-ar
PREFIX ?= /usr/local

.PHONY: all install

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

IFA_DIR=../ifa
PLIB_DIR=../ifa/plib

CFLAGS += -std=c++23 -D__PYC__=1 -I$(PLIB_DIR) -I$(IFA_DIR) -I/usr/local/python2.7/include/python2.7
ifdef USE_SS
CFLAGS += -Ilib -Ilib/os
endif
# for use of 'register' in python2.7
CFLAGS += -Wno-register
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
LIBS += -L$(IFA_DIR) -lifa_gc -lgc -lgccpp -ldparse_gc
IFALIB = $(IFA_DIR)/libifa_gc.a
else
LIBS += -L$(IFA_DIR) -lifa -L../plib -lplib -ldparse
IFALIB = $(IFA_DIR)/libifa.a
endif

ifdef USE_SS
CFLAGS += -DSSLIB="../shedskin/shedskin/lib"
endif

ifeq ($(OS_TYPE),CYGWIN)
  LIBS += -L/usr/lib/$(PYTHON)/config -l$(PYTHON).dll
else
  LIBS += -l$(PYTHON) -lutil
endif

CFLAGS += -Wno-deprecated-register

AUX_FILES = $(MODULE)/index.html $(MODULE)/manual.html $(MODULE)/faq.html $(MODULE)/pyc.1 $(MODULE)/pyc.cat

LIB_SRCS = lib/builtin.cpp $(wildcard lib/*.cpp) $(wildcard lib/os/*.cpp)
LIB_OBJS = $(LIB_SRCS:%.cpp=%.o)

PYC_DEPEND_SRCS = pyc.cc python_ifa.cc version.cc
PYC_SRCS = $(PYC_DEPEND_SRCS) gnuc.g.d_parser.cc
ifdef USE_SS
PYC_SRCS += shedskin.cc
endif
ifdef USE_LLVM
PYC_SRCS += llvm.cc
endif
PYC_OBJS = $(PYC_SRCS:%.cc=%.o)

EXECUTABLE_FILES = pyc
LIBRARY = libpyc_gc.a
LIBRARIES = libpyc_gc.a
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

DEPEND_SRCS = $(PYC_DEPEND_SRCS) $(LIB_SRCS)

defaulttarget: $(EXECUTABLES) pyc.cat

$(LIBRARY):  $(LIB_OBJS)
	ar $(AR_FLAGS) $@ $^

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

clean:
	\rm -f *.o core *.core *.gmon $(EXECUTABLES) LICENSE.i COPYRIGHT.i $(CLEAN_FILES)

realclean: clean
	\rm -f *.a *.orig *.rej

depend:
	./mkdep $(CFLAGS) $(DEPEND_SRCS)

-include .depend
# DO NOT DELETE THIS LINE -- mkdep uses it.
# DO NOT PUT ANYTHING AFTER THIS LINE, IT WILL GO AWAY.

builtin.o: lib/builtin.cpp lib/builtin.hpp lib/re.hpp
ConfigParser.o: lib/ConfigParser.cpp lib/ConfigParser.hpp lib/builtin.hpp \
 lib/re.hpp
bisect.o: lib/bisect.cpp lib/bisect.hpp lib/builtin.hpp
builtin.o: lib/builtin.cpp lib/builtin.hpp lib/re.hpp
cStringIO.o: lib/cStringIO.cpp lib/cStringIO.hpp lib/builtin.hpp
collections.o: lib/collections.cpp lib/collections.hpp lib/builtin.hpp
copy.o: lib/copy.cpp lib/copy.hpp lib/builtin.hpp
datetime.o: lib/datetime.cpp lib/datetime.hpp lib/builtin.hpp \
 lib/time.hpp lib/string.hpp
math.o: lib/math.cpp lib/math.hpp lib/builtin.hpp
random.o: lib/random.cpp lib/random.hpp lib/builtin.hpp lib/math.hpp \
 lib/time.hpp
re.o: lib/re.cpp lib/re.hpp lib/builtin.hpp
signal.o: lib/signal.cpp lib/signal.hpp lib/builtin.hpp
socket.o: lib/socket.cpp lib/socket.hpp lib/builtin.hpp
stat.o: lib/stat.cpp lib/stat.hpp lib/builtin.hpp
string.o: lib/string.cpp lib/string.hpp lib/builtin.hpp
sys.o: lib/sys.cpp lib/sys.hpp lib/builtin.hpp
time.o: lib/time.cpp lib/time.hpp lib/builtin.hpp

# IF YOU PUT ANYTHING HERE IT WILL GO AWAY

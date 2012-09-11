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

MAJOR=0
MINOR=0

include ../plib/Makefile

ifeq ($(OS_TYPE),Darwin)
PYTHON=python2.6
endif

CFLAGS += -D__PYC__=1 -I../plib -I../ifa -I/usr/include/$(PYTHON) -Ilib -Ilib/os
# LLVM flags
CFLAGS += -D_GNU_SOURCE -D__STDC_LIMIT_MACROS -D__STDC_CONSTANT_MACROS -fno-exceptions -fno-rtti -fPIC -Woverloaded-virtual -Wcast-qual
LIBS += -lpcre 
ifdef USE_LLVM
# LLVM libs
LIBS +=-lLLVMMSIL -lLLVMMSILInfo -lLLVMLinker -lLLVMipo -lLLVMInterpreter -lLLVMInstrumentation -lLLVMJIT -lLLVMExecutionEngine -lLLVMCppBackend -lLLVMCppBackendInfo -lLLVMCBackend -lLLVMCBackendInfo -lLLVMBitWriter -lLLVMX86Disassembler -lLLVMX86AsmParser -lLLVMX86AsmPrinter -lLLVMX86CodeGen -lLLVMX86Info -lLLVMAsmParser -lLLVMArchive -lLLVMBitReader -lLLVMMCParser -lLLVMSelectionDAG -lLLVMCodeGen -lLLVMScalarOpts -lLLVMInstCombine -lLLVMTransformUtils -lLLVMipa -lLLVMTarget -lLLVMMC -lLLVMCore -lLLVMAlphaInfo -lLLVMSupport -lLLVMSystem -lLLVMAsmPrinter -lLLVMAnalysis -ldl
CFLAGS += -DUSE_LLVM=1
endif
ifdef USE_GC
LIBS += -L../ifa -lifa_gc -L../plib -lplib_gc -lgc 
IFALIB = ../ifa/libifa_gc.a
else
LIBS += -L../ifa -lifa -L../plib -lplib
IFALIB = ../ifa/libifa.a
endif

LIBS += -ldparse_gc

ifdef USE_SS
CFLAGS += -DSSLIB="../shedskin/shedskin/lib"
endif

ifeq ($(OS_TYPE),CYGWIN)
  LIBS += -L/usr/lib/$(PYTHON)/config -l$(PYTHON).dll
else
  LIBS += -l$(PYTHON) -lutil
endif

AUX_FILES = $(MODULE)/index.html $(MODULE)/manual.html $(MODULE)/faq.html $(MODULE)/pyc.1 $(MODULE)/pyc.cat

LIB_SRCS = lib/builtin.cpp $(wildcard lib/*.cpp) $(wildcard lib/os/*.cpp)
LIB_OBJS = $(LIB_SRCS:%.cpp=%.o)

PYC_SRCS = pyc.cc python_ifa.cc gnuc.g.d_parser.cc version.cc
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

ALL_SRCS = $(PYC_SRCS) $(LIB_SRCS)

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
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^ $(LIBS) 

pyc.cat: pyc.1
	rm -f pyc.cat
	nroff -man pyc.1 | sed -e 's/.//g' > pyc.cat

pyc.o: LICENSE.i COPYRIGHT.i

version.o: Makefile


-include .depend
# DO NOT DELETE THIS LINE -- mkdep uses it.
# DO NOT PUT ANYTHING AFTER THIS LINE, IT WILL GO AWAY.


# IF YOU PUT ANYTHING HERE IT WILL GO AWAY

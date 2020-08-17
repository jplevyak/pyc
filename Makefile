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

CXX ?= clang++
AR ?= llvm-ar

include ../plib/Makefile

ifeq ($(OS_TYPE),Darwin)
PYTHON=python2.6
endif

LLVM_VERSION=6.0

CFLAGS += -std=c++2a -D__PYC__=1 -I../plib -I../ifa -I/usr/include/$(PYTHON) -Ilib -Ilib/os
# for use of 'register' in python2.7
CFLAGS += -Wno-register
# LLVM flags
CFLAGS += -I/usr/include/llvm-$(LLVM_VERSION) -I/usr/include/llvm-c-$(LLVM_VERSION) -D_GNU_SOURCE -D__STDC_LIMIT_MACROS -D__STDC_CONSTANT_MACROS -fno-exceptions -fno-rtti -fPIC -Woverloaded-virtual -Wcast-qual
LIBS += -lpcre 
ifdef USE_LLVM
# LLVM libs
LIBS += -L/usr/lib/llvm-$(LLVM_VERSION)/lib -lLLVM-$(LLVM_VERSION)
CFLAGS += -DUSE_LLVM=1
endif
ifdef USE_GC
LIBS += -L../ifa -lifa_gc -L../plib -lplib_gc -lgc -ldparse_gc
IFALIB = ../ifa/libifa_gc.a
else
LIBS += -L../ifa -lifa -L../plib -lplib -ldparse
IFALIB = ../ifa/libifa.a
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
	nroff -man pyc.1 | sed -e 's/.//g' > pyc.cat

pyc.o: LICENSE.i COPYRIGHT.i

version.o: Makefile


-include .depend
# DO NOT DELETE THIS LINE -- mkdep uses it.
# DO NOT PUT ANYTHING AFTER THIS LINE, IT WILL GO AWAY.

pyc.o: pyc.cc defs.h /usr/include/python2.7/Python.h \
 /usr/include/python2.7/patchlevel.h /usr/include/python2.7/pyconfig.h \
 /usr/include/python2.7/pymacconfig.h /usr/include/python2.7/pyport.h \
 /usr/include/python2.7/pymath.h /usr/include/python2.7/pymem.h \
 /usr/include/python2.7/object.h /usr/include/python2.7/objimpl.h \
 /usr/include/python2.7/pydebug.h /usr/include/python2.7/unicodeobject.h \
 /usr/include/python2.7/intobject.h /usr/include/python2.7/boolobject.h \
 /usr/include/python2.7/longobject.h /usr/include/python2.7/floatobject.h \
 /usr/include/python2.7/complexobject.h \
 /usr/include/python2.7/rangeobject.h \
 /usr/include/python2.7/stringobject.h \
 /usr/include/python2.7/memoryobject.h \
 /usr/include/python2.7/bufferobject.h \
 /usr/include/python2.7/bytesobject.h \
 /usr/include/python2.7/bytearrayobject.h \
 /usr/include/python2.7/tupleobject.h /usr/include/python2.7/listobject.h \
 /usr/include/python2.7/dictobject.h /usr/include/python2.7/enumobject.h \
 /usr/include/python2.7/setobject.h /usr/include/python2.7/methodobject.h \
 /usr/include/python2.7/moduleobject.h \
 /usr/include/python2.7/funcobject.h /usr/include/python2.7/classobject.h \
 /usr/include/python2.7/fileobject.h /usr/include/python2.7/cobject.h \
 /usr/include/python2.7/pycapsule.h /usr/include/python2.7/traceback.h \
 /usr/include/python2.7/sliceobject.h /usr/include/python2.7/cellobject.h \
 /usr/include/python2.7/iterobject.h /usr/include/python2.7/genobject.h \
 /usr/include/python2.7/descrobject.h /usr/include/python2.7/warnings.h \
 /usr/include/python2.7/weakrefobject.h /usr/include/python2.7/codecs.h \
 /usr/include/python2.7/pyerrors.h /usr/include/python2.7/pystate.h \
 /usr/include/python2.7/pyarena.h /usr/include/python2.7/modsupport.h \
 /usr/include/python2.7/pythonrun.h /usr/include/python2.7/ceval.h \
 /usr/include/python2.7/sysmodule.h /usr/include/python2.7/intrcheck.h \
 /usr/include/python2.7/import.h /usr/include/python2.7/abstract.h \
 /usr/include/python2.7/compile.h /usr/include/python2.7/code.h \
 /usr/include/python2.7/eval.h /usr/include/python2.7/pyctype.h \
 /usr/include/python2.7/pystrtod.h /usr/include/python2.7/pystrcmp.h \
 /usr/include/python2.7/dtoa.h /usr/include/python2.7/pyfpe.h \
 /usr/include/python2.7/Python-ast.h /usr/include/python2.7/asdl.h \
 /usr/include/python2.7/symtable.h ../plib/plib.h ../plib/tls.h \
 ../plib/arg.h ../plib/barrier.h ../plib/config.h ../plib/stat.h \
 ../plib/dlmalloc.h ../plib/freelist.h ../plib/defalloc.h ../plib/list.h \
 ../plib/log.h ../plib/vec.h ../plib/map.h ../plib/threadpool.h \
 ../plib/misc.h ../plib/util.h ../plib/conn.h ../plib/md5.h \
 ../plib/mt64.h ../plib/hash.h ../plib/persist.h ../plib/prime.h \
 ../plib/service.h ../plib/timer.h ../plib/unit.h ../ifa/ifa.h \
 ../ifa/ifadefs.h ../ifa/ast.h ../ifa/ifa.h ../ifa/builtin.h \
 ../ifa/builtin_symbols.h ../ifa/cg.h ../ifa/fa.h ../ifa/code.h \
 ../ifa/prim.h ../ifa/prim_data.h ../ifa/sym.h ../ifa/num.h \
 ../ifa/clone.h ../ifa/fail.h ../ifa/fun.h ../ifa/if1.h ../ifa/ifalog.h \
 ../ifa/pdb.h ../ifa/pnode.h ../ifa/var.h ../ifa/fa.h ../ifa/prim.h \
 python_ifa.h ../ifa/pattern.h COPYRIGHT.i LICENSE.i
python_ifa.o: python_ifa.cc defs.h /usr/include/python2.7/Python.h \
 /usr/include/python2.7/patchlevel.h /usr/include/python2.7/pyconfig.h \
 /usr/include/python2.7/pymacconfig.h /usr/include/python2.7/pyport.h \
 /usr/include/python2.7/pymath.h /usr/include/python2.7/pymem.h \
 /usr/include/python2.7/object.h /usr/include/python2.7/objimpl.h \
 /usr/include/python2.7/pydebug.h /usr/include/python2.7/unicodeobject.h \
 /usr/include/python2.7/intobject.h /usr/include/python2.7/boolobject.h \
 /usr/include/python2.7/longobject.h /usr/include/python2.7/floatobject.h \
 /usr/include/python2.7/complexobject.h \
 /usr/include/python2.7/rangeobject.h \
 /usr/include/python2.7/stringobject.h \
 /usr/include/python2.7/memoryobject.h \
 /usr/include/python2.7/bufferobject.h \
 /usr/include/python2.7/bytesobject.h \
 /usr/include/python2.7/bytearrayobject.h \
 /usr/include/python2.7/tupleobject.h /usr/include/python2.7/listobject.h \
 /usr/include/python2.7/dictobject.h /usr/include/python2.7/enumobject.h \
 /usr/include/python2.7/setobject.h /usr/include/python2.7/methodobject.h \
 /usr/include/python2.7/moduleobject.h \
 /usr/include/python2.7/funcobject.h /usr/include/python2.7/classobject.h \
 /usr/include/python2.7/fileobject.h /usr/include/python2.7/cobject.h \
 /usr/include/python2.7/pycapsule.h /usr/include/python2.7/traceback.h \
 /usr/include/python2.7/sliceobject.h /usr/include/python2.7/cellobject.h \
 /usr/include/python2.7/iterobject.h /usr/include/python2.7/genobject.h \
 /usr/include/python2.7/descrobject.h /usr/include/python2.7/warnings.h \
 /usr/include/python2.7/weakrefobject.h /usr/include/python2.7/codecs.h \
 /usr/include/python2.7/pyerrors.h /usr/include/python2.7/pystate.h \
 /usr/include/python2.7/pyarena.h /usr/include/python2.7/modsupport.h \
 /usr/include/python2.7/pythonrun.h /usr/include/python2.7/ceval.h \
 /usr/include/python2.7/sysmodule.h /usr/include/python2.7/intrcheck.h \
 /usr/include/python2.7/import.h /usr/include/python2.7/abstract.h \
 /usr/include/python2.7/compile.h /usr/include/python2.7/code.h \
 /usr/include/python2.7/eval.h /usr/include/python2.7/pyctype.h \
 /usr/include/python2.7/pystrtod.h /usr/include/python2.7/pystrcmp.h \
 /usr/include/python2.7/dtoa.h /usr/include/python2.7/pyfpe.h \
 /usr/include/python2.7/Python-ast.h /usr/include/python2.7/asdl.h \
 /usr/include/python2.7/symtable.h ../plib/plib.h ../plib/tls.h \
 ../plib/arg.h ../plib/barrier.h ../plib/config.h ../plib/stat.h \
 ../plib/dlmalloc.h ../plib/freelist.h ../plib/defalloc.h ../plib/list.h \
 ../plib/log.h ../plib/vec.h ../plib/map.h ../plib/threadpool.h \
 ../plib/misc.h ../plib/util.h ../plib/conn.h ../plib/md5.h \
 ../plib/mt64.h ../plib/hash.h ../plib/persist.h ../plib/prime.h \
 ../plib/service.h ../plib/timer.h ../plib/unit.h ../ifa/ifa.h \
 ../ifa/ifadefs.h ../ifa/ast.h ../ifa/ifa.h ../ifa/builtin.h \
 ../ifa/builtin_symbols.h ../ifa/cg.h ../ifa/fa.h ../ifa/code.h \
 ../ifa/prim.h ../ifa/prim_data.h ../ifa/sym.h ../ifa/num.h \
 ../ifa/clone.h ../ifa/fail.h ../ifa/fun.h ../ifa/if1.h ../ifa/ifalog.h \
 ../ifa/pdb.h ../ifa/pnode.h ../ifa/var.h ../ifa/fa.h ../ifa/prim.h \
 python_ifa.h ../ifa/pattern.h pyc_symbols.h
version.o: version.cc defs.h /usr/include/python2.7/Python.h \
 /usr/include/python2.7/patchlevel.h /usr/include/python2.7/pyconfig.h \
 /usr/include/python2.7/pymacconfig.h /usr/include/python2.7/pyport.h \
 /usr/include/python2.7/pymath.h /usr/include/python2.7/pymem.h \
 /usr/include/python2.7/object.h /usr/include/python2.7/objimpl.h \
 /usr/include/python2.7/pydebug.h /usr/include/python2.7/unicodeobject.h \
 /usr/include/python2.7/intobject.h /usr/include/python2.7/boolobject.h \
 /usr/include/python2.7/longobject.h /usr/include/python2.7/floatobject.h \
 /usr/include/python2.7/complexobject.h \
 /usr/include/python2.7/rangeobject.h \
 /usr/include/python2.7/stringobject.h \
 /usr/include/python2.7/memoryobject.h \
 /usr/include/python2.7/bufferobject.h \
 /usr/include/python2.7/bytesobject.h \
 /usr/include/python2.7/bytearrayobject.h \
 /usr/include/python2.7/tupleobject.h /usr/include/python2.7/listobject.h \
 /usr/include/python2.7/dictobject.h /usr/include/python2.7/enumobject.h \
 /usr/include/python2.7/setobject.h /usr/include/python2.7/methodobject.h \
 /usr/include/python2.7/moduleobject.h \
 /usr/include/python2.7/funcobject.h /usr/include/python2.7/classobject.h \
 /usr/include/python2.7/fileobject.h /usr/include/python2.7/cobject.h \
 /usr/include/python2.7/pycapsule.h /usr/include/python2.7/traceback.h \
 /usr/include/python2.7/sliceobject.h /usr/include/python2.7/cellobject.h \
 /usr/include/python2.7/iterobject.h /usr/include/python2.7/genobject.h \
 /usr/include/python2.7/descrobject.h /usr/include/python2.7/warnings.h \
 /usr/include/python2.7/weakrefobject.h /usr/include/python2.7/codecs.h \
 /usr/include/python2.7/pyerrors.h /usr/include/python2.7/pystate.h \
 /usr/include/python2.7/pyarena.h /usr/include/python2.7/modsupport.h \
 /usr/include/python2.7/pythonrun.h /usr/include/python2.7/ceval.h \
 /usr/include/python2.7/sysmodule.h /usr/include/python2.7/intrcheck.h \
 /usr/include/python2.7/import.h /usr/include/python2.7/abstract.h \
 /usr/include/python2.7/compile.h /usr/include/python2.7/code.h \
 /usr/include/python2.7/eval.h /usr/include/python2.7/pyctype.h \
 /usr/include/python2.7/pystrtod.h /usr/include/python2.7/pystrcmp.h \
 /usr/include/python2.7/dtoa.h /usr/include/python2.7/pyfpe.h \
 /usr/include/python2.7/Python-ast.h /usr/include/python2.7/asdl.h \
 /usr/include/python2.7/symtable.h ../plib/plib.h ../plib/tls.h \
 ../plib/arg.h ../plib/barrier.h ../plib/config.h ../plib/stat.h \
 ../plib/dlmalloc.h ../plib/freelist.h ../plib/defalloc.h ../plib/list.h \
 ../plib/log.h ../plib/vec.h ../plib/map.h ../plib/threadpool.h \
 ../plib/misc.h ../plib/util.h ../plib/conn.h ../plib/md5.h \
 ../plib/mt64.h ../plib/hash.h ../plib/persist.h ../plib/prime.h \
 ../plib/service.h ../plib/timer.h ../plib/unit.h ../ifa/ifa.h \
 ../ifa/ifadefs.h ../ifa/ast.h ../ifa/ifa.h ../ifa/builtin.h \
 ../ifa/builtin_symbols.h ../ifa/cg.h ../ifa/fa.h ../ifa/code.h \
 ../ifa/prim.h ../ifa/prim_data.h ../ifa/sym.h ../ifa/num.h \
 ../ifa/clone.h ../ifa/fail.h ../ifa/fun.h ../ifa/if1.h ../ifa/ifalog.h \
 ../ifa/pdb.h ../ifa/pnode.h ../ifa/var.h ../ifa/fa.h ../ifa/prim.h \
 python_ifa.h ../ifa/pattern.h
builtin.o: lib/builtin.cpp lib/builtin.hpp lib/re.hpp
datetime.o: lib/datetime.cpp lib/datetime.hpp lib/builtin.hpp \
 lib/time.hpp lib/string.hpp
random.o: lib/random.cpp lib/random.hpp lib/builtin.hpp lib/math.hpp \
 lib/time.hpp
re.o: lib/re.cpp lib/re.hpp lib/builtin.hpp
glob.o: lib/glob.cpp lib/glob.hpp lib/builtin.hpp lib/os/path.hpp \
 lib/builtin.hpp lib/os/__init__.hpp lib/stat.hpp lib/fnmatch.hpp \
 lib/os/__init__.hpp lib/re.hpp
stat.o: lib/stat.cpp lib/stat.hpp lib/builtin.hpp
socket.o: lib/socket.cpp lib/socket.hpp lib/builtin.hpp
time.o: lib/time.cpp lib/time.hpp lib/builtin.hpp
getopt.o: lib/getopt.cpp lib/getopt.hpp lib/builtin.hpp lib/sys.hpp \
 lib/os/__init__.hpp lib/builtin.hpp
cStringIO.o: lib/cStringIO.cpp lib/cStringIO.hpp lib/builtin.hpp
string.o: lib/string.cpp lib/string.hpp lib/builtin.hpp
ConfigParser.o: lib/ConfigParser.cpp lib/ConfigParser.hpp lib/builtin.hpp \
 lib/re.hpp
fnmatch.o: lib/fnmatch.cpp lib/fnmatch.hpp lib/builtin.hpp \
 lib/os/path.hpp lib/builtin.hpp lib/os/__init__.hpp lib/stat.hpp \
 lib/os/__init__.hpp lib/re.hpp
bisect.o: lib/bisect.cpp lib/bisect.hpp lib/builtin.hpp
copy.o: lib/copy.cpp lib/copy.hpp lib/builtin.hpp
math.o: lib/math.cpp lib/math.hpp lib/builtin.hpp
sys.o: lib/sys.cpp lib/sys.hpp lib/builtin.hpp
signal.o: lib/signal.cpp lib/signal.hpp lib/builtin.hpp
collections.o: lib/collections.cpp lib/collections.hpp lib/builtin.hpp
builtin.o: lib/builtin.cpp lib/builtin.hpp lib/re.hpp
__init__.o: lib/os/__init__.cpp lib/os/__init__.hpp lib/builtin.hpp \
 lib/os/path.hpp lib/os/__init__.hpp lib/stat.hpp lib/builtin.hpp
path.o: lib/os/path.cpp lib/os/path.hpp lib/builtin.hpp \
 lib/os/__init__.hpp lib/stat.hpp lib/builtin.hpp

# IF YOU PUT ANYTHING HERE IT WILL GO AWAY

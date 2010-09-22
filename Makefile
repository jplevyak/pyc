# Makefile for PYC

all: defaulttarget

USE_PLIB=1
MODULE=pyc
DEBUG=1
#OPTIMIZE=1
#PROFILE=1
USE_GC=1
#LEAK_DETECT=1
#VALGRIND=1

MAJOR=0
MINOR=0

include ../plib/Makefile

CFLAGS += -D__PYC__=1 -I../plib -I../ifa -I/usr/include/python2.6 -Ilib -Ilib/os
# LLVM flags
CFLAGS += -D_GNU_SOURCE -D__STDC_LIMIT_MACROS -D__STDC_CONSTANT_MACROS -fno-exceptions -fno-rtti -fPIC -Woverloaded-virtual -Wcast-qual
LIBS += -lpcre 
# LLVM libs
LIBS +=-lLLVMMSIL -lLLVMMSILInfo -lLLVMLinker -lLLVMipo -lLLVMInterpreter -lLLVMInstrumentation -lLLVMJIT -lLLVMExecutionEngine -lLLVMCppBackend -lLLVMCppBackendInfo -lLLVMCBackend -lLLVMCBackendInfo -lLLVMBitWriter -lLLVMX86Disassembler -lLLVMX86AsmParser -lLLVMX86AsmPrinter -lLLVMX86CodeGen -lLLVMX86Info -lLLVMAsmParser -lLLVMArchive -lLLVMBitReader -lLLVMMCParser -lLLVMSelectionDAG -lLLVMCodeGen -lLLVMScalarOpts -lLLVMInstCombine -lLLVMTransformUtils -lLLVMipa -lLLVMAnalysis -lLLVMTarget -lLLVMMC -lLLVMCore -lLLVMAlphaInfo -lLLVMSupport -lLLVMSystem -lLLVMAsmPrinter -ldl
ifdef USE_GC
LIBS += -L../ifa -lifa_gc -L../plib -lplib_gc -lgc 
IFALIB = ../ifa/libifa_gc.a
else
LIBS += -L../ifa -lifa -L../plib -lplib
IFALIB = ../ifa/libifa.a
endif

ifeq ($(OS_TYPE),CYGWIN)
  LIBS += -L/usr/lib/python2.6/config -lpython2.6.dll
else
  LIBS += -lpython2.6 -lutil
endif

AUX_FILES = $(MODULE)/index.html $(MODULE)/manual.html $(MODULE)/faq.html $(MODULE)/pyc.1 $(MODULE)/pyc.cat

LIB_SRCS = lib/builtin.cpp $(wildcard lib/*.cpp) $(wildcard lib/os/*.cpp)
LIB_OBJS = $(LIB_SRCS:%.cpp=%.o)

PYC_SRCS = pyc.cc python_ifa.cc llvm.cc version.cc
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

$(PYC): $(PYC_OBJS) $(LIB_OBJS) $(LIBRARIES) $(IFALIB)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^ $(LIBS) 

pyc.cat: pyc.1
	rm -f pyc.cat
	nroff -man pyc.1 | sed -e 's/.//g' > pyc.cat

pyc.o: LICENSE.i COPYRIGHT.i

version.o: Makefile


-include .depend
# DO NOT DELETE THIS LINE -- mkdep uses it.
# DO NOT PUT ANYTHING AFTER THIS LINE, IT WILL GO AWAY.

pyc.o: pyc.cc defs.h /usr/include/python2.6/Python.h \
 /usr/include/python2.6/patchlevel.h /usr/include/python2.6/pyconfig.h \
 /usr/include/python2.6/pyconfig-64.h \
 /usr/include/python2.6/pymacconfig.h /usr/include/python2.6/pyport.h \
 /usr/include/python2.6/pymath.h /usr/include/python2.6/pymem.h \
 /usr/include/python2.6/object.h /usr/include/python2.6/objimpl.h \
 /usr/include/python2.6/pydebug.h /usr/include/python2.6/unicodeobject.h \
 /usr/include/python2.6/intobject.h /usr/include/python2.6/boolobject.h \
 /usr/include/python2.6/longobject.h /usr/include/python2.6/floatobject.h \
 /usr/include/python2.6/complexobject.h \
 /usr/include/python2.6/rangeobject.h \
 /usr/include/python2.6/stringobject.h \
 /usr/include/python2.6/bufferobject.h \
 /usr/include/python2.6/bytesobject.h \
 /usr/include/python2.6/bytearrayobject.h \
 /usr/include/python2.6/tupleobject.h /usr/include/python2.6/listobject.h \
 /usr/include/python2.6/dictobject.h /usr/include/python2.6/enumobject.h \
 /usr/include/python2.6/setobject.h /usr/include/python2.6/methodobject.h \
 /usr/include/python2.6/moduleobject.h \
 /usr/include/python2.6/funcobject.h /usr/include/python2.6/classobject.h \
 /usr/include/python2.6/fileobject.h /usr/include/python2.6/cobject.h \
 /usr/include/python2.6/traceback.h /usr/include/python2.6/sliceobject.h \
 /usr/include/python2.6/cellobject.h /usr/include/python2.6/iterobject.h \
 /usr/include/python2.6/genobject.h /usr/include/python2.6/descrobject.h \
 /usr/include/python2.6/warnings.h /usr/include/python2.6/weakrefobject.h \
 /usr/include/python2.6/codecs.h /usr/include/python2.6/pyerrors.h \
 /usr/include/python2.6/pystate.h /usr/include/python2.6/pyarena.h \
 /usr/include/python2.6/modsupport.h /usr/include/python2.6/pythonrun.h \
 /usr/include/python2.6/ceval.h /usr/include/python2.6/sysmodule.h \
 /usr/include/python2.6/intrcheck.h /usr/include/python2.6/import.h \
 /usr/include/python2.6/abstract.h /usr/include/python2.6/compile.h \
 /usr/include/python2.6/code.h /usr/include/python2.6/eval.h \
 /usr/include/python2.6/pystrtod.h /usr/include/python2.6/pystrcmp.h \
 /usr/include/python2.6/pyfpe.h /usr/include/python2.6/Python-ast.h \
 /usr/include/python2.6/asdl.h /usr/include/python2.6/symtable.h \
 ../plib/plib.h ../plib/arg.h ../plib/barrier.h ../plib/config.h \
 ../plib/stat.h ../plib/dlmalloc.h ../plib/freelist.h ../plib/defalloc.h \
 ../plib/list.h ../plib/log.h ../plib/vec.h ../plib/map.h \
 ../plib/threadpool.h ../plib/misc.h ../plib/util.h ../plib/conn.h \
 ../plib/md5.h ../plib/mt64.h ../plib/hash.h ../plib/persist.h \
 ../plib/prime.h ../plib/service.h ../plib/timer.h ../plib/unit.h \
 ../ifa/ifa.h ../ifa/ifadefs.h ../ifa/ast.h ../ifa/ifa.h ../ifa/ifalog.h \
 ../ifa/if1.h ../ifa/sym.h ../ifa/num.h ../ifa/prim_data.h ../ifa/code.h \
 ../ifa/builtin.h ../ifa/builtin_symbols.h ../ifa/fail.h ../ifa/fa.h \
 ../ifa/prim.h ../ifa/var.h ../ifa/pnode.h ../ifa/fun.h ../ifa/pdb.h \
 ../ifa/clone.h ../ifa/cg.h ../ifa/fa.h ../ifa/prim.h python_ifa.h \
 ../ifa/pattern.h COPYRIGHT.i LICENSE.i
python_ifa.o: python_ifa.cc defs.h /usr/include/python2.6/Python.h \
 /usr/include/python2.6/patchlevel.h /usr/include/python2.6/pyconfig.h \
 /usr/include/python2.6/pyconfig-64.h \
 /usr/include/python2.6/pymacconfig.h /usr/include/python2.6/pyport.h \
 /usr/include/python2.6/pymath.h /usr/include/python2.6/pymem.h \
 /usr/include/python2.6/object.h /usr/include/python2.6/objimpl.h \
 /usr/include/python2.6/pydebug.h /usr/include/python2.6/unicodeobject.h \
 /usr/include/python2.6/intobject.h /usr/include/python2.6/boolobject.h \
 /usr/include/python2.6/longobject.h /usr/include/python2.6/floatobject.h \
 /usr/include/python2.6/complexobject.h \
 /usr/include/python2.6/rangeobject.h \
 /usr/include/python2.6/stringobject.h \
 /usr/include/python2.6/bufferobject.h \
 /usr/include/python2.6/bytesobject.h \
 /usr/include/python2.6/bytearrayobject.h \
 /usr/include/python2.6/tupleobject.h /usr/include/python2.6/listobject.h \
 /usr/include/python2.6/dictobject.h /usr/include/python2.6/enumobject.h \
 /usr/include/python2.6/setobject.h /usr/include/python2.6/methodobject.h \
 /usr/include/python2.6/moduleobject.h \
 /usr/include/python2.6/funcobject.h /usr/include/python2.6/classobject.h \
 /usr/include/python2.6/fileobject.h /usr/include/python2.6/cobject.h \
 /usr/include/python2.6/traceback.h /usr/include/python2.6/sliceobject.h \
 /usr/include/python2.6/cellobject.h /usr/include/python2.6/iterobject.h \
 /usr/include/python2.6/genobject.h /usr/include/python2.6/descrobject.h \
 /usr/include/python2.6/warnings.h /usr/include/python2.6/weakrefobject.h \
 /usr/include/python2.6/codecs.h /usr/include/python2.6/pyerrors.h \
 /usr/include/python2.6/pystate.h /usr/include/python2.6/pyarena.h \
 /usr/include/python2.6/modsupport.h /usr/include/python2.6/pythonrun.h \
 /usr/include/python2.6/ceval.h /usr/include/python2.6/sysmodule.h \
 /usr/include/python2.6/intrcheck.h /usr/include/python2.6/import.h \
 /usr/include/python2.6/abstract.h /usr/include/python2.6/compile.h \
 /usr/include/python2.6/code.h /usr/include/python2.6/eval.h \
 /usr/include/python2.6/pystrtod.h /usr/include/python2.6/pystrcmp.h \
 /usr/include/python2.6/pyfpe.h /usr/include/python2.6/Python-ast.h \
 /usr/include/python2.6/asdl.h /usr/include/python2.6/symtable.h \
 ../plib/plib.h ../plib/arg.h ../plib/barrier.h ../plib/config.h \
 ../plib/stat.h ../plib/dlmalloc.h ../plib/freelist.h ../plib/defalloc.h \
 ../plib/list.h ../plib/log.h ../plib/vec.h ../plib/map.h \
 ../plib/threadpool.h ../plib/misc.h ../plib/util.h ../plib/conn.h \
 ../plib/md5.h ../plib/mt64.h ../plib/hash.h ../plib/persist.h \
 ../plib/prime.h ../plib/service.h ../plib/timer.h ../plib/unit.h \
 ../ifa/ifa.h ../ifa/ifadefs.h ../ifa/ast.h ../ifa/ifa.h ../ifa/ifalog.h \
 ../ifa/if1.h ../ifa/sym.h ../ifa/num.h ../ifa/prim_data.h ../ifa/code.h \
 ../ifa/builtin.h ../ifa/builtin_symbols.h ../ifa/fail.h ../ifa/fa.h \
 ../ifa/prim.h ../ifa/var.h ../ifa/pnode.h ../ifa/fun.h ../ifa/pdb.h \
 ../ifa/clone.h ../ifa/cg.h ../ifa/fa.h ../ifa/prim.h python_ifa.h \
 ../ifa/pattern.h
version.o: version.cc defs.h /usr/include/python2.6/Python.h \
 /usr/include/python2.6/patchlevel.h /usr/include/python2.6/pyconfig.h \
 /usr/include/python2.6/pyconfig-64.h \
 /usr/include/python2.6/pymacconfig.h /usr/include/python2.6/pyport.h \
 /usr/include/python2.6/pymath.h /usr/include/python2.6/pymem.h \
 /usr/include/python2.6/object.h /usr/include/python2.6/objimpl.h \
 /usr/include/python2.6/pydebug.h /usr/include/python2.6/unicodeobject.h \
 /usr/include/python2.6/intobject.h /usr/include/python2.6/boolobject.h \
 /usr/include/python2.6/longobject.h /usr/include/python2.6/floatobject.h \
 /usr/include/python2.6/complexobject.h \
 /usr/include/python2.6/rangeobject.h \
 /usr/include/python2.6/stringobject.h \
 /usr/include/python2.6/bufferobject.h \
 /usr/include/python2.6/bytesobject.h \
 /usr/include/python2.6/bytearrayobject.h \
 /usr/include/python2.6/tupleobject.h /usr/include/python2.6/listobject.h \
 /usr/include/python2.6/dictobject.h /usr/include/python2.6/enumobject.h \
 /usr/include/python2.6/setobject.h /usr/include/python2.6/methodobject.h \
 /usr/include/python2.6/moduleobject.h \
 /usr/include/python2.6/funcobject.h /usr/include/python2.6/classobject.h \
 /usr/include/python2.6/fileobject.h /usr/include/python2.6/cobject.h \
 /usr/include/python2.6/traceback.h /usr/include/python2.6/sliceobject.h \
 /usr/include/python2.6/cellobject.h /usr/include/python2.6/iterobject.h \
 /usr/include/python2.6/genobject.h /usr/include/python2.6/descrobject.h \
 /usr/include/python2.6/warnings.h /usr/include/python2.6/weakrefobject.h \
 /usr/include/python2.6/codecs.h /usr/include/python2.6/pyerrors.h \
 /usr/include/python2.6/pystate.h /usr/include/python2.6/pyarena.h \
 /usr/include/python2.6/modsupport.h /usr/include/python2.6/pythonrun.h \
 /usr/include/python2.6/ceval.h /usr/include/python2.6/sysmodule.h \
 /usr/include/python2.6/intrcheck.h /usr/include/python2.6/import.h \
 /usr/include/python2.6/abstract.h /usr/include/python2.6/compile.h \
 /usr/include/python2.6/code.h /usr/include/python2.6/eval.h \
 /usr/include/python2.6/pystrtod.h /usr/include/python2.6/pystrcmp.h \
 /usr/include/python2.6/pyfpe.h /usr/include/python2.6/Python-ast.h \
 /usr/include/python2.6/asdl.h /usr/include/python2.6/symtable.h \
 ../plib/plib.h ../plib/arg.h ../plib/barrier.h ../plib/config.h \
 ../plib/stat.h ../plib/dlmalloc.h ../plib/freelist.h ../plib/defalloc.h \
 ../plib/list.h ../plib/log.h ../plib/vec.h ../plib/map.h \
 ../plib/threadpool.h ../plib/misc.h ../plib/util.h ../plib/conn.h \
 ../plib/md5.h ../plib/mt64.h ../plib/hash.h ../plib/persist.h \
 ../plib/prime.h ../plib/service.h ../plib/timer.h ../plib/unit.h \
 ../ifa/ifa.h ../ifa/ifadefs.h ../ifa/ast.h ../ifa/ifa.h ../ifa/ifalog.h \
 ../ifa/if1.h ../ifa/sym.h ../ifa/num.h ../ifa/prim_data.h ../ifa/code.h \
 ../ifa/builtin.h ../ifa/builtin_symbols.h ../ifa/fail.h ../ifa/fa.h \
 ../ifa/prim.h ../ifa/var.h ../ifa/pnode.h ../ifa/fun.h ../ifa/pdb.h \
 ../ifa/clone.h ../ifa/cg.h ../ifa/fa.h ../ifa/prim.h python_ifa.h \
 ../ifa/pattern.h
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
fnmatch.o: lib/fnmatch.cpp lib/fnmatch.hpp lib/builtin.hpp \
 lib/os/path.hpp lib/builtin.hpp lib/os/__init__.hpp lib/stat.hpp \
 lib/os/__init__.hpp lib/re.hpp
getopt.o: lib/getopt.cpp lib/getopt.hpp lib/builtin.hpp lib/sys.hpp \
 lib/os/__init__.hpp lib/builtin.hpp
glob.o: lib/glob.cpp lib/glob.hpp lib/builtin.hpp lib/os/path.hpp \
 lib/builtin.hpp lib/os/__init__.hpp lib/stat.hpp lib/fnmatch.hpp \
 lib/os/__init__.hpp lib/re.hpp
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
__init__.o: lib/os/__init__.cpp lib/os/__init__.hpp lib/builtin.hpp \
 lib/os/path.hpp lib/os/__init__.hpp lib/stat.hpp lib/builtin.hpp
path.o: lib/os/path.cpp lib/os/path.hpp lib/builtin.hpp \
 lib/os/__init__.hpp lib/stat.hpp lib/builtin.hpp

# IF YOU PUT ANYTHING HERE IT WILL GO AWAY

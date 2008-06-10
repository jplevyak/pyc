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

CFLAGS += -I../plib -I../ifalib -I/usr/include/python2.5
LIBS += -L../plib -lplib_gc -L../ifalib -lifa -lpython2.5 

AUX_FILES = $(MODULE)/index.html $(MODULE)/manual.html $(MODULE)/faq.html $(MODULE)/pyc.1 $(MODULE)/pyc.cat

LIB_SRCS = 
LIB_OBJS = $(LIB_SRCS:%.cc=%.o)

PYC_SRCS = pyc.cc python_ifa.cc python_ifa.h python_ops.h version.cc
PYC_OBJS = $(PYC_SRCS:%.cc=%.o)

EXECUTABLE_FILES = pyc
LIBRARIES = 
#INSTALL_LIBRARIES = 
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

$(PYC): $(PYC_OBJS) $(LIB_OBJS) $(LIBRARIES)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^ $(LIBS) 

pyc.cat: pyc.1
	rm -f pyc.cat
	nroff -man pyc.1 | sed -e 's/.//g' > pyc.cat

pyc.o: LICENSE.i COPYRIGHT.i

version.o: Makefile

-include .depend
# DO NOT DELETE THIS LINE -- mkdep uses it.
# DO NOT PUT ANYTHING AFTER THIS LINE, IT WILL GO AWAY.

pyc.o: pyc.cc defs.h /usr/include/python2.5/Python.h \
  /usr/include/python2.5/patchlevel.h /usr/include/python2.5/pyconfig.h \
  /usr/include/python2.5/pyconfig-64.h /usr/include/python2.5/pyport.h \
  /usr/include/python2.5/pymem.h /usr/include/python2.5/object.h \
  /usr/include/python2.5/objimpl.h /usr/include/python2.5/pydebug.h \
  /usr/include/python2.5/unicodeobject.h \
  /usr/include/python2.5/intobject.h /usr/include/python2.5/boolobject.h \
  /usr/include/python2.5/longobject.h \
  /usr/include/python2.5/floatobject.h \
  /usr/include/python2.5/complexobject.h \
  /usr/include/python2.5/rangeobject.h \
  /usr/include/python2.5/stringobject.h \
  /usr/include/python2.5/bufferobject.h \
  /usr/include/python2.5/tupleobject.h \
  /usr/include/python2.5/listobject.h /usr/include/python2.5/dictobject.h \
  /usr/include/python2.5/enumobject.h /usr/include/python2.5/setobject.h \
  /usr/include/python2.5/methodobject.h \
  /usr/include/python2.5/moduleobject.h \
  /usr/include/python2.5/funcobject.h \
  /usr/include/python2.5/classobject.h \
  /usr/include/python2.5/fileobject.h /usr/include/python2.5/cobject.h \
  /usr/include/python2.5/traceback.h /usr/include/python2.5/sliceobject.h \
  /usr/include/python2.5/cellobject.h /usr/include/python2.5/iterobject.h \
  /usr/include/python2.5/genobject.h /usr/include/python2.5/descrobject.h \
  /usr/include/python2.5/weakrefobject.h /usr/include/python2.5/codecs.h \
  /usr/include/python2.5/pyerrors.h /usr/include/python2.5/pystate.h \
  /usr/include/python2.5/pyarena.h /usr/include/python2.5/modsupport.h \
  /usr/include/python2.5/pythonrun.h /usr/include/python2.5/ceval.h \
  /usr/include/python2.5/sysmodule.h /usr/include/python2.5/intrcheck.h \
  /usr/include/python2.5/import.h /usr/include/python2.5/abstract.h \
  /usr/include/python2.5/compile.h /usr/include/python2.5/code.h \
  /usr/include/python2.5/eval.h /usr/include/python2.5/pystrtod.h \
  /usr/include/python2.5/pyfpe.h /usr/include/python2.5/Python-ast.h \
  /usr/include/python2.5/asdl.h /usr/include/python2.5/symtable.h \
  ../plib/plib.h ../plib/arg.h ../plib/barrier.h ../plib/config.h \
  ../plib/freelist.h ../plib/defalloc.h ../plib/list.h ../plib/log.h \
  ../plib/vec.h ../plib/map.h ../plib/threadpool.h ../plib/misc.h \
  ../plib/util.h ../plib/conn.h ../plib/md5.h ../plib/mt64.h \
  ../plib/prime.h ../plib/service.h ../plib/unit.h ../ifalib/ifa.h \
  ../plib/plib.h ../ifalib/ifadefs.h ../ifalib/ast.h ../ifalib/ifa.h \
  ../ifalib/ifalog.h ../ifalib/if1.h ../ifalib/sym.h ../ifalib/num.h \
  ../ifalib/prim_data.h ../ifalib/code.h ../ifalib/builtin.h \
  ../ifalib/builtin_symbols.h ../ifalib/fail.h ../ifalib/fa.h \
  ../ifalib/var.h ../ifalib/pnode.h ../ifalib/region.h ../ifalib/fun.h \
  ../ifalib/pdb.h ../ifalib/clone.h ../ifalib/cg.h ../ifalib/fa.h \
  ../ifalib/prim.h python_ifa.h COPYRIGHT.i LICENSE.i
python_ifa.o: python_ifa.cc defs.h /usr/include/python2.5/Python.h \
  /usr/include/python2.5/patchlevel.h /usr/include/python2.5/pyconfig.h \
  /usr/include/python2.5/pyconfig-64.h /usr/include/python2.5/pyport.h \
  /usr/include/python2.5/pymem.h /usr/include/python2.5/object.h \
  /usr/include/python2.5/objimpl.h /usr/include/python2.5/pydebug.h \
  /usr/include/python2.5/unicodeobject.h \
  /usr/include/python2.5/intobject.h /usr/include/python2.5/boolobject.h \
  /usr/include/python2.5/longobject.h \
  /usr/include/python2.5/floatobject.h \
  /usr/include/python2.5/complexobject.h \
  /usr/include/python2.5/rangeobject.h \
  /usr/include/python2.5/stringobject.h \
  /usr/include/python2.5/bufferobject.h \
  /usr/include/python2.5/tupleobject.h \
  /usr/include/python2.5/listobject.h /usr/include/python2.5/dictobject.h \
  /usr/include/python2.5/enumobject.h /usr/include/python2.5/setobject.h \
  /usr/include/python2.5/methodobject.h \
  /usr/include/python2.5/moduleobject.h \
  /usr/include/python2.5/funcobject.h \
  /usr/include/python2.5/classobject.h \
  /usr/include/python2.5/fileobject.h /usr/include/python2.5/cobject.h \
  /usr/include/python2.5/traceback.h /usr/include/python2.5/sliceobject.h \
  /usr/include/python2.5/cellobject.h /usr/include/python2.5/iterobject.h \
  /usr/include/python2.5/genobject.h /usr/include/python2.5/descrobject.h \
  /usr/include/python2.5/weakrefobject.h /usr/include/python2.5/codecs.h \
  /usr/include/python2.5/pyerrors.h /usr/include/python2.5/pystate.h \
  /usr/include/python2.5/pyarena.h /usr/include/python2.5/modsupport.h \
  /usr/include/python2.5/pythonrun.h /usr/include/python2.5/ceval.h \
  /usr/include/python2.5/sysmodule.h /usr/include/python2.5/intrcheck.h \
  /usr/include/python2.5/import.h /usr/include/python2.5/abstract.h \
  /usr/include/python2.5/compile.h /usr/include/python2.5/code.h \
  /usr/include/python2.5/eval.h /usr/include/python2.5/pystrtod.h \
  /usr/include/python2.5/pyfpe.h /usr/include/python2.5/Python-ast.h \
  /usr/include/python2.5/asdl.h /usr/include/python2.5/symtable.h \
  ../plib/plib.h ../plib/arg.h ../plib/barrier.h ../plib/config.h \
  ../plib/freelist.h ../plib/defalloc.h ../plib/list.h ../plib/log.h \
  ../plib/vec.h ../plib/map.h ../plib/threadpool.h ../plib/misc.h \
  ../plib/util.h ../plib/conn.h ../plib/md5.h ../plib/mt64.h \
  ../plib/prime.h ../plib/service.h ../plib/unit.h ../ifalib/ifa.h \
  ../plib/plib.h ../ifalib/ifadefs.h ../ifalib/ast.h ../ifalib/ifa.h \
  ../ifalib/ifalog.h ../ifalib/if1.h ../ifalib/sym.h ../ifalib/num.h \
  ../ifalib/prim_data.h ../ifalib/code.h ../ifalib/builtin.h \
  ../ifalib/builtin_symbols.h ../ifalib/fail.h ../ifalib/fa.h \
  ../ifalib/var.h ../ifalib/pnode.h ../ifalib/region.h ../ifalib/fun.h \
  ../ifalib/pdb.h ../ifalib/clone.h ../ifalib/cg.h ../ifalib/fa.h \
  ../ifalib/prim.h python_ifa.h python_ops.h
python_ifa.o: python_ifa.h defs.h /usr/include/python2.5/Python.h \
  /usr/include/python2.5/patchlevel.h /usr/include/python2.5/pyconfig.h \
  /usr/include/python2.5/pyconfig-64.h /usr/include/python2.5/pyport.h \
  /usr/include/python2.5/pymem.h /usr/include/python2.5/object.h \
  /usr/include/python2.5/objimpl.h /usr/include/python2.5/pydebug.h \
  /usr/include/python2.5/unicodeobject.h \
  /usr/include/python2.5/intobject.h /usr/include/python2.5/boolobject.h \
  /usr/include/python2.5/longobject.h \
  /usr/include/python2.5/floatobject.h \
  /usr/include/python2.5/complexobject.h \
  /usr/include/python2.5/rangeobject.h \
  /usr/include/python2.5/stringobject.h \
  /usr/include/python2.5/bufferobject.h \
  /usr/include/python2.5/tupleobject.h \
  /usr/include/python2.5/listobject.h /usr/include/python2.5/dictobject.h \
  /usr/include/python2.5/enumobject.h /usr/include/python2.5/setobject.h \
  /usr/include/python2.5/methodobject.h \
  /usr/include/python2.5/moduleobject.h \
  /usr/include/python2.5/funcobject.h \
  /usr/include/python2.5/classobject.h \
  /usr/include/python2.5/fileobject.h /usr/include/python2.5/cobject.h \
  /usr/include/python2.5/traceback.h /usr/include/python2.5/sliceobject.h \
  /usr/include/python2.5/cellobject.h /usr/include/python2.5/iterobject.h \
  /usr/include/python2.5/genobject.h /usr/include/python2.5/descrobject.h \
  /usr/include/python2.5/weakrefobject.h /usr/include/python2.5/codecs.h \
  /usr/include/python2.5/pyerrors.h /usr/include/python2.5/pystate.h \
  /usr/include/python2.5/pyarena.h /usr/include/python2.5/modsupport.h \
  /usr/include/python2.5/pythonrun.h /usr/include/python2.5/ceval.h \
  /usr/include/python2.5/sysmodule.h /usr/include/python2.5/intrcheck.h \
  /usr/include/python2.5/import.h /usr/include/python2.5/abstract.h \
  /usr/include/python2.5/compile.h /usr/include/python2.5/code.h \
  /usr/include/python2.5/eval.h /usr/include/python2.5/pystrtod.h \
  /usr/include/python2.5/pyfpe.h /usr/include/python2.5/Python-ast.h \
  /usr/include/python2.5/asdl.h /usr/include/python2.5/symtable.h \
  ../plib/plib.h ../plib/arg.h ../plib/barrier.h ../plib/config.h \
  ../plib/freelist.h ../plib/defalloc.h ../plib/list.h ../plib/log.h \
  ../plib/vec.h ../plib/map.h ../plib/threadpool.h ../plib/misc.h \
  ../plib/util.h ../plib/conn.h ../plib/md5.h ../plib/mt64.h \
  ../plib/prime.h ../plib/service.h ../plib/unit.h ../ifalib/ifa.h \
  ../plib/plib.h ../ifalib/ifadefs.h ../ifalib/ast.h ../ifalib/ifa.h \
  ../ifalib/ifalog.h ../ifalib/if1.h ../ifalib/sym.h ../ifalib/num.h \
  ../ifalib/prim_data.h ../ifalib/code.h ../ifalib/builtin.h \
  ../ifalib/builtin_symbols.h ../ifalib/fail.h ../ifalib/fa.h \
  ../ifalib/var.h ../ifalib/pnode.h ../ifalib/region.h ../ifalib/fun.h \
  ../ifalib/pdb.h ../ifalib/clone.h ../ifalib/cg.h ../ifalib/fa.h \
  ../ifalib/prim.h python_ifa.h
python_ops.o: python_ops.h
version.o: version.cc defs.h /usr/include/python2.5/Python.h \
  /usr/include/python2.5/patchlevel.h /usr/include/python2.5/pyconfig.h \
  /usr/include/python2.5/pyconfig-64.h /usr/include/python2.5/pyport.h \
  /usr/include/python2.5/pymem.h /usr/include/python2.5/object.h \
  /usr/include/python2.5/objimpl.h /usr/include/python2.5/pydebug.h \
  /usr/include/python2.5/unicodeobject.h \
  /usr/include/python2.5/intobject.h /usr/include/python2.5/boolobject.h \
  /usr/include/python2.5/longobject.h \
  /usr/include/python2.5/floatobject.h \
  /usr/include/python2.5/complexobject.h \
  /usr/include/python2.5/rangeobject.h \
  /usr/include/python2.5/stringobject.h \
  /usr/include/python2.5/bufferobject.h \
  /usr/include/python2.5/tupleobject.h \
  /usr/include/python2.5/listobject.h /usr/include/python2.5/dictobject.h \
  /usr/include/python2.5/enumobject.h /usr/include/python2.5/setobject.h \
  /usr/include/python2.5/methodobject.h \
  /usr/include/python2.5/moduleobject.h \
  /usr/include/python2.5/funcobject.h \
  /usr/include/python2.5/classobject.h \
  /usr/include/python2.5/fileobject.h /usr/include/python2.5/cobject.h \
  /usr/include/python2.5/traceback.h /usr/include/python2.5/sliceobject.h \
  /usr/include/python2.5/cellobject.h /usr/include/python2.5/iterobject.h \
  /usr/include/python2.5/genobject.h /usr/include/python2.5/descrobject.h \
  /usr/include/python2.5/weakrefobject.h /usr/include/python2.5/codecs.h \
  /usr/include/python2.5/pyerrors.h /usr/include/python2.5/pystate.h \
  /usr/include/python2.5/pyarena.h /usr/include/python2.5/modsupport.h \
  /usr/include/python2.5/pythonrun.h /usr/include/python2.5/ceval.h \
  /usr/include/python2.5/sysmodule.h /usr/include/python2.5/intrcheck.h \
  /usr/include/python2.5/import.h /usr/include/python2.5/abstract.h \
  /usr/include/python2.5/compile.h /usr/include/python2.5/code.h \
  /usr/include/python2.5/eval.h /usr/include/python2.5/pystrtod.h \
  /usr/include/python2.5/pyfpe.h /usr/include/python2.5/Python-ast.h \
  /usr/include/python2.5/asdl.h /usr/include/python2.5/symtable.h \
  ../plib/plib.h ../plib/arg.h ../plib/barrier.h ../plib/config.h \
  ../plib/freelist.h ../plib/defalloc.h ../plib/list.h ../plib/log.h \
  ../plib/vec.h ../plib/map.h ../plib/threadpool.h ../plib/misc.h \
  ../plib/util.h ../plib/conn.h ../plib/md5.h ../plib/mt64.h \
  ../plib/prime.h ../plib/service.h ../plib/unit.h ../ifalib/ifa.h \
  ../plib/plib.h ../ifalib/ifadefs.h ../ifalib/ast.h ../ifalib/ifa.h \
  ../ifalib/ifalog.h ../ifalib/if1.h ../ifalib/sym.h ../ifalib/num.h \
  ../ifalib/prim_data.h ../ifalib/code.h ../ifalib/builtin.h \
  ../ifalib/builtin_symbols.h ../ifalib/fail.h ../ifalib/fa.h \
  ../ifalib/var.h ../ifalib/pnode.h ../ifalib/region.h ../ifalib/fun.h \
  ../ifalib/pdb.h ../ifalib/clone.h ../ifalib/cg.h ../ifalib/fa.h \
  ../ifalib/prim.h python_ifa.h

# IF YOU PUT ANYTHING HERE IT WILL GO AWAY

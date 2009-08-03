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

CFLAGS += -D__PYC__=1 -I../plib -I../ifalib -I/usr/include/python2.5 -Ilib -Ilib/os
LIBS += -lpcre 
ifdef USE_GC
LIBS += -L../ifalib -lifa_gc -L../plib -lplib_gc -lgc 
IFALIB = ../ifalib/libifa_gc.a
else
LIBS += -L../ifalib -lifa -L../plib -lplib
IFALIB = ../ifalib/libifa.a
endif

ifeq ($(OS_TYPE),CYGWIN)
  LIBS += -L/usr/lib/python2.5/config -lpython2.5.dll
else
  LIBS += -lpython2.5
endif

AUX_FILES = $(MODULE)/index.html $(MODULE)/manual.html $(MODULE)/faq.html $(MODULE)/pyc.1 $(MODULE)/pyc.cat

LIB_SRCS = lib/builtin.cpp $(wildcard lib/*.cpp) $(wildcard lib/os/*.cpp)
LIB_OBJS = $(LIB_SRCS:%.cpp=%.o)

PYC_SRCS = pyc.cc python_ifa.cc c_codegen.cc version.cc
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

test:
	./pyc_tests


-include .depend
# DO NOT DELETE THIS LINE -- mkdep uses it.
# DO NOT PUT ANYTHING AFTER THIS LINE, IT WILL GO AWAY.

bisect.o: lib/bisect.cpp lib/bisect.hpp lib/builtin.hpp
cStringIO.o: lib/cStringIO.cpp lib/cStringIO.hpp lib/builtin.hpp
collections.o: lib/collections.cpp lib/collections.hpp lib/builtin.hpp
copy.o: lib/copy.cpp lib/copy.hpp lib/builtin.hpp
datetime.o: lib/datetime.cpp lib/datetime.hpp lib/builtin.hpp \
  lib/time.hpp lib/string.hpp
getopt.o: lib/getopt.cpp lib/getopt.hpp lib/builtin.hpp lib/sys.hpp \
  lib/os/__init__.hpp lib/builtin.hpp
math.o: lib/math.cpp lib/math.hpp lib/builtin.hpp
random.o: lib/random.cpp lib/random.hpp lib/builtin.hpp lib/math.hpp \
  lib/time.hpp
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

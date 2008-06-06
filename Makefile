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

PYC_SRCS = pyc.cc version.cc
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

pyc.o: pyc.cc defs.h ../plib/plib.h ../plib/arg.h ../plib/barrier.h \
  ../plib/config.h ../plib/freelist.h ../plib/defalloc.h ../plib/list.h \
  ../plib/log.h ../plib/vec.h ../plib/map.h ../plib/threadpool.h \
  ../plib/misc.h ../plib/util.h ../plib/conn.h ../plib/md5.h \
  ../plib/mt64.h ../plib/prime.h ../plib/service.h ../plib/unit.h \
  COPYRIGHT.i LICENSE.i
version.o: version.cc defs.h ../plib/plib.h ../plib/arg.h \
  ../plib/barrier.h ../plib/config.h ../plib/freelist.h \
  ../plib/defalloc.h ../plib/list.h ../plib/log.h ../plib/vec.h \
  ../plib/map.h ../plib/threadpool.h ../plib/misc.h ../plib/util.h \
  ../plib/conn.h ../plib/md5.h ../plib/mt64.h ../plib/prime.h \
  ../plib/service.h ../plib/unit.h

# IF YOU PUT ANYTHING HERE IT WILL GO AWAY

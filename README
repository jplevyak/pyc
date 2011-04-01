BUILD

  You need to pull some other software

  1. plib

     This is expected to exist in ../plib.

        git clone git://github.com/jplevyak/plib.git
	cd plib
        make USE_GC=1

  2. ifa

     This is expected to exist in ../ifa.

        git clone git://ifa.git.sourceforge.net/gitroot/ifa/ifa
	cd ifa
	make


  3. dparser
   
     This just need to build and install this. Note: You should enable the
     gc garbage collector (it's an option in the Makefile, or provide as
     an option to make:

        git clone git://dparser.git.sourceforge.net/gitroot/dparser/dparser
        make D_USE_GC=1


  4. the Boehm GC:

     On Fedora:
        yum install gc gc-devel

     On Ubuntu (or other debian distros):
         apt-get install libgc libgc-dev


     Note that this installs gc include files in a different include dir
     so might not currently work.

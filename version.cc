/*
 Copyright 2002-2008 John Plevyak, All Rights Reserved
*/
#include "defs.h"

void
get_version(char *v) {
  v += sprintf(v, "%d.%d", MAJOR_VERSION, MINOR_VERSION);
  if (BUILD_VERSION)
    v += sprintf(v, ".%d", BUILD_VERSION);
}


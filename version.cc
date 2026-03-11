// SPDX-License-Identifier: BSD-3-Clause
#include "defs.h"

void get_version(char *v) {
  v += sprintf(v, "%d.%d", MAJOR_VERSION, MINOR_VERSION);
  if (strcmp("", BUILD_VERSION)) v += sprintf(v, ".%s", BUILD_VERSION);
}

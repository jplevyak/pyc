// SPDX-License-Identifier: BSD-3-Clause
#include "defs.h"

// PYC_VERSION is set by the Makefile: the release tag, or MAJOR.MINOR and
// the commit (see "Version stamp" there).
void get_version(char *v) { strcpy(v, PYC_VERSION); }

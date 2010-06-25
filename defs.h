/*
  Copyright 2003-2008 John Plevyak, All Rights Reserved
*/
#ifndef _defs_H_
#define _defs_H_

#define __STDC_LIMIT_MACROS 1

extern "C" {
#include <Python.h>
#include <Python-ast.h>
#include <symtable.h>
}

// namespace contamination from python headers
#if FREE != 4
#error fixme
#endif
#ifdef FREE
#undef FREE
#endif
#ifdef List
#undef List
#endif
#define PYTHON_FREE 4

#include "plib.h"
#include "ifa.h"
#include "fa.h"
#include "prim.h"
#include "python_ifa.h"

void get_version(char *);

EXTERN int fgraph EXTERN_INIT(0);
EXTERN int fdump_html EXTERN_INIT(0);
EXTERN int fcg EXTERN_INIT(1);
EXTERN int test_scoping EXTERN_INIT(0);
EXTERN int codegen_optimize EXTERN_INIT(0);
EXTERN int codegen_debug EXTERN_INIT(0);
EXTERN int debug_level EXTERN_INIT(0);
EXTERN int verbose_level EXTERN_INIT(0);

void c_codegen_print_c(FILE *fp, FA *fa, Fun *init);
void c_codegen_write_c(FA *fa, Fun *main, cchar *filename);
int c_codegen_compile(cchar *filename);

#endif

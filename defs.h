/*
  Copyright 2003-2008 John Plevyak, All Rights Reserved
*/
#ifndef _defs_H_
#define _defs_H_

#ifndef __STDC_LIMIT_MACROS
#define __STDC_LIMIT_MACROS 1
#endif

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
#ifdef HAVE_MMAP
#undef HAVE_MMAP
#endif
#ifdef Name
#undef Name
#endif
#ifdef alias
#undef alias
#endif
#ifdef Pass
#undef Pass
#endif
#ifdef Set
#undef Set
#endif
#ifdef Attribute
#undef Attribute
#endif
#ifdef Index
#undef Index
#endif
#ifdef Str
#undef Str
#endif
#ifdef Module
#undef Module
#endif

#include "plib.h"
#include "ifa.h"
#include "fa.h"
#include "prim.h"
#include "python_ifa.h"

void get_version(char *);

EXTERN int codegen_llvm EXTERN_INIT(0);
EXTERN int codegen_jit EXTERN_INIT(0);
EXTERN int codegen_shedskin EXTERN_INIT(0);
EXTERN bool runtime_errors EXTERN_INIT(true);
EXTERN int fgraph EXTERN_INIT(0);
EXTERN int fdump_html EXTERN_INIT(0);
EXTERN int fcg EXTERN_INIT(1);
EXTERN int test_scoping EXTERN_INIT(0);
EXTERN int debug_level EXTERN_INIT(0);
EXTERN int verbose_level EXTERN_INIT(0);

void llvm_codegen(FA *fa, Fun *main, cchar *fn);
int llvm_codegen_compile(cchar *fn);
int shedskin_codegen(FA *fa, Fun *main, cchar *fn);

#endif

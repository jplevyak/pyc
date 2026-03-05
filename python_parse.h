#pragma once
/*
  Copyright 2008-2011 John Plevyak, All Rights Reserved
  Python DParser integration: shared types and parse API.
*/
#include "parse_structs.h"

struct PythonGlobals : public Globals {
  IF1 *if1;
  int errors;
  int indent_stack[1024];
  int *current_indent;
  int implicit_line_joining;
};

// Parse a Python source file with DParser.
// Returns 0 on success, -1 on parse error.
int dparse_python_file(const char *filename);

// SPDX-License-Identifier: BSD-3-Clause
// Python DParser integration: shared types and parse API.
#pragma once
#include "parse_structs.h"  // defines D_ParseNode_User=ParseNode, Globals

#include "python_ast.h"     // defines PyDAST (includes common.h)

// Override D_ParseNode_User so Python grammar actions store PyDAST*
#undef D_ParseNode_User
struct PyParseNode { PyDAST *ast; };
#define D_ParseNode_User PyParseNode

struct PythonGlobals : public Globals {
  IF1 *if1;
  int errors;
  int indent_stack[1024];
  int *current_indent;
  int implicit_line_joining;
  PyDAST *root_ast;  // set by file_input grammar action
};

// Parse a Python source file with DParser.
// Returns 0 on success, -1 on parse error.
int dparse_python_file(const char *filename);

// Parse a Python source file and return the root PyDAST*, or null on error.
PyDAST *dparse_python_to_ast(const char *filename);

// Print a PyDAST tree for debugging.
void pyast_print(PyDAST *ast, int depth);

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
  // Indent-tracking tape for the scanner/GLR indent-dedent protocol
  // (python.g: python_whitespace pushes, python_indent/python_dedent
  // pop from *speculative* reduction code). Pushes are definitive
  // but pops ride speculation, so discarded GLR branches leak
  // entries and the high-water mark grows with file size, not
  // nesting depth -- 1024 overflowed (silently, then SIGSEGV) on
  // shedskin's othello3.py at 23k lines / ~3.3k indent transitions;
  // 2048 already sufficed. Sized generously because the cost is one
  // per-parse struct; python.g's push site fail()s cleanly if a
  // pathological input ever exhausts even this.
  int indent_stack[65536];
  int *current_indent;
  int implicit_line_joining;
  PyDAST *root_ast;  // set by file_input grammar action
};

// Parse a Python source file with DParser.
// Returns 0 on success, -1 on parse error.
int dparse_python_file(const char *filename);

// Parse a Python source file and return the root PyDAST*, or null on error.
PyDAST *dparse_python_to_ast(const char *filename);

// Parse all *.py files from a directory (sorted order) as one concatenated module.
PyDAST *dparse_builtin_dir(const char *dirname);

// Parse an in-memory Python source buffer (e.g. a synthesized snippet, not
// backed by a file) and return the root PyDAST*, or null on error. `label`
// is used only for diagnostics (parse-error messages).
PyDAST *dparse_python_buf_to_ast(const char *label, const char *buf, int len);

// Print a PyDAST tree for debugging.
void pyast_print(PyDAST *ast, int depth);

/*
  Copyright 2008-2025 John Plevyak, All Rights Reserved
  Python DParser integration: parse Python source files using python.g tables.
*/
#include "defs.h"
#include "python_parse.h"
#include "dparse.h"

extern D_ParserTables parser_tables_python;
extern int dparser_python_user_size;
extern int dparser_python_globals_size;
extern void python_whitespace(D_Parser *, d_loc_t *, void **);

static D_Parser *make_python_parser(const char *filename, const char *buf, int len) {
  D_Parser *p = new_D_Parser(&parser_tables_python, dparser_python_user_size);
  p->loc.pathname = (char *)filename;
  p->loc.line = 1;
  p->loc.col = 0;
  p->save_parse_tree = 1;
  p->initial_white_space_fn = (D_WhiteSpaceFn)python_whitespace;
  PythonGlobals *pg = (PythonGlobals *)MALLOC(dparser_python_globals_size);
  memset(pg, 0, dparser_python_globals_size);
  pg->current_indent = &pg->indent_stack[2];  // required by python_whitespace
  p->initial_globals = (Globals *)pg;
  return p;
}

int dparse_python_file(const char *filename) {
  char *buf = 0;
  int len = 0;
  if (buf_read(filename, &buf, &len) <= 0) {
    fprintf(stderr, "dparse: unable to read '%s'\n", filename);
    return -1;
  }
  D_Parser *p = make_python_parser(filename, buf, len);
  D_ParseNode *pn = dparse(p, buf, len);
  int ok = pn && !p->syntax_errors;
  if (!ok)
    fprintf(stderr, "dparse: parse error in '%s' near line %d\n", filename, p->loc.line);
  if (pn) free_D_ParseNode(p, pn);
  free_D_Parser(p);
  return ok ? 0 : -1;
}


PyDAST *dparse_python_to_ast(const char *filename) {
  char *buf = 0;
  int len = 0;
  if (buf_read(filename, &buf, &len) <= 0) {
    fprintf(stderr, "dparse: unable to read '%s'\n", filename);
    return nullptr;
  }
  D_Parser *p = make_python_parser(filename, buf, len);
  dparse(p, buf, len);
  PythonGlobals *pg = (PythonGlobals *)p->initial_globals;
  PyDAST *ast = nullptr;
  if (!p->syntax_errors)
    ast = pg->root_ast;
  else
    fprintf(stderr, "dparse: parse error in '%s' near line %d\n", filename, p->loc.line);
  free_D_Parser(p);
  return ast;
}

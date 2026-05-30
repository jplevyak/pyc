// SPDX-License-Identifier: BSD-3-Clause
// Python DParser integration: parse Python source files using python.g tables.
#include "defs.h"
#include "python_parse.h"
#include "dparse.h"
#include <string.h>

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

PyDAST *dparse_builtin_dir(const char *dirname) {
  struct dirent **namelist = nullptr;
  int n = scandir(dirname, &namelist, nullptr, alphasort);
  if (n < 0) {
    fprintf(stderr, "dparse: cannot scan directory '%s'\n", dirname);
    return nullptr;
  }
  // First pass: collect file contents
  Vec<char *> file_bufs;
  Vec<int> file_lens;
  int total = 0;
  for (int i = 0; i < n; i++) {
    const char *name = namelist[i]->d_name;
    int nlen = (int)strlen(name);
    if (nlen < 3 || strcmp(name + nlen - 3, ".py") != 0) continue;
    char path[1024];
    snprintf(path, sizeof(path), "%s/%s", dirname, name);
    char *fbuf = nullptr;
    int flen = 0;
    if (buf_read(path, &fbuf, &flen) > 0) {
      file_bufs.add(fbuf);
      file_lens.add(flen);
      total += flen + 1;  // +1 for newline separator
    }
  }
  if (!total) return nullptr;
  // Allocate GC-managed buffer (must outlive the AST since nodes reference it)
  char *buf = (char *)MALLOC(total + 2);
  int pos = 0;
  for (int i = 0; i < file_bufs.n; i++) {
    memcpy(buf + pos, file_bufs[i], file_lens[i]);
    pos += file_lens[i];
    if (pos > 0 && buf[pos - 1] != '\n') buf[pos++] = '\n';
  }
  buf[pos] = 0;
  buf[pos + 1] = 0;
  D_Parser *p = make_python_parser(dirname, buf, pos);
  dparse(p, buf, pos);
  PythonGlobals *pg = (PythonGlobals *)p->initial_globals;
  PyDAST *ast = nullptr;
  if (!p->syntax_errors)
    ast = pg->root_ast;
  else
    fprintf(stderr, "dparse: parse error in builtin dir '%s' near line %d\n", dirname, p->loc.line);
  free_D_Parser(p);
  return ast;
}

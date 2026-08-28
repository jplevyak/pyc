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

// CPython's tokenizer synthesizes a NEWLINE at end of input, so a source
// file whose last line has no '\n' is perfectly legal Python. python.g's
// `file_input: (NL | stmt)*` has no equivalent rule, so DParser reported
// "syntax error after ..." on the FINAL statement and pyc rejected a
// program CPython accepts. dparse_builtin_dir below already appends
// exactly this separator when it concatenates the builtin files; this
// does the same for a single file.
//
// buf_read's allocation is len+2 with both trailing bytes NUL (terminator
// + sentinel), so there is no room to write the newline in place -- copy.
static char *ensure_trailing_newline(char *buf, int *len) {
  if (*len > 0 && buf[*len - 1] == '\n') return buf;
  char *copy = (char *)MALLOC(*len + 3);
  memcpy(copy, buf, *len);
  copy[*len] = '\n';
  copy[*len + 1] = 0;
  copy[*len + 2] = 0;
  (*len)++;
  return copy;
}

int dparse_python_file(const char *filename) {
  char *buf = 0;
  int len = 0;
  // issues/113: `< 0`, not `<= 0`. buf_read returns -1 only when open()
  // fails; a length of 0 is a successful read of an EMPTY file, and it
  // still hands back a NUL-terminated buffer. An empty `__init__.py` is
  // both legal and the common case for a package (minilight's ml/,
  // quameon's jastrow/ and orbital/), and this rejected every one of
  // them with "unable to read".
  if (buf_read(filename, &buf, &len) < 0) {
    fprintf(stderr, "dparse: unable to read '%s'\n", filename);
    return -1;
  }
  buf = ensure_trailing_newline(buf, &len);
  D_Parser *p = make_python_parser(filename, buf, len);
  D_ParseNode *pn = dparse(p, buf, len);
  int ok = pn && !p->syntax_errors;
  if (!ok)
    fprintf(stderr, "dparse: parse error in '%s' near line %d\n", filename, p->loc.line);
  if (pn) free_D_ParseNode(p, pn);
  free_D_Parser(p);
  return ok ? 0 : -1;
}


static PyDAST *dparse_buf_to_ast_impl(const char *label, char *buf, int len) {
  buf = ensure_trailing_newline(buf, &len);
  D_Parser *p = make_python_parser(label, buf, len);
  dparse(p, buf, len);
  PythonGlobals *pg = (PythonGlobals *)p->initial_globals;
  PyDAST *ast = nullptr;
  if (!p->syntax_errors)
    ast = pg->root_ast;
  else
    fprintf(stderr, "dparse: parse error in '%s' near line %d\n", label, p->loc.line);
  free_D_Parser(p);
  return ast;
}

PyDAST *dparse_python_to_ast(const char *filename) {
  char *buf = 0;
  int len = 0;
  // See dparse_python_file above: 0 is an empty file, not a failure.
  if (buf_read(filename, &buf, &len) < 0) {
    fprintf(stderr, "dparse: unable to read '%s'\n", filename);
    return nullptr;
  }
  return dparse_buf_to_ast_impl(filename, buf, len);
}

PyDAST *dparse_python_buf_to_ast(const char *label, const char *buf, int len) {
  // GC-managed copy with trailing NUL padding (dparse's scanner may probe
  // past the nominal content length; mirrors dparse_builtin_dir's buffer
  // preparation below).
  char *copy = (char *)MALLOC(len + 2);
  memcpy(copy, buf, len);
  copy[len] = 0;
  copy[len + 1] = 0;
  return dparse_buf_to_ast_impl(label, copy, len);
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
  // AST nodes retain this pathname for error reporting (fail.cc's
  // get_file_line re-opens it to print source context). dirname itself is a
  // real directory, not a file -- open()+read() on it fails an assert
  // (EISDIR) the first time a warning inside the concatenated builtin
  // module needs to show source context. Give it a synthetic ".py" name
  // matching the one pyc.cc registers the module under instead.
  char *label = dupstrs(dirname, ".py");
  D_Parser *p = make_python_parser(label, buf, pos);
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

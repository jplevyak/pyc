/*
  Copyright 2008-2011 John Plevyak, All Rights Reserved
  Internal header shared between python_ifa_util.cc, python_ifa_sym.cc,
  python_ifa_build_syms.cc, python_ifa_build_if1.cc, and python_ifa_main.cc.
*/
#pragma once
#include "defs.h"
#include "dirent.h"
#include "python_ast.h"

#define TEST_SCOPE if (debug_level && (!test_scoping || !ctx.is_builtin()))

typedef MapElem<cchar *, PycSymbol *> MapCharPycSymbolElem;

extern int scope_id;

struct PycScope : public gc {
  int id;
  Sym *in;
  Sym *cls, *fun;
  Label *lbreak, *lcontinue, *lreturn, *lyield;
  Map<cchar *, PycSymbol *> map;
  PycScope() : in(0), cls(0), fun(0), lbreak(0), lcontinue(0), lreturn(0), lyield(0) { id = scope_id++; }
};

// -- Globals defined in python_ifa_util.cc --
extern Map<PyDAST *, PycAST *> pydmap;
extern Sym *sym_long, *sym_ellipsis, *sym_ellipsis_type, *sym_unicode, *sym_buffer, *sym_xrange, *sym_declare;
#define S(_x) extern Sym *sym_##_x;
#include "pyc_symbols.h"
extern cchar *cannonical_self;
extern int finalized_aspect;
extern Vec<Sym *> builtin_functions;

// -- PycCompiler: combines PycCallbacks + former PycContext state --

class PycCompiler : public PycCallbacks {
 public:
  // --- State (formerly PycContext) ---
  cchar *filename;
  int lineno;
  void *node;
  PycModule *mod, *package;
  Vec<PycModule *> *modules;
  Vec<cchar *> *search_path;
  Vec<PycScope *> scope_stack;
  Vec<cchar *> c_code;
  Map<void *, PycScope *> saved_scopes;
  Map<int, Sym *> tuple_types;
  Vec<PycScope *> imports;

  // --- Accessors (formerly PycContext methods) ---
  bool is_builtin() { return mod->is_builtin; }
  Sym *fun() { return scope_stack.last()->fun; }
  Sym *cls() { return scope_stack.last()->cls; }
  Label *&lbreak() { return scope_stack.last()->lbreak; }
  Label *&lcontinue() { return scope_stack.last()->lcontinue; }
  Label *&lreturn() { return scope_stack.last()->lreturn; }
  Label *&lyield() { return scope_stack.last()->lyield; }
  bool in_class() { return (cls() && scope_stack.last()->in == cls()); }

  void init();
  PycCompiler() { init(); }

  // --- PycCallbacks overrides ---
  void finalize_functions();
  Sym *new_Sym(cchar *name = 0);
  Fun *default_wrapper(Fun *, Vec<MPosition *> &defaults);
  bool reanalyze(Vec<ATypeViolation *> &type_violations);
  bool c_codegen_pre_file(FILE *);

  // --- Entry point ---
  int run(Vec<PycModule *> &mods);
};

// -- Inline helpers (each TU gets its own copy) --

static inline char *read_file_to_string(cchar *fn, uint64 n = 0, int *pfd = 0) {
  int fd = open(fn, O_RDONLY | O_NOATIME, 00660);
  if (fd < 0) fprintf(stderr, "unable to open: %s\n", fn);
  assert(fd > 0);
  if (!n) {
    n = (uint64)::lseek(fd, 0, SEEK_END);
    ::lseek(fd, 0, SEEK_SET);
  }
  char *m = (char *)MALLOC(n + 1);
  m[n] = 0;
  ssize_t nn = ::read(fd, m, n);
  if (nn != (ssize_t)n) perror("read");
  if (pfd) *pfd = fd;
  return m;
}

static inline PycAST *getAST(PyDAST *n, PycCompiler &ctx) {
  PycAST *ast = pydmap.get(n);
  if (ast) return ast;
  ast = new PycAST;
  ast->filename = n->filename ? n->filename : ctx.filename;
  ast->is_builtin = ctx.is_builtin();
  ast->xpyd = n;
  pydmap.put(n, ast);
  return ast;
}

static inline PycAST *getAST(PyDAST *n, PycAST *a) {
  PycAST *ast = pydmap.get(n);
  if (ast) return ast;
  ast = new PycAST;
  ast->filename = a->filename;
  ast->is_builtin = a->is_builtin;
  ast->xpyd = n;
  pydmap.put(n, ast);
  return ast;
}


// Scope classification
enum PYC_SCOPINGS { PYC_USE, PYC_LOCAL, PYC_GLOBAL, PYC_NONLOCAL };

// Scope marking constants (used in find_PycSymbol and build_syms)
#define EXPLICITLY_MARKED 1
#define IMPLICITLY_MARKED 2
#define GLOBAL_USE ((PycSymbol *)(intptr_t)1)
#define NONLOCAL_USE ((PycSymbol *)(intptr_t)2)
#define GLOBAL_DEF ((PycSymbol *)(intptr_t)3)
#define NONLOCAL_DEF ((PycSymbol *)(intptr_t)4)
#define MARKED(_x) (((uintptr_t)(_x)) < 5)

// -- Cross-file function declarations --

// From python_ifa_util.cc:
cchar *cannonicalize_string(cchar *s);

// From python_ifa_sym.cc:
PycSymbol *new_PycSymbol(cchar *name);
PycSymbol *new_PycSymbol(cchar *name, PycCompiler &ctx);
Sym *new_base_instance(Sym *c, PycAST *ast);
void build_builtin_symbols();
Sym *new_sym(cchar *name = 0, int global = 0);
Sym *new_sym(PycAST *ast, int global = 0);
Sym *new_sym(PycAST *ast, cchar *name, int global = 0);
Sym *new_global(PycAST *ast, cchar *name = 0);
Sym *new_fun(PycAST *ast, Sym *fun = 0);
void enter_scope(PycCompiler &ctx, Sym *in = 0);
void enter_scope(PyDAST *n, PycCompiler &ctx, Sym *in = 0);
void exit_scope(PycCompiler &ctx);
PycSymbol *find_PycSymbol(PycCompiler &ctx, cchar *name, int *level = 0, int *type = 0);
PycSymbol *make_PycSymbol(PycCompiler &ctx, cchar *n, PYC_SCOPINGS scoping);

// From python_ifa_build_syms.cc:
PycModule *get_module(cchar *name, PycCompiler &ctx);
int build_syms(PycModule *x, PycCompiler &ctx);
void scope_sym(PycCompiler &ctx, Sym *sym, cchar *name = 0);
Sym *make_string(cchar *s);
void gen_ifexpr(PycAST *ifcond, PycAST *ifif, PycAST *ifelse, PycAST *ast);
void call_method(Code **code, PycAST *ast, Sym *o, Sym *m, Sym *r, int n, ...);
Sym *make_symbol(cchar *name);
// pyda path (from python_ifa_build_syms.cc):
void get_syms_args_pyda(PycAST *ast, PyDAST *varargslist, Vec<Sym *> &has, PycCompiler &ctx);
void gen_fun_pyda(PyDAST *n, PycAST *ast, PycCompiler &ctx);
void gen_lambda_pyda(PyDAST *n, PycAST *ast, PycCompiler &ctx);
void gen_class_pyda(PyDAST *n, PycAST *ast, PycCompiler &ctx, char *vector_size = nullptr);

// From python_ifa_build_if1.cc:
int build_if1_module_pyda(PyDAST *mod, PycCompiler &ctx, Code **code);

// From python_ifa_main.cc:
void build_module_attributes_if1(PycModule *mod, PycCompiler &ctx, Code **code);
void install_new_fun(Sym *f);

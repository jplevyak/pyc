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
#define EXPR_CONTEXT_SYM ((expr_context_ty)100)

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
extern Map<stmt_ty, PycAST *> stmtmap;
extern Map<expr_ty, PycAST *> exprmap;
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
  PyArena *arena;
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
  int run(Vec<PycModule *> &mods, PyArena *arena_param);
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

static inline PycAST *getAST(stmt_ty s, PycCompiler &ctx) {
  PycAST *ast = stmtmap.get(s);
  if (ast) return ast;
  ast = new PycAST;
  ast->filename = ctx.filename;
  ast->is_builtin = ctx.is_builtin();
  ast->xstmt = s;
  stmtmap.put(s, ast);
  return ast;
}

static inline PycAST *getAST(expr_ty e, PycCompiler &ctx) {
  PycAST *ast = exprmap.get(e);
  if (ast) return ast;
  ast = new PycAST;
  ast->filename = ctx.filename;
  ast->is_builtin = ctx.is_builtin();
  ast->xexpr = e;
  exprmap.put(e, ast);
  return ast;
}

static inline PycAST *getAST(expr_ty e, PycAST *a) {
  PycAST *ast = exprmap.get(e);
  if (ast) return ast;
  ast = new PycAST;
  ast->filename = a->filename;
  ast->is_builtin = a->is_builtin;
  ast->xexpr = e;
  exprmap.put(e, ast);
  return ast;
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

static inline Sym *gen_or_default(expr_ty e, Sym *def, PycAST *ast, PycCompiler &ctx) {
  if (e) {
    PycAST *east = getAST(e, ctx);
    if1_gen(if1, &ast->code, east->code);
    return east->rval;
  } else
    return def;
}

template <class C>
static void add(asdl_seq *seq, Vec<PycAST *> &asts, PycCompiler &ctx) {
  for (int i = 0; i < asdl_seq_LEN(seq); i++) asts.add(getAST((C)asdl_seq_GET(seq, i), ctx));
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

// import_fn typedef (used in build_syms and build_if1)
typedef void import_fn(char *sym, char *as, char *from, PycCompiler &ctx);

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
void enter_scope(PycCompiler &ctx, mod_ty mod);
void enter_scope(stmt_ty x, PycAST *ast, PycCompiler &ctx);
void enter_scope(expr_ty x, PycAST *ast, PycCompiler &ctx);
void enter_scope(PyDAST *n, PycCompiler &ctx, Sym *in = 0);
void exit_scope(PycCompiler &ctx);
void exit_scope(stmt_ty x, PycCompiler &ctx);
void exit_scope(expr_ty x, PycCompiler &ctx);
int needs_scope(expr_ty x);
PycSymbol *find_PycSymbol(PycCompiler &ctx, cchar *name, int *level = 0, int *type = 0);
PycSymbol *make_PycSymbol(PycCompiler &ctx, cchar *n, PYC_SCOPINGS scoping);
PycSymbol *make_PycSymbol(PycCompiler &ctx, PyObject *o, PYC_SCOPINGS type);
void add(expr_ty e, Vec<PycAST *> &asts, PycCompiler &ctx);
void add_comprehension(asdl_seq *comp, Vec<PycAST *> &asts, PycCompiler &ctx);
void get_pre_scope_next(stmt_ty s, Vec<PycAST *> &asts, PycCompiler &ctx);
void get_next(stmt_ty s, Vec<PycAST *> &asts, PycCompiler &ctx);
void get_next(slice_ty s, Vec<PycAST *> &asts, PycCompiler &ctx);
void get_pre_scope_next(expr_ty e, Vec<PycAST *> &asts, PycCompiler &ctx);
void get_next(expr_ty e, Vec<PycAST *> &asts, PycCompiler &ctx);

// From python_ifa_build_syms.cc:
PycModule *get_module(cchar *name, PycCompiler &ctx);
int build_syms(PycModule *x, PycCompiler &ctx);
void scope_sym(PycCompiler &ctx, Sym *sym, cchar *name = 0);
Sym *make_string(cchar *s);
Sym *make_string(PyObject *o);
void get_syms_args(PycAST *a, arguments_ty args, Vec<Sym *> &has, PycCompiler &ctx, asdl_seq *decorators = 0);
void build_import(stmt_ty s, import_fn fn, PycCompiler &ctx);
void build_import_from(stmt_ty s, import_fn fn, PycCompiler &ctx);
void gen_fun(stmt_ty s, PycAST *ast, PycCompiler &ctx);
void gen_fun(expr_ty e, PycAST *ast, PycCompiler &ctx);
void gen_class(stmt_ty s, PycAST *ast, PycCompiler &ctx);
void gen_if(PycAST *ifcond, asdl_seq *ifif, asdl_seq *ifelse, PycAST *ast, PycCompiler &ctx);
void gen_ifexpr(PycAST *ifcond, PycAST *ifif, PycAST *ifelse, PycAST *ast);
int get_stmts_code(asdl_seq *stmts, Code **code, PycCompiler &ctx);
void call_method(Code **code, PycAST *ast, Sym *o, Sym *m, Sym *r, int n, ...);
Sym *make_num(PyObject *o, PycCompiler &ctx);
Sym *make_symbol(cchar *name);
Sym *map_operator(operator_ty op);
Sym *map_ioperator(operator_ty op);
Sym *map_unary_operator(unaryop_ty op);
Sym *map_cmp_operator(cmpop_ty op);
// pyda path (from python_ifa_build_syms.cc):
void get_syms_args_pyda(PycAST *ast, PyDAST *varargslist, Vec<Sym *> &has, PycCompiler &ctx);
void gen_fun_pyda(PyDAST *n, PycAST *ast, PycCompiler &ctx);
void gen_lambda_pyda(PyDAST *n, PycAST *ast, PycCompiler &ctx);
void gen_class_pyda(PyDAST *n, PycAST *ast, PycCompiler &ctx);

// From python_ifa_build_if1.cc:
int build_if1_module(mod_ty mod, PycCompiler &ctx, Code **code);
// pyda path:
int build_if1_module_pyda(PyDAST *mod, PycCompiler &ctx, Code **code);

// From python_ifa_main.cc:
void build_module_attributes_if1(PycModule *mod, PycCompiler &ctx, Code **code);
void install_new_fun(Sym *f);

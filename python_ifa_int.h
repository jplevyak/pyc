// SPDX-License-Identifier: BSD-3-Clause
// Internal header shared between python_ifa_util.cc, python_ifa_sym.cc,
// python_ifa_build_syms.cc, python_ifa_build_if1.cc, and python_ifa_main.cc.
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
  // issues/021: HashMap (content-hashed via StringHashFns) instead of the
  // plain Map (which hashes on the cchar* key's pointer value -- varies by
  // GC/heap layout between processes, making scope-map iteration order,
  // and therefore any codegen that walks a scope without an explicit sort,
  // non-reproducible across runs of byte-identical input).
  HashMap<cchar *, StringHashFns, PycSymbol *> map;
  // issues/116: set on a CLASS scope whose body defines CPython's
  // `__next__` but not pyc's `__pyc_more__`. Such a class gets the
  // __pyc_iterator__ bridge as a base, and its own `__next__` is
  // installed under `__pyc_user_next__` so the bridge's peek-then-fetch
  // pair can sit in front of it. Read by PY_funcdef while emitting the
  // method's class setter, which is why it lives on the scope.
  bool iter_bridge;
  PycScope()
      : in(0), cls(0), fun(0), lbreak(0), lcontinue(0), lreturn(0), lyield(0), iter_bridge(false) {
    id = scope_id++;
  }
};

// -- Globals defined in python_ifa_util.cc --
extern Map<PyDAST *, PycAST *> pydmap;
extern Sym *sym_ellipsis, *sym_ellipsis_type, *sym_declare;
#define S(_x) extern Sym *sym_##_x;
#include "pyc_symbols.h"
extern cchar *cannonical_self;
extern Vec<Sym *> super_aspect_syms;
extern Vec<Sym *> builtin_functions;
extern bool pyc_program_has_raise;

// -- PycCompiler: combines PycCallbacks + former PycContext state --

struct WithCleanup {
  Sym *cm_rval;
  int loop_depth;
};

// issue 011 (exception handling, option C): a lexically-enclosing
// `try` in the CURRENT function. `raise` and the post-call
// pending-exception checks jump to the innermost frame whose fun
// matches ctx.fun(); frames of enclosing FUNCTIONS are unreachable
// by goto (a raise in a nested def propagates by returning, and the
// caller's post-call check routes it here).
struct PycTryFrame {
  Label *dispatch;
  Sym *fun;
};

class PycCompiler : public PycCallbacks {
 public:
  // --- State (formerly PycContext) ---
  cchar *filename;
  int lineno;
  int loop_depth = 0;
  // issues/023/024: true while build_if1_assign_target's initial
  // build_if1_pyda(tgt, ctx) pre-builds an assignment TARGET tree
  // (populating ->code/->rval/->sym/->is_member for emit_assign_to_target
  // to read back -- the target's own "make tuple/list" SEND that pass
  // produces is thrown away, only sub-node fields matter). A target
  // tuple/list can contain a PY_star_expr (issue 024's `a, *b = ...`)
  // that build_if1_pyda's ordinary PY_list/PY_tuple case would
  // otherwise reject (issues/023's defensive check for the SAME node
  // shape used as a genuine, unsupported list/tuple-literal-unpacking
  // VALUE) -- this flag lets that check tell the two contexts apart.
  bool building_assign_target = false;
  void *node;
  PycModule *mod, *package;
  Vec<PycModule *> *modules;
  Vec<cchar *> *search_path;
  Vec<PycScope *> scope_stack;
  // issues/107: names used but bound nowhere. A PYC_USE that resolves to
  // nothing falls through to Lglobal and MINTS a module global, so an
  // undefined name silently becomes a never-assigned symbol with no type
  // -- which warns, compiles with exit 0, and segfaults at runtime. The
  // check must be DEFERRED to the end of the module, because forward
  // references (`def f(): return g()` before `def g()`) legitimately take
  // the same path: `g` is unbound when f's body is walked and bound
  // later. `pending_uses` records name -> first-use line; `bound_names`
  // records every name a real binding created. Anything left over at the
  // end of the module was never defined.
  HashMap<cchar *, StringHashFns, int> pending_uses;
  HashMap<cchar *, StringHashFns, int> bound_names;
  Vec<cchar *> pending_order;  // deterministic report order
  // issues/113: import failures, reported BEFORE the undefined-name
  // pass. An import that cannot be satisfied was silently dropped, and
  // the user saw the consequence -- "name 'entry' is not defined" --
  // rather than the cause. Accumulated as formatted lines so several
  // bad imports are all reported in one run, like undefined names.
  Vec<cchar *> import_errors;
  int in_kwarg_key = 0;        // issues/107: suppress reporting for kwarg names
  Vec<WithCleanup> with_stack;
  Vec<cchar *> c_code;
  Map<void *, PycScope *> saved_scopes;
  Map<int, Sym *> tuple_types;
  Vec<PycScope *> imports;
  // issues/007 split identity: public-name variable Sym -> internal
  // function Sym for every non-method def. Used by PY_name's load
  // path to redirect a function's own-name reference INSIDE its own
  // body to the internal Sym (value self-identity), so recursion
  // neither routes through ifa's stack-disciplined display machinery
  // nor becomes a spurious self-capture. (Known CPython divergence,
  // documented in issues/007: a recursive call inside a decorated
  // function calls the UNDECORATED function.)
  Map<Sym *, Sym *> def_internal_fn;

  // issue 025 module subsystem phase 2: for `import X`, X is bound to
  // a module-marker Sym (is_module set). This maps that marker to the
  // imported PycModule so build_if1's PY_power handler can resolve
  // `X.attr` to the module's member symbol at compile time (modules
  // are compile-time-known namespaces, not runtime objects).
  Map<Sym *, PycModule *> module_syms;

  // issues/115: per generator METHOD's coroutine-body Fun, the
  // __pyc_generator__ wrapper Fun installed into the class under the
  // method's name in its place. Created in build_syms (the setter that
  // installs it is emitted there, before the body exists); its body is
  // filled in by build_if1 once gen_fun_pyda has built the coroutine.
  // Empty for a plain def, whose wrapper is built entirely in build_if1
  // and needs no cross-pass handoff.
  Map<Sym *, Sym *> gen_method_wrapper;

  // issue 011 (exception handling, option C):
  Vec<PycTryFrame> try_stack;  // enclosing trys (see PycTryFrame)
  Vec<Sym *> handler_exc;      // innermost handler's saved exception temp (bare re-raise)
  Label *module_unhandled;     // per-module unhandled-exception block, lazily allocated

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
  Fun *order_wrapper(Fun *, Map<MPosition *, MPosition *> &substitutions);
  bool reanalyze(Vec<ATypeViolation *> &type_violations);
  bool c_codegen_pre_file(FILE *);
  // issue 011/050 (Tier 3a: native can_raise inside FA's own fixed
  // point). See ifa.h's declaration for the general contract; see
  // this method's definition (python_ifa_sym.cc) for the specific
  // __pyc_exc__ pattern it recognizes.
  AType *provably_constant_isinstance(AVar *operand_av, EntrySet *es, PNode *send_pnode);

  // --- Entry point ---
  int run(Vec<PycModule *> &mods);
};

// -- Inline helpers (each TU gets its own copy) --

static inline char *read_file_to_string(cchar *fn, uint64 n = 0, int *pfd = 0) {
#ifndef O_NOATIME
#define O_NOATIME 0
#endif
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
bool decorator_name_is(cchar *s, cchar *want);

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
// issues/107: end-of-module check for names used but never bound.
int report_undefined_names(PycCompiler &ctx);
// issues/113: report accumulated import failures; call before
// report_undefined_names so a bad import is blamed on the import.
int report_import_errors(PycCompiler &ctx);

// From python_ifa_build_syms.cc:
PycModule *get_module(cchar *name, PycCompiler &ctx);
// issues/113: PEP 328 -- turn a dot-prefixed relative module name into
// an absolute dotted one, against the importing module's package.
cchar *resolve_relative_module(cchar *mod, PycCompiler &ctx);
int build_syms(PycModule *x, PycCompiler &ctx);
void scope_sym(PycCompiler &ctx, Sym *sym, cchar *name = 0);
Sym *make_string(cchar *s, int len = -1);
Sym *make_bytes(cchar *s, int len = -1);
void gen_ifexpr(PycAST *ifcond, PycAST *ifif, PycAST *ifelse, PycAST *ast);
void call_method(Code **code, PycAST *ast, Sym *o, Sym *m, Sym *r, int n, ...);
Sym *make_symbol(cchar *name);
// pyda path (from python_ifa_build_syms.cc):
void get_syms_args_pyda(PycAST *ast, PyDAST *varargslist, Vec<Sym *> &has, PycCompiler &ctx);
void gen_fun_pyda(PyDAST *n, PycAST *ast, PycCompiler &ctx);
void gen_lambda_pyda(PyDAST *n, PycAST *ast, PycCompiler &ctx);
void gen_class_pyda(PyDAST *n, PycAST *ast, PycCompiler &ctx, char *vector_size = nullptr, bool derive_compare = false);
// Symbol-table pass entry point, exposed so python_ifa_build_if1.cc can run
// it over freshly-parsed expression ASTs that never went through the
// whole-module build_syms pass (f-string interpolation sub-expressions).
int build_syms_pyda(PyDAST *n, PycCompiler &ctx);
// issue 011: per-callee can-raise gating -- computes Sym::can_raise
// (ifa/if1/sym.h) for every function found across `mods`. Run once
// over the builtin module (ast_to_if1_baseline) and once over user
// modules (ast_to_if1_extend); see its definition for why the split.
void compute_can_raise(Vec<PycModule *> &mods, PycCompiler &ctx);
// issues/011/049: call once, from ast_to_if1_baseline, after the
// builtin module's own build_if1 has run (Sym::direct_raise needs
// PY_raise_stmt to have been walked) -- collects every builtin
// method/function name whose OWN body directly raises, for
// user_code_reaches_raise's method-call-by-name matching below. See
// that function's comment (python_ifa_build_syms.cc) for the full
// story.
void collect_builtin_raise_names(PyDAST *builtin_pymod, PycCompiler &ctx);
// issues/011/049: whole-program precise scan -- true if user code
// (module top level or inside any def/lambda/method) directly raises,
// or calls something whose target directly raises (a plain call
// resolved to a Sym with direct_raise, or a method call whose
// attribute name matches collect_builtin_raise_names's set). Call
// once, after collect_builtin_raise_names, to arm pyc_program_has_raise
// for the ordinary-call-into-a-builtin-raiser gap the 5 AST-shape arms
// in build_syms_pyda don't cover (see this function's definition,
// python_ifa_build_syms.cc, for why Sym::can_raise itself is too
// broad for this).
bool user_code_reaches_raise(Vec<PycModule *> &mods, PycCompiler &ctx);

// From python_ifa_build_if1.cc:
int build_if1_module_pyda(PyDAST *mod, PycCompiler &ctx, Code **code);

// From python_ifa_main.cc:
void build_module_attributes_if1(PycModule *mod, PycCompiler &ctx, Code **code);
void install_new_fun(Sym *f);

/*
  Copyright 2008 John Plevyak, All Rights Reserved
*/
#include "defs.h"

/* TODO
   move static variables into an object
   "__bases__" "__class__" "lambda"
   decorators (functions applied to functions)
   division and floor division correctly
   Eq and Is correctly
   exceptions
*/

#define DBG if (debug_level)

typedef MapElem<char *, PycSymbol*> MapCharPycSymbolElem;

static int scope_id = 0;

struct PycScope : public gc {
  int id;
  Sym *in;
  Sym *cls, *fun;
  Map<char *, PycSymbol*> map;
  PycScope() : in(0), cls(0), fun(0) { id = scope_id++; } 
};

struct PycContext : public gc {
  char *filename;
  int lineno;
  void *node;
  Vec<PycScope *> scope_stack;
  Label *lbreak, *lcontinue, *lreturn, *lyield;
  Map<void *, PycScope*> saved_scopes;
  Vec<PycScope *> imports;
  uint32 is_builtin:1;
  Sym *fun() { return scope_stack.last()->fun; }
  Sym *cls() { return scope_stack.last()->cls; }
  PycContext() : lineno(-1), lbreak(0), lcontinue(0), lreturn(0),
                 is_builtin(0) {}
};

static Map<stmt_ty, PycAST *> stmtmap;
static Map<expr_ty, PycAST *> exprmap;
static Sym *sym_long = 0, *sym_ellipsis = 0, *sym_ellipsis_type = 0,
  *sym_unicode = 0, *sym_buffer = 0, *sym_xrange = 0;
static Sym *sym_write = 0, *sym_writeln = 0, *sym___iter__ = 0, *sym_next = 0;
static Sym *sym___init__ = 0, *sym_super = 0;
static char *cannonical_self = 0;
static int finalized_aspect = 0;
static Vec<Sym *> builtin_functions;

PycSymbol::PycSymbol() : symbol(0), filename(0) {
}

char *
cannonicalize_string(char *s) {
  return if1_cannonicalize_string(if1, s);
}

Sym *
PycSymbol::clone() {
  return copy()->sym;
}

char* 
PycSymbol::pathname() {
  if (filename)
    return filename;
  if (sym->ast)
    return sym->ast->pathname();
  return 0;
}

int 
PycSymbol::line() {
  return 0;
}

int 
PycSymbol::source_line() {
  if (sym->ast && !((PycAST*)sym->ast)->is_builtin)
    return line();
  else
    return 0;
}

int 
PycSymbol::ast_id() {
  return 0;
}

PycAST::PycAST() : xstmt(0), xexpr(0), filename(0), parent(0),code(0), sym(0), rval(0),
                   is_builtin(0), is_member(0) {
  label[0] = label[1] = 0;
}

char
*PycAST::pathname() {
  return filename;
}

int
PycAST::line() {
  return xstmt ? xstmt->lineno : xexpr->lineno;
}

int
PycAST::source_line() {
  if (is_builtin)
    return 0;
  else
    return line();
}

Sym *
PycAST::symbol() {
  if (rval) return rval;
  return sym;
}

IFAAST *
PycAST::copy_node(ASTCopyContext *context) {
  PycAST *a = new PycAST(*this);
  if (context)
    for (int i = 0; i < a->pnodes.n; i++)
      a->pnodes.v[i] = context->nmap->get(a->pnodes.v[i]);
  return a;
}

IFAAST *
PycAST::copy_tree(ASTCopyContext *context) {
  PycAST *a = (PycAST*)copy_node(context);
  for (int i = 0; i < a->children.n; i++)
    a->children.v[i] = (PycAST*)a->children.v[i]->copy_tree(context);
  return a;
}

Vec<Fun *> *
PycAST::visible_functions(Sym *arg0) {
  Vec<Fun *> *v = 0;
  if (arg0->fun) {
    Fun *f = arg0->fun;
    v = new Vec<Fun *>;
    v->add(f);
    return v;
  }
  return NULL;
}

static PycSymbol *
new_PycSymbol(char *name) {
  PycSymbol *s = new PycSymbol;
  s->sym = new Sym;
  s->sym->asymbol = s;
  if1_register_sym(if1, s->sym, name);
  return s;
}

static PycSymbol *
new_PycSymbol(char *name, PycContext &ctx) {
  PycSymbol *s = new_PycSymbol(name);
  s->filename = ctx.filename;
  return s;
}

PycSymbol * 
PycSymbol::copy() {
  PycSymbol *s = new PycSymbol;
  s->sym = sym->copy();
  s->sym->asymbol = s;
  if (s->sym->type_kind != Type_NONE)
    s->sym->type = s->sym;
  return s;
}

Sym *
PycCallbacks::new_Sym(char *name) {
  return new_PycSymbol(name)->sym;
}

static Sym *
new_sym(char *name = 0, int global = 0) {
  Sym *s = new_PycSymbol(name)->sym;
  if (!global)
    s->nesting_depth = LOCALLY_NESTED;
  return s;
}

static Sym *
new_sym(PycAST *ast) {
  Sym *s = new_sym();
  s->ast = ast;
  s->is_local = 1;
  return s;
}

static Sym *
new_global(PycAST *ast, char *name = 0) {
  Sym *sym = new_PycSymbol(name)->sym;
  sym->ast = ast;
  sym->nesting_depth = 0;
  return sym;
}

static Sym *
new_base_instance(Sym *c, PycAST *ast) {
  if (c->type_kind == Type_PRIMITIVE) {
    if (c->num_kind)
      return if1_const(if1, c, "0");
    if (c == sym_string)
      return if1_const(if1, c, "");
  }
  fail("no instance for type '%s' found", c->name);
  return 0;
}

static void
build_builtin_symbols() {
  sym_write = if1_make_symbol(if1, "write");
  sym_writeln = if1_make_symbol(if1, "writeln");
  sym___iter__ = if1_make_symbol(if1, "__iter__");
  sym_next = if1_make_symbol(if1, "next");
  sym___init__ = if1_make_symbol(if1, "__init__");
  sym_super = if1_make_symbol(if1, "super");
  cannonical_self = if1_cannonicalize_string(if1, "self");

  init_default_builtin_types();

  new_builtin_global_variable(sym_init, "init");

  // override default sizes
  sym_int->type_kind = Type_ALIAS;
  sym_int->alias = sym_int32;
  sym_float->type_kind = Type_ALIAS;
  sym_float->alias = sym_float64;
  sym_complex->type_kind = Type_ALIAS;
  sym_complex->alias = sym_complex64;

  // override default names
  sym_string->name = if1_cannonicalize_string(if1, "str");
  sym_void->name = if1_cannonicalize_string(if1, "None");
  sym_unknown->name = if1_cannonicalize_string(if1, "Unimplemented");
  sym_true->name = if1_cannonicalize_string(if1, "True");
  sym_false->name = if1_cannonicalize_string(if1, "False");

  // new types and objects
  new_builtin_primitive_type(sym_unicode, "unicode");
  new_builtin_primitive_type(sym_buffer, "buffer");
  new_builtin_primitive_type(sym_xrange, "xrange");
  new_builtin_primitive_type(sym_ellipsis_type, "Ellipsis_type");
  new_builtin_alias_type(sym_long, "long", sym_int64); // standin for GNU gmp
  new_builtin_unique_object(sym_ellipsis, "Ellipsis", sym_ellipsis_type);

  builtin_functions.set_add(sym_super);
}

static void 
finalize_function(Fun *fun) {
  fun->is_eager = 1;
}

void
PycCallbacks::finalize_functions() {
  forv_Fun(fun, pdb->funs)
    finalize_function(fun);
}

static inline PycAST *getAST(stmt_ty s, PycContext &ctx) {
  PycAST *ast = stmtmap.get(s);
  if (ast) return ast;
  ast = new PycAST;
  ast->filename = ctx.filename;
  ast->is_builtin = ctx.is_builtin;
  ast->xstmt = s;
  stmtmap.put(s, ast);
  return ast;
}

static inline PycAST *getAST(expr_ty e, PycContext &ctx) {
  PycAST *ast = exprmap.get(e);
  if (ast) return ast;
  ast = new PycAST;
  ast->filename = ctx.filename;
  ast->is_builtin = ctx.is_builtin;
  ast->xexpr = e;
  exprmap.put(e, ast);
  return ast;
}

template<class C>
static void add(asdl_seq *seq, Vec<PycAST*> &asts, PycContext &ctx) {
  for (int i = 0; i < asdl_seq_LEN(seq); i++)
    asts.add(getAST((C)asdl_seq_GET(seq, i), ctx));
}

static void add(expr_ty e, Vec<PycAST*> &asts, PycContext &ctx) { 
  if (e) asts.add(getAST(e, ctx)); 
}
//static void add(stmt_ty s, Vec<PycAST*> &asts, PycContext &ctx) { if (s) stmts.add(getAST(s)); } unused

static void add_comprehension(asdl_seq *comp, Vec<PycAST*> &asts, PycContext &ctx) {
  int l = asdl_seq_LEN(comp);
  for (int i = 0; i < l; i++) {
    comprehension_ty c = (comprehension_ty)asdl_seq_GET(comp, i);
    add(c->target, asts, ctx);
    if (i != 0)
      add(c->iter, asts, ctx);
    add<expr_ty>(c->ifs, asts, ctx);
  }
}

static void get_pre_scope_next(stmt_ty s, Vec<PycAST*> &asts, PycContext &ctx) {
  switch (s->kind) {
    default: break;
    case FunctionDef_kind:
      add<expr_ty>(s->v.FunctionDef.args->defaults, asts, ctx);
      add<expr_ty>(s->v.FunctionDef.decorators, asts, ctx);
      break;
    case ClassDef_kind:
      add<expr_ty>(s->v.ClassDef.bases, asts, ctx);
      break;
  }
}

static void get_next(stmt_ty s, Vec<PycAST*> &asts, PycContext &ctx) {
  switch (s->kind) {
    default: assert(!"case");
    case FunctionDef_kind:
      add<expr_ty>(s->v.FunctionDef.args->args, asts, ctx);
      add<stmt_ty>(s->v.FunctionDef.body, asts, ctx);
      break;
    case ClassDef_kind:
      add<stmt_ty>(s->v.ClassDef.body, asts, ctx);
      break;
    case Return_kind: add(s->v.Return.value, asts, ctx); break;
    case Delete_kind: add<expr_ty>(s->v.Delete.targets, asts, ctx); break;
    case Assign_kind: 
      add<expr_ty>(s->v.Assign.targets, asts, ctx); 
      add(s->v.Assign.value, asts, ctx); 
      break;
    case AugAssign_kind:
      add(s->v.AugAssign.target, asts, ctx); 
      add(s->v.AugAssign.value, asts, ctx); 
      break;
    case Print_kind: 
      add(s->v.Print.dest, asts, ctx); 
      add<expr_ty>(s->v.Print.values, asts, ctx); 
      break;
    case For_kind:
      add(s->v.For.target, asts, ctx); 
      add(s->v.For.iter, asts, ctx); 
      add<stmt_ty>(s->v.For.body, asts, ctx); 
      add<stmt_ty>(s->v.For.orelse, asts, ctx); 
      break;
    case While_kind:
      add(s->v.While.test, asts, ctx); 
      add<stmt_ty>(s->v.While.body, asts, ctx); 
      add<stmt_ty>(s->v.While.orelse, asts, ctx); 
      break;
    case If_kind:
      add(s->v.If.test, asts, ctx); 
      add<stmt_ty>(s->v.If.body, asts, ctx); 
      add<stmt_ty>(s->v.If.orelse, asts, ctx); 
      break;
    case With_kind:
      add(s->v.With.context_expr, asts, ctx); 
      add(s->v.With.optional_vars, asts, ctx); 
      add<stmt_ty>(s->v.With.body, asts, ctx); 
      break;
    case Raise_kind:
      add(s->v.Raise.type, asts, ctx); 
      add(s->v.Raise.inst, asts, ctx); 
      add(s->v.Raise.tback, asts, ctx); 
      break;
    case TryExcept_kind: {
      add<stmt_ty>(s->v.TryExcept.body, asts, ctx); 
      for (int i = 0; i < asdl_seq_LEN(s->v.TryExcept.handlers); i++) {
        excepthandler_ty h = (excepthandler_ty)asdl_seq_GET(s->v.TryExcept.handlers, i);
        add(h->type, asts, ctx);
        add(h->name, asts, ctx);
        add<stmt_ty>(h->body, asts, ctx);
      }
      add<stmt_ty>(s->v.TryExcept.orelse, asts, ctx); 
      break;
    }
    case TryFinally_kind: 
      add<stmt_ty>(s->v.TryFinally.body, asts, ctx); 
      add<stmt_ty>(s->v.TryFinally.finalbody, asts, ctx); 
      break;
    case Assert_kind: add(s->v.Assert.test, asts, ctx); add(s->v.Assert.msg, asts, ctx); break;
    case Import_kind: break;
    case ImportFrom_kind: break;
    case Exec_kind: 
      add(s->v.Exec.body, asts, ctx); 
      add(s->v.Exec.globals, asts, ctx); 
      add(s->v.Exec.locals, asts, ctx); 
      break;
    case Global_kind: break;
#if PY_MAJOR_VERSION == 3
    case Nonlocal_kind: break;
#endif
    case Expr_kind: add(s->v.Expr.value, asts, ctx); break;
    case Pass_kind:
    case Break_kind:
    case Continue_kind:
      break;
  }
}

static void get_next(slice_ty s, Vec<PycAST*> &asts, PycContext &ctx) {
  switch (s->kind) {
    default: assert(!"case");
    case Ellipsis_kind: break;
    case Slice_kind: // (expr? lower, expr? upper, expr? step)
      add(s->v.Slice.lower, asts, ctx); 
      add(s->v.Slice.upper, asts, ctx); 
      add(s->v.Slice.step, asts, ctx); 
      break;
    case ExtSlice_kind: // (slice* dims)
      for (int i = 0; i < asdl_seq_LEN(s->v.ExtSlice.dims); i++)
        get_next((slice_ty)asdl_seq_GET(s->v.ExtSlice.dims, i), asts, ctx);
      break;
    case Index_kind: // (expr value)
      add(s->v.Index.value, asts, ctx); break;
  }
}

static void get_pre_scope_next(expr_ty e, Vec<PycAST*> &asts, PycContext &ctx) {
  switch (e->kind) {
    default: 
      break;
    case Lambda_kind:
      add<expr_ty>(e->v.Lambda.args->defaults, asts, ctx);
      break;
    case Dict_kind: // expr* keys, expr* values
#if PY_MAJOR_VERSION == 3
      add(((comprehension_ty)asdl_seq_GET(e->v.Dict.generators, 0))->iter, asts, ctx);
#endif
      break;
    case ListComp_kind: // expr elt, comprehension* generators
      add(((comprehension_ty)asdl_seq_GET(e->v.ListComp.generators, 0))->iter, asts, ctx);
      break;
#if PY_MAJOR_VERSION == 3
    case SetComp_kind:
      add(((comprehension_ty)asdl_seq_GET(e->v.SetComp.generators, 0))->iter, asts, ctx);
      break;
#endif
    case GeneratorExp_kind: // expr elt, comprehension* generators
      add(((comprehension_ty)asdl_seq_GET(e->v.GeneratorExp.generators, 0))->iter, asts, ctx);
      break;
  }
}

static void get_next(expr_ty e, Vec<PycAST*> &asts, PycContext &ctx) {
  switch (e->kind) {
    default: assert(!"case");
    case BoolOp_kind: // boolop op, expr* values
      add<expr_ty>(e->v.BoolOp.values, asts, ctx); break;
    case BinOp_kind: // expr left, operator op, expr right
      add(e->v.BinOp.left, asts, ctx);
      add(e->v.BinOp.right, asts, ctx);
      break;
    case UnaryOp_kind: // unaryop op, expr operand
      add(e->v.UnaryOp.operand, asts, ctx); break;
    case Lambda_kind: // arguments args, expr body
      add<expr_ty>(e->v.Lambda.args->args, asts, ctx);
      add(e->v.Lambda.body, asts, ctx); 
      break;
    case IfExp_kind: // expr test, expr body, expr orelse
      add(e->v.IfExp.test, asts, ctx); 
      add(e->v.IfExp.body, asts, ctx); 
      add(e->v.IfExp.orelse, asts, ctx); 
      break;
    case Dict_kind: // expr* keys, expr* values
#if PY_MAJOR_VERSION == 3
      add_comprehension(e->v.Dict.generators, asts, ctx);
      add(e->v.Dict.value, asts, ctx); 
      add(e->v.Dict.key, asts, ctx); 
#else
      add<expr_ty>(e->v.Dict.keys, asts, ctx); 
      add<expr_ty>(e->v.Dict.values, asts, ctx); 
#endif
      break;
    case ListComp_kind: // expr elt, comprehension* generators
      add_comprehension(e->v.ListComp.generators, asts, ctx);
      add(e->v.ListComp.elt, asts, ctx); 
      break;
#if PY_MAJOR_VERSION == 3
    case SetComp_kind:
      add_comprehension(e->v.SetComp.generators, asts, ctx);
      add(e->v.ListComp.elt, asts, ctx); 
      break;
#endif
    case GeneratorExp_kind: // expr elt, comprehension* generators
      add_comprehension(e->v.GeneratorExp.generators, asts, ctx);
      add(e->v.GeneratorExp.elt, asts, ctx); 
      break;
    case Yield_kind: // expr? value
      add(e->v.Yield.value, asts, ctx); break;
    case Compare_kind: // expr left, cmpop* ops, expr* comparators
      add(e->v.Compare.left, asts, ctx);
      add<expr_ty>(e->v.Compare.comparators, asts, ctx);
      break;
    case Call_kind: // expr func, expr* args, keyword* keywords, expr? starargs, expr? kwargs
      add(e->v.Call.func, asts, ctx);
      add<expr_ty>(e->v.Call.args, asts, ctx);
      add(e->v.Call.starargs, asts, ctx);
      add(e->v.Call.kwargs, asts, ctx);
      break;
    case Repr_kind: // expr value
     add(e->v.Repr.value, asts, ctx); break;
    case Num_kind: // object n) -- a number as a PyObject
    case Str_kind: // string s) -- need to specify raw, unicode, etc
      break;
    case Attribute_kind: // expr value, identifier attr, expr_context ctx
      add(e->v.Attribute.value, asts, ctx); break;
    case Subscript_kind: // expr value, slice slice, expr_context ctx
     add(e->v.Subscript.value, asts, ctx); 
     get_next(e->v.Subscript.slice, asts, ctx);
     break;
    case Name_kind: // identifier id, expr_context ctx
      break;
    case List_kind: // expr* elts, expr_context ctx
      add<expr_ty>(e->v.List.elts, asts, ctx); break;
    case Tuple_kind: // expr *elts, expr_context ctx
      add<expr_ty>(e->v.Tuple.elts, asts, ctx); break;
  }
}

static void enter_scope(PycContext &ctx, Sym *in = 0) {
  PycScope *c = ctx.saved_scopes.get(ctx.node);
  if (!c) {
    c = new PycScope;
    c->in = in;
    if (ctx.scope_stack.n) {
      c->fun = ctx.scope_stack.last()->fun;
      c->cls = ctx.scope_stack.last()->cls;
    }
    if (in) {
      if (in->is_fun)
        c->fun = in;
      else
        c->cls = in;
    }
    ctx.saved_scopes.put(ctx.node, c);
  }
  ctx.scope_stack.add(c);
  DBG printf("enter scope %d level %d\n", ctx.scope_stack.last()->id, ctx.scope_stack.n);
}

static void enter_scope(PycContext &ctx, mod_ty mod) {
  ctx.node = mod;
  enter_scope(ctx);
}

static void enter_scope(stmt_ty x, PycAST *ast, PycContext &ctx) {
  ctx.node = x;
  if (x->kind == FunctionDef_kind)
    enter_scope(ctx, ast->rval);
  else if (x->kind == ClassDef_kind)
    enter_scope(ctx, ast->sym);
}

static int needs_scope(expr_ty x) {
  return (x->kind == Lambda_kind || x->kind == GeneratorExp_kind ||
          x->kind == Dict_kind || x->kind == ListComp_kind 
#if PY_MAJOR_VERSION == 3
          x->kind == SetComp_kind
#endif
    );
}

static void enter_scope(expr_ty x, PycAST *ast, PycContext &ctx) {
  ctx.node = x;
  if (needs_scope(x)) enter_scope(ctx);
}

static void exit_scope(PycContext &ctx) { 
  DBG printf("exit scope %d level %d\n", ctx.scope_stack.last()->id, ctx.scope_stack.n);
  ctx.scope_stack.pop(); 
}

static void exit_scope(stmt_ty x, PycContext &ctx) {
  ctx.node = x;
  if (x->kind == FunctionDef_kind || x->kind == ClassDef_kind)
    exit_scope(ctx);
}

static void exit_scope(expr_ty x, PycContext &ctx) {
  ctx.node = x;
  if (needs_scope(x)) exit_scope(ctx);
}

#define EXPLICITLY_MARKED 1
#define IMPLICITLY_MARKED 2
enum PYC_SCOPINGS { PYC_USE, PYC_LOCAL, PYC_GLOBAL, PYC_NONLOCAL };
static char *pyc_scoping_names[] = { "use", "local", "global", "nonlocal" };
#define GLOBAL_USE ((PycSymbol*)(intptr_t)1)
#define NONLOCAL_USE ((PycSymbol*)(intptr_t)2)
#define GLOBAL_DEF ((PycSymbol*)(intptr_t)3)
#define NONLOCAL_DEF ((PycSymbol*)(intptr_t)4)
#define MARKED(_x) (((uintptr_t)(_x)) < 5)

static PycSymbol *find_PycSymbol(PycContext &ctx, char *name, int *level = 0, int *type = 0) {
  PycSymbol *l = 0;
  int i = ctx.scope_stack.n - 1, xtype = 0;
  int end = -ctx.imports.n;
  for (;i >= end; i--) {
    bool top = i == ctx.scope_stack.n - 1;
    PycScope *s = i >= 0 ? ctx.scope_stack.v[i] : ctx.imports.v[-i - 1];
    if ((l = s->map.get(name))) {
      if (l == NONLOCAL_USE || l == NONLOCAL_DEF) {
        if (top)
          xtype = (l == NONLOCAL_DEF) ? EXPLICITLY_MARKED : IMPLICITLY_MARKED;
        continue;
      }
      if (l == GLOBAL_USE || l == GLOBAL_DEF) {
        assert(i); 
        if (top)
          xtype = (l == GLOBAL_DEF) ? EXPLICITLY_MARKED : IMPLICITLY_MARKED;
        i = 1; 
        continue; 
      }
      break;
    }
  }
  if (level) *level = i;
  if (type) *type = xtype;
  return l;
}

//static PycSymbol *find_PycSymbol(PycContext &ctx, PyObject *o, int *level = 0, int *type = 0) {
//  return find_PycSymbol(ctx, if1_cannonicalize_string(if1, PyString_AS_STRING(o)), level, type);
//}

static PycSymbol *make_PycSymbol(PycContext &ctx, char *n, PYC_SCOPINGS scoping) {
  char *name = if1_cannonicalize_string(if1, n);
  DBG printf("make_PycSymbol %s '%s'\n", pyc_scoping_names[(int)scoping], name);
  int level = 0, type = 0;
  PycSymbol *l = find_PycSymbol(ctx, name, &level, &type);
  bool local = l && (ctx.scope_stack.n - 1 == level); // implies !explicitly && !implicitly
  bool global = l && !level;
  bool nonlocal = l && !global && !local;
  bool explicitly = type == EXPLICITLY_MARKED;
  bool implicitly = type == IMPLICITLY_MARKED;
  bool isfun = l && l->sym->is_fun;
  switch (scoping) {
    case PYC_USE: {
      if (!l) goto Lglobal; // not found
      if (!local && !explicitly) {
        if (global)
          ctx.scope_stack.last()->map.put(name, GLOBAL_USE);
        else
          ctx.scope_stack.last()->map.put(name, NONLOCAL_USE);
      }
      break;
    }
    case PYC_LOCAL:
      if ((local || explicitly) && !isfun) break;
      if (implicitly && !isfun)
        fail("error line %d, '%s' redefined as local", ctx.lineno, name);
      ctx.scope_stack.last()->map.put(name, (l = new_PycSymbol(name, ctx)));
      break;
    case PYC_GLOBAL:
    Lglobal:;
      if (l && !global && (local || explicitly || implicitly))
        fail("error line %d, '%s' redefined as global", ctx.lineno, name);
      if (!global) {
        PycSymbol *g = ctx.scope_stack.v[0]->map.get(name);
        if (!g)
          ctx.scope_stack.v[0]->map.put(name, (l = new_PycSymbol(name, ctx)));
      }
      if (!explicitly)
        ctx.scope_stack.last()->map.put(name, GLOBAL_DEF);
      break;
    case PYC_NONLOCAL:
      if (!l || (!nonlocal && (local || explicitly || implicitly)))
        fail("error line %d, '%s' nonlocal redefined or not found", ctx.lineno, name);
      ctx.scope_stack.last()->map.put(name, NONLOCAL_DEF);
      break;
  }
  return l;
}

static PycSymbol *make_PycSymbol(PycContext &ctx, PyObject *o, PYC_SCOPINGS type) {
  return make_PycSymbol(ctx, PyString_AS_STRING(o), type);
}

static int build_syms(stmt_ty s, PycContext &ctx);
static int build_syms(expr_ty e, PycContext &ctx);

#define AST_RECURSE_PRE(_ast, _fn, _ctx) \
  PycAST *ast = getAST(_ast, _ctx); \
  { \
    Vec<PycAST*> asts; get_pre_scope_next(_ast, asts, _ctx); \
    forv_Vec(PycAST, x, asts) { \
      x->parent = ast; \
      ast->pre_scope_children.add(x);     \
      if (x->xstmt) _fn(x->xstmt, _ctx); else _fn(x->xexpr, ctx);   \
    } \
  }

#define AST_RECURSE_POST(_ast, _fn, _ctx) \
  { \
    Vec<PycAST*> asts; get_next(_ast, asts, _ctx);   \
    forv_Vec(PycAST, x, asts) { \
      x->parent = ast; \
      ast->children.add(x); \
      if (x->xstmt) _fn(x->xstmt, _ctx); else _fn(x->xexpr, ctx);   \
    } \
  }

#define AST_RECURSE(_ast, _fn, _ctx) \
  AST_RECURSE_PRE(_ast, _fn, _ctx) \
  enter_scope(_ast, ast, _ctx); \
  AST_RECURSE_POST(_ast, _fn, _ctx)

static void get_syms_args(
  PycAST *a, arguments_ty args, Vec<Sym *> &has, PycContext &ctx, asdl_seq *decorators = 0) {
  for (int i = 0; i < asdl_seq_LEN(args->args); i++) {
#if PY_MAJOR_VERSION == 3
    assert(!"incomplete");
#else
    Sym *sym = getAST((expr_ty)asdl_seq_GET(args->args, i), ctx)->sym;
#endif
    has.add(sym);
  }
  if (ctx.is_builtin && decorators) {
    for (int j = 0; j < asdl_seq_LEN(decorators); j++) {
      expr_ty e = (expr_ty)asdl_seq_GET(decorators, j);
      if (e->kind == Call_kind) {
        printf("%d\n", e->v.Call.func->kind);
      }
    }
  }
}

static Sym *new_fun(PycAST *ast, Sym *fun = 0) {
  if (!fun)
    fun = new_sym(ast);
  else
    fun->ast = ast;
  fun->is_fun = 1;
  fun->cont = new_sym(ast);
  fun->ret = new_sym(ast);
  return fun;
}

static Sym *
def_fun(stmt_ty s, PycAST *ast, char *name, PycContext &ctx, int constructor = 0) {
  if (constructor)
    enter_scope(s, ast, ctx);
  Sym *fn = make_PycSymbol(ctx, name, PYC_LOCAL)->sym;
  fn->in = ctx.cls();
  if (!constructor)
    enter_scope(s, ast, ctx);
  ctx.scope_stack.last()->fun = new_fun(ast, fn);
  fn->nesting_depth = ctx.scope_stack.n - 1;
  ctx.lreturn = ast->label[0] = if1_alloc_label(if1);
  return fn;
}

static Sym *
def_fun(expr_ty e, PycAST *ast, PycContext &ctx) {
  Sym *fn = new_sym(ast);
  fn->in = ctx.cls();
  enter_scope(e, ast, ctx);
  ctx.scope_stack.last()->fun = new_fun(ast, fn);
  fn->nesting_depth = ctx.scope_stack.n - 1;
  ctx.lreturn = ast->label[0] = if1_alloc_label(if1);
  return fn;
}

static int
build_syms(stmt_ty s, PycContext &ctx) {
  ctx.node = s;
  ctx.lineno = s->lineno;
  AST_RECURSE_PRE(s, build_syms, ctx);
  switch (s->kind) {
    default: break;
    case FunctionDef_kind: // identifier name, arguments args, stmt* body, expr* decorators
      ast->sym = def_fun(s, ast, PyString_AS_STRING(s->v.FunctionDef.name), ctx);
      break;
    case ClassDef_kind: { // identifier name, expr* bases, stmt* body
      PYC_SCOPINGS scope = (ctx.is_builtin && ctx.scope_stack.n == 1) ? PYC_GLOBAL : PYC_LOCAL;
      ast->sym = unalias_type(make_PycSymbol(ctx, s->v.ClassDef.name, scope)->sym);
      if (!ast->sym->type_kind)
        ast->sym->type_kind = Type_RECORD; // do not override
      if (ast->sym->type_kind == Type_RECORD)
        ast->sym->self = new_global(ast); // prototype
      else
        ast->sym->self = new_base_instance(ast->sym, ast);
      ast->rval = def_fun(s, ast, "___init___", ctx, 1);
      ast->rval->self = new_sym(ast);
      ast->rval->self->must_implement_and_specialize(ast->sym);
      break;
    }
    case Global_kind: // identifier* names
      for (int i = 0; i < asdl_seq_LEN(s->v.Global.names); i++)
        make_PycSymbol(ctx, (PyObject*)asdl_seq_GET(s->v.Global.names, i), PYC_GLOBAL);
      break;
#if PY_MAJOR_VERSION == 3
    case Nonlocal_kind: 
      for (int i = 0; i < asdl_seq_LEN(s->v.Global.names); i++)
        make_PycSymbol(ctx, (PyObject*)asdl_seq_GET(s->v.Global.names, i), PYC_NONLOCAL);
      break;
#endif
    case For_kind:
    case While_kind:
      ctx.lcontinue = ast->label[0] = if1_alloc_label(if1);
      ctx.lbreak = ast->label[1] = if1_alloc_label(if1);
      break;
  }
  AST_RECURSE_POST(s, build_syms, ctx);
  switch (s->kind) {
    default: break;
    case ClassDef_kind: // identifier name, expr* bases, stmt* body
      for (int i = 0; i < asdl_seq_LEN(s->v.ClassDef.bases); i++) {
        Sym *base = getAST((expr_ty)asdl_seq_GET(s->v.ClassDef.bases, i), ctx)->sym;
        if (!base)
          fail("error line %d, base not for for class '%s'", ctx.lineno, ast->sym->name);
        ast->sym->inherits_add(getAST((expr_ty)asdl_seq_GET(s->v.ClassDef.bases, i), ctx)->sym);
      }
      form_Map(MapCharPycSymbolElem, x, ctx.scope_stack.last()->map)
        if (!MARKED(x->value) && !x->value->sym->is_fun)
          ast->sym->has.add(x->value->sym);
      break;
    case Continue_kind: ast->label[0] = ctx.lcontinue; break;
    case Break_kind: ast->label[0] = ctx.lbreak; break;
    case Return_kind: ast->label[0] = ctx.lreturn; break;
  }
  exit_scope(s, ctx);
  return 0;
}

static void build_syms_comprehension(PycContext &ctx,
                                     asdl_seq *generators, expr_ty elt, expr_ty value) {
}

static int
build_syms(expr_ty e, PycContext &ctx) {
  PycAST *past = getAST(e, ctx);
  ctx.node = e;
  ctx.lineno = e->lineno;
  switch (e->kind) {
    default: break;
    case Lambda_kind: // arguments args, expr body
      past->rval = def_fun(e, past, ctx);
      break;
    case Name_kind: // identifier id, expr_context ctx
      past->sym = past->rval = 
        make_PycSymbol(ctx, e->v.Name.id, e->v.Name.ctx == Load ? PYC_USE : PYC_LOCAL)->sym;
      break;
    case Dict_kind: // expr* keys, expr* values
    case ListComp_kind: // expr elt, comprehension* generators
#if PY_MAJOR_VERSION == 3
    case SetComp_kind: // expr elt, comprehension* generators
#endif
    case GeneratorExp_kind: // expr elt, comprehension* generators
      ctx.lyield = past->label[0] = if1_alloc_label(if1);
      break;
  }
  AST_RECURSE(e, build_syms, ctx);
  switch (e->kind) {
    default: break;
    case Dict_kind: // expr* keys, expr* values or comprehension* generators, key, value
#if PY_MAJOR_VERSION == 3
      build_syms_comprehension(ctx, e->v.Dict.generators, e->v.Dict.key, e->v.Dict.value);
#endif
      break;
    case ListComp_kind: // expr elt, comprehension* generators
      build_syms_comprehension(ctx, e->v.ListComp.generators, e->v.ListComp.elt, 0);
      break;
#if PY_MAJOR_VERSION == 3
    case SetComp_kind: // expr elt, comprehension* generators
      build_syms_comprehension(ctx, e->v.SetComp.generators, e->v.SetComp.elt, 0);
      break;
#endif
    case GeneratorExp_kind: // expr elt, comprehension* generators
      build_syms_comprehension(ctx, e->v.GeneratorExp.generators, e->v.GeneratorExp.elt, 0);
      break;
    case Tuple_kind: // expr *elts, expr_context ctx
      ast->sym = ast->rval = new_sym(ast);
      for (int i = 0; i < asdl_seq_LEN(e->v.Tuple.elts); i++)
        ast->sym->has.add(getAST((expr_ty)asdl_seq_GET(e->v.Tuple.elts, i), ctx)->sym);
      break;
  }
  exit_scope(e, ctx);
  return 0;
}

static int build_syms_stmts(asdl_seq *stmts, PycContext &ctx) {
  for (int i = 0; i < asdl_seq_LEN(stmts); i++)
    if (build_syms((stmt_ty)asdl_seq_GET(stmts, i), ctx)) return -1;
  return 0;
}

static int
build_syms(mod_ty mod, PycContext &ctx) {
  int r = 0;
  ctx.node = mod;
  enter_scope(ctx);
  switch (mod->kind) {
    case Module_kind: r = build_syms_stmts(mod->v.Module.body, ctx); break;
    case Expression_kind: r = build_syms(mod->v.Expression.body, ctx); break;
    case Interactive_kind: r = build_syms_stmts(mod->v.Interactive.body, ctx); break;
    case Suite_kind: assert(!"handled");
  }
  exit_scope(ctx);
  return r;
}

static Sym *make_string(char *s) {
  Immediate imm;
  imm.v_string = s;
  Sym *sym = if1_const(if1, sym_string, s, &imm);
  if (!sym->implements.n) {
    sym->implements.add(sym_string);
    sym->specializes.add(sym_string);
  }
  return sym;
}

static Sym *make_string(PyObject *o) {
  return make_string(PyString_AsString(o));
}

static void
gen_fun(stmt_ty s, PycAST *ast, PycContext &ctx) {
  Sym *fn = ast->sym;
  Code *body = 0;
  Sym *in = ctx.scope_stack[ctx.scope_stack.n-2]->in;
  for (int i = 0; i < asdl_seq_LEN(s->v.FunctionDef.body); i++)
    if1_gen(if1, &body, getAST((stmt_ty)asdl_seq_GET(s->v.FunctionDef.body, i), ctx)->code);
  if1_move(if1, &body, sym_void, fn->ret, ast);
  if1_label(if1, &body, ast, ast->label[0]);
  if1_send(if1, &body, 4, 0, sym_primitive, sym_reply, fn->cont, fn->ret)->ast = ast;
  Vec<Sym *> as;
  as.add(fn);
  get_syms_args(ast, s->v.FunctionDef.args, as, ctx, s->v.FunctionDef.decorators);
  if (in && !in->is_fun) {
    as.v[0] = if1_make_symbol(if1, as.v[0]->name);
    if (as.n > 1) {
      fn->self = as.v[1];
      fn->self->must_implement_and_specialize(in);
    }
  }
  if1_closure(if1, fn, body, as.n, as.v);
}

static void
gen_fun(expr_ty e, PycAST *ast, PycContext &ctx) {
  Sym *fn = ast->rval;
  Code *body = 0;
  PycAST *b = getAST(e->v.Lambda.body, ctx);
  if1_gen(if1, &body, b->code);
  if1_move(if1, &body, b->rval, fn->ret, ast);
  if1_label(if1, &body, ast, ast->label[0]);
  if1_send(if1, &body, 4, 0, sym_primitive, sym_reply, fn->cont, fn->ret)->ast = ast;
  Vec<Sym *> as;
  as.add(fn);
  get_syms_args(ast, e->v.Lambda.args, as, ctx);
  if1_closure(if1, fn, body, as.n, as.v);
}

static void
gen_class_init(stmt_ty s, PycAST *ast, PycContext &ctx) {
  // build base ___init___ (class specific prototype initialization)
  Sym *fn = ast->rval, *cls = ast->sym;
  bool is_record = cls->type_kind == Type_RECORD;
  Sym *selector = if1_make_symbol(if1, fn->name);
  Code *body = 0;
  for (int i = 0; i < cls->includes.n; i++) {
    Sym *inc = cls->includes.v[i];
    for (int j = 0; j < inc->has.n; j++) {
      Sym *t = new_sym(ast);
      Sym *iv = if1_make_symbol(if1, inc->has.v[j]->name);
      if (inc->self) {
        if1_send(if1, &body, 4, 1, sym_operator, inc->self, sym_period, iv, t)->ast = ast;
        if1_send(if1, &body, 5, 1, sym_operator, fn->self, sym_setter, iv, t, 
                 (ast->rval = new_sym(ast)))->ast = ast;
      }
    }
  }
  for (int i = 0; i < asdl_seq_LEN(s->v.ClassDef.body); i++)
    if1_gen(if1, &body, getAST((stmt_ty)asdl_seq_GET(s->v.ClassDef.body, i), ctx)->code);
  if1_move(if1, &body, fn->self, fn->ret, ast);
  if1_label(if1, &body, ast, ast->label[0]);
  if1_send(if1, &body, 4, 0, sym_primitive, sym_reply, fn->cont, fn->ret)->ast = ast;
  {
    Vec<Sym *> as;
    as.add(selector);
    as.add(fn->self);
    if1_closure(if1, fn, body, as.n, as.v);
  }
  // build prototype
  Sym *proto = cls->self;
  if (is_record) {
    if1_send(if1, &ast->code, 3, 1, sym_primitive, sym_new, cls, proto)->ast = ast;
    if1_send(if1, &ast->code, 2, 1, selector, proto, new_sym(ast))->ast = ast;
  }
  // build default __init__ (user class constructor initialization)
  PycSymbol *init_fun = ctx.scope_stack.last()->map.get(sym___init__->name);
  Sym *init_sym = init_fun ? init_fun->sym : 0;
  if (!init_fun) {
    init_sym = fn = new_fun(ast);
    fn->nesting_depth = ctx.scope_stack.n;
    fn->self = new_sym(ast);
    fn->self->must_implement_and_specialize(cls);
    body = 0;
    if1_move(if1, &body, fn->self, fn->ret);
    if1_send(if1, &body, 4, 0, sym_primitive, sym_reply, fn->cont, fn->ret)->ast = ast;
    Vec<Sym *> as;
    as.add(sym___init__);
    as.add(fn->self);
    if1_closure(if1, fn, body, as.n, as.v);
  }
  // build constructor
  if (is_record) {
    fn = new_fun(ast);
    fn->nesting_depth = ctx.scope_stack.n;
    Vec<Sym *> as;
    as.add(new_sym(ast));
    as[0]->must_implement_and_specialize(ast->sym->meta_type);
    for (int i = 2; i < init_sym->has.n; i++)
      as.add(new_sym(ast));
    body = 0;
    Sym *t = new_sym(ast);
    if1_send(if1, &body, 3, 1, sym_primitive, sym_clone, proto, t)->ast = ast;
    Code *send = if1_send(if1, &body, 2, 1, sym___init__, t, new_sym(ast));
    send->ast = ast;
    for (int i = 2; i < init_sym->has.n; i++)
      if1_add_send_arg(if1, send, as[i-1]);
    if1_move(if1, &body, t, fn->ret);
    if1_send(if1, &body, 4, 0, sym_primitive, sym_reply, fn->cont, fn->ret)->ast = ast;
    if1_closure(if1, fn, body, as.n, as.v);
  }
}

static int 
get_stmts_code(asdl_seq *stmts, Code **code, PycContext &ctx) {
  for (int i = 0; i < asdl_seq_LEN(stmts); i++)
    if1_gen(if1, code, getAST((stmt_ty)asdl_seq_GET(stmts, i), ctx)->code);
  return 0;
}

static void
gen_if(PycAST *ifcond, asdl_seq *ifif, asdl_seq *ifelse, PycAST *ast, PycContext &ctx) {
  Code *ifif_code = 0, *ifelse_code = 0;
  get_stmts_code(ifif, &ifif_code, ctx);
  get_stmts_code(ifelse, &ifelse_code, ctx);
  ast->rval = new_sym(ast);
  if1_if(if1, &ast->code, ifcond->code, ifcond->rval, ifif_code, 0, ifelse_code, 0, 0, ast);
  if1_move(if1, &ast->code, sym_void, ast->rval, ast);
}

static void
gen_ifexpr(PycAST *ifcond, PycAST *ifif, PycAST *ifelse, PycAST *ast) {
  ast->rval = new_sym(ast);
  if1_if(if1, &ast->code, ifcond->code, ifcond->rval, ifif->code, ifif->rval,
         ifelse ? ifelse->code : 0, ifelse ? ifelse->rval : 0, 
         ast->rval, ast);
}

static Sym *make_num(PyObject *o) {
  assert(PyInt_Check(o));
  int i = (int)PyInt_AsLong(o);
  Immediate imm;
  imm.v_int32 = i;
  char s[80]; sprintf(s, "%d", i);
  Sym *sym = if1_const(if1, sym_int32, s, &imm);
  if (!sym->implements.n) {
    sym->implements.add(sym_int32);
    sym->specializes.add(sym_int32);
  }
  return sym;
}

static Sym *
make_symbol(char *name) {
  Sym *s = if1_make_symbol(if1, name);
  return s;
}

static Sym *map_operator(operator_ty op) {
  switch(op) {
    default: assert(!"case");    
    case Add: return make_symbol("__add__");
    case Sub: return make_symbol("__sub__");
    case Mult: return make_symbol("__mul__");
    case Div: return make_symbol("__div__");
    case Mod: return make_symbol("__mod__");
    case Pow: return make_symbol("__pow__");
    case LShift: return make_symbol("__lshift__");
    case RShift: return make_symbol("__rshift__");
    case BitOr: return make_symbol("__or__");
    case BitXor: return make_symbol("__xor__");
    case BitAnd: return make_symbol("__and__");
    case FloorDiv: return make_symbol("__floordiv__");
  }
  return 0;
}

static Sym *map_unary_operator(unaryop_ty op) {
  switch(op) {
    default: assert(!"case");    
    case Invert: return make_symbol("__invert__");
    case Not: return make_symbol("__not__"); // not official
      // what about abs?
    case UAdd: return make_symbol("__pos__");
    case USub: return make_symbol("__neg__");
  }
  return 0;
}

static Sym *map_cmp_operator(cmpop_ty op) {
  switch(op) {
    default: assert(!"case");    
    case Eq: return make_symbol("__eq__");
    case NotEq: return make_symbol("__ne__");
    case Lt: return make_symbol("__lt__");
    case LtE: return make_symbol("__le__");
    case Gt: return make_symbol("__gt__");
    case GtE: return make_symbol("__ge__");
    case Is: return make_symbol("__is__");
    case IsNot: return make_symbol("__nis__");
    case In: return make_symbol("__contains__");
    case NotIn: return make_symbol("__ncontains__");
  }
  return 0;
}

#define RECURSE(_ast, _fn, _ctx) \
  PycAST *ast = getAST(_ast, ctx); \
  forv_Vec(PycAST, x, ast->pre_scope_children) \
    if (x->xstmt) _fn(x->xstmt, ctx); else if (x->xexpr) _fn(x->xexpr, ctx); \
  enter_scope(_ast, ast, ctx);                                           \
  forv_Vec(PycAST, x, ast->children) \
    if (x->xstmt) _fn(x->xstmt, ctx); else if (x->xexpr) _fn(x->xexpr, ctx);

static int build_if1(stmt_ty s, PycContext &ctx);
static int build_if1(expr_ty e, PycContext &ctx);

static int
build_if1(stmt_ty s, PycContext &ctx) {
  RECURSE(s, build_if1, ctx);
  switch (s->kind) {
    case FunctionDef_kind: // identifier name, arguments args, stmt* body, expr* decorators
      gen_fun(s, ast, ctx);
      break;
    case ClassDef_kind: // identifier name, expr* bases, stmt* body
      gen_class_init(s, ast, ctx);
      break;
    case Return_kind: // expr? value
      if (s->v.Return.value) {
        PycAST *a = getAST(s->v.Return.value, ctx);
        ctx.fun()->fun_returns_value = 1;
        if1_gen(if1, &ast->code, a->code);
        if1_move(if1, &ast->code, a->rval, ctx.fun()->ret, ast);
      } else
        if1_move(if1, &ast->code, sym_void, ctx.fun()->ret, ast);
      if1_goto(if1, &ast->code, ast->label[0])->ast = ast;
      break;
    case Delete_kind: // expr * targets
      break;
    case Assign_kind: // expr* targets, expr value
    { 
      PycAST *v = getAST(s->v.Assign.value, ctx);
      if1_gen(if1, &ast->code, v->code);
      for (int i = 0; i < asdl_seq_LEN(s->v.Assign.targets); i++) {
        PycAST *a = getAST((expr_ty)asdl_seq_GET(s->v.Assign.targets, i), ctx);
        if1_gen(if1, &ast->code, a->code);
        if (a->is_member)
          if1_send(if1, &ast->code, 5, 1, sym_operator, 
                   a->rval, sym_setter, a->sym, v->rval, (ast->rval = new_sym(ast)))->ast = ast;
        else 
          if1_move(if1, &ast->code, v->rval, a->sym);
      }
      break;
    }
    case AugAssign_kind: // expr target, operator op, expr value
    {
      PycAST *v = getAST(s->v.AugAssign.value, ctx);
      if1_gen(if1, &ast->code, v->code);
      PycAST *t = getAST(s->v.AugAssign.target, ctx);
      if1_gen(if1, &ast->code, t->code);
      if (t->is_member) {
        Sym *tmp = new_sym(ast);
        Sym *tmp2 = new_sym(ast);
        if1_send(if1, &ast->code, 4, 1, sym_operator, t->rval, sym_period, t->sym, 
                 tmp2)->ast = ast;
        if1_send(if1, &ast->code, 3, 1, map_operator(s->v.AugAssign.op),
                 tmp2, v->rval, tmp)->ast = ast; 
        if1_send(if1, &ast->code, 5, 1, sym_operator, 
                 t->rval, sym_setter, t->sym, tmp, (ast->rval = new_sym(ast)))->ast = ast;
      } else {
        if1_send(if1, &ast->code, 3, 1, map_operator(s->v.AugAssign.op),
                 t->rval, v->rval, (ast->rval = new_sym(ast)))->ast = ast; 
        if1_move(if1, &ast->code, ast->rval, t->sym, ast);
      }
      break;
    }
    case Print_kind: // epxr? dest, expr *values, bool nl
      assert(!s->v.Print.dest);
      for (int i = 0; i < asdl_seq_LEN(s->v.Print.values); i++) {
        PycAST *a = getAST((expr_ty)asdl_seq_GET(s->v.Print.values, i), ctx);
        if1_gen(if1, &ast->code, a->code);
        if1_send(if1, &ast->code, 3, 1, sym_primitive, sym_write, 
                 a->rval, new_sym(ast))->ast = ast;
      }
      if (s->v.Print.nl)
        if1_send(if1, &ast->code, 3, 1, sym_primitive, sym_writeln, make_string(""), new_sym(ast))->ast = ast; 
      break;
    case For_kind: // expr target, expr iter, stmt* body, stmt* orelse
    {
      PycAST *t = getAST(s->v.For.target, ctx), *i = getAST(s->v.For.iter, ctx);
      Code *cond = 0, *body = 0, *orelse = 0, *next = 0;
      get_stmts_code(s->v.For.body, &body, ctx);
      get_stmts_code(s->v.For.orelse, &orelse, ctx);
      Sym *iter = new_sym(ast), *tmp = new_sym(ast), *tmp2 = new_sym(ast);
      if1_gen(if1, &ast->code, i->code);
      if1_send(if1, &ast->code, 2, 1, sym___iter__, i->rval, iter)->ast = ast; 
      if1_gen(if1, &ast->code, t->code);
      if1_send(if1, &next, 2, 1, sym_next, iter, tmp)->ast = ast;
      if1_send(if1, &cond, 3, 1, sym_operator, sym_exclamation, tmp, tmp2)->ast = ast;
      if1_move(if1, &next, tmp, t->sym);
      if1_loop(if1, &ast->code, ast->label[0], ast->label[1], 
               tmp2, 0, cond, next, body, ast);
      if1_gen(if1, &ast->code, orelse);
      break;
    }
    case While_kind: // expr test, stmt*body, stmt*orelse
    {
      PycAST *t = getAST(s->v.While.test, ctx);
      Code *body = 0, *orelse = 0;
      get_stmts_code(s->v.While.body, &body, ctx);
      get_stmts_code(s->v.While.orelse, &orelse, ctx);
      if1_loop(if1, &ast->code, ast->label[0], ast->label[1], 
               t->rval, 0, t->code, 0, body, ast);
      if1_gen(if1, &ast->code, orelse);
      break;
    }
    case If_kind: // expr test, stmt* body, stmt* orelse
      gen_if(getAST(s->v.If.test, ctx), s->v.If.body, s->v.If.orelse, ast, ctx);
      break;
    case With_kind: // expr content_expr, expr? optional_vars, stmt *body
    case Raise_kind: // expr? type, expr? int, expr? tback
    case TryExcept_kind: // stmt* body, excepthandler *handlers, stmt *orelse
    case TryFinally_kind: // stmt *body, stmt *finalbody
      fail("error line %d, exceptions not yet supported", ctx.lineno); break;
      break;
    case Assert_kind: // expr test, expr? msg
      fail("error line %d, 'assert' not yet supported", ctx.lineno); break;
    case Import_kind: // alias* name
      break;
    case ImportFrom_kind: // identifier module, alias *names, int? level
      break;
    case Exec_kind: // expr body, expr? globals, expr? locals
      fail("error line %d, 'exec' not yet supported", ctx.lineno); break;
    case Global_kind: // identifier* names
      break;
#if PY_MAJOR_VERSION == 3
    case Nonlocal_kind: break;
#endif
    case Expr_kind: // expr value
    {
      PycAST *a = getAST(s->v.Expr.value, ctx);
      ast->rval = a->rval;
      ast->code = a->code;
      break;
    }
    case Pass_kind: break;
    case Break_kind:
    case Continue_kind:
      if1_goto(if1, &ast->code, ast->label[0])->ast = ast;
      break;
  }
  exit_scope(s, ctx);
  return 0;
}

static int
build_builtin_call(PycAST *fun, expr_ty e, PycAST *ast, PycContext &ctx) {
  Sym *f = fun->sym;
  if (f && builtin_functions.set_in(f)) {
    if (f == sym_super) {
      if (!ctx.fun())
        fail("super outside of function");
      Vec<Sym *> as;
      get_syms_args(ast, ((PycAST*)ctx.fun()->ast)->xstmt->v.FunctionDef.args, as, ctx);
      if (as.n < 1 || !ctx.fun()->in || ctx.fun()->in->type_kind != Type_RECORD)
        fail("super outside of method");
      int n = asdl_seq_LEN(e->v.Call.args);
      if (!n) {
        ast->rval = new_sym(ast);
        ast->rval->aspect = ctx.cls();
        if1_move(if1, &ast->code, ctx.fun()->self, ast->rval);
      } else if (n == 1) {
        PycAST *cls_ast = getAST((expr_ty)asdl_seq_GET(e->v.Call.args, 0), ctx);
        if (!cls_ast->sym || cls_ast->sym->type_kind != Type_RECORD)
          fail("non-constant super() class");
        ast->rval = new_sym(ast);
        ast->rval->aspect = cls_ast->sym;
        if1_move(if1, &ast->code, as[0], ast->rval);
      } else {
        if (n > 2)
          fail("bad number of arguments to builtin function 'super'");
        PycAST *a0 = getAST((expr_ty)asdl_seq_GET(e->v.Call.args, 0), ctx);
        PycAST *a1 = getAST((expr_ty)asdl_seq_GET(e->v.Call.args, 1), ctx);
        if (!a0->sym || a0->sym->type_kind != Type_RECORD)
          fail("non-constant super() class");
        if (a1->sym && a0->sym->type_kind == Type_RECORD) {
          ast->rval = new_sym(ast);
          ast->rval->aspect = a0->sym;
          if1_move(if1, &ast->code, as[0], ast->rval);
        } else {
          ast->rval = new_sym(ast);
          ast->rval->aspect = a0->sym;
          if1_move(if1, &ast->code, a1->rval, ast->rval);
        }
      }
    } else
      fail("unimplemented builtin '%s'", fun->sym->name);
    return 1;
  } if (ast->is_builtin && fun->rval && fun->rval->is_constant && fun->rval->type == sym_string) {
    char *s = fun->rval->constant;
    if (*s == '#' && !strncmp(&s[1], "operator", 8)) {
      Code *send = if1_send1(if1, &ast->code, ast);
      if1_add_send_arg(if1, send, sym_operator);
      for (int i = 0; i < asdl_seq_LEN(e->v.Call.args); i++) {
        Sym *v = getAST((expr_ty)asdl_seq_GET(e->v.Call.args, i), ctx)->rval;
        if (v->is_constant && v->type == sym_string)
          if1_add_send_arg(if1, send, if1_make_symbol(if1, v->constant));
        else
          if1_add_send_arg(if1, send, v);
      }
      if1_add_send_result(if1, send, (ast->rval = new_sym(ast)));
      return 1;
    }
  }
  return 0;
}

static int
build_if1(expr_ty e, PycContext &ctx) {
  RECURSE(e, build_if1, ctx);
  switch (e->kind) {
    case BoolOp_kind: // boolop op, expr* values
    {
      bool a = e->v.BoolOp.op == And; (void)a;
      int n = asdl_seq_LEN(e->v.BoolOp.values);
      if (n == 1) {
        PycAST *v = getAST((expr_ty)asdl_seq_GET(e->v.BoolOp.values, 0), ctx);
        ast->code = v->code;
        ast->rval = v->rval;
      } else {
        ast->label[0] = if1_alloc_label(if1); // short circuit
        ast->label[1] = if1_alloc_label(if1); // end
        ast->rval = new_sym(ast);
        for (int i = 0; i < n - 1; i++) {
          PycAST *v = getAST((expr_ty)asdl_seq_GET(e->v.BoolOp.values, i), ctx);
          if1_gen(if1, &ast->code, v->code);
          Code *ifcode = if1_if_goto(if1, &ast->code, v->rval, ast);
          if (a) {
            if1_if_label_false(if1, ifcode, ast->label[0]);
            if1_if_label_true(if1, ifcode, if1_label(if1, &ast->code, ast));
          } else {
            if1_if_label_true(if1, ifcode, ast->label[0]);
            if1_if_label_false(if1, ifcode, if1_label(if1, &ast->code, ast));
          }
        }
        PycAST *v = getAST((expr_ty)asdl_seq_GET(e->v.BoolOp.values, n-1), ctx);
        if1_gen(if1, &ast->code, v->code);
        if1_move(if1, &ast->code, v->rval, ast->rval, ast);
        if1_goto(if1, &ast->code, ast->label[1]);
        if1_label(if1, &ast->code, ast, ast->label[0]);
        if (a)
          if1_move(if1, &ast->code, sym_false, ast->rval, ast); 
        else
          if1_move(if1, &ast->code, sym_true, ast->rval, ast); 
        if1_label(if1, &ast->code, ast, ast->label[1]);
      }
      break;
    }
    case BinOp_kind: // expr left, operator op, expr right
      ast->rval = new_sym(ast);
      if1_gen(if1, &ast->code, getAST(e->v.BinOp.left, ctx)->code);
      if1_gen(if1, &ast->code, getAST(e->v.BinOp.right, ctx)->code);
      if1_send(if1, &ast->code, 3, 1, map_operator(e->v.BinOp.op),
               getAST(e->v.BinOp.left, ctx)->rval,
               getAST(e->v.BinOp.right, ctx)->rval, ast->rval)->ast = ast; 
      break;
    case UnaryOp_kind: // unaryop op, expr operand
      ast->rval = new_sym(ast);
      if1_gen(if1, &ast->code, getAST(e->v.UnaryOp.operand, ctx)->code);
      if1_send(if1, &ast->code, 2, 1, map_unary_operator(e->v.UnaryOp.op), 
               getAST(e->v.UnaryOp.operand, ctx)->rval, ast->rval)->ast = ast;
      break;
    case Lambda_kind: // arguments args, expr body
      gen_fun(e, ast, ctx);
      break;
    case IfExp_kind: // expr test, expr body, expr orelse
      gen_ifexpr(getAST(e->v.IfExp.test, ctx), getAST(e->v.IfExp.body, ctx), getAST(e->v.IfExp.orelse, ctx), ast);
      break;
    case Dict_kind: // expr* keys, expr* values
      break;
    case ListComp_kind: // expr elt, comprehension* generators
      break;
    case GeneratorExp_kind: // expr elt, comprehension* generators
      break;
    case Yield_kind: // expr? value
      break;
    case Compare_kind: // expr left, cmpop* ops, expr* comparators
    {
      int n = asdl_seq_LEN(e->v.Compare.ops);
      ast->label[0] = if1_alloc_label(if1); // short circuit
      ast->label[1] = if1_alloc_label(if1); // end
      ast->rval = new_sym(ast);
      PycAST *lv = getAST(e->v.Compare.left, ctx);
      if1_gen(if1, &ast->code, lv->code);
      if (n == 1) {
        PycAST *v = getAST((expr_ty)asdl_seq_GET(e->v.Compare.comparators, 0), ctx);
        if1_gen(if1, &ast->code, v->code);
        if1_send(if1, &ast->code, 3, 1, 
                 map_cmp_operator((cmpop_ty)asdl_seq_GET(e->v.Compare.ops, 0)), 
                 lv->rval, v->rval, ast->rval)->ast = ast; 
      } else {
        Sym *ls = lv->rval, *s = 0;
        for (int i = 0; i < n; i++) {
          PycAST *v = getAST((expr_ty)asdl_seq_GET(e->v.Compare.comparators, i), ctx);
          if1_gen(if1, &ast->code, v->code);
          s = new_sym(ast);
          if1_send(if1, &ast->code, 3, 1, 
                   map_cmp_operator((cmpop_ty)asdl_seq_GET(e->v.Compare.ops, i)), 
                   ls, v->rval, s)->ast = ast; 
          ls = v->rval;
          Code *ifcode = if1_if_goto(if1, &ast->code, s, ast);
          if1_if_label_false(if1, ifcode, ast->label[0]);
          if1_if_label_true(if1, ifcode, if1_label(if1, &ast->code, ast));
        }
        if1_move(if1, &ast->code, s, ast->rval);
        if1_goto(if1, &ast->code, ast->label[1]);
        if1_label(if1, &ast->code, ast, ast->label[0]);
        if1_move(if1, &ast->code, sym_false, ast->rval, ast); 
        if1_label(if1, &ast->code, ast, ast->label[1]);
      }
      break;
    }
    case Call_kind: // expr func, expr* args, keyword* keywords, expr? starargs, expr? kwargs
    {
      PycAST *fun = getAST((expr_ty)e->v.Call.func, ctx);
      if1_gen(if1, &ast->code, fun->code);
      for (int i = 0; i < asdl_seq_LEN(e->v.Call.args); i++)
        if1_gen(if1, &ast->code, getAST((expr_ty)asdl_seq_GET(e->v.Call.args, i), ctx)->code);
      if (build_builtin_call(fun, e, ast, ctx))
        break;
      {
        Code *send = if1_send1(if1, &ast->code, ast);
        if (!fun->is_member)
          if1_add_send_arg(if1, send, fun->rval);
        else {
          if1_add_send_arg(if1, send, fun->sym);
          if1_add_send_arg(if1, send, fun->rval);
        }
        for (int i = 0; i < asdl_seq_LEN(e->v.Call.args); i++) {
          expr_ty arg = (expr_ty)asdl_seq_GET(e->v.Call.args, i);
          if1_add_send_arg(if1, send, getAST(arg, ctx)->rval);
        }
        ast->rval = new_sym(ast);
        if1_add_send_result(if1, send, ast->rval);
        send->partial = Partial_NEVER;
      }
      break;
    }
    case Repr_kind: // expr value
      fail("error line %d, 'repr' not yet supported", ctx.lineno); break;
      break;
    case Num_kind: ast->rval = make_num(e->v.Num.n); break;
    case Str_kind: ast->rval = make_string(e->v.Str.s); break;
    case Attribute_kind: // expr value, identifier attr, expr_context ctx
      if1_gen(if1, &ast->code, getAST(e->v.Attribute.value, ctx)->code);
      if ((ast->parent->is_assign() && ast->parent->children.last() != ast) || 
          (ast->parent->is_call() && ast->parent->xexpr->v.Call.func == e)) {
        ast->sym = make_symbol(PyString_AsString(e->v.Attribute.attr));
        ast->rval = getAST(e->v.Attribute.value, ctx)->rval;
        if (ast->rval->type_kind == Type_RECORD)
          ast->rval = ast->rval->self;
        ast->is_member = 1;
      } else {
        ast->rval = new_sym(ast);
        Sym *v = getAST(e->v.Attribute.value, ctx)->rval;
        if (v->type_kind == Type_RECORD)
          v = v->self;
        if1_send(if1, &ast->code, 4, 1, sym_operator, v, sym_period, 
                 make_symbol(PyString_AsString(e->v.Attribute.attr)), ast->rval)->ast = ast;
      }
      break;
    case Subscript_kind: // expr value, slice slice, expr_context ctx
      fail("error line %d, subscripting not yet supported", ctx.lineno); break;
      break;
    case Name_kind: // identifier id, expr_context ctx
    {
      int level = 0;
      DBG printf("%sfound '%s' at level %d\n", ast->sym ? "" : "not ",
                 if1_cannonicalize_string(if1, PyString_AS_STRING(e->v.Name.id)), level);
      bool load = e->v.Name.ctx == Load;
      Sym *in = ctx.scope_stack[ctx.scope_stack.n-1]->in;
      if (in && in->type_kind == Type_RECORD && in->has.in(ast->sym)) { // in __main__
        if (load)
          if1_send(if1, &ast->code, 4, 1, sym_operator, ctx.fun()->self, sym_period, 
                   make_symbol(ast->sym->name), (ast->rval = new_sym(ast)))->ast = ast;
        else {
          ast->is_member = 1;
          ast->sym = make_symbol(ast->sym->name);
          ast->rval = ctx.fun()->self;
        }
      }
      break;
    }
    case List_kind: // expr* elts, expr_context ctx
      // FALL THROUGH
    case Tuple_kind: // expr *elts, expr_context ctx
      for (int i = 0; i < asdl_seq_LEN(e->v.List.elts); i++)
        if1_gen(if1, &ast->code, getAST((expr_ty)asdl_seq_GET(e->v.List.elts, i), ctx)->code);
      {
        Code *send = if1_send1(if1, &ast->code, ast);
        if1_add_send_arg(if1, send, sym_primitive);
        if1_add_send_arg(if1, send, e->kind == List_kind ? sym_make_list : sym_make_tuple);
        for (int i = 0; i < asdl_seq_LEN(e->v.List.elts); i++) {
          expr_ty arg = (expr_ty)asdl_seq_GET(e->v.List.elts, i);
          if1_add_send_arg(if1, send, getAST(arg, ctx)->rval);
        }
        ast->rval = new_sym(ast);
        if1_add_send_result(if1, send, ast->rval);
      }
      break;
  }
  exit_scope(e, ctx);
  return 0;
}

static int build_if1_stmts(asdl_seq *stmts, PycContext &ctx, Code **code) {
  for (int i = 0; i < asdl_seq_LEN(stmts); i++) {
    if (build_if1((stmt_ty)asdl_seq_GET(stmts, i), ctx)) return -1;
    if1_gen(if1, code, getAST((stmt_ty)asdl_seq_GET(stmts, i), ctx)->code);
  }
  return 0;
}

static int
build_if1(mod_ty mod, PycContext &ctx, Code **code) {
  int r = 0;
  ctx.node = mod;
  enter_scope(ctx);
  switch (mod->kind) {
    case Module_kind: r = build_if1_stmts(mod->v.Module.body, ctx, code); break;
    case Expression_kind: {
      r = build_if1(mod->v.Expression.body, ctx); 
      if1_gen(if1, code, getAST(mod->v.Expression.body, ctx)->code);
      break;
    }
    case Interactive_kind: r = build_if1_stmts(mod->v.Interactive.body, ctx, code); break;
    case Suite_kind: assert(!"handled");
  }
  exit_scope(ctx);
  return r;
}

static void scope_sym(PycContext &ctx, Sym *sym) {
  PycSymbol *s = (PycSymbol*)sym->asymbol;
  ctx.scope_stack.last()->map.put(sym->name, s);
}

static void 
build_environment(mod_ty mod, PycContext &ctx) {
  ctx.node = mod;
  enter_scope(ctx);
  scope_sym(ctx, sym_int);
  scope_sym(ctx, sym_long);
  scope_sym(ctx, sym_float);
  scope_sym(ctx, sym_complex);
  scope_sym(ctx, sym_true);
  scope_sym(ctx, sym_false);
  scope_sym(ctx, sym_void);
  scope_sym(ctx, sym_unknown);
  scope_sym(ctx, sym_ellipsis);
  scope_sym(ctx, sym_object);
  scope_sym(ctx, sym_super);
  exit_scope(ctx);
}

static void build_init(Code *code) {
  Sym *fn = sym_init;
  fn->cont = new_sym();
  fn->ret = sym_void;
  if1_send(if1, &code, 4, 0, sym_primitive, sym_reply, fn->cont, fn->ret);
  if1_closure(if1, fn, code, 1, &fn);
}

static void
return_void_transfer_function(PNode *pn, EntrySet *es) {
  AVar *result = make_AVar(pn->lvals.v[0], es);
  update_gen(result, make_abstract_type(sym_void));
}

static void
add_primitive_transfer_functions() {
  pdb->fa->primitive_transfer_functions.put(
    sym_write->name, new RegisteredPrim(return_void_transfer_function));
  pdb->fa->primitive_transfer_functions.put(
    sym_writeln->name, new RegisteredPrim(return_void_transfer_function));
}

/*
  Sym::aspect is set by the code handling builtin 'super' to
  the class whose superclass we wish to dispatch to.  Replace
  with the dispatched-to class.
*/
static void
fixup_aspect() {
  for (int x = finalized_aspect; x < if1->allsyms.n; x++) {
    Sym *s = if1->allsyms.v[x];
    if (s->aspect) {
      if (s->aspect->dispatch_types.n < 2)
        fail("unable to dispatch to super of '%s'", s->aspect->name);
      s->aspect = s->aspect->dispatch_types.v[1];
    }
  }
  finalized_aspect = if1->allsyms.n;
}

static void
import_scope(PycContext &ctx, mod_ty mod) {
  ctx.imports.add(ctx.saved_scopes.get(mod));
}

int 
ast_to_if1(Vec<PycModule *> &mods) {
  ifa_init(new PycCallbacks);
  build_builtin_symbols();
  add_primitive_transfer_functions();
  PycContext ctx;
  Code *code = 0;
  forv_Vec(PycModule, x, mods)
    x->filename = if1_cannonicalize_string(if1, x->filename);
  ctx.filename = mods.v[0]->filename;
  ctx.is_builtin = mods.v[0]->is_builtin;
  build_environment(mods.v[0]->mod, ctx);
  forv_Vec(PycModule, x, mods) {
    ctx.filename = x->filename;
    ctx.is_builtin = x->is_builtin;
    if (!ctx.is_builtin) import_scope(ctx, mods.v[0]->mod);
    if (build_syms(x->mod, ctx) < 0) return -1;
  }
  finalize_types(if1);
  forv_Vec(PycModule, x, mods) {
    ctx.filename = x->filename;
    ctx.is_builtin = x->is_builtin;
    if (build_if1(x->mod, ctx, &code) < 0) return -1;
  }
  finalize_types(if1);
  if (test_scoping) exit(0);
  enter_scope(ctx, mods.v[0]->mod);  
  build_init(code);
  exit_scope(ctx);
  build_type_hierarchy();
  fixup_aspect();
  return 0;
}

/*
  Copyright 2008 John Plevyak, All Rights Reserved
*/
#include "defs.h"

/* TODO
   move static variables into an object
   "__bases__" "__class__" "super", "lambda"
   decorators (functions applied to functions)
*/

#define OPERATOR_CHAR(_c) \
(((_c > ' ' && _c < '0') || (_c > '9' && _c < 'A') || \
  (_c > 'Z' && _c < 'a') || (_c > 'z')) &&            \
   _c != '_'&& _c != '?' && _c != '$')                \

#define DBG if (debug_level)

static Map<stmt_ty, PycAST *> stmtmap;
static Map<expr_ty, PycAST *> exprmap;
static Sym *sym_long = 0, *sym_ellipsis = 0, *sym_unicode = 0, *sym_buffer = 0, *sym_xrange = 0;
static Sym *sym_write = 0, *sym_writeln = 0;
static char *cannonical_self = 0;

static int scope_id = 0;
static int finalized_symbols = 0;

static inline PycAST *getAST(stmt_ty s) {
  PycAST *ast = stmtmap.get(s);
  if (ast) return ast;
  ast = new PycAST;
  ast->xstmt = s;
  stmtmap.put(s, ast);
  return ast;
}

static inline PycAST *getAST(expr_ty e) {
  PycAST *ast = exprmap.get(e);
  if (ast) return ast;
  ast = new PycAST;
  ast->xexpr = e;
  exprmap.put(e, ast);
  return ast;
}

PycSymbol::PycSymbol() : symbol(0) {
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
  return 0;
}

int 
PycSymbol::line() {
  return 0;
}

int 
PycSymbol::source_line() {
  return line();
}

int 
PycSymbol::ast_id() {
  return 0;
}

PycAST::PycAST() : xstmt(0), xexpr(0), code(0), sym(0), rval(0) {
  label[0] = label[1] = 0;
}

char
*PycAST::pathname() { // TODO: dig up filename
  //return xast->filename;
  return "<python file>";
}

int
PycAST::line() {
  return xstmt ? xstmt->lineno : xexpr->lineno;
}

int
PycAST::source_line() { // TODO: return 0 for builtin code
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
  char *name = 0;
  if (arg0->is_symbol)
    name = arg0->name;
  else
    name = if1_cannonicalize_string(if1, "this");
  // TODO: finish
#if 0
  SymScope* scope = this->xast->parentScope;
  Vec<Symbol *> fss;
  scope->getVisibleFunctions(&fss, name);
  v = new Vec<Fun *>;
  forv_Vec(Symbol, x, fss)
    v->set_add(x->asymbol->sym->fun);
#endif
  return NULL;
}

static PycSymbol *
new_PycSymbol(char *name) {
  PycSymbol *s = new PycSymbol;
  s->sym = new Sym;
  s->sym->asymbol = s;
  if (name)
    if1_register_sym(if1, s->sym, name);
  return s;
}

static PycSymbol *
new_PycSymbol(Sym *sym) {
  if (!sym->asymbol)
    sym->asymbol = new PycSymbol;
  sym->asymbol->sym = sym;
  return (PycSymbol*)sym->asymbol;
}

#if 0
static PycSymbol *
new_PycSymbol(PyObject *o) {
  char *name = if1_cannonicalize_string(if1, PyString_AS_STRING(o));
  return new_PycSymbol(name);
}
#endif

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

static void
finalize_symbols(IF1 *i) {
  for (int x = finalized_symbols; x < i->allsyms.n; x++) {
    Sym *s = i->allsyms.v[x];
    if (s->is_constant || s->is_symbol)
      s->nesting_depth = 0;
    else if (s->type_kind)
      s->nesting_depth = 0;
    compute_type_size(s);
  }
  finalized_symbols = i->allsyms.n;
}

static Sym *
new_sym(char *name = 0, int global = 0) {
  Sym *s = new_PycSymbol(name)->sym;
  if (!global)
    s->nesting_depth = LOCALLY_NESTED;
  return s;
}

static void
new_primitive_type(Sym *&sym, char *name) {
  name = if1_cannonicalize_string(if1, name);
  if (!sym)
    sym = new_sym(name, 1);
  else
    sym->name = name;
  sym->type_kind = Type_PRIMITIVE;
  if1_set_builtin(if1, sym, name);
}

static void
new_global_variable(Sym *&sym, char *name) {
  if (!sym)
    sym = new_sym(name, 1);
  sym->nesting_depth = 0;
  if1_set_builtin(if1, sym, name);
}

static void
new_primitive_object(Sym *&sym, char *name, Sym *sym_type) {
  new_global_variable(sym, name);
  sym->type_kind = Type_NONE;
  sym->type = sym_type;
  sym->is_external = 1;
  sym_type->is_unique_type = 1;
}

static void
new_alias_type(Sym *&sym, char *name, Sym *alias) {
  if (!sym)
    sym = new_sym(name, 1);
  sym->type_kind = Type_ALIAS;
  sym->alias = alias;
  if1_set_builtin(if1, sym, name);
}

static void
new_lub_type(Sym *&sym, char *name, ...)  {
  if (!sym)
    sym = new_sym(name, 1);
  sym->type_kind = Type_LUB;
  if1_set_builtin(if1, sym, name);
  va_list ap;
  va_start(ap, name);
  Sym *s = 0;
  do {
    if ((s = va_arg(ap, Sym*)))
      sym->has.add(s);
  } while (s);
  forv_Sym(ss, sym->has)
    ss->inherits_add(sym);
}

static void
build_builtin_symbols() {
  sym_write = if1_make_symbol(if1, "write");
  sym_writeln = if1_make_symbol(if1, "writeln");
  cannonical_self = if1_cannonicalize_string(if1, "self");

#define S(_n) new_primitive_type(sym_##_n, #_n);
#include "builtin_symbols.h"
#undef S

  new_lub_type(sym_any, "any", 0);
  new_alias_type(sym_int, "int", sym_int32);
  new_alias_type(sym_long, "long", sym_int64); // standin for GNU gmp
  new_lub_type(sym_anyint, "anyint", sym_int32, sym_int64, 0);
  new_alias_type(sym_float, "float", sym_float64);
  new_lub_type(sym_anyfloat, "anyfloat", sym_float, 0);
  new_alias_type(sym_complex, "complex", sym_complex64);
  new_lub_type(sym_anycomplex, "anycomplex", sym_complex, 0);
  sym_object->type_kind = Type_RECORD;
  new_primitive_type(sym_true, "True");
  sym_true->inherits_add(sym_bool);
  new_primitive_type(sym_false, "False");
  sym_false->inherits_add(sym_bool);
  new_lub_type(sym_anynum, "anynum", sym_anyint, sym_anyfloat, sym_anycomplex, 0);

  new_primitive_type(sym_string, "str");
  new_primitive_type(sym_unicode, "unicode");
  // list && tuple already defined
  new_primitive_type(sym_buffer, "buffer");
  new_primitive_type(sym_xrange, "xrange");

  new_primitive_object(sym_void, "None", sym_void_type);
  new_primitive_object(sym_unknown, "Unimplemented", sym_unknown_type);
  new_primitive_object(sym_ellipsis, "Ellipsis", sym_void_type);

  sym_any->implements.add(sym_unknown_type);
  sym_any->specializes.add(sym_unknown_type);
  sym_object->implements.add(sym_any);
  sym_object->specializes.add(sym_any);
  sym_nil_type->implements.add(sym_object);
  sym_nil_type->specializes.add(sym_object);

  make_meta_type(sym_any);
  sym_anytype = sym_any->meta_type;
  sym_anytype->implements.add(sym_any);
  sym_anytype->specializes.add(sym_any);

  make_meta_type(sym_nil_type);
  sym_nil_type->implements.add(sym_nil_type->meta_type);
  sym_nil_type->specializes.add(sym_nil_type->meta_type);

  sym_any->is_system_type = 1;
  sym_anytype->is_system_type = 1;
  sym_value->is_system_type = 1;
  sym_object->is_system_type = 1;
  sym_nil_type->is_system_type = 1;
  sym_unknown_type->is_system_type = 1;
  sym_void_type->is_system_type = 1;
  
  if1_set_symbols_type(if1);
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

static void add(asdl_seq *seq, Vec<expr_ty> &exprs) {
  for (int i = 0; i < asdl_seq_LEN(seq); i++)
    exprs.add((expr_ty)asdl_seq_GET(seq, i));
}

static void add(asdl_seq *seq, Vec<stmt_ty> &stmts) {
  for (int i = 0; i < asdl_seq_LEN(seq); i++)
    stmts.add((stmt_ty)asdl_seq_GET(seq, i));
}

static void add(expr_ty e, Vec<expr_ty> &exprs) { if (e) exprs.add(e); }
//static void add(stmt_ty s, Vec<stmt_ty> &stmts) { if (s) stmts.add(s); } unused

static void add_comprehension(asdl_seq *comp, Vec<expr_ty> &exprs) {
  int l = asdl_seq_LEN(comp);
  for (int i = 0; i < l; i++) {
    comprehension_ty c = (comprehension_ty)asdl_seq_GET(comp, i);
    add(c->target, exprs);
    if (i != 0)
      add(c->iter, exprs);
    add(c->ifs, exprs);
  }
}

static void get_pre_scope_next(stmt_ty s, Vec<stmt_ty> &stmts, Vec<expr_ty> &exprs) {
  switch (s->kind) {
    default: 
      break;
    case FunctionDef_kind:
      add(s->v.FunctionDef.args->defaults, exprs);
      add(s->v.FunctionDef.decorators, exprs);
      break;
    case ClassDef_kind:
      add(s->v.ClassDef.bases, exprs);
      break;
  }
}

static void get_next(stmt_ty s, Vec<stmt_ty> &stmts, Vec<expr_ty> &exprs) {
  switch (s->kind) {
    default: assert(!"case");
    case FunctionDef_kind:
      add(s->v.FunctionDef.args->args, exprs);
      add(s->v.FunctionDef.body, stmts);
      break;
    case ClassDef_kind:
      add(s->v.ClassDef.body, stmts);
      break;
    case Return_kind: add(s->v.Return.value, exprs); break;
    case Delete_kind: add(s->v.Delete.targets, exprs); break;
    case Assign_kind: add(s->v.Assign.targets, exprs); add(s->v.Assign.value, exprs); break;
    case AugAssign_kind: add(s->v.AugAssign.target, exprs); add(s->v.AugAssign.value, exprs); break;
    case Print_kind: add(s->v.Print.dest, exprs); add(s->v.Print.values, exprs); break;
    case For_kind:
      add(s->v.For.target, exprs); 
      add(s->v.For.iter, exprs); 
      add(s->v.For.body, stmts); 
      add(s->v.For.orelse, stmts); 
      break;
    case While_kind:
      add(s->v.While.test, exprs); 
      add(s->v.While.body, stmts); 
      add(s->v.While.orelse, stmts); 
      break;
    case If_kind:
      add(s->v.If.test, exprs); 
      add(s->v.If.body, stmts); 
      add(s->v.If.orelse, stmts); 
      break;
    case With_kind:
      add(s->v.With.context_expr, exprs); 
      add(s->v.With.optional_vars, exprs); 
      add(s->v.With.body, stmts); 
      break;
    case Raise_kind:
      add(s->v.Raise.type, exprs); 
      add(s->v.Raise.inst, exprs); 
      add(s->v.Raise.tback, exprs); 
      break;
    case TryExcept_kind: {
      add(s->v.TryExcept.body, stmts); 
      for (int i = 0; i < asdl_seq_LEN(s->v.TryExcept.handlers); i++) {
        excepthandler_ty h = (excepthandler_ty)asdl_seq_GET(s->v.TryExcept.handlers, i);
        add(h->type, exprs);
        add(h->name, exprs);
        add(h->body, stmts);
      }
      add(s->v.TryExcept.orelse, stmts); 
      break;
    }
    case TryFinally_kind: add(s->v.TryFinally.body, stmts); add(s->v.TryFinally.finalbody, stmts); break;
    case Assert_kind: add(s->v.Assert.test, exprs); add(s->v.Assert.msg, exprs); break;
    case Import_kind: break;
    case ImportFrom_kind: break;
    case Exec_kind: 
      add(s->v.Exec.body, exprs); 
      add(s->v.Exec.globals, exprs); 
      add(s->v.Exec.locals, exprs); 
      break;
    case Global_kind: break;
#if PY_MAJOR_VERSION == 3
    case Nonlocal_kind: break;
#endif
    case Expr_kind: add(s->v.Expr.value, exprs); break;
    case Pass_kind:
    case Break_kind:
    case Continue_kind:
      break;
  }
}

static void get_next(slice_ty s, Vec<stmt_ty> &stmts, Vec<expr_ty> &exprs) {
  switch (s->kind) {
    default: assert(!"case");
    case Ellipsis_kind: break;
    case Slice_kind: // (expr? lower, expr? upper, expr? step)
      add(s->v.Slice.lower, exprs); 
      add(s->v.Slice.upper, exprs); 
      add(s->v.Slice.step, exprs); 
      break;
    case ExtSlice_kind: // (slice* dims)
      for (int i = 0; i < asdl_seq_LEN(s->v.ExtSlice.dims); i++)
        get_next((slice_ty)asdl_seq_GET(s->v.ExtSlice.dims, i), stmts, exprs);
      break;
    case Index_kind: // (expr value)
      add(s->v.Index.value, exprs); break;
  }
}

static void get_pre_scope_next(expr_ty e, Vec<stmt_ty> &stmts, Vec<expr_ty> &exprs) {
  switch (e->kind) {
    default: 
      break;
    case Lambda_kind:
      add(e->v.Lambda.args->defaults, exprs);
      break;
    case Dict_kind: // expr* keys, expr* values
#if PY_MAJOR_VERSION == 3
      add(((comprehension_ty)asdl_seq_GET(e->v.Dict.generators, 0))->iter, exprs);
#endif
      break;
    case ListComp_kind: // expr elt, comprehension* generators
      add(((comprehension_ty)asdl_seq_GET(e->v.ListComp.generators, 0))->iter, exprs);
      break;
#if PY_MAJOR_VERSION == 3
    case SetComp_kind:
      add(((comprehension_ty)asdl_seq_GET(e->v.SetComp.generators, 0))->iter, exprs);
      break;
#endif
    case GeneratorExp_kind: // expr elt, comprehension* generators
      add(((comprehension_ty)asdl_seq_GET(e->v.GeneratorExp.generators, 0))->iter, exprs);
      break;
  }
}

static void get_next(expr_ty e, Vec<stmt_ty> &stmts, Vec<expr_ty> &exprs) {
  switch (e->kind) {
    default: assert(!"case");
    case BoolOp_kind: // boolop op, expr* values
      add(e->v.BoolOp.values, exprs); break;
    case BinOp_kind: // expr left, operator op, expr right
      add(e->v.BinOp.left, exprs);
      add(e->v.BinOp.right, exprs);
      break;
    case UnaryOp_kind: // unaryop op, expr operand
      add(e->v.UnaryOp.operand, exprs); break;
    case Lambda_kind: // arguments args, expr body
      add(e->v.Lambda.args->args, exprs);
      add(e->v.Lambda.body, exprs); 
      break;
    case IfExp_kind: // expr test, expr body, expr orelse
      add(e->v.IfExp.test, exprs); 
      add(e->v.IfExp.body, exprs); 
      add(e->v.IfExp.orelse, exprs); 
      break;
    case Dict_kind: // expr* keys, expr* values
#if PY_MAJOR_VERSION == 3
      add_comprehension(e->v.Dict.generators, exprs);
      add(e->v.Dict.value, exprs); 
      add(e->v.Dict.key, exprs); 
#else
      add(e->v.Dict.keys, exprs); 
      add(e->v.Dict.values, exprs); 
#endif
      break;
    case ListComp_kind: // expr elt, comprehension* generators
      add_comprehension(e->v.ListComp.generators, exprs);
      add(e->v.ListComp.elt, exprs); 
      break;
#if PY_MAJOR_VERSION == 3
    case SetComp_kind:
      add_comprehension(e->v.SetComp.generators, exprs);
      add(e->v.ListComp.elt, exprs); 
      break;
#endif
    case GeneratorExp_kind: // expr elt, comprehension* generators
      add_comprehension(e->v.GeneratorExp.generators, exprs);
      add(e->v.GeneratorExp.elt, exprs); 
      break;
    case Yield_kind: // expr? value
      add(e->v.Yield.value, exprs); break;
    case Compare_kind: // expr left, cmpop* ops, expr* comparators
      add(e->v.Compare.left, exprs);
      add(e->v.Compare.comparators, exprs);
      break;
    case Call_kind: // expr func, expr* args, keyword* keywords, expr? starargs, expr? kwargs
    case Repr_kind: // expr value
     add(e->v.Repr.value, exprs); break;
    case Num_kind: // object n) -- a number as a PyObject
    case Str_kind: // string s) -- need to specify raw, unicode, etc
      break;
    case Attribute_kind: // expr value, identifier attr, expr_context ctx
      add(e->v.Attribute.value, exprs); break;
    case Subscript_kind: // expr value, slice slice, expr_context ctx
     add(e->v.Subscript.value, exprs); 
     get_next(e->v.Subscript.slice, stmts, exprs);
     break;
    case Name_kind: // identifier id, expr_context ctx
      break;
    case List_kind: // expr* elts, expr_context ctx
     add(e->v.List.elts, exprs); break;
    case Tuple_kind: // expr *elts, expr_context ctx
     add(e->v.Tuple.elts, exprs); break;
  }
}

struct PycScope : public gc {
  int id;
  Map<char *, PycSymbol*> map;
  PycScope() { id = scope_id++; } 
};

struct PycContext : public gc {
  int lineno;
  int depth;
  void *node;
  Sym *class_sym;
  Vec<PycScope *> scope_stack;
  Label *lbreak, *lcontinue, *lreturn, *lyield;
  Map<void *, PycScope*> saved_scopes;
  PycContext() : lineno(-1), depth(-1), class_sym(0), lbreak(0), lcontinue(0), lreturn(0) {}
};

static void enter_scope(PycContext &ctx) {
  ctx.depth++;
  PycScope *saved = ctx.saved_scopes.get(ctx.node);
  if (!saved) {
    saved = new PycScope;
    ctx.saved_scopes.put(ctx.node, saved);
  }
  ctx.scope_stack.add(saved);
  DBG printf("enter scope %d level %d\n", ctx.scope_stack.last()->id, ctx.depth);
}

static void enter_scope(stmt_ty x, PycContext &ctx) {
  ctx.node = x;
  if (x->kind == FunctionDef_kind || x->kind == ClassDef_kind)
    enter_scope(ctx);
}

static int needs_scope(expr_ty x) {
  return (x->kind == Lambda_kind || x->kind == GeneratorExp_kind ||
          x->kind == Dict_kind || x->kind == ListComp_kind 
#if PY_MAJOR_VERSION == 3
          x->kind == SetComp_kind
#endif
    );
}

static void enter_scope(expr_ty x, PycContext &ctx) {
  ctx.node = x;
  if (needs_scope(x)) enter_scope(ctx);
}

static void exit_scope(PycContext &ctx) { 
  DBG printf("exit scope %d level %d\n", ctx.scope_stack.last()->id, ctx.depth);
  ctx.scope_stack.pop(); 
  ctx.depth--;
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

#define GLOBAL_USE ((PycSymbol*)(intptr_t)-1)
#define NONLOCAL_USE ((PycSymbol*)(intptr_t)-2)
#define GLOBAL_DEF ((PycSymbol*)(intptr_t)-3)
#define NONLOCAL_DEF ((PycSymbol*)(intptr_t)-4)

static PycSymbol *find_PycSymbol(PycContext &ctx, char *name, int *level = 0, int *type = 0) {
  PycSymbol *l = 0;
  int i = ctx.scope_stack.n - 1, xtype = 0;
  for (;i >= 0; i--) {
    bool top = i == ctx.scope_stack.n - 1;
    if ((l = ctx.scope_stack.v[i]->map.get(name))) {
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

static PycSymbol *find_PycSymbol(PycContext &ctx, PyObject *o, int *level = 0, int *type = 0) {
  return find_PycSymbol(ctx, if1_cannonicalize_string(if1, PyString_AS_STRING(o)), level, type);
}

static PycSymbol *make_PycSymbol(PycContext &ctx, char *n, PYC_SCOPINGS scoping) {
  char *name = if1_cannonicalize_string(if1, n);
  DBG printf("make_PycSymbol %s '%s'\n", pyc_scoping_names[(int)scoping], name);
  int level = 0, type = 0;
  PycSymbol *l = find_PycSymbol(ctx, name, &level, &type);
  bool local = l && (ctx.scope_stack.n - 1 == level); // implies !explicitly && !implicitly
  bool global = l && !level;
  bool nonlocal = l && level && !local;
  bool explicitly = type == EXPLICITLY_MARKED;
  bool implicitly = type == IMPLICITLY_MARKED;
  switch (scoping) {
    case PYC_USE: {
      if (!l) goto Llocal; // not found
      if (!local && !explicitly) {
        if (global)
          ctx.scope_stack.last()->map.put(name, GLOBAL_USE);
        else
          ctx.scope_stack.last()->map.put(name, NONLOCAL_USE);
      }
      break;
    }
    case PYC_LOCAL:
    Llocal:
      if (local || explicitly) break;
      if (implicitly)
        fail("error line %d, '%s' redefined as local", ctx.lineno, name);
      ctx.scope_stack.last()->map.put(name, (l = new_PycSymbol(name)));
      break;
    case PYC_GLOBAL:
      if (l && !global && (local || explicitly || implicitly))
        fail("error line %d, '%s' redefined as global", ctx.lineno, name);
      if (!global) {
        PycSymbol *g = ctx.scope_stack.v[0]->map.get(name);
        if (!g)
          ctx.scope_stack.v[0]->map.put(name, (l = new_PycSymbol(name)));
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

#define AST_RECURSE(_ast, _fn, _ctx)                 \
  PycAST *ast = getAST(_ast); \
  {                                                                       \
    Vec<stmt_ty> stmts; Vec<expr_ty> exprs; get_pre_scope_next(_ast, stmts, exprs); \
    for_Vec(stmt_ty, x, stmts) { _fn(x, _ctx); ast->pre_scope_children.add(getAST(x)); } \
    for_Vec(expr_ty, x, exprs) { _fn(x, _ctx); ast->pre_scope_children.add(getAST(x)); } \
  } \
  enter_scope(_ast, _ctx);                                                   \
  {                                                                       \
    Vec<stmt_ty> stmts; Vec<expr_ty> exprs; get_next(_ast, stmts, exprs); \
    for_Vec(stmt_ty, x, stmts) { _fn(x, _ctx); ast->children.add(getAST(x)); } \
    for_Vec(expr_ty, x, exprs) { _fn(x, _ctx); ast->children.add(getAST(x)); } \
  }

static void build_syms_args(PycAST *a, arguments_ty args, Vec<Sym *> &has) {
  for (int i = 0; i < asdl_seq_LEN(args->args); i++) {
#if PY_MAJOR_VERSION == 3
    assert(!"incomplete");
#else
    Sym *sym = getAST((expr_ty)asdl_seq_GET(args->args, i))->sym;
#endif
    has.add(sym);
  }
}

static void
def_fun(stmt_ty s, PycContext &ctx) {
  PycAST *ast = getAST(s);
  Sym *fn = ast->sym = make_PycSymbol(ctx, s->v.FunctionDef.name, PYC_LOCAL)->sym;
  fn->ast = ast;
  if (ctx.class_sym) {
    fn->self = make_PycSymbol(ctx, cannonical_self, PYC_LOCAL)->sym;
    fn->self->is_read_only = 1;
    fn->self->ast = ast;
    fn->self->must_implement_and_specialize(ctx.class_sym);
  }
  ctx.lreturn = ast->label[0] = if1_alloc_label(if1);
  fn->cont = new_sym();
  fn->ret = new_sym();
  fn->ret->ast = ast;
}

static int
build_syms(stmt_ty s, PycContext &ctx) {
  ctx.node = s;
  ctx.lineno = s->lineno;
  PycAST *past = getAST(s);
  switch (s->kind) {
    default: break;
    case FunctionDef_kind: // identifier name, arguments args, stmt* body, expr* decorators
      def_fun(s, ctx);
      break;
    case ClassDef_kind: // identifier name, expr* bases, stmt* body
      past->sym = make_PycSymbol(ctx, s->v.ClassDef.name, PYC_LOCAL)->sym;
      past->sym->type_kind = Type_RECORD;
      ctx.class_sym = past->sym;
      break;
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
      ctx.lcontinue = past->label[0] = if1_alloc_label(if1);
      ctx.lbreak = past->label[1] = if1_alloc_label(if1);
      break;
  }
  AST_RECURSE(s, build_syms, ctx);
  switch (s->kind) {
    default: break;
    case ClassDef_kind: // identifier name, expr* bases, stmt* body
      for (int i = 0; i < asdl_seq_LEN(s->v.ClassDef.bases); i++) {
        Sym *base = getAST((expr_ty)asdl_seq_GET(s->v.ClassDef.bases, i))->sym;
        if (!base)
          fail("error line %d, base not for for class '%s'", ctx.lineno, past->sym->name);
        past->sym->specializes.add(getAST((expr_ty)asdl_seq_GET(s->v.ClassDef.bases, i))->sym);
      }
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
  PycAST *past = getAST(e);
  ctx.node = e;
  ctx.lineno = e->lineno;
  switch (e->kind) {
    default: break;
    case Lambda_kind: // arguments args, expr body
      past->sym = new_sym();
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
      ast->sym = ast->rval = new_sym();
      for (int i = 0; i < asdl_seq_LEN(e->v.Tuple.elts); i++)
        ast->sym->has.add(getAST((expr_ty)asdl_seq_GET(e->v.Tuple.elts, i))->sym);
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
gen_fun(stmt_ty s, PycContext &ctx) {
  PycAST *ast = getAST(s);
  Sym *fn = ast->sym;
  Code *body = 0;
  for (int i = 0; i < asdl_seq_LEN(s->v.FunctionDef.body); i++)
    if1_gen(if1, &body, getAST((stmt_ty)asdl_seq_GET(s->v.FunctionDef.body, i))->code);
  if1_move(if1, &body, sym_void, fn->ret, ast);
  if1_label(if1, &body, ast, ast->label[0]);
  if1_send(if1, &body, 4, 0, sym_primitive, sym_reply, fn->cont, fn->ret)->ast = ast;
  Vec<Sym *> as;
  as.add(fn);
  if (fn->self)
    as.add(fn->self);
  build_syms_args(ast, s->v.FunctionDef.args, as);
  if1_closure(if1, fn, body, as.n, as.v);
}

#define RECURSE(_ast, _fn) \
  PycAST *ast = getAST(_ast); \
  forv_Vec(PycAST, x, ast->pre_scope_children) \
    if (x->xstmt) _fn(x->xstmt, ctx); else if (x->xexpr) _fn(x->xexpr, ctx); \
  enter_scope(_ast, ctx); \
  forv_Vec(PycAST, x, ast->children) \
    if (x->xstmt) _fn(x->xstmt, ctx); else if (x->xexpr) _fn(x->xexpr, ctx);

static int build_if1(stmt_ty s, PycContext &ctx);
static int build_if1(expr_ty e, PycContext &ctx);

static int
build_if1(stmt_ty s, PycContext &ctx) {
  RECURSE(s, build_if1);
  switch (s->kind) {
    case FunctionDef_kind: // identifier name, arguments args, stmt* body, expr* decorators
      gen_fun(s, ctx);
      break;
    case ClassDef_kind: // identifier name, expr* bases, stmt* body
      break;
    case Return_kind: // expr? value
      break;
    case Delete_kind: // expr * targets
      break;
    case Assign_kind: // expr* targets, expr value
      break;
    case AugAssign_kind: // expr target, operator op, expr value
      break;
    case Print_kind: // epxr? dest, expr *values, bool nl
      assert(!s->v.Print.dest);
      for (int i = 0; i < asdl_seq_LEN(s->v.Print.values); i++)
        if1_send(if1, &ast->code, 3, 1, sym_primitive, sym_write, 
                 getAST((expr_ty)asdl_seq_GET(s->v.Print.values, i))->rval, new_sym())->ast = ast;
      if (s->v.Print.nl)
        if1_send(if1, &ast->code, 3, 1, sym_primitive, sym_writeln, make_string(""), new_sym())->ast = ast; 
      break;
    case For_kind: // expr target, expr, iter, stmt* body, stmt* orelse
      break;
    case While_kind: // expr test, stmt*body, stmt*orelse
      break;
    case If_kind: // expr tet, stmt* body, stmt* orelse
      break;
    case With_kind: // expr content_expr, expr? optional_vars, stmt *body
      break;
    case Raise_kind: // expr? type, expr? int, expr? tback
      break;
    case TryExcept_kind: // stmt* body, excepthandler *handlers, stmt *orelse
      break;
    case TryFinally_kind: // stmt *body, stmt *finalbody
      break;
    case Assert_kind: // expr test, expr? msg
      break;
    case Import_kind: // alias* name
      break;
    case ImportFrom_kind: // identifier module, alias *names, int? level
      break;
    case Exec_kind: // expr body, expr? globals, expr? locals
      break;
    case Global_kind: // identifier* names
      break;
#if PY_MAJOR_VERSION == 3
    case Nonlocal_kind: break;
#endif
    case Expr_kind: // expr value
    {
      PycAST *a = getAST(s->v.Expr.value);
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

static int
build_if1(expr_ty e, PycContext &ctx) {
  RECURSE(e, build_if1);
  switch (e->kind) {
    case BoolOp_kind: // boolop op, expr* values
      break;
    case BinOp_kind: // expr left, operator op, expr right
      break;
    case UnaryOp_kind: // unaryop op, expr operand
      break;
    case Lambda_kind: // arguments args, expr body
      break;
    case IfExp_kind: // expr test, expr body, expr orelse
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
      break;
    case Call_kind: // expr func, expr* args, keyword* keywords, expr? starargs, expr? kwargs
      if1_gen(if1, &ast->code, getAST((expr_ty)e->v.Call.func)->code);
      for (int i = 0; i < asdl_seq_LEN(e->v.Call.args); i++)
        if1_gen(if1, &ast->code, getAST((expr_ty)asdl_seq_GET(e->v.Call.args, i))->code);
      {
        Code *send = if1_send1(if1, &ast->code);
        send->ast = ast;
        if1_add_send_arg(if1, send, getAST((expr_ty)e->v.Call.func)->rval);
        for (int i = 0; i < asdl_seq_LEN(e->v.Call.args); i++)
          if1_add_send_arg(if1, send, getAST((expr_ty)asdl_seq_GET(e->v.Call.args, i))->rval);
        ast->rval = new_sym();
        if1_add_send_result(if1, send, ast->rval);
      }
      break;
    case Repr_kind: // expr value
      break;
    case Num_kind: ast->rval = make_num(e->v.Num.n); break;
    case Str_kind: ast->rval = make_string(e->v.Str.s); break;
    case Attribute_kind: // expr value, identifier attr, expr_context ctx
      break;
    case Subscript_kind: // expr value, slice slice, expr_context ctx
      break;
    case Name_kind: // identifier id, expr_context ctx
    {
      int level = 0;
      PycSymbol *s = find_PycSymbol(ctx, e->v.Name.id, &level);
      DBG printf("%sfound '%s' at level %d\n", s ? "" : "not ",
                 if1_cannonicalize_string(if1, PyString_AS_STRING(e->v.Name.id)), level);
      break;
    }
    case List_kind: // expr* elts, expr_context ctx
      break;
    case Tuple_kind: // expr *elts, expr_context ctx
      break;
  }
  exit_scope(e, ctx);
  return 0;
}

static int build_if1_stmts(asdl_seq *stmts, PycContext &ctx, Code **code) {
  for (int i = 0; i < asdl_seq_LEN(stmts); i++) {
    if (build_if1((stmt_ty)asdl_seq_GET(stmts, i), ctx)) return -1;
    if1_gen(if1, code, getAST((stmt_ty)asdl_seq_GET(stmts, i))->code);
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
      if1_gen(if1, code, getAST(mod->v.Expression.body)->code);
      break;
    }
    case Interactive_kind: r = build_if1_stmts(mod->v.Interactive.body, ctx, code); break;
    case Suite_kind: assert(!"handled");
  }
  exit_scope(ctx);
  return r;
}

static void scope_sym(PycContext &ctx, Sym *sym) {
  PycSymbol *s = new_PycSymbol(sym);
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

int 
ast_to_if1(mod_ty module) {
  ifa_init(new PycCallbacks);
  build_builtin_symbols();
  add_primitive_transfer_functions();
  PycContext ctx;
  build_environment(module, ctx);
  if (build_syms(module, ctx) < 0) return -1;
  if1_set_primitive_types(if1);
  finalize_types(if1, false);
  Code *code = 0;
  if (build_if1(module, ctx, &code) < 0) return -1;
  build_init(code);
  finalize_symbols(if1);
  build_type_hierarchy();
  finalize_types(if1, false);  // again to catch any new ones
  return 0;
}

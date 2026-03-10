/*
  Copyright 2008-2011 John Plevyak, All Rights Reserved
*/
#include "python_ifa_int.h"

PycSymbol *new_PycSymbol(cchar *name) {
  PycSymbol *s = new PycSymbol;
  s->sym = new Sym;
  s->sym->asymbol = s;
  if1_register_sym(if1, s->sym, name);
  return s;
}

PycSymbol *new_PycSymbol(cchar *name, PycCompiler &ctx) {
  PycSymbol *s = new_PycSymbol(name);
  s->filename = ctx.filename;
  return s;
}

PycSymbol *PycSymbol::copy() {
  PycSymbol *s = new PycSymbol;
  s->sym = sym->copy();
  s->sym->asymbol = s;
  if (s->sym->type_kind != Type_NONE) s->sym->type = s->sym;
  return s;
}

Sym *PycCompiler::new_Sym(cchar *name) { return new_PycSymbol(name)->sym; }

Sym *new_sym(cchar *name, int global) {
  Sym *s = new_PycSymbol(name)->sym;
  if (!global) s->nesting_depth = LOCALLY_NESTED;
  return s;
}

Sym *new_sym(PycAST *ast, int global) {
  Sym *s = new_PycSymbol(0)->sym;
  s->ast = ast;
  s->is_local = !global;
  if (s->is_local) s->nesting_depth = LOCALLY_NESTED;
  return s;
}

Sym *new_sym(PycAST *ast, cchar *name, int global) {
  Sym *s = new_PycSymbol(name)->sym;
  s->ast = ast;
  s->is_local = !global;
  if (s->is_local) s->nesting_depth = LOCALLY_NESTED;
  return s;
}

Sym *new_global(PycAST *ast, cchar *name) {
  Sym *sym = new_PycSymbol(name)->sym;
  sym->ast = ast;
  sym->nesting_depth = 0;
  return sym;
}

Sym *new_base_instance(Sym *c, PycAST *ast) {
  if (c->type_kind == Type_PRIMITIVE) {
    if (c->num_kind) return if1_const(if1, c, "0");
    if (c == sym_string) return if1_const(if1, c, "");
  }
  if (c == sym_nil_type) return sym_nil;
  if (c == sym_list) return sym_empty_list;
  if (c == sym_tuple) return sym_empty_tuple;
  if (c == sym_any) return NULL;
  fail("no instance for type '%s' found", c->name);
  return 0;
}

void build_builtin_symbols() {
#define S(_x) sym_##_x = if1_make_symbol(if1, #_x);
#include "pyc_symbols.h"

  cannonical_self = cannonicalize_string("self");

  init_default_builtin_types();

  new_builtin_global_variable(sym___main__, "__main__");
  new_builtin_global_variable(sym_declare, "pyc__declare__");

  // override default sizes
  sym_int->alias = sym_int64;
  sym_float->alias = sym_float64;
  sym_complex->alias = sym_complex64;
  sym_size->alias = sym_int64;
  sym_char->alias = sym_string;

  // override default names
  sym_string->name = cannonicalize_string("str");
  sym_nil->name = cannonicalize_string("None");
  sym_nil_type->name = cannonicalize_string("__pyc_None_type__");
  sym_unknown->name = cannonicalize_string("NotImplemented");
  sym_true->name = cannonicalize_string("True");
  sym_false->name = cannonicalize_string("False");
  sym_any->name = cannonicalize_string("__pyc_any_type__");

  // new types and objects
  new_builtin_primitive_type(sym_unicode, "unicode");
  new_builtin_primitive_type(sym_list, "list");
  new_builtin_primitive_type(sym_tuple, "tuple");
  new_builtin_primitive_type(sym_buffer, "buffer");
  new_builtin_primitive_type(sym_xrange, "xrange");
  new_builtin_primitive_type(sym_ellipsis_type, "Ellipsis_type");
  new_builtin_alias_type(sym_long, "long", sym_int64);  // standin for GNU gmp
  new_builtin_unique_object(sym_ellipsis, "Ellipsis", sym_ellipsis_type);
  sym_ellipsis_type->is_unique_type = 1;

#define B(_x) builtin_functions.set_add(sym_##_x);
#include "pyc_symbols.h"

  sym_list->element = new_sym();
  sym_vector->element = new_sym();
}

static void finalize_function(Fun *f) {
  Sym *fn = f->sym;
  if (!f->ast) return;  // __main__
  PycAST *a = (PycAST *)f->ast;
  // Handle pyda path
  if (a->xpyd) {
    PyDAST *funcnode = a->xpyd;
    if (funcnode->kind == PY_classdef || funcnode->kind == PY_decorated) return;
    PyDAST *varargslist = nullptr;
    if (funcnode->kind == PY_funcdef && funcnode->children.n >= 2) {
      PyDAST *params = funcnode->children[1];  // PY_parameters
      if (params->children.n > 0) varargslist = params->children[0];
    } else if (funcnode->kind == PY_lambda) {
      if (funcnode->children.n > 0 && funcnode->children[0]->kind == PY_varargslist)
        varargslist = funcnode->children[0];
    }
    if (!varargslist) return;
    Vec<PycAST *> defaults;
    for (auto c : varargslist->children.values())
      if (c->kind == PY_arg_default) defaults.add(getAST(c->children[1], a));
    if (!defaults.n) return;
    int skip = fn->has.n - defaults.n;
    assert(skip >= 0);
    MPosition p;
    p.push(skip + 1);
    for (int i = 0; i < defaults.n; i++) {
      fn->fun->default_args.put(cannonicalize_mposition(p), defaults[i]);
      p.inc();
    }
    return;
  }
  arguments_ty args = 0;
  if (a->xstmt) {
    stmt_ty s = a->xstmt;
    if (s->kind != FunctionDef_kind) {
      if (!fn->init || ((PycAST *)fn->init->ast)->xstmt->kind != FunctionDef_kind) return;
      s = ((PycAST *)fn->init->ast)->xstmt;
    }
    args = s->v.FunctionDef.args;
  } else if (a->xexpr && a->xexpr->kind == Lambda_kind)
    args = a->xexpr->v.Lambda.args;
  if (args) {
    int defaults_len = args ? asdl_seq_LEN(args->defaults) : 0;
    if (defaults_len) {
      int skip = fn->has.n - defaults_len;
      assert(skip >= 0);
      MPosition p;
      p.push(skip + 1);
      for (int i = 0; i < fn->has.n - skip; i++) {
        fn->fun->default_args.put(cannonicalize_mposition(p), getAST((expr_ty)asdl_seq_GET(args->defaults, i), a));
        p.inc();
      }
    }
  }
}

void PycCompiler::finalize_functions() { for (auto fun : pdb->funs.values()) finalize_function(fun); }

Sym *new_fun(PycAST *ast, Sym *fun) {
  if (!fun)
    fun = new_sym(ast, 1);
  else
    fun->ast = ast;
  if (!fun->name && ast->rval && ast->rval->name) fun->name = ast->rval->name;
  fun->is_fun = 1;
  fun->cont = new_sym(ast);
  fun->ret = new_sym(ast);
  return fun;
}

Fun *PycCompiler::default_wrapper(Fun *f, Vec<MPosition *> &default_args) {
  PycAST *ast = (PycAST *)f->ast;
  Sym *fn = new_fun(ast);
  fn->nesting_depth = f->sym->nesting_depth;
  Vec<Sym *> as;
  int n = f->sym->has.n - default_args.n;
  MPosition pos;
  pos.push(1);
  Code *body = 0;
  Sym *ret = new_sym(ast);
  Code *send = if1_send(if1, &body, 0, 1, ret);
  send->ast = ast;
  for (int i = 0; i < n; i++) {
    Sym *a = new_sym(ast);
    as.add(a);
    if1_add_send_arg(if1, send, a);
    pos.inc();
  }
  for (int i = 0; i < default_args.n; i++) {
    if1_add_send_arg(if1, send, ((PycAST *)f->default_args.get(cannonicalize_mposition(pos)))->sym);
    pos.inc();
  }
  if1_move(if1, &body, ret, fn->ret, ast);
  if1_send(if1, &body, 4, 0, sym_primitive, sym_reply, fn->cont, fn->ret)->ast = ast;
  if1_closure(if1, fn, body, as.n, as.v);
  install_new_fun(fn);
  fn->fun->wraps = f;
  return fn->fun;
}

bool PycCompiler::reanalyze(Vec<ATypeViolation *> &type_violations) {
  if (!type_violations.n) return false;
  bool again = false;
  for (auto v : type_violations.values()) if (v) {
    if (v->kind == ATypeViolation_NOTYPE) {
      if (!v->av->var->def || v->av->var->def->rvals.n < 2) continue;
      AVar *av = make_AVar(v->av->var->def->rvals[1], (EntrySet *)v->av->contour);
      for (auto cs : av->out->sorted.values()) {
        for (auto i : cs->unknown_vars.values()) {
          if (cs->var_map.get(i)) continue;
          again = true;
          Sym *s = new_PycSymbol(i)->sym;
          s->var = new Var(s);
          cs->sym->has.add(s);
          AVar *iv = unique_AVar(s->var, cs);
          add_var_constraint(iv);
          cs->vars.add(iv);
          cs->var_map.put(i, iv);
        }
      }
    }
  }
  return again;
}

bool PycCompiler::c_codegen_pre_file(FILE *fp) {
  for (int i = 0; i < c_code.n; i++) {
    fputs(c_code[i], fp);
    fputs("\n", fp);
  }
  return true;
}

void add(expr_ty e, Vec<PycAST *> &asts, PycCompiler &ctx) {
  if (e) asts.add(getAST(e, ctx));
}
// static void add(stmt_ty s, Vec<PycAST*> &asts, PycCompiler &ctx) { if (s) stmts.add(getAST(s)); } unused

void add_comprehension(asdl_seq *comp, Vec<PycAST *> &asts, PycCompiler &ctx) {
  int l = asdl_seq_LEN(comp);
  for (int i = 0; i < l; i++) {
    comprehension_ty c = (comprehension_ty)asdl_seq_GET(comp, i);
    add(c->target, asts, ctx);
    if (i != 0) add(c->iter, asts, ctx);
    add<expr_ty>(c->ifs, asts, ctx);
  }
}

void get_pre_scope_next(stmt_ty s, Vec<PycAST *> &asts, PycCompiler &ctx) {
  switch (s->kind) {
    default:
      break;
    case FunctionDef_kind:
      add<expr_ty>(s->v.FunctionDef.args->defaults, asts, ctx);
      add<expr_ty>(s->v.FunctionDef.decorator_list, asts, ctx);
      break;
    case ClassDef_kind:
      add<expr_ty>(s->v.ClassDef.bases, asts, ctx);
      break;
  }
}

void get_next(stmt_ty s, Vec<PycAST *> &asts, PycCompiler &ctx) {
  switch (s->kind) {
    default:
      assert(!"case");
    case FunctionDef_kind:
      add<expr_ty>(s->v.FunctionDef.args->args, asts, ctx);
      add<stmt_ty>(s->v.FunctionDef.body, asts, ctx);
      break;
    case ClassDef_kind:
      add<stmt_ty>(s->v.ClassDef.body, asts, ctx);
      break;
    case Return_kind:
      add(s->v.Return.value, asts, ctx);
      break;
    case Delete_kind:
      add<expr_ty>(s->v.Delete.targets, asts, ctx);
      break;
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
        add(h->v.ExceptHandler.type, asts, ctx);
        add(h->v.ExceptHandler.name, asts, ctx);
        add<stmt_ty>(h->v.ExceptHandler.body, asts, ctx);
      }
      add<stmt_ty>(s->v.TryExcept.orelse, asts, ctx);
      break;
    }
    case TryFinally_kind:
      add<stmt_ty>(s->v.TryFinally.body, asts, ctx);
      add<stmt_ty>(s->v.TryFinally.finalbody, asts, ctx);
      break;
    case Assert_kind:
      add(s->v.Assert.test, asts, ctx);
      add(s->v.Assert.msg, asts, ctx);
      break;
    case Import_kind:
      break;
    case ImportFrom_kind:
      break;
    case Exec_kind:
      add(s->v.Exec.body, asts, ctx);
      add(s->v.Exec.globals, asts, ctx);
      add(s->v.Exec.locals, asts, ctx);
      break;
    case Global_kind:
      break;
#if PY_MAJOR_VERSION == 3
    case Nonlocal_kind:
      break;
#endif
    case Expr_kind:
      add(s->v.Expr.value, asts, ctx);
      break;
    case Pass_kind:
    case Break_kind:
    case Continue_kind:
      break;
  }
}

void get_next(slice_ty s, Vec<PycAST *> &asts, PycCompiler &ctx) {
  switch (s->kind) {
    default:
      assert(!"case");
    case Ellipsis_kind:
      break;
    case Slice_kind:  // (expr? lower, expr? upper, expr? step)
      add(s->v.Slice.lower, asts, ctx);
      add(s->v.Slice.upper, asts, ctx);
      add(s->v.Slice.step, asts, ctx);
      break;
    case ExtSlice_kind:  // (slice* dims)
      for (int i = 0; i < asdl_seq_LEN(s->v.ExtSlice.dims); i++)
        get_next((slice_ty)asdl_seq_GET(s->v.ExtSlice.dims, i), asts, ctx);
      break;
    case Index_kind:  // (expr value)
      add(s->v.Index.value, asts, ctx);
      break;
  }
}

void get_pre_scope_next(expr_ty e, Vec<PycAST *> &asts, PycCompiler &ctx) {
  switch (e->kind) {
    default:
      break;
    case Lambda_kind:
      add<expr_ty>(e->v.Lambda.args->defaults, asts, ctx);
      break;
    case Dict_kind:  // expr* keys, expr* values
#if PY_MAJOR_VERSION == 3
      add(((comprehension_ty)asdl_seq_GET(e->v.Dict.generators, 0))->iter, asts, ctx);
#endif
      break;
    case ListComp_kind:  // expr elt, comprehension* generators
      add(((comprehension_ty)asdl_seq_GET(e->v.ListComp.generators, 0))->iter, asts, ctx);
      break;
#if PY_MAJOR_VERSION == 3
    case SetComp_kind:
      add(((comprehension_ty)asdl_seq_GET(e->v.SetComp.generators, 0))->iter, asts, ctx);
      break;
#endif
    case GeneratorExp_kind:  // expr elt, comprehension* generators
      add(((comprehension_ty)asdl_seq_GET(e->v.GeneratorExp.generators, 0))->iter, asts, ctx);
      break;
  }
}

void get_next(expr_ty e, Vec<PycAST *> &asts, PycCompiler &ctx) {
  switch (e->kind) {
    default:
      assert(!"case");
    case BoolOp_kind:  // boolop op, expr* values
      add<expr_ty>(e->v.BoolOp.values, asts, ctx);
      break;
    case BinOp_kind:  // expr left, operator op, expr right
      add(e->v.BinOp.left, asts, ctx);
      add(e->v.BinOp.right, asts, ctx);
      break;
    case UnaryOp_kind:  // unaryop op, expr operand
      add(e->v.UnaryOp.operand, asts, ctx);
      break;
    case Lambda_kind:  // arguments args, expr body
      add<expr_ty>(e->v.Lambda.args->args, asts, ctx);
      add(e->v.Lambda.body, asts, ctx);
      break;
    case IfExp_kind:  // expr test, expr body, expr orelse
      add(e->v.IfExp.test, asts, ctx);
      add(e->v.IfExp.body, asts, ctx);
      add(e->v.IfExp.orelse, asts, ctx);
      break;
    case Dict_kind:  // expr* keys, expr* values
#if PY_MAJOR_VERSION == 3
      add_comprehension(e->v.Dict.generators, asts, ctx);
      add(e->v.Dict.value, asts, ctx);
      add(e->v.Dict.key, asts, ctx);
#else
      add<expr_ty>(e->v.Dict.keys, asts, ctx);
      add<expr_ty>(e->v.Dict.values, asts, ctx);
#endif
      break;
    case ListComp_kind:  // expr elt, comprehension* generators
      add_comprehension(e->v.ListComp.generators, asts, ctx);
      add(e->v.ListComp.elt, asts, ctx);
      break;
#if PY_MAJOR_VERSION == 3
    case SetComp_kind:
      add_comprehension(e->v.SetComp.generators, asts, ctx);
      add(e->v.ListComp.elt, asts, ctx);
      break;
#endif
    case GeneratorExp_kind:  // expr elt, comprehension* generators
      add_comprehension(e->v.GeneratorExp.generators, asts, ctx);
      add(e->v.GeneratorExp.elt, asts, ctx);
      break;
    case Yield_kind:  // expr? value
      add(e->v.Yield.value, asts, ctx);
      break;
    case Compare_kind:  // expr left, cmpop* ops, expr* comparators
      add(e->v.Compare.left, asts, ctx);
      add<expr_ty>(e->v.Compare.comparators, asts, ctx);
      break;
    case Call_kind:  // expr func, expr* args, keyword* keywords, expr? starargs, expr? kwargs
      add(e->v.Call.func, asts, ctx);
      add<expr_ty>(e->v.Call.args, asts, ctx);
      add(e->v.Call.starargs, asts, ctx);
      add(e->v.Call.kwargs, asts, ctx);
      break;
    case Repr_kind:  // expr value
      add(e->v.Repr.value, asts, ctx);
      break;
    case Num_kind:  // object n) -- a number as a PyObject
    case Str_kind:  // string s) -- need to specify raw, unicode, etc
      break;
    case Attribute_kind:  // expr value, identifier attr, expr_context ctx
      add(e->v.Attribute.value, asts, ctx);
      break;
    case Subscript_kind:  // expr value, slice slice, expr_context ctx
      add(e->v.Subscript.value, asts, ctx);
      get_next(e->v.Subscript.slice, asts, ctx);
      break;
    case Name_kind:  // identifier id, expr_context ctx
      break;
    case List_kind:  // expr* elts, expr_context ctx
      add<expr_ty>(e->v.List.elts, asts, ctx);
      break;
    case Tuple_kind:  // expr *elts, expr_context ctx
      add<expr_ty>(e->v.Tuple.elts, asts, ctx);
      break;
  }
}

void enter_scope(PycCompiler &ctx, Sym *in) {
  PycScope *c = ctx.saved_scopes.get(ctx.node);
  if (!c) {
    c = new PycScope;
    c->in = in;
    if (ctx.scope_stack.n) {
      c->fun = ctx.scope_stack.last()->fun;
      c->cls = ctx.scope_stack.last()->cls;
    }
    if (in) {
      if (in->is_fun || (in->alias && in->alias->is_fun))
        c->fun = in;
      else
        c->cls = in;
    }
    ctx.saved_scopes.put(ctx.node, c);
  }
  ctx.scope_stack.add(c);
  TEST_SCOPE printf("enter scope level %d\n", ctx.scope_stack.n);
}

void enter_scope(PycCompiler &ctx, mod_ty mod) {
  ctx.node = mod;
  enter_scope(ctx);
}

void enter_scope(stmt_ty x, PycAST *ast, PycCompiler &ctx) {
  ctx.node = x;
  if (x->kind == FunctionDef_kind)
    enter_scope(ctx, ast->rval);
  else if (x->kind == ClassDef_kind)
    enter_scope(ctx, ast->sym);
}

int needs_scope(expr_ty x) {
  return (x->kind == Lambda_kind || x->kind == GeneratorExp_kind || x->kind == Dict_kind ||
          x->kind ==
              ListComp_kind
#if PY_MAJOR_VERSION == 3
                  x->kind == SetComp_kind
#endif
  );
}

void enter_scope(expr_ty x, PycAST *ast, PycCompiler &ctx) {
  ctx.node = x;
  if (needs_scope(x)) enter_scope(ctx, ast->rval);
}

void exit_scope(PycCompiler &ctx) {
  TEST_SCOPE printf("exit scope level %d\n", ctx.scope_stack.n);
  ctx.scope_stack.pop();
}

void exit_scope(stmt_ty x, PycCompiler &ctx) {
  ctx.node = x;
  if (x->kind == FunctionDef_kind || x->kind == ClassDef_kind) exit_scope(ctx);
}

void exit_scope(expr_ty x, PycCompiler &ctx) {
  ctx.node = x;
  if (needs_scope(x)) exit_scope(ctx);
}

// PyDAST scope helpers
void enter_scope(PyDAST *n, PycCompiler &ctx, Sym *in) {
  ctx.node = n;
  enter_scope(ctx, in);
}

static cchar *pyc_scoping_names[] = {"use", "local", "global", "nonlocal"};

PycSymbol *find_PycSymbol(PycCompiler &ctx, cchar *name, int *level, int *type) {
  PycSymbol *l = 0;
  int i = ctx.scope_stack.n - 1, xtype = 0;
  int end = -ctx.imports.n;
  for (; i >= end; i--) {
    bool top = i == ctx.scope_stack.n - 1;
    PycScope *s = i >= 0 ? ctx.scope_stack[i] : ctx.imports.v[-i - 1];
    if ((l = s->map.get(name))) {
      if (l == NONLOCAL_USE || l == NONLOCAL_DEF) {
        if (top) xtype = (l == NONLOCAL_DEF) ? EXPLICITLY_MARKED : IMPLICITLY_MARKED;
        continue;
      }
      if (l == GLOBAL_USE || l == GLOBAL_DEF) {
        assert(i > end);
        if (top) xtype = (l == GLOBAL_DEF) ? EXPLICITLY_MARKED : IMPLICITLY_MARKED;
        i = i > 1 ? 1 : i;
        continue;
      }
      break;
    }
  }
  if (level) *level = i;
  if (type) *type = xtype;
  return l;
}

// static PycSymbol *find_PycSymbol(PycCompiler &ctx, PyObject *o, int *level = 0, int *type = 0) {
//  return find_PycSymbol(ctx, cannonicalize_string(PyString_AS_STRING(o)), level, type);
//}

PycSymbol *make_PycSymbol(PycCompiler &ctx, cchar *n, PYC_SCOPINGS scoping) {
  cchar *name = cannonicalize_string(n);
  TEST_SCOPE printf("make_PycSymbol %s '%s'\n", pyc_scoping_names[(int)scoping], name);
  int level = 0, type = 0;
  PycSymbol *l = find_PycSymbol(ctx, name, &level, &type), *previous = l;
  bool local = l && (ctx.scope_stack.n - 1 == level);  // implies !explicitly && !implicitly
  bool global = l && !level;
  bool nonlocal = l && !global && !local;
  bool explicitly = type == EXPLICITLY_MARKED;
  bool implicitly = type == IMPLICITLY_MARKED;
  bool isfun = l && (l->sym->is_fun || (l->sym->alias && l->sym->alias->is_fun));
  switch (scoping) {
    case PYC_USE: {
      if (!l) goto Lglobal;  // not found
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
      if (implicitly && !isfun) fail("error line %d, '%s' redefined as local", ctx.lineno, name);
      ctx.scope_stack.last()->map.put(name, (l = new_PycSymbol(name, ctx)));
      if (local && previous) l->previous = previous;
      break;
    case PYC_GLOBAL:
    Lglobal:;
      if (l && !global && (local || explicitly || implicitly))
        fail("error line %d, '%s' redefined as global", ctx.lineno, name);
      if (!global) {
        PycSymbol *g = ctx.scope_stack[0]->map.get(name);
        if (!g) ctx.scope_stack[0]->map.put(name, (l = new_PycSymbol(name, ctx)));
      }
      if (!explicitly && !(ctx.scope_stack.n == 1)) ctx.scope_stack.last()->map.put(name, GLOBAL_DEF);
      break;
    case PYC_NONLOCAL:
      if (!l || (!nonlocal && (local || explicitly || implicitly)))
        fail("error line %d, '%s' nonlocal redefined or not found", ctx.lineno, name);
      ctx.scope_stack.last()->map.put(name, NONLOCAL_DEF);
      break;
  }
  return l;
}

PycSymbol *make_PycSymbol(PycCompiler &ctx, PyObject *o, PYC_SCOPINGS type) {
  return make_PycSymbol(ctx, PyString_AS_STRING(o), type);
}

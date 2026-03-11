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
    if (funcnode->kind == PY_classdef || funcnode->kind == PY_decorated) {
      // ___init___ or __new__ wrapper from gen_class_pyda.
      // For __new__ functions, fn->init is the __init__ sym; get defaults from it.
      if (!fn->init || !fn->init->ast) return;
      PycAST *init_ast = (PycAST *)fn->init->ast;
      if (init_ast->xpyd) {
        PyDAST *init_funcnode = init_ast->xpyd;
        if (init_funcnode->kind != PY_funcdef) return;
        PyDAST *varargslist = nullptr;
        if (init_funcnode->children.n >= 2) {
          PyDAST *params = init_funcnode->children[1];
          if (params->children.n > 0) varargslist = params->children[0];
        }
        if (!varargslist) return;
        Vec<PycAST *> defaults;
        for (auto c : varargslist->children.values())
          if (c->kind == PY_arg_default) defaults.add(getAST(c->children[1], init_ast));
        if (!defaults.n) return;
        int skip = fn->has.n - defaults.n;
        assert(skip >= 0);
        MPosition p;
        p.push(skip + 1);
        for (int i = 0; i < defaults.n; i++) {
          fn->fun->default_args.put(cannonicalize_mposition(p), defaults[i]);
          p.inc();
        }
      }
      return;
    }
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


void exit_scope(PycCompiler &ctx) {
  TEST_SCOPE printf("exit scope level %d\n", ctx.scope_stack.n);
  ctx.scope_stack.pop();
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

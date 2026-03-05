/*
  Copyright 2008-2011 John Plevyak, All Rights Reserved
*/
#include "python_ifa_int.h"
#include "python_parse.h"


static int build_syms(stmt_ty s, PycCompiler &ctx);
static int build_syms(expr_ty e, PycCompiler &ctx);
int build_syms(PycModule *x, PycCompiler &ctx);

#define AST_RECURSE_PRE(_ast, _fn, _ctx)  \
  PycAST *ast = getAST(_ast, _ctx);       \
  {                                       \
    Vec<PycAST *> asts;                   \
    get_pre_scope_next(_ast, asts, _ctx); \
    for (auto x : asts.values()) {        \
      x->parent = ast;                    \
      ast->pre_scope_children.add(x);     \
      if (x->xstmt)                       \
        _fn(x->xstmt, _ctx);              \
      else                                \
        _fn(x->xexpr, ctx);               \
    }                                     \
  }

#define AST_RECURSE_POST(_ast, _fn, _ctx) \
  {                                       \
    Vec<PycAST *> asts;                   \
    get_next(_ast, asts, _ctx);           \
    for (auto x : asts.values()) {        \
      x->parent = ast;                    \
      ast->children.add(x);              \
      if (x->xstmt)                       \
        _fn(x->xstmt, _ctx);              \
      else                                \
        _fn(x->xexpr, ctx);               \
    }                                     \
  }

#define AST_RECURSE(_ast, _fn, _ctx) \
  AST_RECURSE_PRE(_ast, _fn, _ctx)   \
  enter_scope(_ast, ast, _ctx);      \
  AST_RECURSE_POST(_ast, _fn, _ctx)

void get_syms_args(PycAST *a, arguments_ty args, Vec<Sym *> &has, PycCompiler &ctx, asdl_seq *decorators) {
  for (int i = 0; i < asdl_seq_LEN(args->args); i++) {
#if PY_MAJOR_VERSION == 3
    assert(!"incomplete");
#else
    Sym *sym = getAST((expr_ty)asdl_seq_GET(args->args, i), ctx)->sym;
#endif
    has.add(sym);
  }
  if (ctx.is_builtin() && decorators) {
    for (int j = 0; j < asdl_seq_LEN(decorators); j++) {
      expr_ty e = (expr_ty)asdl_seq_GET(decorators, j);
      if (e->kind == Call_kind) {
        printf("%d\n", e->v.Call.func->kind);
      }
    }
  }
}

static Sym *def_fun(stmt_ty s, PycAST *ast, Sym *fn, PycCompiler &ctx) {
  fn->in = ctx.scope_stack.last()->in;
  new_fun(ast, fn);
  enter_scope(s, ast, ctx);
  ctx.scope_stack.last()->fun = fn;
  fn->nesting_depth = ctx.scope_stack.n - 1;
  ctx.lreturn() = ast->label[0] = if1_alloc_label(if1);
  return fn;
}

static Sym *def_fun(expr_ty e, PycAST *ast, PycCompiler &ctx) {
  Sym *fn = new_sym(ast, 1);
  fn->in = ctx.scope_stack.last()->in;
  enter_scope(e, ast, ctx);
  ctx.scope_stack.last()->fun = new_fun(ast, fn);
  fn->nesting_depth = ctx.scope_stack.n - 1;
  ctx.lreturn() = ast->label[0] = if1_alloc_label(if1);
  return fn;
}

static void import_file(cchar *name, cchar *p, PycCompiler &ctx) {
  cchar *f = dupstrs(p, "/", name, ".py");
  mod_ty mod = file_to_mod(f, ctx.arena);
  PycModule *m = new PycModule(mod, f);
  m->pymod = dparse_python_to_ast(f);
  ctx.modules->add(m);
  // Save state that build_syms modifies
  PycModule *saved_mod = ctx.mod;
  cchar *saved_filename = ctx.filename;
  int saved_imports_n = ctx.imports.n;
  Vec<PycScope *> saved_scope_stack;
  saved_scope_stack.move(ctx.scope_stack);  // hide outer scopes; scope_stack is now empty
  build_syms(m, ctx);
  // Restore state (scope_stack is empty: enter/exit are balanced in build_syms)
  ctx.scope_stack.move(saved_scope_stack);
  ctx.imports.n = saved_imports_n;
  ctx.filename = saved_filename;
  ctx.mod = saved_mod;
}

PycModule *get_module(cchar *name, PycCompiler &ctx) {
  for (auto m : ctx.modules->values()) {
    if (!strcmp(name, m->name)) return m;
  }
  return 0;
}

static void build_import_syms(char *sym, char *as, char *from, PycCompiler &ctx) {
  char *mod = from ? from : sym;
  assert(!strchr(mod, '.'));  // package
  assert(!ctx.package);       // package
  if (!strcmp(mod, "pyc_compat")) return;
  PycModule *m = get_module(mod, ctx);
  if (!m) {
    for (auto p : ctx.search_path->values()) {
      if (file_exists(p, "/__init__.py")) continue;  // package
      if (!is_regular_file(p, "/", mod, ".py")) continue;
      import_file(mod, p, ctx);
      break;
    }
  }
}

void build_import(stmt_ty s, import_fn fn, PycCompiler &ctx) {
  for (int i = 0; i < asdl_seq_LEN(s->v.Import.names); i++) {
    alias_ty a = (alias_ty)asdl_seq_GET(s->v.Import.names, i);
    fn(PyString_AsString(a->name), a->asname ? PyString_AsString(a->asname) : 0, 0, ctx);
  }
}

void build_import_from(stmt_ty s, import_fn fn, PycCompiler &ctx) {
  for (int i = 0; i < asdl_seq_LEN(s->v.ImportFrom.names); i++) {
    alias_ty a = (alias_ty)asdl_seq_GET(s->v.ImportFrom.names, i);
    fn(PyString_AsString(a->name), a->asname ? PyString_AsString(a->asname) : 0,
       PyString_AsString(s->v.ImportFrom.module), ctx);
  }
}

static int build_syms(stmt_ty s, PycCompiler &ctx) {
  ctx.node = s;
  ctx.lineno = s->lineno;
  AST_RECURSE_PRE(s, build_syms, ctx);
  switch (s->kind) {
    default:
      break;
    case FunctionDef_kind: {  // identifier name, arguments args, stmt* body, expr* decorators
      if (ctx.in_class() && ctx.cls()->type_kind == Type_RECORD) {
        ast->rval = make_PycSymbol(ctx, PyString_AS_STRING(s->v.FunctionDef.name), PYC_LOCAL)->sym;
        ast->sym = new_sym(ast, 1);
        ast->rval->alias = ast->sym;
        ast->sym = def_fun(s, ast, ast->sym, ctx);
        if1_send(if1, &ast->code, 5, 1, sym_operator, ctx.cls()->self, sym_setter,
                 if1_make_symbol(if1, ast->rval->name), ast->sym, new_sym(ast))
            ->ast = ast;

      } else
        ast->rval = ast->sym =
            def_fun(s, ast, make_PycSymbol(ctx, PyString_AS_STRING(s->v.FunctionDef.name), PYC_LOCAL)->sym, ctx);
      break;
    }
    case ClassDef_kind: {  // identifier name, expr* bases, stmt* body
      PYC_SCOPINGS scope = (ctx.is_builtin() && ctx.scope_stack.n == 1) ? PYC_GLOBAL : PYC_LOCAL;
      ast->sym = unalias_type(make_PycSymbol(ctx, s->v.ClassDef.name, scope)->sym);
      if (!ast->sym->is_constant) {
        if (!ast->sym->type_kind) ast->sym->type_kind = Type_RECORD;  // do not override
        if (ast->sym->type_kind == Type_RECORD)
          ast->sym->self = new_global(ast);  // prototype
        else
          ast->sym->self = new_base_instance(ast->sym, ast);
      } else
        ast->sym->self = ast->sym;
      Sym *fn = new_sym(ast, "___init___", 1);  // builtin constructor
      ast->rval = def_fun(s, ast, fn, ctx);
      ast->rval->self = new_sym(ast);
      ast->rval->self->must_implement_and_specialize(ast->sym);
      ast->rval->self->in = fn;
      break;
    }
    case Global_kind:  // identifier* names
      for (int i = 0; i < asdl_seq_LEN(s->v.Global.names); i++)
        make_PycSymbol(ctx, (PyObject *)asdl_seq_GET(s->v.Global.names, i), PYC_GLOBAL);
      break;
#if PY_MAJOR_VERSION == 3
    case Nonlocal_kind:
      for (int i = 0; i < asdl_seq_LEN(s->v.Global.names); i++)
        make_PycSymbol(ctx, (PyObject *)asdl_seq_GET(s->v.Global.names, i), PYC_NONLOCAL);
      break;
#endif
    case For_kind:
    case While_kind:
      ctx.lcontinue() = ast->label[0] = if1_alloc_label(if1);
      ctx.lbreak() = ast->label[1] = if1_alloc_label(if1);
      break;
  }
  AST_RECURSE_POST(s, build_syms, ctx);
  switch (s->kind) {
    default:
      break;
    case FunctionDef_kind:
      form_Map(MapCharPycSymbolElem, x, ctx.scope_stack.last()->map) if (!MARKED(x->value) && !x->value->sym->is_fun) {
        x->value->sym->is_local = 1;
        x->value->sym->nesting_depth = LOCALLY_NESTED;
      }
      break;
    case ClassDef_kind:  // identifier name, expr* bases, stmt* body
      for (int i = 0; i < asdl_seq_LEN(s->v.ClassDef.bases); i++) {
        Sym *base = getAST((expr_ty)asdl_seq_GET(s->v.ClassDef.bases, i), ctx)->sym;
        if (!base) fail("error line %d, base not for for class '%s'", ctx.lineno, ast->sym->name);
        ast->sym->inherits_add(getAST((expr_ty)asdl_seq_GET(s->v.ClassDef.bases, i), ctx)->sym);
      }
      form_Map(MapCharPycSymbolElem, x, ctx.scope_stack.last()->map) if (!MARKED(x->value) && !x->value->sym->is_fun) {
        ast->sym->has.add(x->value->sym);
        x->value->sym->in = ast->sym;
      }
      break;
    case Continue_kind:
      ast->label[0] = ctx.lcontinue();
      break;
    case Break_kind:
      ast->label[0] = ctx.lbreak();
      break;
    case Return_kind:
      ast->label[0] = ctx.lreturn();
      break;
    case Import_kind:
      build_import(s, build_import_syms, ctx);
      break;
    case ImportFrom_kind:
      build_import_from(s, build_import_syms, ctx);
      break;
  }
  exit_scope(s, ctx);
  return 0;
}

#if 0
static Sym *make_num(int64 i) {
  Immediate imm;
  imm.v_int64 = i;
  char s[80];
  sprintf(s, "%ld", i);
  return if1_const(if1, sym_int64, s, &imm);
}
#endif

static Sym *make_tuple_type(expr_ty e, PycCompiler &ctx, PycAST *ast, asdl_seq *elts) {
#if 0
  int n = asdl_seq_LEN(elts);
  printf("make_tuple_type # %d\n", n);
  Sym *parent = ctx.tuple_types.get(n);
  if (!parent) {
    parent = new_sym(ast);
    parent->type_kind = Type_RECORD;
    parent->inherits_add(sym_tuple);
    for (int i = 0; i < n; i++)
      parent->has.add(getAST((expr_ty)asdl_seq_GET(elts, i), ctx)->sym);
    ctx.tuple_types.put(n, parent);
    Sym *fn = def_fun(e, ast, ctx);
    printf("make_tuple_type # %d id %d\n", n, fn->id);
    fn->self = new_sym(ast);
    fn->self->must_implement_and_specialize(parent);
    fn->self->in = fn;
    Code *body = nullptr;
    for (int64 i = 0; i < n; i++) {
      Sym *t = new_sym(ast);
      if1_send(if1, &body, 3, 1, sym___getitem__, fn->self, make_num(i), t)
          ->ast = ast;
      if1_send(if1, &body, 2, 0, sym___str__, t)->ast = ast;
      if1_move(if1, &body, t, fn->ret);
    }
    Vec<Sym *> as;
    as.add(new_sym(ast, "__str__"));
    as[0]->must_implement_and_specialize(sym___str__);
    as.add(fn->self);
    if1_closure(if1, fn, body, as.n, as.v);
  }
  auto sym = new_sym(ast);
  sym->type_kind = Type_RECORD;
  sym->inherits_add(parent);
  return sym;
#endif
  return sym_tuple;
}

static void build_syms_comprehension(PycCompiler &ctx, asdl_seq *generators, expr_ty elt, expr_ty value) {}

static int build_syms(expr_ty e, PycCompiler &ctx) {
  PycAST *past = getAST(e, ctx);
  ctx.node = e;
  ctx.lineno = e->lineno;
  AST_RECURSE_PRE(e, build_syms, ctx);
  switch (e->kind) {
    default:
      break;
    case Lambda_kind:  // arguments args, expr body
      past->sym = past->rval = def_fun(e, past, ctx);
      break;
    case Name_kind: {  // identifier id, expr_context ctx
      if (e->v.Name.ctx != EXPR_CONTEXT_SYM) {
        PycSymbol *s = make_PycSymbol(ctx, e->v.Name.id, e->v.Name.ctx == Load ? PYC_USE : PYC_LOCAL);
        if (!s) fail("error line %d, '%s' not found", ctx.lineno, PyString_AS_STRING(e->v.Name.id));
        past->sym = past->rval = s->sym;
      }  // else skip
      break;
    }
    case Dict_kind:      // expr* keys, expr* values
    case ListComp_kind:  // expr elt, comprehension* generators
#if PY_MAJOR_VERSION == 3
    case SetComp_kind:  // expr elt, comprehension* generators
#endif
    case GeneratorExp_kind:  // expr elt, comprehension* generators
      enter_scope(e, ast, ctx);
      ctx.lyield() = past->label[0] = if1_alloc_label(if1);
      break;
  }
  AST_RECURSE_POST(e, build_syms, ctx);
  switch (e->kind) {
    default:
      break;
    case Dict_kind:  // expr* keys, expr* values or comprehension* generators, key, value
#if PY_MAJOR_VERSION == 3
      build_syms_comprehension(ctx, e->v.Dict.generators, e->v.Dict.key, e->v.Dict.value);
#endif
      break;
    case ListComp_kind:  // expr elt, comprehension* generators
      build_syms_comprehension(ctx, e->v.ListComp.generators, e->v.ListComp.elt, 0);
      break;
#if PY_MAJOR_VERSION == 3
    case SetComp_kind:  // expr elt, comprehension* generators
      build_syms_comprehension(ctx, e->v.SetComp.generators, e->v.SetComp.elt, 0);
      break;
#endif
    case GeneratorExp_kind:  // expr elt, comprehension* generators
      build_syms_comprehension(ctx, e->v.GeneratorExp.generators, e->v.GeneratorExp.elt, 0);
      break;
    case List_kind:  // expr *elts, expr_context ctx
    // FALL THROUGH
    case Tuple_kind: {  // expr *elts, expr_context ctx
      ast->rval = new_sym(ast);
      bool def = true;
      for (int i = 0; i < asdl_seq_LEN(e->v.Tuple.elts); i++)
        def = !!getAST((expr_ty)asdl_seq_GET(e->v.Tuple.elts, i), ctx)->sym && def;
      if (def) ast->sym = make_tuple_type(e, ctx, ast, e->v.Tuple.elts);
      break;
    }
  }
  exit_scope(e, ctx);
  return 0;
}

static int build_syms_stmts(asdl_seq *stmts, PycCompiler &ctx) {
  for (int i = 0; i < asdl_seq_LEN(stmts); i++)
    if (build_syms((stmt_ty)asdl_seq_GET(stmts, i), ctx)) return -1;
  return 0;
}

static int build_syms(mod_ty mod, PycCompiler &ctx) {
  int r = 0;
  ctx.node = mod;
  enter_scope(ctx);
  switch (mod->kind) {
    case Module_kind:
      r = build_syms_stmts(mod->v.Module.body, ctx);
      break;
    case Expression_kind:
      r = build_syms(mod->v.Expression.body, ctx);
      break;
    case Interactive_kind:
      r = build_syms_stmts(mod->v.Interactive.body, ctx);
      break;
    case Suite_kind:
      assert(!"handled");
  }
  exit_scope(ctx);
  return r;
}

static void import_scope(PycModule *mod, PycCompiler &ctx) { ctx.imports.add(mod->ctx->saved_scopes.get(mod->mod)); }

void scope_sym(PycCompiler &ctx, Sym *sym, cchar *name) {
  PycSymbol *s = (PycSymbol *)sym->asymbol;
  ctx.scope_stack.last()->map.put(name ? cannonicalize_string(name) : sym->name, s);
}

static void build_module_attributes_syms(PycModule *mod, PycCompiler &ctx) {
  ctx.node = mod->mod;
  enter_scope(ctx);
  mod->name_sym = make_PycSymbol(ctx, "__name__", PYC_GLOBAL);
  mod->file_sym = make_PycSymbol(ctx, "__file__", PYC_GLOBAL);
  scope_sym(ctx, mod->name_sym->sym);
  scope_sym(ctx, mod->file_sym->sym);
  // scope_sym(ctx, sym___path__); package support
  exit_scope(ctx);
}

int build_syms(PycModule *x, PycCompiler &ctx) {
  x->ctx = &ctx;
  ctx.mod = x;
  ctx.filename = x->filename;
  if (!ctx.is_builtin()) import_scope(ctx.modules->v[0], ctx);
  build_module_attributes_syms(x, ctx);
  if (build_syms(x->mod, ctx) < 0) return -1;
  return 0;
}

Sym *make_string(cchar *s) {
  Immediate imm;
  imm.v_string = s;
  Sym *sym = if1_const(if1, sym_string, s, &imm);
  return sym;
}

Sym *make_string(PyObject *o) { return make_string(PyString_AsString(o)); }

void gen_fun(stmt_ty s, PycAST *ast, PycCompiler &ctx) {
  Sym *fn = ast->sym;
  Code *body = 0;
  for (int i = 0; i < asdl_seq_LEN(s->v.FunctionDef.args->defaults); i++) {
    PycAST *a = getAST((expr_ty)asdl_seq_GET(s->v.FunctionDef.args->defaults, i), ctx);
    if1_gen(if1, &ast->code, a->code);
    Sym *g = new_sym(ast, 1);
    a->sym = g;  // save global
    if1_move(if1, &ast->code, a->rval, g, ast);
  }
  Sym *in = ctx.scope_stack[ctx.scope_stack.n - 2]->in;
  for (int i = 0; i < asdl_seq_LEN(s->v.FunctionDef.body); i++)
    if1_gen(if1, &body, getAST((stmt_ty)asdl_seq_GET(s->v.FunctionDef.body, i), ctx)->code);
  if1_move(if1, &body, sym_nil, fn->ret, ast);
  if1_label(if1, &body, ast, ast->label[0]);
  if1_send(if1, &body, 4, 0, sym_primitive, sym_reply, fn->cont, fn->ret)->ast = ast;
  Vec<Sym *> as;
  as.add(new_sym(ast));
  as[0]->must_implement_and_specialize(if1_make_symbol(if1, ast->rval->name));
  get_syms_args(ast, s->v.FunctionDef.args, as, ctx, s->v.FunctionDef.decorator_list);
  if (in && !in->is_fun) {
    if (as.n > 1) {
      fn->self = as[1];
      fn->self->must_implement_and_specialize(in);
    }
  }
  if1_closure(if1, fn, body, as.n, as.v);
}

void gen_fun(expr_ty e, PycAST *ast, PycCompiler &ctx) {
  Sym *fn = ast->rval;
  Code *body = 0;
  for (int i = 0; i < asdl_seq_LEN(e->v.Lambda.args->defaults); i++) {
    PycAST *a = getAST((expr_ty)asdl_seq_GET(e->v.Lambda.args->defaults, i), ctx);
    if1_gen(if1, &ast->code, a->code);
    Sym *g = new_sym(ast, 1);
    a->sym = g;  // save global
    if1_move(if1, &ast->code, a->rval, g, ast);
  }
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

void gen_class(stmt_ty s, PycAST *ast, PycCompiler &ctx) {
  // build base ___init___ (class specific prototype initialization)
  Sym *fn = ast->rval, *cls = ast->sym;
  bool is_record = cls->type_kind == Type_RECORD && cls != sym_object;
  Code *body = 0;
  // Handle class decorators
  asdl_seq *decorators = s->v.ClassDef.decorator_list;
  char *vector_size = 0;
  if (ctx.is_builtin() && decorators) {
    for (int j = 0; j < asdl_seq_LEN(decorators); j++) {
      expr_ty e = (expr_ty)asdl_seq_GET(decorators, j);
      if (e->kind == Call_kind && e->v.Call.func->kind == Name_kind) {
        if (STREQ(PyString_AS_STRING(e->v.Call.func->v.Name.id), "vector")) {
          cls->is_vector = 1;
          cls->element = new_sym();
          asdl_seq *args = e->v.Call.args;
          for (int j = 0; j < asdl_seq_LEN(args); j++) {
            expr_ty e = (expr_ty)asdl_seq_GET(args, j);
            if (e->kind == Str_kind) vector_size = PyString_AS_STRING(e->v.Str.s);
          }
        }
      }
    }
  }
  for (int i = 0; i < cls->includes.n; i++) {
    Sym *inc = cls->includes[i];
    for (int j = 0; j < inc->has.n; j++) {
      Sym *iv = if1_make_symbol(if1, inc->has[j]->name);
      if (!inc->has[j]->alias || !inc->has.v[j]->alias->is_fun) {
        Sym *t = new_sym(ast);
        if (inc->self) {
          if1_send(if1, &body, 4, 1, sym_operator, inc->self, sym_period, iv, t)->ast = ast;
          if1_send(if1, &body, 5, 1, sym_operator, fn->self, sym_setter, iv, t, (ast->rval = new_sym(ast)))->ast = ast;
        }
      } else
        if1_send(if1, &body, 5, 1, sym_operator, fn->self, sym_setter, iv, inc->has[j]->alias,
                 (ast->rval = new_sym(ast)))
            ->ast = ast;
    }
  }
  for (int i = 0; i < asdl_seq_LEN(s->v.ClassDef.body); i++)
    if1_gen(if1, &body, getAST((stmt_ty)asdl_seq_GET(s->v.ClassDef.body, i), ctx)->code);
  if1_move(if1, &body, fn->self, fn->ret, ast);
  if1_label(if1, &body, ast, ast->label[0]);
  if1_send(if1, &body, 4, 0, sym_primitive, sym_reply, fn->cont, fn->ret)->ast = ast;
  {
    Vec<Sym *> as;
    as.add(fn);
    as.add(fn->self);
    if1_closure(if1, fn, body, as.n, as.v);
  }
  // build prototype
  Sym *proto = cls->self;
  if (is_record) {
    if1_send(if1, &ast->code, 3, 1, sym_primitive, sym_new, cls, proto)->ast = ast;
    if1_send(if1, &ast->code, 2, 1, fn, proto, new_sym(ast))->ast = ast;
  }
  // build default __init__ (user class constructor initialization)
  PycSymbol *init_fun = ctx.scope_stack.last()->map.get(sym___init__->name);
  Sym *init_sym = init_fun ? init_fun->sym->alias : 0;
  if (!init_fun) {
    init_sym = fn = new_fun(ast);
    fn->nesting_depth = ctx.scope_stack.n;
    fn->self = new_sym(ast);
    fn->self->must_implement_and_specialize(cls);
    fn->self->in = fn;
    body = 0;
    if1_move(if1, &body, fn->self, fn->ret);
    if1_send(if1, &body, 4, 0, sym_primitive, sym_reply, fn->cont, fn->ret)->ast = ast;
    Vec<Sym *> as;
    as.add(new_sym(ast, "__init__"));
    as[0]->must_implement_and_specialize(sym___init__);
    as.add(fn->self);
    if1_closure(if1, fn, body, as.n, as.v);
  }
  // build constructor
  while (1) {
    if (is_record) {
      fn = new_fun(ast);
      fn->init = init_sym;
      fn->nesting_depth = ctx.scope_stack.n;
      Vec<Sym *> as;
      as.add(new_sym(ast, "__new__"));
      as[0]->must_implement_and_specialize(ast->sym->meta_type);
      fn->name = as[0]->name;
      for (int i = 2; i < init_sym->has.n; i++) as.add(new_sym(ast));
      body = 0;
      Sym *t = new_sym(ast);
      if (!cls->is_vector)
        if1_send(if1, &body, 3, 1, sym_primitive, sym_clone, proto, t)->ast = ast;
      else {
        Sym *vec_size = 0;
        for (int i = 2; i < init_sym->has.n; i++)
          if (vector_size && init_sym->has[i]->name && !strcmp(init_sym->has[i]->name, vector_size))
            vec_size = as[i - 1];
        if (!vec_size) fail("vector size missing, line %d", ctx.lineno);
        if1_send(if1, &body, 4, 1, sym_primitive, sym_clone_vector, proto, vec_size, t)->ast = ast;
      }
      Code *send = if1_send(if1, &body, 2, 1, sym___init__, t, new_sym(ast));
      send->ast = ast;
      for (int i = 2; i < init_sym->has.n; i++) if1_add_send_arg(if1, send, as[i - 1]);
      if1_move(if1, &body, t, fn->ret);
      if1_send(if1, &body, 4, 0, sym_primitive, sym_reply, fn->cont, fn->ret)->ast = ast;
      if1_closure(if1, fn, body, as.n, as.v);
    }
    if (init_fun && init_fun->previous) {
      init_fun = init_fun->previous;
      init_sym = init_fun->sym->alias;
    } else
      break;
  }
  // Add coerciion functions.
  if (cls->num_kind != IF1_NUM_KIND_NONE) {
    fn = new_fun(ast);
    fn->nesting_depth = ctx.scope_stack.n;
    Vec<Sym *> as;
    as.add(new_sym(ast, "__coerce__"));
    as[0]->must_implement_and_specialize(ast->sym->meta_type);
    fn->name = as[0]->name;
    Sym *rhs = new_sym(ast);
    as.add(rhs);
    body = 0;
    Sym *t = new_sym(ast);
    if1_send(if1, &body, 4, 1, sym_primitive, sym_coerce, cls, rhs, t)->ast = ast;
    if1_move(if1, &body, t, fn->ret);
    if1_send(if1, &body, 4, 0, sym_primitive, sym_reply, fn->cont, fn->ret)->ast = ast;
    if1_closure(if1, fn, body, as.n, as.v);
  }
  PycSymbol *call_fun = ctx.scope_stack.last()->map.get(sym___call__->name);
  Sym *call_sym = call_fun ? call_fun->sym->alias : 0;
  // callable redirect to __call__
  if (call_fun) {
    fn = new_fun(ast);
    fn->nesting_depth = ctx.scope_stack.n;
    Vec<Sym *> as;
    as.add(new_sym(ast, "__call__"));
    as[0]->must_implement_and_specialize(cls);
    int n = call_sym->has.n - 1;
    for (int i = 2; i <= n; i++) as.add(new_sym(ast));
    body = 0;
    Sym *t = new_sym(ast);
    Code *send = if1_send(if1, &body, 2, 1, sym___call__, as[0], (t = new_sym(ast)));
    send->ast = ast;
    for (int i = 2; i <= n; i++) if1_add_send_arg(if1, send, as[i - 1]);
    if1_move(if1, &body, t, fn->ret);
    if1_send(if1, &body, 4, 0, sym_primitive, sym_reply, fn->cont, fn->ret)->ast = ast;
    if1_closure(if1, fn, body, as.n, as.v);
  }
}

int get_stmts_code(asdl_seq *stmts, Code **code, PycCompiler &ctx) {
  for (int i = 0; i < asdl_seq_LEN(stmts); i++) if1_gen(if1, code, getAST((stmt_ty)asdl_seq_GET(stmts, i), ctx)->code);
  return 0;
}

void call_method(Code **code, PycAST *ast, Sym *o, Sym *m, Sym *r, int n, ...) {
  va_list ap;
  Sym *t = new_sym(ast);
  Code *method = if1_send(if1, code, 4, 1, sym_operator, o, sym_period, m, t);
  method->ast = ast;
  method->partial = Partial_OK;
  Code *send = if1_send(if1, code, 1, 1, t, r);
  send->ast = ast;
  va_start(ap, n);
  for (int i = 0; i < n; i++) {
    Sym *v = va_arg(ap, Sym *);
    if (v)
      if1_add_send_arg(if1, send, v);
    else
      if1_add_send_arg(if1, send, sym_nil);
  }
}

void gen_if(PycAST *ifcond, asdl_seq *ifif, asdl_seq *ifelse, PycAST *ast, PycCompiler &ctx) {
  Code *ifif_code = 0, *ifelse_code = 0;
  Sym *t = new_sym(ast);
  get_stmts_code(ifif, &ifif_code, ctx);
  get_stmts_code(ifelse, &ifelse_code, ctx);
  if1_gen(if1, &ast->code, ifcond->code);
  call_method(&ast->code, ast, ifcond->rval, sym___pyc_to_bool__, t, 0);
  if1_if(if1, &ast->code, 0, t, ifif_code, 0, ifelse_code, 0, 0, ast);
}

void gen_ifexpr(PycAST *ifcond, PycAST *ifif, PycAST *ifelse, PycAST *ast) {
  ast->rval = new_sym(ast);
  if1_gen(if1, &ast->code, ifcond->code);
  Sym *t = new_sym(ast);
  call_method(&ast->code, ast, ifcond->rval, sym___pyc_to_bool__, t, 0);
  if1_if(if1, &ast->code, 0, t, ifif->code, ifif->rval, ifelse ? ifelse->code : 0, ifelse ? ifelse->rval : 0, ast->rval,
         ast);
}

Sym *make_num(PyObject *o, PycCompiler &ctx) {
  Sym *sym = 0;
  if (PyInt_Check(o)) {
    int64 i = (int64)PyInt_AsLong(o);
    Immediate imm;
    imm.v_int64 = i;
    char s[80];
    sprintf(s, "%ld", i);
    sym = if1_const(if1, sym_int64, s, &imm);
  } else if (PyFloat_Check(o)) {
    double f = PyFloat_AsDouble(o);
    Immediate imm;
    imm.v_float64 = f;
    char s[80];
    sprintf(s, "%g", f);
    sym = if1_const(if1, sym_float64, s, &imm);
  } else
    fail("unhandled num type, line %d", ctx.lineno);
  return sym;
}

Sym *make_symbol(cchar *name) {
  Sym *s = if1_make_symbol(if1, name);
  return s;
}

Sym *map_operator(operator_ty op) {
  switch (op) {
    default:
      assert(!"case");
    case Add:
      return make_symbol("__add__");
    case Sub:
      return make_symbol("__sub__");
    case Mult:
      return make_symbol("__mul__");
    case Div:
      return make_symbol("__div__");
    case Mod:
      return make_symbol("__mod__");
    case Pow:
      return make_symbol("__pow__");
    case LShift:
      return make_symbol("__lshift__");
    case RShift:
      return make_symbol("__rshift__");
    case BitOr:
      return make_symbol("__or__");
    case BitXor:
      return make_symbol("__xor__");
    case BitAnd:
      return make_symbol("__and__");
    case FloorDiv:
      return make_symbol("__floordiv__");
  }
  return 0;
}

Sym *map_ioperator(operator_ty op) {
  switch (op) {
    default:
      assert(!"case");
    case Add:
      return make_symbol("__iadd__");
    case Sub:
      return make_symbol("__isub__");
    case Mult:
      return make_symbol("__imul__");
    case Div:
      return make_symbol("__idiv__");
    case Mod:
      return make_symbol("__imod__");
    case Pow:
      return make_symbol("__ipow__");
    case LShift:
      return make_symbol("__ilshift__");
    case RShift:
      return make_symbol("__irshift__");
    case BitOr:
      return make_symbol("__ior__");
    case BitXor:
      return make_symbol("__ixor__");
    case BitAnd:
      return make_symbol("__iand__");
    case FloorDiv:
      return make_symbol("__ifloordiv__");
  }
  return 0;
}

Sym *map_unary_operator(unaryop_ty op) {
  switch (op) {
    default:
      assert(!"case");
    case Invert:
      return make_symbol("__invert__");
    case Not:
      return make_symbol("__not__");  // not official
    // what about abs?
    case UAdd:
      return make_symbol("__pos__");
    case USub:
      return make_symbol("__neg__");
  }
  return 0;
}

Sym *map_cmp_operator(cmpop_ty op) {
  switch (op) {
    default:
      assert(!"case");
    case Eq:
      return make_symbol("__eq__");
    case NotEq:
      return make_symbol("__ne__");
    case Lt:
      return make_symbol("__lt__");
    case LtE:
      return make_symbol("__le__");
    case Gt:
      return make_symbol("__gt__");
    case GtE:
      return make_symbol("__ge__");
    case Is:
      return make_symbol("__is__");
    case IsNot:
      return make_symbol("__nis__");
    case In:
      return make_symbol("__contains__");
    case NotIn:
      return make_symbol("__ncontains__");
  }
  return 0;
}

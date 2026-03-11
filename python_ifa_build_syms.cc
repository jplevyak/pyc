// SPDX-License-Identifier: BSD-3-Clause
#include "python_ifa_int.h"
#include "python_parse.h"


// ---- Shared utility functions ----

static void import_file(cchar *name, cchar *p, PycCompiler &ctx) {
  cchar *f = dupstrs(p, "/", name, ".py");
  PycModule *m = new PycModule(f);
  m->pymod = dparse_python_to_ast(f);
  ctx.modules->add(m);
  PycModule *saved_mod = ctx.mod;
  cchar *saved_filename = ctx.filename;
  int saved_imports_n = ctx.imports.n;
  Vec<PycScope *> saved_scope_stack;
  saved_scope_stack.move(ctx.scope_stack);
  build_syms(m, ctx);
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

static void rtrim_str(char *s) {
  if (!s) return;
  int len = strlen(s);
  while (len > 0 && isspace((unsigned char)s[len-1])) s[--len] = 0;
}

static void build_import_syms(char *sym, char *as, char *from, PycCompiler &ctx) {
  rtrim_str(sym); rtrim_str(from);
  char *mod = from ? from : sym;
  if (!strcmp(mod, "pyc_compat")) return;
  PycModule *m = get_module(mod, ctx);
  if (!m) {
    for (auto p : ctx.search_path->values()) {
      if (file_exists(p, "/__init__.py")) continue;
      if (!is_regular_file(p, "/", mod, ".py")) continue;
      import_file(mod, p, ctx);
      break;
    }
  }
}

static void import_scope(PycModule *mod, PycCompiler &ctx) {
  ctx.imports.add(mod->ctx->saved_scopes.get(mod->pymod));
}

void scope_sym(PycCompiler &ctx, Sym *sym, cchar *name) {
  PycSymbol *s = (PycSymbol *)sym->asymbol;
  ctx.scope_stack.last()->map.put(name ? cannonicalize_string(name) : sym->name, s);
}

// ---- PyDAST (pyda) build_syms path ----

// Recursively mark PY_name nodes in lvalue position as PY_STORE
static void mark_store(PyDAST *n) {
  if (!n) return;
  if (n->kind == PY_name) { n->ctx = PY_STORE; return; }
  if (n->kind == PY_power) { n->ctx = PY_STORE; return; }
  if (n->kind == PY_fpdef || n->kind == PY_fplist || n->kind == PY_tuple || n->kind == PY_testlist ||
      n->kind == PY_exprlist)
    for (auto c : n->children.values()) mark_store(c);
}

// Set up function scope for pyda path
static Sym *def_fun_pyda(PyDAST *n, PycAST *ast, Sym *fn, PycCompiler &ctx) {
  fn->in = ctx.scope_stack.last()->in;
  new_fun(ast, fn);
  ctx.node = n;
  if (n->kind == PY_classdef)
    enter_scope(ctx, ast->sym);  // class: in = class sym
  else
    enter_scope(ctx, (Sym *)0);  // funcdef/lambda: in set later via scope_stack.last()->fun
  ctx.scope_stack.last()->fun = fn;
  fn->nesting_depth = ctx.scope_stack.n - 1;
  ctx.lreturn() = ast->label[0] = if1_alloc_label(if1);
  return fn;
}

// Extract parameter syms from PY_varargslist into has[]
void get_syms_args_pyda(PycAST *ast, PyDAST *varargslist, Vec<Sym *> &has, PycCompiler &ctx) {
  if (!varargslist) return;
  for (auto c : varargslist->children.values()) {
    if (c->kind == PY_star_arg || c->kind == PY_dstar_arg) {
      Sym *s = getAST(c->children[0], ctx)->sym;
      if (s) has.add(s);
    } else if (c->kind == PY_arg_default) {
      Sym *s = getAST(c->children[0], ctx)->sym;
      if (s) has.add(s);
    } else {
      Sym *s = getAST(c, ctx)->sym;
      if (s) has.add(s);
    }
  }
}

static void build_import_syms_name_pyda(PyDAST *n, PycCompiler &ctx);
static void build_import_syms_from_pyda(PyDAST *n, PycCompiler &ctx);
static int build_syms_pyda(PyDAST *n, PycCompiler &ctx);

static void build_import_syms_name_pyda(PyDAST *n, PycCompiler &ctx) {
  // n->kind == PY_import_name, children are PY_dotted_as_name or PY_testlist of them
  Vec<PyDAST *> names;
  if (n->children.n == 1 && n->children[0]->kind == PY_dotted_as_name)
    names.add(n->children[0]);
  else
    for (auto c : n->children.values())
      if (c->kind == PY_dotted_as_name) names.add(c);
  for (auto d : names.values()) {
    // d: PY_dotted_as_name: children[0]=PY_dotted_name, children[1]=PY_name (as)
    cchar *mod_name = d->children[0]->str_val;
    cchar *as_name = (d->children.n > 1) ? d->children[1]->str_val : nullptr;
    build_import_syms(const_cast<char *>(mod_name), const_cast<char *>(as_name), nullptr, ctx);
  }
}

static void build_import_syms_from_pyda(PyDAST *n, PycCompiler &ctx) {
  // n->kind == PY_import_from
  // children: [PY_dotted_name, PY_import_as_name* or PY_testlist]
  if (n->children.n < 1) return;
  cchar *from_mod = n->children[0]->str_val;
  // Process each import_as_name
  for (int i = 1; i < n->children.n; i++) {
    PyDAST *child = n->children[i];
    if (child->kind == PY_testlist) {
      for (auto ia : child->children.values())
        if (ia->kind == PY_import_as_name) {
          cchar *sym_name = ia->children[0]->str_val;
          cchar *as_name = (ia->children.n > 1) ? ia->children[1]->str_val : nullptr;
          build_import_syms(const_cast<char *>(sym_name), const_cast<char *>(as_name),
                            const_cast<char *>(from_mod), ctx);
        }
    } else if (child->kind == PY_import_as_name) {
      cchar *sym_name = child->children[0]->str_val;
      cchar *as_name = (child->children.n > 1) ? child->children[1]->str_val : nullptr;
      build_import_syms(const_cast<char *>(sym_name), const_cast<char *>(as_name),
                        const_cast<char *>(from_mod), ctx);
    }
  }
}

static int build_syms_pyda(PyDAST *n, PycCompiler &ctx) {
  if (!n) return 0;
  PycAST *ast = getAST(n, ctx);
  ctx.node = n;
  ctx.lineno = n->line;

  switch (n->kind) {
    case PY_module:
      for (auto c : n->children.values()) build_syms_pyda(c, ctx);
      return 0;

    case PY_suite:
      for (auto c : n->children.values()) build_syms_pyda(c, ctx);
      return 0;

    case PY_decorated: {
      // Last child is the funcdef or classdef; earlier children are decorators
      PyDAST *def = n->children.last();
      // Pre-scope: process decorators
      for (int i = 0; i < n->children.n - 1; i++) build_syms_pyda(n->children[i], ctx);
      // Dispatch to funcdef or classdef handling
      if (def->kind == PY_funcdef) {
        PycAST *def_ast = getAST(def, ctx);
        PyDAST *params = def->children[1];
        PyDAST *varargsl = (params->children.n > 0) ? params->children[0] : nullptr;
        if (varargsl)
          for (auto c : varargsl->children.values())
            if (c->kind == PY_arg_default) build_syms_pyda(c->children[1], ctx);
        PycSymbol *ps = make_PycSymbol(ctx, def->children[0]->str_val, PYC_LOCAL);
        def_ast->rval = def_ast->sym = def_fun_pyda(def, def_ast, ps->sym, ctx);
        ast->rval = ast->sym = def_ast->sym;
        if (varargsl)
          for (auto c : varargsl->children.values()) {
            if (c->kind == PY_arg_default) {
              mark_store(c->children[0]);
              build_syms_pyda(c->children[0], ctx);
            } else if (c->kind == PY_star_arg || c->kind == PY_dstar_arg) {
              mark_store(c->children[0]);
              build_syms_pyda(c->children[0], ctx);
            } else {
              mark_store(c);
              build_syms_pyda(c, ctx);
            }
          }
        if (def->children.n >= 3) build_syms_pyda(def->children[2], ctx);
        form_Map(MapCharPycSymbolElem, x, ctx.scope_stack.last()->map)
          if (!MARKED(x->value) && !x->value->sym->is_fun) {
            x->value->sym->is_local = 1;
            x->value->sym->nesting_depth = LOCALLY_NESTED;
          }
        exit_scope(ctx);
      } else if (def->kind == PY_classdef) {
        goto Lclassdef_inner;
      }
      return 0;
    }

    case PY_funcdef: {
      PyDAST *params = n->children[1];
      PyDAST *varargsl = (params->children.n > 0) ? params->children[0] : nullptr;
      if (varargsl)
        for (auto c : varargsl->children.values())
          if (c->kind == PY_arg_default) build_syms_pyda(c->children[1], ctx);
      PycSymbol *ps = make_PycSymbol(ctx, n->children[0]->str_val, PYC_LOCAL);
      if (ctx.in_class() && ctx.cls()->type_kind == Type_RECORD) {
        // Mirror CPython FunctionDef_kind path: create named sym + func sym with alias
        ast->rval = ps->sym;
        ast->sym = new_sym(ast, 1);
        ast->rval->alias = ast->sym;
        ast->sym = def_fun_pyda(n, ast, ast->sym, ctx);
        // Generate setter into ast->code (collected by gen_class_pyda into class init body)
        if1_send(if1, &ast->code, 5, 1, sym_operator, ctx.cls()->self, sym_setter,
                 if1_make_symbol(if1, ast->rval->name), ast->sym, new_sym(ast))->ast = ast;
      } else
        ast->rval = ast->sym = def_fun_pyda(n, ast, ps->sym, ctx);
      if (varargsl)
        for (auto c : varargsl->children.values()) {
          if (c->kind == PY_arg_default) {
            mark_store(c->children[0]);
            build_syms_pyda(c->children[0], ctx);
          } else if (c->kind == PY_star_arg || c->kind == PY_dstar_arg) {
            mark_store(c->children[0]);
            build_syms_pyda(c->children[0], ctx);
          } else {
            mark_store(c);
            build_syms_pyda(c, ctx);
          }
        }
      if (n->children.n >= 3) build_syms_pyda(n->children[2], ctx);
      form_Map(MapCharPycSymbolElem, x, ctx.scope_stack.last()->map)
        if (!MARKED(x->value) && !x->value->sym->is_fun) {
          x->value->sym->is_local = 1;
          x->value->sym->nesting_depth = LOCALLY_NESTED;
        }
      exit_scope(ctx);
      return 0;
    }

    case PY_classdef: {
    Lclassdef_inner:;
      PyDAST *cdef = (n->kind == PY_decorated) ? n->children.last() : n;
      PycAST *cdef_ast = getAST(cdef, ctx);
      // Process base classes (all children of cdef between name and last suite)
      for (int i = 1; i < cdef->children.n - 1; i++) build_syms_pyda(cdef->children[i], ctx);
      PYC_SCOPINGS scope = (ctx.is_builtin() && ctx.scope_stack.n == 1) ? PYC_GLOBAL : PYC_LOCAL;
      cdef_ast->sym = unalias_type(make_PycSymbol(ctx, cdef->children[0]->str_val, scope)->sym);
      if (!cdef_ast->sym->is_constant) {
        if (!cdef_ast->sym->type_kind) cdef_ast->sym->type_kind = Type_RECORD;
        if (cdef_ast->sym->type_kind == Type_RECORD)
          cdef_ast->sym->self = new_global(cdef_ast);
        else
          cdef_ast->sym->self = new_base_instance(cdef_ast->sym, cdef_ast);
      } else
        cdef_ast->sym->self = cdef_ast->sym;
      Sym *fn = new_sym(cdef_ast, "___init___", 1);
      cdef_ast->rval = def_fun_pyda(cdef, cdef_ast, fn, ctx);
      cdef_ast->rval->self = new_sym(cdef_ast);
      cdef_ast->rval->self->must_implement_and_specialize(cdef_ast->sym);
      cdef_ast->rval->self->in = fn;
      // For decorated: set the outer node's rval/sym; for non-decorated, cdef_ast==ast, rval=fn, sym=class_sym already
      if (n != cdef) ast->rval = ast->sym = cdef_ast->sym;
      // Process body (last child = PY_suite)
      build_syms_pyda(cdef->children.last(), ctx);
      // Post-classdef: collect base classes and members
      for (int i = 1; i < cdef->children.n - 1; i++) {
        Sym *base = getAST(cdef->children[i], ctx)->sym;
        if (!base) fail("error line %d, base not found for class '%s'", ctx.lineno, cdef_ast->sym->name);
        cdef_ast->sym->inherits_add(base);
      }
      form_Map(MapCharPycSymbolElem, x, ctx.scope_stack.last()->map)
        if (!MARKED(x->value) && !x->value->sym->is_fun) {
          cdef_ast->sym->has.add(x->value->sym);
          x->value->sym->in = cdef_ast->sym;
        }
      exit_scope(ctx);
      return 0;
    }

    case PY_lambda: {
      PyDAST *varargsl = (n->children.n > 0 && n->children[0]->kind == PY_varargslist) ? n->children[0] : nullptr;
      if (varargsl)
        for (auto c : varargsl->children.values())
          if (c->kind == PY_arg_default) build_syms_pyda(c->children[1], ctx);
      ast->sym = ast->rval = def_fun_pyda(n, ast, new_sym(ast, 1), ctx);
      if (varargsl)
        for (auto c : varargsl->children.values()) {
          PyDAST *param = (c->kind == PY_arg_default) ? c->children[0] : c;
          mark_store(param);
          build_syms_pyda(param, ctx);
        }
      build_syms_pyda(n->children.last(), ctx);
      exit_scope(ctx);
      return 0;
    }

    case PY_name: {
      if (n->ctx == PY_STORE) {
        PycSymbol *s = make_PycSymbol(ctx, n->str_val, PYC_LOCAL);
        ast->sym = ast->rval = s->sym;
      } else {
        PycSymbol *s = make_PycSymbol(ctx, n->str_val, PYC_USE);
        if (!s) fail("error line %d, '%s' not found", ctx.lineno, n->str_val);
        ast->sym = ast->rval = s->sym;
      }
      return 0;
    }

    case PY_global_stmt:
      for (auto c : n->children.values()) make_PycSymbol(ctx, c->str_val, PYC_GLOBAL);
      return 0;

    case PY_for_stmt:
      ctx.lcontinue() = ast->label[0] = if1_alloc_label(if1);
      ctx.lbreak() = ast->label[1] = if1_alloc_label(if1);
      mark_store(n->children[0]);
      for (auto c : n->children.values()) build_syms_pyda(c, ctx);
      return 0;

    case PY_while_stmt:
      ctx.lcontinue() = ast->label[0] = if1_alloc_label(if1);
      ctx.lbreak() = ast->label[1] = if1_alloc_label(if1);
      for (auto c : n->children.values()) build_syms_pyda(c, ctx);
      return 0;

    case PY_assign:
      for (int i = 0; i < n->children.n - 1; i++) mark_store(n->children[i]);
      for (auto c : n->children.values()) build_syms_pyda(c, ctx);
      return 0;

    case PY_augassign:
      // children: [target, PY_augassign_op, value] for statement node; 0 children for operator node
      if (n->children.n < 3) return 0;  // Skip the operator-only PY_augassign child
      mark_store(n->children[0]);
      build_syms_pyda(n->children[0], ctx);  // target
      build_syms_pyda(n->children[2], ctx);  // value (skip operator children[1])
      return 0;

    case PY_listcomp:
    case PY_genexpr: {
      enter_scope(n, ctx, nullptr);
      ctx.lyield() = ast->label[0] = if1_alloc_label(if1);
      for (auto c : n->children.values()) build_syms_pyda(c, ctx);
      exit_scope(ctx);
      return 0;
    }

    case PY_list_for:
    case PY_comp_for:
      mark_store(n->children[0]);
      for (auto c : n->children.values()) build_syms_pyda(c, ctx);
      return 0;

    case PY_import_name:
      build_import_syms_name_pyda(n, ctx);
      return 0;

    case PY_import_from:
      build_import_syms_from_pyda(n, ctx);
      return 0;

    case PY_continue_stmt: ast->label[0] = ctx.lcontinue(); return 0;
    case PY_break_stmt:    ast->label[0] = ctx.lbreak(); return 0;
    case PY_return_stmt:
      ast->label[0] = ctx.lreturn();
      for (auto c : n->children.values()) build_syms_pyda(c, ctx);
      return 0;

    case PY_expr_stmt:
    case PY_pass_stmt:
    case PY_del_stmt:
    case PY_raise_stmt:
    case PY_yield_stmt:
    case PY_assert_stmt:
    case PY_exec_stmt:
    case PY_print_stmt:
    case PY_if_stmt:
    case PY_elif_clause:
    case PY_else_clause:
    case PY_try_stmt:
    case PY_except_clause:
    case PY_except_handler:
    case PY_finally_clause:
    case PY_with_stmt:
    case PY_with_item:
    case PY_bool_or:
    case PY_bool_and:
    case PY_bool_not:
    case PY_compare:
    case PY_cmp_op:
    case PY_binop:
    case PY_unaryop:
    case PY_power:
    case PY_call:
    case PY_attribute:
    case PY_subscript:
    case PY_ternary:
    case PY_tuple:
    case PY_exprlist:
    case PY_testlist: {
      // Mirrors CPython Tuple_kind: recurse children, set sym=sym_tuple for destructuring
      ast->rval = new_sym(ast);
      for (auto c : n->children.values()) build_syms_pyda(c, ctx);
      bool def = true;
      for (auto c : n->children.values())
        def = !!getAST(c, ctx)->sym && def;
      if (def) ast->sym = sym_tuple;
      return 0;
    }

    case PY_number:
    case PY_string:
    case PY_backquote:
    case PY_list:
    case PY_dict:
    case PY_set:
    case PY_slice:
    case PY_subscriptlist:
    case PY_parameters:
    case PY_varargslist:
    case PY_fpdef:
    case PY_fplist:
    case PY_arglist:
    case PY_keyword_arg:
    case PY_star_arg:
    case PY_dstar_arg:
    case PY_arg_default:
    case PY_testlist1:
    case PY_list_if:
    case PY_comp_if:
    case PY_decorator:
    case PY_yield_expr:
    case PY_dotted_name:
    case PY_dotted_as_name:
    case PY_import_as_name:
    default:
      for (auto c : n->children.values()) build_syms_pyda(c, ctx);
      return 0;
  }
}

static void build_module_attributes_syms_pyda(PycModule *mod, PycCompiler &ctx) {
  ctx.node = mod->pymod;
  enter_scope(ctx);
  mod->name_sym = make_PycSymbol(ctx, "__name__", PYC_GLOBAL);
  mod->file_sym = make_PycSymbol(ctx, "__file__", PYC_GLOBAL);
  scope_sym(ctx, mod->name_sym->sym);
  scope_sym(ctx, mod->file_sym->sym);
  exit_scope(ctx);
}

// ---- gen_fun_pyda, gen_lambda_pyda, gen_class_pyda ----

void gen_fun_pyda(PyDAST *n, PycAST *ast, PycCompiler &ctx) {
  // n is PY_funcdef
  Sym *fn = ast->sym;
  Code *body = 0;
  PyDAST *params = n->children[1];
  PyDAST *varargsl = (params->children.n > 0) ? params->children[0] : nullptr;
  // Process defaults (emit code for each default expr, save as global)
  if (varargsl)
    for (auto c : varargsl->children.values())
      if (c->kind == PY_arg_default) {
        PycAST *a = getAST(c->children[1], ctx);
        if1_gen(if1, &ast->code, a->code);
        Sym *g = new_sym(ast, 1);
        a->sym = g;
        if1_move(if1, &ast->code, a->rval, g, ast);
      }
  Sym *in = ctx.scope_stack[ctx.scope_stack.n - 2]->in;
  // Process body (may be PY_suite or single statement)
  if (n->children.n >= 3) {
    PyDAST *body_node = n->children[2];
    if (body_node->kind == PY_suite) {
      for (auto c : body_node->children.values()) if1_gen(if1, &body, getAST(c, ctx)->code);
    } else {
      if1_gen(if1, &body, getAST(body_node, ctx)->code);
    }
  }
  if1_move(if1, &body, sym_nil, fn->ret, ast);
  if1_label(if1, &body, ast, ast->label[0]);
  if1_send(if1, &body, 4, 0, sym_primitive, sym_reply, fn->cont, fn->ret)->ast = ast;
  Vec<Sym *> as;
  as.add(new_sym(ast));
  as[0]->must_implement_and_specialize(if1_make_symbol(if1, ast->rval->name));
  get_syms_args_pyda(ast, varargsl, as, ctx);
  if (in && !in->is_fun) {
    if (as.n > 1) {
      fn->self = as[1];
      fn->self->must_implement_and_specialize(in);
    }
  }
  if1_closure(if1, fn, body, as.n, as.v);
}

void gen_lambda_pyda(PyDAST *n, PycAST *ast, PycCompiler &ctx) {
  Sym *fn = ast->rval;
  Code *body = 0;
  PyDAST *varargsl = (n->children.n > 0 && n->children[0]->kind == PY_varargslist) ? n->children[0] : nullptr;
  if (varargsl)
    for (auto c : varargsl->children.values())
      if (c->kind == PY_arg_default) {
        PycAST *a = getAST(c->children[1], ctx);
        if1_gen(if1, &ast->code, a->code);
        Sym *g = new_sym(ast, 1);
        a->sym = g;
        if1_move(if1, &ast->code, a->rval, g, ast);
      }
  PycAST *b = getAST(n->children.last(), ctx);
  if1_gen(if1, &body, b->code);
  if1_move(if1, &body, b->rval, fn->ret, ast);
  if1_label(if1, &body, ast, ast->label[0]);
  if1_send(if1, &body, 4, 0, sym_primitive, sym_reply, fn->cont, fn->ret)->ast = ast;
  Vec<Sym *> as;
  as.add(fn);
  get_syms_args_pyda(ast, varargsl, as, ctx);
  if1_closure(if1, fn, body, as.n, as.v);
}

void gen_class_pyda(PyDAST *cdef, PycAST *ast, PycCompiler &ctx, char *vector_size) {
  // cdef is the PY_classdef node
  Sym *fn = ast->rval, *cls = ast->sym;
  bool is_record = cls->type_kind == Type_RECORD && cls != sym_object;
  Code *body = 0;
  // Build base ___init___ (class prototype initialization)
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
  // Body statements (last child = PY_suite or single stmt)
  {
    PyDAST *body_node = cdef->children.last();
    if (body_node->kind == PY_suite) {
      for (auto c : body_node->children.values()) if1_gen(if1, &body, getAST(c, ctx)->code);
    } else {
      if1_gen(if1, &body, getAST(body_node, ctx)->code);
    }
  }
  if1_move(if1, &body, fn->self, fn->ret, ast);
  if1_label(if1, &body, ast, ast->label[0]);
  if1_send(if1, &body, 4, 0, sym_primitive, sym_reply, fn->cont, fn->ret)->ast = ast;
  {
    Vec<Sym *> as;
    as.add(fn);
    as.add(fn->self);
    if1_closure(if1, fn, body, as.n, as.v);
  }
  // Build prototype
  Sym *proto = cls->self;
  if (is_record) {
    if1_send(if1, &ast->code, 3, 1, sym_primitive, sym_new, cls, proto)->ast = ast;
    if1_send(if1, &ast->code, 2, 1, fn, proto, new_sym(ast))->ast = ast;
  }
  // Find __init__ in the class scope
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
  if (call_fun) {
    fn = new_fun(ast);
    fn->nesting_depth = ctx.scope_stack.n;
    Vec<Sym *> as;
    as.add(new_sym(ast, "__call__"));
    as[0]->must_implement_and_specialize(cls);
    int n_args = call_sym->has.n - 1;
    for (int i = 2; i <= n_args; i++) as.add(new_sym(ast));
    body = 0;
    Sym *t = new_sym(ast);
    Code *send = if1_send(if1, &body, 2, 1, sym___call__, as[0], (t = new_sym(ast)));
    send->ast = ast;
    for (int i = 2; i <= n_args; i++) if1_add_send_arg(if1, send, as[i - 1]);
    if1_move(if1, &body, t, fn->ret);
    if1_send(if1, &body, 4, 0, sym_primitive, sym_reply, fn->cont, fn->ret)->ast = ast;
    if1_closure(if1, fn, body, as.n, as.v);
  }
}

// ---- Updated build_syms(PycModule*) ----

int build_syms(PycModule *x, PycCompiler &ctx) {
  x->ctx = &ctx;
  ctx.mod = x;
  ctx.filename = x->filename;
  if (!ctx.is_builtin()) import_scope(ctx.modules->v[0], ctx);
  build_module_attributes_syms_pyda(x, ctx);
  ctx.node = x->pymod;
  enter_scope(ctx);
  build_syms_pyda(x->pymod, ctx);
  exit_scope(ctx);
  return 0;
}

Sym *make_string(cchar *s) {
  Immediate imm;
  imm.v_string = s;
  Sym *sym = if1_const(if1, sym_string, s, &imm);
  return sym;
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

void gen_ifexpr(PycAST *ifcond, PycAST *ifif, PycAST *ifelse, PycAST *ast) {
  ast->rval = new_sym(ast);
  if1_gen(if1, &ast->code, ifcond->code);
  Sym *t = new_sym(ast);
  call_method(&ast->code, ast, ifcond->rval, sym___pyc_to_bool__, t, 0);
  if1_if(if1, &ast->code, 0, t, ifif->code, ifif->rval, ifelse ? ifelse->code : 0, ifelse ? ifelse->rval : 0, ast->rval,
         ast);
}


Sym *make_symbol(cchar *name) {
  return if1_make_symbol(if1, name);
}

/*
  Copyright 2008-2011 John Plevyak, All Rights Reserved
*/
#include "python_ifa_int.h"

static int build_if1(mod_ty mod, PycContext &ctx, Code **code);

static void build_import_if1(char *sym, char *as, char *from, PycContext &ctx) {
  char *mod = from ? from : sym;
  if (!strcmp(mod, "pyc_compat")) return;
  PycModule *m = get_module(mod, ctx);
  assert(m);
  if (!m->built_if1) {
    Code **c = &getAST((stmt_ty)ctx.node, ctx)->code;
    build_module_attributes_if1(m, *m->ctx, c);
    build_if1(m->mod, *m->ctx, c);
    m->built_if1 = true;
  }
}

static Code *find_send(Code *c) {
  if (c->kind == Code_SEND) return c;
  assert(c->kind == Code_SUB);
  for (int i = c->sub.n - 1; i >= 0; i--) {
    if (c->sub[i]->kind == Code_SUB) {
      Code *cc = find_send(c->sub[i]);
      if (cc) return cc;
    }
    if (c->sub[i]->kind == Code_SEND) return c->sub[i];
  }
  return 0;
}

#define RECURSE(_ast, _fn, _ctx)                 \
  PycAST *ast = getAST(_ast, ctx);               \
  for (auto x : ast->pre_scope_children.values()) { \
    if (x->xstmt)                                \
      _fn(x->xstmt, ctx);                        \
    else if (x->xexpr)                           \
      _fn(x->xexpr, ctx);                        \
  }                                              \
  enter_scope(_ast, ast, ctx);                   \
  for (auto x : ast->children.values()) {         \
    if (x->xstmt)                                \
      _fn(x->xstmt, ctx);                        \
    else if (x->xexpr)                           \
      _fn(x->xexpr, ctx);                        \
  }                                              \
  ASSERT(ast == getAST(_ast, ctx));

static int build_if1(stmt_ty s, PycContext &ctx);
static int build_if1(expr_ty e, PycContext &ctx);

static int build_if1(stmt_ty s, PycContext &ctx) {
  RECURSE(s, build_if1, ctx);
  switch (s->kind) {
    case FunctionDef_kind:  // identifier name, arguments args, stmt* body, expr* decorator_list
      gen_fun(s, ast, ctx);
      break;
    case ClassDef_kind:  // identifier name, expr* bases, stmt* body, expr* decorator_list
      gen_class(s, ast, ctx);
      break;
    case Return_kind:  // expr? value
      if (s->v.Return.value) {
        PycAST *a = getAST(s->v.Return.value, ctx);
        ctx.fun()->fun_returns_value = 1;
        if1_gen(if1, &ast->code, a->code);
        if1_move(if1, &ast->code, a->rval, ctx.fun()->ret, ast);
      } else
        if1_move(if1, &ast->code, sym_nil, ctx.fun()->ret, ast);
      if1_goto(if1, &ast->code, ast->label[0])->ast = ast;
      break;
    case Delete_kind:  // expr * targets
      break;
    case Assign_kind:  // expr* targets, expr value
    {
      PycAST *v = getAST(s->v.Assign.value, ctx);
      if1_gen(if1, &ast->code, v->code);
      for (int i = 0; i < asdl_seq_LEN(s->v.Assign.targets); i++) {
        PycAST *a = getAST((expr_ty)asdl_seq_GET(s->v.Assign.targets, i), ctx);
        if (a->xexpr && a->xexpr->kind == Tuple_kind) {  // destructure
          if (!a->sym) fail("error line %d, illegal destructuring", ctx.lineno);
          expr_ty t = a->xexpr;
          for (int i = 0; i < asdl_seq_LEN(t->v.Tuple.elts); i++) {
            expr_ty ret = (expr_ty)asdl_seq_GET(t->v.Tuple.elts, i);
            call_method(if1, &ast->code, ast, v->rval, sym___getitem__, getAST(ret, ctx)->rval, 1, int64_constant(i));
          }
        } else {
          if1_gen(if1, &ast->code, a->code);
          if (a->is_member)
            if1_send(if1, &ast->code, 5, 1, sym_operator, a->rval, sym_setter, a->sym, v->rval,
                     (ast->rval = new_sym(ast)))
                ->ast = ast;
          else if (a->is_object_index)
            if1_add_send_arg(if1, find_send(a->code), v->rval);
          else
            if1_move(if1, &ast->code, v->rval, a->sym);
        }
      }
      ast->rval = 0;
      break;
    }
    case AugAssign_kind:  // expr target, operator op, expr value
    {
      PycAST *v = getAST(s->v.AugAssign.value, ctx);
      if1_gen(if1, &ast->code, v->code);
      PycAST *t = getAST(s->v.AugAssign.target, ctx);
      if1_gen(if1, &ast->code, t->code);
      if (t->is_member) {
        Sym *tmp = new_sym(ast);
        Sym *tmp2 = new_sym(ast);
        if1_send(if1, &ast->code, 4, 1, sym_operator, t->rval, sym_period, t->sym, tmp2)->ast = ast;
        if1_send(if1, &ast->code, 3, 1, map_ioperator(s->v.AugAssign.op), tmp2, v->rval, tmp)->ast = ast;
        if1_send(if1, &ast->code, 5, 1, sym_operator, t->rval, sym_setter, t->sym, tmp, (ast->rval = new_sym(ast)))
            ->ast = ast;
      } else if (t->is_object_index) {
        if1_add_send_arg(if1, find_send(ast->code), v->rval);
        if1_send(if1, &ast->code, 3, 1, map_ioperator(s->v.AugAssign.op), t->rval, v->rval, (ast->rval = new_sym(ast)))
            ->ast = ast;
      } else {
        if1_send(if1, &ast->code, 3, 1, map_ioperator(s->v.AugAssign.op), t->rval, v->rval, (ast->rval = new_sym(ast)))
            ->ast = ast;
        if1_move(if1, &ast->code, ast->rval, t->sym, ast);
      }
      break;
    }
    case Print_kind:  // epxr? dest, expr *values, bool nl
      assert(!s->v.Print.dest);
      for (int i = 0; i < asdl_seq_LEN(s->v.Print.values); i++) {
        PycAST *a = getAST((expr_ty)asdl_seq_GET(s->v.Print.values, i), ctx);
        if1_gen(if1, &ast->code, a->code);
        if (i) if1_send(if1, &ast->code, 3, 1, sym_primitive, sym_write, make_string(" "), new_sym(ast))->ast = ast;
        Sym *t = new_sym(ast);
        call_method(if1, &ast->code, ast, a->rval, sym___str__, t, 0);
        if1_send(if1, &ast->code, 3, 1, sym_primitive, sym_write, t, new_sym(ast))->ast = ast;
      }
      if (s->v.Print.nl) if1_send(if1, &ast->code, 2, 1, sym_primitive, sym_writeln, new_sym(ast))->ast = ast;
      break;
    case For_kind:  // expr target, expr iter, stmt* body, stmt* orelse
    {
      PycAST *t = getAST(s->v.For.target, ctx), *i = getAST(s->v.For.iter, ctx);
      Sym *iter = new_sym(ast), *tmp = new_sym(ast), *tmp2 = new_sym(ast);
      if1_gen(if1, &ast->code, i->code);
      if1_gen(if1, &ast->code, t->code);
      if1_send(if1, &ast->code, 2, 1, sym___iter__, i->rval, iter)->ast = ast;
      Code *cond = 0, *body = 0, *orelse = 0, *next = 0;
      call_method(if1, &cond, ast, iter, sym___pyc_more__, tmp, 0);
      call_method(if1, &body, ast, iter, sym_next, tmp2, 0);
      if1_move(if1, &body, tmp2, t->sym, ast);
      get_stmts_code(s->v.For.body, &body, ctx);
      get_stmts_code(s->v.For.orelse, &orelse, ctx);
      if1_loop(if1, &ast->code, ast->label[0], ast->label[1], tmp, 0, cond, next, body, ast);
      if1_gen(if1, &ast->code, orelse);
      break;
    }
    case While_kind:  // expr test, stmt*body, stmt*orelse
    {
      PycAST *t = getAST(s->v.While.test, ctx);
      Code *body = 0, *orelse = 0;
      get_stmts_code(s->v.While.body, &body, ctx);
      get_stmts_code(s->v.While.orelse, &orelse, ctx);
      if1_loop(if1, &ast->code, ast->label[0], ast->label[1], t->rval, 0, t->code, 0, body, ast);
      if1_gen(if1, &ast->code, orelse);
      break;
    }
    case If_kind:  // expr test, stmt* body, stmt* orelse
      gen_if(getAST(s->v.If.test, ctx), s->v.If.body, s->v.If.orelse, ast, ctx);
      break;
    case With_kind:        // expr content_expr, expr? optional_vars, stmt *body
    case Raise_kind:       // expr? type, expr? int, expr? tback
    case TryExcept_kind:   // stmt* body, excepthandler *handlers, stmt *orelse
    case TryFinally_kind:  // stmt *body, stmt *finalbody
      fail("error line %d, exceptions not yet supported", ctx.lineno);
      break;
    case Assert_kind:  // expr test, expr? msg
      fail("error line %d, 'assert' not yet supported", ctx.lineno);
      break;
    case Import_kind:  // alias* name
      build_import(s, build_import_if1, ctx);
      break;
    case ImportFrom_kind:  // identifier module, alias *names, int? level
      build_import_from(s, build_import_if1, ctx);
      break;
    case Exec_kind:  // expr body, expr? globals, expr? locals
      fail("error line %d, 'exec' not yet supported", ctx.lineno);
      break;
    case Global_kind:  // identifier* names
      break;
#if PY_MAJOR_VERSION == 3
    case Nonlocal_kind:
      break;
#endif
    case Expr_kind:  // expr value
    {
      PycAST *a = getAST(s->v.Expr.value, ctx);
      ast->rval = a->rval;
      ast->code = a->code;
      break;
    }
    case Pass_kind:
      break;
    case Break_kind:
    case Continue_kind:
      if1_goto(if1, &ast->code, ast->label[0])->ast = ast;
      break;
  }
  exit_scope(s, ctx);
  return 0;
}

static Code *splice_primitive(PycAST *fun, expr_ty e, PycAST *ast, PycContext &ctx) {
  Code *send = if1_send1(if1, &ast->code, ast);
  if1_add_send_arg(if1, send, sym_primitive);
  if1_add_send_arg(if1, send, fun->rval);
  for (int i = 0; i < asdl_seq_LEN(e->v.Call.args); i++) {
    expr_ty arg = (expr_ty)asdl_seq_GET(e->v.Call.args, i);
    if1_add_send_arg(if1, send, getAST(arg, ctx)->rval);
  }
  ast->rval = new_sym(ast);
  if1_add_send_result(if1, send, ast->rval);
  return send;
}

static int build_builtin_call(PycAST *fun, expr_ty e, PycAST *ast, PycContext &ctx) {
  Sym *f = fun->sym;
  if (f && builtin_functions.set_in(f)) {
    if (f == sym_super) {
      if (!ctx.fun()) fail("super outside of function");
      Vec<Sym *> as;
      get_syms_args(ast, ((PycAST *)ctx.fun()->ast)->xstmt->v.FunctionDef.args, as, ctx);
      if (as.n < 1 || !ctx.fun()->in || ctx.fun()->in->is_fun) fail("super outside of method");
      int n = asdl_seq_LEN(e->v.Call.args);
      if (!n) {
        ast->rval = new_sym(ast);
        ast->rval->aspect = ctx.cls();
        if1_move(if1, &ast->code, ctx.fun()->self, ast->rval);
      } else if (n == 1) {
        PycAST *cls_ast = getAST((expr_ty)asdl_seq_GET(e->v.Call.args, 0), ctx);
        if (!cls_ast->sym || cls_ast->sym->type_kind != Type_RECORD) fail("non-constant super() class");
        ast->rval = new_sym(ast);
        ast->rval->aspect = cls_ast->sym;
        if1_move(if1, &ast->code, as[0], ast->rval);
      } else {
        if (n > 2) fail("bad number of arguments to builtin function 'super'");
        PycAST *a0 = getAST((expr_ty)asdl_seq_GET(e->v.Call.args, 0), ctx);
        PycAST *a1 = getAST((expr_ty)asdl_seq_GET(e->v.Call.args, 1), ctx);
        if (!a0->sym || a0->sym->type_kind != Type_RECORD) fail("non-constant super() class");
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
    } else if (f == sym___pyc_symbol__) {
      int n = asdl_seq_LEN(e->v.Call.args);
      if (n != 1) fail("bad number of arguments to builtin function %s", f->name);
      PycAST *a0 = getAST((expr_ty)asdl_seq_GET(e->v.Call.args, 0), ctx);
      if (a0->rval->type != sym_string || !a0->rval->constant)
        fail("string argument required for builtin function %s", f->name);
      ast->rval = make_symbol(a0->rval->constant);
    } else if (f == sym___pyc_clone_constants__) {
      Vec<Sym *> as;
      get_syms_args(ast, ((PycAST *)ctx.fun()->ast)->xstmt->v.FunctionDef.args, as, ctx);
      int n = asdl_seq_LEN(e->v.Call.args);
      if (n != 1) fail("bad number of arguments to builtin function %s", f->name);
      PycAST *a0 = getAST((expr_ty)asdl_seq_GET(e->v.Call.args, 0), ctx);
      ast->rval = a0->rval;
      ast->rval->clone_for_constants = 1;
    } else if (f == sym___pyc_c_call__) {
      splice_primitive(fun, e, ast, ctx)->rvals.v[2]->is_fake = 1;
    } else if (f == sym___pyc_c_code__) {
      PycAST *a0 = getAST((expr_ty)asdl_seq_GET(e->v.Call.args, 0), ctx);
      if (a0->rval->type != sym_string || !a0->rval->constant)
        fail("string argument required for builtin function %s", f->name);
      ctx.c_code.add(a0->rval->constant);
    } else if (f == sym___pyc_insert_c_code__ || f == sym___pyc_insert_c_header__ ||
               f == sym___pyc_include_c_header__) {
      PycAST *a0 = getAST((expr_ty)asdl_seq_GET(e->v.Call.args, 0), ctx);
      if (a0->rval->type != sym_string || !a0->rval->constant)
        fail("string argument required for builtin function %s", f->name);
      cchar *file = a0->rval->constant;
      cchar *prefix = strrchr(ctx.mod->filename, '/');
      char path[PATH_MAX];
      cchar *pathname = 0;
      if (f->name[0] == '/' || !prefix)
        pathname = file;
      else {
        strcpy(path, ctx.mod->filename);
        strcpy(path + (prefix - ctx.mod->filename + 1), file);
        pathname = path;
      }
      if (f == sym___pyc_insert_c_code__)
        ctx.c_code.add((char *)read_file_to_string(pathname));
      else if (f == sym___pyc_insert_c_header__) {
        char code[PATH_MAX + 100];
        sprintf(code, "#include \"%s\"\n", pathname);
        ctx.c_code.add(strdup(code));
      } else {
        char cmd[PATH_MAX + 100];
        sprintf(cmd, "gcc -E %s", pathname);
        FILE *fp = popen(cmd, "r");
        if (!fp) fail("unable to include '%s'", pathname);
        pclose(fp);
        sprintf(cmd, "#include \"%s\"\n", pathname);
      }
    } else
      fail("unimplemented builtin '%s'", fun->sym->name);
    return 1;
  }
  return 0;
}

static void build_list_comp(asdl_seq *generators, int x, expr_ty elt, PycAST *ast, Code **code, PycContext &ctx) {
  int n = asdl_seq_LEN(generators);
  if (x < n) {
    comprehension_ty c = (comprehension_ty)asdl_seq_GET(generators, x);
    PycAST *t = getAST(c->target, ctx), *i = getAST(c->iter, ctx);
    Code *before = 0, *cond = 0, *body = 0, *next = 0;
    Sym *iter = new_sym(ast), *cond_var = new_sym(ast), *tmp = new_sym(ast);
    if1_gen(if1, &before, i->code);
    if1_send(if1, &before, 2, 1, sym___iter__, i->rval, iter)->ast = ast;
    call_method(if1, &cond, ast, iter, sym___pyc_more__, cond_var, 0);
    if1_gen(if1, &body, t->code);
    call_method(if1, &body, ast, iter, sym_next, tmp, 0);
    if1_move(if1, &body, tmp, t->sym, ast);
    if (asdl_seq_LEN(c->ifs)) {
      Label *short_circuit = if1_alloc_label(if1);
      for (int i = 0; i < asdl_seq_LEN(c->ifs); i++) {
        PycAST *ifast = getAST((expr_ty)asdl_seq_GET(c->ifs, i), ctx);
        if1_gen(if1, &body, ifast->code);
        Code *ifcode = if1_if_goto(if1, &body, ifast->rval, ifast);
        if1_if_label_false(if1, ifcode, short_circuit);
        if1_if_label_true(if1, ifcode, if1_label(if1, &body, ifast));
      }
      build_list_comp(generators, x + 1, elt, ast, &body, ctx);
      if1_label(if1, &body, ast, short_circuit);
    } else
      build_list_comp(generators, x + 1, elt, ast, &body, ctx);
    if1_loop(if1, code, if1_alloc_label(if1), if1_alloc_label(if1), cond_var, before, cond, next, body, ast);
  } else {
    PycAST *a = getAST(elt, ctx);
    if1_gen(if1, code, a->code);
    call_method(if1, code, ast, ast->rval, sym_append, new_sym(ast), 1, a->rval);
  }
}

static int build_if1(expr_ty e, PycContext &ctx) {
  RECURSE(e, build_if1, ctx);
  switch (e->kind) {
    case BoolOp_kind:  // boolop op, expr* values
    {
      bool a = e->v.BoolOp.op == And;
      (void)a;
      int n = asdl_seq_LEN(e->v.BoolOp.values);
      if (n == 1) {
        PycAST *v = getAST((expr_ty)asdl_seq_GET(e->v.BoolOp.values, 0), ctx);
        ast->code = v->code;
        ast->rval = v->rval;
      } else {
        ast->label[0] = if1_alloc_label(if1);
        ast->rval = new_sym(ast);
        for (int i = 0; i < n - 1; i++) {
          PycAST *v = getAST((expr_ty)asdl_seq_GET(e->v.BoolOp.values, i), ctx);
          if1_gen(if1, &ast->code, v->code);
          if1_move(if1, &ast->code, v->rval, ast->rval);
          Sym *t = new_sym(ast);
          call_method(if1, &ast->code, ast, v->rval, sym___pyc_to_bool__, t, 0);
          Code *ifcode = if1_if_goto(if1, &ast->code, t, ast);
          if (a) {
            if1_if_label_false(if1, ifcode, ast->label[0]);
            if1_if_label_true(if1, ifcode, if1_label(if1, &ast->code, ast));
          } else {
            if1_if_label_true(if1, ifcode, ast->label[0]);
            if1_if_label_false(if1, ifcode, if1_label(if1, &ast->code, ast));
          }
        }
        PycAST *v = getAST((expr_ty)asdl_seq_GET(e->v.BoolOp.values, n - 1), ctx);
        if1_gen(if1, &ast->code, v->code);
        if1_move(if1, &ast->code, v->rval, ast->rval, ast);
        if1_label(if1, &ast->code, ast, ast->label[0]);
      }
      break;
    }
    case BinOp_kind:  // expr left, operator op, expr right
      ast->rval = new_sym(ast);
      if1_gen(if1, &ast->code, getAST(e->v.BinOp.left, ctx)->code);
      if1_gen(if1, &ast->code, getAST(e->v.BinOp.right, ctx)->code);
      if1_send(if1, &ast->code, 3, 1, map_operator(e->v.BinOp.op), getAST(e->v.BinOp.left, ctx)->rval,
               getAST(e->v.BinOp.right, ctx)->rval, ast->rval)
          ->ast = ast;
      break;
    case UnaryOp_kind:  // unaryop op, expr operand
      ast->rval = new_sym(ast);
      if1_gen(if1, &ast->code, getAST(e->v.UnaryOp.operand, ctx)->code);
      if1_send(if1, &ast->code, 2, 1, map_unary_operator(e->v.UnaryOp.op), getAST(e->v.UnaryOp.operand, ctx)->rval,
               ast->rval)
          ->ast = ast;
      break;
    case Lambda_kind:  // arguments args, expr body
      gen_fun(e, ast, ctx);
      break;
    case IfExp_kind:  // expr test, expr body, expr orelse
      gen_ifexpr(getAST(e->v.IfExp.test, ctx), getAST(e->v.IfExp.body, ctx), getAST(e->v.IfExp.orelse, ctx), ast);
      break;
    case Dict_kind:  // expr* keys, expr* values
      break;
#if PY_MAJOR_VERSION > 2 || PY_MINOR_VERSION > 6
    case SetComp_kind:
    case DictComp_kind:
      assert(!"implemented");
#endif
    case ListComp_kind: {  // expr elt, comprehension* generators
      // elt is the expression describing the result
      ast->rval = new_sym(ast);
      if1_send(if1, &ast->code, 3, 1, sym_primitive, sym_make, sym_list, ast->rval)->ast = ast;
      build_list_comp(e->v.ListComp.generators, 0, e->v.ListComp.elt, ast, &ast->code, ctx);
      break;
    }
    case GeneratorExp_kind:  // expr elt, comprehension* generators
      break;
    case Yield_kind:  // expr? value
      break;
    case Compare_kind: {  // expr left, cmpop* ops, expr* comparators
      int n = asdl_seq_LEN(e->v.Compare.ops);
      ast->label[0] = if1_alloc_label(if1);  // short circuit
      ast->label[1] = if1_alloc_label(if1);  // end
      ast->rval = new_sym(ast);
      PycAST *lv = getAST(e->v.Compare.left, ctx);
      if1_gen(if1, &ast->code, lv->code);
      if (n == 1) {
        PycAST *v = getAST((expr_ty)asdl_seq_GET(e->v.Compare.comparators, 0), ctx);
        if1_gen(if1, &ast->code, v->code);
        if1_send(if1, &ast->code, 3, 1, map_cmp_operator((cmpop_ty)asdl_seq_GET(e->v.Compare.ops, 0)), lv->rval,
                 v->rval, ast->rval)
            ->ast = ast;
      } else {
        Sym *ls = lv->rval, *s = 0;
        for (int i = 0; i < n; i++) {
          PycAST *v = getAST((expr_ty)asdl_seq_GET(e->v.Compare.comparators, i), ctx);
          if1_gen(if1, &ast->code, v->code);
          s = new_sym(ast);
          if1_send(if1, &ast->code, 3, 1, map_cmp_operator((cmpop_ty)asdl_seq_GET(e->v.Compare.ops, i)), ls, v->rval, s)
              ->ast = ast;
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
    case Call_kind: {  // expr func, expr* args, keyword* keywords, expr? starargs, expr? kwargs
      PycAST *fun = getAST((expr_ty)e->v.Call.func, ctx);
      if1_gen(if1, &ast->code, fun->code);
      for (int i = 0; i < asdl_seq_LEN(e->v.Call.args); i++)
        if1_gen(if1, &ast->code, getAST((expr_ty)asdl_seq_GET(e->v.Call.args, i), ctx)->code);
      if (build_builtin_call(fun, e, ast, ctx)) break;
      {
        Code *send = 0;
        if (!fun->is_member) {
          send = if1_send1(if1, &ast->code, ast);
          if1_add_send_arg(if1, send, fun->rval);
        } else {
          Sym *t = new_sym(ast);
          Code *op = if1_send(if1, &ast->code, 4, 1, sym_operator, fun->rval, sym_period, fun->sym, t);
          op->ast = ast;
          op->partial = Partial_OK;
          send = if1_send1(if1, &ast->code, ast);
          if1_add_send_arg(if1, send, t);
        }
        for (int i = 0; i < asdl_seq_LEN(e->v.Call.args); i++) {
          expr_ty arg = (expr_ty)asdl_seq_GET(e->v.Call.args, i);
          if1_add_send_arg(if1, send, getAST(arg, ctx)->rval);
        }
        ast->rval = new_sym(ast);
        if1_add_send_result(if1, send, ast->rval);
      }
      break;
    }
    case Repr_kind:  // expr value
      fail("error line %d, 'repr' not yet supported", ctx.lineno);
      break;
      break;
    case Num_kind:
      ast->rval = make_num(e->v.Num.n, ctx);
      break;
    case Str_kind:
      ast->rval = make_string(e->v.Str.s);
      break;
    case Attribute_kind:  // expr value, identifier attr, expr_context ctx
      if1_gen(if1, &ast->code, getAST(e->v.Attribute.value, ctx)->code);
      if ((ast->parent->is_assign() && ast->parent->children.last() != ast) ||
          (ast->parent->is_call() && ast->parent->xexpr->v.Call.func == e)) {
        ast->sym = make_symbol(PyString_AsString(e->v.Attribute.attr));
        ast->rval = getAST(e->v.Attribute.value, ctx)->rval;
        if (ast->rval->self) ast->rval = ast->rval->self;
        ast->is_member = 1;
      } else {
        ast->rval = new_sym(ast);
        Sym *v = getAST(e->v.Attribute.value, ctx)->rval;
        if (v->self) v = v->self;
        Code *send = if1_send(if1, &ast->code, 4, 1, sym_operator, v, sym_period,
                              make_symbol(PyString_AsString(e->v.Attribute.attr)), ast->rval);
        send->ast = ast;
        send->partial = Partial_OK;
      }
      break;
    case Subscript_kind: {  // expr value, slice slice, expr_context ctx
      if1_gen(if1, &ast->code, getAST(e->v.Subscript.value, ctx)->code);
      ast->is_object_index = 1;
      if (e->v.Subscript.slice->kind == Index_kind) {
        if1_gen(if1, &ast->code, getAST(e->v.Subscript.slice->v.Index.value, ctx)->code);
        // AugLoad, Load, AugStore, Store, Del, !Param
        if (e->v.Subscript.ctx == Load) {
          call_method(if1, &ast->code, ast, getAST(e->v.Subscript.value, ctx)->rval, sym___getitem__,
                      (ast->rval = new_sym(ast)), 1, getAST(e->v.Subscript.slice->v.Index.value, ctx)->rval);
        } else {
          assert(e->v.Subscript.ctx == Store);
          call_method(if1, &ast->code, ast, getAST(e->v.Subscript.value, ctx)->rval, sym___setitem__,
                      (ast->rval = new_sym(ast)), 1, getAST(e->v.Subscript.slice->v.Index.value, ctx)->rval);
        }
      } else if (e->v.Subscript.slice->kind == Slice_kind) {
        Sym *l = gen_or_default(e->v.Subscript.slice->v.Slice.lower, int64_constant(0), ast, ctx);
        Sym *u = gen_or_default(e->v.Subscript.slice->v.Slice.upper, int64_constant(INT_MAX), ast, ctx);
        Sym *s = gen_or_default(e->v.Subscript.slice->v.Slice.step, int64_constant(1), ast, ctx);
        (void)s;
        if (e->v.Subscript.ctx == Load) {
          if (!e->v.Subscript.slice->v.Slice.step)
            call_method(if1, &ast->code, ast, getAST(e->v.Subscript.value, ctx)->rval, sym___getslice__,
                        (ast->rval = new_sym(ast)), 2, l, u);
          else
            call_method(if1, &ast->code, ast, getAST(e->v.Subscript.value, ctx)->rval, sym___pyc_getslice__,
                        (ast->rval = new_sym(ast)), 3, l, u, s);
        } else {
          if (!e->v.Subscript.slice->v.Slice.step)
            call_method(if1, &ast->code, ast, getAST(e->v.Subscript.value, ctx)->rval, sym___setslice__,
                        (ast->rval = new_sym(ast)), 2, l, u);
          else
            call_method(if1, &ast->code, ast, getAST(e->v.Subscript.value, ctx)->rval, sym___pyc_setslice__,
                        (ast->rval = new_sym(ast)), 3, l, u, s);
        }
      } else
        assert(!"implemented");
      break;
    }
    case Name_kind: {                                // identifier id, expr_context ctx
      if (e->v.Name.ctx == EXPR_CONTEXT_SYM) break;  // skip
      int level = 0;
      TEST_SCOPE printf("%sfound '%s' at level %d\n", ast->sym ? "" : "not ",
                        cannonicalize_string(PyString_AS_STRING(e->v.Name.id)), level);
      bool load = e->v.Name.ctx == Load;
      Sym *in = ctx.scope_stack[ctx.scope_stack.n - 1]->in;
      if (in && in->type_kind == Type_RECORD && in->has.in(ast->sym)) {  // in __main__
        if (load)
          if1_send(if1, &ast->code, 4, 1, sym_operator, ctx.fun()->self, sym_period, make_symbol(ast->sym->name),
                   (ast->rval = new_sym(ast)))
              ->ast = ast;
        else {
          ast->is_member = 1;
          ast->sym = make_symbol(ast->sym->name);
          ast->rval = ctx.fun()->self;
        }
      }
      break;
    }
#if PY_MAJOR_VERSION > 2 || PY_MINOR_VERSION > 6
    case Set_kind:
      assert(!"implemented");
      break;
#endif
    case List_kind:  // expr* elts, expr_context ctx
    // FALL THROUGH
    case Tuple_kind:  // expr *elts, expr_context ctx
      for (int i = 0; i < asdl_seq_LEN(e->v.List.elts); i++)
        if1_gen(if1, &ast->code, getAST((expr_ty)asdl_seq_GET(e->v.List.elts, i), ctx)->code);
      {
        Code *send = if1_send1(if1, &ast->code, ast);
        if1_add_send_arg(if1, send, sym_primitive);
        if1_add_send_arg(if1, send, sym_make);
        if1_add_send_arg(if1, send, e->kind == List_kind ? sym_list : sym_tuple);
        for (int i = 0; i < asdl_seq_LEN(e->v.List.elts); i++) {
          expr_ty arg = (expr_ty)asdl_seq_GET(e->v.List.elts, i);
          if1_add_send_arg(if1, send, getAST(arg, ctx)->rval);
        }
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

static int build_if1(mod_ty mod, PycContext &ctx, Code **code) {
  int r = 0;
  ctx.node = mod;
  enter_scope(ctx);
  switch (mod->kind) {
    case Module_kind:
      r = build_if1_stmts(mod->v.Module.body, ctx, code);
      break;
    case Expression_kind: {
      r = build_if1(mod->v.Expression.body, ctx);
      if1_gen(if1, code, getAST(mod->v.Expression.body, ctx)->code);
      break;
    }
    case Interactive_kind:
      r = build_if1_stmts(mod->v.Interactive.body, ctx, code);
      break;
    case Suite_kind:
      assert(!"handled");
  }
  exit_scope(ctx);
  return r;
}

int build_if1_module(mod_ty mod, PycContext &ctx, Code **code) { return build_if1(mod, ctx, code); }

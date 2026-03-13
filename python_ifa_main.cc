// SPDX-License-Identifier: BSD-3-Clause
#include "python_ifa_int.h"

static void build_environment(PycModule *mod, PycCompiler &ctx) {
  ctx.mod = mod;
  ctx.node = mod->pymod;
  enter_scope(ctx);
  scope_sym(ctx, sym_int);
  scope_sym(ctx, sym_float);
  scope_sym(ctx, sym_complex);
  scope_sym(ctx, sym_string);
  scope_sym(ctx, sym_list);
  scope_sym(ctx, sym_tuple);
  scope_sym(ctx, sym_bool);
  scope_sym(ctx, sym_true);
  scope_sym(ctx, sym_false);
  scope_sym(ctx, sym_nil);
  scope_sym(ctx, sym_nil_type);
  scope_sym(ctx, sym_any);
  scope_sym(ctx, sym_unknown);
  scope_sym(ctx, sym_ellipsis);
  scope_sym(ctx, sym_object);
  scope_sym(ctx, sym_super);
  scope_sym(ctx, sym_uint8, "__pyc_char__");
  scope_sym(ctx, sym_operator, "__pyc_operator__");
  scope_sym(ctx, sym_primitive, "__pyc_primitive__");
  scope_sym(ctx, sym_declare, "__pyc_declare__");
  sym_declare->is_fake = true;
#define P(_x) scope_sym(ctx, sym_##_x);
#include "pyc_symbols.h"
  exit_scope(ctx);
}

static void build_init(Code *code) {
  Sym *fn = sym___main__;
  fn->cont = new_sym();
  fn->ret = sym_nil;
  if1_send(if1, &code, 4, 0, sym_primitive, sym_reply, fn->cont, fn->ret);
  if1_closure(if1, fn, code, 1, &fn);
}

static void c_call_transfer_function(PNode *pn, EntrySet *es) {
  AVar *a = make_AVar(pn->rvals[2], es);
  AVar *result = make_AVar(pn->lvals[0], es);
  // either provide an example or an explicity type (which will be a meta_type)
  if (a->out->n == 1 && a->out->v[0]->sym->is_meta_type)
    update_gen(result, make_abstract_type(a->out->v[0]->sym->meta_type));
  else
    flow_vars(a, result);
}

static void c_call_codegen(FILE *fp, PNode *n, Fun *f) {
  fputs(n->rvals[3]->sym->constant, fp);
  fputs("(", fp);
  int first = 1;
  for (int i = 5; i < n->rvals.n; i += 2) {
    if (!first) {
      fputs(", ", fp);
    } else
      first = 0;
    fputs(n->rvals[i]->cg_string, fp);
  }
  fputs(");\n", fp);
}

static void format_string_transfer_function(PNode *pn, EntrySet *es) {
  AVar *result = make_AVar(pn->lvals[0], es);
  update_gen(result, make_abstract_type(sym_string));
}

static void to_str_transfer_function(PNode *pn, EntrySet *es) {
  AVar *result = make_AVar(pn->lvals[0], es);
  update_gen(result, make_abstract_type(sym_string));
}

static void format_string_codegen(FILE *fp, PNode *n, Fun *f) {
  fputs("_CG_format_string(", fp);
  fputs(n->rvals[2]->cg_string, fp);
  Var *v = n->rvals[3];
  if (v->type->type_kind == Type_RECORD) {
    for (int i = 0; i < v->type->has.n; i++) fprintf(fp, ", %s->e%d", v->cg_string, i);
  } else {
    fputs(", ", fp);
    fputs(n->rvals[3]->cg_string, fp);
  }
  fputs(");\n", fp);
}

static void to_str_codegen(FILE *fp, PNode *n, Fun *f) {
  Var *v = n->rvals[2];
  if (v->type->is_meta_type && v->type->name) {
    fputs("_CG_String(\"<class '", fp);
    fputs(v->type->name, fp);
    fputs("'>\");", fp);
  } else
    fputs("_CG_String(\"<instance>\");", fp);
}

static void write_codegen(FILE *fp, PNode *n, Fun *f) {
  fputs("_CG_write(", fp);
  fputs(n->rvals[n->rvals.n - 1]->cg_string, fp);
  fputs(");\n", fp);
}

static void writeln_codegen(FILE *fp, PNode *n, Fun *f) {
  fputs("_CG_writeln(", fp);
  fputs(");\n", fp);
}

static void add_primitive_transfer_functions() {
  prim_reg(sym_write->name, return_nil_transfer_function, write_codegen)->is_visible = 1;
  prim_reg(sym_writeln->name, return_nil_transfer_function, writeln_codegen)->is_visible = 1;
  prim_reg(sym___pyc_c_call__->name, c_call_transfer_function, c_call_codegen)->is_visible = 1;
  prim_reg(sym___pyc_format_string__->name, format_string_transfer_function, format_string_codegen)->is_visible = 1;
  prim_reg(sym___pyc_to_str__->name, to_str_transfer_function, to_str_codegen)->is_visible = 1;
  prim_reg(cannonicalize_string("to_string"), return_string_transfer_function)->is_visible = 1;
}

/*
  Sym::aspect is set by the code handling builtin 'super' to
  the class whose superclass we wish to dispatch to.  Replace
  with the dispatched-to class.
*/
static void fixup_aspect() {
  for (int x = finalized_aspect; x < if1->allsyms.n; x++) {
    Sym *s = if1->allsyms[x];
    if (s->aspect) {
      if (s->aspect->dispatch_types.n < 2) fail("unable to dispatch to super of '%s'", s->aspect->name);
      s->aspect = s->aspect->dispatch_types[1];
    }
  }
  finalized_aspect = if1->allsyms.n;
}

void build_module_attributes_if1(PycModule *mod, PycCompiler &ctx, Code **code) {
  ctx.node = mod->pymod;
  enter_scope(ctx);
  if (mod == ctx.modules->v[1])
    if1_move(if1, code, make_string("__main__"), mod->name_sym->sym);
  else
    if1_move(if1, code, make_string(mod->name), mod->name_sym->sym);
  if1_move(if1, code, make_string(mod->filename), mod->file_sym->sym);
  // if1_move(if1, code, ..., __path__);
  exit_scope(ctx);
}

static int add_dirnames(cchar *p, Vec<cchar *> &a) {
  if (a.n > 100) return 0;
  struct dirent **namelist = 0;
  int n = scandir(p, &namelist, 0, alphasort), r = 0;
  if (n < 1) return r;
  for (int i = 0; i < n; i++) {
    if (STREQ(namelist[i]->d_name, ".") || STREQ(namelist[i]->d_name, "..")) continue;
    if (STREQ(namelist[i]->d_name, "EGG-INFO")) continue;
    if (strlen(namelist[i]->d_name) > 9 && STREQ(&namelist[i]->d_name[strlen(namelist[i]->d_name) - 9], ".egg-info"))
      continue;
    if (!is_directory(p, "/", namelist[i]->d_name)) continue;
    if (is_regular_file(p, "/__init__.py")) continue;
    a.add(dupstrs(p, "/", namelist[i]->d_name));
    r++;
    // free(namelist[i]); GC doesn't play well with standard malloc/free
  }
  // free(namelist); GC doesn't play well with standard malloc/free
  return r;
}

static int add_subdirs(cchar *p, Vec<cchar *> &a) {
  int s = a.n, n = add_dirnames(p, a), e = s + n;
  for (int i = s; i < e; i++) add_subdirs(a[i], a);
  return n;
}

static void build_search_path(PycCompiler &ctx) {
  char f[PATH_MAX];
  char *here = dupstr(getcwd(f, PATH_MAX));
  ctx.search_path = new Vec<cchar *>;
  ctx.search_path->add(here);
  const char *pythonpath_env = getenv("PYTHONPATH");
  if (!pythonpath_env) return;
  char *path = (char *)pythonpath_env;
  while (1) {
    char *p = path;
    char *e = strchr(p, ':'), *ee = e;
    while (e > p && e[-1] == '/') e--;
    p = dupstr(p, e);
    if (file_exists(p)) {
      ctx.search_path->add(p);
      add_subdirs(p, *ctx.search_path);
    }
    if (!ee) break;
    path = ee + 1;
  }
}


void install_new_fun(Sym *f) {
  if1_finalize_closure(if1, f);
  Fun *fun = new Fun(f);
  finalize_types(if1);
  fixup_aspect();
  build_arg_positions(fun);
  pdb->add(fun);
  if1_write_log();
}

int ast_to_if1(Vec<PycModule *> &mods) {
  PycCompiler *ctx = new PycCompiler();
  ifa_init(ctx);
  if1->partial_default = Partial_NEVER;
  build_builtin_symbols();
  add_primitive_transfer_functions();
  ctx->modules = &mods;
  Code *code = 0;
  for (auto x : mods.values()) x->filename = cannonicalize_string(x->filename);
  ctx->filename = mods[0]->filename;
  build_search_path(*ctx);
  build_environment(mods[0], *ctx);
  Vec<PycModule *> base_mods(mods);
  for (auto x : base_mods.values()) if (build_syms(x, *ctx) < 0) return -1;
  finalize_types(if1);
  for (auto x : base_mods.values()) {
    ctx->mod = x;
    build_module_attributes_if1(x, *ctx, &code);
    build_if1_module_pyda(x->pymod, *ctx, &code);
  }
  finalize_types(if1);
  if (test_scoping) exit(0);
  enter_scope(mods[0]->pymod, *ctx);
  build_init(code);
  exit_scope(*ctx);
  build_type_hierarchy();
  fixup_aspect();
  return 0;
}

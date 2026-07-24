// SPDX-License-Identifier: BSD-3-Clause
#include "python_ifa_int.h"
#include "python_parse.h"

#ifdef USE_LLVM
#include "codegen/llvm.h"
#endif

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
  cchar *name = n->rvals[3]->sym->constant;
  if (name && !strcmp(name, "__pyc_net_wait_read__")) {
    fprintf(fp, "co_await _CG_Await_Net_Read{(int)%s};\n", n->rvals[5]->cg_string);
    return;
  }
  if (name && !strcmp(name, "__pyc_net_wait_write__")) {
    fprintf(fp, "co_await _CG_Await_Net_Write{(int)%s};\n", n->rvals[5]->cg_string);
    return;
  }
  fputs(name, fp);
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
  // The 4th arg to prim_reg (llvm_cgfn) and the special-case
  // `to_string` LLVM cgfn retired with v1 LLVM (issue 014).
  // v2 LLVM doesn't consult RegisteredPrim::llvm_cgfn — it
  // dispatches via the lower_send_prim chain in cg_normalize_v2.
  prim_reg(sym_write->name, return_nil_transfer_function, write_codegen)->is_visible = 1;
  prim_reg(sym_writeln->name, return_nil_transfer_function, writeln_codegen)->is_visible = 1;
  RegisteredPrim *c_call_prim = prim_reg(sym___pyc_c_call__->name, c_call_transfer_function, c_call_codegen);
  c_call_prim->is_visible = 1;
  c_call_prim->is_functional = 0;
  prim_reg(sym___pyc_format_string__->name, format_string_transfer_function, format_string_codegen)->is_visible = 1;
  prim_reg(sym___pyc_to_str__->name, to_str_transfer_function, to_str_codegen)->is_visible = 1;
  prim_reg(cannonicalize_string("to_string"), return_string_transfer_function)->is_visible = 1;
  prim_reg(cannonicalize_string("__pyc_net_wait_read__"), return_nil_transfer_function)->is_visible = 1;
  prim_reg(cannonicalize_string("__pyc_net_wait_write__"), return_nil_transfer_function)->is_visible = 1;
}

/*
  Sym::aspect is set by the code handling builtin 'super' to
  the class whose superclass we wish to dispatch to.  Replace
  with the dispatched-to class.

  Only the Syms super's lowering registered in super_aspect_syms
  get this hop: issue 027's class-qualified static dispatch
  (`Base.method(recv, ...)`) also sets ->aspect, but to its FINAL
  masquerade class -- hopping those to the superclass would
  mis-dispatch every qualified call one level up.
*/
static void fixup_aspect() {
  for (Sym *s : super_aspect_syms) if (s && s->aspect) {
    if (s->aspect->dispatch_types.n < 2) fail("unable to dispatch to super of '%s'", s->aspect->name);
    s->aspect = s->aspect->dispatch_types[1];
  }
  super_aspect_syms.clear();
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
  // pyc's own standard-library shims (math, ...) live under
  // <system_dir>/pyc_lib, alongside the __pyc__ builtin module. Put
  // them on the module search path so `import math` resolves to the
  // shim (issue 025 bucket C). The cwd is searched first, so a user
  // module can still shadow a shim with its own file.
  ctx.search_path->add(dupstrs((cchar *)system_dir, "/pyc_lib"));
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

// Stage-3 REPL: one-time baseline.  Initialises the global if1/pdb/ctx and
// processes the builtin module through build_syms + build_if1.  Does NOT call
// build_init or build_type_hierarchy — those require the user module.
// builtin_mods must outlive all fork children (use a static Vec in the caller).
BaselineIF1State ast_to_if1_baseline(Vec<PycModule *> &builtin_mods) {
  PycCompiler *ctx = new PycCompiler();
  ifa_init(ctx);
  if1->partial_default = Partial_NEVER;
  build_builtin_symbols();
  add_primitive_transfer_functions();
  ctx->modules = &builtin_mods;
  Code *code = 0;
  builtin_mods[0]->filename = cannonicalize_string(builtin_mods[0]->filename);
  ctx->filename = builtin_mods[0]->filename;
  build_search_path(*ctx);
  build_environment(builtin_mods[0], *ctx);
  if (build_syms(builtin_mods[0], *ctx) < 0) fail("baseline: builtin build_syms failed");
  // issue 011: computed once here (self-contained -- the builtin
  // module never calls into user code) and shared via CoW across
  // every REPL fork child, so ast_to_if1_extend's own call only has
  // to iterate over the user-level call graph it collects.
  compute_can_raise(builtin_mods, *ctx);
  finalize_types(if1);
  ctx->mod = builtin_mods[0];
  build_module_attributes_if1(builtin_mods[0], *ctx, &code);
  build_if1_module_pyda(builtin_mods[0]->pymod, *ctx, &code);
  finalize_types(if1);
  return {ctx, code};
}

// Stage-3 REPL: per-iteration extend (called in a fork child).
// The fork child inherits baseline's if1/ctx via CoW.  This function updates
// ctx->modules to all_mods, processes user modules (all_mods[1..]), then
// finalises with build_init + build_type_hierarchy.
int ast_to_if1_extend(Vec<PycModule *> &all_mods, BaselineIF1State bl) {
  PycCompiler *ctx = bl.ctx;
  Code *code = bl.code;
  ctx->modules = &all_mods;
  // Snapshot n: build_syms may add imported modules to all_mods via import_file.
  // Those are processed lazily by build_import_if1; we must not double-process them.
  int n_user = all_mods.n;
  for (int i = 1; i < n_user; i++) {
    PycModule *x = all_mods[i];
    x->filename = cannonicalize_string(x->filename);
    if (build_syms(x, *ctx) < 0) return -1;
  }
  // issue 011: user-level call graph (the builtin module's own was
  // already computed once in ast_to_if1_baseline). all_mods.n here
  // (not the n_user snapshot) so imports build_syms pulled in mid-loop
  // are included -- their build_syms already ran, inline, above.
  {
    Vec<PycModule *> user_mods;
    for (int i = 1; i < all_mods.n; i++) user_mods.add(all_mods[i]);
    compute_can_raise(user_mods, *ctx);
  }
  finalize_types(if1);
  for (int i = 1; i < n_user; i++) {
    PycModule *x = all_mods[i];
    ctx->mod = x;
    build_module_attributes_if1(x, *ctx, &code);
    build_if1_module_pyda(x->pymod, *ctx, &code);
  }
  finalize_types(if1);
  if (test_scoping) exit(0);
  enter_scope(all_mods[0]->pymod, *ctx);
  build_init(code);
  exit_scope(*ctx);
  build_type_hierarchy();
  fixup_aspect();
  return 0;
}

// issue 011 (per-callee can-raise gating, post-FA refinement): the
// precise Fun-level pass complementing Sym::can_raise's pre-FA
// syntactic one (python_ifa_build_syms.cc's compute_can_raise). Must
// run after ifa_analyze() (pyc.cc) -- that's when Fun::calls, built
// by clone() partway through it, first exists and is stable; running
// any earlier would see an incomplete or absent call graph. A simple
// worklist fixed point over calls_funs() (the union of a Fun's own
// call-site resolutions, not the per-site precision codegen's
// cg_exc_check_provably_safe separately needs from Fun::calls
// directly) -- same shape as ifa/optimize/inline.cc's simple_inlining,
// which walks the same post-clone call graph.
void compute_fun_can_raise() {
  for (Fun *f : fa->funs)
    if (f->sym && f->sym->direct_raise) f->can_raise = 1;
  bool changed = true;
  while (changed) {
    changed = false;
    for (Fun *f : fa->funs) {
      if (f->can_raise) continue;
      Vec<Fun *> callees;
      f->calls_funs(callees);
      for (Fun *callee : callees.values())
        if (callee && callee->can_raise) {
          f->can_raise = 1;
          changed = true;
          break;
        }
    }
  }
}

// issue 069 (per-program unroll): tuple __eq__/__lt__ are unrolled
// constant-index / len-guarded folds; the unroll count must cover the
// program's largest tuple. Scan all module ASTs for the max (fold-aware)
// tuple arity, GENERATE the two methods at exactly that arity, parse them,
// and append them to the builtin `tuple` class -- so the unroll fits the
// program instead of a fixed bound (removing the cap and the verbosity).

// Fixed-arity tuple value: a tuple literal (its element count) or a `+` of
// two fixed-arity tuples (literal-only concat folding, cf.
// try_fold_tuple_arity in build_if1). -1 if not a fixed-arity tuple.
static int estimate_tuple_arity(PyDAST *n) {
  if (!n) return -1;
  if (n->kind == PY_tuple) return n->children.n;
  if (n->kind == PY_binop && n->op == PY_OP_ADD && n->children.n == 2) {
    int l = estimate_tuple_arity(n->children[0]), r = estimate_tuple_arity(n->children[1]);
    if (l >= 0 && r >= 0) return l + r;
  }
  return -1;
}
static void scan_max_tuple_arity(PyDAST *n, int &mx) {
  if (!n) return;
  int a = estimate_tuple_arity(n);
  if (a > mx) mx = a;
  for (PyDAST *c : n->children) scan_max_tuple_arity(c, mx);
}

// min_arity: a floor for the unroll count. The REPL can't pre-scan future
// interactive input, so it passes a generous floor; the batch path passes 0
// and gets the exact program max.
void inject_tuple_compare(Vec<PycModule *> &mods, int min_arity) {
  int max_arity = min_arity;
  for (PycModule *m : mods) scan_max_tuple_arity(m->pymod, max_arity);
  // Generate the two methods at exactly max_arity, wrapped in a throwaway
  // class so the parser yields funcdef nodes (which we move onto `tuple`).
  char *buf = nullptr;
  size_t sz = 0;
  FILE *f = open_memstream(&buf, &sz);
  fputs("class __pyc_tuple_cmp__:\n", f);
  fputs("  def __eq__(self, t):\n", f);
  fputs("    n = len(self)\n", f);
  fputs("    if n != len(t): return False\n", f);
  for (int i = 0; i < max_arity; i++)
    fprintf(f, "    if n >= %d and not (self[%d] == t[%d]): return False\n", i + 1, i, i);
  fputs("    return True\n", f);
  fputs("  def __lt__(self, t):\n", f);
  fputs("    n = len(self)\n", f);
  fputs("    m = len(t)\n", f);
  for (int i = 0; i < max_arity; i++) {
    fprintf(f, "    if n >= %d and m >= %d:\n", i + 1, i + 1);
    fprintf(f, "      if self[%d] < t[%d]: return True\n", i, i);
    fprintf(f, "      if t[%d] < self[%d]: return False\n", i, i);
  }
  fputs("    return n < m\n", f);
  fclose(f);
  PyDAST *gen = dparse_python_buf_to_ast("<tuple_cmp>", buf, (int)sz);
  free(buf);
  if (!gen) return;
  PyDAST *gcls = nullptr;
  for (PyDAST *c : gen->children)
    if (c->kind == PY_classdef) {
      gcls = c;
      break;
    }
  if (!gcls || !gcls->children.n) return;
  PyDAST *gbody = gcls->children.last();
  // Append the generated funcdefs to the builtin `tuple` class body.
  for (PyDAST *c : mods[0]->pymod->children) {
    if (c->kind != PY_classdef || !c->children.n) continue;
    PyDAST *nm = c->children[0];
    if (!nm || !nm->str_val || strcmp(nm->str_val, "tuple")) continue;
    PyDAST *body = c->children.last();
    for (PyDAST *meth : gbody->children)
      if (meth->kind == PY_funcdef) body->children.add(meth);
    break;
  }
}

int ast_to_if1(Vec<PycModule *> &mods) {
  inject_tuple_compare(mods, 0);  // issue 069: program-sized tuple __eq__/__lt__
  // For the non-REPL path: build baseline for mods[0] (builtin), then extend.
  // The builtin_mods Vec is local; ctx->modules is updated to &mods by extend.
  Vec<PycModule *> builtin_mods;
  builtin_mods.add(mods[0]);
  BaselineIF1State bl = ast_to_if1_baseline(builtin_mods);
  return ast_to_if1_extend(mods, bl);
}

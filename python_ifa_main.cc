// SPDX-License-Identifier: BSD-3-Clause
#include "python_ifa_int.h"
#include "python_parse.h"

#include "codegen/codegen_common.h"

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
  scope_sym(ctx, sym_bytes);
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
  if (name && !strcmp(name, "__pyc_sleep__")) {
    fprintf(fp, "co_await _CG_Await_Sleep{(double)%s};\n", n->rvals[5]->cg_string);
    return;
  }
  // issue 077 (extended by 096): __pyc_c_call__(ret_type, name, type1,
  // arg1, type2, arg2, ...) declares each argument's expected type
  // explicitly (the Vars at rvals[4], rvals[6], ... -- the odd
  // offsets from 5 below are the actual argument VALUES). Dispatch
  // that picked this __pyc_c_call__-based dunder body (e.g.
  // str.__eq__) only checked the RECEIVER's type; nothing has ever
  // verified the OTHER arguments still match what's declared here. A
  // salvage-degraded operand reaching such a call (issue 076's
  // mechanism, or any similar imprecision) can let them diverge --
  // printing the mismatched argument verbatim produces C the call was
  // never built to accept (e.g. _CG_str_eq(t2, <int64 value>), or
  // msp_ss.py's issue 096 repro: _CG_fopen(t2, <_CG_any value>)).
  //
  // Deliberately scoped to a whitelist of specific target names, NOT
  // applied to every __pyc_c_call__ site. Discovered the hard way
  // (three rounds of false positives against the full corpus, ~200
  // failures down to 14, before 077 found this): some __pyc_c_call__
  // sites declare a type that DOESN'T match what's actually passed,
  // on purpose, because the underlying C macro does its own internal
  // conversion -- e.g. __pyc__/04_sequence.py's `list.__add__`
  // declares `int, l` for `l`, a whole LIST, because `_CG_list_add`'s
  // macro (pyc_c_runtime.h) runs each side through `_CG_to_list(...)`
  // regardless of the nominal declared type. A per-argument type
  // check can't distinguish "this declared type is a real constraint"
  // from "this declared type is a placeholder the macro will
  // reinterpret" without per-call-site knowledge nothing in the IR
  // currently carries -- so this only runs for names individually
  // confirmed, by inspection of pyc_c_runtime.h, to take their
  // declared types literally. Each addition below issue 077's
  // original str-comparison family was verified this way for 096:
  // `_CG_fopen`/`_CG_str_to_int64_base`/`_CG_ord` take a real `char*`
  // path/string argument (no macro-level reinterpretation); `_CG_chr`
  // takes a real `int`; `_CG_strcat` (bytes.__add__/__radd__,
  // 01b_bytes.py) takes two real `bytes` operands, unlike
  // `_CG_list_add`'s deliberately-erased second argument.
  bool strict_c_call = name && (!strcmp(name, "_CG_str_eq") || !strcmp(name, "_CG_str_ne") ||
                                 !strcmp(name, "_CG_str_lt") || !strcmp(name, "_CG_str_le") ||
                                 !strcmp(name, "_CG_str_gt") || !strcmp(name, "_CG_str_ge") ||
                                 !strcmp(name, "_CG_fopen") || !strcmp(name, "_CG_chr") ||
                                 !strcmp(name, "_CG_ord") || !strcmp(name, "_CG_str_to_int64_base") ||
                                 !strcmp(name, "_CG_strcat"));
  // The mismatch judgment itself (unalias_type() for Type_ALIAS
  // declared types like `int`; tolerating any two numeric types
  // regardless of width/precision; requiring exact cg_string
  // agreement -- and thus correctly flagging, not exempting -- a
  // pointer-representable actual like `_CG_any` reaching a call that
  // declares a real pointer type -- see sudoku2.py's str.__ne__
  // history, ifa/issues/closed/077) lives in
  // c_call_arg_type_mismatch() (ifa/if1/sym.{h,cc}) so cg_emit_llvm.cc's
  // LLVM-backend counterpart (issue 096 design point 4) shares it
  // rather than re-deriving the same two false-positive fixes.
  for (int i = 5; strict_c_call && i < n->rvals.n; i += 2) {
    bool mismatch = c_call_arg_type_mismatch(n->rvals[i - 1]->sym, n->rvals[i]->type);
    if (mismatch) {
      if (!fruntime_errors) fail("argument type mismatch at C call '%s'", name ? name : "?");
      // write_c_prim's P_prim_primitive case (this function's only
      // caller) already wrote the "lval = " prefix -- or, if the
      // result is dead, just "  " -- before invoking this cgfn, so
      // this has to complete a valid C EXPRESSION, not a standalone
      // statement. The comma operator lets the always-firing assert
      // run first; the trailing dummy value is never reached (assert
      // aborts) and only needs to typecheck against the destination.
      if (n->lvals.n && n->lvals[0]->cg_string)
        fprintf(fp, "(assert(!\"runtime error: C call argument type mismatch\"), (%s)0);\n",
                n->lvals[0]->type ? n->lvals[0]->type->cg_string : "int");
      else
        fputs("(assert(!\"runtime error: C call argument type mismatch\"), 0);\n", fp);
      return;
    }
  }
  // issues/114: cast the result to the DECLARED type. __pyc_c_call__'s
  // first argument says what the call yields as far as FA is
  // concerned, and that no longer always matches the C function's own
  // return type -- _CG_generator_value returns a machine word while
  // the declared type is now whatever the generator yields. The word
  // IS the pointer. A no-op when the two already agree.
  if (n->lvals.n && n->lvals[0]->cg_string && n->lvals[0]->type && n->lvals[0]->type->cg_string)
    fprintf(fp, "(%s)", n->lvals[0]->type->cg_string);
  fputs(name, fp);
  fputs("(", fp);
  int first = 1;
  // issues/114: for pyc's own generator entry points, cast each
  // int-declared argument to `long long`. Their handle argument is a
  // machine word but now arrives typed `{None, <yielded>}`, and
  // passing that verbatim is a C++ overload-resolution failure.
  //
  // Scoped to `_CG_generator_*` deliberately: a blanket argument cast
  // is NOT safe, because some sites declare a type that does not match
  // what they pass on purpose -- list.__add__ declares `int` for a
  // whole LIST because _CG_list_add runs each side through
  // _CG_to_list, and casting there would destroy the pointer.
  bool cast_args = name && !strncmp(name, "_CG_generator_", 14);
  for (int i = 5; i < n->rvals.n; i += 2) {
    if (!first) {
      fputs(", ", fp);
    } else
      first = 0;
    if (cast_args && n->rvals[i - 1] && n->rvals[i - 1]->sym && n->rvals[i - 1]->sym->name &&
        !strcmp(n->rvals[i - 1]->sym->name, "int"))
      fputs("(long long)", fp);
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

// issues/040: a %d/%i/%o/%x/%X/%u/%c spec receiving a float argument
// (or %f/%e/%g/%E/%G receiving an int argument) is undefined behavior
// in C -- vsnprintf's va_arg(int)/va_arg(double) reads the wrong
// register class (x86-64 SysV: float/double varargs pass via XMM,
// while an integer specifier's va_arg reads the general-purpose/stack
// save area) -- garbage, not a truncated or promoted value, unlike
// Python's own %d/%f which happily accept either. Only fixable when
// the format string is a compile-time constant we can parse here
// (matches this same primitive's existing %s-stringification scope
// boundary, python_ifa_build_if1.cc's __mod__ handling -- non-constant
// formats keep the old, unchecked behavior; per-argument types, by
// contrast, are only known this late, post-FA, not at that frontend
// site).
static void collect_format_convs(cchar *fmt, Vec<char> &convs) {
  for (cchar *p = fmt; *p; p++) {
    if (*p != '%') continue;
    p++;
    if (*p == '%') continue;
    while (*p && (strchr("-+ #0", *p) || (*p >= '0' && *p <= '9') || *p == '.')) p++;
    if (*p) convs.add(*p);
  }
}

// issues/165: a Python `%d` is rewritten to `%lld` by
// _CG_widen_int_convs (pyc_c_runtime.h), because the value it reads is
// int64 -- so the emitter's job is to make sure it REALLY is. That was
// only half true before: an int-typed argument was passed at its own
// width, which for `_CG_bool` (uint8) or `_CG_int` (32-bit) is narrower
// than the conversion now reads.
//
// `%c` is the exception: C's `%c` consumes an `int`, and the rewrite
// leaves it alone, so cast to `(int)` and not `(int64)`.
//
// The pre-existing issues/040 case -- a float reaching an integer
// conversion, or an int reaching a float one -- is the same fix and is
// kept: on x86-64 SysV the two land in different register classes, so a
// mismatch is not a truncation but garbage.
static bool fmt_arg_is_numeric(Sym *t) {
  if (!t) return false;
  return t->num_kind == IF1_NUM_KIND_INT || t->num_kind == IF1_NUM_KIND_UINT ||
         t->num_kind == IF1_NUM_KIND_FLOAT || t == sym_bool;
}

// Emit the cast that makes `t` match `conv`. conv == 0 means the format
// string is not a compile-time constant, so the conversion is unknown:
// widen an integer to int64 anyway (that is what the rewrite will read)
// and leave a float alone, since nothing can be inferred for it.
static void format_string_emit_cast(FILE *fp, Sym *t, char conv) {
  if (!fmt_arg_is_numeric(t)) return;
  bool is_float = t->num_kind == IF1_NUM_KIND_FLOAT;
  if (!conv) {
    if (!is_float) fputs("(int64)", fp);
  } else if (strchr("diouxX", conv)) {
    fputs("(int64)", fp);
  } else if (conv == 'c') {
    fputs("(int)", fp);
  } else if (strchr("feEgGF", conv) && !is_float) {
    fputs("(double)", fp);
  }
}

static void format_string_emit_arg(FILE *fp, Var *av, char conv) {
  fputs(", ", fp);
  format_string_emit_cast(fp, av->type, conv);
  fputs(av->cg_string, fp);
}

// issues/165: one tag per argument for the non-constant-format path --
// 'i' integer (passed as int64), 'f' float (passed as double), 's'
// anything else (passed as a pointer). The runtime pairs these with the
// conversions it finds; see _CG_format_string_tagged in pyc_c_runtime.h
// for why the repair cannot happen at either end alone.
static char format_string_tag(Sym *t) {
  if (!fmt_arg_is_numeric(t)) return 's';
  // 'b' is 'i' everywhere except `%s`: CPython's `"%d" % True` is "1" but
  // `"%s" % True` is "True". With a constant format the frontend's __str__
  // pre-conversion carries that distinction; the tag has to carry it here.
  if (t == sym_bool) return 'b';
  return t->num_kind == IF1_NUM_KIND_FLOAT ? 'f' : 'i';
}

static void format_string_codegen(FILE *fp, PNode *n, Fun *f) {
  Var *v = n->rvals[3];
  cchar *fmt = n->rvals[2]->sym->constant;
  Vec<char> convs;
  if (fmt) collect_format_convs(fmt, convs);
  bool is_tuple = v->type && v->type->type_kind == Type_RECORD && !cg_has_classtag(v->type);
  // issues/165: with a NON-constant format there are no conversions to
  // match arguments against, so the casts below cannot be chosen -- an
  // integer got widened and a float was left as a double, which then met
  // `%d` in the wrong register class. Hand the argument TYPES to the
  // runtime instead and let it pair them with the format it can see.
  if (!fmt) {
    fputs("_CG_format_string_tagged(", fp);
    fputs(n->rvals[2]->cg_string, fp);
    fputs(", \"", fp);
    if (is_tuple)
      for (int i = 0; i < v->type->has.n; i++) fputc(format_string_tag(v->type->has[i]->type), fp);
    else
      fputc(format_string_tag(v->type), fp);
    fputs("\"", fp);
    if (is_tuple) {
      for (int i = 0; i < v->type->has.n; i++) {
        fputs(", ", fp);
        if (format_string_tag(v->type->has[i]->type) == 'i') fputs("(int64)", fp);
        fprintf(fp, "%s->e%d", v->cg_string, i);
      }
    } else {
      fputs(", ", fp);
      if (format_string_tag(v->type) == 'i') fputs("(int64)", fp);
      fputs(v->cg_string, fp);
    }
    fputs(");\n", fp);
    return;
  }
  fputs("_CG_format_string(", fp);
  fputs(n->rvals[2]->cg_string, fp);
  if (is_tuple) {
    for (int i = 0; i < v->type->has.n; i++) {
      fputs(", ", fp);
      format_string_emit_cast(fp, v->type->has[i]->type, i < convs.n ? convs[i] : 0);
      fprintf(fp, "%s->e%d", v->cg_string, i);
    }
  } else if (convs.n == 1) {
    format_string_emit_arg(fp, v, convs[0]);
  } else {
    format_string_emit_arg(fp, v, 0);
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
  prim_reg(cannonicalize_string("__pyc_sleep__"), return_nil_transfer_function)->is_visible = 1;
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
  // issues/011/049: Sym::direct_raise is only set once build_if1 has
  // walked a function's body (PY_raise_stmt), so this has to run after
  // the call just above, not alongside compute_can_raise near the top
  // of this function. See user_code_reaches_raise's own comment
  // (python_ifa_build_syms.cc) for why.
  collect_builtin_raise_names(builtin_mods[0]->pymod, *ctx);
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
    // issues/107: after the whole module is walked, any name still
    // unresolved was never bound anywhere -- forward references have had
    // their chance by now.
    if (report_import_errors(*ctx) > 0) return -1;
    if (report_undefined_names(*ctx) > 0) return -1;
  }
  // issue 011: user-level call graph (the builtin module's own was
  // already computed once in ast_to_if1_baseline). all_mods.n here
  // (not the n_user snapshot) so imports build_syms pulled in mid-loop
  // are included -- their build_syms already ran, inline, above.
  {
    Vec<PycModule *> user_mods;
    for (int i = 1; i < all_mods.n; i++) user_mods.add(all_mods[i]);
    compute_can_raise(user_mods, *ctx);
    // issues/011/049: the 5 AST-shape-specific arms in build_syms_pyda
    // (raise/assert/yield family) miss an ordinary call into a
    // builtin method that raises (str.index(), issues/037) -- see
    // user_code_reaches_raise's own comment (python_ifa_build_syms.cc)
    // for the full story.
    if (user_code_reaches_raise(user_mods, *ctx)) pyc_program_has_raise = true;
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

// ROADMAP 6.1: a `*args` parameter holds a tuple no literal in the source
// spells -- its arity is a CALL's surplus argument count. When the program
// defines any `*args` function, a call's argument count bounds the tuples
// it can create, so the unroll must cover the largest call too; otherwise
// `f(1, "two", 3.0)` printed its 3-tuple through the runtime-index tail and
// mixed its slot types. Programs without `*args` are unaffected.
static void scan_star_args(PyDAST *n, bool &has_star, int &max_call_args) {
  if (!n) return;
  if (n->kind == PY_star_arg) has_star = true;
  if (n->kind == PY_arglist && n->children.n > max_call_args) max_call_args = n->children.n;
  for (PyDAST *c : n->children) scan_star_args(c, has_star, max_call_args);
}

// min_arity: a floor for the unroll count. The REPL can't pre-scan future
// interactive input, so it passes a generous floor; the batch path passes 0
// and gets the exact program max.
void inject_tuple_methods(Vec<PycModule *> &mods, int min_arity) {
  int max_arity = min_arity;
  for (PycModule *m : mods) scan_max_tuple_arity(m->pymod, max_arity);
  {
    bool has_star = false;
    int max_call_args = 0;
    for (PycModule *m : mods) scan_star_args(m->pymod, has_star, max_call_args);
    if (has_star && max_call_args > max_arity) max_arity = max_call_args;
  }
  // Generate the two methods at exactly max_arity, wrapped in a throwaway
  // class so the parser yields funcdef nodes (which we move onto `tuple`).
  char *buf = nullptr;
  size_t sz = 0;
  FILE *f = open_memstream(&buf, &sz);
  // issues/110: the unroll count comes from scanning tuple LITERALS, so a
  // tuple built by `tuple(iterable)` (make_seq -- runtime arity, no
  // PY_tuple node anywhere) contributes nothing to max_arity. The unrolled
  // body then compared only the first max_arity elements and returned
  // True for everything past it: rubik2's id_() -> tuple(state[20:32])
  // deduped 15 distinct 12-tuples down to 3 in a set (silently -- wrong
  // answers, no diagnostic), and with NO tuple literal in the program at
  // all max_arity is 0, making every same-length tuple compare equal.
  //
  // So each method gets a runtime tail past the unrolled prefix. For a
  // RECORD tuple `n` is a compile-time constant <= max_arity, so `n >
  // max_arity` folds to False and the tail is dead -- the same folding the
  // `n >= k` guards already rely on, which is what keeps the non-constant
  // `self[i]` out of a record contour where it would be invalid. For a
  // LIST-layout tuple `n` is a runtime value and the tail does the work.
  //
  // Every guard on `n` / `m` is the operator PRIMITIVE, inline, not
  // `n >= k` (a call to int.__ge__). The fold has to happen in THIS
  // contour, where `n` is already a constant per receiver arity. Through
  // int.__ge__ it needed that callee split per constant, and since
  // ifa/151 only a demand splits it: sudoku5 compares 2-tuples and
  // 3-tuples, the shared __ge__ answered `bool` for both, every
  // `self[2]` on a 2-tuple and the runtime tail went live, and the
  // unions they produced kept TYPE_CONFLUENCE acting on all 50 passes,
  // so CONST_DEMAND -- the rung that could have split __ge__ -- never
  // ran (ifa/157's cascade). A fold is not a demand; don't make one
  // depend on a split.
  fputs("class __pyc_tuple_cmp__:\n", f);
  fputs("  def __eq__(self, t):\n", f);
  // ifa/issues/090 repro 2: the operand may be None -- `move = None`
  // then `move in [(1,2), ...]`, where list.__contains__ compares each
  // element against it. `len(t)` below is then len(None), which
  // resolves to nothing, and the whole comparison degrades: sunfish
  // line 448 reports `unresolved call '__not__'` and the loop body
  // never runs. CPython says a tuple is never equal to a non-tuple, so
  // answer False directly. A None-typed operand cannot be handled by
  // giving __pyc_None_type__ a __len__ -- 00_runtime.py documents why
  // container stubs there inject None into element types program-wide.
  fputs("    if t is None: return False\n", f);
  fputs("    n = len(self)\n", f);
  fputs("    if __pyc_operator__(n, __pyc_symbol__(\"!=\"), len(t)): return False\n", f);
  for (int i = 0; i < max_arity; i++)
    fprintf(f, "    if __pyc_operator__(n, __pyc_symbol__(\">=\"), %d) and not (self[%d] == t[%d]): return False\n", i + 1, i, i);
  fprintf(f, "    if __pyc_operator__(n, __pyc_symbol__(\">\"), %d):\n", max_arity);
  fprintf(f, "      for i in range(%d, n):\n", max_arity);
  fputs("        if not (self[i] == t[i]): return False\n", f);
  fputs("    return True\n", f);
  fputs("  def __lt__(self, t):\n", f);
  fputs("    if t is None: return False\n", f);
  fputs("    n = len(self)\n", f);
  fputs("    m = len(t)\n", f);
  for (int i = 0; i < max_arity; i++) {
    fprintf(f, "    if __pyc_operator__(n, __pyc_symbol__(\">=\"), %d) and __pyc_operator__(m, __pyc_symbol__(\">=\"), %d):\n", i + 1, i + 1);
    fprintf(f, "      if self[%d] < t[%d]: return True\n", i, i);
    fprintf(f, "      if t[%d] < self[%d]: return False\n", i, i);
  }
  fprintf(f, "    if __pyc_operator__(n, __pyc_symbol__(\">\"), %d) and __pyc_operator__(m, __pyc_symbol__(\">\"), %d):\n", max_arity, max_arity);
  fprintf(f, "      for i in range(%d, n):\n", max_arity);
  fputs("        if i >= m: return False\n", f);
  fputs("        if self[i] < t[i]: return True\n", f);
  fputs("        if t[i] < self[i]: return False\n", f);
  fputs("    return n < m\n", f);
  // issues/119: __str__ and __hash__ need the same unroll, for the same
  // reason. Both dispatch a method ON AN ELEMENT (self[k].__repr__() /
  // self[k].__hash__()); with a loop index the element type is the union
  // of every field type, and for a HETEROGENEOUS tuple that union has no
  // single resolution. `print((1, (2, 3)))` compiled with zero
  // diagnostics and then aborted with `matching function not found` on
  // the C backend, and silently printed `(, )` on the LLVM one.
  // __hash__ carried a comment claiming the index loop was safe here
  // because the result type is int either way -- but it is the DISPATCH
  // that fails, not the result type, so `hash((1, (2, 3)))` aborted too.
  // A CONSTANT index makes self[k] one field, so each dispatch resolves.
  // Homogeneous tuples never hit this, which is why `print((1, 2))` and
  // `print(((1, 2), (3, 4)))` were fine and hid the bug.
  fputs("  def __str__(self):\n", f);
  fputs("    n = len(self)\n", f);
  fputs("    x = \"(\"\n", f);
  for (int i = 0; i < max_arity; i++) {
    fprintf(f, "    if __pyc_operator__(n, __pyc_symbol__(\">=\"), %d):\n", i + 1);
    if (i) fputs("      x += \", \"\n", f);
    fprintf(f, "      x += self[%d].__repr__()\n", i);
  }
  fprintf(f, "    if __pyc_operator__(n, __pyc_symbol__(\">\"), %d):\n", max_arity);
  fprintf(f, "      for i in range(%d, n):\n", max_arity);
  fputs("        if i: x += \", \"\n", f);
  fputs("        x += self[i].__repr__()\n", f);
  fputs("    if __pyc_operator__(n, __pyc_symbol__(\"==\"), 1): x += \",\"\n", f);
  fputs("    x += \")\"\n", f);
  fputs("    return x\n", f);
  fputs("  def __hash__(self):\n", f);
  fputs("    h = 0\n", f);
  fputs("    n = len(self)\n", f);
  for (int i = 0; i < max_arity; i++)
    fprintf(f, "    if __pyc_operator__(n, __pyc_symbol__(\">=\"), %d): h = h * 1000003 + self[%d].__hash__()\n", i + 1, i);
  fprintf(f, "    if __pyc_operator__(n, __pyc_symbol__(\">\"), %d):\n", max_arity);
  fprintf(f, "      for i in range(%d, n):\n", max_arity);
  fputs("        h = h * 1000003 + self[i].__hash__()\n", f);
  fputs("    return h\n", f);
  // `in`, count and index compare each element by `==`, so they need the
  // same unroll: with a loop index, self[i] on a heterogeneous record tuple
  // is not even a legal read (`"a" in (1, "a")` was "illegal primitive
  // argument type 'key'"), and with a constant index each `==` is one
  // field's own. Cross-type `==` answers False, as in CPython.
  fputs("  def __contains__(self, item):\n", f);
  fputs("    n = len(self)\n", f);
  for (int i = 0; i < max_arity; i++)
    fprintf(f, "    if __pyc_operator__(n, __pyc_symbol__(\">=\"), %d) and self[%d] == item: return True\n", i + 1, i);
  fprintf(f, "    if __pyc_operator__(n, __pyc_symbol__(\">\"), %d):\n", max_arity);
  fprintf(f, "      for i in range(%d, n):\n", max_arity);
  fputs("        if self[i] == item: return True\n", f);
  fputs("    return False\n", f);
  fputs("  def count(self, x):\n", f);
  fputs("    c = 0\n", f);
  fputs("    n = len(self)\n", f);
  for (int i = 0; i < max_arity; i++)
    fprintf(f, "    if __pyc_operator__(n, __pyc_symbol__(\">=\"), %d) and self[%d] == x: c += 1\n", i + 1, i);
  fprintf(f, "    if __pyc_operator__(n, __pyc_symbol__(\">\"), %d):\n", max_arity);
  fprintf(f, "      for i in range(%d, n):\n", max_arity);
  fputs("        if self[i] == x: c += 1\n", f);
  fputs("    return c\n", f);
  // CPython's tuple.index: start/stop normalized like a slice (as
  // list.index in 04_sequence.py), ValueError when absent.
  fputs("  def index(self, x, start=0, stop=None):\n", f);
  fputs("    n = len(self)\n", f);
  fputs("    b = start\n", f);
  fputs("    if b < 0:\n", f);
  fputs("      b += n\n", f);
  fputs("      if b < 0: b = 0\n", f);
  fputs("    e = n\n", f);
  fputs("    if stop is not None:\n", f);
  fputs("      e = stop\n", f);
  fputs("      if e < 0: e += n\n", f);
  fputs("      if e > n: e = n\n", f);
  for (int i = 0; i < max_arity; i++)
    fprintf(f, "    if __pyc_operator__(n, __pyc_symbol__(\">=\"), %d) and b <= %d and %d < e and self[%d] == x: return %d\n",
            i + 1, i, i, i, i);
  fprintf(f, "    if __pyc_operator__(n, __pyc_symbol__(\">\"), %d):\n", max_arity);
  fprintf(f, "      for i in range(%d, n):\n", max_arity);
  fputs("        if b <= i and i < e and self[i] == x: return i\n", f);
  fputs("    raise ValueError(\"tuple.index(x): x not in tuple\")\n", f);
  // issues/110: an element-recursive __deepcopy__. The any-type fallback
  // it replaces was a SHALLOW copy: `deepcopy((T(),))` shared the T with
  // the original.
  //
  // The result is CONSTRUCTED, never copied and then overwritten. A
  // copy-then-overwrite leaves every field typed as the union of the
  // original element and its deep copy, and for an element that is itself
  // a tuple those are two record CreationSets with no common C type
  // (measured: `deepcopy(((T(), 1), 2))` emitted a `_CG_void` field and
  // segfaulted). So: one literal per arity, unrolled like __str__ so each
  // element keeps its own type -- on a RECORD tuple `n` is a constant and
  // only its own arity's branch is live -- and, for a runtime-length tuple
  // (list layout; `n` is not a constant), make_seq over the copied
  // elements, the same construction tuple.__add__ uses.
  fputs("  def __deepcopy__(self):\n", f);
  fputs("    n = len(self)\n", f);
  fputs("    if __pyc_operator__(n, __pyc_symbol__(\"==\"), 0): return self\n", f);
  for (int k = 1; k <= max_arity; k++) {
    fprintf(f, "    if __pyc_operator__(n, __pyc_symbol__(\"==\"), %d): return (", k);
    for (int i = 0; i < k; i++) fprintf(f, "%sself[%d].__deepcopy__()", i ? ", " : "", i);
    fputs(k == 1 ? ",)\n" : ")\n", f);
  }
  fputs("    r = []\n", f);
  fputs("    for i in range(n):\n", f);
  fputs("      r.append(self[i].__deepcopy__())\n", f);
  fputs("    return __pyc_primitive__(__pyc_symbol__(\"make_seq\"), tuple, r)\n", f);
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

// issues/171 #13, issues/007: `@property` getters. A descriptor is
// TYPE-DIRECTED -- `o.NAME` calls the getter when o's class defines the
// property and reads a field otherwise (voronoi2 has both for one name) --
// so the choice is left to ordinary method dispatch:
//
//  - each `@property def NAME(self)` is renamed `__pyc_get_NAME__` (its
//    decorator dropped), so the class has an accessor METHOD;
//  - every builtin class gets a default `__pyc_get_NAME__` that reads the
//    field (`return self.NAME`); user classes inherit it through `object`;
//  - build_if1 lowers every attribute READ of NAME to
//    `o.__pyc_get_NAME__()` (pyc_is_property_name).
//
// Only the names are global; which accessor runs is decided per receiver
// class by dispatch, like any method. Runs before the builtin module is
// built, which is why it is a pre-scan over the program's modules (as
// inject_tuple_methods is). A property in a module only reached through an
// import is not seen here and is refused by build_syms. Setters are refused.
static Vec<cchar *> pyc_property_names;

bool pyc_is_property_name(cchar *name) {
  if (!name) return false;
  for (cchar *p : pyc_property_names)
    if (!strcmp(p, name)) return true;
  return false;
}

static cchar *decorated_single_decorator_name(PyDAST *d) {
  // The one PY_decorator of a PY_decorated node, if there is exactly one.
  if (!d || d->kind != PY_decorated || d->children.n != 2) return nullptr;
  PyDAST *dec = d->children[0];
  if (dec->kind == PY_suite) {
    if (dec->children.n != 1) return nullptr;
    dec = dec->children[0];
  }
  if (dec->kind != PY_decorator || dec->children.n != 1) return nullptr;
  return dec->children[0]->str_val;
}

static bool is_property_decorated_def(PyDAST *d) {
  cchar *nm = decorated_single_decorator_name(d);
  return nm && decorator_name_is(nm, "property") && d->children.last()->kind == PY_funcdef;
}

static void scan_properties(PyDAST *n, cchar *filename) {
  if (!n) return;
  if (n->kind == PY_classdef && n->children.n) {
    PyDAST *body = n->children.last();
    Vec<PyDAST *> *stmts = body->kind == PY_suite ? &body->children : nullptr;
    auto visit = [&](PyDAST *&stmt) {
      if (!is_property_decorated_def(stmt)) return;
      PyDAST *def = stmt->children.last();
      cchar *pname = def->children[0]->str_val;
      if (!pyc_is_property_name(pname)) pyc_property_names.add(dupstr(pname));
      char buf[512];
      snprintf(buf, sizeof(buf), "__pyc_get_%s__", pname);
      def->children[0]->str_val = dupstr(buf);
      stmt = def;  // the decorator is consumed
    };
    if (stmts)
      for (int i = 0; i < stmts->n; i++) visit(stmts->v[i]);
    else
      visit(n->children.v[n->children.n - 1]);
  }
  for (PyDAST *c : n->children) scan_properties(c, filename);
}

// A `@NAME.setter` / `@NAME.deleter` needs a store/delete dispatch that does
// not exist; refuse it rather than leave the decorator unapplied.
static void refuse_property_setters(PyDAST *n) {
  if (!n) return;
  if (n->kind == PY_decorator && n->children.n && n->children[0]->str_val) {
    cchar *s = n->children[0]->str_val;
    cchar *dot = strrchr(s, '.');
    if (dot && (decorator_name_is(dot + 1, "setter") || decorator_name_is(dot + 1, "deleter")))
      fail("error line %d: property %s (@%s) are not supported; only read-only @property getters are "
           "(issues/007)", n->line, decorator_name_is(dot + 1, "setter") ? "setters" : "deleters", s);
  }
  for (PyDAST *c : n->children) refuse_property_setters(c);
}

void inject_property_accessors(Vec<PycModule *> &mods) {
  for (int i = 1; i < mods.n; i++) {
    refuse_property_setters(mods[i]->pymod);
    scan_properties(mods[i]->pymod, mods[i]->filename);
  }
  if (!pyc_property_names.n) return;
  // One throwaway class of default accessors per builtin class, parsed at
  // once; each builtin class takes its own copy's funcdefs.
  Vec<PyDAST *> targets;
  for (PyDAST *c : mods[0]->pymod->children)
    if (c->kind == PY_classdef && c->children.n) targets.add(c);
  char *buf = nullptr;
  size_t sz = 0;
  FILE *f = open_memstream(&buf, &sz);
  for (int k = 0; k < targets.n; k++) {
    fprintf(f, "class __pyc_prop_acc_%d__:\n", k);
    for (cchar *pname : pyc_property_names)
      fprintf(f, "  def __pyc_get_%s__(self):\n    return self.%s\n", pname, pname);
  }
  fclose(f);
  PyDAST *gen = dparse_python_buf_to_ast("<property_accessors>", buf, (int)sz);
  free(buf);
  if (!gen) fail("internal error: property accessor generation failed to parse");
  int k = 0;
  for (PyDAST *gcls : gen->children) {
    if (gcls->kind != PY_classdef || k >= targets.n) continue;
    PyDAST *body = targets[k++]->children.last();
    for (PyDAST *meth : gcls->children.last()->children)
      if (meth->kind == PY_funcdef) body->children.add(meth);
  }
}

int ast_to_if1(Vec<PycModule *> &mods) {
  inject_property_accessors(mods);  // issues/171 #13: @property getters
  inject_tuple_methods(mods, 0);  // issue 069: program-sized tuple __eq__/__lt__
  // For the non-REPL path: build baseline for mods[0] (builtin), then extend.
  // The builtin_mods Vec is local; ctx->modules is updated to &mods by extend.
  Vec<PycModule *> builtin_mods;
  builtin_mods.add(mods[0]);
  BaselineIF1State bl = ast_to_if1_baseline(builtin_mods);
  return ast_to_if1_extend(mods, bl);
}

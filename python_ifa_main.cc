// SPDX-License-Identifier: BSD-3-Clause
#include "python_ifa_int.h"
#include "python_parse.h"
#include <set>
#include <string>
#include <vector>

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

// The __pyc_c_call__ targets whose declared argument types are real
// constraints, not placeholders a C macro reinterprets (see the
// whitelist rationale in c_call_codegen). cg_emit_llvm.cc keeps its own
// copy of this list.
static bool is_strict_c_call(cchar *name) {
  return name && (!strcmp(name, "_CG_str_eq") || !strcmp(name, "_CG_str_ne") || !strcmp(name, "_CG_str_lt") ||
                  !strcmp(name, "_CG_str_le") || !strcmp(name, "_CG_str_gt") || !strcmp(name, "_CG_str_ge") ||
                  !strcmp(name, "_CG_fopen") || !strcmp(name, "_CG_chr") || !strcmp(name, "_CG_ord") ||
                  !strcmp(name, "_CG_str_to_int64_base") || !strcmp(name, "_CG_strcat"));
}

// A strict target's arguments, checked in FA against their declared
// types. Codegen used to be the only check, and under the default
// permissive mode it emitted a runtime assert with no diagnostic at all:
// pygasus's `ord(f.read(1))` passed `bytes` to `_CG_ord`'s `str`, compiled
// silently, and aborted at startup. Recorded here, the mismatch is an
// ordinary type violation -- fatal (ifa/158), reported at the call with
// its call chain. The codegen checks stay as a backstop.
//
// The rule is c_call_arg_type_mismatch's, on FA's per-CreationSet types
// rather than codegen's C representations: any two numeric types agree;
// a numeric and a non-numeric one never do; two non-numeric types must
// be the same type. `None` is skipped -- a `{None, str}` argument is a
// pointer, and a None reaching the call is ifa/164's concern.
static void c_call_check_function(PNode *pn, EntrySet *es) {
  if (!is_strict_c_call(pn->rvals[3]->sym->constant)) return;
  for (int i = 5; i < pn->rvals.n; i += 2) {
    Sym *declared = unalias_type(pn->rvals[i - 1]->sym);
    if (!declared) continue;
    AVar *arg = make_AVar(pn->rvals[i], es);
    Vec<CreationSet *> bad;
    for (CreationSet *cs : arg->out->sorted) {
      if (!cs || !cs->sym) continue;
      Sym *t = cs->sym->type;
      if (!t || t == declared || t == sym_nil_type) continue;
      if (declared->num_kind && t->num_kind) continue;
      bad.add(cs);
    }
    if (bad.n) type_violation(ATypeViolation_kind::PRIMITIVE_ARGUMENT, arg, make_AType(bad), make_AVar(pn->lvals[0], es));
  }
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
  bool strict_c_call = is_strict_c_call(name);
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
  //
  // A BACKSTOP since c_call_check_function: FA now records these
  // mismatches as type violations, which are fatal (ifa/158), so a
  // program reaching here means FA saw a type codegen does not -- a
  // `_CG_any` actual, or a representation the per-CreationSet rule
  // considers equal.
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
  c_call_prim->check_fn = c_call_check_function;
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

static Vec<cchar *> *make_search_path() {
  Vec<cchar *> *search_path = new Vec<cchar *>;
  char f[PATH_MAX];
  char *here = dupstr(getcwd(f, PATH_MAX));
  search_path->add(here);
  // pyc's own standard-library shims (math, ...) live under
  // <system_dir>/pyc_lib, alongside the __pyc__ builtin module. Put
  // them on the module search path so `import math` resolves to the
  // shim (issue 025 bucket C). The cwd is searched first, so a user
  // module can still shadow a shim with its own file.
  search_path->add(dupstrs((cchar *)system_dir, "/pyc_lib"));
  const char *pythonpath_env = getenv("PYTHONPATH");
  if (!pythonpath_env) return search_path;
  char *path = (char *)pythonpath_env;
  while (1) {
    char *p = path;
    char *e = strchr(p, ':'), *ee = e;
    while (e > p && e[-1] == '/') e--;
    p = dupstr(p, e);
    if (file_exists(p)) {
      search_path->add(p);
      add_subdirs(p, *search_path);
    }
    if (!ee) break;
    path = ee + 1;
  }
  return search_path;
}

static void build_search_path(PycCompiler &ctx) { ctx.search_path = make_search_path(); }


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
static void scan_tuple_arities(PyDAST *n, std::set<int> &as) {
  if (!n) return;
  int a = estimate_tuple_arity(n);
  if (a >= 0) as.insert(a);
  for (PyDAST *c : n->children) scan_tuple_arities(c, as);
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

// ifa/185: the dispatch constraints of the generated tuple methods'
// formals, keyed on each parameter's PyDAST (pyc_formal_dispatch).
static Map<PyDAST *, PycFormalDispatch *> formal_dispatch;

PycFormalDispatch *pyc_formal_dispatch(PyDAST *param) { return formal_dispatch.get(param); }

// One generated def's constraints, one entry per positional parameter.
typedef std::vector<PycFormalDispatch> TupleDefSpec;

// ifa/185: every element is read with the `index_object` primitive and a
// LITERAL index, not `self[k]`. `self[k]` is a call to tuple.__getitem__,
// which is that same primitive behind `__pyc_clone_constants__(key)`, so
// each read minted a __getitem__ contour per constant index per receiver
// group (3,066 of 4,125 EntrySets on sunfish at pass 10 under the unroll).
// Measured with arity dispatch: sunfish compiles in 134 s this way, 168 s
// through __getitem__. A runtime index (`self[i]`) still goes through
// __getitem__.
//
// ifa/185: tuple.__eq__ as overloads on the operands' ARITY, written to `f`
// as a class body with one TupleDefSpec per def in `specs`. "k" is a static
// arity, "dyn" a tuple with none (list layout, from tuple(iterable) or a
// slice). Subsumption picks the most specific, so:
//
//   (self k,   t tuple k)    element by element, straight line;
//   (self k,   t tuple dyn)  length check, then the same;
//   (self dyn, t tuple)      `t == self`: a record `t` goes to (k, dyn), so
//                            it is never indexed at a runtime position,
//                            which would demote it to list layout;
//   (self dyn, t tuple dyn)  the runtime loop;
//   (self,     t)            False: different static arities, None
//                            (ifa/090 repro 2: `move in [(1, 2), ...]`
//                            with move None), or not a tuple at all --
//                            CPython never equates a tuple with a list.
//
// Bodies are emitted for every arity 0..max_arity; one that no CreationSet
// reaches is never analysed.
static void emit_tuple_arity_eq(FILE *f, int max_arity, std::vector<TupleDefSpec> &specs) {
  const PycFormalDispatch none = {-1, false}, any_tuple = {-1, true}, dyn_self = {DISPATCH_ARITY_DYNAMIC, false},
                          dyn_tuple = {DISPATCH_ARITY_DYNAMIC, true};
  fputs("  def __eq__(self, t):\n    return False\n", f);
  specs.push_back({none, none});
  fputs("  def __eq__(self, t):\n    return t == self\n", f);
  specs.push_back({dyn_self, any_tuple});
  fputs("  def __eq__(self, t):\n", f);
  fputs("    n = len(self)\n", f);
  fputs("    if n != len(t): return False\n", f);
  fputs("    for i in range(n):\n", f);
  fputs("      if not (self[i] == t[i]): return False\n", f);
  fputs("    return True\n", f);
  specs.push_back({dyn_self, dyn_tuple});
  for (int k = 0; k <= max_arity; k++) {
    PycFormalDispatch self_k = {k, false}, tuple_k = {k, true};
    for (int dyn = 0; dyn < 2; dyn++) {
      fputs("  def __eq__(self, t):\n", f);
      if (dyn) fprintf(f, "    if len(t) != %d: return False\n", k);
      for (int i = 0; i < k; i++) fprintf(f, "    if not (__pyc_primitive__(__pyc_symbol__(\"index_object\"), self, %d) == __pyc_primitive__(__pyc_symbol__(\"index_object\"), t, %d)): return False\n", i, i);
      fputs("    return True\n", f);
      specs.push_back({self_k, dyn ? dyn_tuple : tuple_k});
    }
  }
}

// ifa/185: the other tuple methods, dispatched on arity like __eq__.
// `arities` is every arity a fixed-arity tuple can have (see
// inject_tuple_methods_over). Each method gets one straight-line body per
// arity, so every index is a constant in range, plus an UNCONSTRAINED body:
// a runtime loop, for a tuple with no fixed arity (list layout, which a
// runtime index can read) and for any arity the scan missed.
//
// Why straight-line at all (issues/119): __str__, __hash__ and the `==`
// in __contains__/count/index dispatch a method ON AN ELEMENT, and with a
// loop index the element of a HETEROGENEOUS record is the union of its
// fields, which has no single resolution (`print((1, (2, 3)))` aborted).
// A constant index is one field. Before arity dispatch the constant
// indices came from an unroll to max_arity behind `n >= k` guards, which
// did not fold once one contour held several arities.
static void emit_tuple_arity_methods(FILE *f, std::vector<int> &arities, std::vector<TupleDefSpec> &specs) {
  const PycFormalDispatch none = {-1, false}, dyn_self = {DISPATCH_ARITY_DYNAMIC, false},
                          dyn_tuple = {DISPATCH_ARITY_DYNAMIC, true};
  auto self_k = [](int k) { return PycFormalDispatch{k, false}; };
  auto tuple_k = [](int k) { return PycFormalDispatch{k, true}; };

  // __lt__: lexicographic, as CPython defines it -- the first unequal pair
  // decides, otherwise the shorter tuple is smaller. Both operands'
  // arities are dispatched, so a pair of fixed arities compares exactly
  // min(k, m) elements. A list-layout side is read at runtime positions.
  // `t is None` answers False, as the unrolled method did.
  for (int k : arities)
    for (int m : arities) {
      fputs("  def __lt__(self, t):\n", f);
      for (int i = 0; i < k && i < m; i++) {
        fprintf(f, "    if __pyc_primitive__(__pyc_symbol__(\"index_object\"), self, %d) < __pyc_primitive__(__pyc_symbol__(\"index_object\"), t, %d): return True\n", i, i);
        fprintf(f, "    if __pyc_primitive__(__pyc_symbol__(\"index_object\"), t, %d) < __pyc_primitive__(__pyc_symbol__(\"index_object\"), self, %d): return False\n", i, i);
      }
      fprintf(f, "    return %s\n", k < m ? "True" : "False");
      specs.push_back({self_k(k), tuple_k(m)});
    }
  for (int k : arities) {
    fputs("  def __lt__(self, t):\n    m = len(t)\n", f);
    for (int i = 0; i < k; i++) {
      fprintf(f, "    if %d >= m: return False\n", i);
      fprintf(f, "    if __pyc_primitive__(__pyc_symbol__(\"index_object\"), self, %d) < __pyc_primitive__(__pyc_symbol__(\"index_object\"), t, %d): return True\n", i, i);
      fprintf(f, "    if __pyc_primitive__(__pyc_symbol__(\"index_object\"), t, %d) < __pyc_primitive__(__pyc_symbol__(\"index_object\"), self, %d): return False\n", i, i);
    }
    fprintf(f, "    return %d < m\n", k);
    specs.push_back({self_k(k), dyn_tuple});
    fputs("  def __lt__(self, t):\n    n = len(self)\n", f);
    for (int i = 0; i < k; i++) {
      fprintf(f, "    if %d >= n: return True\n", i);
      fprintf(f, "    if __pyc_primitive__(__pyc_symbol__(\"index_object\"), self, %d) < __pyc_primitive__(__pyc_symbol__(\"index_object\"), t, %d): return True\n", i, i);
      fprintf(f, "    if __pyc_primitive__(__pyc_symbol__(\"index_object\"), t, %d) < __pyc_primitive__(__pyc_symbol__(\"index_object\"), self, %d): return False\n", i, i);
    }
    fprintf(f, "    return n < %d\n", k);
    specs.push_back({dyn_self, tuple_k(k)});
  }
  fputs("  def __lt__(self, t):\n", f);
  fputs("    if t is None: return False\n", f);
  fputs("    n = len(self)\n", f);
  fputs("    m = len(t)\n", f);
  fputs("    for i in range(n):\n", f);
  fputs("      if i >= m: return False\n", f);
  fputs("      if self[i] < t[i]: return True\n", f);
  fputs("      if t[i] < self[i]: return False\n", f);
  fputs("    return n < m\n", f);
  specs.push_back({none, none});

  // One-operand methods: `self` alone is dispatched. `per` is the
  // statement at index %d (printf'd with the index as every argument).
  struct OneOp {
    const char *sig;   // "def ...(...):"
    int nparams;       // positional parameters, self first
    const char *pre;   // before the elements; %d is the arity
    const char *per;   // per element; every %d is the index
    const char *post;  // after the elements; %d is the arity
    const char *loop;  // the whole unconstrained body
  };
  static const OneOp ops[] = {
      {"__hash__(self)", 1, "    h = 0\n", "    h = h * 1000003 + __pyc_primitive__(__pyc_symbol__(\"index_object\"), self, %d).__hash__()\n", "    return h\n",
       "    h = 0\n    for i in range(len(self)):\n      h = h * 1000003 + self[i].__hash__()\n    return h\n"},
      // `in`, count and index compare each element by `==`; cross-type
      // `==` answers False, as in CPython.
      {"__contains__(self, item)", 2, "", "    if __pyc_primitive__(__pyc_symbol__(\"index_object\"), self, %d) == item: return True\n", "    return False\n",
       "    for i in range(len(self)):\n      if self[i] == item: return True\n    return False\n"},
      {"count(self, x)", 2, "    c = 0\n", "    if __pyc_primitive__(__pyc_symbol__(\"index_object\"), self, %d) == x: c += 1\n", "    return c\n",
       "    c = 0\n    for i in range(len(self)):\n      if self[i] == x: c += 1\n    return c\n"},
      // The tuple's elements as a list of int / of __pyc_bytes_fmtarg__,
      // read at constant indices so each conversion is one field's own:
      // pyc_lib/struct.py's pack (minpng's (bool, int, int), ifa/134) and
      // bytes %-formatting (minilight's (bytes, bytes, int, int)).
      {"__pyc_bytes_fmtargs__(self)", 1, "    r = []\n", "    r.append(__pyc_primitive__(__pyc_symbol__(\"index_object\"), self, %d).__pyc_bytes_fmtarg__())\n",
       "    return r\n",
       "    r = []\n    for i in range(len(self)):\n      r.append(self[i].__pyc_bytes_fmtarg__())\n    return r\n"},
      {"__pyc_toints__(self)", 1, "    r = []\n", "    r.append(int(__pyc_primitive__(__pyc_symbol__(\"index_object\"), self, %d)))\n", "    return r\n",
       "    r = []\n    for i in range(len(self)):\n      r.append(int(self[i]))\n    return r\n"},
  };
  auto spec_for = [&](PycFormalDispatch s0, int nparams) {
    TupleDefSpec sp(nparams, none);
    sp[0] = s0;
    return sp;
  };
  for (const OneOp &op : ops) {
    for (int k : arities) {
      fprintf(f, "  def %s:\n", op.sig);
      fprintf(f, op.pre, k);
      for (int i = 0; i < k; i++) fprintf(f, op.per, i, i, i, i);
      fprintf(f, op.post, k);
      specs.push_back(spec_for(self_k(k), op.nparams));
    }
    fprintf(f, "  def %s:\n%s", op.sig, op.loop);
    specs.push_back(spec_for(none, op.nparams));
  }

  // __str__: CPython's repr, `(1,)` for one element.
  for (int k : arities) {
    fputs("  def __str__(self):\n", f);
    if (!k) {
      fputs("    return \"()\"\n", f);
    } else {
      fputs("    x = \"(\"\n", f);
      for (int i = 0; i < k; i++) {
        if (i) fputs("    x += \", \"\n", f);
        fprintf(f, "    x += __pyc_primitive__(__pyc_symbol__(\"index_object\"), self, %d).__repr__()\n", i);
      }
      fprintf(f, "    x += \"%s)\"\n", k == 1 ? "," : "");
      fputs("    return x\n", f);
    }
    specs.push_back(spec_for(self_k(k), 1));
  }
  fputs("  def __str__(self):\n", f);
  fputs("    n = len(self)\n", f);
  fputs("    x = \"(\"\n", f);
  fputs("    for i in range(n):\n", f);
  fputs("      if i: x += \", \"\n", f);
  fputs("      x += self[i].__repr__()\n", f);
  fputs("    if n == 1: x += \",\"\n", f);
  fputs("    x += \")\"\n", f);
  fputs("    return x\n", f);
  specs.push_back(spec_for(none, 1));

  // CPython's tuple.index: start/stop normalized like a slice (as list.index
  // in 04_sequence.py), ValueError when absent.
  auto index_prologue = [&](const char *n) {
    fprintf(f, "    n = %s\n", n);
    fputs("    b = start\n", f);
    fputs("    if b < 0:\n", f);
    fputs("      b += n\n", f);
    fputs("      if b < 0: b = 0\n", f);
    fputs("    e = n\n", f);
    fputs("    if stop is not None:\n", f);
    fputs("      e = stop\n", f);
    fputs("      if e < 0: e += n\n", f);
    fputs("      if e > n: e = n\n", f);
  };
  for (int k : arities) {
    fputs("  def index(self, x, start=0, stop=None):\n", f);
    char nk[16];
    snprintf(nk, sizeof(nk), "%d", k);
    index_prologue(nk);
    for (int i = 0; i < k; i++) fprintf(f, "    if b <= %d and %d < e and __pyc_primitive__(__pyc_symbol__(\"index_object\"), self, %d) == x: return %d\n", i, i, i, i);
    fputs("    raise ValueError(\"tuple.index(x): x not in tuple\")\n", f);
    specs.push_back(spec_for(self_k(k), 4));
  }
  fputs("  def index(self, x, start=0, stop=None):\n", f);
  index_prologue("len(self)");
  fputs("    for i in range(n):\n", f);
  fputs("      if b <= i and i < e and self[i] == x: return i\n", f);
  fputs("    raise ValueError(\"tuple.index(x): x not in tuple\")\n", f);
  specs.push_back(spec_for(none, 4));

  // issues/110: an element-recursive __deepcopy__, CONSTRUCTED rather than
  // copied and overwritten: copy-then-overwrite leaves each field the union
  // of the original element and its copy, and for a tuple element those are
  // two record CreationSets with no common C type (`deepcopy(((T(), 1), 2))`
  // segfaulted). One literal per arity; make_seq for a list-layout tuple,
  // the construction tuple.__add__ uses.
  for (int k : arities) {
    fputs("  def __deepcopy__(self):\n", f);
    if (!k) {
      fputs("    return self\n", f);
    } else {
      fputs("    return (", f);
      for (int i = 0; i < k; i++) fprintf(f, "%s__pyc_primitive__(__pyc_symbol__(\"index_object\"), self, %d).__deepcopy__()", i ? ", " : "", i);
      fputs(k == 1 ? ",)\n" : ")\n", f);
    }
    specs.push_back(spec_for(self_k(k), 1));
  }
  fputs("  def __deepcopy__(self):\n", f);
  fputs("    n = len(self)\n", f);
  fputs("    if n == 0: return self\n", f);
  fputs("    r = []\n", f);
  fputs("    for i in range(n):\n", f);
  fputs("      r.append(self[i].__deepcopy__())\n", f);
  fputs("    return __pyc_primitive__(__pyc_symbol__(\"make_seq\"), tuple, r)\n", f);
  specs.push_back(spec_for(none, 1));
}

// Attach `specs` to the funcdefs of the generated class body `gbody`, in
// order. Only plain positional parameters are generated, so each is the
// varargslist child itself.
static void apply_tuple_def_specs(PyDAST *gbody, std::vector<TupleDefSpec> &specs) {
  size_t i = 0;
  for (PyDAST *meth : gbody->children) {
    if (meth->kind != PY_funcdef) continue;
    assert(i < specs.size());
    TupleDefSpec &spec = specs[i++];
    PyDAST *params = meth->children[1];
    PyDAST *varargsl = params->children.n ? params->children[0] : nullptr;
    assert(varargsl && varargsl->children.n == (int)spec.size());
    for (int j = 0; j < varargsl->children.n; j++)
      if (spec[j].arity != -1 || spec[j].class_typed)
        formal_dispatch.put(varargsl->children[j], new PycFormalDispatch(spec[j]));
  }
  assert(i == specs.size());
}

// min_arity: a floor for the unroll count. The REPL can't pre-scan future
// interactive input, so it passes a generous floor; the batch path passes 0
// and gets the exact program max.
//
// The scan covers every module the program imports, not just `mods`:
// imports are resolved inside build_syms, after this has shaped the
// builtin module, so without prescan_imported_modules a tuple built in
// an imported module -- or a `*args` function defined in one, like
// pyc_lib/struct.py's pack -- was missed, and a heterogeneous one was
// refused through the runtime-index tail.
// `asts` is every module to scan: `mods` plus what they import.
static void inject_tuple_methods_over(Vec<PycModule *> &mods, Vec<PyDAST *> &asts, int min_arity) {
  int max_arity = min_arity;
  for (PyDAST *a : asts) scan_max_tuple_arity(a, max_arity);
  // ifa/185: the arities a fixed-arity tuple can have: every literal's
  // (builtin modules included), a `*args` tuple's (any call's argument
  // count), and the REPL's floor. A tuple of any other arity -- none can be
  // built, but a missed construct would be one -- still reaches each
  // method's unconstrained runtime-loop body, so the set bounds precision,
  // not correctness. (__eq__ alone is emitted for all of 0..max_arity: its
  // unconstrained body answers False.)
  std::set<int> arity_set;
  for (int k = 0; k <= min_arity; k++) arity_set.insert(k);
  for (PyDAST *a : asts) scan_tuple_arities(a, arity_set);
  {
    bool has_star = false;
    int max_call_args = 0;
    for (PyDAST *a : asts) scan_star_args(a, has_star, max_call_args);
    if (has_star && max_call_args > max_arity) max_arity = max_call_args;
    if (has_star)
      for (int k = 0; k <= max_call_args; k++) arity_set.insert(k);
  }
  arity_set.insert(max_arity);
  std::vector<int> arities(arity_set.begin(), arity_set.end());
  // The generated methods, wrapped in a throwaway class so the parser
  // yields funcdef nodes (which we move onto `tuple`).
  char *buf = nullptr;
  size_t sz = 0;
  FILE *f = open_memstream(&buf, &sz);
  // ifa/185: every method dispatches on ARITY (Sym::dispatch_arity) rather
  // than unrolling to max_arity behind `n >= k` guards; see
  // emit_tuple_arity_methods. One throwaway class, whose funcdefs are moved
  // onto `tuple` with their specs.
  std::vector<TupleDefSpec> arity_specs;
  fputs("class __pyc_tuple_arity__:\n", f);
  emit_tuple_arity_eq(f, max_arity, arity_specs);
  emit_tuple_arity_methods(f, arities, arity_specs);
  fclose(f);
  std::string gen_src(buf, sz);
  free(buf);
  PyDAST *gen = dparse_python_buf_to_ast("<tuple_cmp>", gen_src.c_str(), (int)gen_src.size());
  if (!gen) return;
  PyDAST *gcls = nullptr;
  for (PyDAST *c : gen->children)
    if (c->kind == PY_classdef && c->children.n) gcls = c;
  if (!gcls) return;
  apply_tuple_def_specs(gcls->children.last(), arity_specs);
  // Append the generated funcdefs to the builtin `tuple` class body.
  for (PyDAST *c : mods[0]->pymod->children) {
    if (c->kind != PY_classdef || !c->children.n) continue;
    PyDAST *nm = c->children[0];
    if (!nm || !nm->str_val || strcmp(nm->str_val, "tuple")) continue;
    PyDAST *body = c->children.last();
    for (PyDAST *meth : gcls->children.last()->children)
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
// inject_tuple_methods is), including every module reached by import
// (prescan_imported_modules). Setters are refused.
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

// `imported` is the pre-parsed ASTs of every module `mods` imports
// (prescan_imported_modules). import_file builds exactly these ASTs, so
// the getters renamed here are the ones build_syms later sees.
static void inject_property_accessors(Vec<PycModule *> &mods, Vec<PyDAST *> &imported) {
  for (int i = 1; i < mods.n; i++) {
    refuse_property_setters(mods[i]->pymod);
    scan_properties(mods[i]->pymod, mods[i]->filename);
  }
  for (PyDAST *a : imported) {
    refuse_property_setters(a);
    scan_properties(a, nullptr);
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

void inject_tuple_methods(Vec<PycModule *> &mods, int min_arity) {
  Vec<PyDAST *> asts;
  for (PycModule *m : mods) asts.add(m->pymod);
  prescan_imported_modules(mods, *make_search_path(), asts);
  inject_tuple_methods_over(mods, asts, min_arity);
}

int ast_to_if1(Vec<PycModule *> &mods) {
  // Both injections shape the builtin module from the WHOLE program, so
  // both need the imported modules, which build_syms only loads later.
  // Pre-scanned once: import_file reuses these ASTs.
  Vec<PyDAST *> imported;
  prescan_imported_modules(mods, *make_search_path(), imported);
  inject_property_accessors(mods, imported);  // issues/171 #13: @property getters
  Vec<PyDAST *> asts;
  for (PycModule *m : mods) asts.add(m->pymod);
  for (PyDAST *a : imported) asts.add(a);
  inject_tuple_methods_over(mods, asts, 0);  // issue 069: program-sized tuple __eq__/__lt__
  // For the non-REPL path: build baseline for mods[0] (builtin), then extend.
  // The builtin_mods Vec is local; ctx->modules is updated to &mods by extend.
  Vec<PycModule *> builtin_mods;
  builtin_mods.add(mods[0]);
  BaselineIF1State bl = ast_to_if1_baseline(builtin_mods);
  return ast_to_if1_extend(mods, bl);
}

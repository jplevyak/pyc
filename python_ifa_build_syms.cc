// SPDX-License-Identifier: BSD-3-Clause
#include "python_ifa_int.h"
#include "python_parse.h"


// Comparator for sorting PycSymbol* by sym->name (the visible
// attribute name).  Used to deterministically order class
// attributes before adding them to `cls_sym->has`.  Without
// this sort, struct field order is non-deterministic across
// runs — the underlying `Map<cchar*, PycSymbol*>` hashes on
// key-string POINTER values, which vary with GC/heap layout
// between processes (see issue 016 follow-up).  Each compile
// stays internally consistent, but pyc's generated .c / .ll
// differs across runs of byte-identical input, breaking build
// reproducibility and any future golden-file diff on emitted
// code.
static int compar_pycsymbol_by_name(const void *ai, const void *aj) {
  const Sym *a = (*(PycSymbol *const *)ai)->sym;
  const Sym *b = (*(PycSymbol *const *)aj)->sym;
  if (a->name && b->name) return strcmp(a->name, b->name);
  if (!a->name && !b->name) return (a->id > b->id) - (a->id < b->id);
  return a->name ? 1 : -1;       // unnamed sorts first
}

// ---- Shared utility functions ----

static void import_file(cchar *name, cchar *f, PycCompiler &ctx);

// issues/113: resolve a possibly-DOTTED module name to a file under the
// search-path root `p`. `a.b.c` is `<p>/a/b/c.py` if that is a module,
// or `<p>/a/b/c/__init__.py` if it is a package. Returns null if neither
// exists, so the caller can try the next root.
//
// Before this, resolution was `dupstrs(p, "/", mod, ".py")` -- the
// module name pasted in verbatim -- so `com.github.tarsa.tarsalzp.Main`
// looked for a file literally named `com.github.tarsa.tarsalzp.Main.py`
// and a package directory was never considered at all.
static cchar *module_file(cchar *p, cchar *mod) {
  int n = strlen(mod);
  char *rel = (char *)GC_malloc(n + 1);
  for (int i = 0; i < n; i++) rel[i] = mod[i] == '.' ? '/' : mod[i];
  rel[n] = 0;
  cchar *f = dupstrs(p, "/", rel, ".py");
  if (is_regular_file(f)) return f;
  f = dupstrs(p, "/", rel, "/__init__.py");
  if (is_regular_file(f)) return f;
  return nullptr;
}

// Search every root for `mod`, importing it on the first hit. Roots that
// are themselves packages are skipped -- a package's interior is reached
// through its own dotted name, never as a root.
static PycModule *find_and_import(cchar *mod, PycCompiler &ctx) {
  if (PycModule *m = get_module(mod, ctx)) return m;
  for (auto p : ctx.search_path->values()) {
    if (file_exists(p, "/__init__.py")) continue;
    cchar *f = module_file(p, mod);
    if (!f) continue;
    import_file(mod, f, ctx);
    break;
  }
  return get_module(mod, ctx);
}

static void import_file(cchar *name, cchar *f, PycCompiler &ctx) {
  PycModule *m = new PycModule(f);
  // The registry keys on `name`, and PycModule derives it from the
  // BASENAME -- which for `ml/entry.py` is `entry`, and for any
  // package's `__init__.py` is `__init__`. Both collide across
  // packages, so record the dotted name instead.
  m->name = cannonicalize_string(name);
  int flen = strlen(f);
  m->is_package = flen >= 12 && !strcmp(f + flen - 12, "/__init__.py");
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

// issues/113: PEP 328. `mod` may start with dots -- `.camera`,
// `..base`, or bare `.` / `..`. Resolve against the importing module's
// package: one dot means that package, each extra dot strips a level.
// A module with no dots is already absolute and passes through.
cchar *resolve_relative_module(cchar *mod, PycCompiler &ctx) {
  if (!mod || mod[0] != '.') return mod;
  int dots = 0;
  while (mod[dots] == '.') dots++;
  cchar *rest = mod + dots;
  // The importing module's package: itself if it IS a package, else its
  // parent. `ctx.mod` is null only for the top-level script, which has
  // no package and so cannot host a relative import.
  cchar *base = (ctx.mod && ctx.mod->name) ? ctx.mod->name : "";
  char *b = dupstr(base);
  if (ctx.mod && !ctx.mod->is_package) {
    char *d = strrchr(b, '.');
    if (d) *d = 0; else b[0] = 0;
  }
  for (int i = 1; i < dots; i++) {
    char *d = strrchr(b, '.');
    if (d) *d = 0; else { b[0] = 0; break; }
  }
  if (!*rest) return b[0] ? (cchar *)b : mod;
  return b[0] ? dupstrs(b, ".", rest) : rest;
}

static void rtrim_str(char *s) {
  if (!s) return;
  int len = strlen(s);
  while (len > 0 && isspace((unsigned char)s[len-1])) s[--len] = 0;
}

// True for a real PycSymbol pointer (not one of the scope sentinels
// GLOBAL_USE/NONLOCAL_USE/GLOBAL_DEF/NONLOCAL_DEF, which are small
// integers cast to PycSymbol*).
static inline bool is_real_pycsymbol(PycSymbol *s) { return (intptr_t)s > (intptr_t)NONLOCAL_DEF; }

static void build_import_syms(char *sym, char *as, char *from, PycCompiler &ctx) {
  rtrim_str(sym); rtrim_str(from);
  char *mod = (char *)(from ? resolve_relative_module(from, ctx) : (cchar *)sym);
  if (!strcmp(mod, "pyc_compat")) return;
  PycModule *m = find_and_import(mod, ctx);
  // Plain dotted import with no matching file for the full dotted
  // name (e.g. `import os.path`): pyc has no real package hierarchy,
  // so fall back to the top-level component. This matches CPython's
  // own binding behavior for this form (only the top package name is
  // bound) and lets pyc_lib modules that already expose a submodule
  // as a plain attribute (os.py's `path = _os_path()`) work as-is.
  char *bind_mod = mod;
  if (!m && !from) {
    char *dot = strchr(mod, '.');
    if (dot) {
      char *top = (char *)GC_malloc(dot - mod + 1);
      memcpy(top, mod, dot - mod);
      top[dot - mod] = 0;
      m = find_and_import(top, ctx);
      if (m) bind_mod = top;
    }
  }
  if (!m) return;  // module not found; build_import_if1 emits the diagnostic
  // `from X import Y [as Z]`: bind the module's symbol Y into the
  // importing scope under Z (or Y). The module's symbols were fully
  // built by import_file (or a prior import), so its top scope is
  // saved under m->pymod. Without this the imported name has no
  // binding and every use fails as "'Y' has no type" (issue 025
  // bucket C -- the module-import subsystem). `import X` (no `from`)
  // is the module-object case, handled separately.
  if (from) {
    PycScope *modscope = m->ctx->saved_scopes.get(m->pymod);
    if (modscope) {
      if (!sym) {
        // `from X import *`: bind every public (non-underscore) top-level
        // name of X directly into the importing scope. pyc has no __all__
        // handling; the CPython default (skip names starting with '_')
        // is used instead.
        form_Map(MapCharPycSymbolElem, x, modscope->map) {
          if (x->key[0] == '_') continue;
          if (!is_real_pycsymbol(x->value)) continue;
          ctx.scope_stack.last()->map.put(x->key, x->value);
        }
      } else {
        PycSymbol *y = modscope->map.get(cannonicalize_string(sym));
        if (is_real_pycsymbol(y))
          ctx.scope_stack.last()->map.put(cannonicalize_string(as ? as : sym), y);
        else if (PycModule *sm = find_and_import(dupstrs(mod, ".", sym), ctx)) {
          // issues/113: `from PKG import NAME` where NAME is a SUBMODULE
          // rather than one of the package's own names -- minilight's
          // `from ml import entry`, quameon's `from stats import
          // average`. CPython imports PKG.NAME and binds the module
          // object; do the same, with the module-marker symbol `import X`
          // already uses (modules are compile-time namespaces here, so
          // `entry.main(...)` resolves at build_if1 time via PY_power).
          cchar *bind = cannonicalize_string(as ? as : sym);
          PycSymbol *marker = new_PycSymbol(bind, ctx);
          marker->sym->is_module = 1;
          marker->sym->nesting_depth = 0;
          ctx.module_syms.put(marker->sym, sm);
          ctx.scope_stack.last()->map.put(bind, marker);
        }
      }
    }
  } else {
    // `import X [as Z]`: bind Z (or X) to a module-marker symbol.
    // Modules are compile-time namespaces here, not runtime values,
    // so `X.attr` is resolved to the module member at build_if1 time
    // (see PY_power). The marker itself never flows as a value.
    cchar *bind = cannonicalize_string(as ? as : bind_mod);
    PycSymbol *marker = new_PycSymbol(bind, ctx);
    marker->sym->is_module = 1;
    marker->sym->nesting_depth = 0;
    ctx.module_syms.put(marker->sym, m);
    ctx.scope_stack.last()->map.put(bind, marker);
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
  // issues/024: `a, *b = [1, 2, 3]` -- a starred sub-target still
  // binds a name (or attribute/subscript/nested-tuple target), just
  // to a list slice instead of a single element (see
  // emit_assign_to_target). Recurse into its own inner target the
  // same way every other wrapper kind here does.
  if (n->kind == PY_star_expr) { mark_store(n->children[0]); return; }
  if (n->kind == PY_fpdef || n->kind == PY_fplist || n->kind == PY_tuple || n->kind == PY_testlist ||
      n->kind == PY_exprlist)
    for (auto c : n->children.values()) mark_store(c);
}

// issues/025 (mwmatching): Python decides a name's scope over the WHOLE
// function -- a name assigned anywhere in a function body is local
// throughout, even at a point that READS it before that assignment.
// build_syms_pyda walks the body in source order, so a read-before-
// write (`if first or d < bd: ... bd = d`) resolves the read as a
// not-yet-bound USE, falls through to module scope, and mints a
// SPURIOUS module global -- which a second same-shaped function then
// collides with ("'bd' redefined as local"). The helpers below collect
// every bare-name local assignment target in a function body (BEFORE
// the body is walked) so those names can be pre-bound PYC_LOCAL and the
// reads resolve to the correct local.

// Collect bare NAME(s) bound by an assignment/for target expression.
// Attribute/subscript targets (`a.x`, `a[i]`, a PY_power) bind no local
// and are skipped, exactly as build_syms_pyda's own PY_name/PY_power
// handling distinguishes them.
static void collect_bind_names(PyDAST *t, Vec<cchar *> &out) {
  if (!t) return;
  if (t->kind == PY_name) {
    if (t->str_val) out.add(t->str_val);
    return;
  }
  if (t->kind == PY_star_expr) {
    if (t->children.n) collect_bind_names(t->children[0], out);
    return;
  }
  if (t->kind == PY_tuple || t->kind == PY_testlist || t->kind == PY_exprlist || t->kind == PY_list ||
      t->kind == PY_fplist || t->kind == PY_fpdef)
    for (auto c : t->children.values()) collect_bind_names(c, out);
}

// Walk a function body collecting local assignment-target names (into
// `out`) and names removed from local consideration by an explicit
// `global`/`nonlocal` (into `excluded`). Does NOT descend into nested
// funcdef/lambda/class bodies or comprehension for-clauses -- those are
// separate scopes -- and deliberately does NOT collect nested def/class
// NAMES (pre-binding those as plain locals would rob the def of its own
// function Sym).
static void collect_prebind_targets(PyDAST *n, Vec<cchar *> &out, Vec<cchar *> &excluded) {
  if (!n) return;
  switch (n->kind) {
    case PY_funcdef:
    case PY_lambda:
    case PY_classdef:
      return;  // separate scope
    case PY_global_stmt:
    case PY_nonlocal_stmt:
      for (auto c : n->children.values())
        if (c->str_val) excluded.add(c->str_val);
      return;
    case PY_assign:
      for (int i = 0; i < n->children.n - 1; i++) collect_bind_names(n->children[i], out);
      break;
    case PY_annassign:
    case PY_augassign:
      if (n->children.n) collect_bind_names(n->children[0], out);
      break;
    case PY_for_stmt:
      if (n->children.n) collect_bind_names(n->children[0], out);
      break;
    default:
      break;
  }
  for (auto c : n->children.values())
    if (c && c->kind != PY_comp_for && c->kind != PY_list_for) collect_prebind_targets(c, out, excluded);
}

// Pre-bind whole-function local targets. Runs after the parameters are
// bound and before the body is walked (PY_funcdef).
static void prebind_function_locals(PyDAST *body, PycCompiler &ctx) {
  Vec<cchar *> targets, excluded;
  collect_prebind_targets(body, targets, excluded);
  for (cchar *name : targets) {
    bool skip = false;
    for (cchar *e : excluded)
      if (e == name || (e && name && !strcmp(e, name))) {
        skip = true;
        break;
      }
    if (skip) continue;
    if (!ctx.scope_stack.last()->map.get(name))  // not already a param / earlier pre-bind
      make_PycSymbol(ctx, name, PYC_LOCAL);
  }
}

// Recursively mark every bare, non-wildcard NAME reachable through
// capture position in a match/case PATTERN as PY_STORE. Mirrors
// build_if1_pyda's build_pattern_match traversal exactly (wildcard/
// capture/or-pattern/sequence/literal) -- a capture pattern can
// appear nested inside a sequence pattern (`case [a, b]:` binds
// BOTH `a` and `b`), not just at the pattern's top level. Deliberately
// NOT reusing mark_store: it doesn't know about the wildcard `_`
// exclusion (a pattern's `_` must NOT become a binding, matching
// build_pattern_match treating it as a no-op), doesn't recurse into
// PY_list (only PY_tuple/testlist/exprlist/fpdef/fplist), and would
// happily (and wrongly, for a pattern) mark an or-pattern's or a
// literal pattern's non-name expression as if it were an lvalue.
static void mark_pattern_captures(PyDAST *n) {
  if (!n) return;
  if (n->kind == PY_name) {
    // `_` (wildcard) and `None`/`True`/`False` (PEP 634 singleton
    // patterns, matched by identity -- see build_pattern_match's
    // literal-pattern handling) are excluded: they parse as bare
    // PY_name too (ordinary global constants in this grammar, not
    // keywords), but must stay ordinary reads, not become a fresh
    // local binding.
    if (strcmp(n->str_val, "_") != 0 && strcmp(n->str_val, "None") != 0 && strcmp(n->str_val, "True") != 0 &&
        strcmp(n->str_val, "False") != 0)
      n->ctx = PY_STORE;
    return;
  }
  if (n->kind == PY_binop && n->op == PY_OP_BITOR) {
    // Or-pattern: build_pattern_match rejects a capture/wildcard
    // alternative outright (fail()), so there's nothing to mark --
    // but recurse anyway rather than assume, in case that
    // restriction is ever relaxed.
    mark_pattern_captures(n->children[0]);
    mark_pattern_captures(n->children[1]);
    return;
  }
  if (n->kind == PY_star_expr) {
    // issues/023: sequence pattern's star capture (`case [a, *rest]:`)
    // -- same PY_star_expr node issue 024's assignment-target star
    // reuses, restricted (by build_pattern_match) to a bare name or
    // `_`. Recurse into the inner name exactly like mark_store does
    // for the assignment-target case; the existing PY_name branch
    // above already excludes `_` correctly.
    mark_pattern_captures(n->children[0]);
    return;
  }
  if (n->kind == PY_list || n->kind == PY_tuple) {
    for (auto c : n->children.values()) mark_pattern_captures(c);
    return;
  }
  if (n->kind == PY_dict) {
    // Mapping pattern (`{"k": v, ...}`, python.g's flat PY_dict shape:
    // children alternate key/value). Only the VALUE side is a
    // sub-pattern that can bind -- the key side is an ordinary value
    // expression (a literal or a value pattern like `Color.RED`),
    // deliberately left untouched here so it falls through to the
    // generic recurse below as a normal read, same as any other
    // expression position.
    for (int i = 0; i + 1 < n->children.n; i += 2) mark_pattern_captures(n->children[i + 1]);
    // issues/023: `**rest` (a trailing PY_dstar_arg, odd child count
    // -- see python.g's dict_rest_arg) always binds a fresh capture,
    // same as sequence patterns' star capture. Unlike a star target,
    // `**rest` can never be `_` (build_pattern_match rejects it) --
    // recursing into mark_pattern_captures anyway rather than
    // special-casing, since its PY_name branch already handles both
    // `_` and real names correctly regardless.
    if (n->children.n % 2 == 1) mark_pattern_captures(n->children[n->children.n - 1]->children[0]);
    return;
  }
  if (n->kind == PY_power && n->children.n == 2 && n->children[1]->kind == PY_call) {
    // Class pattern (`ClassName(attr=pat, ...)` or, issues/023,
    // positional `ClassName(pat, ...)` resolved via __match_args__),
    // parsed as an ordinary constructor-call-shaped PY_power/PY_call
    // by build_pattern_match. Every sub-pattern binds -- keyword
    // values AND positional args alike (build_pattern_match's own arg
    // loop treats them identically once positional args are resolved
    // to an attribute name); only the class name and the keyword
    // names themselves (`attr` in `attr=pat`) are left untouched, same
    // rationale as PY_dict's keys above: they fall through to the
    // generic recurse as ordinary reads, exactly like an ordinary
    // call's keyword-argument names already do (ordinary `foo(x=1)`
    // calls resolve `x` the same harmless way -- see
    // tests/keyword_args.py).
    PyDAST *call = n->children[1];
    if (call->children.n > 0) {
      PyDAST *arglist = call->children[0];
      for (auto arg : arglist->children.values())
        mark_pattern_captures(arg->kind == PY_keyword_arg ? arg->children[1] : arg);
    }
    return;
  }
  // Literal pattern (number/string/etc.): nothing to bind.
}

// issues/014: a function is a generator iff its OWN body contains a
// `yield` statement/expression anywhere -- no separate keyword, unlike
// `async def`. Recurse through the funcdef's children (params, body,
// decorators) but stop at any nested function/class boundary: a yield
// inside a nested def/lambda/class belongs to THAT scope, not this one.
static bool pyda_contains_yield(PyDAST *n) {
  if (!n) return false;
  if (n->kind == PY_yield_stmt || n->kind == PY_yield_expr || n->kind == PY_yield_from_expr) return true;
  if (n->kind == PY_funcdef || n->kind == PY_lambda || n->kind == PY_classdef) return false;
  for (auto c : n->children.values())
    if (pyda_contains_yield(c)) return true;
  return false;
}

// issue 011: same shape as pyda_contains_yield, scanning for ANY
// `return` (bare or with a value -- both provide a def for fn->ret,
// see return_stmt's build_if1_pyda case) anywhere in this function's
// OWN body. Needed BEFORE the body is built (not discoverable
// mid-build the way PY_return_stmt's own `fun_returns_value = 1`
// side effect works): a `raise` textually BEFORE the function's only
// `return` -- the common early-exit-guard shape, `if bad: raise
// X(); return normal_value` -- must already know a later return
// exists so goto_exc_target doesn't manufacture a {result, nil}
// union fn->ret never actually has (found via risky()-shaped tests
// in tests/exception_basic.py/exception_propagation.py breaking when
// this was instead a build-order-dependent flag check).
static bool pyda_contains_return(PyDAST *n) {
  if (!n) return false;
  if (n->kind == PY_return_stmt) return true;
  if (n->kind == PY_funcdef || n->kind == PY_lambda || n->kind == PY_classdef) return false;
  for (auto c : n->children.values())
    if (pyda_contains_return(c)) return true;
  return false;
}

// issue 011 (per-callee can-raise gating): syntactic call-graph
// collector, run AFTER build_syms (so every name reference already
// has its resolved Sym cached in pydmap -- see getAST). Populates:
//   - raisers: functions that directly raise/assert, OR make a call
//     this pass can't resolve to a specific def (method dispatch, a
//     builtin intercept, a constructor, calling a variable) -- these
//     seed can_raise=true unconditionally, since an unresolved call
//     can't be disproven safe.
//   - call_edges: caller -> Vec of callees, for PLAIN calls only
//     (`foo(...)`, a bare name in USE context immediately followed
//     by a call trailer) resolved to another def's own Sym via
//     ctx.def_internal_fn (build_syms_pyda's PY_funcdef case: a
//     plain def's PUBLIC name Sym and its INTERNAL closure Sym are
//     different objects, linked only by this map -- see the PY_funcdef
//     comment there). Recursive self-calls resolve through the same
//     map (build_syms never does the recursion-specific swap
//     build_if1's PY_name case does; that only matters at build_if1
//     time).
// current_fn is null at module/class-body top level and inside
// lambdas (no raise/assert is possible in an expression-only lambda
// body; class bodies run at definition time, not as a call target) --
// their contents are still walked (for nested defs), just not
// attributed to any enclosing function.
static void collect_can_raise(PyDAST *n, PycCompiler &ctx, Sym *current_fn, Map<PyDAST *, bool> &resolved_calls,
                               Map<Sym *, Vec<Sym *> *> &call_edges, Vec<Sym *> &raisers) {
  if (!n) return;
  switch (n->kind) {
    case PY_funcdef: {
      Sym *fn_sym = getAST(n, ctx)->sym;
      for (auto c : n->children.values()) collect_can_raise(c, ctx, fn_sym, resolved_calls, call_edges, raisers);
      return;
    }
    case PY_lambda:
    case PY_classdef:
      for (auto c : n->children.values()) collect_can_raise(c, ctx, nullptr, resolved_calls, call_edges, raisers);
      return;
    case PY_raise_stmt:
    case PY_assert_stmt:
      if (current_fn) raisers.add(current_fn);
      break;
    case PY_power:
      if (n->children.n >= 2 && n->children[0]->kind == PY_name && n->children[0]->ctx != PY_STORE &&
          n->children[1]->kind == PY_call) {
        Sym *ref = getAST(n->children[0], ctx)->sym;
        Sym *callee = nullptr;
        if (ref) {
          Sym *ifn = ctx.def_internal_fn.get(ref);
          callee = ifn ? ifn : (ref->is_fun ? ref : nullptr);
        }
        resolved_calls.put(n->children[1], true);  // don't ALSO flag this node as unresolved below
        if (current_fn) {
          if (callee)
            map_set_add(call_edges, current_fn, callee);
          else
            raisers.add(current_fn);  // e.g. a constructor call: ref resolves to a class, not a def
        }
      }
      break;
    case PY_call:
      // Any call trailer not already claimed by the plain-call case
      // above -- a method call (obj.method(...)), a call on a
      // subscript/further-chained result (foo(...).bar(), foo[i]()),
      // or a call whose atom didn't resolve at all.
      if (current_fn && !resolved_calls.get(n)) raisers.add(current_fn);
      break;
    default:
      break;
  }
  for (auto c : n->children.values()) collect_can_raise(c, ctx, current_fn, resolved_calls, call_edges, raisers);
}

// issues/011/049: pyc_program_has_raise (python_ifa_build_if1.cc,
// gates whether emit_exc_check emits ANYTHING at all) is armed by
// build_syms_pyda at 5 specific user-code AST shapes (bare raise,
// assert, yield/yield-expr/yield-from) -- each deliberately re-arming
// the gate at the exact point user code becomes reachable to a
// BUILTIN raiser (__pyc_assert_fail__, generator StopIteration),
// since a builtin-module raise itself is excluded there (else it
// would permanently arm every program regardless of whether user code
// ever uses it -- see PY_raise_stmt's own comment in
// build_syms_pyda). An ORDINARY call from user code into a builtin
// method that raises -- e.g. str.index() (issues/037) -- was never
// given the same treatment: no AST shape special-cased "this call's
// target can raise". A program whose only reachable raise is through
// such a call never armed pyc_program_has_raise, so emit_exc_check
// skipped every exception check in the whole program -- including the
// user's own try/except around that exact call -- and the raise's
// own (correct in isolation) "leave fn->ret undefined on this path"
// behaviour (goto_exc_target) became a genuine uninitialized-memory
// read at runtime, silently, with zero compile warnings. Confirmed via
// direct reduction: `for d in "123456789": try: s.index(d) except
// ValueError: ...` at module level compiled clean and returned
// garbage instead of ever reaching the except clause.
//
// FIRST ATTEMPT (reverted): a scanner mirroring collect_can_raise's
// own conservative "unresolved call -> assume it can raise" rule,
// using Sym::can_raise (transitive) for resolved plain calls. Both
// halves turned out too broad: Sym::can_raise is TRANSITIVE (set the
// same way for "this function directly raises" and "this function
// calls something unresolved, e.g. a polymorphic __repr__ dispatch"
// -- collect_can_raise conflates the two on purpose, since ITS
// consumer, the per-call-site known_callee optimization, only cares
// whether a check can be SKIPPED, where over-approximating is cheap).
// `print` doesn't raise directly but calls unresolved dispatch
// internally, so its Sym::can_raise is true -- newly arming
// pyc_program_has_raise for every program that calls print (i.e.
// nearly all of them) regressed --test_scoping golden traces (a new
// __pyc_exc__ symbol lookup appears) and, worse, broke async/coroutine
// codegen outright (`co_await t4` on a `_CG_nil_type` -- inserting
// exception-check control flow into an async body in a new place
// isn't safe in general). Treating every unresolved (method-dispatch)
// call as conservative "could raise" made it worse: virtually any
// non-trivial program calls at least one method.
//
// FIX: use Sym::direct_raise (ifa/if1/sym.h) instead -- set only when
// a function's OWN body textually contains a raise (build_if1_pyda's
// PY_raise_stmt case), not propagated through calls -- and handle
// method calls (unresolvable to a specific Sym pre-FA; `s.index(x)`
// could be any class's `.index`) by NAME instead of by Sym: collect
// every builtin method name with direct_raise set (once, in
// ast_to_if1_baseline, after the builtin module's own build_if1 has
// run so direct_raise is populated -- see pyc_builtin_raise_names
// below) and match a user-code method-call's attribute name against
// that set. This precisely catches str.index() (name "index" is in
// the set) without touching print (not in the set: print is a native
// compiler intercept, sym_print, not a PY_funcdef-backed Sym at all,
// so it was never a "direct_raise" candidate to begin with) or any
// other call that merely transitively depends on something raise-
// capable. User-level direct raisers don't need either path: a
// user-defined function/method with its own `raise` already arms the
// gate unconditionally via PY_raise_stmt's existing build_syms_pyda
// case, regardless of whether it's ever called.
static void collect_raise_names(PyDAST *n, PycCompiler &ctx, Vec<cchar *> &out) {
  if (!n) return;
  if (n->kind == PY_funcdef) {
    Sym *fn_sym = getAST(n, ctx)->sym;
    if (fn_sym && fn_sym->direct_raise && fn_sym->name) out.add(cannonicalize_string(fn_sym->name));
  }
  for (auto c : n->children.values()) collect_raise_names(c, ctx, out);
}

// Populated once from the builtin module, after its own build_if1 has
// run (ast_to_if1_baseline) -- see collect_raise_names's comment
// above. Names are cannonicalize_string-interned, so pointer equality
// is safe for lookup.
Vec<cchar *> pyc_builtin_raise_names;

void collect_builtin_raise_names(PyDAST *builtin_pymod, PycCompiler &ctx) {
  collect_raise_names(builtin_pymod, ctx, pyc_builtin_raise_names);
}

static bool ast_reaches_raise(PyDAST *n, PycCompiler &ctx) {
  if (!n) return false;
  switch (n->kind) {
    case PY_raise_stmt:
    case PY_assert_stmt:
    case PY_yield_stmt:
    case PY_yield_expr:
    case PY_yield_from_expr:
      return true;
    case PY_power:
      for (int i = 1; i < n->children.n; i++) {
        PyDAST *trailer = n->children[i];
        if (trailer->kind != PY_call) continue;
        PyDAST *prev = n->children[i - 1];
        if (prev->kind == PY_attribute && prev->children.n && prev->children[0]->str_val) {
          // method call: `<expr>.<name>(...)` -- name isn't resolvable
          // to a specific Sym pre-FA (any class could define it), so
          // match by name against the builtin direct-raiser set.
          cchar *attr = cannonicalize_string(prev->children[0]->str_val);
          for (auto raiser : pyc_builtin_raise_names.values())
            if (raiser == attr) return true;
        } else if (i == 1 && n->children[0]->kind == PY_name && n->children[0]->ctx != PY_STORE) {
          // plain call: `name(...)` -- resolvable, check direct_raise
          // on the actual target (covers a hypothetical builtin free
          // function that raises directly; none exist today, but this
          // costs nothing and needs no separate name-matching path).
          Sym *ref = getAST(n->children[0], ctx)->sym;
          Sym *callee = nullptr;
          if (ref) {
            Sym *ifn = ctx.def_internal_fn.get(ref);
            callee = ifn ? ifn : (ref->is_fun ? ref : nullptr);
          }
          if (callee && callee->direct_raise) return true;
        }
      }
      break;
    default:
      break;
  }
  for (auto c : n->children.values())
    if (ast_reaches_raise(c, ctx)) return true;
  return false;
}

bool user_code_reaches_raise(Vec<PycModule *> &mods, PycCompiler &ctx) {
  for (auto m : mods.values())
    if (ast_reaches_raise(m->pymod, ctx)) return true;
  return false;
}

// issue 011: compute Sym::can_raise (see its declaration, ifa/if1/sym.h)
// for every function found across `mods` via collect_can_raise, then
// a simple worklist fixed point propagating along call_edges --
// standard "compute closure" iteration, same shape as ast.cc's
// implementor/specializer closure. Run once over just the builtin
// module (ast_to_if1_baseline -- self-contained, shared via CoW
// across REPL fork children) and once over user modules
// (ast_to_if1_extend, per compile): builtin callees already carry
// their final bit by the second call, so that pass's fixed point
// only needs to iterate over the newly-collected user-level edges.
void compute_can_raise(Vec<PycModule *> &mods, PycCompiler &ctx) {
  Map<PyDAST *, bool> resolved_calls;
  Map<Sym *, Vec<Sym *> *> call_edges;
  Vec<Sym *> raisers;
  for (auto m : mods.values()) collect_can_raise(m->pymod, ctx, nullptr, resolved_calls, call_edges, raisers);
  for (auto f : raisers.values()) f->can_raise = 1;
  Vec<Sym *> callers;
  call_edges.get_keys(callers);
  bool changed = true;
  while (changed) {
    changed = false;
    for (auto caller : callers.values()) {
      if (caller->can_raise) continue;
      Vec<Sym *> *callees = call_edges.get(caller);
      for (auto callee : callees->values())
        if (callee->can_raise) {
          caller->can_raise = 1;
          changed = true;
          break;
        }
    }
  }
}

// Set up function scope for pyda path
static Sym *def_fun_pyda(PyDAST *n, PycAST *ast, Sym *fn, PycCompiler &ctx) {
  fn->in = ctx.scope_stack.last()->in;
  fn->is_async = n->is_async;
  if (n->kind == PY_funcdef) {
    for (auto c : n->children.values())
      if (pyda_contains_yield(c)) { fn->is_generator = 1; break; }
    for (auto c : n->children.values())
      if (pyda_contains_return(c)) { fn->fun_returns_value = 1; break; }
  }
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

// issues/001: if a just-finished lambda/nested-def scope references any
// names from an enclosing FUNCTION scope (not global), synthesize a
// closure-carrier class instead of relying on ifa's nesting_depth/display
// machinery -- which only supports nested functions called while their
// lexical parent's activation is still on the call stack, and asserts
// (unique_AVar) the moment a closure escapes that activation (e.g. `f =
// make_adder(3)` returned, then `f(4)` called later from an unrelated
// scope). Mirrors gen_class_pyda's record-class construction: `self`
// threaded as an explicit argument, one field per captured name, sidestepping
// nesting_depth/display entirely (the same reason bound methods already work
// when they escape -- `self` is a real heap value, not a stack-relative
// lookup).
//
// Must run *after* the scope's own body has been walked by build_syms_pyda
// (so every free-variable reference has already been marked NONLOCAL_USE by
// make_PycSymbol's PYC_USE case) and *before* exit_scope. Returns the
// closure class Sym, or null if the scope captured nothing (the common,
// unaffected case -- top-level lambdas, methods, non-capturing nested defs).
static Sym *maybe_synthesize_closure_pyda(PycAST *ast, PycCompiler &ctx) {
  Vec<Sym *> captured;
  form_Map(MapCharPycSymbolElem, x, ctx.scope_stack.last()->map)
    if (x->value == NONLOCAL_USE) {
      int level = 0;
      PycSymbol *outer = find_PycSymbol(ctx, x->key, &level);
      if (!outer) continue;
      // A bare-name sub-node inside a PY_attribute trailer (e.g. the `i`
      // in `y.i`, or the `append` in `L.append(a)`) gets spuriously
      // resolved and marked NONLOCAL_USE too: build_syms_pyda's
      // PY_attribute case falls through to the same generic recursive
      // case as PY_power/PY_call/etc, which walks *every* child --
      // including a trailer's attribute-name child -- via ordinary
      // PYC_USE scope lookup exactly like a real identifier reference.
      // Harmless before this change (attribute access uses the
      // trailer's raw string, not that resolution's result), but the
      // scope-map side effect still fires. Two ways this shows up as a
      // false positive, both excluded here:
      //  - Found via an import scope (level < 0): these are compiler-
      //    internal dispatch-name placeholders (e.g. `sym_append` from
      //    pyc_symbols.h's S(append), used to reference "the append
      //    operation" from C++ code, not a real Python-level binding)
      //    that happen to be resolvable this way -- never a real
      //    enclosing-function local regardless.
      //  - Found in a class body (that scope's own `in` is set): real
      //    Python scoping doesn't let a nested function see a class
      //    attribute via a bare name anyway (class scope is famously
      //    excluded from the enclosing-scope chain), so this was never
      //    a genuine capture either. Checking `in` directly (not its
      //    type_kind) matters: `enter_scope` sets `in` only for a real
      //    PY_classdef, but a class's *own* type_kind isn't always
      //    Type_RECORD -- int/float/bool/list/tuple/str all get a
      //    different type_kind via their own special registration
      //    (Type_ALIAS, Type_PRIMITIVE, etc, see issue 022), yet their
      //    method bodies are still ordinary class-body scopes for this
      //    purpose. A scope only ever gets a non-null `in` from
      //    `enter_scope(ctx, ast->sym)` at a classdef, or retroactively
      //    from this very function for an already-synthesized closure
      //    (also correctly excluded here, since that scope's own body
      //    already resolves its captures via self.field).
      if (level < 0) continue;
      if (ctx.scope_stack[level]->in) continue;
      // issues/001 follow-up: a function's own name, referenced
      // recursively from inside its own body (`def outer(): def
      // fact(n): ... n * fact(n - 1)`), resolves to the enclosing
      // scope's binding exactly like a genuine free variable -- but
      // it must NOT be treated as a capture: threading the function
      // through a carrier-class field rebinds ast->sym to the
      // closure instance and triggers the issue-007 Finding 2
      // self-referential-reassignment FA warnings. A direct
      // recursive call is always stack-disciplined (the activation
      // is live), so the ordinary nesting_depth/display path
      // handles it correctly.
      if (outer->sym == ast->sym) continue;
      // issues/007 split identity: the public name is now a distinct
      // variable Sym (ast->rval) from the closure body's internal Sym
      // (ast->sym); a recursive self-reference resolves to the public
      // one. Same reasoning as above: not a capture.
      if (ast->rval && outer->sym == ast->rval) continue;
      captured.add(outer->sym);
      // Transitive captures (issues/007 parameterized decorators,
      // e.g. `def add_n(n): def dec(f): def wrapper(x): return
      // f(x) + n`): `wrapper` captures `n` from its GRANDPARENT
      // scope. Every intermediate function scope must capture it
      // too, so this scope's creation-site snapshot can read it
      // (via the intermediate scope's own self.field rewrite).
      // Mark the name NONLOCAL_USE in each intermediate scope;
      // inner scopes exit (and synthesize) before outer ones, so
      // the outer maybe_synthesize call sees the propagated mark.
      for (int lvl = level + 1; lvl < ctx.scope_stack.n - 1; lvl++) {
        PycScope *s = ctx.scope_stack[lvl];
        if (s->in) continue;  // class-body scopes don't capture
        if (!s->map.get(x->key)) s->map.put(x->key, NONLOCAL_USE);
      }
    }
  if (!captured.n) return nullptr;
  if (captured.n > 1) qsort(captured.v, captured.n, sizeof(captured.v[0]), compar_syms);
  Sym *cls = new_sym(ast, "__closure__", 1);
  cls->type_kind = Type_RECORD;
  cls->self = new_global(ast);
  for (Sym *cap : captured.values()) cls->has.add(cap);
  // Make bare references to a captured name inside this scope's body
  // resolve as `self.name` reads/writes instead of raw nesting-depth
  // lookups -- reusing the exact mechanism PY_name's build_if1_pyda case
  // already uses for class-body-level attribute access (checks
  // `scope_stack.last()->in`'s `has[]`), since `captured`'s Syms are the
  // very same Sym objects those bare-name references already resolve to.
  ctx.scope_stack.last()->in = cls;
  // Stashed for build_if1_pyda's gen_lambda_pyda/gen_fun_pyda, which sets
  // up fn->self (as the closure's own first formal, specialized against
  // cls, mirroring gen_class_pyda's __call__ wrapper) and emits the
  // creation-site instantiation + per-field capture.
  ast->closure_cls = cls;
  return cls;
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
// Not static: also called from python_ifa_build_if1.cc to build symbols for
// expression ASTs synthesized after the whole-module build_syms pass has
// already run (currently: f-string `{expr}` interpolation sub-expressions,
// parsed on demand at build_if1 time). Declared in python_ifa_int.h.
int build_syms_pyda(PyDAST *n, PycCompiler &ctx);

// Symbol-table pass for a comprehension / generator expression body,
// run inside the comprehension's own scope (already entered by the
// caller). In every comprehension form the `for`-chain
// (PY_list_for / PY_comp_for) is the LAST child and the element
// expressions (list/gen elt, set expr, or dict key+value) precede
// it. The for-chain is what binds the loop targets, so it MUST be
// walked before the element expressions: otherwise an element that
// references a target (e.g. `[i for i in xs]`) resolves that name as
// a not-yet-bound USE, which falls through to the module scope and
// creates a spurious global. A second same-named comprehension then
// sees that global and dies with "'i' redefined as local". Binding
// targets first matches CPython, where the comprehension is analyzed
// as a function whose parameters are the targets.
static void build_comprehension_body_syms(PyDAST *n, PycCompiler &ctx) {
  int last = n->children.n - 1;
  build_syms_pyda(n->children[last], ctx);                              // for-chain: bind targets
  for (int i = 0; i < last; i++) build_syms_pyda(n->children[i], ctx);  // then element exprs
}

static void build_import_syms_name_pyda(PyDAST *n, PycCompiler &ctx) {
  // n->kind == PY_import_name, children are PY_dotted_as_name or PY_testlist of them
  Vec<PyDAST *> names;
  for (auto c : n->children.values()) {
    if (c->kind == PY_dotted_as_name)
      names.add(c);
    else if (c->kind == PY_testlist)
      for (auto cc : c->children.values())
        if (cc->kind == PY_dotted_as_name) names.add(cc);
  }
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
  bool any = false;
  for (int i = 1; i < n->children.n; i++) {
    PyDAST *child = n->children[i];
    if (child->kind == PY_testlist) {
      for (auto ia : child->children.values())
        if (ia->kind == PY_import_as_name) {
          cchar *sym_name = ia->children[0]->str_val;
          cchar *as_name = (ia->children.n > 1) ? ia->children[1]->str_val : nullptr;
          build_import_syms(const_cast<char *>(sym_name), const_cast<char *>(as_name),
                            const_cast<char *>(from_mod), ctx);
          any = true;
        }
    } else if (child->kind == PY_import_as_name) {
      cchar *sym_name = child->children[0]->str_val;
      cchar *as_name = (child->children.n > 1) ? child->children[1]->str_val : nullptr;
      build_import_syms(const_cast<char *>(sym_name), const_cast<char *>(as_name),
                        const_cast<char *>(from_mod), ctx);
      any = true;
    }
  }
  // `from X import *`: the '*' is a bare token, so no import_as_name
  // children exist. A null sym means star (bind all public names).
  if (!any) build_import_syms(nullptr, nullptr, const_cast<char *>(from_mod), ctx);
}

int build_syms_pyda(PyDAST *n, PycCompiler &ctx) {
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
      // issue 027 feature: @staticmethod / @classmethod are definition
      // MARKERS, not runtime decorators (pyc has no staticmethod/
      // classmethod callables to apply). Detect them before decorator
      // processing so the names are never resolved as variables, and
      // record them on the def's PycAST for gen_fun_pyda (formal-list
      // convention) and build_if1_pyda (skip in the application loop).
      bool marker_static = false, marker_class = false;
      for (int i = 0; i < n->children.n - 1; i++) {
        Vec<PyDAST *> decs;
        if (n->children[i]->kind == PY_suite)
          for (auto c : n->children[i]->children.values()) decs.add(c);
        else
          decs.add(n->children[i]);
        for (auto dec : decs.values()) {
          if (dec->kind != PY_decorator || dec->children.n < 1) continue;
          if (dec->children.n >= 2 && dec->children[1]->kind == PY_arglist) continue;
          if (decorator_name_is(dec->children[0]->str_val, "staticmethod")) marker_static = true;
          else if (decorator_name_is(dec->children[0]->str_val, "classmethod")) marker_class = true;
        }
      }
      // Pre-scope: process decorators (markers excluded -- their names
      // intentionally resolve to nothing).
      if (!marker_static && !marker_class)
        for (int i = 0; i < n->children.n - 1; i++) build_syms_pyda(n->children[i], ctx);
      // Dispatch to funcdef or classdef handling
      if (def->kind == PY_funcdef) {
        PycAST *def_ast = getAST(def, ctx);
        def_ast->is_staticmethod = marker_static;
        def_ast->is_classmethod = marker_class && !marker_static;
        PyDAST *params = def->children[1];
        PyDAST *varargsl = (params->children.n > 0) ? params->children[0] : nullptr;
        if (varargsl)
          for (auto c : varargsl->children.values())
            if (c->kind == PY_arg_default) build_syms_pyda(c->children[1], ctx);
        PycSymbol *ps = make_PycSymbol(ctx, def->children[0]->str_val, PYC_LOCAL);
        bool marker_method = (marker_static || marker_class) && ctx.in_class() && ctx.cls()->type_kind == Type_RECORD;
        if (marker_method) {
          // Method-FIELD shape (mirrors PY_funcdef's is_method branch):
          // the public name is a class prototype field whose alias is
          // the function, installed via a setter send. The function
          // itself uses the VALUE convention (as[0] == fn, no self
          // specialization -- see gen_fun_pyda), so reads of the field
          // yield a plain callable: no receiver for @staticmethod; the
          // caller supplies the class value for @classmethod.
          def_ast->rval = ps->sym;
          def_ast->sym = new_sym(def_ast, 1);
          def_ast->rval->alias = def_ast->sym;
          def_ast->sym = def_fun_pyda(def, def_ast, def_ast->sym, ctx);
          def_ast->sym->is_static_method = marker_static;
          def_ast->sym->is_class_method = def_ast->is_classmethod;
          // Python semantics: a class body is NOT an enclosing lexical
          // scope for functions defined in it. Marker methods flow as
          // bare VALUES callable from any scope, and FA's make_AVar
          // resolves a value-carried fn's outer references through the
          // calling EntrySet's display -- which only has entries for
          // real enclosing FUNCTIONS. With the class-scope level
          // counted (def_fun_pyda's scope_stack depth), a staticmethod
          // in a module-level class gets depth 2 and a call from
          // module code indexes display[1] of an empty display
          // (SEGFAULT -- bh's `BH.main(argv)`). Uncount the class
          // scope; formals/locals re-derive as nesting_depth+1 at if1
          // finalization, which runs after this.
          def_ast->sym->nesting_depth -= 1;
          if1_send(if1, &def_ast->code, 5, 1, sym_operator, ctx.cls()->self, sym_setter,
                   if1_make_symbol(if1, def_ast->rval->name), def_ast->sym, new_sym(def_ast))
              ->ast = def_ast;
        } else {
          // issues/007: same split identity as the plain PY_funcdef
          // case -- the public name is a variable; the internal Sym
          // carries the closure. Decorator application then rebinds
          // the variable (`f = d(f)`) via ordinary moves (see
          // PY_decorated in build_if1_pyda).
          def_ast->rval = ps->sym;
          def_ast->sym = def_fun_pyda(def, def_ast, new_sym(def_ast, def->children[0]->str_val, 1), ctx);
          ctx.def_internal_fn.put(def_ast->rval, def_ast->sym);
        }
        ast->rval = def_ast->rval;
        ast->sym = def_ast->sym;
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
      bool is_method = ctx.in_class() && ctx.cls()->type_kind == Type_RECORD;
      if (is_method) {
        // Mirror CPython FunctionDef_kind path: create named sym + func sym with alias
        ast->rval = ps->sym;
        ast->sym = new_sym(ast, 1);
        ast->rval->alias = ast->sym;
        ast->sym = def_fun_pyda(n, ast, ast->sym, ctx);
        // Generate setter into ast->code (collected by gen_class_pyda into class init body)
        if1_send(if1, &ast->code, 5, 1, sym_operator, ctx.cls()->self, sym_setter,
                 if1_make_symbol(if1, ast->rval->name), ast->sym, new_sym(ast))->ast = ast;
      } else if (ctx.in_class()) {
        // Class-body def in a NON-Type_RECORD class (the builtin
        // int/float/list/tuple/str/... classes get other type_kinds
        // via their special registration -- issue 022). Their method
        // machinery binds through the scope Sym directly
        // (gen_class_pyda's has[] collection / name-symbol
        // dispatch), so keep the legacy identity here.
        ast->rval = ast->sym = def_fun_pyda(n, ast, ps->sym, ctx);
      } else {
        // issues/007 Finding 2 (and issues/001's residuals): never
        // attach if1_closure directly to the public-name Sym.
        // The function body gets its own internal Sym (mirroring the
        // is_method branch above); the public name is an ordinary
        // variable bound via if1_move at the def site (see
        // PY_funcdef in build_if1_pyda). Reassigning the name (a
        // decorator's `f = d(f)`, or any `f = g`) then behaves like
        // any other variable reassignment instead of rewriting a Sym
        // FA treats as BEING a function. Note: no `alias` link here
        // -- build_if1_pyda uses `rval->alias == sym` to recognize
        // the method case.
        ast->rval = ps->sym;
        ast->sym = def_fun_pyda(n, ast, new_sym(ast, n->children[0]->str_val, 1), ctx);
        ctx.def_internal_fn.put(ast->rval, ast->sym);
      }
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
      // issues/025: pre-bind whole-function locals so a read-before-
      // write of a local (common `if first or x < best: ... best = x`
      // shape) resolves to the local instead of minting a spurious
      // global -- see prebind_function_locals.
      if (n->children.n >= 3) prebind_function_locals(n->children[2], ctx);
      if (n->children.n >= 3) build_syms_pyda(n->children[2], ctx);
      form_Map(MapCharPycSymbolElem, x, ctx.scope_stack.last()->map)
        if (!MARKED(x->value) && !x->value->sym->is_fun) {
          x->value->sym->is_local = 1;
          x->value->sym->nesting_depth = LOCALLY_NESTED;
        }
      // issues/001: a nested `def` (this function defined directly inside
      // another function's body, not a class body -- ctx.in_class() above
      // already special-cased methods) can capture enclosing-function
      // locals exactly like a lambda can (e.g. `def make_adder(n): def
      // adder(x): return x + n; return adder`). Harmless no-op for the
      // overwhelmingly common non-capturing case (top-level defs, methods).
      if (!is_method) maybe_synthesize_closure_pyda(ast, ctx);
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
      // Post-classdef: collect base classes and members.
      // A base must already BE a class here (classdefs are processed
      // in program order, so a legal base's type_kind is set by the
      // time a subclass names it). An unresolved or non-class name
      // (e.g. inheriting from a builtin pyc doesn't define) yields a
      // plain Type_NONE Sym; letting inherits_add wire that into the
      // hierarchy plants a meta_type-less sym that
      // build_type_hierarchy later derefs as null -- the
      // amaze/tictactoe/voronoi2 SIGSEGV family (ifa/issues/042).
      // Fail cleanly instead.
      auto check_base = [&](Sym *base) {
        if (!base) fail("error line %d, base not found for class '%s'", ctx.lineno, cdef_ast->sym->name);
        if (!base->type_kind && !base->is_constant)
          fail("error line %d, base '%s' of class '%s' is not a class (undefined, or not a type)", ctx.lineno,
               base->name ? base->name : "<anonymous>", cdef_ast->sym->name);
      };
      bool any_base = false;
      for (int i = 1; i < cdef->children.n - 1; i++) {
        PyDAST *base_ast = cdef->children[i];
        if (base_ast->kind == PY_tuple) {
          for (int j = 0; j < base_ast->children.n; j++) {
            Sym *base = getAST(base_ast->children[j], ctx)->sym;
            check_base(base);
            cdef_ast->sym->inherits_add(base);
            any_base = true;
          }
        } else {
          Sym *base = getAST(base_ast, ctx)->sym;
          check_base(base);
          cdef_ast->sym->inherits_add(base);
          any_base = true;
        }
      }
      // Python 3: a bare `class A:` implicitly derives from object.
      // Without this, user classes get NO base, so object-level
      // defaults (__pyc_to_bool__, __not__, __str__, ...) never
      // dispatch and `if a:` / `not a` on a plain instance has no
      // type (issue 025). Builtin-module classes are exempt: they
      // define the root hierarchy itself (object, __pyc_any_type__).
      if (!any_base && !ctx.is_builtin()) cdef_ast->sym->inherits_add(sym_object);
      // Collect class fields into a temporary Vec, sort by
      // name, then commit to `has` in sorted order.  Direct
      // iteration over the scope map would walk the
      // pointer-hashed bucket layout, producing non-
      // deterministic field order across runs.
      {
        Vec<PycSymbol *> fields;
        form_Map(MapCharPycSymbolElem, x, ctx.scope_stack.last()->map)
          if (!MARKED(x->value) && !x->value->sym->is_fun) {
            fields.add(x->value);
          }
        if (fields.n > 1)
          qsort(fields.v, fields.n, sizeof(fields.v[0]),
                compar_pycsymbol_by_name);
        for (PycSymbol *ps : fields) {
          cdef_ast->sym->has.add(ps->sym);
          ps->sym->in = cdef_ast->sym;
        }
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
      maybe_synthesize_closure_pyda(ast, ctx);
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

    case PY_nonlocal_stmt:
      for (auto c : n->children.values()) make_PycSymbol(ctx, c->str_val, PYC_NONLOCAL);
      return 0;

    // Loops don't push a scope, so lcontinue/lbreak live in the
    // ENCLOSING function scope's slots -- they must be saved and
    // restored around the loop's own subtree, or every statement
    // AFTER a nested loop (but still inside an outer one) resolves
    // break/continue to the inner loop's labels. othello2's
    // vs_cpu_ugi hit exactly that: a `break` meant for the outer
    // `while` (as a sibling after an inner `for`) bound to the inner
    // for's break label -- the label it is placed straight after --
    // lowering to a goto-to-self infinite loop (a CFG region with no
    // path to exit, which then crashed the dominator build; see
    // ifa/optimize/dom.cc). Restore also runs BEFORE the loop's
    // else_clause: Python's for/while-else runs after the loop, and
    // break/continue inside it belong to the OUTER loop.
    case PY_for_stmt:
    case PY_while_stmt: {
      Label *save_continue = ctx.lcontinue(), *save_break = ctx.lbreak();
      ctx.lcontinue() = ast->label[0] = if1_alloc_label(if1);
      ctx.lbreak() = ast->label[1] = if1_alloc_label(if1);
      if (n->kind == PY_for_stmt) mark_store(n->children[0]);
      for (auto c : n->children.values()) {
        if (c->kind == PY_else_clause) {
          ctx.lcontinue() = save_continue;
          ctx.lbreak() = save_break;
        }
        build_syms_pyda(c, ctx);
      }
      ctx.lcontinue() = save_continue;
      ctx.lbreak() = save_break;
      return 0;
    }

    case PY_assign:
      for (int i = 0; i < n->children.n - 1; i++) mark_store(n->children[i]);
      for (auto c : n->children.values()) build_syms_pyda(c, ctx);
      return 0;

    case PY_namedexpr_test:
      mark_store(n->children[0]);
      build_syms_pyda(n->children[0], ctx);
      build_syms_pyda(n->children[1], ctx);
      return 0;

    case PY_annassign:
      mark_store(n->children[0]);
      build_syms_pyda(n->children[0], ctx);
      if (n->children.n == 3) {
        build_syms_pyda(n->children[2], ctx);
      }
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
      build_comprehension_body_syms(n, ctx);
      exit_scope(ctx);
      return 0;
    }

    case PY_list_for:
    case PY_comp_for:
      mark_store(n->children[0]);
      for (auto c : n->children.values()) build_syms_pyda(c, ctx);
      return 0;

    case PY_attribute:
      // An attribute trailer `.name`: children[0] is the attribute
      // NAME, a literal identifier that build_if1's PY_power handler
      // consumes as a raw string (`trailer->children[0]->str_val` ->
      // make_symbol). It is NOT a variable reference and must NOT be
      // run through build_syms_pyda / PYC_USE scope resolution. Doing
      // so (the old generic-recurse behavior) looked the name up as an
      // ordinary identifier, failed, and created a spurious *module
      // global* for every attribute name in the program. Usually inert
      // -- but if that same name was later a reassigned parameter or
      // local (`color` in go.py: `[SHOW[sq.color] ...]` then a method
      // with `def m(self, color): ...; color = ...`), the store saw the
      // global sentinel and died with "'X' redefined as local". The
      // object being accessed is the sibling atom in the enclosing
      // PY_power, not a child here, so there is nothing to recurse.
      ast->rval = new_sym(ast);
      return 0;

    case PY_import_name:
      build_import_syms_name_pyda(n, ctx);
      return 0;

    case PY_import_from:
      build_import_syms_from_pyda(n, ctx);
      return 0;

    case PY_continue_stmt:
      if (!ctx.lcontinue()) fail("error line %d, 'continue' not properly in loop", ctx.lineno);
      ast->label[0] = ctx.lcontinue();
      return 0;
    case PY_break_stmt:
      if (!ctx.lbreak()) fail("error line %d, 'break' outside loop", ctx.lineno);
      ast->label[0] = ctx.lbreak();
      return 0;
    case PY_return_stmt:
      ast->label[0] = ctx.lreturn();
      for (auto c : n->children.values()) build_syms_pyda(c, ctx);
      return 0;

    case PY_raise_stmt:
      // issue 011: arm the exception machinery program-wide (the
      // post-call pending checks) only when some USER module
      // actually raises -- exception-free programs build byte-
      // identical IF1. Deliberately excludes the builtin module: it
      // always contains __pyc_assert_fail__'s own `raise` (loaded
      // for every program regardless of whether user code ever calls
      // assert), which would otherwise permanently arm every
      // compilation and defeat the gate (confirmed empirically --
      // scoping tests' symbol-resolution traces picked up
      // __pyc_exc__/__pyc_unhandled_exception__ lookups with no
      // exception-handling code anywhere in the source). PY_assert_stmt
      // below arms it instead, exactly when user code can actually
      // reach that raise.
      if (!ctx.is_builtin()) pyc_program_has_raise = true;
      goto generic_recurse;

    case PY_except_clause:
      // issue 011: `except X as e` -- e is a fresh local binding
      // (STORE), not a use; unmarked, the generic recursion would
      // resolve it as a load of an undefined name.
      if (n->children.n == 2 && n->children[1]->kind == PY_name) mark_store(n->children[1]);
      goto generic_recurse;

    case PY_assert_stmt:
      // issue 011: `assert` lowers to a call to __pyc_assert_fail__,
      // which raises AssertionError -- arms the same gate a direct
      // `raise` would, for the same reason (builtin-module raises
      // excluded above; this is the point where a USER module
      // becomes reachable to one).
      if (!ctx.is_builtin()) pyc_program_has_raise = true;
      goto generic_recurse;

    case PY_yield_stmt:
      // issues/014: a generator's __next__()/.send() (09_generator.py,
      // a builtin module) raises StopIteration on exhaustion -- arms
      // the same gate a direct `raise` would, for the same reason
      // builtin-module raises are excluded above (__pyc_assert_fail__
      // would otherwise permanently arm it): `yield` is the point
      // where a USER module becomes reachable to that builtin raise,
      // exactly mirroring PY_assert_stmt's role for
      // __pyc_assert_fail__. Without this, a program with a generator
      // but no direct user-level raise/assert anywhere (e.g. `def
      // gen(): yield 1; return 42` driven by manual `.__next__()`
      // calls to exhaustion) left pyc_program_has_raise false, so
      // emit_exc_check (python_ifa_build_if1.cc) never emitted the
      // post-call check needed to actually catch it -- confirmed via
      // a minimal repro: identical `raise`/try-except code worked
      // fine when the raise was user-level, and silently produced
      // garbage instead of raising when the only raise reachable was
      // this builtin one.
      if (!ctx.is_builtin()) pyc_program_has_raise = true;
      goto generic_recurse;

    case PY_yield_expr:
      // issues/014: same reasoning as PY_yield_stmt above -- `x =
      // yield foo` is the other AST shape a generator's body can use
      // (def_fun_pyda's is_generator scan checks both). NOT routed
      // through generic_recurse: that shared block also does
      // tuple-expression treatment (ast->rval + sym_tuple-if-all-
      // children-have-syms) that doesn't apply here -- keep
      // PY_yield_expr's original plain recursion (this is exactly
      // where it sat in the `default:` group below before this case
      // was split out).
      if (!ctx.is_builtin()) pyc_program_has_raise = true;
      for (auto c : n->children.values()) build_syms_pyda(c, ctx);
      return 0;

    case PY_yield_from_expr:
      // issues/014: `yield from EXPR` -- one child (the sub-iterable
      // expression). Arms pyc_program_has_raise for the same reason
      // PY_yield_stmt/PY_yield_expr do: build_if1_pyda desugars this
      // into a loop that calls EXPR.send(...) and explicitly catches
      // StopIteration -- that catch's own dispatch code is dead
      // without the gate armed (emit_exc_check no-ops entirely when
      // !pyc_program_has_raise), and a program using ONLY `yield from`
      // (no bare `yield`, no direct user-level raise/assert) would
      // otherwise never arm it at all.
      if (!ctx.is_builtin()) pyc_program_has_raise = true;
      for (auto c : n->children.values()) build_syms_pyda(c, ctx);
      return 0;

    case PY_expr_stmt:
    case PY_pass_stmt:
    case PY_del_stmt:
    case PY_if_stmt:
    case PY_elif_clause:
    case PY_else_clause:
    case PY_try_stmt:
    case PY_except_handler:
    case PY_finally_clause:
    case PY_with_item:
      // issues/108: `with EXPR as TARGET` BINDS TARGET. The grammar is
      // `with_item: test ('as' expr)?`, so children[1] is the target when
      // present. It was never marked PY_STORE, so the body's reads of it
      // resolved as unresolved USEs and minted module globals -- silently
      // before issues/107, and reported as undefined afterwards. Marking
      // it is exactly what assignment targets and `for` variables get
      // (mark_store, above).
      if (n->children.n >= 2) mark_store(n->children[1]);
      goto generic_recurse;

    case PY_with_stmt:
    case PY_match_stmt:
      goto generic_recurse;

    case PY_case_block: {
      // children[1] is the case pattern (python.g: `case_block:
      // CASE_KW test case_guard? ':' suite` -- children[0] is the
      // CASE_KW marker node build_match_pyda's own indexing dance
      // already has to account for). Recursively mark every bare,
      // non-wildcard NAME reachable through capture position as
      // PY_STORE -- mirrors build_match_pyda/build_pattern_match's
      // own traversal exactly (wildcard/capture/or-pattern/sequence/
      // literal), since a capture pattern can appear nested inside
      // a sequence pattern (`case [a, b]:` binds BOTH `a` and `b`),
      // not just at the top level. A capture pattern's name always
      // matches and binds the subject to a NEW local (shadows any
      // same-named outer binding -- capture patterns never compare
      // against an existing value; only dotted/attribute patterns
      // like `Color.RED` do that, and those aren't implemented yet
      // either). Mark it PY_STORE, the same way assignment targets
      // and `for` loop variables are marked (mark_store, above), so
      // build_syms_pyda's own PY_name case creates a fresh
      // PYC_LOCAL instead of failing to resolve an undefined USE.
      if (n->children.n > 1) mark_pattern_captures(n->children[1]);
      goto generic_recurse;
    }

    generic_recurse:
    case PY_bool_or:
    case PY_bool_and:
    case PY_bool_not:
    case PY_compare:
    case PY_cmp_op:
    case PY_binop:
    case PY_unaryop:
    case PY_await_expr:
    case PY_power:
    case PY_call:
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

    case PY_dict: {
      // Two grammar shapes (python.g dictorsetmaker): a flat literal
      // `{k: v, ...}` (2N children, alternating key/value exprs), or a dict
      // comprehension `{key: value for target in iter}` (3 children:
      // [key_expr, value_expr, PY_comp_for]). The comprehension form gets
      // its own scope, mirroring PY_listcomp/PY_genexpr/PY_set above (so the
      // loop target doesn't leak into the enclosing scope).
      if (n->children.n == 3 && n->children[2]->kind == PY_comp_for) {
        enter_scope(n, ctx, nullptr);
        ctx.lyield() = ast->label[0] = if1_alloc_label(if1);
        build_comprehension_body_syms(n, ctx);
        exit_scope(ctx);
      } else {
        for (auto c : n->children.values()) build_syms_pyda(c, ctx);
      }
      PycSymbol *ds = make_PycSymbol(ctx, "dict", PYC_USE);
      if (ds) ast->sym = ds->sym;
      else fprintf(stderr, "PY_dict: 'dict' not found in scope (line %d, imports.n=%d)\n", n->line, ctx.imports.n);
      ast->rval = new_sym(ast);
      return 0;
    }

    case PY_set: {
      // Two grammar shapes (python.g dictorsetmaker): a flat literal
      // `{e1, e2, ...}` (n children, all element exprs), or a set
      // comprehension `{expr for target in iter}` (2 children: [expr,
      // PY_comp_for]). The comprehension form gets its own scope, mirroring
      // PY_listcomp/PY_genexpr just above (so the loop target doesn't leak
      // into the enclosing scope).
      if (n->children.n == 2 && n->children[1]->kind == PY_comp_for) {
        enter_scope(n, ctx, nullptr);
        ctx.lyield() = ast->label[0] = if1_alloc_label(if1);
        build_comprehension_body_syms(n, ctx);
        exit_scope(ctx);
      } else {
        for (auto c : n->children.values()) build_syms_pyda(c, ctx);
      }
      PycSymbol *ss = make_PycSymbol(ctx, "set", PYC_USE);
      if (ss) ast->sym = ss->sym;
      else fprintf(stderr, "PY_set: 'set' not found in scope (line %d)\n", n->line);
      ast->rval = new_sym(ast);
      return 0;
    }

    case PY_number:
    case PY_string:
    case PY_list:
    case PY_slice:
    case PY_subscriptlist:
    case PY_parameters:
    case PY_varargslist:
    case PY_fpdef:
    case PY_fplist:
    case PY_arglist:
    case PY_keyword_arg: {
      // issues/107: children[0] is the PARAMETER NAME (`print(x, end=" ")`
      // -- `end` names a formal, not a reference to a variable called
      // `end`). It must still be WALKED (the earlier attempt to skip it
      // broke 283 tests), but an unresolved lookup on it must not be
      // reported as an undefined name.
      ctx.in_kwarg_key++;
      if (n->children.n) build_syms_pyda(n->children[0], ctx);
      ctx.in_kwarg_key--;
      for (int i = 1; i < n->children.n; i++) build_syms_pyda(n->children[i], ctx);
      return 0;
    }
    case PY_star_arg:
    case PY_dstar_arg:
    case PY_arg_default:
    case PY_list_if:
    case PY_comp_if:
    case PY_decorator:
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
        // A literal default (None/True/False/number/string: no
        // computation code, constant rval) is referenced directly --
        // no global, no initializing MOVE. The MOVE lands in the
        // stream where the `def` executes, which for a METHOD is the
        // class body's ___init___ closure -- and gen_class_pyda only
        // CALLS that closure for Type_RECORD classes. For the core
        // non-record builtins (list, tuple, str, ...) it never runs,
        // so the default global stayed bottom in FA and any call
        // relying on the default died with NOTYPE inside
        // default_wrapper's forwarding send (issue 025:
        // `[3,1,2].sort()` failed while `.sort(None, False)` worked).
        // Computed defaults (e.g. `size=-1`, a negate send) keep the
        // global+MOVE path: they genuinely need def-time evaluation,
        // which non-record-builtin methods simply can't have today.
        if (!a->code && a->rval && (a->rval->is_constant || a->rval->is_symbol || a->rval == sym_nil ||
                                    a->rval == sym_true || a->rval == sym_false)) {
          a->sym = a->rval;
          continue;
        }
        if1_gen(if1, &ast->code, a->code);
        Sym *g = new_sym(ast, 1);
        a->sym = g;
        if1_move(if1, &ast->code, a->rval, g, ast);
      }
  Sym *in = ctx.scope_stack[ctx.scope_stack.n - 2]->in;
  // issues/014: a generator body's reply value is never the user's --
  // it's the raw coroutine-handle int the synthesized wrapper (see
  // build_if1_pyda's PY_funcdef) reads to construct a
  // __pyc_generator__. Give it an int64-typed default here (instead
  // of None) so FA infers an int return type for this Fun regardless
  // of which exit path (fall-through or bare `return`, see
  // PY_return_stmt) is taken; codegen (cg.cc, is_generator) replaces
  // the actual reply mechanics with co_yield/co_return and ignores
  // this value's runtime content entirely.
  //
  // MUST NOT be a literal FA constant (int64_constant(0), tried
  // first): FA faithfully propagates a constant reply value through
  // every caller, including the synthesized wrapper -- collapsing
  // the wrapper's "call the coroutine body, read its handle" step to
  // the same fake constant everywhere downstream (observed:
  // __pyc_generator__.handle always 0 at runtime, never the real
  // handle). Routed through an opaque C call instead (the same IF1
  // shape __pyc_c_call__ produces from Python source, see PY_power's
  // sym___pyc_c_call__ case above) so FA anchors the type without
  // believing it knows the value.
  //
  // issues/014 (infinite generator loops): computed and moved into
  // fn->ret BEFORE the user's body is processed below -- NOT after,
  // unlike the non-generator fall-through-None default further down.
  // A generator body whose own control flow never falls through
  // (`while True: yield i`, no `break`) has no reachable path to
  // whatever comes after the body; appending this there (as it used
  // to) made the placeholder move dead code, and FA infers fn->ret as
  // bottom/NOTYPE with no live move reaching it. The synthesized
  // wrapper's `handle_result = call this_fn(...)` then can't be
  // typed, and constructing __pyc_generator__(handle_result) fails
  // downstream ("matching function not found" at codegen). Placing
  // the move first makes it unconditionally reachable regardless of
  // the generator's own loop shape -- free to do, since this value is
  // never runtime-observed either way.
  Sym *default_ret;
  if (fn->is_generator) {
    int lvl = 0;
    PycSymbol *int_cls_ps = find_PycSymbol(ctx, cannonicalize_string("int"), &lvl);
    Sym *type_arg = (int_cls_ps && int_cls_ps->sym) ? int_cls_ps->sym : sym_int64;
    Code *placeholder_send = if1_send1(if1, &body, ast);
    if1_add_send_arg(if1, placeholder_send, sym_primitive);
    if1_add_send_arg(if1, placeholder_send, sym___pyc_c_call__);
    if1_add_send_arg(if1, placeholder_send, type_arg);
    if1_add_send_arg(if1, placeholder_send, make_string("_CG_generator_placeholder_return"));
    default_ret = new_sym(ast);
    if1_add_send_result(if1, placeholder_send, default_ret);
    placeholder_send->rvals.v[2]->is_fake = 1;
    if1_move(if1, &body, default_ret, fn->ret, ast);
    // issues/014 (infinite generator loops): FA only ever flows a
    // Fun's return type from a LIVE P_prim_reply node (fa.cc's
    // P_prim_reply case flows straight into es->rets -- never visited
    // if the reply itself is unreachable). The move above alone isn't
    // enough: a body whose control flow never falls through to the
    // reply at label[0] (an unconditional `while True:` with no
    // `break`/`return` anywhere -- no CFG edge out of the loop exists
    // at all) leaves that reply dead, so FA never visits it and
    // infers fn->ret as bottom/NOTYPE regardless of the move --
    // breaking the synthesized wrapper's `handle_result = call
    // this_fn(...)` (build_if1_pyda's PY_funcdef) the same way the
    // unfixed placeholder ordering did. (Confirmed as a pure
    // reachability issue, not a value/type mismatch: a loop with a
    // technically-reachable-but-never-taken `break` compiles and
    // runs correctly; an unconditional one didn't, until this fix.)
    //
    // Fix: an opaque conditional branch straight to label[0],
    // structurally present in the CFG so FA can't prune it away --
    // using default_ret itself (the SAME `_CG_generator_placeholder_
    // return()` opaque call above, __pyc_to_bool__-coerced the same
    // way PY_while_stmt/PY_if_stmt coerce any condition) keeps its
    // value exactly as unknowable to FA as it already is for the
    // fn->ret move (that's the whole reason it's routed through an
    // opaque C call rather than a literal constant), so FA can't fold
    // this branch to "always taken" or "never taken" either -- both
    // arms (the jump to label[0], and the fall-through into the
    // user's body below) stay live. At actual runtime the call always
    // literally returns 0 (falsy), so the branch is never really
    // taken -- purely a reachability signal for FA, zero behavior
    // change.
    Sym *never_cond = new_sym(ast);
    call_method(&body, ast, default_ret, sym___pyc_to_bool__, never_cond, 0);
    Code *never_ifcode = if1_if_goto(if1, &body, never_cond, ast);
    if1_if_label_true(if1, never_ifcode, ast->label[0]);
    if1_if_label_false(if1, never_ifcode, if1_label(if1, &body, ast));
  } else {
    default_ret = sym_nil;
  }
  // Process body (may be PY_suite or single statement)
  if (n->children.n >= 3) {
    PyDAST *body_node = n->children[2];
    if (body_node->kind == PY_suite) {
      for (auto c : body_node->children.values()) if1_gen(if1, &body, getAST(c, ctx)->code);
    } else {
      if1_gen(if1, &body, getAST(body_node, ctx)->code);
    }
  }
  // The fall-off-the-end default move into fn->ret. Normally this
  // injects None for the implicit-return path (CPython semantics).
  //
  // issue 071 fix option 1 (shedskin-style implicit return), opt-in via
  // `ifa_no_implicit_none`: when a non-generator function has at least
  // one EXPLICIT value `return` (`fun_returns_value`), skip this nil
  // move. Otherwise a scalar-returning function that also falls off the
  // end is typed `T | None`, and pyc cannot lay out a `scalar | None`
  // union in one unboxed field (chess's fatal `bool | None` closure
  // field). Dropping the nil arm leaves fn->ret as the union of the
  // explicit returns only (the fall-through arm contributes nothing) --
  // the SAME principle goto_exc_target already applies on the raise
  // edge (see its `!fun_returns_value` guard, python_ifa_build_if1.cc):
  // a nil arm here manufactures a spurious {result, nil} union. Cost: a
  // program that actually reaches the end and USES the None replies an
  // arbitrary-but-typed value instead of None -- a deliberate CPython
  // divergence, matching shedskin (which fills the path with the return
  // type's default). Not applied to bare `return`/`return None` (those
  // are explicit and route through PY_return_stmt, not this default).
  //
  // issues/014: generators already got their placeholder moved into
  // fn->ret above, before the body -- unconditionally reachable, so
  // this fall-through move would just be a redundant (same value,
  // same type) second write, dead code whenever the body's own
  // control flow doesn't fall through. Skipped for them entirely.
  if (!fn->is_generator && !(ifa_no_implicit_none && fn->fun_returns_value))
    if1_move(if1, &body, default_ret, fn->ret, ast);
  if1_label(if1, &body, ast, ast->label[0]);
  if1_send(if1, &body, 4, 0, sym_primitive, sym_reply, fn->cont, fn->ret)->ast = ast;
  Vec<Sym *> as;
  Sym *cls = ast->closure_cls;
  // issue 027 feature: @staticmethod/@classmethod use the VALUE
  // convention despite living in a class body -- no receiver formal,
  // no name-symbol dispatch placeholder. Python semantics: neither
  // receives an instance; @classmethod's first formal (cls) is an
  // ordinary parameter that call sites fill with the class value
  // (dispatch is resolved statically at the class-qualified call site
  // in build_if1_pyda, so no receiver specialization is needed).
  bool is_method = in && !in->is_fun && !ast->is_staticmethod && !ast->is_classmethod;
  if (cls) {
    // issues/001: this nested def captures enclosing-function locals --
    // fn->self was already created and specialized against the
    // closure-carrier class in PY_funcdef's build_if1_pyda case (before
    // the body above was walked); reuse it as as[0] here, mirroring
    // gen_lambda_pyda's identical pattern.
    as.add(fn->self);
  } else if (is_method) {
    as.add(new_sym(ast));
    as[0]->must_implement_and_specialize(if1_make_symbol(if1, ast->rval->name));
  } else {
    // issues/007 split identity: a plain (non-method) def is now a
    // first-class function value bound to its public-name variable
    // via if1_move; call sites read the variable and call the value.
    // Use the lambda convention (as[0] IS the function Sym) so the
    // pattern matcher recognizes value-carried applications, instead
    // of the name-symbol dispatch placeholder methods use.
    as.add(fn);
  }
  get_syms_args_pyda(ast, varargsl, as, ctx);
  if (!cls && is_method) {
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
  Sym *cls = ast->closure_cls;
  if (cls) {
    // issues/001: this lambda captures enclosing-function locals. fn->self
    // was already created and specialized against the closure-carrier
    // class in PY_lambda's build_if1_pyda case (before the body above was
    // walked, since PY_name's self.field rewrite for a captured-name
    // reference needs it to already exist at that point) -- reuse it here
    // as as[0], mirroring gen_class_pyda's __call__ wrapper: as[0] is a
    // dispatch placeholder matched against the callee's own type at call
    // sites, rather than `fn` self-identifying as the callee the way a
    // non-capturing lambda does below.
    as.add(fn->self);
  } else {
    as.add(fn);
  }
  get_syms_args_pyda(ast, varargsl, as, ctx);
  if1_closure(if1, fn, body, as.n, as.v);
}

// issues/029 helper: collect the names of instance fields assigned as
// `self.NAME = ...` (or augmented / tuple-unpacked) anywhere in a
// class body's AST. Instance fields are NOT in cls->has at build time
// -- FA promotes them lazily (promote_field) as accesses are analyzed
// -- but the synthesized __deepcopy__ needs the field list BEFORE
// analysis, and for pyc's Python subset (no setattr) the stores are
// fully visible syntactically. `self` by name: pyc code (and the
// corpus) uses the conventional receiver name; a method receiver
// named otherwise just leaves its stores un-deep-copied (shallow,
// the pre-029 behavior).
static void collect_self_store_fields(PyDAST *n, Vec<cchar *> &fields) {
  if (!n) return;
  // NB PY_augassign doubles as the 0-children OPERATOR node inside
  // an augassign statement (python.g) -- only the statement form has
  // a target to inspect.
  if ((n->kind == PY_assign || n->kind == PY_augassign) && n->children.n > 1) {
    int ntgt = (n->kind == PY_assign) ? n->children.n - 1 : 1;
    auto add_if_self_attr = [&](PyDAST *tgt) {
      if (tgt && tgt->kind == PY_power && tgt->children.n == 2 && tgt->children[0]->kind == PY_name &&
          tgt->children[0]->str_val && !strcmp(tgt->children[0]->str_val, "self") &&
          tgt->children[1]->kind == PY_attribute && tgt->children[1]->children.n &&
          tgt->children[1]->children[0]->str_val)
        {
          cchar *cn = if1_cannonicalize_string(if1, tgt->children[1]->children[0]->str_val);
          if (!fields.in(cn)) fields.add(cn);  // insertion order (layout-relevant), not set/hash order
        }
    };
    for (int i = 0; i < ntgt; i++) {
      PyDAST *tgt = n->children[i];
      if (tgt && (tgt->kind == PY_tuple || tgt->kind == PY_testlist || tgt->kind == PY_exprlist)) {
        for (auto c : tgt->children.values()) add_if_self_attr(c);
      } else {
        add_if_self_attr(tgt);
      }
    }
  }
  for (auto c : n->children.values()) collect_self_store_fields(c, fields);
}

// issues/078: does `n` (an expression subtree) reference the name
// `self` anywhere? Used below to check whether an __init__ field
// assignment's RHS is self-independent -- if it read `self`, treating
// the corresponding class-body default as safely elidable from the
// clone step isn't sound to reason about with this conservative,
// purely-syntactic check.
static bool pyast_references_self(PyDAST *n) {
  if (!n) return false;
  if (n->kind == PY_name && n->str_val && !strcmp(n->str_val, "self")) return true;
  for (auto c : n->children.values())
    if (pyast_references_self(c)) return true;
  return false;
}

// issues/078: is `stmt` a `self.NAME = <expr>` statement (same target
// shape as collect_self_store_fields) whose RHS doesn't read `self`?
// If so, return NAME (canonicalized); otherwise nullptr. Such a
// statement unconditionally overwrites NAME with a value that can't
// itself depend on NAME's (or any other field's) prior value -- a
// necessary condition for treating the __new__ clone step's copy of
// NAME into a fresh instance as dead.
static cchar *simple_self_field_overwrite(PyDAST *stmt) {
  if (!stmt || stmt->kind != PY_assign || stmt->children.n != 2) return nullptr;
  PyDAST *tgt = stmt->children[0], *val = stmt->children[1];
  if (!(tgt->kind == PY_power && tgt->children.n == 2 && tgt->children[0]->kind == PY_name &&
        tgt->children[0]->str_val && !strcmp(tgt->children[0]->str_val, "self") &&
        tgt->children[1]->kind == PY_attribute && tgt->children[1]->children.n &&
        tgt->children[1]->children[0]->str_val))
    return nullptr;
  if (pyast_references_self(val)) return nullptr;
  return if1_cannonicalize_string(if1, tgt->children[1]->children[0]->str_val);
}

// issues/078: is `__init__`'s ENTIRE body just a sequence of
// `self.NAME = <self-independent expr>` statements (plus `pass`)?
// This is a conservative stand-in for a full "definitely reassigned
// before any use" dominance analysis -- sufficient to cover the known
// real cases (dict.__init__, set.__init__, and issue 078's MiniDict
// repro all have exactly this shape) without attempting one. When
// true, every field named this way is collected into `out`: on every
// real construction (__new__ clones the prototype, then unconditionally
// calls __init__ on the clone, no branching in between), the clone
// step's copy of NAME is immediately overwritten before the resulting
// instance is observable anywhere. Any unrecognized statement (a
// conditional, a loop, a call, a self-referencing RHS, ...) bails the
// WHOLE analysis, leaving `out` empty: a bail means we can no longer
// prove any single field is safe (e.g. an early return inside a loop
// could skip a later assignment this walk would otherwise have
// credited), so partial credit isn't sound. NB this says nothing
// about the PROTOTYPE's own field (still seeded normally by the
// class-body statement, untouched by this analysis) -- only about
// what a freshly cloned instance's field should be credited with; see
// issue 078's "Option D" for why that distinction is load-bearing.
static void compute_init_elidable_fields(PyDAST *init_def, Vec<cchar *> &out) {
  if (!init_def || init_def->children.n < 3) return;
  PyDAST *ibody = init_def->children[2];
  Vec<PyDAST *> stmts;
  if (ibody->kind == PY_suite)
    for (auto c : ibody->children.values()) stmts.add(c);
  else
    stmts.add(ibody);
  for (auto stmt : stmts.values()) {
    if (stmt->kind == PY_pass_stmt) continue;
    cchar *field = simple_self_field_overwrite(stmt);
    if (!field) {
      out.clear();
      return;
    }
    if (!out.in(field)) out.add(field);
  }
}

// issues/023: read back a class-body `__match_args__ = ("x", "y")`
// literal at compile time -- positional class patterns
// (`case Point(0, 0):`) need to map position -> attribute name, and
// PEP 634 requires that mapping to come from an explicit
// __match_args__, never guessed from __init__'s parameter list.
// Unlike collect_self_store_fields, this only looks at the class
// body's OWN top-level statements (no recursion into methods --
// __match_args__ is a class attribute, never meaningfully assigned
// inside one). String literals are read the same way the
// @vector("s") decorator argument already is (strip the raw
// source's surrounding quote char) -- there's no runtime value to
// evaluate here, this runs before build_if1 does anything.
static void collect_match_args(PyDAST *cdef, Vec<cchar *> &out) {
  PyDAST *body_node = cdef->children.last();
  Vec<PyDAST *> stmts;
  if (body_node->kind == PY_suite)
    for (auto c : body_node->children.values()) stmts.add(c);
  else
    stmts.add(body_node);
  for (auto n : stmts.values()) {
    if (n->kind != PY_assign || n->children.n != 2) continue;
    PyDAST *tgt = n->children[0], *val = n->children[1];
    if (tgt->kind != PY_name || !tgt->str_val || strcmp(tgt->str_val, "__match_args__")) continue;
    if (val->kind != PY_tuple && val->kind != PY_list) continue;
    Vec<cchar *> names;
    bool ok = true;
    for (auto elt : val->children.values()) {
      if (elt->kind != PY_string || !elt->str_val) { ok = false; break; }
      cchar *s = elt->str_val;
      int len = strlen(s);
      char *inner = (char *)MALLOC(len + 1);
      if (len >= 2 && (s[0] == '"' || s[0] == '\'')) {
        strncpy(inner, s + 1, len - 2);
        inner[len - 2] = 0;
      } else {
        strcpy(inner, s);
      }
      names.add(if1_cannonicalize_string(if1, inner));
    }
    if (ok) out = names;  // last __match_args__ statement wins, matching Python's own assignment semantics
  }
}

// issue 068: synthesize one derived comparison method `opname` on record
// `cls` -- the class side of the derive / field-fold framework, and the
// binary generalization of the synthesized __deepcopy__. Skipped if the
// class defines its own. Every method is built from ORDINARY sends
// (period-gets + the field's own comparison + bool combinators), so field
// comparison rides normal demand-driven dispatch -- no primitive, no
// inline codegen, unlike the tuple side (issue 067). Shapes:
//   __eq__:  AND fold        r = True;  r = r & (self.f == other.f)
//   __lt__:  lexicographic   r = False; r = (self.f < other.f) | ((self.f == other.f) & r)   [fields reversed]
//   __ne__/__gt__/__le__/__ge__: delegate to derived __eq__/__lt__ (mirrors tuple's reflected ops)
// Registration mirrors the synthesized __deepcopy__.
static void synthesize_derived_compare(PycCompiler &ctx, PyDAST *cdef, PycAST *ast, Sym *cls, Sym *fn,
                                       Code **classbody, cchar *opname) {
  if (ctx.scope_stack.last()->map.get(if1_cannonicalize_string(if1, opname))) return;  // user-defined
  Sym *mfn = new_fun(ast);
  mfn->nesting_depth = fn->nesting_depth + 1;
  mfn->self = new_sym(ast);
  mfn->self->must_implement_and_specialize(cls);
  mfn->self->in = mfn;
  Sym *other = new_sym(ast);  // second formal, any type
  other->in = mfn;
  Vec<Sym *> as;
  as.add(new_sym(ast, opname));
  as[0]->must_implement_and_specialize(if1_make_symbol(if1, opname));
  mfn->name = as[0]->name;
  as.add(mfn->self);
  as.add(other);
  Sym *self = mfn->self;
  Code *b = 0;
  Sym *res = nullptr;
  bool eq = !strcmp(opname, "__eq__"), lt = !strcmp(opname, "__lt__");
  if (eq || lt) {
    Vec<cchar *> fields;  // source order (same rationale as __deepcopy__)
    collect_self_store_fields(cdef, fields);
    for (int i = 0; i < cls->has.n; i++) {
      Sym *m = cls->has[i];
      if (!m || !m->name) continue;
      if (m->alias && m->alias->is_fun) continue;  // methods, not data
      cchar *cn = if1_cannonicalize_string(if1, m->name);
      if (!fields.in(cn)) fields.add(cn);
    }
    // __lt__ folds fields in REVERSE so the outermost term is the first
    // field (lexicographic); __eq__'s AND is order-free.
    if (lt)
      for (int i = 0, j = fields.n - 1; i < j; i++, j--) {
        cchar *t = fields[i];
        fields[i] = fields[j];
        fields[j] = t;
      }
    Sym *r = eq ? sym_true : sym_false;
    for (cchar *fname : fields) {
      Sym *nm = if1_make_symbol(if1, fname);
      Sym *sv = new_sym(ast), *ov = new_sym(ast);
      if1_send(if1, &b, 4, 1, sym_operator, self, sym_period, nm, sv)->ast = ast;
      if1_send(if1, &b, 4, 1, sym_operator, other, sym_period, nm, ov)->ast = ast;
      if (eq) {
        Sym *c = new_sym(ast), *rn = new_sym(ast);
        call_method(&b, ast, sv, if1_make_symbol(if1, "__eq__"), c, 1, ov);
        call_method(&b, ast, r, if1_make_symbol(if1, "__and__"), rn, 1, c);  // r = r and (self.f == other.f)
        r = rn;
      } else {
        Sym *cl = new_sym(ast), *ce = new_sym(ast), *t = new_sym(ast), *rn = new_sym(ast);
        call_method(&b, ast, sv, if1_make_symbol(if1, "__lt__"), cl, 1, ov);  // self.f < other.f
        call_method(&b, ast, sv, if1_make_symbol(if1, "__eq__"), ce, 1, ov);  // self.f == other.f
        call_method(&b, ast, ce, if1_make_symbol(if1, "__and__"), t, 1, r);   // (self.f == other.f) and r
        call_method(&b, ast, cl, if1_make_symbol(if1, "__or__"), rn, 1, t);   // (self.f < other.f) or t
        r = rn;
      }
    }
    res = r;
  } else if (!strcmp(opname, "__ne__")) {
    Sym *e = new_sym(ast);
    call_method(&b, ast, self, if1_make_symbol(if1, "__eq__"), e, 1, other);  // self == other
    res = new_sym(ast);
    call_method(&b, ast, e, if1_make_symbol(if1, "__not__"), res, 0);  // not (...)
  } else if (!strcmp(opname, "__gt__")) {
    res = new_sym(ast);
    call_method(&b, ast, other, if1_make_symbol(if1, "__lt__"), res, 1, self);  // other < self
  } else if (!strcmp(opname, "__le__")) {
    Sym *x = new_sym(ast);
    call_method(&b, ast, other, if1_make_symbol(if1, "__lt__"), x, 1, self);  // other < self
    res = new_sym(ast);
    call_method(&b, ast, x, if1_make_symbol(if1, "__not__"), res, 0);  // not (...)
  } else if (!strcmp(opname, "__ge__")) {
    Sym *x = new_sym(ast);
    call_method(&b, ast, self, if1_make_symbol(if1, "__lt__"), x, 1, other);  // self < other
    res = new_sym(ast);
    call_method(&b, ast, x, if1_make_symbol(if1, "__not__"), res, 0);  // not (...)
  } else {
    return;  // unknown op
  }
  if1_move(if1, &b, res, mfn->ret);
  if1_send(if1, &b, 4, 0, sym_primitive, sym_reply, mfn->cont, mfn->ret)->ast = ast;
  if1_closure(if1, mfn, b, as.n, as.v);
  Sym *member = new_PycSymbol(opname)->sym;
  member->var = new Var(member);
  member->alias = mfn;
  member->in = cls;
  cls->has.add(member);
  if1_send(if1, classbody, 5, 1, sym_operator, fn->self, sym_setter, if1_make_symbol(if1, opname), mfn, new_sym(ast))
      ->ast = ast;
}

// issue 034: CPython's data model falls back from `__i<op>__` to
// `__<op>__` (then reassigns) when a class defines the latter but not
// the former -- `c += x` on a class with only `__add__` is exactly
// `c = c.__add__(x)`, not an error. pyc's augmented-assignment
// lowering (PY_augassign, python_ifa_build_if1.cc) always sends
// `__i<op>__` directly with no such fallback, so ANY class defining
// only the non-in-place dunder (the common case -- most classes never
// bother writing a separate in-place method solely for `+=`) hit an
// unconditionally unresolvable call, degrading to a runtime assert
// (found via shedskin_examples/yopyra/yopyra.py's `color`/`punto3d`,
// which each define `__add__` alone). Unlike synthesize_derived
// _compare (opt-in, `@pyc_compare` -- Python's REAL default for
// `__eq__`/ordering is identity/unimplemented, so synthesizing them
// unconditionally would change semantics), this reproduces CPython's
// own UNCONDITIONAL default behavior, so it always runs when the
// asymmetry exists -- there is no "opt out" of `+=` falling back to
// `+` in real Python. `iopname`'s check mirrors
// synthesize_derived_compare's "skipped if the class defines its
// own" (own-scope only, not inherited -- same precedent, same
// rationale: this file's existing pattern for this exact question).
static void synthesize_default_iop(PycCompiler &ctx, PycAST *ast, Sym *cls, Sym *fn, Code **classbody,
                                    cchar *iopname, cchar *opname) {
  if (ctx.scope_stack.last()->map.get(if1_cannonicalize_string(if1, iopname))) return;  // user-defined __i<op>__
  if (!ctx.scope_stack.last()->map.get(if1_cannonicalize_string(if1, opname))) return;   // no __<op>__ to fall back to
  Sym *mfn = new_fun(ast);
  mfn->nesting_depth = fn->nesting_depth + 1;
  mfn->self = new_sym(ast);
  mfn->self->must_implement_and_specialize(cls);
  mfn->self->in = mfn;
  Sym *other = new_sym(ast);  // second formal, any type
  other->in = mfn;
  Vec<Sym *> as;
  as.add(new_sym(ast, iopname));
  as[0]->must_implement_and_specialize(if1_make_symbol(if1, iopname));
  mfn->name = as[0]->name;
  as.add(mfn->self);
  as.add(other);
  Code *b = 0;
  Sym *res = new_sym(ast);
  call_method(&b, ast, mfn->self, if1_make_symbol(if1, opname), res, 1, other);  // return self.__<op>__(other)
  if1_move(if1, &b, res, mfn->ret);
  if1_send(if1, &b, 4, 0, sym_primitive, sym_reply, mfn->cont, mfn->ret)->ast = ast;
  if1_closure(if1, mfn, b, as.n, as.v);
  Sym *member = new_PycSymbol(iopname)->sym;
  member->var = new Var(member);
  member->alias = mfn;
  member->in = cls;
  cls->has.add(member);
  if1_send(if1, classbody, 5, 1, sym_operator, fn->self, sym_setter, if1_make_symbol(if1, iopname), mfn, new_sym(ast))
      ->ast = ast;
}

void gen_class_pyda(PyDAST *cdef, PycAST *ast, PycCompiler &ctx, char *vector_size, bool derive_compare) {
  // cdef is the PY_classdef node
  Sym *fn = ast->rval, *cls = ast->sym;
  bool is_record = cls->type_kind == Type_RECORD && cls != sym_object;
  Code *body = 0;
  // issues/023: __match_args__ for positional class patterns
  // (case Point(0, 0):). Own class body first; if it declares none,
  // fall back to the first direct base that has some -- classes are
  // processed in program order (pass 1 already validated every base
  // is a real class before this pass-2 function runs for ANY class),
  // so a base named in cls->includes here already carries its OWN
  // fully-resolved (declared-or-inherited) match_args.
  collect_match_args(cdef, cls->match_args);
  for (int i = 0; i < cls->includes.n && !cls->match_args.n; i++)
    if (cls->includes[i]->match_args.n) cls->match_args = cls->includes[i]->match_args;
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
  // issues/029: synthesize a recursive __deepcopy__ for every record
  // class that doesn't define its own. The class's data layout is
  // fully known here (build_syms discovered every `self.x = ...`
  // member in pass 1), so the method is exactly what a user would
  // write by hand: shallow-clone self, then re-point each DATA
  // member (methods -- has-entries whose alias is a fun, including
  // the prototype's method-pointer slots -- are skipped, same
  // discriminator as the `includes` copy loop above) at
  // member.__deepcopy__(). Field recursion rides normal method
  // dispatch: lists via list.__deepcopy__, nested records via THEIR
  // synthesized method (each level gets a monomorphic contour via
  // recursive-ES splitting, issues/025 R1 item 5), scalars/strings/
  // tuples via __pyc_any_type__'s shallow fallback, None via
  // __pyc_None_type__'s identity. FA is demand-driven: classes
  // never deep-copied pay nothing. Three registrations make it a
  // real method (each was independently necessary): the closure
  // with as[0] must_implement_and_specialize'd on the selector
  // symbol (pattern matching), a member sym in cls->has with
  // alias = the fn (period dispatch walks has), and a prototype
  // field-install in THIS class-body init (mirrors what a def
  // statement's setter emits) so instances carry the method value.
  // No memo table (v1): CPython's memo preserves shared/cyclic
  // structure; pyc duplicates diamonds and does not terminate on
  // cycles -- the corpus need (genetic2's genome TREES) is trees.
  // Inherited (includes) fields are shallow-cloned but not
  // deep-recursed (v1).
  if (is_record && !ctx.scope_stack.last()->map.get(if1_cannonicalize_string(if1, "__deepcopy__"))) {
    // Field list = syntactic `self.NAME = ...` stores (instance
    // fields; NOT in cls->has until FA promotes them) plus the
    // build-time class-body data attributes already in has. Kept in
    // FIRST-STORE SOURCE ORDER, deliberately NOT sorted: struct slot
    // numbers follow cls->has, which promote_field appends to in FA
    // analysis order -- with __init__ promoting fields in write
    // order and this method promoting in ITS body order, the two
    // must AGREE or whichever contour the (heap-order-sensitive)
    // worklist analyzes first decides the layout: an alphabetized
    // loop here made field/slot assignment differ across identical
    // compiles (the determinism gate caught it on
    // tests/deepcopy_objects.py).
    Vec<cchar *> sorted_fields;  // source order despite the name
    collect_self_store_fields(cdef, sorted_fields);
    for (int i = 0; i < cls->has.n; i++) {
      Sym *m = cls->has[i];
      if (!m || !m->name) continue;
      if (m->alias && m->alias->is_fun) continue;  // methods, not data
      cchar *cn = if1_cannonicalize_string(if1, m->name);
      if (!sorted_fields.in(cn)) sorted_fields.add(cn);
    }
    Sym *dcfn = new_fun(ast);
    // One deeper than the class-body init fn, NOT ctx.scope_stack.n:
    // during an IMPORTED module's build_if1 the importer's scopes sit
    // under the imported module's on the stack, so scope_stack.n
    // over-counts -- and dcfn is referenced as a VALUE inside the
    // class-body init (the prototype install below), where a
    // too-deep nesting_depth walks a display the init fn doesn't
    // have (unique_AVar `es` assert on tests/from_import.py).
    dcfn->nesting_depth = fn->nesting_depth + 1;
    dcfn->self = new_sym(ast);
    dcfn->self->must_implement_and_specialize(cls);
    dcfn->self->in = dcfn;
    Vec<Sym *> as;
    as.add(new_sym(ast, "__deepcopy__"));
    as[0]->must_implement_and_specialize(if1_make_symbol(if1, "__deepcopy__"));
    dcfn->name = as[0]->name;
    as.add(dcfn->self);
    Code *dcbody = 0;
    Sym *t = new_sym(ast);
    if1_send(if1, &dcbody, 3, 1, sym_primitive, if1_make_symbol(if1, "copy"), dcfn->self, t)->ast = ast;
    for (cchar *fname : sorted_fields) {
      Sym *nm = if1_make_symbol(if1, fname);
      Sym *fv = new_sym(ast);
      if1_send(if1, &dcbody, 4, 1, sym_operator, dcfn->self, sym_period, nm, fv)->ast = ast;
      Sym *dv = new_sym(ast);
      call_method(&dcbody, ast, fv, if1_make_symbol(if1, "__deepcopy__"), dv, 0);
      if1_send(if1, &dcbody, 5, 1, sym_operator, t, sym_setter, nm, dv, new_sym(ast))->ast = ast;
    }
    if1_move(if1, &dcbody, t, dcfn->ret);
    if1_send(if1, &dcbody, 4, 0, sym_primitive, sym_reply, dcfn->cont, dcfn->ret)->ast = ast;
    if1_closure(if1, dcfn, dcbody, as.n, as.v);
    // Class-member registration + prototype install (same shapes as
    // the inherited-method branch of the includes loop above).
    Sym *member = new_PycSymbol("__deepcopy__")->sym;
    member->var = new Var(member);
    member->alias = dcfn;
    member->in = cls;
    cls->has.add(member);
    if1_send(if1, &body, 5, 1, sym_operator, fn->self, sym_setter, if1_make_symbol(if1, "__deepcopy__"), dcfn,
             new_sym(ast))
        ->ast = ast;
  }

  // issue 068: @pyc_compare derives the record comparison family as
  // field-folds of ORDINARY sends (see synthesize_derived_compare). Opt-in
  // (Python classes default to identity __eq__ and no ordering); each op is
  // skipped when the class defines its own. __eq__/__lt__ are folds;
  // __ne__/__gt__/__le__/__ge__ delegate to them (order matters not for
  // synthesis -- dispatch resolves at FA time). Matches CPython
  // dataclass(order=True) + functools.total_ordering (see pyc_compat).
  if (derive_compare && is_record) {
    synthesize_derived_compare(ctx, cdef, ast, cls, fn, &body, "__eq__");
    synthesize_derived_compare(ctx, cdef, ast, cls, fn, &body, "__lt__");
    synthesize_derived_compare(ctx, cdef, ast, cls, fn, &body, "__ne__");
    synthesize_derived_compare(ctx, cdef, ast, cls, fn, &body, "__gt__");
    synthesize_derived_compare(ctx, cdef, ast, cls, fn, &body, "__le__");
    synthesize_derived_compare(ctx, cdef, ast, cls, fn, &body, "__ge__");
  }
  // issue 034: unconditional (not opt-in, see synthesize_default_iop's
  // own comment) -- every class that defines the non-in-place operator
  // alone gets CPython's real default `__i<op>__` = `__<op>__` fallback.
  if (is_record) {
    synthesize_default_iop(ctx, ast, cls, fn, &body, "__iadd__", "__add__");
    synthesize_default_iop(ctx, ast, cls, fn, &body, "__isub__", "__sub__");
    synthesize_default_iop(ctx, ast, cls, fn, &body, "__imul__", "__mul__");
    synthesize_default_iop(ctx, ast, cls, fn, &body, "__itruediv__", "__truediv__");
    synthesize_default_iop(ctx, ast, cls, fn, &body, "__imod__", "__mod__");
    synthesize_default_iop(ctx, ast, cls, fn, &body, "__ipow__", "__pow__");
    synthesize_default_iop(ctx, ast, cls, fn, &body, "__ilshift__", "__lshift__");
    synthesize_default_iop(ctx, ast, cls, fn, &body, "__irshift__", "__rshift__");
    synthesize_default_iop(ctx, ast, cls, fn, &body, "__ior__", "__or__");
    synthesize_default_iop(ctx, ast, cls, fn, &body, "__ixor__", "__xor__");
    synthesize_default_iop(ctx, ast, cls, fn, &body, "__iand__", "__and__");
    synthesize_default_iop(ctx, ast, cls, fn, &body, "__ifloordiv__", "__floordiv__");
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
  // issue 078 (Option D): if this class defines its OWN __init__
  // whose entire body is safe, self-independent field-literal
  // assignments (compute_init_elidable_fields), record which fields
  // fa.cc's structural_assignment (P_prim_clone) may skip copying
  // when cloning FROM this exact prototype Sym -- structurally, that
  // can only ever be the __new__-synthesized `clone(proto, t)` below,
  // never a user clone() call (the prototype Sym is never reachable
  // from Python source). This does NOT touch the class-body statement
  // loop above -- the prototype's OWN field is still seeded normally,
  // so the inherited-field copy loop above (for subclasses) and
  // direct `ClassName.attr` reads still see a real value. Scope
  // deliberately conservative for a first cut: vector classes (clone
  // via sym_clone_vector, a different primitive) and inherited-only
  // __init__ (no OWN textual funcdef here) are excluded -- see issue
  // 078's "Option D" writeup for why.
  if (is_record && !cls->is_vector) {
    PyDAST *body_node = cdef->children.last();
    if (body_node->kind == PY_suite)
      for (auto c : body_node->children.values()) {
        PyDAST *d = (c->kind == PY_decorated) ? c->children.last() : c;
        if (d->kind == PY_funcdef && d->children[0]->str_val && !strcmp(d->children[0]->str_val, "__init__")) {
          compute_init_elidable_fields(d, proto->clone_elides_fields);
          break;
        }
      }
  }
  // Find __init__: own scope first, else inherited. A `pass`-only
  // subclass has no OWN scope entry -- this used to fall straight to
  // the trivial "return self" synthesis below regardless of an
  // inherited __init__, so the __new__ wrapper built its formal-
  // parameter list (below, from init_sym->has) with ZERO parameters
  // beyond self: a real inherited __init__'s args were silently
  // dropped at every call site (`Derived("hi")` compiled but
  // "hi" reached nothing, msg defaulting instead). The actual
  // dispatch already goes through the polymorphic __init__ selector
  // send (below) and correctly resolves to the inherited
  // implementation via the `includes` copy-into-`has` above --
  // only the wrapper's PARAMETER LIST needed the real Sym.
  PycSymbol *init_fun = ctx.scope_stack.last()->map.get(sym___init__->name);
  Sym *init_sym = init_fun ? init_fun->sym->alias : 0;
  if (!init_sym) {
    for (int i = 0; i < cls->includes.n && !init_sym; i++) {
      Sym *inc = cls->includes[i];
      for (int j = 0; j < inc->has.n; j++) {
        if (inc->has[j]->name == sym___init__->name && inc->has[j]->alias && inc->has[j]->alias->is_fun) {
          init_sym = inc->has[j]->alias;
          break;
        }
      }
    }
  }
  if (!init_sym) {
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
      // Name each __new__-wrapper formal after the corresponding
      // __init__ parameter so keyword arguments to a constructor
      // (`T(a=1, b=2)`) can bind by name. Without a name the formal has
      // no entry in named_to_positional (pattern.cc build_arg_position),
      // so the matcher can't place a keyword actual and the whole
      // constructor call fails to resolve ("'t' has no type"). A null
      // param name (e.g. *args) degrades to an unnamed formal as before.
      // issue 025 bucket A: timsort's `Timsort(list_, comparefn=comparefn)`.
      //
      // Also propagate clone_for_constants from the __init__ param to
      // the wrapper formal: constant-driven contour separation must
      // start at __new__ (where the instance CS is created via
      // sym_clone -- one CS per __new__ contour), or every caller's
      // constants merge in one shared __new__ ES and the per-constant
      // __init__ clones all write into the SAME instance CS, unioning
      // the fields anyway. Issue 040/043: `range(0, 0)` vs
      // `range(0, 2)` from list.__str__'s per-receiver clones must
      // produce distinct range CSs so `self.i < self.j` folds false
      // for the empty clone and its dead loop body is pruned.
      for (int i = 2; i < init_sym->has.n; i++) {
        Sym *wf = new_sym(ast, init_sym->has[i]->name);
        wf->clone_for_constants = init_sym->has[i]->clone_for_constants;
        // A constant-cloned ctor param also marks the CLASS for
        // per-receiver-CS method contours (ifa/issues/045), plus the
        // __new__ wrapper and __init__ Fun syms for HARD per-constant
        // contour separation (entry_set_compatibility): the
        // per-constant instance CSs this creates are only useful if
        // the class's methods split per CS too -- otherwise shared
        // method contours write through the union and widen every
        // sibling's fields (issue 040's range trace).
        if (wf->clone_for_constants) {
          cls->clone_methods_per_cs = 1;
          fn->clone_methods_per_cs = 1;
          init_sym->clone_methods_per_cs = 1;
        }
        as.add(wf);
      }
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
  // ifa/issues/091: give int/float/bool/list/tuple a real, storable
  // ZERO-ARG __new__ candidate -- attached to the class's meta_type
  // via must_implement_and_specialize, the exact same registration
  // dict/set's real __new__ wrapper uses just above (the `is_record`
  // loop). Because it's the SAME registration mechanism, the existing
  // generic "call a stored value" dispatch (build_if1_pyda's plain-
  // call SEND on cur_val, python_ifa_build_if1.cc -- unchanged by
  // this fix) finds it automatically: `factory = dict; factory()`
  // already works today by resolving dict's meta_type __new__ through
  // this exact path, and `factory = int; factory()` was only failing
  // because int had no __new__ there to find, not because the
  // dispatch mechanism itself needed anything new.
  //
  // This is deliberately NOT the same shape as the reverted 2026-08-10
  // attempt (see issues/091's writeup): that one substituted the Sym
  // every bare load of int/float/bool/list/tuple resolved to, which
  // also hit every isinstance(x, list)-style type-descriptor use of
  // the same name (__pyc__/02_numeric.py's own `isinstance(x, list)`
  // among them) and broke broadly. This only ADDS a __new__ candidate
  // reachable via the meta_type -- the class Sym itself, and every
  // existing use of it (isinstance, the direct-call-site fast paths
  // below), is completely untouched.
  //
  // Zero-arg only: scoped to the issue's actual motivating case
  // (`defaultdict(int)`/`defaultdict(list)` -- pyc_lib/collections.py's
  // `self.factory()`) without attempting the 1-arg/2-arg conversion
  // forms (int(x), int(x,base), list(x), tuple(x) stored-and-called
  // indirectly) that build_if1.cc's existing direct-call-site special
  // cases already give the DIRECT-call shape -- those match earlier
  // in build_builtin_call_pyda and return before any generic dispatch
  // is even considered, so they're untouched by this either way.
  // Reuses the exact literal/primitive each fast path already
  // produces for its own zero-arg case (build_if1.cc's `if (f &&
  // pos_args.n == 0)` block).
  if (!is_record) {
    // `cls` here is `ast->sym` from PY_classdef processing
    // (build_syms_pyda), which for `class int:`/`class float:`
    // specifically is ALREADY unalias_type()'d (Type_ALIAS ->
    // underlying concrete type -- ifa/if1/sym.cc) to `sym_int64`/
    // `sym_float64` directly, name "int64"/"float64", not "int"/
    // "float" (confirmed empirically: matching cls->name against
    // "int" here never fired). bool/list/tuple aren't Type_ALIAS, so
    // their classdef's `cls` stays pointer-identical to the plain
    // sym_bool/sym_list/sym_tuple globals -- no unaliasing involved,
    // direct pointer comparison is correct as-is.
    bool is_int_ctor = cls->name && !strcmp(cls->name, "int64");
    bool is_float_ctor = cls->name && !strcmp(cls->name, "float64");
    if (is_int_ctor || is_float_ctor || cls == sym_bool || cls == sym_list || cls == sym_tuple) {
      fn = new_fun(ast);
      fn->nesting_depth = ctx.scope_stack.n;
      Vec<Sym *> as;
      as.add(new_sym(ast, "__new__"));
      as[0]->must_implement_and_specialize(ast->sym->meta_type);
      fn->name = as[0]->name;
      body = 0;
      Sym *t;
      if (is_int_ctor) {
        Immediate imm;
        imm.v_int64 = 0;
        t = if1_const(if1, sym_int64, "0", &imm);
      } else if (is_float_ctor) {
        Immediate imm;
        imm.v_float64 = 0.0;
        t = if1_const(if1, sym_float64, "0", &imm);
      } else if (cls == sym_bool) {
        t = sym_false;
      } else {
        t = new_sym(ast);
        if1_send(if1, &body, 3, 1, sym_primitive, sym_make, cls, t)->ast = ast;
      }
      if1_move(if1, &body, t, fn->ret);
      if1_send(if1, &body, 4, 0, sym_primitive, sym_reply, fn->cont, fn->ret)->ast = ast;
      if1_closure(if1, fn, body, as.n, as.v);
    }
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

// `len`: explicit byte length when `s` may contain an embedded NUL (a
// str/bytes literal decoded from a \x00/\0 escape, ifa/issues/070); -1
// (default) means "not provided, if1_const falls back to strlen(s)".
Sym *make_string(cchar *s, int len) {
  Immediate imm;
  imm.v_string = s;
  Sym *sym = if1_const(if1, sym_string, s, &imm, 0, len);
  return sym;
}

Sym *make_bytes(cchar *s, int len) {
  Immediate imm;
  imm.v_string = s;
  Sym *sym = if1_const(if1, sym_bytes, s, &imm, 0, len);
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

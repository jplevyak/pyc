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

// True for a real PycSymbol pointer (not one of the scope sentinels
// GLOBAL_USE/NONLOCAL_USE/GLOBAL_DEF/NONLOCAL_DEF, which are small
// integers cast to PycSymbol*).
static inline bool is_real_pycsymbol(PycSymbol *s) { return (intptr_t)s > (intptr_t)NONLOCAL_DEF; }

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
    m = get_module(mod, ctx);
  }
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
      m = get_module(top, ctx);
      if (!m) {
        for (auto p : ctx.search_path->values()) {
          if (file_exists(p, "/__init__.py")) continue;
          if (!is_regular_file(p, "/", top, ".py")) continue;
          import_file(top, p, ctx);
          break;
        }
        m = get_module(top, ctx);
      }
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
    return;
  }
  if (n->kind == PY_power && n->children.n == 2 && n->children[1]->kind == PY_call) {
    // Class pattern (`ClassName(attr=pat, ...)`, parsed as an ordinary
    // constructor-call-shaped PY_power/PY_call by build_pattern_match).
    // Only keyword sub-patterns' VALUES can bind -- the class name and
    // the keyword names themselves (`attr` in `attr=pat`) are left
    // untouched, same rationale as PY_dict's keys above: they fall
    // through to the generic recurse as ordinary reads, exactly like
    // an ordinary call's keyword-argument names already do (ordinary
    // `foo(x=1)` calls resolve `x` the same harmless way -- see
    // tests/keyword_args.py).
    PyDAST *call = n->children[1];
    if (call->children.n > 0) {
      PyDAST *arglist = call->children[0];
      for (auto arg : arglist->children.values())
        if (arg->kind == PY_keyword_arg) mark_pattern_captures(arg->children[1]);
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
  if (n->kind == PY_yield_stmt || n->kind == PY_yield_expr) return true;
  if (n->kind == PY_funcdef || n->kind == PY_lambda || n->kind == PY_classdef) return false;
  for (auto c : n->children.values())
    if (pyda_contains_yield(c)) return true;
  return false;
}

// Set up function scope for pyda path
static Sym *def_fun_pyda(PyDAST *n, PycAST *ast, Sym *fn, PycCompiler &ctx) {
  fn->in = ctx.scope_stack.last()->in;
  fn->is_async = n->is_async;
  if (n->kind == PY_funcdef) {
    for (auto c : n->children.values())
      if (pyda_contains_yield(c)) { fn->is_generator = 1; break; }
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
      // Post-classdef: collect base classes and members
      bool any_base = false;
      for (int i = 1; i < cdef->children.n - 1; i++) {
        PyDAST *base_ast = cdef->children[i];
        if (base_ast->kind == PY_tuple) {
          for (int j = 0; j < base_ast->children.n; j++) {
            Sym *base = getAST(base_ast->children[j], ctx)->sym;
            if (!base) fail("error line %d, base not found for class '%s'", ctx.lineno, cdef_ast->sym->name);
            cdef_ast->sym->inherits_add(base);
            any_base = true;
          }
        } else {
          Sym *base = getAST(base_ast, ctx)->sym;
          if (!base) fail("error line %d, base not found for class '%s'", ctx.lineno, cdef_ast->sym->name);
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
    case PY_if_stmt:
    case PY_elif_clause:
    case PY_else_clause:
    case PY_try_stmt:
    case PY_except_clause:
    case PY_except_handler:
    case PY_finally_clause:
    case PY_with_stmt:
    case PY_with_item:
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
    case PY_keyword_arg:
    case PY_star_arg:
    case PY_dstar_arg:
    case PY_arg_default:
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
  } else {
    default_ret = sym_nil;
  }
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
      // Name each __new__-wrapper formal after the corresponding
      // __init__ parameter so keyword arguments to a constructor
      // (`T(a=1, b=2)`) can bind by name. Without a name the formal has
      // no entry in named_to_positional (pattern.cc build_arg_position),
      // so the matcher can't place a keyword actual and the whole
      // constructor call fails to resolve ("'t' has no type"). A null
      // param name (e.g. *args) degrades to an unnamed formal as before.
      // issue 025 bucket A: timsort's `Timsort(list_, comparefn=comparefn)`.
      for (int i = 2; i < init_sym->has.n; i++) as.add(new_sym(ast, init_sym->has[i]->name));
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

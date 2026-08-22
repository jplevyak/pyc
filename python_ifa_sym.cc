// SPDX-License-Identifier: BSD-3-Clause
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
    if (c == sym_bytes) return if1_const(if1, c, "");
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
  new_builtin_primitive_type(sym_list, "list");
  new_builtin_primitive_type(sym_tuple, "tuple");
  new_builtin_primitive_type(sym_ellipsis_type, "Ellipsis_type");
  new_builtin_unique_object(sym_ellipsis, "Ellipsis", sym_ellipsis_type);
  sym_ellipsis_type->is_unique_type = 1;

#define B(_x) builtin_functions.set_add(sym_##_x);
#include "pyc_symbols.h"

  sym_list->element = new_sym();
  sym_vector->element = new_sym();
  // ifa/issues/104: give `tuple` the same element/per-index duality
  // `list` has. Without an element sym a tuple CreationSet has NO generic
  // element AVar at all -- get_element_avar() returns 0, tuple_able() is
  // therefore always false for tuples, and nothing can ask "is this tuple
  // monomorphic?". That is the prerequisite for letting homogeneous
  // tuples of DIFFERENT ARITY share one list representation instead of
  // being distinct record types (shedskin's variable-length `tuple<T>`
  // versus its fixed `tuple2<A,B>`). Measured target: 109/172/117 split
  // partitions on plcfrs/rdb/sudoku5 hold same-element different-arity
  // tuples.
  // ifa/issues/109: ON by default. This is no longer just the 104
  // experiment's prerequisite -- `tuple.__pyc_getslice__` reads
  // `sizeof_element`, so without an element sym a tuple slice SEGFAULTS
  // the compiler. With it, `(1,2,3,4)[0:2]` gives `(1, 2)` and a runtime
  // range works too, because populating the element makes tuple_able()
  // false and clone.cc gives the CreationSet list layout -- the same
  // "unknown arity, known element type" representation a list has.
  // PYC_TUPELEM=0 restores the old no-element tuple.
  if (getenv("PYC_TUPELEM") ? atoi(getenv("PYC_TUPELEM")) : 1) sym_tuple->element = new_sym();
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
  // ifa/issues/045: hard per-constant contour separation
  // (entry_set_compatibility) keys on the MATCHED Fun's sym; calls
  // relying on defaults match the wrapper, so it must carry the flag.
  fn->clone_methods_per_cs = f->sym->clone_methods_per_cs;
  Vec<Sym *> as;
  // The positions to default are exactly `default_args` -- NOT
  // necessarily a contiguous tail. A keyword argument can provide a
  // LATER positional while an earlier default goes unfilled
  // (`pack(circles, damping=0.1, exclude=None)` called as
  // `pack([c], exclude=c)` defaults position 3 but receives position
  // 4). The old tail-based construction (forward the first
  // has.n - defaults.n positions, append defaults) silently SWAPPED
  // arguments in that case: the kwarg's value landed in the
  // defaulted slot and the wrong default landed in the kwarg's slot
  // (issue 025, circle.py: damping bound to a Circle). The matcher's
  // contract (pattern.cc fixup_maps_for_defaults) is: wrapper
  // formals = the non-defaulted positions in original order,
  // compacted; the inner send re-expands them around the default
  // expressions at their true positions.
  Vec<MPosition *> defaults_set;
  defaults_set.set_union(default_args);
  MPosition pos;
  pos.push(1);
  Code *body = 0;
  Sym *ret = new_sym(ast);
  Code *send = if1_send(if1, &body, 0, 1, ret);
  send->ast = ast;
  for (int i = 0; i < f->sym->has.n; i++) {
    MPosition *cp = cannonicalize_mposition(pos);
    if (defaults_set.set_in(cp)) {
      if1_add_send_arg(if1, send, ((PycAST *)f->default_args.get(cp))->sym);
    } else {
      Sym *a = new_sym(ast);
      // Inherit clone_for_constants from the wrapped formal:
      // edge_constant_compatible_with_entry_set reads the flag off
      // the MATCHED fun's formal syms, which for a defaulted call are
      // these wrapper formals (ifa/issues/045).
      Sym *orig = f->arg_syms.get(cp);
      if (orig) {
        a->clone_for_constants = orig->clone_for_constants;
        // ifa/issues/087: carry the wrapped formal's own name through onto
        // the wrapper's fresh formal. A further wrapper layered on top of
        // this one (order_wrapper, when a call needs both a default fill
        // and a reorder) forwards through an anonymous SEND whose callee
        // value resolves back to the ORIGINAL (unwrapped) function -- the
        // matcher re-resolves that inner send from scratch, and without a
        // name to key on it falls back to raw position, silently
        // mismatching once this wrapper's compacted position order no
        // longer lines up with the original's. Keeping the name intact
        // lets that re-resolution route by name instead, which is
        // position-order-independent and already proven correct.
        a->name = orig->name;
      }
      as.add(a);
      if1_add_send_arg(if1, send, a);
    }
    pos.inc();
  }
  if1_move(if1, &body, ret, fn->ret, ast);
  if1_send(if1, &body, 4, 0, sym_primitive, sym_reply, fn->cont, fn->ret)->ast = ast;
  if1_closure(if1, fn, body, as.n, as.v);
  install_new_fun(fn);
  fn->fun->wraps = f;
  return fn->fun;
}

// ifa/issues/087: out-of-order keyword arguments (`f(high=9, low=2)`
// against `def f(low, high)`) matched fine all the way through
// pattern.cc's Matcher (find_all_matches/find_best_cs_match/
// covers_formals/verify_args all correctly remap actual<->formal
// position via PMatch::actual_to_formal_position, confirmed by direct
// instrumentation) but Matcher::build() then detected the reordering
// (to_actual(p, m) != p for some formal position p) and called this
// callback to synthesize an adapter -- which, before this fix, was
// IFACallbacks's base-class stub (`return 0`), silently dropping the
// match and surfacing far downstream as a runtime "matching function
// not found" trap. pyc never needed this before: it's the ONLY one of
// order/coercion/promotion_wrapper actually exercised by pyc's own
// type system (coercion/promotion go through a separate mechanism,
// AVar::num_coerce, and stay unimplemented stubs same as order_wrapper
// always was -- confirmed no other caller of any of the three exists).
//
// `substitutions` is keyed by FORMAL position -> its ACTUAL (call-
// site) position, for every one of f's has-positions (pattern.cc's
// build_arg_positions treats has[0], the function's own selector sym,
// as position 1 as uniformly as every real parameter -- confirmed by
// direct instrumentation: position 1 always self-maps, since the
// callee slot never reorders). Mirrors default_wrapper's exact shape
// (one fresh formal per has-position, a single forwarding send marked
// `wraps`) with the opposite indexing: default_wrapper's wrapper
// formals are in F's OWN declared order (defaulted positions filled
// from the default expression instead); this wrapper's formals must
// be in ACTUAL (call-site) order instead, since it's what the ORIGINAL
// call site's arguments get forwarded to positionally -- codegen never
// re-runs keyword matching, it just calls whatever Fun the matcher
// settled on with the original send's rvals, unchanged, in order.
Fun *PycCompiler::order_wrapper(Fun *f, Map<MPosition *, MPosition *> &substitutions) {
  PycAST *ast = (PycAST *)f->ast;
  Sym *fn = new_fun(ast);
  fn->nesting_depth = f->sym->nesting_depth;
  fn->clone_methods_per_cs = f->sym->clone_methods_per_cs;
  int n = f->sym->has.n;
  Vec<MPosition *> formal_positions;
  Vec<Sym *> as;
  as.fill(n);
  MPosition pos;
  pos.push(1);
  for (int i = 0; i < n; i++) {
    MPosition *fcp = cannonicalize_mposition(pos);
    formal_positions.add(fcp);
    MPosition *acp = substitutions.get(fcp);
    if (!acp) acp = fcp;
    int ai = (int)Position2int(acp->last()) - 1;
    Sym *a = new_sym(ast);
    // Inherit clone_for_constants from the wrapped formal, same as
    // default_wrapper -- entry_set_compatibility (ifa/issues/045)
    // reads the flag off the MATCHED fun's formal syms, which for a
    // reordered call are these wrapper formals.
    Sym *orig = f->arg_syms.get(fcp);
    if (orig) a->clone_for_constants = orig->clone_for_constants;
    as[ai] = a;
    pos.inc();
  }
  Code *body = 0;
  Sym *ret = new_sym(ast);
  Code *send = if1_send(if1, &body, 0, 1, ret);
  send->ast = ast;
  for (int i = 0; i < n; i++) {
    MPosition *fcp = formal_positions[i];
    MPosition *acp = substitutions.get(fcp);
    if (!acp) acp = fcp;
    int ai = (int)Position2int(acp->last()) - 1;
    // ifa/issues/087: name this forwarded arg after f's OWN formal at
    // fcp. The callee value flowing into this send's position-1 slot is
    // f's underlying (possibly further-unwrapped) symbol, so the matcher
    // re-resolves this send from scratch against that symbol's real
    // signature; a name lets it route by name (order-independent, already
    // proven correct) instead of falling back to raw position, which is
    // wrong here precisely because this send's args are in ACTUAL
    // (call-site) order, not f's order. See the matching comment in
    // default_wrapper for why the plain-position path silently
    // mismatches once reordering and default-compaction combine.
    Sym *orig = f->arg_syms.get(fcp);
    if1_add_send_arg(if1, send, as[ai], orig ? orig->name : 0);
  }
  if1_move(if1, &body, ret, fn->ret, ast);
  if1_send(if1, &body, 4, 0, sym_primitive, sym_reply, fn->cont, fn->ret)->ast = ast;
  if1_closure(if1, fn, body, as.n, as.v);
  install_new_fun(fn);
  fn->fun->wraps = f;
  return fn->fun;
}

// Issue 026 third bug: promote a single (cs, field_name)
// from unknown_vars to var_map.  Reuses an existing field
// Sym from cs->sym->has if one already exists for this
// name (so two CSs of the same class don't create
// duplicate has entries for the same field).
static bool promote_field_one(CreationSet *cs, Sym *field_sym, cchar *name) {
  if (cs->var_map.get(name)) return false;
  AVar *iv = unique_AVar(field_sym->var, cs);
  add_var_constraint(iv);
  cs->vars.add(iv);
  cs->var_map.put(name, iv);
  // Issue 026 layer 2 (COMPLETE): Enqueue ALL EntrySets that contain
  // an AVar whose `out` type includes this CreationSet.
  // This ensures that `prim_period` consumers in different functions
  // re-run their constraint setup and form backwards liveness links.
  for (EntrySet *es : fa->ess) {
    if (!es || es->in_es_worklist) continue;
    bool found = false;
    for (Var *v : es->fun->fa_Vars) {
      AVar *av = make_AVar(v, es);
      if (av->out && av->out->sorted.set_in(cs)) {
        found = true;
        break;
      }
    }
    if (found) {
      es->in_es_worklist = 1;
      fa->es_worklist.enqueue(es);
    }
  }
  return true;
}

// Promote `name` on `cs` only.  Reuses an existing field
// Sym from `cs->sym->has` if one already exists for this
// name so two CSs of the same class don't create
// duplicate has entries.  Sibling CSs of the same class
// are intentionally NOT touched — they may legitimately
// have different field sets (writes that landed on this
// CS but not others), and `determine_basic_clones`
// correctly marks CSs with different `vars.n` as
// not_equiv so they get distinct struct types and clones.
static bool promote_field(CreationSet *cs, cchar *name) {
  if (cs->var_map.get(name)) return false;
  Sym *field_sym = nullptr;
  for (Sym *h : cs->sym->has) {
    if (h && h->name && !strcmp(h->name, name)) {
      field_sym = h;
      break;
    }
  }
  if (!field_sym) {
    field_sym = new_PycSymbol(name)->sym;
    field_sym->var = new Var(field_sym);
    cs->sym->has.add(field_sym);
  }
  return promote_field_one(cs, field_sym, name);
}

bool PycCompiler::reanalyze(Vec<ATypeViolation *> &type_violations) {
  bool again = false;
  int n_notype_promote = 0, n_eager_promote = 0;  // ifa/issues/111 M1 probe
  // (1) NOTYPE-violation-driven promotion (legacy path).
  // Some reads through union receivers don't bubble up
  // a NOTYPE (the union has SOME types even if specific
  // CSs are missing the field), so this path alone misses
  // CSs that received writes from inside-function Node
  // creation patterns — issue 026's third bug.
  for (auto v : type_violations.values()) if (v) {
    if (v->kind == ATypeViolation_kind::NOTYPE) {
      if (!v->av->var->def || v->av->var->def->rvals.n < 2) continue;
      AVar *av = make_AVar(v->av->var->def->rvals[1], (EntrySet *)v->av->contour);
      for (auto cs : av->out->sorted.values()) {
        for (auto i : cs->unknown_vars.values()) {
          if (promote_field(cs, i)) { again = true; ++n_notype_promote; }
        }
      }
    }
  }
  // (2) Eagerly walk every CreationSet and promote any
  // pending unknown_vars regardless of read context.  A
  // write that lands in `cs->unknown_vars` is enough
  // evidence on its own that the field exists on this CS;
  // we shouldn't wait for a downstream read to notice it.
  // Without this, the value-iv on a Node created inside a
  // function and stored into another Node's field (e.g.
  // `set_next(node, v): node.next = Node(v)`) never makes
  // it into var_map, so the dispatch over union receivers
  // at the read site sees only the outer-scope Node's
  // value contribution and constant-folds.
  for (CreationSet *cs : fa->css) {
    if (!cs) continue;
    for (auto i : cs->unknown_vars.values()) {
      if (promote_field(cs, i)) { again = true; ++n_eager_promote; }
    }
  }
  // (3) Numeric-confluence coercion (issue 025 numeric
  // unification): a variable whose converged type mixes a numeric
  // constant with a wider numeric (`x = 0` meeting a float in a
  // loop or branch join) gets the Go/Dart untyped-constant
  // treatment -- annotate the AVar so the re-run materializes the
  // constant in the wider type at that flow point (0 -> 0.0).
  // Flow- and contour-sensitive (per AVar); int-only contours of
  // the same code keep int. NOTE the deliberate CPython
  // divergence, inherent to static typing without boxing: on a
  // path where the variable would still hold the original int at
  // runtime (e.g. the loop never ran), it now holds the float
  // (prints 0.0, not 0) -- the shedskin-style compromise.
  bool coerced = fa_coerce_numeric_confluences(type_violations);
  if (coerced) again = true;
  // ifa/issues/111 M1: which path keeps the outer loop alive? The
  // closure probe showed passes with ZERO split marks still costing a
  // full re-analysis, which means they are reanalyze()-driven, not
  // splitter-driven. Attribute them. Probe only (IFA_DBG_CLOSURE).
  if (getenv("IFA_DBG_CLOSURE"))
    fprintf(stderr, "REANALYZE again=%d notype_promote=%d eager_promote=%d coerce=%d\n",
            again ? 1 : 0, n_notype_promote, n_eager_promote, coerced ? 1 : 0);
  return again;
}

// issue 011/050 (Tier 3a): FA's own P_prim_isinstance transfer
// function (analysis/fa.cc) calls this before its normal
// CreationSet-intersection logic, for EVERY isinstance() check FA
// analyzes -- so this must recognize ONLY the exact pattern
// emit_exc_check (python_ifa_build_if1.cc) builds and stay
// conservative (return nullptr) for anything else, including a
// genuine user `isinstance(x, SomeNoneUnion)`. Mirrors
// ifa/optimize/exc_check_fold.cc's exc_check_operand_provably_safe
// (Tier 2, post-clone, reads Fun::calls) but pre-clone, over the
// live ES/AEdge graph FA's OWN fixed point maintains -- see
// EntrySet::can_raise (fa.h) and compute_es_can_raise() (fa.cc).
AType *PycCompiler::provably_constant_isinstance(AVar *operand_av, EntrySet *es, PNode *send_pnode) {
  if (!operand_av || !operand_av->var || !es || !send_pnode) return nullptr;
  PNode *def = operand_av->var->def;
  if (!def || !def->code || def->code->kind != Code_MOVE) return nullptr;
  if (!def->rvals.n || !def->rvals[0]->sym || !def->rvals[0]->sym->name) return nullptr;
  if (strcmp(def->rvals[0]->sym->name, "__pyc_exc__")) return nullptr;
  if (!def->code->ast) return nullptr;
  IFAAST *ast = def->code->ast;
  bool found_any = false;
  for (AEdge *e : es->out_edges) {
    if (!e || !e->pnode || !e->pnode->code || e->pnode->code->ast != ast) continue;
    found_any = true;
    if (!e->to || e->to->can_raise) return nullptr;  // unresolved or raising -- stay conservative
  }
  if (!found_any) return nullptr;  // no matching outgoing edge (yet) -- stay conservative
  return fa->type_world.true_type;
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
    // Python scoping: a CLASS body is not an enclosing scope for the
    // functions defined in it -- from inside a method, a bare name
    // skips the class's bindings and resolves at module level (only
    // code directly in the class body, i.e. when the class scope is
    // the CURRENT scope, sees them). Without this skip, a method
    // named like an imported module shadowed the module inside every
    // sibling method: genetic2's `import copy` + `Individual.copy()`
    // made `copy.deepcopy(...)` inside Individual.copy resolve
    // `copy` to the METHOD, untyping the whole call chain (pyc
    // issues/025). Guards: i > 0 keeps the module scope (whose
    // enter_scope also lands in the `cls` branch) walkable; the
    // is_module test keeps imported-module scopes walkable if one
    // ever appears mid-stack.
    if (!top && i > 0 && s->in && s->in == s->cls && !s->in->is_module) continue;
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

// issues/107: report names that were used but bound nowhere. Deferred to
// the end of the module so forward references resolve first. CPython
// raises `NameError: name 'X' is not defined` at RUNTIME; pyc knows it
// statically and reports it as a compile error, which is strictly
// better -- and, critically, replaces the previous behaviour of warning
// ("'X' has no type"), exiting 0, and emitting a binary that segfaults.
// CPython builtins pyc does not implement. Reporting these as "not
// defined" would be misleading -- the user did not misspell anything,
// pyc simply lacks them -- so they get their own wording. Reaching one
// is still an error: the program cannot work.
static bool is_unimplemented_builtin(cchar *name) {
  static cchar *kNames[] = {
      "divmod", "property", "EOFError", "staticmethod", "classmethod", "eval", "exec", "compile",
      "globals", "locals", "vars", "dir", "hasattr", "getattr", "setattr", "delattr", "callable",
      "memoryview", "frozenset", "bytearray", "complex", "format", "hash", "id", "iter", "next",
      "object", "repr", "reversed", "slice", "super", "type", "OverflowError", "RecursionError",
      "NotImplementedError", "UnicodeDecodeError", "FileNotFoundError", "PermissionError", nullptr};
  for (int i = 0; kNames[i]; i++)
    if (!strcmp(name, kNames[i])) return true;
  return false;
}

int report_import_errors(PycCompiler &ctx) {
  int n = ctx.import_errors.n;
  for (cchar *e : ctx.import_errors) fprintf(stderr, "%s\n", e);
  ctx.import_errors.clear();
  // A failed import leaves names unbound, so the undefined-name pass
  // would fire too and bury the real cause. Exit here instead, the way
  // report_undefined_names does.
  if (n) fail("%d import error%s", n, n == 1 ? "" : "s");
  return n;
}

int report_undefined_names(PycCompiler &ctx) {
  int n = 0;
  for (cchar *name : ctx.pending_order) {
    if (!name || ctx.bound_names.get(name)) continue;
    int line = ctx.pending_uses.get(name);
    if (is_unimplemented_builtin(name))
      fprintf(stderr, "error line %d, builtin '%s' is not supported by pyc\n", line, name);
    else
      fprintf(stderr, "error line %d, name '%s' is not defined\n", line, name);
    ++n;
  }
  ctx.pending_uses.clear();
  ctx.bound_names.clear();
  ctx.pending_order.clear();
  // Exit the way every other frontend error does. Returning a code here
  // instead left the caller to unwind into a half-built IF1 and the
  // COMPILER ITSELF segfaulted (rc=139) after printing the diagnostic --
  // precisely the failure mode issues/107 exists to remove.
  if (n) fail("%d undefined name%s", n, n == 1 ? "" : "s");
  return n;
}

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
  bool unresolved_use = false;
  switch (scoping) {
    case PYC_USE: {
      if (!l) {
        // issues/107: record the unresolved use, then fall through to
        // Lglobal exactly as before. The name may still be bound later
        // in the module (a forward reference), so the verdict is
        // deferred to report_undefined_names() at end of module.
        // `_` is the match-statement WILDCARD (`case _:`), not a name
        // reference, and is conventionally a throwaway elsewhere. Never
        // report it.
        if (strcmp(name, "_") && !ctx.in_kwarg_key && !ctx.pending_uses.get(name)) {
          ctx.pending_uses.put(name, ctx.lineno);
          ctx.pending_order.add(name);
        }
        unresolved_use = true;
        goto Lglobal;  // not found
      }
      if (!local && !explicitly) {
        if (global)
          ctx.scope_stack.last()->map.put(name, GLOBAL_USE);
        else
          ctx.scope_stack.last()->map.put(name, NONLOCAL_USE);
      }
      break;
    }
    case PYC_LOCAL:
      if ((local || explicitly) && !isfun) {
        if (ctx.scope_stack.n == 1) ctx.bound_names.put(name, 1);  // issues/107, module scope only
        break;
      }
      if (implicitly && !isfun) fail("error line %d, '%s' redefined as local", ctx.lineno, name);
      ctx.scope_stack.last()->map.put(name, (l = new_PycSymbol(name, ctx)));
      if (local && previous) l->previous = previous;
      // issues/107: ONLY a module-scope binding can resolve an earlier
      // unresolved USE that fell through to the module. A local inside
      // some other function must not -- that is scope-insensitivity, and
      // it is exactly the bug that let a 212-line plcfrs reduction pass
      // the reducer's own name check while `parse()` read `tolabel`,
      // bound only as a local of `splitgrammar`.
      if (ctx.scope_stack.n == 1) ctx.bound_names.put(name, 1);
      break;
    case PYC_GLOBAL:
    Lglobal:;
      if (l && !global && (local || explicitly || implicitly))
        fail("error line %d, '%s' redefined as global", ctx.lineno, name);
      if (!global) {
        PycSymbol *g = ctx.scope_stack[0]->map.get(name);
        if (!g) ctx.scope_stack[0]->map.put(name, (l = new_PycSymbol(name, ctx)));
      }
      // issues/107: a REAL `global x` / definition binds the name; the
      // PYC_USE fallthrough above does not.
      if (!unresolved_use) ctx.bound_names.put(name, 1);  // module scope by construction
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

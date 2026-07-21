# issues/

Open work items for the pyc frontend / project-wide concerns —
limitations or bugs in `pyc.cc`, the `python_ifa_*` lowering, the
`python.g` grammar, the `__pyc__/` builtin module, the runtime
(`pyc_runtime.c` / `pyc_c_runtime.h`), or the harness.

For ifa-library issues, see [`ifa/issues/`](../ifa/issues/). The
conventions are the same; the only difference is location.

## Conventions

- Filenames: `NNN-short-slug.md`, NNN zero-padded. Pick the next
  number; don't reuse.
- One issue per file. Cross-link with relative paths.
- Status: `open`, `in-progress`, `closed` (with closing commit
  ref). Closed issues stay in the tree as history.
- Cite specific files / line numbers / commits where helpful.
- Include a "Verification plan" so the next person knows how to
  prove the fix works.
- Include a "What this unblocks" section — issues with no
  consequence should not be filed.

## Current open issues

- [007-decorators-not-applied.md](007-decorators-not-applied.md)
  — **Largely fixed** by the split-identity rework: user-defined
  function decorators now apply (closure-wrapping, replacement,
  parameterized — `decorator_basic.py`), and Finding 2's
  self-referential reassignment is fixed
  (`function_selfref_reassign.py`). Remaining, all loud failures:
  stacked applications of the same closure-wrapping decorator
  (needs carrier-class method slots — `ifa/issues/030` scope),
  class-based decorators (`@Wrapper` — decorator-position class
  call doesn't route through constructor lowering), dotted-name
  decorators (silent no-op), and decorated *methods* (legacy
  no-op).
- [014-generators-yield-unimplemented.md](014-generators-yield-unimplemented.md)
  — `yield` (generator functions) is unimplemented; needs a
  resumable-function execution model (state-machine transform or
  stackful coroutines) not present anywhere else in pyc today.
- [018-dict-mixed-key-types-boxing-failure.md](018-dict-mixed-key-types-boxing-failure.md)
  — A program using `dict` (or `set`) with two different key/
  element types anywhere fails to compile with a `BOXING`/"mixed
  basic types" FA violation — each shared internal comparison
  method (`_keys[i] == key`) isn't specialized per key type.
- [023-structural-pattern-matching.md](023-structural-pattern-matching.md)
  — `match`/`case` now covers every PEP 634 pattern KIND: literal,
  wildcard, capture, or-patterns, guards, sequence, mapping, class,
  and `None`/`True`/`False` singleton patterns all work (**fixed
  2026-07-12**), verified against real `python3` output on both
  backends. Or-patterns and class patterns had both been **silent
  miscompiles** (parsed as an ordinary expression/constructor call
  and compared via `__eq__`, no error at all); so had `None`/`True`/
  `False` (silently caught by the capture-pattern branch, matching
  unconditionally regardless of the subject's actual value) and
  mixed-literal-type matches (`case 5: ... case "hi":` crashed the
  underlying C compiler). Sequence/mapping/class patterns share one
  lesson: FA type-checks the whole program statically, so a
  structural check's element/attribute access needs genuine nested
  `if1_if` control flow (not a flat boolean fold) to narrow the
  subject's type within a branch, and a guard must be evaluated
  *inside* that same nesting, not appended afterward. **One
  remaining limitation**: `case None:` combined with almost any
  OTHER pattern in the same match crashes at runtime (`"matching
  function not found"`) — same assertion text as closed issue 026,
  but ruled out as the same bug once 026 was root-caused: this one
  is dispatch over a union of PRIMITIVE/boxed types
  (`None|int|float`), not 026's class-method classtag gap.
  `build_match_pyda` refuses the combination at compile time rather
  than risk shipping it. **Precisely root-caused 2026-07-22** (still
  not fixed): a gap in `ifa/analysis/fa.cc`'s `peel_wrapper_def`
  (issue 025's per-branch narrowing) that never engages for
  `match`/`case`'s generated code — see
  [`ifa/issues/059`](../ifa/issues/059-narrowing-peel-wrapper-boolean-collapse-gap.md)
  for the full mechanism and proposed fix. Positional class patterns (`case Point(0,
  0):`, matched via a compile-time read-back of `__match_args__`,
  including base-class inheritance and mixing with keyword args) were
  added **2026-07-21**, along with sequence-pattern star capture
  (`case [a, *rest]:`, reusing issue 024's `star_expr` grammar node in
  `listmaker`/`testlist_comp` -- with a defensive `fail()` added for
  the ordinary, unsupported list/tuple-literal-unpacking shape (PEP
  448) that grammar sharing also newly parses), and mapping-pattern
  `**rest` (`case {"k": v, **rest}:`, a new `dictorsetmaker` grammar
  rule; real Python structurally limits `**rest` to one, last, never
  `_` — pyc's grammar enforces the first two for free, `build_pattern_match`
  checks the third). No pattern-matching features remain deferred;
  only the `case None:`-combination runtime limitation above is left.
- [028-raise-exception-regression-qualified-dispatch.md](028-raise-exception-regression-qualified-dispatch.md)
  — `raise Exception("...")` regressed bh and richards from
  compile-with-warn to FAIL (`'Exception' has no type`); bisected
  to `a32a6467` (issue 027's qualified-static-dispatch commit),
  which simultaneously fixed go and loop, so corpus bucket COUNTS
  didn't move — compare member sets, not counts.
- [030-with-exit-not-called-on-exception.md](030-with-exit-not-called-on-exception.md)
  — `with`'s `__exit__` is never called when the body raises (and
  can't suppress the exception the way real Python allows) — the
  exception-safety gap issue 012's own filing anticipated and
  deferred pending issue 011, which landed three weeks after 012
  closed without this being revisited. `with`'s cleanup only hooks
  into `return`/`break`/`continue`, never into `raise`/unwinding.

## Closed (archive)

Closed issues live in [`closed/`](closed/) with the closing
commit ref recorded in each file's status line.

- [011](closed/011-exception-handling-unimplemented.md) —
  `try`/`except`/`else`/`finally`/`raise` implemented (option C:
  exception slot + explicit post-call checks, FA-gated), including
  typed clauses (`except X as e:`, tuple forms), bare re-raise, and
  cross-function propagation on both backends. Landing it fixed four
  unrelated pre-existing bugs (a stubbed `isinstance()` against real
  classes, `pass`-only exception subclasses losing constructor args,
  a shared clonable `isinstance()` wrapper breaking per-class
  dispatch, and a raise-only function body leaving its return value
  untyped) and went through three further optimization passes
  (per-callee, then post-FA precise, then FA-native `can_raise`
  gating) to fold the post-call check away entirely for provably-safe
  calls. File had been left in `issues/` (never moved to `closed/`)
  for several days despite being fully implemented — moved as part of
  the same 2026-07-21 pass that filed
  [030](030-with-exit-not-called-on-exception.md); its content needed
  no correction, only relocation. `with`'s exception-safety
  integration was the one real gap this surfaced — see 030.
- [012](closed/012-with-statement-unimplemented.md) — `with`
  (context managers) implemented: `__enter__`/binding/`__exit__`
  desugaring, multiple context managers in one statement, and
  cleanup firing correctly on fallthrough, `return`, and loop
  `break`/`continue` (tracked via a `with_stack`). File had been
  moved to `closed/` with stale content (still said "Status: open")
  until a 2026-07-20 pass reconciled it. Exception-safety
  (`__exit__` on a raising body) was correctly anticipated as a
  follow-on dependent on issue 011, which landed after this issue
  closed and was never revisited — now concretely confirmed broken
  and filed as [030](030-with-exit-not-called-on-exception.md).
- [016](closed/016-missing-grammar-level-syntax.md) — the five
  grouped parse-gap syntax forms (`async`/`await`, walrus `:=`,
  `match`/`case`, PEP 484 annotations, extended iterable unpacking)
  were split out into their own per-form issues as each was picked
  up, per this filing's own stated plan.
- [008](closed/008-set-literal-genexpr-crash.md) — set literals
  and set comprehensions fixed (`04a85584`/`f67cf692`, new
  `__pyc__/08_set.py` `set` class); also fixed a pre-existing
  `x in y`/`x not in y` operand-order bug affecting *every*
  container type, found along the way. Generator expressions got
  only an interim clean `fail()` here, then were actually
  implemented later (`20fdc72d`, eager `list` materialization, not
  true laziness — see issue 014) as part of the shedskin-corpus
  push tracked in issue 025.
- [024](closed/024-extended-iterable-unpacking.md) — extended
  iterable unpacking assignment targets (`a, *b = [1, 2, 3]`,
  PEP 3132): new `star_expr`/`testlist_item` grammar (`testlist`,
  used by `expr_stmt`'s targets), `mark_store` and
  `emit_assign_to_target` both recurse through a `PY_star_expr`
  wrapper. Leading/trailing targets bind positionally as before; the
  star target binds a NEW list (always a list, even from a tuple
  source) built by a hand-rolled runtime loop. Bare star target and
  multiple stars both fail loudly with CPython's own error wording.
  Known gaps, not silent traps: nested parenthesized tuple targets
  (`x, (y, *z) = ...`) and `for`-loop targets don't parse (separate
  grammar rules, not extended — out of this issue's stated scope).
  Found and filed separately while landing this: a pre-existing,
  unrelated FA bug where an empty list literal sharing a method
  clone with a non-empty, differently-element-typed list fails to
  type-check — [ifa/issues/040](../ifa/issues/040-empty-list-shared-clone-type-inference.md).
  Test: `tests/star_unpack.py`, both backends.

- [026](closed/026-polymorphic-method-dispatch-partial-override-crash.md)
  — polymorphic method dispatch over a union where at least one
  class doesn't override the called method (relies purely on
  inheritance) crashed on the C backend ("matching function not
  found") and silently dropped the call on v2 LLVM (ran clean, no
  output). Root cause: FA types such a method's `self` formal as a
  `Type_SUM` (union) Sym covering every inheriting class, but the
  classtag dispatch construction (both backends) and the shared
  method-pointer-slot registry (`codegen_common.cc`) only knew how
  to read a single concrete class's own fields, never recursed into
  a union's members. Fixed in all three places; two regressions
  surfaced and were fixed along the way (a non-union self arg's slot
  getting mis-resolved per `CreationSet`, and a class with its own
  override losing to a DIFFERENT candidate's looser union match) —
  `tests/poly_dispatch_low.py` / `poly_dispatch_high.py`
  (pre-existing) caught both. New test:
  `tests/poly_dispatch_partial_override.py`. Found while
  stress-testing issue 003; unrelated to that issue's struct-layout
  root cause.

- [027](closed/027-unbound-base-method-call-self-type-mismatch.md) —
  explicit unbound base-method calls (`Base.method(self, ...)`,
  `A.__init__(self)`) now dispatch STATICALLY as the named class via
  `Sym::aspect` (the super() mechanism, with `fixup_aspect` gated to
  super-registered Syms so qualified-call aspects stay final).
  Landed together with `@staticmethod`/`@classmethod` support: both
  markers are recognized at class scope (definition markers, not
  runtime decorators), staticmethods are callable through the class,
  an instance, a subclass, and as plain values (a new
  `Sym::is_static_method` bit routes instance reads through
  P_prim_period's bare-value rule unbound); classmethods called
  through a class receive the class value as `cls`, and `cls(...)`
  constructs via the ordinary `__new__`-through-meta dispatch.
  Tests: `unbound_base_call.py`, `static_method.py`,
  `class_method.py`, both backends. Known first-cut limits:
  classmethod through an instance (`a.cf()`) and bare classmethod/
  method references without a call keep the old prototype-bound
  behavior.
- [029](closed/029-deepcopy-user-objects.md) — `copy.deepcopy(obj)`
  on a user-class instance now recurses per-field instead of doing a
  shallow one-level clone: every record class without its own
  `__deepcopy__` gets a compiler-synthesized recursive one. Fixed
  five latent compiler bugs along the way (a stubbed
  `isinstance()`-adjacent Type_SUM inliner assert, a mixed-length
  tuple/list literal length off-by-one, `cg.cc` dropping nil-typed
  moves into real locals, a degenerate recursion-pending-map fan-out,
  and non-canonical struct-layout field ordering). No memo table
  (v1) — shared subtrees duplicate and cycles don't terminate,
  deliberately deferred (no corpus need). File had sat in `issues/`
  unmoved despite being fully implemented; `tests/deepcopy_list.py`/
  `deepcopy_objects.py` had no `.exec.check` (compile-only in CI) —
  both fixed 2026-07-21, along with a test bug in
  `deepcopy_objects.py` that called a pyc-only synthesized method
  directly (`tree.__deepcopy__()`, which doesn't exist on real
  CPython objects) — changed to `copy.deepcopy(tree)`, now fully
  CPython-comparable. genetic2 (the corpus example that motivated
  this) has correct deepcopy semantics but still doesn't compile due
  to an unrelated FA flow-divergence bug, tracked separately as
  `ifa/issues/048`.

- [003](closed/003-subclass-struct-layout-mismatch.md) — the
  originally-filed struct-layout-mismatch bug no longer reproduces
  (four escalating adversarial tests pass, matching CPython
  byte-for-byte); resolved as an emergent property of
  `ifa/analysis/clone.cc`'s CS-equivalence struct unification, not
  the prefix-copy fix this issue proposed. Regression added:
  `tests/polymorphic_list.py`. The related
  `class_attr_mutation.py.python.expect_fail` xfail (mutable
  shared class-attribute state across subclasses) is explicitly
  NOT retired — accepted as a deliberate CPython incompatibility,
  not scheduled for a fix. A separate polymorphic method-dispatch
  crash found while stress-testing this issue is filed as 026.
- [010](closed/010-multiple-inheritance-unrelated-bases.md) —
  multiple inheritance from unrelated bases no longer reproduces
  the original compile failure; also verified diamond inheritance,
  MRO tie-break on conflicting method names, and multi-base data
  fields all work, matching CPython on both backends. Very likely
  fixed by the same underlying mechanism as issue 003, closed the
  same day. Regression added: `tests/multi_inherit.py.exec.check`
  (the test file already existed from this issue's filing but had
  never been given a golden). A separate, unrelated gap — explicit
  unbound base-method calls (`A.__init__(self)`) failing a type
  check even under single inheritance — found while stress-testing
  this issue and filed as 027.
- [004](closed/004-is-operator-unimplemented.md) — `is`/`is not`
  now lower to identity comparison (`prim_isinstance`/`prim_is`)
  instead of an unresolved `__is__` dispatch; the follow-on
  `is None` union-narrowing gap this exposed was tracked and fixed
  separately as `ifa/issues/closed/024`.
- [006](closed/006-fstring-interpolation-not-implemented.md) —
  f-strings fully implemented: interpolation and PEP 3101 format
  specs (`{x:.2f}`, `{x:>10}`, `{x:,}`, etc.) both work on both
  backends, dispatched via a new `__format__` dunder mirroring
  CPython's `format(x, spec)`. Found and fixed an unrelated
  pre-existing bug while verifying against realistic values: the C
  backend (not LLVM) silently corrupted any float literal needing
  more than 6 significant digits when embedding it as a compiled
  constant (`ifa/if1/num.cc`'s `sprint_float_val` used bare `%g`
  instead of the `%.17g` convention already used elsewhere in the
  runtime).
- [009](closed/009-dict-comprehension-drops-comp-for.md) — dict
  comprehensions now work correctly on both backends, sharing the
  loop-lowering machinery added for issue 008's set comprehensions.
  Two unrelated pre-existing gaps found while testing this were
  filed separately (018, 019).
- [020](closed/020-str-builtin-call-broken.md) — `str(x)` (1-arg
  call) now lowers directly to `x.__str__()` instead of falling
  through the generic (and broken, for `str`) constructor-call
  path. The zero-arg `str()` case turned out to be a separate,
  broader pre-existing bug affecting every builtin type's zero-arg
  constructor call, filed as 022.
- [022](closed/022-builtin-type-zero-arg-constructor-broken.md) —
  `int()`, `float()`, `bool()`, `list()`, `tuple()`, `str()` all now
  produce the correct zero value instead of failing to compile.
  Root cause: `int`/`float`/`bool`/`list`/`tuple` are never
  `Type_RECORD` (aliased or ifa-core primitive types instead), so
  the generic `__new__`-from-`__init__` constructor-call machinery
  never had a candidate to dispatch a direct call to, regardless of
  arg count. Fixed by synthesizing each type's zero value directly
  in the frontend rather than going through class instantiation.
  `tuple`'s missing `__eq__`/`__str__`, found along the way, filed
  separately as 023.
- [023](closed/023-tuple-missing-eq-str.md) — `tuple` now has
  `__eq__`/`__ne__`/`__str__`/`__repr__`. Root cause of the C-backend
  crash hit while landing `__str__` (traced via `gdb`, not guessed):
  a null-pointer dereference in `ifa/codegen/cg.cc`'s empty-tuple
  codegen (`element` can be null for an always-empty tuple, not just
  `element->type`). Also fixed along the way: a missing
  `PointerHash<PNode*>` specialization (same bug class as issue 021's
  `Var*` finding — `PNode` has a monotonic id but was left out of
  `ifa/notes/004`'s six); `list.__eq__`'s latent bug (`.len()`,
  not a real method, silently returned wrong answers for
  different-length lists — `[1,2]==[1,2,3]` returned `True`); and
  `__ne__` was missing on both `list` and `tuple` entirely. `dict`'s
  matching gap needs a different (key-order-independent) shape,
  filed separately as 024.
- [024](closed/024-dict-missing-eq.md) — `dict` now has
  `__eq__`/`__ne__`, key-order-independent (mutual-containment plus
  value comparison per key, mirroring `set.__eq__`'s shape rather
  than `list`/`tuple`'s index-aligned one from issue 023).
- [015](closed/015-pyc-pod-records-no-frontend-hook.md) —
  `@pyc_struct` decorator wired (originally
  `ifa/issues/015`; moved here because the gap was in the pyc
  frontend, not the ifa library).
- [017](closed/017-multi-instance-mutation-corruption.md) —
  `dict`/`set` lacked an `__init__`, so their `_keys`/`_vals`/
  `_items` list fields were bare class-body attributes — Python's
  classic mutable-class-attribute footgun, applied to a builtin
  container type. A second instance written to after a first one
  silently read/wrote the wrong data. Fixed by giving both an
  explicit `__init__`.
- [019](closed/019-dict-missing-str-repr.md) — `dict` now has
  `__str__`/`__repr__`, modeled on `set`'s (which already had it).
  `print(some_dict)` shows real contents instead of the generic
  `<instance>` placeholder.
- [013](closed/013-assert-statement-unimplemented.md) — `assert`
  now lowers to an abort (print `AssertionError[: msg]`, `exit(1)`)
  on both backends; a fully catchable `AssertionError` still awaits
  issue 011. Found and fixed an unrelated pre-existing bug along the
  way: the v2 LLVM backend's `__pyc_c_call__` never stripped a
  leading `::` (a C++-only global-scope qualifier) from the target
  function name, so `exit()` — and anything else calling a
  `"::"`-qualified C function — could never link on that backend.
- [021](closed/021-scope-map-pointer-hash-nondeterminism.md) —
  frontend/FA run-to-run nondeterminism from pointer-keyed hashing.
  Landed fixes: `PycScope::map` → content-hashed `HashMap`, missing
  `PointerHash` specializations, and finally an ID-based
  `AEdgeHashFns` for `EdgeHash` (commit `1613af8`), the last known
  surface-level instance. Full byte-identical-build reproducibility
  (~150+ remaining pointer-hashed sites) is formally deferred to
  `ifa/issues/010`'s planned audit.
- [002](closed/002-fa-crash-escaped-closure.md) — bound-method
  closures now survive escaping their binding scope on both
  backends: passed as arguments and returned (`b7721ae`, two
  `simple_inlining` bounds bugs at first-PNode/escaped-call-site
  shapes), and stored in / read back from a `global` (`41aa654`:
  the `None` initializer made the global's type
  `SUM{None, closure}`; codegen now sees through the nullable SUM
  via `closure_fun_type`, plus a bounds-checked `write_send_arg`).
  Tests: `escaped_closure.py`, `closure_returned_from_function.py`,
  `closure_in_global.py`. Multiple *different* closure shapes in
  one global remain `ifa/issues/029`/`030` territory (defined
  runtime error, not a crash).
- [005](closed/005-while-true-fa-crash.md) — `while True:` no
  longer crashes FA: fixed by the `update_in` guard in `97f6a6c`,
  then made structural in `2b3bcd3` (`ifa/issues/031` step 1:
  `GLOBAL_CONTOUR` is a real singleton EntrySet, so the sentinel
  deref class is gone entirely). Committed coverage:
  `while_true_loop.py`. The separate `while True:`-as-first-
  statement-of-a-function FA bug found while closing this is filed
  as issue 025.
- [025](closed/025-while-true-first-statement-of-function.md) —
  `while True:` opening a function body no longer breaks FA typing
  of the formals (and no longer trips the LLVM
  entry-block-predecessor verifier). Root cause: the loop-header
  LABEL became the entry PNode, so SSU's loop phi (sized by
  `cfg_pred.n`) only saw the back edge and the formals' values
  never flowed into the loop. Fixed in `Fun::build_cfg` by
  prepending a synthetic NOP whenever the body's first leaf Code
  is a LABEL — the entry PNode is never a jump target. Coverage in
  `while_true_loop.py`. Filed and closed same day, found while
  closing issue 005.
- [001](closed/001-fa-crash-captured-locals.md) — closures
  capturing enclosing-scope locals: the original `unique_AVar`
  crash was fixed by closure-carrier classes; every residual
  (nested-def cosmetic FA warning, self-capture regression, the
  mixed recursion+capture shape, transitive grandparent-scope
  captures) resolved by issue 007's split-identity rework. Tests:
  `captured_local.py`, `nested_capture.py`, `nested_recursion.py`,
  `decorator_basic.py`.

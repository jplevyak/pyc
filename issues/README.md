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
- [008-set-literal-genexpr-crash.md](008-set-literal-genexpr-crash.md)
  — **Set literals/comprehensions fixed** (new `__pyc__/08_set.py`
  `set` class); also fixed a pre-existing `x in y`/`x not in y`
  operand-order bug affecting *every* container type, found along
  the way. Generator expressions now fail cleanly instead of
  crashing (real support tracked with issue 014).
- [011-exception-handling-unimplemented.md](011-exception-handling-unimplemented.md)
  — `try`/`except`/`finally`/`raise` are entirely unimplemented
  (`fail("statement not supported")`); no exception object model
  or unwinding mechanism exists yet. Largest lift in this batch
  besides generators.
- [012-with-statement-unimplemented.md](012-with-statement-unimplemented.md)
  — `with` (context managers) is unimplemented; a happy-path
  `__enter__`/`__exit__` desugaring doesn't need issue 011's
  exception model, only full exception-safety does.
- [014-generators-yield-unimplemented.md](014-generators-yield-unimplemented.md)
  — `yield` (generator functions) is unimplemented; needs a
  resumable-function execution model (state-machine transform or
  stackful coroutines) not present anywhere else in pyc today.
- [016-missing-grammar-level-syntax.md](016-missing-grammar-level-syntax.md)
  — Five newer syntax forms aren't in the grammar at all and fail
  as parse errors: `async`/`await`, walrus `:=`, `match`/`case`,
  PEP 484 type annotations, and extended unpacking (`a, *b =
  ...`). Grouped as one filing; split out per sub-item as work
  starts (priority/effort vary widely — annotations and walrus
  are small, async/await and match/case are substantial).
- [018-dict-mixed-key-types-boxing-failure.md](018-dict-mixed-key-types-boxing-failure.md)
  — A program using `dict` (or `set`) with two different key/
  element types anywhere fails to compile with a `BOXING`/"mixed
  basic types" FA violation — each shared internal comparison
  method (`_keys[i] == key`) isn't specialized per key type.
- [023-structural-pattern-matching.md](023-structural-pattern-matching.md)
  — `match`/`case` further along than originally filed: literal,
  wildcard, and (**fixed 2026-07-12**) capture patterns
  (`case x:`) all work, verified against real `python3` output.
  Remaining: or-patterns (`case 1 | 2:`) **silently miscompile**
  (evaluated as bitwise-OR then compared, no error at all) — the
  same trap class/sequence/mapping patterns would hit too. Guards
  fail to parse (no grammar rule). Class patterns remain a separate
  large undertaking.
- [026-polymorphic-method-dispatch-partial-override-crash.md](026-polymorphic-method-dispatch-partial-override-crash.md)
  — Polymorphic method dispatch over a union where at least one
  class doesn't override the called method (relies purely on
  inheritance) crashes on the C backend ("matching function not
  found") and silently drops the call on v2 LLVM (runs clean, no
  output). Found while stress-testing issue 003; unrelated to that
  issue's struct-layout root cause.
- [028-raise-exception-regression-qualified-dispatch.md](028-raise-exception-regression-qualified-dispatch.md)
  — `raise Exception("...")` regressed bh and richards from
  compile-with-warn to FAIL (`'Exception' has no type`); bisected
  to `a32a6467` (issue 027's qualified-static-dispatch commit),
  which simultaneously fixed go and loop, so corpus bucket COUNTS
  didn't move — compare member sets, not counts.

## Closed (archive)

Closed issues live in [`closed/`](closed/) with the closing
commit ref recorded in each file's status line.

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

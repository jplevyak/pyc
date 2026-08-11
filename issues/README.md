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

- [045-tonyjpegdecoder-second-call-hangs.md](045-tonyjpegdecoder-second-call-hangs.md)
  — a second call to `main()` in tonyjpegdecoder.py hangs (sustained
  100% CPU, no progress) even though each call constructs entirely
  fresh objects; the first call runs correctly, matching CPython.
  Found while confirming the doc's "crashes the compiler with an FPE"
  claim (TODO item 5) was stale — the compiler doesn't crash at all
  today; two real bugs were found and fixed getting to this actual
  current blocker (`bytes(x)` never checked for a user-defined
  `__bytes__`; the LLVM backend's `_CG_string_identity` was never
  linkable). Not root-caused past "stalls in `InitDecoder()`/the
  Huffman decode loop" — possibly another instance of the
  shared-prototype-state bug family (closed-017, closed-044), not
  confirmed.
- [043-slice-target-augmented-assignment-silently-wrong.md](043-slice-target-augmented-assignment-silently-wrong.md)
  — `a[i:j] += x` silently corrupts the list: the existing code
  comment claimed it just "acts like `=`" (drops the operator), but
  it's worse — the RHS is applied twice, once as a wrong full-slice
  replacement and again via a genuine `__iadd__` mistakenly called
  against the whole list rather than the extracted slice, appending
  to the end regardless of the original slice bounds. Confirmed via
  generated C. Found while auditing
  [issues/025](025-shedskin-examples-coverage.md)'s TODO list (item
  17) — the gap was already documented inline but never filed.
- [042-package-directory-import-resolution.md](042-package-directory-import-resolution.md)
  — no support for directory-based Python packages (`pkg/__init__.py`
  + submodule files) — every import resolves as a single flat
  `<mod>.py` filename, and package directories are explicitly skipped
  during search. Confirmed real on two corpus examples: `minilight`
  (`from ml import entry`, 1-level) and `tarsalzp` (a 4-level-deep
  dotted import), so any fix needs recursive nesting, not just one
  level. "The last structural import blocker" per
  [issues/025](025-shedskin-examples-coverage.md)'s own 2026-07-22
  note — named then, never filed until now.
- [041-stdlib-shim-stubs-silently-wrong.md](041-stdlib-shim-stubs-silently-wrong.md)
  — `pyc_lib/struct.py`, `colorsys.py`, `getopt.py`, and `os.py`'s
  filesystem functions all exist as importable shims but were no-op
  stubs (`struct.pack` returns `b""`, `getopt.getopt` always
  `([], [])`, `os.listdir` always `[]`) — silent wrong output, zero
  diagnostic, confirmed load-bearing in `minpng.py`/`sha.py`/
  `mandelbrot2.py`. `fnmatch` and `os.path`'s string functions are, by
  contrast, genuinely correct; `array`/`re` are real but incompletely-
  featured. Found while auditing
  [issues/025](025-shedskin-examples-coverage.md)'s TODO list (item
  14). **`colorsys` fixed 2026-08-08** (real HSV/HLS/YIQ conversions,
  ported from CPython) — found and fixed two genuine, general compiler
  bugs along the way: `%` had no float support at all (three separate
  layers: C runtime header, LLVM codegen, and the compile-time
  constant-folder each independently assumed integer-only), and even
  plain `int % int` had the wrong sign convention vs. Python's floored
  semantics (`-7 % 3` gave C's `-1` instead of `2`). `struct`/
  `getopt`/`os` filesystem functions remain open.
- [039-list-mul-shared-element-type-cross-contamination.md](039-list-mul-shared-element-type-cross-contamination.md)
  — `list.__mul__`/`__rmul__` (`n * [x]`) shares a CreationSet/element-
  type representation across unrelated call sites: `bh.py`'s genuinely
  heterogeneous `Cell.subp = [None] * Cell.NSUB` (`Body | Cell`) leaks
  into the unrelated, genuinely homogeneous `Tree.bodies = [None] *
  nbody` (`Body`-only), producing spurious "illegal call argument
  type... Cell" warnings. Same architectural gap as 035's tictactoe
  finding, traced one step further and pinned specifically to
  `list.__mul__`'s construction path. Also see
  [ifa/079](../ifa/issues/079-DISPATCH-single-candidate-dispatch-unchecked-cast.md),
  a related but independent dispatch-codegen bug this same corpus
  example exposed.
- [035-list-element-cast-salvage-guard-and-set-item-union.md](035-list-element-cast-salvage-guard-and-set-item-union.md)
  — partially fixed: `P_prim_set_index_object` (both branches) cast
  an assigned value into a list/tuple-list's element type with no
  compatibility check, producing a hard C compile error on a
  pointer/scalar mismatch (same bug class as
  [056](../ifa/issues/closed/056-CGEN-degraded-index-type-raw-c-compile-error.md), the value
  rather than the index) — fixed, `shedskin_examples/tictactoe/
  tictactoe.py` now compiles clean. Does **not** yet run: a genuine
  `set`-element type union (int64 vs float64, reachable through
  `set`'s own generic `union()`/`intersection()`/`__pyc_set_from
  _iterable__`) still crashes it at runtime, not fully traced.
  Separately: a first attempt at that gap (removing
  `__set_iter__`/`__dict_iter__`'s class-body defaults, mirroring
  [ifa/076](../ifa/issues/closed/076-mutation-driven-receiver-divergence-not-cloned.md))
  regressed `webserver.py`
  ([032](closed/032-dict-view-membership-missing-contains.md)) — root-
  caused (these iterator classes are shared program-wide, so their
  fields are a genuine cross-instance union unlike `dict`/`set`'s own
  same-instance artifact 076 fixed) and fixed instead via
  `__pyc_clone_constants__` per-receiver-CS splitting, the same lever
  `__list_iter__`/`range` already use
  ([ifa/045](../ifa/issues/closed/045-receiver-cs-method-cloning.md)).
  `webserver.py` now compiles and runs correctly again. One narrower,
  pre-existing (not newly introduced) limitation remains: bare
  module-level `.keys()` calls (outside any function) don't get the
  same per-call-site splitting. Full trace in the issue.
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
- [018-dict-mixed-key-types-boxing-failure.md](018-dict-mixed-key-types-boxing-failure.md)
  — A program using `dict` (or `set`) with two different key/
  element types anywhere fails to compile with a `BOXING`/"mixed
  basic types" FA violation — each shared internal comparison
  method (`_keys[i] == key`) isn't specialized per key type. No
  container needed, either: any raw scalar (e.g. `int | str`) union,
  however it arises (branch merge, function return, list literal),
  has no coherent runtime representation for a generic consumer
  (`+`, `print`, `isinstance`, ...) to dispatch on — this is also
  the real blocker behind `ifa/issues/025`'s three originally-filed
  narrowing cases, not a narrowing gap.
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

- [040](closed/040-percent-format-float-arg-int-specifier-garbage.md)
  — `"%d" % <float>` (Python truncates; valid and common) produced
  deterministic garbage instead of the truncated integer, on both
  backends. Root cause: `_CG_format_string` forwarded the Python
  format string and raw C varargs directly to `vsnprintf`, and a
  `double` argument reaching a `%d` specifier is undefined behavior in
  C (ABI mismatch — float varargs and `%d`'s `va_arg(int)` read from
  different register classes on x86-64 SysV). Fixed at both backends'
  codegen sites (not the frontend `__mod__` lowering — argument types
  aren't known until post-FA): parse the format string's specifiers
  when it's a compile-time constant, insert an explicit `(int64)` /
  `(double)` cast when a specifier and its argument's resolved type
  disagree. New regression test
  `tests/format_string_int_float_mismatch.py`; full suite clean, both
  backends. yopyra's downscaled render no longer shows the garbage
  signature but still doesn't byte-match CPython — confirmed **not**
  this bug (an isolated test of the exact `color.__str__` expression
  matches CPython exactly) — some other, separate, not-yet-diagnosed
  divergence in yopyra's ray-tracing math, not chased further.
  Separately (and unrelated): `print(x, file=some_file_object)`
  doesn't actually write to `some_file_object` — output goes to
  stdout regardless — found via yopyra's own `.ppm`-writing code;
  flagged, not filed, not fixed.
- [044](closed/044-list-add-mutates-receiver.md) — `list.__add__`
  (`+`) mutated its left operand's list header in place and returned
  that same object, instead of allocating an independent result —
  invisible whenever the left operand went unread afterward, silent
  data corruption the moment it was aliased elsewhere (an object
  field, another variable). Root cause of
  [issues/025](025-shedskin-examples-coverage.md)'s TODO item 2
  (rubik2's ~7675-move degenerate phase-0 "solution", vs.
  Thistlethwaite's proven ≤7-move bound). Fixed in
  `pyc_c_runtime.h`/`pyc_runtime.c` (both backends carry independent
  copies of this primitive); new regression test
  `tests/list_add_no_mutate.py`; full suite clean on both backends
  (257/11/0/4, no regressions). rubik2.py itself still doesn't
  complete in reasonable time post-fix — separate, not-yet-root-caused
  (044's "Residual" section) — and a related LLVM-backend segfault
  (a class constructed both with and without its default-`None`
  constructor arg) was found and filed separately as
  [ifa/issues/088](../ifa/issues/closed/088-llvm-class-list-field-plus-construct-segfault.md).
- [034](closed/034-iadd-fallback-and-mixed-numeric-regression.md) —
  `+=`/`-=`/etc. against a class defining only the non-in-place
  operator (`__add__` without `__iadd__`, the common case — CPython
  falls back to it automatically) always failed with "unresolved call
  '__iadd__'"; fixed by auto-synthesizing the 12 `__i<op>__` →
  `__<op>__` fallback methods (`gen_class_pyda`, unconditional, same
  shape as `__deepcopy__`'s existing auto-synthesis). Verifying it
  surfaced a real regression in
  [ifa/077](../ifa/issues/closed/077-primitive-equality-codegen-missing-salvage-guard.md)'s
  own recent type-mismatch guard — it flagged ordinary `int * float`
  as a mismatch via raw C-type-string comparison; fixed with the same
  num_kind-based numeric tolerance 077's other half already used.
  Found via `shedskin_examples/yopyra/yopyra.py`, which now compiles
  with zero warnings on both backends and runs.
- [033](closed/033-comprehension-filter-and-or-boolean-context-gap.md)
  — `and`/`or` used as a comprehension `if`-filter
  (`[x for x in xs if a and b]`) built the crash-prone value-preserving
  union instead of the boolean-context bool-only form an existing
  optimization (issue 025, `python_ifa_build_if1.cc`'s
  `in_boolean_context`) already applies to plain `if`/`while`/`elif` —
  `PY_list_if`/`PY_comp_if` just weren't in its recognized parent-kind
  list. Same crash signature as
  [ifa/071](../ifa/issues/071-FA-chess-accumulated-union-notype-cascade.md)
  (`mismatched field sizes: class 'closure'...`), a third independent
  source of it. Found via `shedskin_examples/yopyra/yopyra.py`, which
  now compiles (a second, unrelated `__iadd__`-fallback gap in that
  file is tracked separately, not yet fixed).
- [032](closed/032-dict-view-membership-missing-contains.md) —
  `x in d.keys()` / `x in d.values()` / `(k, v) in d.items()` were
  completely unresolvable, not an imprecision bug: `in` dispatches
  directly to the right operand's `__contains__`
  (`python_ifa_build_if1.cc:168`, no iterable-protocol fallback), and
  `__pyc__/07_dict.py`'s `__dict_iter__`/`__dict_items_iter__` (what
  those three methods return) never defined one. Found via
  `shedskin_examples/webserver/webserver.py`
  (`s in self.mapSocks.keys()`), reduced to a 5-line module-level
  repro. Fixed by adding a linear-scan `__contains__` to both classes,
  matching `dict.__contains__`'s own style. New test:
  `tests/dict_keys_values_membership.py`. `webserver.py` now compiles
  with zero warnings and actually serves a real HTTP request
  end-to-end.
- [014](closed/014-generators-yield-unimplemented.md) — generators
  (`yield`) are feature-complete on both backends: core landed
  2026-07-14 as a C++20-coroutine C backend (`is_generator`
  split-Fun design, `__pyc_generator__`), then in four more passes:
  LLVM backend (`5872fafc`, LLVM coroutine intrinsics — surfaced and
  fixed a genuine LLVM `CoroSplit` bug, also latent in the
  pre-existing `is_async` path, reproduced independently on LLVM
  18/20/22); `while True:`-bodied (no `break`) generators (`a016b9dc`
  — FA only types a Fun's return from a *live* `P_prim_reply` node,
  fixed via an opaque always-reachable branch to the exit label);
  real `StopIteration(value)` on exhaustion (`00f74a26` — also found
  and fixed `pyc_program_has_raise`'s builtin-module raise gating not
  covering `yield`, the same class of gap `assert` needed for
  `__pyc_assert_fail__`); and `yield from` (`676b4fcb`, a hand-built
  delegation loop mirroring `try/except`'s own IF1 shape — also found
  and fixed a separate, pre-existing bug where *any* `raise` inside a
  generator body was silently masked as `StopIteration`, since
  `_CG_generator_advance`/`_CG_generator_send`'s `done` signal alone
  can't distinguish normal completion from an unwound raise). Known
  remaining gaps, documented not fixed: `for` loops over a
  raise-internally generator still don't propagate the exception
  (a for-loop-lowering gap affecting every iterator type, not
  generator-specific); `yield from` only supports generator targets,
  not arbitrary iterables (needs `.send()`). Generator expressions
  (issue 008, resolved separately via eager `list` materialization)
  were never part of this issue's own mechanism.
- [031](closed/031-eq-none-dispatch-crash.md) — `x == None` /
  `x != None` now lower directly to an isinstance-against-nil check,
  mirroring the existing `is None`/`is not None` treatment, instead
  of dispatching through the generic `__eq__`/`__ne__` method. The
  old dispatch monomorphized container methods like `list.__eq__`
  assuming their argument was another instance of the same
  container, crashing at runtime (`getter not resolved`) when the
  argument was the `None` literal — root cause of
  `shedskin_examples/chaos/chaos.py`'s crash
  (`Spline.__init__`'s `if knots == None:`, `knots: Optional[list]`).
  `chaos.py` now runs to completion on both backends. New test:
  `tests/eq_none.py`.
- [023](closed/023-structural-pattern-matching.md) — `match`/`case`
  (PEP 634): every pattern kind implemented and matching CPython on
  both backends, including all three rest-capture forms (`*rest`,
  `**rest`, positional class patterns via `__match_args__`) and every
  `case None:` combination. The last limitation — `case None:` mixed
  with a narrowing/capturing pattern — was resolved 2026-07-21 by
  [ifa/issues/060](../ifa/issues/closed/060-none-branch-dropped-mixed-with-literal-bool-sequence.md)
  (isinstance wrapper-clone sharing + the general `None`-plus-scalar
  contour merge, fixed in `type_cannonicalize`); the compile-time
  guard was then removed. `tests/match_none.py`. (Note: distinct from
  the also-closed [023-tuple-missing-eq-str](closed/023-tuple-missing-eq-str.md)
  — a pre-existing frontend numbering collision.)
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
  type-check — [ifa/issues/040](../ifa/issues/closed/040-empty-list-shared-clone-type-inference.md).
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

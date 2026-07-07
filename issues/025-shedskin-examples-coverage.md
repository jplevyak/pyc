# Issue 025: shedskin examples as an incremental pyc coverage corpus

**Status:** open (tracking corpus). Baseline established 2026-07.
**Affects:** frontend scoping (`python_ifa_*`), import resolution
(`build_import_if1`), the DParser grammar (`python.g`), and FA
(`unresolved call`). Not a single bug — an umbrella tracking the
real-world programs pyc can't yet compile and the order to attack
them.

## What this is

`shedskin_examples/` is the `examples/` tree of the
[shedskin](https://github.com/shedskin/shedskin) project (a
Python-to-C++ compiler with a similar remit), vendored into this
repo via **git subtree with `--squash`**. 86 example programs of
idiomatic, runnable Python — a far broader corpus than `tests/`,
which is mostly minimal feature probes. The goal is to
*incrementally* extend pyc until it compiles and runs them.

### Provenance / updating

- Imported from upstream `examples/` at commit `6646da74`
  (upstream `master` was `d01d4948` at import time).
- `--squash` was deliberate: the examples carry 889 commits of
  history and multi-MB data/asset files; we want the sources as a
  test corpus, not that history in pyc's `.git`.
- To pull upstream updates later:
  ```sh
  # in a shedskin checkout:
  git subtree split --prefix=examples -b pyc-examples-split
  # in pyc:
  git subtree pull --prefix=shedskin_examples \
      ../shedskin pyc-examples-split --squash
  ```
- The bundled `shedskin_examples/.gitignore` already excludes the
  per-example compiled binaries, so building examples in place
  won't dirty the tree. Do NOT commit pyc build outputs
  (`<name>.c`, executables) either — the sweep below builds in a
  scratch dir for exactly this reason.

## Measuring coverage

`./shedskin_sweep.sh` (repo root) runs `pyc -D <root> <name>.py`
on every `shedskin_examples/<name>/<name>.py` in an isolated
scratch build dir and buckets the outcome by first diagnostic.
Pass a substring to sweep a subset (`./shedskin_sweep.sh sudoku`).
It reports "N compiled to C" and a normalized failure histogram.

## Baseline — 2026-07 (0 / 77 reach C)

None compile to C yet; this is the honest starting line. The
failures cluster into four actionable buckets plus a couple of
deep crashes. Ordered by how close each bucket is to working:

### A. `unresolved call` — 7 examples (investigated 2026-07: NOT "one dispatch away")

`block` (`__sub__`), `dijkstra2` (`__gt__`), `life` (`__iter__`),
`loop` (`__iadd__`), `mwmatching` (`__iter__`), `oliva2`
(`__add__`), `timsort` (`__gt__`).

The baseline framing ("each is one missing dispatch away") was
wrong. The unresolved operator is a *downstream symptom*: the
receiver has no type because inference broke further up. Every one
of these has cascading `'X' has no type` warnings before the
`unresolved call`. Root causes found so far are unrelated to the
operators themselves:
- `dijkstra2` — `len(sys.argv) > 1`; the `import sys` failure
  (bucket C) leaves `sys.argv` untyped, so `__gt__` has no
  receiver. It's really a bucket-C example.
- `timsort` — `Timsort(list_, comparefn=comparefn)` stores a
  *function* in an instance field and later calls it
  (`self.comparefn(...)`). Blocked on **first-class functions in
  struct fields** (see below), a deep FA gap. The `__gt__` at
  :1033 is in `range_check`, never reached with typed args.
- `life` — `map(process, generator(...))` plus `itertools.product`
  / `collections.defaultdict`; needs those higher-order builtins.

**Two real, standalone bugs found and fixed while digging here**
(neither fully clears an example — they were necessary but not
sufficient — but both are genuine correctness fixes):
1. **Keyword args were silently dropped from calls.** `PY_call`
   collected `name=value` pairs and even built their values but
   never added them to the emitted send, so the callee's params
   went untyped. Fixed: keyword actuals are now added to the send
   under their (interned) parameter name; the IFA matcher already
   supports named→positional mapping. `tests/keyword_args.py`.
2. **Keyword args to constructors failed** even for plain values:
   the `__new__` wrapper's formals were created unnamed, so no
   `named_to_positional` entry existed to bind a keyword actual.
   Fixed: wrapper formals are named after the `__init__` params.

**Known remaining blockers for this bucket** (each its own task):
- First-class functions stored in struct fields and called through
  them (`self.fn(...)`) — timsort, and a common callback pattern.
- Out-of-order keyword args (`f(high=9, low=2)`) still fail to
  compile (safely — no miscompile). In-order works.
- Higher-order builtins `map` / `filter` / `itertools.product` /
  `collections.defaultdict` — life, others.
- The `import` bucket (C) masks dijkstra2 and likely more.

Net: bucket A is not a quick win. It bottoms out in first-class
function values and higher-order builtins.

### B. `'X' redefined as local` — 18 → 0 examples (FIXED 2026-07)

Originally 18: `chess collatz dijkstra go linalg minpng othello
path_tracing pisang pygmy pylife sat sudoku2 sudoku3 sudoku4
sudoku5 sunfish tictactoe`.

**Root cause (the common case, fixed):** comprehension bodies were
symbol-walked before the `for`-chain bound the loop targets, so an
element that referenced its own target (`[i for i in xs]`)
resolved `i` as a not-yet-bound USE, fell through to the module
scope, and created a spurious global. A second same-named
comprehension then died with `'i' redefined as local`. All four
comprehension forms (list/gen/set/dict) shared the bug. Fix:
`build_comprehension_body_syms` in `python_ifa_build_syms.cc`
walks the `for`-chain (always the last child) before the element
expressions, matching CPython's "comprehension is a function whose
params are the targets." Regression test:
`tests/comprehension_reused_var.py`. This advanced 13 of the 18
past this error (most now land in bucket C, sudoku5 in D's
generator-expression gate).

**Remaining 4 — SECOND root cause, also fixed 2026-07.** `go`
`color` @373, `path_tracing` `direction` @245, `pygmy` `position`
@205, `sunfish` `board` @202 all shared a *different* bug: an
attribute name in `x.attr` was resolved as a variable reference by
the symbol-table pass (`PY_attribute` fell through to the generic
recurse-all-children case), which looked the name up, failed, and
created a spurious *module global* for every attribute name in the
program. Inert until that same name was later a reassigned
parameter/local — then the store saw the global sentinel and died
"'X' redefined as local". E.g. go's `[SHOW[sq.color] …]` created a
global `color`, poisoning `update_path`'s `color` parameter.
Diagnosed by instrumenting the global-creation path (the offending
name was created at the comprehension line, not the failing line).
Fix: a dedicated `PY_attribute` case in `build_syms_pyda` that does
NOT resolve the attribute-name child (build_if1 already consumes it
as a raw string via `make_symbol`). Regression test:
`tests/attr_name_not_global.py`.

**Bucket B is now fully eliminated: 18 → 0.** All 18 advanced —
most to bucket C (imports), `go` to bucket E (the `if1_send`
crash), `sudoku5` to D (generator expressions).

### C. missing-module imports — module subsystem BUILT 2026-07; per-module shims ongoing

After the bucket-B fixes this grew to ~40 examples (B's examples
flowed in). The imports needed, by frequency: `time` (32,
mostly benchmark timing), `random` (13), `math` (12), `sys` (6),
`copy` (5), then a long tail (`struct`, `itertools`, `functools`,
`colorsys`, `array`, `re`, `getopt`, plus quameon's local
submodules).

**UPDATE 2026-07 — the module subsystem now works.** What was
called a "deep blocker" below is implemented (commits after
`82d2fa1b`):
- **Phase 1 — `from X import Y [as Z]`**: `build_import_syms` now
  binds the imported name into the importing scope from the
  module's saved top scope. Functions, classes, and data all
  import and are callable cross-module.
- **Phase 2 — `import X [as Z]`**: `X` binds to a module-marker
  Sym (`is_module`), mapped to its `PycModule` in
  `PycCompiler::module_syms`. `build_if1`'s `PY_power` resolves
  `X.attr` to the module member at compile time (modules are
  compile-time namespaces, not runtime objects). Unknown members
  give a clean "no attribute" diagnostic.
- **Phase 3 — stdlib shims** under `pyc_lib/` (on the search path
  at `<system_dir>/pyc_lib`, after the cwd): `math` (elementary
  functions → libc `<math.h>` via `__pyc_c_call__`, results match
  CPython) and `time` (`time.time()` whole-second wall clock;
  no-op `sleep`). Tests: `tests/from_import.py`,
  `tests/import_module.py`, `tests/math_module.py`,
  `tests/time_module.py`, all both backends.

Effect: `import math`/`import time`/`from X import Y`/`import X;
X.attr` all resolve and run. The module-blocked count dropped only
slightly in the sweep (40→38) because most examples import
*several* modules and still lack `random`/`sys`/`copy` shims — and
once past imports they hit their *next*, non-import blocker (type
inference, destructuring, the `if1_send` crash). The import wall
itself is gone.

**Shims landed 2026-07:** `math`, `time`, **`random`**, **`sys`
(easy parts)**. `random` is a pure-Python LCG (seed/random/uniform/
randrange/randint/choice/shuffle) — deterministic given a seed but
NOT CPython's stream. `sys` provides `argv` (stub `["pyc"]`),
`maxsize`, `exit`, `setrecursionlimit` (no-op). With these four
shims the module-blocked count fell **40 → 14** in the sweep.

**Remaining shim work:**
- `copy` — INVESTIGATED, not an easy win. Live usage is
  `copy.deepcopy` (recursive; not modellable under static typing).
  Shallow `copy.copy` via the `clone` primitive is unsound: a
  cloned live object loses its field types in flow analysis (the
  primitive is built for the `__new__` wrapper, where `__init__`
  re-initializes the fields immediately after). Needs either a
  type-preserving shallow-copy mechanism in FA or real deepcopy.
- `sys` file objects — `sys.stdout`/`stderr`/`stdin`. Two examples
  (othello2, path_tracing) now hit the clean
  "module 'sys' has no attribute 'stdout'" diagnostic. Needs a
  file/stream object type.
- long tail: `struct`, `itertools`, `functools`, `colorsys`,
  `array`, `re`, `getopt`, plus quameon's local submodules.

The 14 still-module-blocked are mostly `copy` importers and the
long tail. Examples that cleared imports now surface their next
blocker, which is NOT module-related: tuple destructuring, the
`if1_send` crash (bucket E), FA timeouts, or -- newly exposed by
getting further -- a couple of segfaults in later code
(pystone, tictactoe). A known LLVM-backend bug also surfaced:
`int_fn() / float` mis-types as an integer op when the int comes
from a mutable-global helper (worked around with explicit `float()`
in the random shim; worth fixing at the source).

Historical note (the original bucket-C framing, now resolved):

**Crash fixed (committed).** `build_import_if1` did `assert(m)` on
the module `get_module` returns, which is null for any unfound
module — so an `import time` aborted the whole compiler with
SIGABRT. Replaced with a `fail()` diagnostic naming the module and
line.

**Why coverage is still blocked — a two-layer subsystem gap, NOT a
quick win.** I prototyped stdlib shims (a `pyc_lib/` on the search
path with `math.py` calling libc via `__pyc_c_call__`) and found
the shims can't be used, because **pyc cannot call functions from
an imported module at all** — even a trivial local
`import mymod; mymod.double(21)` fails with `'mymod' has no type`,
and `from mymod import double; double(21)` fails too. The import
machinery (`build_import_syms`) loads a module's *symbols* but
never binds the imported names into the importing scope; only the
builtin `__pyc__` module is wired in (via the single
`import_scope` call at `python_ifa_build_syms.cc:1007`). The
existing `tests/module_import.py` only ever imports for a side
effect (`print(__name__)`), so this was never exercised. Two
distinct capabilities are missing, both needed since the corpus
uses both styles (≈55 `import X` vs ≈30 `from X import Y`):
  1. `from X import Y` — bind `Y` (a module symbol) into the
     current scope. The more tractable half: a flat-namespace
     merge like the builtin module already gets.
  2. `import X; X.attr` — needs **module objects**: `X` bound to a
     value whose attribute access resolves module members. A real
     feature (module-as-value + `.` dispatch), the dominant style.

On top of that, each stdlib module needs a shim with runtime
support: `math` maps cleanly to libc (`__pyc_c_call__(float,
"sqrt", ...)`, correct); `time`/`random` need primitives and are
nondeterministic (so timing/PRNG examples can't have exact-match
goldens); `copy.deepcopy` is hard under static typing.

The prototype (search-path entry + `pyc_lib/math.py`) was reverted
as non-functional; the crash fix stands. **Recommended:** treat
"module import + module objects" as its own issue — it's the real
unlock here and also what bucket A's `import sys` cases need — and
land it before writing shims. Then `math` (correct, libc-backed)
is the first shim; `time`/`random`/`sys` follow with the caveat
that their examples become compile-only (no exact-output goldens).

### D. syntax errors — ~25 examples (DParser grammar gaps)

`ac_encode adatron astar bh chaos circle doom hq2x lz2 mao
mastermind2 nbody neural1 plcfrs rdb rubik2 score4 sokoban
solitaire stereo tonyjpegdecoder voronoi voronoi2 webserver
yopyra`

Assorted `python.g` gaps — several involve triple-quoted-string
edge cases (`rdb`, `tonyjpegdecoder`), others distinct
constructs. Each is its own grammar fix; triage individually and
fold minimal repros into the DParser test set.

### E. deep crashes — 1

`amaze`: `if1_send` assertion `v' failed` (past the frontend,
crashes building IF1). Needs its own investigation.

(`othello3`, `rsync`, `sha` produced no captured diagnostic —
likely a differently-worded message or the 60s compile timeout;
re-run individually to classify.)

## Recommended order

1. ~~**B (`redefined as local`)**~~ — DONE, 2026-07: 18 → 0. Two
   root causes, both fixed (comprehension target-binding order +
   attribute names resolved as variables); see bucket B.
2. **A (`unresolved call`)** — investigated 2026-07: not "one
   dispatch away." Fixed two real keyword-argument bugs along the
   way; remaining blockers are first-class functions in struct
   fields and higher-order builtins. See bucket A.
3. ~~**C** (make the import path a diagnostic, not `assert(m)`)~~ —
   crash DONE, 2026-07. Actual coverage blocked on a module-import
   subsystem (module objects + cross-module name binding) plus
   per-module shims; treat as its own issue. See bucket C.
4. ~~**module import + module objects**~~ — DONE, 2026-07: from-import
   name binding, `import X`/`X.attr` module objects, and `pyc_lib/`
   shims for math/time/random/sys. Module-blocked 40 → 14. See
   bucket C.
5. ~~**illegal destructuring**~~ — DONE, 2026-07. Tuple-unpacking to
   attribute (`self.x, self.y = ...`), subscript (`a[i], a[j] = ...`
   swaps), and nested targets now works (recursive assign in
   `build_if1_assign_target`). Cleared fysphun/softrender/sat off
   this error. Test `tests/destructuring_targets.py`.
6. **D** — grammar fixes, one construct at a time (~24 examples,
   the largest remaining bucket).
7. **type resolution** — the deepest bucket and the real gate to the
   first *green* example: `unresolved call` / `has no type` /
   `_CG_any` (void\*) in generated C (~19 examples). Investigated via
   the mandelbrot vertical slice 2026-07; findings below.
8. **crashes** — PARTIALLY DONE 2026-07. Two assert crashes fixed:
   - `if1_send` assert (amaze, go, othello, sudoku3): `try`/`except`/
     `finally` were routed to the WITH-item handler, mis-lowering the
     try body to a null rval. Now `try` has its own handler (build
     body + else/finally, skip except); exception handling stays
     unimplemented (issue 011) but no longer crashes. Test
     `tests/try_body_runs.py`.
   - `coerce_immediate` assert (sudoku2): `fold_constant` numerically
     coerced a string operand in a mixed op (`str == int`). Now it
     bails to a runtime op. Test `tests/const_str_compare.py`.
   Remaining: the pystone/tictactoe **segfaults** are a deep FA
   split-lifecycle bug (dug in fully 2026-07). The crashing entry set
   (`make_AVar(Proc1, es#N)` first) has `display.n == 0`, **`edges ==
   0`**, and `es->split` set -- an **emptied split ES**: all its
   edges were moved away by a further split, but it is still being
   analyzed. Because it never went through (or was un-done from)
   `set_entry_set`, none of its per-ES state is populated -- display,
   `rets`, and `args` are all empty. Fixing one symptom just exposes
   the next: a `make_AVar` guard that walks `es->split` for a display
   removes the `make_AVar` OOB (fa.cc:218), and the crash then moves
   to `analyze_edge` (fa.cc:2498) dereferencing `ee->to->rets[i]` on
   the same empty ES. So a guard is whack-a-mole; the real fix is in
   the **split machinery** (`split_edges` / `check_split` /
   `make_entry_set`): an ES whose edges are all split away must be
   removed from the worklist and not analyzed (or the split must fully
   re-`set_entry_set` the ES before it is enqueued). That is a
   substantial, risky change to core FA convergence. Left open, but
   now precisely located: emptied split ES analyzed ->
   `make_AVar`:218 (display) then `analyze_edge`:2498 (rets), both on
   an ES with `edges==0 && split!=null`.

Newly-surfaced non-module blockers after clearing imports +
destructuring: `open()`/file I/O (softrender), generator
expressions (sat, sudoku5; issue 014), FA convergence timeouts
(fysphun).

### Type-resolution investigation — mandelbrot vertical slice (2026-07)

**Root cause of the `_CG_any`-in-generated-C failures: loop-carried
numeric type unification.** mandelbrot does
`zi = 0; zr = 0; while True: ...; zr = zr2 - zi2 + cr; zi = temp +
temp + ci`. `zr`/`zi` are **int on loop entry** (the `0`) and
**float after the first iteration**, so the loop-head phi merges
`{int, float}`. That union has no single concrete C type, so
concretization emits `_CG_any` (void\*) for the variable and codegen
then does `(_CG_any)<double>` — a double-to-void\* cast, which is
invalid C (and invalid LLVM IR). Minimal repro:
`x = 0; while ...: x = x * 2.0 + 1.0`. (A *non*-loop
`z = 0; z = 1.5; z*z` is fine — SSU separates the two defs; only the
loop back-edge forces a genuine union.)

**Promoting the type is necessary but NOT sufficient — coercion
insertion is the hard part.** I prototyped the promotion:
`make_LUB_type` (currently the default identity in `ifa.h`, unridden
by pyc) reducing an all-numeric `Type_SUM` via `coerce_num`
(`{int,float}->float`), plus relaxing the `BOXING` type-violation
guard (`mixed_basics` in fa.cc) for all-numeric sets. That makes the
variable's *type* float and the pyc→C step succeed, but it is
unsound: the int values flowing in (the initial `0`) are never
**coerced** to float, so at runtime the dispatch for `zr * 2.0`
finds no matching function ("matching function not found") and the
LLVM backend rejects the IR. Reverted as fragile.

**Second attempt — Go/Dart untyped-constant coercion — and the
fundamental obstacle it hit (2026-07).** The clean approach is to
let a numeric literal adopt the wider type when it meets one:
`x = 0` in a float loop becomes `0.0`, coercion is compile-time and
free. Proven by construction — `x = 0.0` (float literal) compiles
and runs; `x = 0` (int literal) is the *only* difference. I
prototyped it as `coerce_numeric_constants(AType*)` called from
`update_in`, rewriting a numeric-mixed AVar type to the widest
numeric (`coerce_num`). Two findings:
  1. By the time the union forms at the loop-head phi, the literal
     `0` has already been **widened from the constant to the generic
     `int64` type** — so a constant-only rewrite never fires; the
     narrower member is an abstract `int64`, not the literal.
  2. **The real blocker is monotonicity.** Rewriting an AVar's `out`
     to drop `int` and keep `float` is *non-monotonic*: in pyc's
     lattice `int` and `float` are siblings under `any`, not
     ordered, so removing `int` shrinks the type. FA is a monotone
     fixpoint (`type_union` only grows types); a shrinking `out`
     violates the worklist invariants and **segfaults** (consistently
     once the state is corrupted). Coercion simply cannot run *during*
     convergence. And post-convergence is too late — polymorphic
     dispatch for `x * 2.0` was already baked from the `{int,float}`
     receiver during FA (this is why the earlier `make_LUB_type`
     type-only change failed at runtime).

**What a real fix requires — a new lattice element, not a coercion
pass.** The monotonic, Go/Dart-faithful design is an **"untyped
numeric constant"** that sits *below* every concrete numeric in the
lattice (a numeric ⊥). Then a literal contributes `untyped`, and
`untyped ∪ float = float` is a proper monotone *join* (float is
above untyped — nothing is removed), so the loop variable is `float`
throughout FA, dispatch is monomorphic, and the fixpoint stays
stable. Pieces: (a) a CS/type kind for untyped numeric literals;
(b) `type_union` absorption `untyped_num ∪ concrete_num =
concrete_num` (and `untyped ∪ untyped = untyped`); (c) a
post-convergence default `untyped → int` for literals that never met
a concrete type; (d) codegen emitting the constant in its resolved
type. This is a real type-system feature (worth its own issue), but
it is the correct and *stable* shape — the naive "coerce the union"
and "promote the LUB" shortcuts are both dead ends (crash / runtime
dispatch failure respectively), now proven.

**Third attempt — absorb the constant inside `type_cannonicalize`
— and the invariant it exposed (2026-07).** `type_cannonicalize`
*already* strips a constant when its base type is present
(`{const_0, int64} → {int64}`), and does so stably (the absorption
is part of forming the canonical value, not a post-hoc mutation), so
this looked like the right home: extend it to drop a numeric
constant when a *wider* numeric is present (`{const_0, float} →
{float}`). Bisected result: computing the widest type is harmless;
the **strip itself segfaults**, consistently. The reason is a core
invariant — *the type a value flows in as must remain in the AVar's
type*. The existing strip is safe precisely because it keeps the
base type (`int64` stays); mine removes `int64` while an int value
(the `0` move) still flows in, and downstream (clone/concretize)
crashes looking the type up. So even canonicalization cannot drop a
concrete type that a value physically flows as.

**Consequence — the real shape is confirmed as coercion at the
flow, not any type-set edit.** For `x` to be float-only, the literal
`0` must *itself flow as float* — i.e. the `x = 0` move must become
`x = 0.0`. That is a post-convergence IR rewrite of the constant at
its source (cheap and lossless for a literal), after which the type
is uniformly float with no invariant violation. Equivalently, the
untyped-numeric lattice element (above) works only if it is threaded
through **all** consumers (FA dispatch, clone/concretize, codegen) so
none of them see a bare int flowing into a float-typed var. Either
way it is a multi-subsystem change; three post-hoc shortcuts
(mutate `out`, promote the LUB, strip in canonicalize) are now each
proven to fail, and for understood reasons.

The other type-resolution symptoms (`unresolved call`, `has no
type`) are a mix of this same numeric-union problem surfacing at a
call and the bucket-A first-class-function-in-field gap.

Re-run `./shedskin_sweep.sh` after each change; the bucket counts
are the regression/progress signal. As examples start reaching C
(and running), promote the stable ones into `test_pyc.py` with
`.exec.check` goldens so they don't regress.

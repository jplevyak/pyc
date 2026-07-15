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
6. **D — grammar/scanner** — INVESTIGATED 2026-07. Down to 8
   examples (module/destructuring/etc. fixes advanced the rest):
   astar, mao, neural1, path_tracing, plcfrs, rdb, solitaire,
   voronoi2. These are subtle DParser scanner/GLR issues, not simple
   missing grammar rules, and fixing them won't reach a *green*
   example on its own (downstream blockers remain). Concrete finding:
   an **exponent float immediately followed by `]`** fails to parse
   (mao's `> 1.0e-17:`... actually the `]` case in path_tracing's
   `V3(0.0, 10e6, 0.0)` lists). Minimal repro: `z = [1e5]` FAILS but
   `z = [1e5 ]` (one space) PARSES; `{1e5}`, `(1e5)`, `1e5,`, `1e5;`
   all parse -- only `<expfloat>]` with no separating space fails.
   `[1.5]` (no exponent), `[10]`, `[0x1f]` all parse, so it is
   specific to the exponent part (`[eE][+-]?[0-9]+`) abutting `]`. It
   is NOT the number regex: DParser treats the `.` in the NUMBER rule
   as a literal dot already (origin rejects `1x5`), so escaping it to
   `\.` is a no-op. This is a genuine DParser scanner/longest-match
   vs GLR ambiguity at the `]` boundary and needs dedicated
   DParser-scanner work (grammar changes require a slow
   make_dparser regeneration). The triple-quoted-string cases
   (solitaire, plcfrs, rdb) do NOT reproduce with minimal triple
   strings (embedded `"`, `|`, `<`, `:` all parse) -- something more
   specific in those files; astar/neural1/voronoi2 are separate
   constructs. Left open; low ROI relative to the type-resolution and
   split-lifecycle work.
7. **type resolution** — LANDED 2026-07, and it produced the
   **first green examples: mandelbrot, nbody, neural2 compile to C
   (0 → 3), with mandelbrot verified byte-identical to CPython at
   runtime** (15,600 fractal lines). The working design, after the
   three documented dead ends: a persistent per-AVar annotation
   (`AVar::num_coerce`) set BETWEEN passes by
   `fa_coerce_numeric_confluences` (called from
   `PycCompiler::reanalyze`, which then `clear_results()`s so the
   re-run re-derives flow from scratch), applied in `update_in` /
   the permit variants as an element-wise monotone rewrite: every
   numeric CS ≠ target maps to the target (constants to coerced
   constants, 0 → 0.0; abstract narrows to the abstract target).
   Flow- and contour-sensitive per (Var, contour) — an int-only
   specialization of the same code keeps int (a source rewrite
   could not do this; a confluence can also arise against a
   restrict-narrowed monomorphic numeric far from any MOVE).
   Annotation eligibility: ES-contour vars + `closure`-CS ivars
   (pyc lowers locals through the closure frame); USER record
   fields stay on the classtag-dispatch machinery. Two subtleties
   that shaped the design: (a) the constant cap
   (num_constants_per_variable) strips loop-accumulated constants
   to abstract base types, so the annotation must map abstract
   narrows too, and eligibility can't require a visible narrow
   constant; (b) the violation collector skips phi-carriers
   (only_used_by_phy_or_phi), so annotation scans all AVars
   directly, not the BOXING violations. Deliberate CPython
   divergence (shedskin-style, documented): on a path where the
   variable never leaves its int initializer, it holds the float.
   Companion fixes that completed the green examples: Python-3
   true division (`int.__truediv__` coerces both operands to
   float; `//` untouched; `__pyc__`'s own `__hex` switched to
   `//`) and `str.join`. Tests: `numeric_unification`,
   `true_division`, `str_join` — all both backends; suites 166/0,
   ifa-test unchanged (base callback never annotates).
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
   **The "signal 117" (SIGSEGV) family fully dug into and resolved
   2026-07-14** — it turned out to be FOUR unrelated bugs sharing an
   exit code (amaze, othello2, othello3, pystone, score4, tictactoe,
   voronoi2):
   - **pystone: the bare/unpopulated split-ES crash** (the
     "emptied split ES" previously located above — the earlier
     `edges == 0` reading was close but not exact). Root cause:
     `split_edges` (`ifa/analysis/fa.cc`, the dispatch splitter)
     re-pointed an edge's `->to` at a `find_or_make_filtered_entry_set`
     product with a **bare assignment** instead of `set_entry_set` —
     and those products are BARE EntrySets (filters + split lineage
     only; no display/args/rets, which only `set_entry_set`
     populates). `analyze_edge`'s `make_entry_set` early-returns on a
     non-null `->to`, so nothing ever repaired it; the first
     `make_AVar(formal, es)` indexed the empty display OOB and
     deref'd garbage. Fixed by routing both `split_edges` paths
     through the full re-entry recipe `apply_entry_set_split` already
     uses (null `to`, clear stale per-edge `filtered_args`, remove
     from the old ES's edge set, `set_entry_set`). pystone compiles;
     score4/othello2 progressed past FA to downstream stages.
   - **amaze/tictactoe/voronoi2 (+ rdb, msp_ss): inheriting from
     undefined `Exception`** — resolved, see `ifa/issues/042` (real
     builtin `Exception` class in `__pyc__/08_exception.py` + clean
     "base is not a class" frontend error).
   - **othello2: break-label scoping miscompile** —
     `PY_for_stmt`/`PY_while_stmt` (`python_ifa_build_syms.cc`)
     overwrote the enclosing scope's `lbreak`/`lcontinue` and never
     restored them, so a `break`/`continue` AFTER a nested loop (but
     inside an outer one) bound to the INNER loop's label — lowering
     to a goto placed directly after its own target label: an
     infinite loop at runtime and a no-path-to-exit CFG region that
     crashed the dominator build (`ifa/optimize/dom.cc`
     `df_traversal`, null Dom). Fixed with save/restore (also
     restored before a loop's `else_clause`, whose break/continue
     belong to the outer loop, and clean errors for break/continue
     outside any loop). Regression test
     `tests/break_after_inner_loop.py` (both backends). Dominator
     construction additionally prunes universe-escaping edges as
     defense-in-depth, and `PNode::dom/rdom` are now explicitly
     null-initialized.
   - **othello3: scanner indent-stack overflow** — the
     `PythonGlobals::indent_stack` tape (pushes definitive in the
     scanner, pops speculative in GLR reduction code, so discarded
     branches leak entries and high-water grows with file size)
     overflowed its fixed 1024 slots on the 23k-line file and
     scribbled until SIGSEGV. Now 64k entries + a clean parse-error
     guard at the push site (`python.g` / `python_parse.h`).
   - **score4: codegen null deref on FA-misspecialized container
     method** — `P_prim_sizeof_element` (`ifa/codegen/cg.cc`)
     deref'd `t->element` null when FA specialized `list.__add__`
     with an `int64` right operand (`board[y][0] + board[y][1]` on a
     list-of-lists — the type-resolution imprecision family below).
     Now a clean internal error with location. The underlying FA
     imprecision remains open (same family as the `_CG_any` /
     "expression has no type" items).
   After these fixes none of the seven segfault: pystone compiles;
   othello3 stops at generator expressions (issue 008/014); score4
   stops at the new clean internal error; amaze/tictactoe/othello2/
   voronoi2 reach FA-diagnostics/generated-C failures in the known
   type-resolution family (`_CG_any` operands, undeclared labels from
   no-type branches).

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

### "has no type" bucket — builtins batch (2026-07)

Dug the 28-example `'X' has no type` bucket: it is dominated by
**missing builtins** (an undefined name types as nothing and poisons
everything downstream). Landed:

- **12 pure-Python builtins** in `__pyc__/05_builtins.py`: `zip`,
  `enumerate`, `sum`, `min`, `max`, `reversed`, `pow`, `round`,
  `map`, `filter`, `sorted`, `repr`. zip/map/filter/enumerate/
  reversed return LISTS (shedskin-style divergence from Py3
  iterators; equivalent under iteration). min/max support both the
  two-arg and sequence forms via the **default-None + `is None`
  narrowing pattern** (verified: each call shape gets its own ES
  contour and nil narrowing prunes the dead branch -- plain-function
  arity overloading and *args do NOT work, tested). `sum` of floats
  rides the numeric unification (int seed -> float).
- **`list(iterable)`** via a frontend intercept (same shape as the
  existing `str(x)` one) dispatching to `__pyc_tolist__`, defined on
  `range` and `list`. `list(range(n))` was collatz's first blocker.
- **Tuple unpacking in for/comprehension targets**: `for i, c in
  zip(...)` (statement and comprehension forms) went through raw
  moves into a null tuple sym; both now route through
  `emit_assign_to_target` (the destructuring fix's recursive
  helper, generalized to take the emission stream).

Corpus effect: "has no type" 28 -> 23; **ant** joins the compiled
list (3 total: ant, mandelbrot, neural2); collatz compiles+runs to
its next error (line 65).

**Pre-existing soundness bug found while testing -- FIXED 2026-07**
(reproduced builtin-free): a program with two functions that
append-build lists of DIFFERENT tuple types, one indexed and one
iterated, segfaulted at runtime with a CLEAN compile
(`__list_iter__::__next__` returned null on the first iteration).
Root cause, traced through the generated C: the two grown lists
concretize as the GENERIC `_CG_list`, whose shared element sym's
type is the program-wide union of element types. With one tuple
type that's a record (size resolves); with 2+ it's a `Type_SUM`
with **no compile-time size**, and `P_prim_sizeof_element` emitted
`0` -- so `list::append` called `_CG_list_resize(list, 0, n)`, the
storage never grew, and reads returned null. The elements of a
SUM-of-records are all boxed pointers, so both emitters (cg.cc and
cg_emit_llvm.cc's `emit_send_sizeof`) now emit pointer size for
that case. Verified on the builtin-free repro and the original
zip+enumerate trigger, both backends
(`tests/tuple_list_mix.py`).

Remaining in the bucket (~23): missing I/O builtins (`open` 18
uses, `input` 4 -- need file objects / stdin), `sys.stdout/stderr`
file objects, dict/set methods (`keys`, `values`, `items`,
`get`...), and genuine inference gaps (timsort's
first-class-function-in-field, etc.).

### First-class-function-in-field — FIXED (2026-07)

The bucket-A blocker (timsort's `self.comparefn(...)`; the common
callback pattern) is fixed at its root in `P_prim_period`
(fa.cc). The period prim treated EVERY function-valued field as a
method: it filtered the function part out of the direct flow
(`type_diff` with `function_type`) and re-routed it through a
method-binding partial application (`make_period_closure`
capturing the object) -- so `self.cf(3, 1)` dispatched
`cf(self, 3, 1)` and matched nothing (diagnosed by dumping the
failing `pattern_match`: args were `[mycmp, T, 3, 1]`).

The fix implements Python's actual rule -- a function found on the
CLASS binds; a function stored as an INSTANCE attribute does not:

- METHOD-like values keep the binding path: `Fun->sym->self` set
  (real methods, capturing-def carriers) or `Sym::in` a class
  (class-body lambdas/defs -- pyc stores class attributes as
  prototype fields, so definition scope is the FA-visible
  equivalent of "found on the class"). A first, blanket unbind
  broke 62 tests; scoping by `self`-only still broke the 9
  class-body-lambda closure tests; the definition-scope rule
  restores all of them.
- BARE values (module-/function-level defs stored into instance
  attributes) flow through UNBOUND.

Verified: the timsort-shape repros (ctor-kwarg function, default
passthrough, positional store, HOF chain) all run correctly both
backends; `tests/function_in_field.py` (direct call, read-then-
call, polymorphic field with two stored functions, class-attr
lambda still binding) matches CPython exactly. Suites 167/0 both
backends; ifa-test unchanged. timsort itself now types everything
except one deeper layer (method + kwargs + FUNCTION-VALUED DEFAULT
`comparefn=cmp` through the default-arg wrapper) -- its
`range_check` params stay untyped; next onion layer, distinct from
the field gap.

### Bucket D triple-quote parse bug — FOUND AND FIXED (2026-07)

Chasing timsort's "next onion layer" bottomed out in the bucket-D
parse bug, minimized to 9 lines: **a docstring'd function followed
by a deeper-nested docstring'd method fails to parse** (CPython
parses it fine; both docstrings required; any quote style; 4->8
nesting required -- 0->4, same-depth, and deeper-then-shallower all
parsed). Root cause in python.g: `longstringitem ::= "[^\\]"` let a
triple-string body swallow quote characters freely, so two
triple-quoted strings in one file could ALSO scan as one giant
string spanning the code between them; GLR carried the scanner
ambiguity and this nesting shape made the bogus scan win. Fix:
standard triple-string lexing (body chars exclude the quote; 1- and
2-quote runs allowed only before a non-quote), making the scanner
unambiguous. Test `tests/docstring_depths.py`.

Effect: syntax errors 8 -> 2 (only mao and voronoi2's distinct
constructs remain); **astar joins the compiled set (4 total: ant,
astar, mandelbrot, neural2)**; rdb/solitaire/plcfrs/neural1 advance
to their imports. timsort (with its own Py3 bug patched -- see
below) advances to missing `id()` and exception-machinery names.

**timsort reclassified: invalid Python 3 as written.** CPython 3
raises `TypeError: 'float' object cannot be interpreted as an
integer` at line 450 (`self.tmp = list(range(ternary))` where
`ternary = length / 2` -- Py2-era true-division bug). pyc's Py3
division semantics are CORRECT here; the example needs an upstream
`//` fix. The function-valued-default "onion layer" suspected
earlier does not exist: all four minimal combinations (fn default
x positional/kwargs x function/method) plus the full
timsort()->Timsort(...)->sort(low=,high=) chain compile and run
correctly.

### "has no type" bucket dig, round 3 (2026-07-14)

Swept at 31 examples in the bucket; landed one real compiler bug fix
plus a batch of missing builtins; ended at 29 with ~8 examples
advanced past their first blocker (bh past SystemExit to a real
`__ne__` dispatch gap; block past `list(str)`; lz2, sudoku1, timsort,
tonyjpegdecoder each one layer deeper; circle past `list.sort(key=)`
to a float×Circle union; msp_ss past `Exception`).

**Compiler bug found and fixed: default arguments on methods of
non-record builtin classes never worked.** `a.sort()` silently
no-opped while `a.sort(None, False)` worked. Root cause:
`gen_fun_pyda` initializes each default's global (`g <- <default>`
MOVE) in the code stream where the `def` executes — for a method
that is the class body, which `gen_class_pyda` wraps into the class's
`___init___` closure and only CALLS for `Type_RECORD` classes.
`list`/`tuple`/`str` are core non-record builtins, so their methods'
default globals stayed bottom in FA and every defaulted call died
with NOTYPE inside `default_wrapper`'s forwarding send (invisible
until now because no non-record builtin method with defaults was ever
called relying on them — `list.index` was a `pass` stub). Fix:
literal defaults (None/True/False/number/string — no computation
code) skip the global+MOVE entirely and are referenced directly.
Computed defaults (`size=-1`) keep the old path, which non-record
builtin methods still can't use. Test `tests/list_sort_builtins.py`
(both backends).

**Builtins/stdlib landed:** the standard exception-class hierarchy
(`__pyc__/08_exception.py`: BaseException, SystemExit, ValueError,
KeyError, IndexError, RuntimeError, NotImplementedError,
StopIteration, AssertionError, OSError/IOError, TypeError, ...);
`id()` (new `P_prim_id` primitive end-to-end: prim_data, FA transfer
anchoring int64, C `(_CG_int64)(uintptr_t)`, LLVM
ptrtoint/zext/bitcast); `hash()` dispatching `__hash__` (str via new
FNV-1a `_CG_str_hash` — deterministic across runs, unlike CPython's
seeded hashing; int pre-existing); `list("abc")` via
`str.__pyc_tolist__`; `list.sort(key=None, reverse=False)` (stable
insertion sort, `<`-only comparisons per CPython's `__lt__`-only
contract — voronoi2's Site has only `__lt__`) and `list.reverse()`;
`sorted` kept `<`-only.

**Two FA split-order fragilities found and dodged (issue 033/040
family, documented for the eventual real fix):**
1. Merely ADDING `key=None, reverse=False` parameters to `sorted` —
   even unused, and regardless of whether the default is inlined or
   a global — routes its calls through a `default_wrapper`, and that
   one extra Fun shifted the splitter's trajectory enough that
   `builtins_batch`'s `sum()` lost its per-call-site contours (int
   result printed as float bits: the int64/float64 ret union with no
   boxing). `sorted` deliberately stays 1-parameter; use
   `list.sort(key=, reverse=)` instead.
2. TWO different default-subset shapes of one method in one program
   (`a.sort()` + `b.sort(reverse=True)`, or None-key + lambda-key)
   union at the method's contours and abort at runtime with
   "matching function not found" on an `_CG_any` argument. One shape
   per program works fully.

Sweep: compiled 26 → 25 — voronoi2 moved from compiled-with-warnings
to FAIL, but its prior "compiled" state silently SKIPPED
`self.__sites.sort()` (the method didn't exist; the call warned and
no-opped), so the site list was never sorted — now the sort resolves
and voronoi2 stops at its pre-existing PriorityQueue inference gaps
(no-type branches → undeclared C labels). An honest fail replacing a
silent miscompile.

### Deep-resolution dig, round 4 (2026-07-15)

**Silent-miscompile compiler bug found and fixed: a keyword argument
past an unfilled default swapped arguments.**
`PycCompiler::default_wrapper` assumed defaulted positions form a
contiguous TAIL (forward the first `has.n - defaults.n` positions,
append default expressions at the end). A kwarg can provide a LATER
positional while an EARLIER default goes unfilled — circle.py's
`pack([c], exclude=c)` with `pack(circles, damping=0.1,
exclude=None)` defaults position 3 but supplies position 4 — and the
tail construction then bound `damping <- the Circle` and `exclude <-
None`, silently. The matcher's own contract
(pattern.cc `fixup_maps_for_defaults`: wrapper formals = the
non-defaulted positions in original order, compacted) was never what
the wrapper built. Now the wrapper's inner send interleaves forwarded
formals and default expressions at their true positions, driven by
the exact `default_args` position set. **circle compiles CLEAN and
joins the compiled set** — its float×Circle "union" was never an
inference weakness at all, just this mis-binding. Test
`tests/kwarg_past_default.py` (both backends).

**str/list method batch** (each some example's blocker; split was
referenced by 12 bucket examples, strip by 7):
`str.strip/split(sep=None)/startswith/endswith/find/replace/count/
isdigit` (pure-Python, char-compare based — str still has no real
slice path, see `__pyc_substr__`), real `list.index/count`
(replacing `pass` stubs; index returns -1 when absent, no exception
model). `iter()`/`next()` builtins; all builtin iterator classes are
now self-iterable (`__iter__: return self` — Python's iterator
protocol; lets `for x in it:` consume an existing iterator);
`functools.reduce` rewritten pyc-style (default-None narrowing
instead of CPython's sentinel-class default, which is untypable
here).

**Sweep honesty correction:** resolving previously-missing methods
converted 4 more silent miscompiles into visible failures (pisang,
sudoku4, hq2x, rdb — like voronoi2 last round). Verified: pisang's
old "compiled" binary aborts at startup (`bad getter`), it was never
functional. Sweep counts: compiled 26 -> 22 by the numbers, but the
delta is fake-compiles becoming honest fails plus circle's genuine
join; "has no type" 29 -> 26.

Remaining bucket roots sampled: dijkstra2 needs dict-with-object-keys
+ heapq-of-tuples + empty-dict element inference (deep); pisang's
next layer is module-level `if __name__` globals feeding
comprehension chains; chess/rubik/sudoku3 unsampled.

### FA-precision round (2026-07-15, ifa/issues/043/044/045)

Chased the "empty-container inference" theory from the round-4
notes to ground; outcome recorded across three ifa issues:

- **[ifa/issues/043](../ifa/issues/043-empty-container-inference-options.md)**:
  the empty-container-element premise is FALSE — FA's union flow
  already populates empty siblings' element types; every candidate
  repro compiles clean today. The dijkstra2-family dict failure is a
  **union cross-product dead combination** (`op(A, B)` pairings that
  never co-occur at runtime), a distinct, still-open shape whose
  bucket share is unmeasured.
- **[ifa/issues/044](../ifa/issues/044-mixed-length-tuple-list-len-miscompile.md)**
  (NEW, silent wrong output): mixed-length list literals in one
  container print phantom elements (`[[3], [1, 2]]` →
  `[[3, 0], [1, 2, 0]]`) — len over a union of different-length
  tuple-list CSs. Open; ranked above further bucket work.
- **[ifa/issues/045](../ifa/issues/045-receiver-cs-method-cloning.md)**
  (LANDED): receiver-CS-directed method cloning as a precision move
  (opt-in `clone_methods_per_cs`, first user `range`). **Fixes
  ifa/issues/040** (`k=[]; print(k)` next to a non-empty list) and
  the branch-merged-receiver shape; also fixed a latent
  `build_call_dominators` SIGSEGV and made `__pyc_clone_constants__`
  on ctor params actually work through `__new__`/default wrappers.
  Corpus-neutral at the example level (the 040 shape is no example's
  FIRST blocker); suites 196/0 + 17/0.

Sweep state after the round: 22 compiled / 77, "has no type" 26 —
unchanged from round 4; the round's value is the two fixed FA bugs,
the new-issue filings, and the corrected taxonomy (cross-product
imprecision, not empty containers, is the live inference gap).

### The "has no type" bucket grouped by root cause (2026-07-15)

All 28 bucket members' full diagnostics collected and classified.
First-warning variable names are cascade VICTIMS (e.g. `'ll' has no
type` is `list.__eq__`'s local; `'lt'` is `tuple.__lt__`'s) -- the
groups below are by best-evidence ROOT, with confidence labels.
Micro-repros verified with 10-line standalone programs.

**R1 -- missing sequence operations (mechanical, pure-Python
fixable; each VERIFIED by micro-repro):**

- `n * [x]` / reflected multiplication: `int.__mul__` doesn't match
  a list/tuple right operand and `list.__rmul__`/`__imul__` are
  `pass` stubs; pyc has no reflected-operator fallback. Micro:
  `print(3 * [0])` fails to compile. Members: **rubik2, tictactoe**
  (first blocker, verified from source: `20*[0]`, `edge*[0]`), plus
  **hq2x, sudoku2** (grep-attributed, deeper in their chains).
- `list.extend` missing -- AND IT SILENTLY NO-OPS today (unknown
  method on a known class warns and drops: `a=[1]; a.extend([2,3]);
  print(a)` prints `[1]` -- the pisang/voronoi2 silent-miscompile
  shape again). Members: **softrender** (first blocker), **chull,
  hq2x, rdb** (grep-attributed).
- `reversed(range(n))`: `reversed` indexes its argument;
  `range.__getitem__` doesn't exist. Micro: runtime abort ("getter
  not resolved"). Member: **linalg** (first blocker).
- Tuple concatenation/repetition: `(1, 2) + (None,) * 3` fails
  (tuple.__add__/__mul__ missing -- tuples are fixed-arity
  structs, so these need CONSTANT-length folding at the frontend or
  FA level, not a runtime loop). Member: **chess** (first blocker:
  its `setup` board literal is exactly this shape).
- `copy.deepcopy` on containers: the shim aliases deepcopy to the
  shallow `copy` prim; genetic2 deep-copies a genome list. Member:
  **genetic2** (SUSPECT -- first-diag + shim inspection, no micro).

NOT a root (verified working): nested tuple unpacking
`a, (b, c) = f()` -- pygmy's first-line suspect compiles and runs
correctly in isolation.

**R2 -- heterogeneous unions / BOXING mixes (deep FA; the
ifa/issues/043 cross-product family):** explicit `has mixed basic
types` warnings name the mix. Members: **pygasus** (int64/str --
its `val`/`x` memory-bus values), **hq2x** (int64/str, second root
besides R1), **rubik** (list/tuple/int64/str/face in ONE union),
**pygmy** (tuple/int64/float64/str/vec). These need the
cross-product measurement + issue 030-style tagged dispatch or
better splitting; no quick fix.

**R3 -- dict-with-object-keys / heapq / cross-product (deep,
verified for dijkstra2 earlier):** **dijkstra2**, and likely
**sudoku2/sudoku4** (Norvig-style dict/set solvers; sudoku2 also in
R1 via `n*[x]`).

**R4 -- unattributed cascades (first diagnostic is a victim inside
builtin code or a call-tree tail; each needs its own dig):**
**amaze, block, chaos, mastermind2, neural1, othello2, pylife, rdb
(beyond extend), sha, sudoku1, timsort, tonyjpegdecoder, voronoi2,
yopyra.** Known non-blocking context: othello2's diagnostics sit in
possible_moves' int-bit tricks; timsort's in merge_at; voronoi2's
PriorityQueue was diagnosed pre-045 as genuine inference gaps.

**Suggested attack order by leverage:** R1's four verified ops
(mechanical, ~4 first-blockers plus 4 second-blockers, and the
extend silent no-op is a correctness hole regardless); then the R2
cross-product measurement (043's open item) before any deep FA
work; R4 one example at a time afterward.

### "has no type" tail dig (2026-07-08)

Root-caused and fixed three mechanical gaps plus one wrong-code
parse bug found under brainfuck:

1. **Comma multi-imports lost every module after the first**
   (`import random, math, sys, time` — used by 6 of the 20 tail
   examples). Two independent bugs: (a) both passes' collectors
   missed the PY_testlist wrapper the grammar puts around 2+
   dotted_as_names; (b) build_module_attributes_if1 /
   build_if1_module_pyda re-point ctx.node at the imported
   module's scope node without restoring, so the second module's
   top-level code was spliced into the first module's dead PycAST
   stream (ctx.node is the import statement whose ast->code the
   statement loop consumes). Both fixed.

2. **File objects** (the dominant tail cause, ~10/20): _CG_f*
   helpers in pyc_c_runtime.h (+ extern decls in pyc_runtime.c for
   the LLVM archive), `__pyc_file__` + `__file_iter__` + `open()`
   + `input()` in __pyc__/07_file.py, sys.stdin/stdout/stderr in
   pyc_lib/sys.py. FILE* handles smuggled through int64; failed
   open = handle 0 = EOF/ignore-writes (no exceptions, issue 011).
   `for line in f` works via one-line-lookahead iterator
   (__pyc_more__ protocol). tests/file_io.py golden vs CPython.
   Side effect: test_async_read now reaches the coroutine C
   codegen and exposes a co_await result-typing gap (void* vs
   _CG_string) — marked .check_fail until async lands.

3. **Star imports** (`from math import *`, yopyra): '*' produces
   no import_as_name child, so nothing was bound. Null sym now
   means star: bind all public (non-underscore) top-level names.
   Plus **str.__contains__** (substring by char compare; str has
   no working slice path yet).

4. **Dangling-elif GLR misparse (wrong code!)**: an outer-level
   `elif`/`else` after a DEDENT attached to the innermost `if`, so
   a nested if ending an elif arm silently swallowed the rest of
   the chain — later arms unreachable at runtime, no diagnostics,
   both backends identical (the flat if1 carried the misparse).
   Found because brainfuck's interpreter dispatch dropped its
   `<`/`.`/`]` arms. Fixed with speculative indent guards on
   if_stmt/while_stmt/for_stmt clauses (same column as the opening
   keyword). Regression: tests/dangling_elif.py (12 lines). Any
   corpus example with this shape was silently miscompiled before.

Corpus: compiled 4 -> 8 (block, neural1, oliva2, brainfuck join
ant/astar/mandelbrot/neural2); **brainfuck's 99-bottles output is
byte-identical to CPython** (5th fully-green). block aborts at
runtime (also needs print(..., file=sys.stderr) kwarg); neural1
truncates after its header (semantic gap TBD); oliva2 runs (slow;
timed out under the sweep cap but produces correct-looking PGM).

Remaining tail after these: sudoku2 hits the unique_AVar
emptied-ES assert (same family as pystone/tictactoe); loop /
softrender / go / pygmy / lz2 are genuine inference cases
(optional-None fields, `x or default` idiom); rdb needs os.path/
getopt/fnmatch; dijkstra2 needs heapq; tonyjpegdecoder crashes the
compiler with an FPE; hq2x/rubik/rubik2/sudoku4 still to diagnose.

### Compile-timeout bucket — root-caused (2026-07-09)

The five compile timeouts (fysphun, kmeanspp, pygasus, pylife,
stereo) were FA's splitting loop failing to reach a fixed point:
split decisions are not idempotent across passes, so ess/violation
counts oscillate while per-pass cost grows superlinearly. Full
analysis and the real-fix plan live in
[../ifa/issues/033-splitter-non-idempotent-divergence.md](../ifa/issues/033-splitter-non-idempotent-divergence.md);
the stall guard (IFA_STALL_LIMIT) mitigates it. After the guard:
stereo COMPILES; fysphun converges to 0 violations in <1s (the
guard un-starves the numeric-coercion reanalyze callback) and now
fails at a void-typed-field codegen bug; kmeanspp/pylife fast-fail
with inference diagnostics; pygasus surfaces a previously-masked
update_display assert
([../ifa/issues/034](../ifa/issues/034-pygasus-update-display-assert.md)).

Re-run `./shedskin_sweep.sh` after each change; the bucket counts
are the regression/progress signal. As examples start reaching C
(and running), promote the stable ones into `test_pyc.py` with
`.exec.check` goldens so they don't regress.

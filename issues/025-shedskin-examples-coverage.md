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

### R1 item 1 fixed: reflected int*sequence multiplication (2026-07-15)

`int.__mul__` now checks `isinstance(x, list)` and dispatches to
`x.__rmul__(self)`; `list.__rmul__` (previously a `pass` stub) now
returns `self.__mul__(n)` (multiplication is commutative for
sequence-repeat). Fixes `print(3 * [0])` and both verified
first-blocker shapes (rubik2's `20*[0]`, tictactoe's `edge*[0]`
comprehension) — output byte-identical to CPython on both backends.
rubik2 and tictactoe now compile past this point and hit their next
(unrelated) blockers, confirming this was genuinely their first
blocker per the earlier grep-attribution.

Landing this exposed a **pre-existing LLVM-backend codegen bug**,
unrelated to reflected operators specifically: `cg_emit_llvm.cc`'s
`sym_to_llvm_type` mapped FA's `sym_void_type` (the marker for
FA-unreachable/no-value results — the C backend's harmless
`_CG_void_type = void*` placeholder that's declared but never
touched) to a generic opaque `ptr`, instead of LLVM's real `void`
type. `discover_phi_targets`' union-find (which decides which Vars
share a mutable alloca slot, for loop-carried/branch-joined
variables) doesn't distinguish dead from live members when picking
a class's storage type, so a void-typed dead branch's Var could
claim a phi-class's slot as `ptr` before a live, genuinely-`int64`
member of the *same* class got a chance to — corrupting that live
var's storage type. Symptom: an LLVM verifier failure (`mul ptr,
i64`) on plain `i * 10` inside a `while` loop, with *no* isinstance
or list involved on the live path — any `isinstance(x, T)`-guarded
dead branch inside a loop-reached function could trigger it. Fixed
by mapping `sym_void_type` to LLVM's void type in `sym_to_llvm_type`
so `discover_phi_targets`' existing (previously dead) `t->isVoidTy()`
skip actually fires. Verified: both backends' `test_pyc.py` still
196/0 (7 expected fails unchanged), `make test-unit` 58/0.

### R1 item 2 fixed: list.extend (2026-07-15)

Added a real `extend` to `__pyc__/04_sequence.py`'s `list` class (a
per-element `append` loop; `append`'s `merge_in`-tagged resize
mutates the backing store in place, verified through a plain local
var, a field access, and a helper-function indirection -- no
explicit `self = self.extend(...)` rebind needed at any call site).
Previously `extend` was an unresolved method that silently
warned-and-dropped (`a=[1]; a.extend([2,3]); print(a)` printed `[1]`
-- a correctness hole independent of any example, not just a compile
blocker). Fixes the micro-repro and matches CPython on both
backends. softrender, chull, and rdb (the three grep-attributed
members) now compile past their `.extend()` call sites into new,
unrelated blockers: softrender hits a string/float64 cast error
later in the file; chull hits the R2 mixed-basic-types bucket
(`tuple/int64/float64/str/Vector/Vertex/Edge/Face` union); rdb hits
a `\x` string-escape parse bug plus `os.path`/`getopt` gaps already
noted above. Verified: both backends' `test_pyc.py` 196/0, `make
test-unit` 58/0.

### R1 item 3 fixed: range.__len__/__getitem__ (2026-07-15)

Added `__len__` (CPython's exact ceil-division formula, split by
step sign) and `__getitem__` (with negative-index support) to
`range` in `__pyc__/05_builtins.py`. `reversed()` is index-based
(`len(seq)` + `seq[i]` in a countdown loop) and `range` had neither
method, so `reversed(range(n))` aborted at runtime ("getter not
resolved"). Verified against CPython: positive/negative step,
negative indexing, and empty ranges all byte-identical on both
backends. linalg (the sole member) now compiles past
`reversed(range(1025))` into new, unrelated type-resolution errors
further down the file. Verified: both backends' `test_pyc.py`
196/0, `make test-unit` 58/0.

### R1 item 4 fixed: tuple literal concat/repeat via compile-time folding (2026-07-15)

Tuples are fixed-arity structs (no general runtime `__add__`/`__mul__`
is possible -- each length is a distinct struct type), so this
needed frontend-level constant folding rather than a `__pyc__`
stub, per the plan above. Added `try_fold_tuple_arity` in
`python_ifa_build_if1.cc`, wired into the `PY_binop` ADD/MUL case
before normal dispatch: recursively recognizes `tuple_literal +
tuple_literal` and `tuple_literal * int_literal` (any nesting depth,
either operand order for `*`) and flattens the whole expression into
a single tuple-literal element list at compile time, falling through
to the normal (still-missing) runtime path unchanged for anything
that doesn't match this shape (a non-literal repeat count, tuple +
non-tuple, etc). Elements are evaluated exactly once per Python
semantics (`expr * n` evaluates `expr` once and repeats references
to its already-computed elements, never re-executes them) --
`try_fold_tuple_arity` may return the same source PyDAST* pointer at
multiple output positions, so the call site dedupes before calling
`build_if1_pyda`, then re-references each element's already-built
`rval` for every position it appears at, including repeats.

Verified: `(1, 2) + (None,) * 3` and `(10, 20, 30) * 2` both
byte-identical to CPython on both backends (length, per-element
identity/value, and str() of the homogeneous case all checked);
regression suite 196/0 both backends, `make test-unit` 58/0. chess
(the sole member) now compiles cleanly past line 24's `setup =
(4,2,...) + (iTrue,)*4 + ... + (iNone,)*40` (no diagnostics at that
line at all) into new, unrelated blockers at lines 26+: `tuple(range
(...))` and `tuple([comprehension])` -- the dynamic-length `tuple()`
**constructor** call, a genuinely different and harder problem (needs
an actual variable-length tuple or a list-backed fallback) than the
literal concat/repeat this item covers. Noting for a future item;
out of scope here.

One known gap surfaced, NOT a regression (reproduces identically on
`main` with a plain literal, no `+`/`*` involved):
`print()`/`str()` of a tuple with genuinely mixed element types
(`(1, None)`) aborts at runtime ("matching function not found") --
`tuple.__str__` indexes elements through a generic runtime loop that
can't dispatch per-position on a heterogeneous struct. Homogeneous
tuples (all-int, etc.) print fine, matching CPython, on both
backends -- only affects printing a truly mixed-type tuple.

### R1 item 5 investigated, NOT fixed: copy.deepcopy (2026-07-15)

**SUPERSEDED by the next section (2026-07-16):** point 2 below
("not implementable") was wrong -- it described an FA BUG, not a
design limit. Handling recursion is core IFA design; the recursive-
ES splitting fix below makes deepcopy compile and run correctly.
Point 1 (genetic2's separate optional-None crash) stands -- filed
as [ifa/issues/046](../ifa/issues/046-optional-none-field-inline-type-sum-assert.md).

This item was filed as SUSPECT (first-diag + shim inspection, no
micro) and turns out to be wrong on both counts once verified:

1. **genetic2's real first blocker is unrelated to deepcopy.**
   `class TreeNode: def __init__(self, ..., args=None)` -- an
   optional-list-of-same-class-instances field defaulting to `None`
   -- crashes the compiler on its own, with no `copy`/`deepcopy`
   involved at all: `pyc: optimize/inline.cc:407: ... Assertion
   \`v->type->type_kind != Type_SUM' failed`. Minimal repro (any
   class with an Optional[list-of-self-type] field, `Node(1,
   [Node(2, None), Node(3, None)])`) reproduces it standalone. This
   is the SAME "optional-None fields" gap the tail-dig section above
   already attributes to loop/softrender/pygmy/lz2 -- genetic2
   belongs in that bucket, not R1, and hits it before ever reaching
   its `copy.deepcopy(self.genome)` call.

2. **A general recursive Python-level deepcopy is not implementable
   in the current compiler.** `deepcopy`'s shallow `copy` alias is a
   real semantic gap in principle (mutating a "deep" copy's nested
   containers should not corrupt the source -- `_CG_prim_copy_dst`
   is a single-level struct memcpy, pointer fields stay shared), but
   attempting a fix -- an `isinstance`-dispatched recursive Python
   function (list/dict/set branches recursing into `deepcopy`,
   falling back to shallow `copy` for scalars/tuples/objects) --
   breaks compilation even in the narrowest form tried (list-only
   recursion, no dict/set): the base-case `copy(obj)` call's `obj`
   comes out typed `_CG_any` (opaque `void*`), because FA merges
   `obj`'s type across every depth of the SAME recursive call rather
   than specializing per depth/type the way per-call-site dispatch
   works for a plain (non-recursive) method -- unlike `int.__mul__`
   (R1 item 1), which has exactly two call shapes and got a working
   isinstance branch, `deepcopy` recurses back into itself with
   *different* argument types at each level and the shared function
   contour can't carry that. This needs either `clone_methods_per_cs`
   -style per-contour cloning extended to plain recursive functions,
   or a tagged-dispatch mechanism (ifa/issues 030's territory) --
   real FA work, not a pure-Python fix. Reverted the attempted change
   (`pyc_lib/copy.py` back to `deepcopy = copy`); regression suite
   unaffected (still 196/0 both backends).

No corpus member is unblocked by (or blocked on) this item. Closing
it out of R1; if revisited, scope it as an FA/cloning item next to
043/045, not a `__pyc__`/shim edit.

### R1 item 5 RESOLVED: recursive-ES splitting; deepcopy works (2026-07-16)

The previous section's "recursion can't specialize per depth" was an
FA bug, not a design limit -- resolving recursion to monomorphic
contours is core IFA design (split + find-ES bind the recursive call
in the next pass to the same ES as its top-level caller's contour).
Three fixes, all in ifa:

1. **`decide_entry_set_split` excluded every recursive edge from
   type grouping** (`is_es_recursive(ee) -> continue`), then
   short-circuited on `non_rec_edges == 1` -- so a self-recursive
   function with one caller could NEVER split: its formal held the
   union of all recursion depths' types forever (deepcopy's `obj`
   boxed to `void*`). Now recursive edges join type-driven grouping
   when the recursion is LEVEL-DESCENDING, gated on separability:
   the recursive edge's type at the confluence position must be
   IDENTICAL TO or DISJOINT FROM every other edge's. Partial overlap
   (same-shape recursion over one union -- tests/expr_evaluator.py's
   kind-discriminated Expr tree, lhs/rhs actuals {Expr, None} vs a
   caller's {Expr}) keeps the recursion fused as before: splitting
   those re-derives forever, strands runtime-dead union members
   (None) in contours where nothing resolves, and fans single call
   sites across same-class contours runtime dispatch can't
   discriminate ("polymorphic dispatch: no branch matched" -- both
   failure modes observed under weaker gates). The setter path keeps
   the blanket exclusion (recursive DATA isn't level-separable).
   The single-real-caller short-circuit is now setter-path-only.

2. **`check_split`'s pending-backedge binding could veto the
   split.** record_backedges plants "recursion binds to the split
   product" entries; when the splitter later decides (on type
   evidence) to detach a recursive edge from that very product,
   the pending route re-bound it straight back -- the split silently
   no-oped and re-derived every pass (2-level `f([[1,2],[3,4]])`
   stalled with the level-1 contour permanently {list, int64}).
   make_entry_set now passes the split-source ES into check_split as
   `avoid`; routes back into it are skipped (the monomorphic-
   recursion binding is a default, not evidence).

3. **`P_prim_copy` on scalars** (both backends): the clone macro's
   `sizeof(*(T)0)` only compiles for records; a scalar/string copy
   is identity. Newly reachable because deepcopy's int64 leaf
   contour is now monomorphic instead of boxed.

Result: level-descending recursion gets one monomorphic contour per
level (verified via ES dump: `deepcopy` over `[[1,2],[3,4]]` yields
exactly {outer-list} -> {inner-lists} -> {int64}, each recursive
edge bound to its own ES); monomorphic recursion (fib) binds back to
its caller's ES; mutual recursion works; statically-unbounded
polymorphic recursion (`g(n-1, [x])`) still compiles in <1s
(depth-independent -- stall guard bounds it) and degrades honestly
at runtime. `pyc_lib/copy.py`'s deepcopy is now a real recursive
list deep-copy, byte-identical to CPython on both backends,
including mutation-isolation (`b[0][0] = 99` leaves the source
untouched). One constraint: deepcopy iterates by INDEX, not
`for x in obj` -- iteration shares one `__list_iter__` CS across
recursion levels (its `thelist` unions every level's lists), which
re-fuses the freshly-separated contours; that's the CS-contour
cross-product gap (ifa/issues/043), noted there.

New regression tests: tests/recursive_polymorphic.py (descent +
fib + mutual), tests/deepcopy_list.py (deep + shallow, mutation
isolation). Verified: both suites 198/0 (C + LLVM), `make
test-unit` 58/0, `make test-ir` clean, corpus sweep unchanged at
22/77 compiled with pygasus's pre-existing 65s-vs-60s-cap timeout
(baseline-identical compile time, measured), fysphun/kmeanspp/
pylife/stereo all ~1s. genetic2 now compiles PAST its old
`inline.cc:407` crash into later, separate blockers; the
optional-None micro still reproduces standalone --
[ifa/issues/046](../ifa/issues/046-optional-none-field-inline-type-sum-assert.md).

### 043 shape C resolved: iterator-CS re-fusion; `for x in obj` recursion works (2026-07-16)

Follow-on to the R1-item-5 resolution above: the "index loop, not
`for x in obj`" constraint is GONE. Two changes (full mechanics in
[ifa/issues/043](../ifa/issues/043-empty-container-inference-options.md),
shape C):

1. `__list_iter__` joined the ifa/issues/045 `clone_methods_per_cs`
   track (one-line `__pyc_clone_constants__` marking in
   `__pyc__/04_sequence.py`) -- one iterator CS per creating
   contour, `__pyc_more__`/`__next__` per receiver CS, so each
   recursion level's loop carries only that level's element types.

2. The FA stall guard is now dup-aware (`IFA_STALL_LIMIT` counts
   only re-deriving passes; new `IFA_NONIMPROVE_LIMIT 32` bounds
   dup-free descent): the iterator method chain legitimately needs
   ~14 one-split-per-pass passes, and the old unconditional 8-pass
   counter killed splitting mid-chain whenever a second recursive
   function shared the file (found via tests/recursive_polymorphic
   .py's combined form failing while each half passed alone).

`pyc_lib/copy.py` deepcopy reverted to the natural `for x in obj:`
loop; tests/recursive_polymorphic.py extended with the iterator-
descent + second-recursive-function combination. Verified: both
suites 198/0, unit 58/0, compile times flat (pygasus 64s), and the
corpus IMPROVED 22 -> 24 compiled: **kanoodle** (iterator-CS
precision) and **oliva2** (its float64 violation now actually
resolves with the extra dup-free passes -- compiles in 0.7s; it was
never a timeout case).

### genetic2 dig: six fixes; now runs to a deepcopy-rooted crash (2026-07-16)

Peeled genetic2's blockers one at a time (each verified by a
standalone micro + both suites staying 198/0 after every step):

1. **`min`/`max` `key=` parameter** (`__pyc__/05_builtins.py`):
   `max(self.population, key=fitness)` had no formal to bind `key`
   to; the unmatched call's untyped result cascaded into the
   cg.cc:306 getter assert this dig started from. Implemented with
   the list.sort nil-narrowing pattern (key-is-None branches keep
   contours monomorphic); two-arg + key forms too.
2. **`random.randrange(stop)` one-arg form and `random.triangular`**
   (`pyc_lib/random.py`): both used by genetic2; randrange(N) simply
   didn't match, triangular didn't exist (CPython formula, explicit
   float() coercions per that file's LLVM note).
3. **Class scope does not nest into methods** (`find_PycSymbol`,
   `python_ifa_sym.cc`): `import copy` + a METHOD named `copy` made
   `copy.deepcopy(...)` inside `Individual.copy` resolve `copy` to
   the method (Python semantics: class bodies are not enclosing
   scopes for their functions -- bare names in methods resolve at
   module level). 13-line repro; one bare intra-class reference in
   `__pyc__` (`list.__iadd__`'s `__add__(self, l)`) updated to
   `self.__add__(l)`. Scoping suite goldens unchanged.
4. **Dynamic `tuple(iterable)` + `list(tuple)`**
   (python_ifa_build_if1.cc intercept + `tuple.__pyc_tolist__`, an
   INDEX loop -- see ifa/issues/047 for why not iteration): returns
   a LIST (fixed-arity tuples can't be dynamic; same compromise as
   zip/map/reversed). Unblocks genetic2's
   `node.args = tuple([TreeNode() ...])` and chess's board lines.
   `list + tuple` concat added to `list.__add__` (inline append
   loops -- a self-recursive conversion helper cross-contaminated
   unrelated sites' element types).
5. **Boolean-context `and`/`or` lowering**
   (python_ifa_build_if1.cc): when the chain feeds an
   if/while/elif/`not` condition, the result var is the per-operand
   `__pyc_to_bool__` BOOL, not the operand value -- the value form
   made `if node.args and <test>:` a {nil, list, bool} union that
   reached clone.cc's "mismatched field sizes" through a
   partial-application closure (and was needlessly polymorphic
   everywhere). Python's operand-value semantics are unobservable
   in boolean context.
6. **C-backend dispatch + cast gaps** (cg.cc): (a) multi-candidate
   sends with one NIL branch + exactly ONE untagged-receiver method
   candidate (list/str receivers carry no classtag) now emit
   `if (!recv) nil_branch else direct_call` instead of the flat
   "matching function not found" trap -- with a scalar-receiver
   guard (0 vs NULL conflation) that EXEMPTS the truthiness
   selectors (`__pyc_to_bool__`/`__bool__`/`__not__`), where None
   and zero coincide (`if f():` on an implicit-None-returning int
   function is genetic2's TreeNode.execute). (b) single-target
   calls and all dispatch arms now CAST results when the callee
   contour returns a wider C type than the call site's lval.
7. **Constant-format `%` pre-conversion**
   (python_ifa_build_if1.cc): `fmt % args` with a constant format
   now stringifies each %s argument through a real `__str__` send
   before the `__pyc_format_string__` prim -- raw C varargs
   strlen'd scalars (`"Epoch: %s" % epoch` segfaulted) and objects
   could never print. `%d`/`%f` args stay raw. Byte-identical to
   CPython on scalars, strings, and objects with `__str__`.

Also probed and REJECTED (each regressed the suites and was backed
out): `__pyc_None_type__.__getitem__` (injected None into element
unions program-wide; printing tuple element 0 became "None"),
`__pyc_None_type__.__len__` (turned every iterator prototype's
None-default field into live multi-candidate dispatches; the LLVM
backend's dispatch emitter lacks the C backend's new routes and
19 EXEC tests silently broke), and `clone_methods_per_cs` on
`__tuple_iter__` (didn't produce per-contour CSs; the real
different-arity tuple iteration bug is ifa/issues/047).
`__pyc_None_type__.__pyc_getslice__` returning `[]` KEPT (value-
safe; unblocks `args[:k]` slicing on optional fields).

**genetic2 now compiles on the C backend and runs deep into its GP
simulation**; the remaining crash is `copy.deepcopy` being shallow
for USER OBJECTS (TreeNode trees end up sharing subtrees across
individuals until crossover creates a cycle and execute's recursion
overflows) -- filed as
[issues/029](029-deepcopy-user-objects.md) with fix directions
(compiler-synthesized per-class `__deepcopy__`).

### genetic2 dig, round 2: %-format fix, tuple.__add__, two more compiler fixes (2026-07-16)

Continuing the dig after the six fixes above:

8. **Constant-format `%` pre-conversion refined** (see item 7 above;
   the final form generates the literal arg tuple's ELEMENTS
   directly, stringifies %s members, then makes a fresh tuple -- the
   naive "build a second tuple" left the original heterogeneous
   make-tuple dead-but-diagnosed, and mutating it in place ran the
   conversions before the elements were computed).
9. **Dynamic `tuple + tuple` of NAMED values** returns a list
   (`tuple.__add__`, `__pyc__/04_sequence.py`): the compile-time
   literal fold only covers literal operands; chess's
   `queenLines = bishopLines + rookLines` became live once the
   tuple() intercept typed its operands.
10. **`cg_build_new_to_val_map` null-hole crash**
    (`ifa/codegen/codegen_common.cc`): iterated `Fun::ess` (a
    hash-set Vec with null holes) without the `if (es)` guard every
    sibling loop has -- latent; surfaced on chess once the
    dup-aware stall guard's extra passes left holes. Compiler
    SIGSEGV -> fixed.

Filed [ifa/issues/047](../ifa/issues/047-different-arity-tuple-iteration-shared-cs.md):
iterating two DIFFERENT-ARITY tuples in one program segfaults
(shared `__tuple_iter__` CS: void `thetuple`, per-arity folded
lengths, prototype method-pointer slots cross-wired) -- pre-existing
at user level (4-line repro on main), sidestepped in
`tuple.__pyc_tolist__` via an index loop; the `clone_methods_per_cs`
lever does NOT work for the prototype-instantiation path.

New regression test: tests/genetic2_idioms.py (max/min key=,
method-named-like-module, dynamic tuple()/list()/+, Optional-field
guard + slice + graft, %-formatting with scalars/objects, named
tuple concat). Suites 199/0 both backends, unit 58/0.

**End state: genetic2 COMPILES and runs deep into its GP simulation**
(epochs of evolution, correct dispatch through every layer this dig
fixed); the remaining crash is `copy.deepcopy` being SHALLOW for
user objects -- crossover on shared subtrees eventually creates a
cyclic "tree" and TreeNode.execute's recursion overflows. Filed as
[issues/029](029-deepcopy-user-objects.md) (fix direction:
compiler-synthesized per-class `__deepcopy__`; layouts are fully
known at codegen). chaos also newly compiles (25/77 at the sweep
before chess's frontier wobble); chess bounced back to FAIL with an
honest sizeof_element diagnostic -- it sits on the mixed-types
frontier (R2), where its sweep3 "compile" was never a verified run.

### issues/029 implemented: synthesized per-class __deepcopy__ (+5 latent bugs) (2026-07-17)

The deepcopy fix sketch landed: every record class without its own
`__deepcopy__` gets a compiler-synthesized recursive one (shallow
clone + per-field dispatch; fields collected syntactically from
`self.NAME = ...` stores in first-store order). Full mechanics and
the five latent compiler bugs it surfaced and fixed -- ifa/issues/
046 (inline Type_SUM assert), ifa/issues/044 (listish-tuple length
off-by-one / phantom elements -- also fixes that issue's standalone
print repro), an uninitialized nil-typed `return` in cg's
simple_move, the recursion pending-map edge fan-out, and
determine_layouts' order-dependent offsets -- are in
[issues/029](029-deepcopy-user-objects.md). New regression test:
tests/deepcopy_objects.py (deterministic, both backends). Suites
200/0 x2.

Corpus effect: genetic2's deepcopy SEMANTICS are now correct (the
runtime cyclic-tree crash is structurally impossible), but its
compile currently diverges in FA flow over the copy-chain unions --
filed with analysis and fix directions as
[ifa/issues/048](../ifa/issues/048-deepcopy-flow-divergence-genetic2.md);
genetic2 drops out of the compiled column until 048 lands (24/77;
its previous "compiled" state ran on miscompiled shallow copies).

### `set(iterable)` fixed; "undeclared label" crash-class investigated (2026-07-18)

Went looking for the highest-leverage remaining bucket in the
"compilation failure" column (generated C fails to compile, not a
pyc-side rejection). 11 examples (`dijkstra`, `chull`, `pisang`,
`sha`, `rdb`, `softrender`, `sieve`, `amaze`, `rubik2`, `yopyra`,
`voronoi2`) shared a "use of undeclared label 'LNNNN'" C compile
error -- a `goto` to a label whose own emission got skipped.

Traced the actual chain, since the label mismatch turned out to be a
*symptom*, not the root cause: `runtime_errors` defaults to `true`
(`defs.h:23`), so `fruntime_errors` is always on unless explicitly
disabled -- meaning `ifa_analyze()` never hard-fails on unresolved
("NOTYPE") violations; `convert_NOTYPE_to_void()` silently salvages
them to `void_type` and lets the compile proceed. All three sampled
examples had real, nonzero violations at convergence (`dijkstra`:
20, `chull`: 10, `sha`: 12, `amaze`: 404) -- codegen's `write_c_pnode`
isn't robust to the resulting void-typed CFG islands, and the
`extra_goto` mechanism (`cg.cc`) can emit a `goto` to a label region
that never gets printed.

**One concrete, well-scoped violation found and fixed**: `rubik2.py`
(the smallest of the 11, 100 lines) hit `set([current_id])` at line
81. `set` (`__pyc__/08_set.py`) only ever had a zero-arg
`__init__(self)` -- `set(iterable)` fell through to the generic
constructor path (which the zero-arg form already handles fine, so
this had never surfaced as an issues/022-style zero-arg gap) and
silently dropped its argument, same shape as the `str(x)`/
`list(iterable)` 1-arg intercepts already in
`python_ifa_build_if1.cc` before this fix -- `set` just never got
the same treatment. Isolated with a 17-line repro (`set([1,2,3])`
alone, no tuples needed, already failed) before touching the real
example. Fixed by adding `set.update(other)` and a module-level
`__pyc_set_from_iterable__(other)` helper (`__pyc__/08_set.py`),
dispatched from a new 1-arg `set(iterable)` intercept mirroring the
existing `str`/`list`/`tuple` ones. New test
`tests/set_from_iterable.py` (list/tuple/range/empty sources,
`update()`, both backends). Corpus effect verified via full
77-example sweep, before/after diff: **only** `rubik2` changes,
`rc=1` -> `rc=0` (compiles) -- 26/77. No other example in the
11-cluster shares `set(iterable)` as ITS trigger; each has some
other, not-yet-diagnosed violation feeding the same
salvage-then-invalid-C failure mode.

**Not fully resolved**: `rubik2.py` now compiles but hangs at
runtime -- it still has separate, unrelated violations
(`result`/`newstate`, `__imod__` on augmented list-element
assignment around `apply_move`'s cube-turn logic, lines 48/66) not
yet diagnosed. The other 10 "undeclared label" examples are
untouched by this fix and need individual root-causing the same way
`rubik2`'s `set()` gap was found -- there is no single shared trigger
across the cluster, only a shared *symptom* (the void-salvage +
`write_c_pnode` robustness gap described above, which a defensive
codegen fix could address structurally without resolving any
individual program's actual type violations).

### `simple_move` null-type codegen bug fixed (`-r` mode); genuine root cause, not a violation (2026-07-18)

Per the previous entry's "defensive codegen fix" thread: went
looking directly at the `write_c_pnode` robustness gap rather than
another individual program's violation. Built a minimal,
**violation-free** repro under `-r` (`runtime_errors=false`, which
skips `convert_NOTYPE_to_void`'s salvage and surfaces the raw FA
state): `x=0; for e in range(3): x=x+1; print(x)`. This alone
produced invalid C (`t0 = (_CG_bool)t2;` reading an
un-assigned/undeclared-type temp, right after a `return`) -- proof
the bug lives in codegen itself, independent of any program's type
violations.

Root cause, found by instrumenting `do_phy_nodes`/`do_phi_nodes`
(`ifa/codegen/cg.cc`) and tracing exact Var ids/types through a
fresh build each time (**gdb was unusable in this sandbox --
silently swallowed output across several invocations; printf-bisection
worked reliably and should be the default for future crashes here**):
`range::__pyc_more__`'s `self.s >= 0` branch is proved always-true
for the `__pyc_clone_constants__`'d contour (`range(3)`'s 1-arg
`__init__` leaves `self.s` at its class-default `1`), so the `else`
arm's value never gets a type -- its Var's `->type` field is a bare
`nullptr`, not `sym_void_type`. `simple_move`'s existing void-guard
(`cg.cc`) only checked `rhs->type == sym_void->type`, which a null
`->type` doesn't satisfy, even though `c_type()` (`codegen_common.cc`)
already treats `!v->type` as void for declaring/printing the same
Var. The mismatch let a MOVE with a genuinely-untyped, never-defined
source slip through as valid-looking C.

First fix attempt was wrong and is worth recording: gating
`do_phy_nodes`/`do_phi_nodes` on the phy/phi PNode's own
`->live`/`->fa_live` fixed the compile error but broke *correct*
moves too (traced via added Var-id/type fields on the debug prints)
-- `mark_live_code` (`ifa/optimize/dead.cc`) never touches phy/phi
PNodes at all, so those flags are always `0` regardless of whether
the specific move is needed, and the blanket skip silently dropped
the legitimate `self`-passthrough and return-value moves too
(reproduced as a segfault from reading `t9` uninitialized). Reverted
that approach entirely.

**Actual fix**: extended `simple_move`'s existing void-guard to also
treat a null `->type` as void-equivalent on either side of the move
(`ifa/codegen/cg.cc`) -- one line, no new control flow. Verified:
minimal repro now compiles and runs correctly (prints `3`); full
`test_pyc.py` (both C and `-b` LLVM backends, 204/204) and `ifa`'s
`make test` (all phases) still 100% clean; a before/after diff of a
full corpus sweep run under **default** settings (`runtime_errors=true`)
is byte-identical (this bug only manifests when the salvage is
disabled, so no effect on normal operation); a before/after diff of
the corpus run **under `-r`** shows 7 examples strictly improve and
zero regress -- `circle`, `mandelbrot`, `nbody` now compile and run
correctly; `genetic`, `neural2`, `voronoi` now compile and run but
hang (separate, undiagnosed issues); `ant` progresses from a compile
failure to a runtime crash (also separate). `rubik2` itself still
does not compile clean under `-r` -- it has its own, unrelated
violations (`__imod__`/`__ior__` on augmented list-element
assignment, lines 48/66, matching the previous entry's note) that
this fix doesn't touch.

### rubik2's `__ior__`/`__imod__` and the crash that followed it, both fixed (2026-07-18)

Continuation of the previous entry's open thread: rubik2's
remaining `__ior__`/`__imod__` "unresolved call" warnings turned out
to be two separate, real bugs, both now fixed.

**Bug 1 (silent wrong output, not just a warning)**: augmented
assignment to a subscript target (`result[0] |= x`, `newstate[i] %=
n`) built `__setitem__(index, value)` directly with the *raw* RHS
value, discarding the in-place operator entirely --
`python_ifa_build_if1.cc`'s `PY_power` STORE-mode subscript branch
called `call_method(..., sym___setitem__, ..., 1, sub_ast->rval)`
eagerly, so `PY_augassign`'s `is_object_index` branch had nothing to
read the *current* element from -- it appended the raw value to that
already-built setitem call (`a[i] |= x` behaved exactly like `a[i] =
x`) and separately computed an `__ior__` result that was stored
nowhere. Repro: `result = [0,0,0]; result[0] |= 5` prints `5`
either way (0|5==5, coincidence) but `result[0] |= e` inside a loop
over non-constant `e` gave `2` instead of the correct `3`
(`0|0|1|2`) -- confirmed against CPython.

Fixed by making the STORE-mode subscript branch *defer* instead of
eagerly building `__setitem__` -- mirrors how the adjacent
`is_member` branch already defers (stores object+name, lets the
assignment/augassign caller build getter/setter on demand). Added
`PycAST::is_slice` (`python_ifa.h`) to keep the (separately, still
eager -- see below) slice-target path distinguishable from the new
deferred plain-index path, since both previously shared the single
`is_object_index` flag and eager-`__setitem__`-then-append-arg
shape. `PY_augassign`'s `is_object_index` branch now does
`__getitem__` → apply op → `__setitem__`, the same read-compute-write
shape as `is_member`. `PY_assign`/`PY_namedexpr_test`/`PY_annassign`
updated to build the full `__setitem__` call themselves for the
deferred (non-slice) case. New test `tests/augassign_subscript.py`
(list `+=`/`-=`/`*=`/`%=` incl. loop-carried, `.attr[i] |=`, dict
`d[k] +=`) -- verified WRONG on the pre-fix binary, correct after,
clean on both backends.

Slice-target augmented assignment (`a[i:j] |= x`) still has the same
eager-write shape and is *not* fixed here -- no failing corpus
example exercises it, and `__pyc_setslice__` takes a whole
replacement sequence rather than a single value, so it needs its own
translation, not just the same read-compute-write swap. Left as a
documented gap (comment at the `is_slice` branch in
`python_ifa_build_if1.cc`).

**Bug 2 (the crash that appeared once Bug 1 stopped masking it)**:
fixing Bug 1 made rubik2 compile clean (`-r` no longer reports *any*
violation) but the binary then segfaulted a few BFS levels into the
solve. Root cause: `list` is a single generic class program-wide, so
its element type is the union of *every* list's element type in the
whole program (`P_prim_sizeof_element`, `ifa/codegen/cg.cc`).
rubik2 mixes a list-of-lists (`affected_cubies`/`phase_moves`
literals -- element type `list` itself, `Type_PRIMITIVE`) with a
list-of-class-instances (`states`/`next_states` -- element type
`cube_state`, `Type_RECORD`). The existing `sizeof_element`
mitigation (from issues/025's original tuple-list soundness fix,
see above) only recognized a union where *every* member is
`Type_RECORD` as safely pointer-sized; `list`'s `Type_PRIMITIVE`
membership didn't qualify, so `sizeof_element` fell through to `0`
for this specific union -- `list.append`'s resize call never grew
storage, and reads returned corrupted/aliased objects (confirmed via
a debug trace: `state_ids._items[0]`'s reported `len()` went 12 → 0
→ 32292 garbage across three `.add()` calls, then segfault).
Generalized the check in both backends (`ifa/codegen/cg.cc`'s
`P_prim_sizeof_element` case and `ifa/codegen/cg_emit_llvm.cc`'s
`emit_send_sizeof`) from "every member is `Type_RECORD`" to "every
member has the same concrete size" -- the real invariant a uniform
element slot needs; boxed records and other boxed containers (list,
str, set, dict, ...) all happen to be one `pointer_size`, and
same-width scalar unions (two `int64` contours, say) are covered by
the same check for free.

New test `tests/list_element_type_union.py` (a trimmed, deterministic
slice of rubik2's actual `cube_state`/`apply_move`/BFS shape) --
confirmed it segfaults on the pre-fix binary and runs correctly
(matches CPython's counts for 3 BFS levels) after, on the C backend.
**Compile-only in the suite (no `.exec.check`)**: the LLVM backend
(`-b`) hits a separate, deeper, still-open bug on this same union
shape -- crashes during the very first `apply_move`, before this
fix's `sizeof_element` path is meaningfully exercised, so an
`.exec.check` here would make `PYC_FLAGS=-b ./test_pyc.py` red.
Filed as [ifa/issues/051](../ifa/issues/051-llvm-nested-list-index-mixed-union-crash.md).

**Net effect**: rubik2 now compiles with zero violations (`-r`) and
zero warnings (default), and runs without crashing -- it correctly
solves phase 0 (a long but valid move sequence, matching goal state)
and proceeds into phase 1 before hitting a multi-minute wall-clock
budget. That remaining slowness looks like a *third*, separate issue
worth flagging for whoever picks this up next: the phase-0
"solution" found is ~7675 moves long (cycling through all 18
phase-0 moves repeatedly), wildly longer than Thistlethwaite phase 0
should ever need -- consistent with `state_ids`' `not in` dedup
check not actually pruning already-seen states as effectively as it
should (on top of `set`'s inherent O(n) linear-scan cost, which
alone would explain slow-but-eventually-correct, not this specific
degenerate-looking move pattern). Full regression verified clean
throughout this entry's work: `test_pyc.py` and `PYC_FLAGS=-b
test_pyc.py` both 206/206 (204 prior + the 2 new tests here), `ifa`'s
`make test` all phases clean, full corpus sweep before/after shows
zero regressions and 4 examples make forward progress (`ant`,
`loop`: crash → different/no crash; `mastermind2`, `score4`: no
longer fail to compile).

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

**Remaining shim work** (each its own increment):
- `random` — needs a PRNG; a pure-Python LCG gives deterministic,
  self-consistent (not CPython-matching) output. 13 examples.
- `sys` — `sys.argv` (list), `sys.exit`, `sys.setrecursionlimit`
  (no-op), `sys.maxsize` (const) are easy; `sys.stdout/stderr/stdin`
  are file objects and are the hard part. 6 examples.
- `copy` — `copy.copy` shallow is feasible; `copy.deepcopy` is
  hard under static typing. 5 examples.
- long tail: `struct`, `itertools`, `functools`, `colorsys`,
  `array`, `re`, `getopt`.

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
4. **module import + module objects** — the real unlock for C (and
   bucket A's `import sys`): `import X; X.attr` and
   `from X import Y` for user/library modules. Prerequisite for any
   stdlib shim.
5. **D** — grammar fixes, one construct at a time.
6. **E** — the lone deep crash.

Re-run `./shedskin_sweep.sh` after each change; the bucket counts
are the regression/progress signal. As examples start reaching C
(and running), promote the stable ones into `test_pyc.py` with
`.exec.check` goldens so they don't regress.

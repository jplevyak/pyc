# Document Index

## Project-wide

- [PIPELINE.md](PIPELINE.md) — Top-level compilation flow: phase-by-phase map from `pyc <file.py>` through parse → IF1 → flow analysis → clone → optimise → codegen. The "where do I start?" doc.
- [PYTHON_FRONTEND.md](PYTHON_FRONTEND.md) — pyc Python frontend: `pyc.cc`, `python_parse.cc` + `python.g`, `python_ifa_*` two-pass lowering (`build_syms` → `build_if1`), scope sentinels, builtin module loading, language extensions, gotchas.
- [RUNTIME.md](RUNTIME.md) — pyc runtime layer: `pyc_c_runtime.h` (`_CG_*` types/macros, string layout, allocation, GC), `__pyc__/*.py` (Python builtin module), `pyc_compat.py` (CPython shim), recipes for adding new runtime support.
- [DOCUMENTATION_PLAN.md](DOCUMENTATION_PLAN.md) — Plan for filling out the rest of the documentation set, with checkboxes.

## IFA library

See [ifa/INDEX.md](ifa/INDEX.md) for the full per-subsystem index
(ARCHITECTURE, IR, IFA, CLONE, DISPATCH, PRIMITIVES, CFG_SSU,
OPTIMIZE, CODEGEN_C, CODEGEN_LLVM, CAST, FRONTEND, COMMON).

## Issue tracking

Deferred work worth a trail lives in two parallel directories:

- [issues/](issues/) — pyc frontend / project-wide concerns
  (Python lowering, grammar, builtin module, runtime, harness).
- [ifa/issues/](ifa/issues/) — ifa library concerns (FA,
  codegen, ifa-level IR).

Both use numbered markdown files (`NNN-short-slug.md`); each
documents symptom, root cause, proposed fix, verification plan,
and what fixing it would unblock. See
[ifa/issues/README.md](ifa/issues/README.md) for conventions and
when to file vs. fix-now.

## Change acceptance — run what CI runs, before committing

CI (`.github/workflows/ci.yml`) gates every push to `main`. A change
is not done until these five pass locally, in this order. They take
roughly four minutes together.

```sh
make                          # 1. builds pyc + ifa (CI sets USE_LLVM=1)
make test                     # 2. ifa --test, then test-ir, then test-e2e
make -C ifa test_llvm         # 3. V-language LLVM backend smoke
PYC_FLAGS=-b ./test_pyc.py    # 4. LLVM-backend pyc e2e
make test_dparse              # 5. grammar validation
```

**`make test` is the one that gets skipped, and it is the one that
matters.** It chains three gates and `set -e`s out of the first
failure, so a red `test-ir` means `test-e2e` NEVER RAN and its summary
is absent rather than failing — easy to read as "fine". Two habits
follow:

- Running `./test_pyc.py` alone is NOT the gate. It is only step 2's
  last third. `make test` is what CI runs.
- `test-ir` covers **16 phases**, and `./ifa-test --phase <name>` prints
  a per-phase summary. Reading the tail of `make test-ir` shows you the
  LAST phase only. Check every phase's `failed:` line, or just trust
  `make test`'s exit code, which is the point of running it.

Expected state when green: `ifa --test` 58/0; `test-ir` 0 failed with
2 known (below); `test_pyc.py` 0 failed on both backends; step 4 well
above CI's `LLVM_BASELINE_PASS` floor (raise that floor in ci.yml when
a change lifts the count).

**Touched a header? `make clean` first, before you trust any of it.**
Header dependencies are incomplete, so `make` alone happily links stale
objects against a changed layout. The resulting failure looks like a
real bug and is not: adding a bit to `Sym` produced `fail: no instance
for type 'int'` and a bogus `Primitives::find` assertion, and inserting
a `virtual` into `IFACallbacks` (which renumbers the vtable) segfaulted
`ifa-test --phase codegen-c` with no output at all. Each time the fix
was `make clean && make`, and each time the crash first read as a
regression worth debugging. Anything that changes a struct layout, a
bitfield, an enum's numbering, or a vtable needs the clean build.

### Goldens: re-bless only what the change is ABOUT

`ifa-test --rebless` rewrites `.expected` files wholesale. Before using
it, diff the old goldens against the new output and confirm every
changed line belongs to your change. Two real cases from this repo:

- All 22 `codegen-c` goldens went stale for ten days because
  `93a771e3` added `_CG_set_argv(argc, argv)` to the emitted `main()`
  and nobody re-blessed. Correct behaviour, stale fixture: re-bless.
- `mark_distance_skew` / `mark_setter_skew` (ifa/issues/007) differ by
  `ess=3` vs `ess=4` — a splitter stage that stopped firing. The golden
  is the RIGHT answer: re-blessing would bake the regression in and
  silently retire the coverage. These carry
  `<fixture>.<phase>.known_issue` instead, which reports `KNOWN`,
  does not fail the run, and flips to `PASS` by itself when the stage
  works again. `--rebless` refuses to touch a fixture that has one.

Same rule as `tests/<name>.py.known_issue` for the pyc suite — see
[issues/README.md](issues/README.md). Prefer it over baking in wrong
output whenever you intend to fix the bug.

### CI's environment is not yours

CI pins clang/LLVM **20** on ubuntu-24.04 (the unversioned packages
resolve to 18, whose coroutine ABI breaks the async tests) and exports
`USE_LLVM=1` job-wide. A local box on a different LLVM can pass all
five steps and still surface a version-specific failure there — the
C-backend goldens are text and version-independent, but anything
touching coroutines or emitted IR is not.

## Corpus sweeps — check the cache before running one

A `shedskin_examples` sweep gets re-run across sessions because nothing
recorded that it had been done, or what tree it was done against.
`./corpus_sweep.sh` fixes that: results are cached under `sweeps/`, keyed
on the WORKING TREE — HEAD's short hash, plus a digest of the uncommitted
diff when the tree is dirty — so a repeat on an unchanged tree returns
instantly, and a result from a different tree is never mistaken for a
current one.

```sh
./corpus_sweep.sh -l                      # what has already been measured
./corpus_sweep.sh -m compile              # pyc exit status only       (~5 min)
./corpus_sweep.sh -m run                  # + the binary's exit status (~11 min)
./corpus_sweep.sh -m check                # + warnings, CPython rc,
                                          #   and stdout vs CPython    (~11 min)
./corpus_sweep.sh -m compile -e "PYC_CSELEM=3"
./corpus_sweep.sh -m check -R             # + confirm run timeouts alone
```

**It runs parallel (2026-08-31): `-j` compiles at `nproc`, `-J` runs and
CPython at `nproc/4`, and CPython results are cached in
`sweeps/cpython-cache/` (gitignored) across sweeps.** `check` went 40 →
11 minutes, validated at 76-of-77 programs byte-identical to the serial
script on one tree and one binary; the 77th is `score4`, which straddles
the 120 s cap and flips on repeats of a single build. The remaining floor
is `othello3`, which takes 317 s **on its own** to fail to compile.

Two things follow from the cache. CPython's answer changes only when the
corpus does, so the key is the corpus tree hash + uncommitted `**/*.py` +
the python3 version — pass `-C` to force a re-run (a few programs, e.g.
`oliva2`, read a file their own run rewrites). And a timeout is the one
verdict a parallel pass can fabricate, so `rc=124` is always re-taken
ALONE for CPython (`hq2x` needs 116 s of the 120 s cap and WAS being
fabricated) and under `-R` for the pyc binaries (measured: 0 of 72).

**The cache is keyed twice.** The `tree` key names a sweep for a human
(which commit?) and deliberately ignores what a sweep itself writes — its
own `sweeps/*.tsv` and `INDEX.md` row, and every corpus output file the
binaries rewrite (`chaos/py.ppm`, `tonyjpegdecoder/tiger1.bmp`, …).
Without that, finishing a sweep changed the key it had just recorded, so
**the cache could never hit**. The `# content` key answers the other
question — is the thing under test the same? — over the `pyc` binary,
`__pyc__/*.py`, every corpus `*.py`, `-e`, the mode and both timeouts. It
exists because the tree key necessarily changes when you COMMIT, which
orphaned the measurement the commit was landing. Lookup tries the
filename, then the content digest. A corpus `.py` edit invalidates both.

**Run `-l` before starting a sweep**, and record the result of any new one
in the issue it was measured for. `sweeps/*.tsv` is text and IS committed
— it is a record of what has been measured, not a build artifact.

`compile` is not enough evidence for most changes. A binary that builds
and then segfaults is invisible to it and to the test harness alike
(ifa/issues/102), and `check` is the only mode that catches a program
that compiles with **no warnings at all** and still prints the wrong
answer.

Two ways to get a sweep that looks real and is not:

- **Never run two sweeps concurrently.** More so now that one sweep uses
  the whole machine. They contend and produce spurious `rc=124` timeouts
  — a "regression" in one arm of an A/B that vanishes when the program is
  re-run alone. One such reading survived into a comparison in this repo
  before being caught. Within a single sweep this is now handled: see the
  confirmation rule above.
- **Never `make` while a sweep is running.** Relinking `pyc` mid-sweep
  makes in-flight invocations die with `Permission denied`, which the
  sweep records as a compile failure.

`shedskin_sweep.sh` (parallel, compile-only, buckets failures by their
first diagnostic) is still the right tool for *triaging* what is broken;
`corpus_sweep.sh` is for *comparing two trees* and for the run/output
status. `ifa/issues/runstatus.sh` predates both.

## Do not check in build artifacts

Never `git add` compiled binaries, object files, generated IR, or
debug-info bundles — this repo has needed cleanup for exactly this
before (compiled test binaries, `.dSYM` bundles, and generated
`.ll` files had accumulated under `tests/` and `ifa/tests/`).
`.gitignore` uses a pattern-based rule (`tests/*` / `ifa/tests/*`
plus extension negations) rather than a per-file whitelist, so new
tests should never need a matching `.gitignore` edit to stay
untracked — if a new build output isn't being ignored, fix the
pattern instead of adding the file. Same rule for `ifa/ifa`,
`ifa/ifa-test`, and any other Makefile-produced binary: these are
rebuilt by `make` and must never be committed.

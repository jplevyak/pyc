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

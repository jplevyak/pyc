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

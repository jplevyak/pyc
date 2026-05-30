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

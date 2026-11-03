# 187 — `import X` from a file named `X.py` overflows the frontend's stack

**Status:** open; observed 2026-09-28, filed 2026-11-03 from
[025](025-shedskin-examples-coverage.md)'s side findings. A crash, both
backends. Not re-verified since.

## Symptom

A module that imports itself (`X.py` containing `import X`, or a
cycle `A` imports `B` imports `A`) makes `build_import_if1` →
`build_one_module_if1` → `build_if1_pyda` recurse without bound
(`python_ifa_build_if1.cc`), and pyc exits with SIGSEGV. CPython binds
the partially initialised module and continues.

## Fix

Keep the set of modules being built; an import of a module already on
that stack binds the module Sym that exists and does not re-enter. The
semantics of reading a name the partially initialised module has not
yet defined is then [173](173-silent-deviations-found-by-the-strict-suite-check.md)
item 2's question, and the answer is the same: refuse it.

## Verification

A two-file fixture (`tests/import_cycle_*`) compiles and prints what
CPython prints; a self-import that reads an undefined name is refused.

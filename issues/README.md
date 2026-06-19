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

- [001-fa-crash-captured-locals.md](001-fa-crash-captured-locals.md)
  — `def outer(): return lambda x: x + captured` crashes the FA
  at `unique_AVar` assertion. Blocks Python closures over
  enclosing-scope locals.
- [002-fa-crash-escaped-closure.md](002-fa-crash-escaped-closure.md)
  — Passing a bound method closure across a function-call
  boundary segfaults pyc. Blocks closures that escape their
  binding scope as arguments / return values / global assignment.
- [003-subclass-struct-layout-mismatch.md](003-subclass-struct-layout-mismatch.md)
  — Subclasses that redefine inherited fields produce C compile
  errors ("struct has no member 'e0'"). Same root cause as the
  existing `class_attr_mutation.py` xfail. Blocks inheritance-
  driven polymorphism in lists.

## Closed (archive)

Closed issues live in [`closed/`](closed/) with the closing
commit ref recorded in each file's status line.

- [015](closed/015-pyc-pod-records-no-frontend-hook.md) —
  `@pyc_struct` decorator wired (originally
  `ifa/issues/015`; moved here because the gap was in the pyc
  frontend, not the ifa library).

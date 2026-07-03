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
- [004-is-operator-unimplemented.md](004-is-operator-unimplemented.md)
  — `is` and `is not` operators are unimplemented (frontend
  generates `__is__` dispatch but no class defines the method).
  Blocks `x is None`-style narrowing required for recursive
  data structures.
- [005-while-true-fa-crash.md](005-while-true-fa-crash.md) —
  `while True:` segfaults FA's `update_in` (constant-cond IF has
  no valid contour). Workaround: use a real/sentinel loop
  condition.
- [006-fstring-interpolation-not-implemented.md](006-fstring-interpolation-not-implemented.md)
  — **Partially fixed**: f-strings now interpolate correctly
  (`{expr}`, `!s`/`!r`/`!a`, nested indexing/calls) on both
  backends. Format specs (`{x:.2f}`) remain a clean compile error
  rather than a silent no-op, pending a follow-up.
- [007-decorators-not-applied.md](007-decorators-not-applied.md)
  — Any decorator other than the builtin `@vector`/`@pyc_struct`
  directives is evaluated but never applied; decorated
  functions/classes behave as if undecorated with no diagnostic.
- [008-set-literal-genexpr-crash.md](008-set-literal-genexpr-crash.md)
  — **Set literals/comprehensions fixed** (new `__pyc__/08_set.py`
  `set` class); also fixed a pre-existing `x in y`/`x not in y`
  operand-order bug affecting *every* container type, found along
  the way. Generator expressions now fail cleanly instead of
  crashing (real support tracked with issue 014).
- [009-dict-comprehension-drops-comp-for.md](009-dict-comprehension-drops-comp-for.md)
  — Dict comprehensions "compile" (exit 0) but silently drop the
  `for` clause, producing generated C that aborts at runtime with
  `"matching function not found"`.
- [010-multiple-inheritance-unrelated-bases.md](010-multiple-inheritance-unrelated-bases.md)
  — `class C(A, B)` with two independent base classes fails with
  a confusing type-inference cascade ending in a C compile error;
  root cause only partially traced (frontend side looks
  base-count-agnostic; likely an FA/dispatch-level gap).
- [011-exception-handling-unimplemented.md](011-exception-handling-unimplemented.md)
  — `try`/`except`/`finally`/`raise` are entirely unimplemented
  (`fail("statement not supported")`); no exception object model
  or unwinding mechanism exists yet. Largest lift in this batch
  besides generators.
- [012-with-statement-unimplemented.md](012-with-statement-unimplemented.md)
  — `with` (context managers) is unimplemented; a happy-path
  `__enter__`/`__exit__` desugaring doesn't need issue 011's
  exception model, only full exception-safety does.
- [013-assert-statement-unimplemented.md](013-assert-statement-unimplemented.md)
  — `assert` is unimplemented (`fail("'assert' not yet
  supported")`); smallest of the missing-statement issues, good
  first target.
- [014-generators-yield-unimplemented.md](014-generators-yield-unimplemented.md)
  — `yield` (generator functions) is unimplemented; needs a
  resumable-function execution model (state-machine transform or
  stackful coroutines) not present anywhere else in pyc today.
- [016-missing-grammar-level-syntax.md](016-missing-grammar-level-syntax.md)
  — Five newer syntax forms aren't in the grammar at all and fail
  as parse errors: `async`/`await`, walrus `:=`, `match`/`case`,
  PEP 484 type annotations, and extended unpacking (`a, *b =
  ...`). Grouped as one filing; split out per sub-item as work
  starts (priority/effort vary widely — annotations and walrus
  are small, async/await and match/case are substantial).

## Closed (archive)

Closed issues live in [`closed/`](closed/) with the closing
commit ref recorded in each file's status line.

- [015](closed/015-pyc-pod-records-no-frontend-hook.md) —
  `@pyc_struct` decorator wired (originally
  `ifa/issues/015`; moved here because the gap was in the pyc
  frontend, not the ifa library).
- [017](closed/017-multi-instance-mutation-corruption.md) —
  `dict`/`set` lacked an `__init__`, so their `_keys`/`_vals`/
  `_items` list fields were bare class-body attributes — Python's
  classic mutable-class-attribute footgun, applied to a builtin
  container type. A second instance written to after a first one
  silently read/wrote the wrong data. Fixed by giving both an
  explicit `__init__`.

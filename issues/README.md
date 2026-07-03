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
  — **Fixed**: dict comprehensions now work correctly on both
  backends, sharing the loop-lowering machinery added for issue
  008's set comprehensions. Two unrelated pre-existing gaps found
  while testing this were filed separately (018, 019).
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
- [018-dict-mixed-key-types-boxing-failure.md](018-dict-mixed-key-types-boxing-failure.md)
  — A program using `dict` (or `set`) with two different key/
  element types anywhere fails to compile with a `BOXING`/"mixed
  basic types" FA violation — each shared internal comparison
  method (`_keys[i] == key`) isn't specialized per key type.
- [020-str-builtin-call-broken.md](020-str-builtin-call-broken.md)
  — `str(x)` (the builtin call, not the `.__str__()` method) fails
  for every type — `class str` has no constructor accepting a value
  to convert, and there's no frontend special-case for it (unlike
  `print()`, which does have one). Compiles with exit 0 despite the
  errors; the `.__str__()` method form works fine.
- [021-scope-map-pointer-hash-nondeterminism.md](021-scope-map-pointer-hash-nondeterminism.md)
  — `PycScope::map` hashes on the raw pointer value of interned
  name strings, not their content, so unsorted iteration over it
  varies by process (ASLR/heap layout) even for byte-identical
  input. Confirmed via a one-off `expr_evaluator.py` LLVM compile
  flake that cleared on rerun. One narrow fix exists (class-field
  ordering); most other consumers are unaudited. Distinct from
  (and not fixed by) `ifa/issues/closed/009` + `ifa/notes/004`,
  which addressed the *ifa library's* own `Vec`-as-set hashing,
  not the pyc frontend's `PycScope::map`.

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
- [019](closed/019-dict-missing-str-repr.md) — `dict` now has
  `__str__`/`__repr__`, modeled on `set`'s (which already had it).
  `print(some_dict)` shows real contents instead of the generic
  `<instance>` placeholder.
- [013](closed/013-assert-statement-unimplemented.md) — `assert`
  now lowers to an abort (print `AssertionError[: msg]`, `exit(1)`)
  on both backends; a fully catchable `AssertionError` still awaits
  issue 011. Found and fixed an unrelated pre-existing bug along the
  way: the v2 LLVM backend's `__pyc_c_call__` never stripped a
  leading `::` (a C++-only global-scope qualifier) from the target
  function name, so `exit()` — and anything else calling a
  `"::"`-qualified C function — could never link on that backend.

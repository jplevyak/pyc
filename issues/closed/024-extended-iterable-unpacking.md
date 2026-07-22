# Issue 024: Extended iterable unpacking (`a, *b = [1, 2, 3]`, PEP 3132)

**Status:** closed 2026-07-13. Fixed for the assignment-target
position (this issue's own scope); one narrower gap and one
unrelated, pre-existing bug found along the way are documented
below rather than silently left unmentioned.
**Affects:** `python.g` (`testlist`'s grammar), `python_ifa_build_syms.cc`
(`mark_store`), `python_ifa_build_if1.cc` (`emit_assign_to_target`,
`build_if1_pyda`'s new `PY_star_expr` case).

## Description (original filing)

```python
a, *b = [1, 2, 3]
```
```
star_unpack.py:1: syntax error after ','
```
No starred-target form in the assignment-target grammar (`expr_stmt`, `python.g:80`). Note this is *not* the same gap as `*args` in function definitions/calls, which already works (see `PY_star_arg`/`PY_dstar_arg` handling in `python_ifa_build_syms.cc`/`python_ifa_build_if1.cc`) — this is specifically the assignment-target position. Once parsed, the lowering needs to bind the starred name to a (list) slice of the remaining unpacked elements, alongside the existing plain multi-target unpacking already supported (`tests/tuple_unpack.py`).

## Fix

**Grammar** (`python.g`): a new `star_expr: '*' expr` production and
a `testlist_item: test | star_expr` wrapper; `testlist` (used by
`expr_stmt` for both assignment targets AND the final RHS value —
the grammar can't tell them apart syntactically, same as chained
assignment `a = b = value` already couldn't) now accepts
`testlist_item` instead of a bare `test`. New `PY_star_expr`
`PyASTKind`.

**Symbol building** (`mark_store`, `python_ifa_build_syms.cc`): a
`PY_star_expr` node recurses into its own inner target (the same way
every other wrapper kind here already does), so `*b`'s `b` gets
marked a fresh local exactly like a plain (non-star) target would.

**Lowering** (`python_ifa_build_if1.cc`):
- `build_if1_pyda`'s new `PY_star_expr` case exists only so the
  enclosing `PY_tuple` target's own (for a target, unused —
  `emit_assign_to_target` never reads it) "make tuple from targets"
  send has something to build for that child; it just recurses into
  the real inner target.
- `emit_assign_to_target`'s `PY_tuple`/`PY_testlist`/`PY_exprlist`
  branch now scans for a `PY_star_expr` child. Without one, behavior
  is byte-for-byte the pre-existing non-star logic. With one:
  leading targets (before the star) bind `val[0..n_leading-1]`
  positionally, same as before; trailing targets (after the star)
  bind the LAST `n_trailing` elements, positionally from the end (so
  they're correct regardless of how many elements the star itself
  consumes); the star target binds a NEW list built by a runtime
  loop (`idx = n_leading; while idx < len(val) - n_trailing:
  star_list.append(val[idx]); idx += 1`, hand-built via `if1_loop` —
  the same primitive `PY_while_stmt`/list-comprehensions use, mirrored
  rather than reusing `__pyc_getslice__` because that would preserve
  `val`'s own type instead of PEP 3132's "always a list" rule, and
  `tuple` doesn't define `__pyc_tolist__` to convert it afterward
  anyway). No length-mismatch validation (real Python raises
  `ValueError` for "not enough values to unpack") — matches this
  function's own pre-existing non-star behavior, which never
  bounds-checks `val[j]` either.
- A bare `*b = x` (star target not inside a list/tuple) and
  `a, *b, *c = x` (two stars) both `fail()` at compile time with
  CPython's own wording ("starred assignment target must be in a
  list or tuple" / "multiple starred expressions in assignment")
  rather than silently doing something wrong.

Verified byte-identical to `python3` on both backends: leading-star
(`*c, d = ...`), trailing-star (`a, *b = ...`), middle-star
(`e, *f, g = ...`), a tuple source (`h, *i = (10, 20, 30)`), and a
non-simple-name star target (`p, *box.items = [1, 2, 3]`, an
attribute) — all in `tests/star_unpack.py` / `.exec.check`. Also
manually verified (not committed, see "Known gaps" below): a
subscript-slice star target (`p, *a[0:1] = [1, 2, 3]`, matches
CPython's `1 [2, 3, 0]`) and both error cases against CPython's own
`SyntaxError` wording.

## Known gaps (explicitly out of scope, not silent traps)

- **Nested parenthesized tuple targets** (`x, (y, *z) = (1, (2, 3,
  4))`) don't parse. The inner `(y, *z)` goes through
  `testlist_comp`, a separate grammar rule from `testlist` that
  wasn't extended — a real gap (CPython supports this), loudly a
  syntax error rather than silently wrong. `for_stmt`'s target
  (`exprlist`) is similarly unextended (`for a, *b in pairs:` also
  doesn't parse) — issue 024's own filing scoped to `expr_stmt`
  specifically.
- **`j, *k = [1]`-shaped always-empty star targets are correct in
  isolation but not committed as a test.** Combining one with ANY
  OTHER list of the same element type elsewhere in the same program
  can hit a separate, pre-existing, unrelated FA bug — see
  [ifa/issues/040-empty-list-shared-clone-type-inference.md](../../ifa/issues/closed/040-empty-list-shared-clone-type-inference.md),
  filed while landing this fix. Confirmed NOT caused by this
  feature's own lowering: a plain `b = [2, 3]; print(b); k = [];
  print(k)`, zero star syntax involved, reproduces it identically.

## Verification plan

1. ~~Extended unpacking: `a, *b = [1, 2, 3]` gives `a == 1`,
   `b == [2, 3]`; also the leading/middle-star forms (`*a, b = ...`,
   `a, *b, c = ...`).~~ — **done**, see "Fix" above.
2. ~~Add one test file for extended unpacking once implemented.~~ —
   **done**: `tests/star_unpack.py` / `.exec.check`, verified on
   both backends. Full suite 189/0 both backends, `test_dparse` and
   `ifa test_llvm` clean.

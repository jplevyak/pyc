# 037 — sudoku2: `_CG_str_ne`'s `_CG_any` exemption was unsound (hard C++ compile error), plus a missing `str.index()`; a deeper pre-existing FA bug remains

**Status: partially fixed 2026-08-06** (two of three issues closed;
the third is a pre-existing, deep FA bug, tracked separately, not
attempted here). Found investigating
`shedskin_examples/sudoku2/sudoku2.py`.
**Affects:** `python_ifa_main.cc`'s `c_call_codegen` (the
`strict_c_call` salvage guard added by
[ifa/077](../../ifa/issues/closed/077-primitive-equality-codegen-missing-salvage-guard.md));
`__pyc__/01_str.py` (new `str.index`).
**Related:** [036](036-list-pop-insert-tuple-hash-and-unary-literal-defaults.md)
— same "missing sequence op" bucket, same session;
[ifa/077](../../ifa/issues/closed/077-primitive-equality-codegen-missing-salvage-guard.md)
— the guard this issue found a real gap in, one call away from its
own stated scope;
[ifa/049](../../ifa/issues/049-raise-only-contour-notype.md) — the
pre-existing, still-open FA bug this issue's third finding maps onto
(now with two new trigger variants, see that doc).

## Issue 1: `_CG_str_ne`'s `_CG_any` exemption was unsound — FIXED

### Symptom

`sudoku2.py` failed with a hard C++ compile error (not a warning):

```
sudoku2.py.c:640:8: error: no matching function for call to '_CG_str_ne'
  640 |   t1 = _CG_str_ne(t2, t3);
      |        ^~~~~~~~~~
pyc_c_runtime.h:1295:17: note: candidate function not viable: cannot
convert argument of incomplete type '_CG_any' (aka 'void *') to
'const char *' for 2nd argument
```

### Root cause

`ifa/077`'s `strict_c_call` salvage guard (`python_ifa_main.cc`)
checks, for `str`'s six `_CG_str_*` comparison primitives only, that
each argument's actual resolved type matches its declared type —
degrading to a runtime assert instead of emitting an invalid cast
when it doesn't (the same `num_kind`-tolerance pattern issues
034/035/077 established elsewhere). It exempted **either** side being
`_CG_any` (`void*`) unconditionally, reasoning (correctly, but only
for a different call family) that `void*` converts implicitly to any
other pointer type — true for `list`/`tuple`/`dict`/`set`/... , whose
own C representation (`pyc_c_runtime.h`) is `typedef void
*_CG_list;` etc., literally identical to `_CG_any`'s `void*`, so an
`_CG_any` argument there is a void*-to-void* no-op. **False** for
`str`/`bytes`, whose C representation is `typedef char *_CG_string;`
— a genuinely different pointer type. C++ (unlike C) does not
implicitly convert `void*` to `char*` (or any other unrelated pointer
type) at a function call boundary; it requires an explicit cast. Since
`strict_c_call`'s loop is *already* scoped exclusively to `str`'s six
comparison primitives — whose declared argument type is
unconditionally `str` — the exemption could only ever fire in the one
direction that's unsafe (`ac == "_CG_any"`, since `dc` is always
`"_CG_string"` here), masking exactly the case the guard exists to
catch. `str.__eq__`'s sibling contour, hit with the identical shape
elsewhere in the same trace, correctly degraded to the assert — only
`__ne__`'s contour slipped through, confirming this wasn't a design
gap in the guard's overall shape, just this one exemption line.

### Fix

Removed the `_CG_any` exemption from this scope entirely (it's dead
weight at best and unsound at worst for a loop that only ever sees
`str`-declared arguments). `mismatch = dc && ac && strcmp(dc, ac);` —
no special case.

## Issue 2: `str` has no `.index()` — FIXED

`sudoku2.py`'s `fread()` does
`self.setval(row, lines[row].index(str(digit)), digit)` inside `try:
... except ValueError: pass`. `str` had `.find()` (returns -1) but no
`.index()` at all; the call silently dispatched into **`list.index`'s
own body** (`__pyc__.py:1028` in the diagnostic trace — `list.index`
returns -1 on failure), treating the `str` receiver as if it were a
`list`, and the resulting type confusion cascaded into "mixed basic
types: (list int64 str)" warnings on `self.final`'s cells and,
ultimately, a "matching function not found" runtime assert. Added
`str.index(sub)` (`__pyc__/01_str.py`): calls `find()`, raises
`ValueError` on -1 — matching CPython exactly (unlike `list.index`'s
established "-1 instead of raising" convention, deliberately chosen
before [011](../../issues/closed/011-exception-handling-unimplemented.md)'s
exception support existed; `str.index` doesn't need that
grandfathering and `sudoku2.py`'s own `except ValueError` needs the
real thing).

With both fixes, `sudoku2.py` **compiles with zero warnings** (was a
hard compile error).

## Issue 3: a deeper, pre-existing FA bug remains — NOT fixed, mapped to `ifa/049`

`sudoku2.py` compiles clean but **segfaults at runtime** during
`fread()`, before the first `print`. Root-caused (by direct reduction,
not full FA tracing) to a shape [ifa/049](../../ifa/issues/049-raise-only-contour-notype.md)
doesn't yet document: `fread`'s `try/except ValueError` around the new
`str.index()` call, exercised across a loop where the digit is
sometimes present (returns normally) and sometimes absent (raises),
hits the same "raise path contributes nothing to `fn->ret`" FA gap
049 already tracks — but in a shape 049's own fix verification doesn't
cover. Confirmed via three separate minimal repros, added to 049's own
doc:

1. Two independent raise-capable functions (a free function and an
   unrelated record-class method), each individually fine, each called
   from two *static* call sites (one raising, one not) — fails to type
   when both exist in the same program. Confirmed pre-existing on
   unmodified HEAD, unrelated to this issue's two fixes.
2. The same, in miniature, is what `sudoku2.py` itself hits:
   `fread`'s try/except is effectively a second independent raiser
   once `str.index()` (fix #2 above) gives it something to raise.
3. An even simpler variant found while writing this issue's own
   regression test: **one** call site, in a loop, with a
   runtime-varying (not literal) argument that sometimes raises and
   sometimes doesn't — compiles with **zero warnings** but silently
   returns garbage instead of ever reaching the `except` branch. No
   crash, no diagnostic — a strictly worse failure mode than the other
   two shapes' loud NOTYPE/assert.

Not attempted here — this is squarely the kind of deep, whole-program
FA convergence issue 049 already flags as needing dedicated
instrumentation to root-cause, not a narrow fix discoverable by
inspection. `tests/str_index.py` was deliberately trimmed to only the
non-raising path (`.find()`/`.index()` on a present substring) once
this surfaced, rather than asserting behavior that doesn't reliably
work yet.

## Verification

- New test `tests/str_index.py` (non-raising path only, per Issue 3
  above), both backends, output byte-identical to `python3`.
- `test_pyc.py`, C and LLVM backends, `PYC_CSM` unset: 247/11/0/4 both
  (0 regressions vs. the issue-036 baseline; no new tests added here
  beyond `str_index.py` since the other candidate test hit Issue 3).
- `test_pyc.py`, C and LLVM backends, `PYC_CSM=2`: same 4 pre-existing
  failures both, no new ones.
- `shedskin_sweep.sh`: clean before/after from the same commit (stash/
  sweep/pop/rebuild/sweep, not a diff against a stale snapshot). Net:
  **zero regressions**, two examples improved: `sudoku2` FAIL →
  compiles (this issue), and `sudoku5` FAIL → compiles-with-warning as
  a side effect of `str.index()` alone (not investigated further, out
  of this issue's scope).
- `sudoku2.py`: compiles with zero warnings on the C backend
  (confirmed both fixes are needed together — the void*-cast fix alone
  gets a clean-with-warnings compile with `list`/`str` dispatch
  confusion still visible; adding `str.index()` clears the warnings
  entirely) but **does not yet run to completion** (Issue 3, segfault
  in `fread()`, mapped to `ifa/049`'s now-expanded doc).

## What this unblocks

- The `_CG_str_ne`/`_CG_any` fix is general, not sudoku2-specific: any
  program where a salvage-degraded (`_CG_any`-typed) value reaches a
  `str`/`bytes` comparison dunder other than `__eq__` (which already
  had *some* coverage via a different contour) no longer risks a hard
  C++ compile error — matches the loud-assert-over-invalid-C
  convention every other salvage-reachable call site in this codebase
  already follows.
- `str.index()` closes a real, CPython-shaped gap independent of
  sudoku2 (any program doing substring-position lookup with a
  raise-on-missing contract, matching `list.index`'s pre-011 "-1"
  convention finally having a real counterpart for `str`).
- `sudoku5` (a related corpus example) also progressed, apparently for
  free.
- `ifa/049` now has three concrete, minimal, corpus-independent repros
  for a previously single-shape-documented bug — meaningfully lowers
  the bar for whoever picks that issue up next.
- Does **not** unblock `sudoku2.py` running to completion — that
  remains gated on `ifa/049`'s open FA work.

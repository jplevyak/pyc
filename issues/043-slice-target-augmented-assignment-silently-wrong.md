# 043 — slice-target augmented assignment (`a[i:j] += x`) silently corrupts the list, worse than the known "acts like `=`" gap

**Status:** open, found 2026-08-08 while diagnosing
[issues/025](025-shedskin-examples-coverage.md)'s TODO list item 17.
The gap itself was already known and documented inline (a comment in
`python_ifa_build_if1.cc`, added alongside the sibling plain-index fix
that motivated it) but never given its own issue file, and — as
verified here — the actual behavior is worse than that comment's own
description.

**Affects:** `python_ifa_build_if1.cc`'s `PY_augassign` case, the
`t->is_slice` branch (~line 2773).

## What the existing code comment says vs. what's actually happening

The comment at the `is_slice` branch says:
```cpp
// issues/025: slice augmented-assignment (`a[i:j] |= x`) still
// has the same silent-wrong-answer shape as the plain-index
// case below -- the value the operator sees isn't the current
// slice's contents, so `|=` acts like `=`. Not fixed here...
```
i.e. the claimed symptom is "`+=`/`|=`/etc. silently behaves like a
plain `=`." Verified this significantly understates it: it's not just
a no-op in-place operator — the RHS gets applied to the list **twice**,
once wrongly (as a full replacement) and once more as a genuine
`__iadd__`/extend, but against the *whole list*, not the slice.

## Repro

```python
b = [1, 2, 3, 4, 5]
b[1:3] += [10, 20]
print(b)
```
- CPython: `[1, 2, 3, 10, 20, 4, 5]` (the slice `[2, 3]` is replaced by
  `[2, 3] + [10, 20]` = `[2, 3, 10, 20]`).
- pyc: `[1, 10, 20, 4, 5, 10, 20]` — **not** simply the documented
  "acts like `=`" result (which would be `[1, 10, 20, 4, 5]`, matching
  plain `b[1:3] = [10, 20]`) — `[10, 20]` additionally appears a
  *second* time, appended at the very end of the whole list.

## Root cause, traced via generated C

```c
t2 = _CG_f_2270_3/*list::__pyc_setslice__*/(t5, t3);  // b[1:3] = [10, 20] -- the documented bug
t4 = _CG_f_2417_5/*list::__iadd__*/(t2, t3);           // t2 (= b, now already mutated) .__iadd__([10, 20])
```
Confirms the mechanism precisely: the `is_slice` branch first eagerly
calls `__pyc_setslice__` with the raw RHS (the already-documented
"acts like `=`" step), **then separately** calls the in-place operator
(`__iadd__` for `+=`) with the *slice-target object itself* (`t->rval`
— which for a slice target is the whole list being sliced, `b`, not a
view of just the sliced-out elements) and the same RHS again. For
`list.__iadd__`, that's `extend()` — which appends to the end of the
*whole list*, regardless of the original slice bounds. The
already-known bug (treating the current slice's contents as
irrelevant) and this newly-confirmed one (running the in-place op
against the wrong receiver — the whole container, not the extracted
slice) compound into genuine list corruption, not just a lost
`+`/`|`/etc.

Confirmed the code comment's own repro shape (`|=`) doesn't even apply
to lists in real Python (`list |= list` is a `TypeError` in CPython —
`|=`/`&=`/etc. are for sets and ints, not lists) — pyc currently
"handles" it anyway, degrading to a warning
(`unresolved call '__ior__'`) and silently no-op'ing per-element,
which is arguably fine (invalid input, no crash) but means the
comment's own illustrative example was never actually a valid
CPython program to compare against. `+=`, which *is* valid for lists,
is the operator that actually demonstrates the real bug.

## Why not fixed here

Confirmed by the existing comment and independently: `__pyc_setslice__`
takes a whole replacement *sequence*, not a single value, so a correct
fix needs its own read-slice → apply-op-elementwise-or-via-concat →
write-slice-replacement shape — structurally different from the
plain-index case's single-value read/compute/write swap (already
fixed). No corpus example currently exercises this (per the original
2026-07-16 note, still true), so there's no forcing function beyond
correctness itself. Given the confirmed severity (list corruption, not
just a dropped operator), this deserves its own fix pass rather than
continuing to defer it as "no failing example needs it."

## Verification plan

- `python3 repro.py` → `[1, 2, 3, 10, 20, 4, 5]` is the reference.
- Also verify `-=`/`*=` and other in-place operators against a slice
  target, and a slice assigned from a *shorter* or *longer* sequence
  than the original slice (Python's slice-assignment semantics allow
  the replacement to change the list's length) — none of these shapes
  are covered by the existing `tests/augassign_subscript.py` (plain
  index only, confirmed by reading that test file).
- Full `test_pyc.py` both backends.
- New regression test once fixed — `tests/augassign_subscript.py`
  could be extended, or a new `tests/augassign_slice.py` added.

## What this unblocks

Correct behavior for slice-target augmented assignment generally — not
demonstrated as any specific corpus example's blocker (per the
original note, still true today), but a genuine, confirmed
data-corruption bug in ordinary, valid Python code (`lst[i:j] += x`),
not just a missing-feature gap.

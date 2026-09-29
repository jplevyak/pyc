# 043 — `a[i:j] += x` acts like `a[i:j] = x` (silent wrong answer)

**Status:** open. Re-verified 2026-09-28. Rewritten; history in git:
`git show e3b44e2c:issues/043-slice-target-augmented-assignment-silently-wrong.md`.

## Symptom

```python
b = [1, 2, 3, 4, 5]
b[1:3] += [10, 20]
print(b)     # CPython: [1, 2, 3, 10, 20, 4, 5]   pyc: [1, 10, 20, 4, 5]
```

No diagnostic, exit 0. (The 2026-08 version also appended `[10, 20]` a
second time to the end of the list. That half is gone, and it now behaves
exactly like a plain slice assignment.)

## Cause

`PY_augassign`'s `t->is_slice` branch (`python_ifa_build_if1.cc`, the
comment `issues/025: slice augmented-assignment … acts like '='`) stores
the RHS into the slice without reading the slice first. The plain-index
case was fixed with a read/compute/write sequence. A slice needs the
sequence-shaped version of the same thing.

## Fix

Lower `a[i:j] op= x` as CPython evaluates it:

```python
t = a.__pyc_getslice__(i, j, step)   # read the slice (a new list)
t = t.__iop__(x)                     # __iadd__ etc. on the slice, not on a
a.__pyc_setslice__(i, j, t)          # write back; length may change
```

Evaluate `i` and `j` once. For `list |= list` (a `TypeError` in CPython)
the result should be the ordinary unresolved-operator refusal.

## Verification

A `tests/augassign_slice.py` fixture with `+=`, `*=`, a replacement that
changes the length, negative bounds and a step, all matching CPython.
`tests/augassign_subscript.py` stays green.

# 185 — `[1, 2] == (1, 2)` is True

**Status:** open. Found 2026-10-08 in the sunfish work
([ifa/closed/185](../ifa/issues/closed/185-FA-merged-tuple-receiver-makes-the-arity-unroll-live.md)
item 2); filed here 2026-11-03. A silent wrong answer on the default path,
with no warning.

## Symptom

```python
print([1, 2] == (1, 2))     # CPython: False.   pyc: True
print((1, 2) == [1, 2])     # False on both (tuple.__eq__ is per-arity since ifa/185)
```

## Cause

`list.__eq__` (`__pyc__/04_sequence.py`) never checks that its argument is
a list; it compares elements. `tuple.__eq__` got a per-arity body whose
unconstrained branch answers `False`, which is why the mirror case is
right.

## What was tried

An `isinstance(l, list)` guard in `list.__eq__` untyped `l` in two suite
tests (`builtin_zero_arg_ctor`, `list_tuple_eq_ne`: `'l' has no type`) and
was reverted. Why the guard untypes the operand is the thing to find
first; the layout plan of ifa/102 (`_CG_TUPLE_FIXED_ARITY` /
`_CG_TUPLE_DYNAMIC` header bits) now gives a `{list, tuple}` value a
run-time test, so a `list.__eq__` that dispatches on the argument's kind
is the other route.

## Rule

By README rule 2 this is either fixed or flagged: until fixed it needs a
named accommodation that warns and is off under `--strict`.

## Verification

Both prints are `False` on both backends; `builtin_zero_arg_ctor` and
`list_tuple_eq_ne` still pass.

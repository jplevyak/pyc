# 185 — `[1, 2] == (1, 2)` is True

**Status:** closed 2026-11-14. `list.__eq__` returns False for a
non-list, behind an `isinstance` guard. The guard had failed because FA
narrowed `not isinstance(...)` as if the `not` were not there; that is
fixed, and it was a silent mis-narrowing for any program. Found 2026-10-08 in the sunfish work
([ifa/closed/185](../../ifa/issues/closed/185-FA-merged-tuple-receiver-makes-the-arity-unroll-live.md)
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

## Resolution (2026-11-14)

**Why the guard untyped the operand.** FA narrows a variable per branch
after a discriminator test (issue 025): `isinstance`, `is None`,
`is not None`. To find the test under an `if`, `peel_wrapper_def`
(`fa.cc`) looked through wrapper calls, and it treated EVERY
zero-argument method call as a wrapper. `not x` lowers to `x.__not__()`,
so `if not isinstance(l, list): return False` was narrowed as
`if isinstance(l, list)`. The code after the guard got l's non-list
members, which for a list caller is nothing: `'l' has no type`. The same
held at user level for any `if not isinstance(...)` and `if not x is
None`. It also peeled through `it.__next__()` and a loop's
`__pyc_more__`, as if their result were the receiver.

A census of the suite: the peeler went through `__pyc_to_bool__`
(345 532 times), `__pyc_more__` (44 475), `__not__` (26 178), `__next__`
(2 726), `__bool__` (251) and `more` (68).

**Fix.** `IFACallbacks::narrowing_wrapper_polarity(Sym *selector)`: +1
for a call that keeps the operand's truth, -1 for one that inverts it, 0
for anything else. pyc answers +1 for `__pyc_to_bool__` and `__bool__`
and -1 for `__not__`, by interned symbol. The peeler goes only through a
nonzero one and counts the inversions; on an odd count the narrowed
branches swap. ifa's default peels nothing, as for the other narrowing
hooks (ifa/082).

Then `list.__eq__` (`__pyc__/04_sequence.py`) opens with
`if not isinstance(l, list): return False`, which folds away in a
list-against-list contour.

**Fixtures.** `tests/list_eq_other_type.py` (list against tuple, str and
an appended list, both orders) and `tests/narrowing_not_isinstance.py`
(`not isinstance`, `not not isinstance`, `not x is None`). Both print
CPython's output on both backends; `builtin_zero_arg_ctor` and
`list_tuple_eq_ne` still pass.

**Verified.** `make test` green: `test_pyc` 523 passed, 0 failed on
both backends. Corpus: `check__default__c7badb43+dc450f88` against
`check__default__a3134327+c2ee613f`: every program's compile, run,
warning and CPython verdict is unchanged (othello2 still differs, 177).
Contour counts moved in 15 programs, most by +1 EntrySet and +2
CreationSets, the shape of the new `isinstance` call in `list.__eq__`.
The largest moves are `loop` (EntrySets 377 → 365, CreationSets
1329 → 1314) and `rubik` (EntrySets 1163 → 1170). The two changes have
no separate toggle, so the counts are not attributed between them.

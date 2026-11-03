# 186 — a class that defines a method twice is refused as an ambiguous call

**Status:** open. Found 2026-10-08 in the sunfish work
([ifa/closed/185](../ifa/issues/closed/185-FA-merged-tuple-receiver-makes-the-arity-unroll-live.md)
item 5); filed here 2026-11-03. A refusal of a valid program, not a wrong
answer.

## Symptom

```python
class A:
    def f(self): return 1
    def f(self): return 2
print(A().f())      # CPython: 2.   pyc: ambiguous call
```

## Cause

Every `def` registers a candidate, and the generated per-arity tuple
overloads rely on that (ifa/185): several defs of one name on one class
are how arity dispatch is expressed. CPython's rule is last definition
wins, decided at class-body execution.

## Fix

Last-wins in the frontend, at class-body lowering, for user classes: a
later `def` of the same name in the same class body replaces the earlier
Sym before the candidates are registered. The arity-constrained defs the
generator emits carry `Sym::dispatch_arity` and must be left alone, which
is a structural test, not a name test.

## Verification

The repro prints `2` on both backends; `tests/tuple_eq_arity_dispatch.py`
and the per-arity tuple tests still pass.

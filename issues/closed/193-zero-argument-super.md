# 193 — zero-argument `super()` asserted in every method

**Status:** closed 2026-11-19, the day it was filed. Found writing the test
for [191](191-base-class-named-through-a-module.md). The suite tested only
`super(C, self)`, so the zero-argument form, the usual Python 3 spelling,
had never worked. A crash at compile time, not a wrong answer.

## Symptom

```python
class A:
    def f(self):
        return 1
class B(A):
    def f(self):
        return super().f() + 1
# pyc: if1_move: Assertion `a && b && a != b' failed.
```

## Cause

`super()` is `super(__class__, <first argument>)`. The lowering
(`build_builtin_call_pyda`) took the receiver from `ctx.fun()->self`, which
is the closure's self and null in an ordinary method. The one- and
two-argument forms take it from the arguments and were not affected.

## Fix

The receiver is the method's first formal (`fun_has[0]`, which the lowering
already computes and checks), whatever it is named.

## Tests

`tests/super_zero_arg.py`: `super()` in `__init__` and in methods, through
three classes, so that `B.f`'s `super()` reaches `A` when called on a `C`,
and in a method whose first parameter is not named `self`. Matches CPython
on both backends and under `--strict`.

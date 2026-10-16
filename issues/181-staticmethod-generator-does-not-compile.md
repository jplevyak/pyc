# 181 — a `@staticmethod` generator does not compile

**Status:** open. Found 2026-10-16 while closing issues/172 #1. A refusal,
not a wrong answer.

## Symptom

```python
class C:
    @staticmethod
    def s():
        yield 3
        yield 4
for v in C.s():
    print(v)        # CPython: 3 4.   pyc: error: unresolved call '__iter__'
```

With a yield EXPRESSION in it (`x = yield 3`), it is refused earlier with
"the value of this yield expression has nowhere to come from" (issues/172).

## Root cause

A generator is compiled as a coroutine body plus a WRAPPER that creates its
`__pyc_generator__` object and binds the body to it
(`build_generator_wrapper`, `python_ifa_build_if1.cc`). The wrapper is built
for plain functions (`gen_def_wrapper`) and for methods
(`gen_method_wrapper`). A `@staticmethod` gets neither, so `C.s()` returns the
raw coroutine handle, an `int`, and no return/send cell exists
(`gen_retcell`).

## Fix

Give a static method generator the plain-function wrapper. It has no `self`
to forward, which is exactly the plain-def case. Check `@classmethod`
generators at the same time.

## Verification

`tests/staticmethod_generator.py` (`.known_issue`) prints `3 4`, also with
`x = yield` and `send()`.

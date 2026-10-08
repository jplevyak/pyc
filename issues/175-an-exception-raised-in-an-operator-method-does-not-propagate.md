# 175 — an exception raised inside an operator method does not propagate

**Status:** open. Found 2026-10-07 while making bytes `%`-formatting raise
CPython's errors (minilight).

## Symptom

```python
import sys
d = len(sys.argv) - 1
try:
    print(7 // d)
except ZeroDivisionError:
    print("caught zdiv")

class V:
    def __add__(self, o):
        raise ValueError("no add")
try:
    print(V() + V())
except ValueError:
    print("caught add")
```

| | result |
| --- | --- |
| CPython | `caught zdiv`, `caught add` |
| pyc | the binary dies with SIGFPE on `7 // 0` |

Without the division, `V() + V()` is not caught either. The exception stays
pending, the operator's result is used, and the exception surfaces at the
next exception check, or as `Unhandled exception` at exit. Bytes formatting
behaves the same way: `b'%c' % 300` should raise `OverflowError`, but prints
`b''`, and the LLVM build can crash on that value.

**Subscripts too, and silently (2026-10-07).** `d[3]` on a dict without
that key, or on a class whose `__getitem__` raises, inside
`try: ... except KeyError:`, is not caught. Worse, the program then exits
**0 with no output at all**: neither handler runs, and the pending
exception is never reported. A subscript lowers through `call_method`
to `__getitem__`, the same operator-style send with no check after it.
`defaultdict.__getitem__`'s new `KeyError` (missing key, no factory) is
affected the same way.

## Root cause

The frontend emits a pending-exception check (`emit_exc_check`,
`python_ifa_build_if1.cc`) after a CALL, but not after a binary or unary
operator. An operator lowers to a method send (`PY_binop`,
`map_pyop_to_operator`) with nothing after it, so a raise inside the
operator method (`__add__`, `__mod__`, `__floordiv__`, ...) never reaches
the caller's handler. Separately, `int.__floordiv__` does not raise
`ZeroDivisionError` at all: C integer division by zero traps.

## Proposed fix

Emit `emit_exc_check` after the operator send. The Tier 2 fold
(`ifa/optimize/exc_check_fold.cc`, `mark_exc_checks_constant`) removes a
check when every resolved callee is proven unable to raise, so arithmetic on
ints and floats should keep no check. Measure that it does, because a check
left after every arithmetic operation would cost in hot loops. Separately,
integer `//` and `%` by zero must raise `ZeroDivisionError`.

## Verification

`tests/operator_raise_propagates.py` (a `.known_issue` today) flips to PASS.
Corpus: no stdout or run-status change, and no measurable run-time change on
the arithmetic-heavy programs (nbody, mandelbrot, chaos).

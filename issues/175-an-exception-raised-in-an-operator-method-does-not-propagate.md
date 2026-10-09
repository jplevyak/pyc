# 175 — an exception raised inside an operator method does not propagate

**Status:** open (slice subscripts fixed 2026-10-09; operators and item subscripts remain). Found 2026-10-07 while making bytes `%`-formatting raise
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

**Slice stores too (2026-10-09). FIXED the same day.** `ba[::2] =
bytearray(5)` raises `ValueError` in `bytearray.__pyc_setslice__`
(`__pyc__/06_bytearray.py`). Inside `try: ... except ValueError:` the
handler did not run, and in a program with no other `raise` the raise was
dropped entirely. Three gaps, each fixed:

1. **No check after a slice send.** `__pyc_getslice__`, `__pyc_delslice__`
   and, once the assignment attaches its value, `__pyc_setslice__` are now
   followed by `emit_exc_check`, carrying the send's AST so the post-FA
   fold can find the callee (`python_ifa_build_if1.cc`).
2. **Callers did not check.** `collect_can_raise` counts a slice subscript
   as an unresolved method send, like a method call, so a function
   containing one is `can_raise` and its call sites keep their check.
3. **The gate was not armed.** Two structural AST shapes now arm
   `pyc_program_has_raise` (`python_ifa_build_syms.cc`): an `except`
   clause in user code (the program observes exceptions), and a slice
   store (CPython raises `ValueError` for an extended-slice length
   mismatch on every mutable sequence, so the shape alone can raise, as
   with `assert`).

Tests: `slice_store_raise_is_caught` (the raise crosses a function
boundary into the caller's handler) and `bytearray_slice`'s mismatch case.
Uncaught, the program now stops with `Unhandled exception` and exit 1, as
CPython does.

**Item subscripts are NOT done, and the measurement says why.** The same
three changes applied to every `a[i]` load, store and `del` compiled
hq2x in 125 s against 27 s alone (old and new builds back to back), +72%
compile time corpus-wide. A check after every item access, and every
function with one counted `can_raise`, multiplies FA's work; the post-FA
fold removes the checks only after FA has paid for them. Item subscripts
and operators need a mechanism that does not emit the check up front, or
one that FA prunes before it analyzes the exceptional arm.

Also found on the way: once a subscript carried a check, minilight's
`float('inf')` folded to a constant and the C backend printed it as
`inf.0`. Non-finite floats are now emitted as `__builtin_inf()` /
`__builtin_nan("")` (`cg_sprint_imm`, `cg.cc`).

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

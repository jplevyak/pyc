# 175 — a raise inside an operator or item-subscript method does not reach the handler

**Status:** open on operators and item subscripts; slice subscripts fixed
2026-10-09. Rewritten 2026-11-03; the slice fix and the measurements are
in git (`git show 23cd789:issues/175-an-exception-raised-in-an-operator-method-does-not-propagate.md`).

## Symptom

```python
import sys
d = len(sys.argv) - 1
try: print(7 // d)
except ZeroDivisionError: print("caught zdiv")    # CPython: caught.   pyc: SIGFPE

class V:
    def __add__(self, o): raise ValueError("no add")
try: print(V() + V())
except ValueError: print("caught add")           # CPython: caught.   pyc: pending past the expression
```

The same for `d[k]` on a dict without the key, or a class whose
`__getitem__` raises, inside `except KeyError:`: the handler does not run
and the program exits 0 with NO output, because nothing reports the
pending exception. `b'%c' % 300` prints `b''` where CPython raises
`OverflowError`.

## Mechanism

The frontend emits a pending-exception check (`emit_exc_check`,
`python_ifa_build_if1.cc`) after a CALL, not after an operator or an item
subscript, which lower to a method send (`PY_binop`, `call_method` to
`__getitem__`) with nothing after it. Separately, integer `//` and `%` by
zero trap in C instead of raising `ZeroDivisionError`.

The slice fix shows the shape: a check after the send carrying the send's
AST (so `exc_check_fold` can find the callee), the function counted
`can_raise` so callers keep their check, and the `pyc_program_has_raise`
gate armed by the AST shape. **Applied to every item access it cost +72 %
compile time corpus-wide (hq2x 27 s to 125 s)**: FA analyses every
exceptional arm before the post-FA fold removes the check. Do not re-run
that measurement.

## Plan

1. **Operators.** Emit the check after an operator send and measure that
   the fold removes it for int and float arithmetic, whose callees cannot
   raise; the cost on nbody, mandelbrot and chaos must be zero. Make
   `int.__floordiv__` and `__mod__` raise `ZeroDivisionError`.
2. **Item subscripts need a check that FA does not pay for up front.**
   Either place the check only after FA resolves the callee to one that
   can raise (a post-FA insertion, the inverse of the fold), or give FA a
   check whose exceptional arm is not analysed until a resolved callee
   can raise. This is the exception-flow change
   [172](172-stopiteration-value-reads-the-global-exception-slot.md) and
   [ifa/165](../ifa/issues/165-none-reaching-an-operation-is-silently-accepted.md)
   also need; land it once.
3. **Then the two stores that wait on it:** `tuple.__setitem__` raising
   `TypeError` (today `t[i] = x` mutates; making it raise without an item
   store check killed everything after the store and the program printed
   nothing, so it was reverted), and the bounds check of
   [179](179-sequence-index-out-of-range-does-not-raise.md).

## Verification

`tests/operator_raise_propagates.py` (`.known_issue`) flips to PASS; a
fixture for the item-subscript `KeyError`; corpus: no stdout or run-status
change and no measurable run-time change on the arithmetic-heavy programs.

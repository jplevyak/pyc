# 211 — `round()`, and floor division near the int64 limit and in `//=`

**Status:** closed 2026-11-24 on landing; filed and fixed in one change.
Found while timing `re` (issues/210): `round(t, 3)` did not compile.

## Symptom

`round()` was `def round(x)` over libc `round`:

- `round(x, ndigits)` was a compile error ("illegal call argument type
  expression illegal: int64").
- It rounded half away from zero: `round(2.5)` was 3 and `round(-0.5)` -1,
  where CPython 3 gives 2 and 0. A silent wrong answer, described in the
  source as "a documented divergence" but never flagged as an accommodation
  (issues/171).
- `round(int)` went through float, so a large int lost precision; a class's
  `__round__` was never called.

Testing the int path found floor division wrong too, each a silent wrong
answer:

| expression | pyc | CPython |
| --- | --- | --- |
| `-9223372036854775807 // 10**18` | 8 | -10 |
| `x = -7; x //= 3` | -2 | -3 |
| `f = -7.0; f //= 2.0` | -3.5 | -4.0 |
| `-1.0 // -2.5` | -0.0 | 0.0 |

## Root causes

- `int.__floordiv__` computed `(self - self % x) / x`: exact, but the
  subtraction passes int64 when `self` is near a limit even though the
  quotient does not.
- `int.__ifloordiv__` and `float.__ifloordiv__` were the bare `/` prim, a C
  division: truncation for int, true division for float.
- `float.__floordiv__` used the same identity, which loses the sign of a
  zero quotient.

## Fix

- `round(number, ndigits=None)` in `__pyc__/05_builtins.py`:
  - float, no ndigits: `nearbyint` (half to even in the default rounding
    mode) to int; NaN raises ValueError and an infinity OverflowError, with
    CPython's messages.
  - float with ndigits: `_CG_round_ndigits` in the runtime, CPython's
    `double_round`. It takes the exact binary value rounded half-even to
    ndigits decimals (printf's `%f`/`%e`, the same correctly rounded
    conversion as CPython's dtoa) and parses it back, so `round(2.675, 2)`
    is 2.67 and `round(0.125, 2)` 0.12. Negative ndigits round through the
    exact expansion. NaN and infinities are returned unchanged, ndigits over
    323 returns x, and under -308 a signed zero, as CPython's limits do.
  - int: unchanged for ndigits >= 0, else half-even to a multiple of
    10**-ndigits.
  - Anything else: `number.__round__()` or `__round__(ndigits)`.
- `int.__floordiv__`: `_CG_int_floordiv` in the runtime truncates, then
  subtracts one when the signs differ and `q * b != a` (never larger than
  the operands). A float right operand goes to float floor division. The
  same steps written in Python made fysphun's flow analysis run forever,
  an analysis defect filed as [212](212-flow-analysis-never-finishes-a-pass-on-fysphun.md).
- `__ifloordiv__` on int and float calls `__floordiv__`.
- `float.__floordiv__`: `_CG_float_floordiv`, CPython's `float_floor_div`,
  signed zero included.

Division by zero is unchanged (IEEE for float, the C behaviour for int):
that is [175](175-an-exception-raised-in-an-operator-method-does-not-propagate.md).

## Verification

`tests/round_builtin.py` (16 lines: ties, ndigits positive, zero and
negative on floats and ints, -0.0, NaN and infinity, the limits, int64
edges, `__round__`) and `tests/floordiv_semantics.py` (`//` and `//=` on
every sign combination of int and float, the int64 edges, signed zeros,
divmod). Both are identical to CPython on the C and LLVM backends and under
`--strict`. `make test` green; corpus check sweep below.

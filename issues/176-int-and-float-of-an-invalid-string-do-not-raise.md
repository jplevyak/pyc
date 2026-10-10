# 176 — `int()` / `float()` of an invalid string return a number instead of raising

**Status:** open. Found 2026-10-09 while verifying the `try_stmt` parse fix
(its test needed a `ValueError` from `int("q")`; it uses an explicit
`raise` instead).

## Symptom

Silent: no warning, and a `try: ... except ValueError:` around the call
never runs its handler.

| expression | CPython | pyc |
| --- | --- | --- |
| `int("12")`, `int(" 7 ")`, `int("-4")` | 12, 7, -4 | same |
| `int("q")` | `ValueError: invalid literal for int() with base 10: 'q'` | `0` |
| `int("")` | `ValueError` | `0` |
| `int("3.5")` | `ValueError` | `3` |
| `int("1_000")` | `1000` | `1` |
| `float("1.5")`, `float("inf")` | 1.5, inf | same |
| `float("x")` | `ValueError: could not convert string to float: 'x'` | `0.0` |

The pattern is C's `strtol` / `strtod` semantics: parse the longest valid
prefix, return 0 when there is none, ignore the rest.

## Where

`str.__pyc_int_base__` (`__pyc__/01_str.py`), `bytes`'s
(`__pyc__/01b_bytes.py`), and the one-argument `int(x)` / `float(x)`
conversions, which reach `_CG_str_to_int64` / `_CG_str_to_float64`
(`pyc_c_runtime.h`).

## Proposed fix

Validate the whole string as CPython does (surrounding whitespace allowed;
`_` only between digits; for `int`, no `.` or exponent) and raise
`ValueError` with CPython's message otherwise. The raise is inside a
builtin reached through a call, so it propagates like `str.index`'s
(issues/038).

## Verification

The table above as a test, byte-identical to CPython on both backends,
including the `ValueError` messages. Corpus: `check` sweep with no stdout
change.

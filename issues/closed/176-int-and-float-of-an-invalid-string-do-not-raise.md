# 176 — `int()` / `float()` of an invalid string return a number instead of raising

> **CLOSED 2026-10-15.** `int(x)` / `float(x)` (and `map(int, ...)`) now
> dispatch on the argument, through the synthesized `__coerce__`
> (`python_ifa_build_syms.cc`): `x.__pyc_int__()` / `x.__pyc_float__()`.
> `str` and `bytes` validate the WHOLE string (`_CG_str_int_ok`,
> `_CG_str_float_ok` in `pyc_c_runtime.h`: whitespace, sign, a base prefix,
> `_` only between digits, inf/nan, no hex floats) and raise CPython's
> `ValueError`; `int(s, base)` too, and its builtin shortcut now carries the
> exception check it lacked. Numbers and bool define their own; any other
> object takes `__int__` / `__float__`. `tests/int_float_of_invalid_string.py`
> matches CPython on both backends. Corpus `check__default__6ac83790+4ec9875b`
> against `ea9a60bc+23598ae4`: every verdict and warning unchanged, compile
> time 2414 → 2411 s, +463 EntrySets / +981 CreationSets (a floor of +5
> CreationSets per program from the new library code).

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

# 183 — a `{None, int}` value in arithmetic compiles, and reads None as 0

**Status:** open. Found 2026-10-16 while closing issues/181. A silent wrong
answer in EVERY mode, `--strict` included, on both backends.

## Symptom

```python
import sys
x = None if len(sys.argv) > 5 else 1
print(x + 5)
```

| run | CPython | pyc (any mode) |
| --- | --- | --- |
| `./nl` | `6` | `6` |
| `./nl a b c d e f` | `TypeError: unsupported operand type(s) for +: 'NoneType' and 'int'` | `5` |

It compiles clean, with no warning, even under `--strict`. The same shape
through a field (`b.f = None` on one path, `b.f + 5`) behaves the same, and so
does a generator's sent value (issues/172): a generator resumed by `next()`
and also sent numbers prints `6` under `--strict` where CPython raises, with
only issues/172's note.

A METHOD call on the same value is refused, as
[048](048-none-int-field-pair-runtime-abort.md) describes:
`b.f.bit_count()` gives `expression has mixed basic types:(
__pyc_None_type__ int64 )`.

## Root cause (to confirm)

048's refusal is `collect_var_type_violations` (`fa.cc`). It raises BOXING
on the receiver of a live `P_prim_period` whose type holds None and a
scalar, so it sees `x.m()`. An operator is not a period send: `x + 5`
dispatches `__add__` directly on its operands, the None arm finds no method,
and an unresolved arm is dropped. At run time the `{None, int64}` value is
one int64 slot in which None is 0, so the int arm computes `0 + 5`.

## Fix

Report the operator case as the same demand: a `{None, scalar}` operand of
an operator dispatch is BOXING, exactly as a period receiver is (and with
the same truthiness exemption, which `not x` / `if x:` need). It then
backtracks to the merged CreationSet like the period case, and is refused
when it is genuine. Then confirm:

- issues/172's `next(g); g.send(5)` idiom is refused under `--strict` (its
  permissive record keeps the None out of the cell, so permissive is
  unaffected);
- the strict suite comparison (171's method) and the strict corpus sweep
  show no new silent deviation and only expected new refusals.

**The tension to settle first.** Refusing every `{None, int}` operand
refuses programs that are correct in CPython, because their None never
reaches the operator at run time. `tests/none_or_int_arithmetic.py` is the
corpus case: pisang's `unfixed` returns an int from inside a loop or falls
off the end (None), and the result is used in `abs(lit)` and `lit > 0`. It
compiles and matches CPython today, so a blanket refusal would lose pisang.
The choices are:

- refuse, and edit pisang under PYC_CHANGES.md's policy, if its None
  truly cannot arrive;
- make None a distinguishable value in a `{None, int}` slot (048's
  sentinel, an accommodation) and raise `TypeError` when the operator meets
  it;
- split the confluence when the None and the int come from separable
  creation points (the AGENTS.md method). That is the right answer when
  pyc invented the union rather than the program.

Whichever it is, it must keep `none_or_int_arithmetic` passing or say
why it no longer can.

## Verification

`tests/none_or_int_operator_raises.py` (`.known_issue`; it holds CPython's
output for no arguments, where `y` is None and `y + 5` raises) and a
`.check_fail` refusal test under `--strict`.

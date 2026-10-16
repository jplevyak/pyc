# 183 — a `{None, int}` value in arithmetic compiles, and reads None as 0

**Status:** CLOSED 2026-10-16. Found the same day while closing issues/181. It
was a silent wrong answer in EVERY mode, `--strict` included, on both backends.

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
does a generator's sent value ([172](../172-generator-send-channel-and-stopiteration-value.md)): a generator resumed by `next()`
and also sent numbers prints `6` under `--strict` where CPython raises, with
only issues/172's note.

A METHOD call on the same value is refused, as
[048](../048-none-int-field-pair-runtime-abort.md) describes:
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

## Resolution (2026-10-16)

**The check.** `collect_var_type_violations` (`ifa/analysis/fa.cc`) now
treats an OPERATOR like a period send. That covers a send with no primitive
whose `rvals[0]` is the selector symbol (`__add__`, `__eq__`, `__nis__`, …)
and whose operands follow. Every operand that holds None and a scalar is
reported as BOXING, with the period case's truthiness exemption. That makes
it the same DEMAND as the method case, so a union pyc invented is split
before anything is refused, and only a genuine one is refused.

- **Invented, split:** two `Box` objects from one creation point (`mk(v)`),
  one holding None and one an int. `b.v + 5` compiles under `--strict` to two
  `Box` layouts, `v: int64` and `v: None`, and prints CPython's answer.
- **Genuine, refused:** `x = None if c else 1; x + 5`, in every mode, with
  `expression has mixed basic types:( __pyc_None_type__ int64 )`.
  `tests/none_or_int_operator_raises.py` is now a `.check_fail` pinning that.
  `is None`/`is not None` on such a value is refused too: it is a `__nis__`
  dispatch, and an int slot with None stored as 0 cannot answer it either.
- **issues/172**: the `next(g); g.send(5)` idiom is now refused under
  `--strict`, as its resolution claimed. Permissive keeps its flagged
  record, which keeps the None out of the cell.

**Corpus.** Sweep `check__default__cfce7227+7a628085`, with the check and no edit: every program
identical in every column, contour counts included, except **pisang**, now
refused. Its `unfixed` falls off the end after a loop that always returns
(it is called only when `length(clause) == 1`). The None is unreachable and
genuine at the type level, so it is a corpus edit under PYC_CHANGES.md: a
`raise ValueError(...)` after the loop, as `genetic2`'s `execute` got.
CPython's output is identical before and after apart from its `TIME` line.
`tests/none_or_int_arithmetic.py`, the pisang shape, is edited the same way.

Sweep `check__default__cfce7227+5071387a`, with the edit: only pisang changed: it compiles and runs again (compile_rc 1→0, run_rc 0), and its ess/css fell to 352/1083 from the pre-fix 375/1096. Every other program is identical in every column, to the fix-only sweep and to the pre-fix `check__default__82ca40fa+813b1a1e`. CPython times out on pisang in the sweep (rc=124), so its stdout was compared by hand: it matches CPython on C, LLVM and `--strict`.

`make test`: 477 passed, 17 known, 0 failed on both backends.

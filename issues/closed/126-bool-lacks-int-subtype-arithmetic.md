# 126 — `bool` had no int-subtype arithmetic, and fixing it exposed an LLVM sign-extension bug

**Status: CLOSED 2026-09-12**, fixed by `b0ee5bb9`. The titled defect —
`bool` having no int-subtype arithmetic — is fixed and verified on both
backends, with `tests/bool_int_subtype_arith.py` in the suite and no
`.known_issue` sidecar. Its leftovers were re-filed rather than left
dangling here: see "Not fixed here" at the bottom.

Root cause #3 in the `unresolved call` census
([ifa/149](../../ifa/issues/149-the-largest-diagnostic-class-reports-nothing.md)),
after [ifa/150](../../ifa/issues/150-is-not-none-never-folds.md) and
[125](../125-in-has-no-iterable-fallback.md).

## Symptom

`bool` is an int subtype in Python for arithmetic as well as ordering, and
`00_runtime.py` already carried `__lt__`/`__le__`/`__gt__`/`__ge__` for the
ordering half — with a comment warning that such a gap "cascaded into
unrelated NOTYPE collapses". The arithmetic half was missing, so each of
these dispatched to nothing:

| corpus | expression |
| --- | --- |
| timsort | `return (a > b) - (a < b)` — the standard three-way compare |
| quameon | `return not(x^y)` |
| softrender | `if currentInside ^ previousInside:` |

Minimal repros, all `unresolved call` before and all matching CPython after:

```python
(a > b) ^ (a < b)     # True     __xor__
(a > b) - (a < b)     # 1        __sub__
(a > b) + (a < b)     # 1        __add__
(a > b) & (a < b)     # False    __and__ -- already worked
```

## The implementation shape is the whole decision

The comparisons above `__lt__` are written as a branch on `self` returning
bool literals, because — as their comment records — the numeric primitives
**reject a bool operand** ("illegal primitive argument type ... bool") and
`int()` **miscompiles on the LLVM backend** (`int(True)` yields -1).

Extending that shape to arithmetic is the obvious move and it is wrong:

```python
def __add__(self, x):        # WRONG
  if self:
    if x: return 2
    return 1
  ...
```

Correct for a bool `x`, and **silently wrong for an int one** — `True + 5`
returns 2 instead of 6. An unresolved call is a loud failure; a wrong answer
that compiles clean is the one outcome this project treats as worse than
not compiling at all.

What works instead is to reduce `self` to the int literal `0` or `1` and let
the **numeric** operator run:

```python
def __add__(self, x):
  if self:
    return 1 + x
  return 0 + x
```

The literal is the RECEIVER, so this is `int + x`, never `bool + x` — which
sidesteps both recorded hazards at once: no numeric primitive ever receives
a bool operand, and `int()` is never called. Measured correct on both
backends for a bool and an int `x` alike.

`__xor__` is the one that needs a type split, because CPython's return TYPE
differs: `bool ^ bool` is a **bool** (`True ^ False` is `True`, not `1`)
while `bool ^ int` is an int (`True ^ 3` is 2). `isinstance(x, bool)` folds
to a single constant per contour, so only one arm survives in each caller.

Added: `__add__`, `__sub__`, `__mul__`, `__xor__`. Deliberately NOT added —
`__truediv__`, `__floordiv__`, `__mod__`, `__pow__`, the shifts, and the
reflected/in-place forms: no corpus program and no test needs them, and
each would be an untested guess at a shape that may not be reachable.

`tests/bool_int_subtype_arith.py` pins 25 cases, byte-identical to CPython,
with an empty compile golden so it also asserts zero warnings. It covers
the mixed bool-with-int cases specifically, because those are what the
rejected shape would have gotten wrong.

## It exposed a real LLVM codegen bug

The new test passed on the C backend and FAILED on LLVM, and every wrong
value fitted one hypothesis exactly — `True` is **-1** as an integer
operand:

```
t * t  ->  1 * (-1) = -1      (want 1)
t + t  ->  1 + (-1) =  0      (want 2)
t - t  ->  1 - (-1) =  2      (want 0)
f - t  ->  0 - (-1) =  1      (want -1)
```

Six lines reproduce it:

```python
def gt(a, b): return a > b
def add2(p, q): return p + q
print(add2(gt(3, 2), gt(3, 2)))   # C and CPython: 2.  LLVM: 0
print(add2(gt(2, 3), gt(3, 2)))   # C and CPython: 1.  LLVM: -1
```

`emit_convert` in `cg_emit_llvm.cc` already documented the rule — widening
from i1 must ZERO-extend, since i1 is only ever bool and sign-extending
True gives all-ones — and applied it at its own site. But **six other
places adjust integer width** with a bare `CreateSExt`/`CreateSExtOrTrunc`
and had no i1 guard: local-slot store, global store, widening load, struct
field store, numeric-conversion result, and the binary-op operand promotion
that this repro hits.

Fixed by routing all six through one `int_width_cast` helper, so they
cannot drift apart again.

**Why it lay hidden:** with a constant receiver the `if self:` branch folds
and no runtime bool is ever widened, which is why every small repro passed.
It needed a bool that is genuinely both True and False at runtime, which
only became reachable once `bool` had arithmetic at all.

## Measured

Six gates green, **317/0 on both backends**.

Corpus `compile__default__2c1a8e8f+71bec4dd` -> `4509936e+12802374`, no
program worse:

```
quameon     55 -> 0      timsort  24 -> 0      softrender  50 -> 13
warning lines  1356 -> 1240      unresolved call  145 -> 124
programs warning  38 -> 36       every __xor__ gone
```

The `check` sweep is the stronger result: both programs were in `run-fail`,
compiling with warnings and then ABORTING on the unresolved dispatch.
Joined per-program against `check__default__2c1a8e8f+e7b8451f`, exactly two
rows change and no others -- quameon and timsort both go `run 134 -> 0`,
`run_fail` 36 -> 34.

**quameon's output is not verifiable against CPython, and this issue does
not claim it is correct.** It is a seeded Monte Carlo (`random.seed(n)`,
`random.random()`, `random.uniform`), so pyc's PRNG stream necessarily
differs from CPython's Mersenne Twister and the sample paths diverge; the
numbers cannot be compared. pyc's RNG itself is well-formed -- 2000 draws,
none outside [0, 1), mean 0.50 against CPython's 0.49, `uniform(-1, 1)` in
range. What is worth a separate look is that quameon's reported energy for
He comes out POSITIVE (+1.6) where CPython's run gives -2.4, and a bound
atom's energy should be negative; that is unexplained, but it is not
attributable to this change, since the program aborted before producing any
output at all until now. timsort's CPython side times out (`cpy 124`), so
it has nothing to compare against either.

## Not fixed here — where each leftover went

- **`bool & int` / `bool | int` give the wrong answer** →
  [127](127-bool-bitwise-ops-wrong-for-int-operands.md). `__and__` and
  `__or__` were never missing; they are present and wrong for a non-bool
  operand (`True | 4` returns `True`, CPython gives `5`), with zero
  warnings. That is a different defect from "bool lacks arithmetic" and is
  not covered by this issue's title or its fix, so it is filed on its own.
  Verified table there. The fix is the `isinstance` split this issue added
  for `__xor__`.
- **`tarsalzp`'s `unresolved call '__and__'`** on
  `if (b > 0) & (self.divisor(a, b) > 1200) else (a + 1)` — **not this
  issue.** `bool.__and__` exists and resolves fine in isolation
  (`(a > b) & (a < b)` compiles clean), so something else about that
  ternary's receiver is responsible. Unexamined; it is one of the residual
  sites in [ifa/149](../../ifa/issues/149-the-largest-diagnostic-class-reports-nothing.md)'s
  census.
- **`rdb`'s `bool.__iand__`** with a class-instance argument
  (`basis &= MatchRule(props, rule)`) — a `TypeError` in CPython unless
  `MatchRule` defines `__rand__`, so pyc may be right to refuse it. `rdb` is
  also one of the two corpus programs that do not compile at all, so it
  needs its own look before anything is added for it.

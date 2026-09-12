# 127 — `bool & int` and `bool | int` compile clean and give the wrong answer

**Status: CLOSED 2026-09-12**, fixed the same day it was filed. It was a
**silent wrong answer** — no diagnostic, exit 0, wrong value — which this
project treats as the worst available outcome.

The fix is the `isinstance` split described below, applied to both
`__and__` and `__or__`. All six cases in the table now match CPython, the
mixed cases are covered by `tests/bool_int_subtype_arith.py` (including an
int operand that is NOT a constant, so the splitter cannot fold it), six
gates are green at **318/0 on both backends**, and a corpus `check` sweep
joined per program shows **not one of the 77 programs changed** `compile_rc`,
`run_rc`, `cpy_rc` or `stdout_match`. That last result is the one the issue
predicted: nothing in the corpus was relying on the wrong answer.

Split out of [126](126-bool-lacks-int-subtype-arithmetic.md) 2026-09-12,
which closed by adding bool's missing arithmetic. `__and__` and `__or__` were
NOT missing — they are present and wrong — so they are a separate defect and
were not in 126's title or its fix.

## Verified

```python
def f(a, b):
    t = (a > b)          # True
    u = (a < b)          # False
    print(t & 3); print(u & 3); print(t | 4); print(u | 4)
    print(t & u); print(t | u)
f(3, 2)
```

Zero warnings, exit 0:

| expression | pyc | CPython |
| --- | --- | --- |
| `True & 3` | **3** | **1** |
| `False & 3` | **False** | **0** |
| `True \| 4` | **True** | **5** |
| `False \| 4` | 4 | 4 |
| `True & False` | False | False |
| `True \| False` | True | True |

bool-with-bool is correct. **Three of the four mixed cases are wrong**, one
of them (`True | 4` → `True` instead of `5`) a wrong value rather than
merely a wrong type.

## Cause

`__pyc__/00_runtime.py`:

```python
class bool:
  def __and__(self, x):
    if (self):
      return x          # True & 3  -> 3.  CPython: 1
    else:
      return self       # False & 3 -> False.  CPython: 0
  def __or__(self, x):
    if (self):
      return self       # True | 4  -> True.  CPython: 5
    else:
      return x
```

Correct for a bool `x` — `True & False` is `False`, `False | True` is `True`
— and wrong for anything else, because bool is an **int subtype** in Python:
`bool op int` follows int semantics on the operands' integer values.

## Fix

The same shape `bool.__xor__` already uses (added in 126), which is correct
for both and is measured on both backends:

```python
def __and__(self, x):
    if isinstance(x, bool):
        if self:
            return x
        return self
    if self:
        return 1 & x
    return 0 & x
```

`isinstance(x, bool)` folds to a single constant per contour, so only one arm
survives in each caller. In the non-bool arm the int literal is the
RECEIVER, so it is `int & x`, never `bool & x` — which is what keeps a bool
operand away from the numeric primitives that reject it, and avoids `int()`
(`int(True)` yields -1 on the LLVM backend). See 126 for why the shorter
branch-on-self-returning-literals form is not usable here: it is exactly the
form that produced this bug.

`__or__` the same. Consider `__rand__`/`__ror__` and the in-place forms only
if something needs them — 126 deliberately added no untested shapes, and
`bool.__iand__` is separately dubious (rdb wants it with a class-instance
argument, which is a `TypeError` in CPython).

## Verification

- The table above, byte-identical to CPython, as a fixture next to
  `tests/bool_int_subtype_arith.py` — which already covers the bool-bool and
  `__xor__` mixed cases and must keep passing.
- Six gates on **both** backends: the LLVM `int_width_cast` fix landed in 126
  is what makes bool arithmetic correct there at all, so this is exactly the
  path that regressed once already.
- Corpus `check` compared per program. No corpus program is known to depend
  on `bool & int`, so the expected result is no movement; if something does
  move, it was relying on the wrong answer.

## What this unblocks

Nothing is blocked on it, which is why it is filed rather than fixed in
126's commit. It matters because of its failure MODE: `tarsalzp` computes
`(b > 0) & (self.divisor(a, b) > 1200)` and `rdb` does `basis &= ...`, so
`bool`'s bitwise ops are on live corpus paths, and a wrong answer there is
invisible to every gate that does not diff output against CPython.

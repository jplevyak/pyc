# 212 — flow analysis never finishes its first pass: fysphun with a Python-level `int.__floordiv__`

**Status:** open. Found 2026-11-24 while fixing floor division
([211](211-round-and-floor-division.md)), and avoided there; the analysis
defect is unfixed.

## Symptom

`shedskin_examples/fysphun` compiles in 9 s. With `int.__floordiv__` written
as Python-level arithmetic, its flow analysis never completes pass 1 (no
`IFA_DBG_ESPERFUN` line is ever printed; the sweep's 400 s cap, and 90 s
alone, both expire). A non-terminating analysis is a defect whatever the
library code is: the same body compiles everywhere else in the suite.
Sweep `check__default__bbce7f9f+e1bf1974` is the measurement (fysphun the
only change, `compile_fail`); `bbce7f9f+b11efc73`, with the workaround, has
every verdict of the sweep before the change.

## Reproduce

Replace `int.__floordiv__` in `__pyc__/02_numeric.py` with this (correct,
CPython-equivalent) body, then `PYC_DBG_PHASES=1 ./pyc
shedskin_examples/fysphun/fysphun.py`; it stops after `if1-finalize`:

```python
  def __floordiv__(self, x):
    if isinstance(x, float):
      return float(self).__floordiv__(x)
    q = __pyc_operator__(self, __pyc_symbol__("/"), x)
    if (self < 0) != (x < 0) and q * x != self:
      q = q - 1
    return q
```

Either half alone (without the `isinstance` branch, or with the old
`(self - self % x) / x` body after it) analyses in 0.87 s. Reduced in the
program (each step measured with a 25 s cap):

- removing `halfspoke = numspokes // 2` in `Wheel.__init__` cures it;
- so does removing the `__main__` block, or only its `l.applyme()`;
- so does removing `l.dist = dist` in `addlinkrr`, which is what makes
  `Link.dist` hold `{int, float}` (`addlinki` stores an int, `addlinkrr` a
  float from `hypot`), the union the permissive numeric coercion widens.

So the ingredients are a field the coercion widens, a `//` on an int, and
an `int.__floordiv__` whose body makes further operator calls (`q * x`,
`q - 1`, comparisons) on a value whose type depends on `x`. Small programs
with each ingredient alone (a defaulted int `// 2`, a widened local used in
`7 // x`, the `Wheel` shape on its own) do not hang; the reduction to a
standalone file has not been done.

## Next step

Reduce fysphun further with the body above in place until a file of a few
lines hangs, then find which worklist cycles inside pass 1
(`IFA_DBG_FATRACE`). Suspect: the coercion's widening feeding a type back
into `x` of a contour of `__floordiv__` whose result feeds the operand
again, without the monotone growth a pass needs to terminate. Per
AGENTS.md and ifa/055/057, the fix is in the analysis (monotonicity /
productivity), not a widening limit.

## Workaround in place

[211](211-round-and-floor-division.md) does int floor division in one
runtime call (`_CG_int_floordiv`), which has a fixed int result and makes no
further calls. That is also the faster form, and it is semantically
identical, so it stays when this is fixed; this issue is about the
analysis.

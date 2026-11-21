# 199 — `%r` in a computed format printed `str()`

**Status:** closed 2026-11-19, the day it was filed. Found porting `logging`
([197](../197-logging.md)): every logging call formats `record.msg %
record.args` with a format the compiler cannot see, so `log.error("bad:
%r", "x")` printed `bad: x` where CPython prints `bad: 'x'`. A silent wrong
answer.

## Symptom

```python
def fmt(k):
    return "%r" if k == 0 else "[%s|%r]"
class P:
    def __str__(self): return "P-str"
    def __repr__(self): return "P-repr"
print(fmt(0) % "x")          # CPython 'x'      pyc x
print(fmt(0) % P())          # CPython P-repr   pyc P-str
print(fmt(1) % ("s", "t"))   # CPython [s|'t']  pyc [s|t]
```

Three more forms were wrong in ways of their own:

| form | pyc |
| --- | --- |
| a format that is constant only after analysis (`fmt` returns one literal) | the literal `%r`, because `vsnprintf` has no `%r` |
| `%a` of a string through the tagged path | a hex float: the runtime took `a` for C's `%a` |
| `%a` anywhere | `str()`, as `%r` |

## Cause

[168](168-object-at-a-computed-format-s-prints-garbage.md) made a computed
format convert its arguments before the format is applied:
`str.__mod__` sends `__pyc_fmtconv__`, and each argument's `__pyc_fmtval__`
returns `__str__()`, or itself for a number or a str. It converts without
knowing which conversion each argument meets, so `%r` got `str()`.

## Fix

The conversions are read from the run-time format:

- `_CG_format_convs(fmt)` returns one character per argument, skipping `%%`
  and a `%(key)`.
- `str.__mod__` passes that string to `__pyc_fmtconv__(convs)`, which gives
  each argument its own character: `__pyc_fmtval__(c)`. The tuple methods
  are generated per arity with constant slices (`python_ifa_main.cc`).
- An object or a str meeting `%r` converts through `__repr__`, and one
  meeting `%a` through `__pyc_fmt_ascii__(repr)`: the repr when it is ASCII,
  which is then `ascii()`'s answer, and otherwise `NotImplementedError`
  (pyc has no `ascii()`). Numbers stay raw for the type-tag path, which
  already renders a number at `%r`.
- `_CG_widen_int_convs`, used by both constant paths, rewrites `%r` and `%a`
  to `%s`: an argument there has already been converted. The tagged
  runtime no longer reads `a`/`A` as C's hex float.

## Tests

`tests/format_nonconstant_repr.py`: `%r`, `%a` and `%s` through formats
chosen at run time, of strs, objects whose `__repr__` and `__str__` differ,
ints, floats, bools, None and lists, as one value and in tuples, with widths.
Matches CPython on both backends. A list-layout tuple as the right operand
crashes before and after this fix:
[201](../201-list-layout-tuple-at-a-computed-format-segfaults.md).

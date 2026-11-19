# 200 — `"%d" % "x"` prints a pointer

**Status:** open. Filed 2026-11-19, found probing what `logging`
([197](197-logging.md)) would need. A silent wrong answer.

## Symptom

```python
try:
    print("%d" % "x")
except Exception as e:
    print("caught", str(e))
# CPython: caught %d format: a real number is required, not str
# pyc:     131777393778136
```

The number is the string's address, and it changes from run to run.

## Cause

Neither formatting path checks that a numeric conversion meets a number:

- **Constant format.** The frontend converts arguments for `%s` and `%r`
  only. A str at `%d` reaches `_CG_format_string`, whose `vsnprintf` reads
  the pointer as an int64 (`_CG_widen_int_convs` has made it `%lld`).
- **Computed format.** `_CG_format_string_tagged` has the argument's tag,
  `'s'`, and reads `va_arg(ap, int64)` for `%d` regardless.

The same holds for every integer and float conversion (`%x`, `%o`, `%f`,
`%e`, `%g`, …) and for `%c` of a str longer than one character.

## Fix (proposed)

CPython raises `TypeError` (`%d format: a real number is required, not
str`; `%x format: an integer is required, not str` for the integer-only
conversions; `%c requires int or char`).

- Constant format: the conversion and the argument's type are both known
  after analysis, in `format_string_codegen`. Emit the raise there, through
  the runtime's exception path, rather than the call.
- Computed format: the tagged runtime knows the tag meets `d`. It needs a
  way to raise a Python `TypeError` from a C helper. Check whether one
  exists (issues/175 has the operator-raise machinery) before adding one.

## Tests to add

`tests/format_type_mismatch.py`: each conversion family against a str,
through a constant and a computed format, each caught and printed. Compare
with CPython on both backends.

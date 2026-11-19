# 201 — a list-layout tuple at a computed `%` segfaults

**Status:** open. Filed 2026-11-19, found writing the test for
[199](closed/199-percent-r-in-a-computed-format.md). Present before 199's
change (checked against the pushed HEAD `ce537989`). A crash at run time.

## Symptom

```python
def fmt(k):
    return "%s" if k == 0 else "[%s|%s]"
n = len("abcd")
xs = tuple(["u", "v"])
print(fmt(n - 3) % xs)     # CPython [u|v]   pyc: Segmentation fault
```

`tuple(list)` and a tuple slice have a run-time arity, so the tuple has
LIST layout (ifa/109): one element type, no fixed fields.

## Cause

`format_string_codegen` (`python_ifa_main.cc`) spreads a tuple operand into
the vararg list only when it is a fixed-arity record
(`is_tuple = Type_RECORD && !cg_has_classtag`). A list-layout tuple is not
one, so it is passed as a single pointer argument, and the format's
conversions then read past the varargs.

Its `__pyc_fmtconv__` (the run-time loop variant) already builds the
converted elements back into a list-layout tuple, so the values are right;
only the call is wrong.

## Fix (proposed)

A runtime entry that takes the sequence instead of varargs:
`_CG_format_string_seq(fmt, tag, seq)`, which reads `len(seq)` elements of
the one element type and formats them as `_CG_format_string_tagged` does,
raising CPython's "not enough / not all arguments converted" for a length
mismatch (as `tuple.__pyc_fmtargs__` does, issues/174). Both emitters need
it: `format_string_codegen` and the LLVM lowering in `cg_emit_llvm.cc`.

## Tests to add

Extend `tests/format_nonconstant_repr.py` with `tuple(list)` and a tuple
slice as the operand, of strs, ints and floats.

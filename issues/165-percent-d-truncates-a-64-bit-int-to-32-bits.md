# 165 — `"%d" % n` silently truncated a 64-bit int to 32 bits

**Status: FIXED** 2026-09-18 (`pyc_c_runtime.h` `_CG_widen_int_convs`,
`python_ifa_main.cc` `format_string_emit_cast`, `ifa/codegen/cg_emit_llvm.cc`).

**Related:** [164](164-time-time-has-whole-second-resolution.md) (found in
the same measurement), [040](closed/040-percent-format-float-arg-int-specifier-garbage.md)
(the float/int half of this, fixed earlier the same way).

## Symptom

```python
n = 199999990000000
print(n)          # 199999990000000
print("%d" % n)   # 542894464        <-- wrong, silently
print("%s" % n)   # 199999990000000
```

`542894464` is exactly `199999990000000 mod 2**32`. Every `%d` / `%i` /
`%o` / `%u` / `%x` / `%X` path was affected; `print()` and `%s` were
correct, which is what let it go unnoticed.

Found while benchmarking a loop whose sum was printed with `%d` — the
elapsed time was wrong ([164](164-time-time-has-whole-second-resolution.md))
*and* so was the sum.

## Root cause

Python's `%d` is C's `int` conversion — 32 bits. `format_string_codegen`
widens the *argument* to `int64` but passed the Python format string
**verbatim** to `vsnprintf`, so the conversion read 32 bits of a 64-bit
vararg. This is the same class as [040](closed/040-percent-format-float-arg-int-specifier-garbage.md),
which fixed the float/int *register class* mismatch and left the integer
*width* mismatch in place.

## Fix

Two halves, because the format and the argument must agree:

1. **`_CG_widen_int_convs`** (`pyc_c_runtime.h`) rewrites the format,
   inserting the `ll` length modifier into every `d i o u x X`
   conversion, copying flags/width/precision through and dropping any
   length modifier already present (so a hand-written `%ld` does not
   become `%lldd`). It returns the original pointer when there is nothing
   to rewrite, so the common case allocates nothing. Done in the runtime
   rather than in the two emitters because it is the one place both
   backends meet, and because it also covers a format string that is not
   a compile-time constant — which neither emitter can parse.

2. **Both emitters** now guarantee the argument width. Previously only a
   *float* reaching an integer conversion was cast; an integer was passed
   at its own width, and `_CG_bool` is `uint8` while `_CG_int` is 32-bit.
   `%c` is the exception — C's `%c` consumes an `int` and the rewrite
   leaves it alone — so that one narrows to `int`/i32 instead.

`va_end` was also missing from `_CG_format_string`'s retry loop, which is
UB; added.

## Measured

14 cases byte-identical to CPython on **both** backends: plain `%d`,
embedded, `%s`, two conversions, mixed with `%.2f`, width/flag forms
(`%5d`, `%-8d`, `%08d`), `%x`/`%X`/`%o`, `%c`, a bool through `%d`,
negatives, `%i`, `%%`, and a `%f` of 1e300 beside a `%d`.

Six CI gates green, suite 315/0/26 both backends.

## Still open

A format string that is **not** a compile-time constant gets no argument
casts at all (the emitters cannot match conversions to arguments without
parsing it), so a float reaching `%d` there is still the issues/040
register-class bug. The `ll` rewrite is correct for it because pyc's
Python `int` is already int64; only the float case remains.

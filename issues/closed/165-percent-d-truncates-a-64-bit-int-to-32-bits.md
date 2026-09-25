# 165 — `"%d" % n` silently truncated a 64-bit int to 32 bits

**Status: CLOSED** 2026-09-25. The constant-format half landed 2026-09-18
(`pyc_c_runtime.h` `_CG_widen_int_convs`, `python_ifa_main.cc`
`format_string_emit_cast`, `ifa/codegen/cg_emit_llvm.cc`); the
non-constant-format half landed 2026-09-25 (`_CG_format_string_tagged`,
both emitters). Regression tests `tests/format_string_width.py` and
`tests/format_nonconstant_string.py`.

**Related:** [164](../closed/164-time-time-has-whole-second-resolution.md) (found in
the same measurement), [040](../closed/040-percent-format-float-arg-int-specifier-garbage.md)
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
elapsed time was wrong ([164](../closed/164-time-time-has-whole-second-resolution.md))
*and* so was the sum.

## Root cause

Python's `%d` is C's `int` conversion — 32 bits. `format_string_codegen`
widens the *argument* to `int64` but passed the Python format string
**verbatim** to `vsnprintf`, so the conversion read 32 bits of a 64-bit
vararg. This is the same class as [040](../closed/040-percent-format-float-arg-int-specifier-garbage.md),
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

## Fixed 2026-09-25 — the non-constant format string

A format string that is not a compile-time constant got **no argument
casts at all**, because `format_string_codegen` derives each cast from the
conversion the argument will meet and there are no conversions to read
without parsing the format. With `conv == 0` an integer was still widened
to `int64`, which is why `"%d" % 7` worked — but a float was left alone,
and that double then met `%d`, which on x86-64 SysV reads an integer
register while the value sits in an xmm one.

**It was worse than "the issues/040 register-class bug", which is how this
section originally described it.** Measured: `"%d" % 3.7` through a
computed format printed `25637`, and `"[%d]" % 2.9` printed
`[1566844251]` — garbage that changes between runs, not a truncation.

**Neither end can repair it alone**, which is what made this the half left
over: the emitter cannot see the format, and the runtime cannot see the
argument types. So the emitter now passes the TYPES — one tag character
per argument, `i`/`f`/`s` — and `_CG_format_string_tagged`
(`pyc_c_runtime.h`) pairs them with the conversions it finds while walking
the format, which it was already doing for `_CG_widen_int_convs`.

It formats **one conversion at a time** rather than handing the whole
format to `vsnprintf`, and that is forced rather than chosen: the mismatch
cannot be repaired by rewriting the format, because CPython's
`"%d" % 3.7` is `3` — a truncation toward zero — and no printf conversion
truncates a double. The value has to be read as a double and converted,
which is exactly what the constant path's `(int64)` cast does.

The **constant path is untouched** and still goes through
`_CG_format_string`, so the case that was already correct carries no risk
from this change.

### Measured

`tests/format_nonconstant_string.py`, 15 cases byte-identical to CPython
on **both** backends, every one through a format the compiler cannot fold:
a float at `%d`/`%x`/`%o` (positive and negative), an int at `%f`, the
matching cases, `%s`, width/flag/precision forms (`%08.3f`, `%5d|%-5d`),
`%c` (which NARROWS to `int` where the others widen), a 64-bit value, a
bool, and a mixed tuple through one computed format.

Six CI gates green: 58/0 unit, 16 IR phases 0 failed with the 2 known,
319 passed / 0 failed on both backends (318 before, +1 for the new test).

### Not covered, and why it is not this issue

A `%s` whose argument is an OBJECT still relies on the frontend
pre-converting it through `__str__`, and that pre-conversion also needs a
constant format to know which arguments meet `%s`. The tag mechanism
cannot close that one: converting an object to its `str` means calling
back into generated code, which a runtime helper cannot do. That is a
different defect from this one — a missing conversion, not a wrong
register class — and it wants its own issue if it ever bites.

# 040 — `"%d" % <float>` produces garbage output instead of Python's truncate-to-int (both backends)

**Status:** fixed 2026-08-08. Found 2026-08-07 while diagnosing
[issues/025](../025-shedskin-examples-coverage.md)'s `yopyra` entry
(TODO list item 10 — "rendered pixel values look wrong," left
unconfirmed since the reference `python3` run hadn't been compared
side-by-side).

**Affects:** `pyc_c_runtime.h`'s `_CG_format_string` (both backends
call this at runtime); `python_ifa_main.cc`'s `format_string_codegen`
(C backend emission) and `ifa/codegen/cg_emit_llvm.cc`'s
`__pyc_format_string__` handler (LLVM backend emission) — neither does
any per-specifier type coercion before the call.

## Confirmed real (not just a slow-comparison artifact)

The original `yopyra` finding was never confirmed because CPython's
own reference run didn't finish in the 30s used at the time. Re-ran
today with a scaled-down scene (`size 20 20`, `oversampling 1`,
`renderslice 8 10` — a few seconds instead of minutes) so a real
side-by-side comparison was possible:

- **CPython:** valid, varied RGB triples, e.g. `25 25 25 43 38 38 30
  30 30 ... 219 138 138 ...`.
- **pyc (C backend):** every single pixel, every line, identical:
  `622879781 8 622879781`.

`622879781` is nowhere near the valid 0-255 channel range — this is
the exact garbage the original spot-check noticed, now confirmed
against a real reference, not a hunch.

## Root cause

`color.__str__` (`yopyra.py`) does:
```python
def __str__(self):
    return "%d %d %d" % (max(0.0, min(self.r*255.0, 255.0)),
                         max(0.0, min(self.g*255.0, 255.0)),
                         max(0.0, min(self.b*255.0, 255.0)))
```
Python's `%d` **accepts a float and truncates it** — `"%d" % 3.7` is
valid Python, prints `"3"`. C's `printf`-family `%d`, by contrast,
requires an actual `int` argument; passing a `double` where `%d`
expects an `int` is undefined behavior in C (on the x86-64 SysV ABI
specifically: float/double varargs are passed via `XMM` registers,
while `%d`'s internal `va_arg(ap, int)` reads from the
general-purpose-register/stack save area — completely the wrong
memory, hence garbage, not the actual float value misread).

`_CG_format_string` (`pyc_c_runtime.h:456`) is a thin wrapper that
forwards the Python format string and raw C varargs **directly** to
`vsnprintf`:
```c
inline char *_CG_format_string(char *str, ...) {
  ...
  int ll = vsnprintf(s, l, str, ap);
  ...
}
```
Neither backend's emission site does anything to reconcile a `%d` in
the format string against a `float`/`double`-typed argument before
this call — `format_string_codegen` (C backend) and the
`__pyc_format_string__` handler (LLVM backend) both just collect the
argument values (expanding tuple fields when the RHS is a tuple) and
pass them straight through.

## Minimal repro (2 lines, general — not yopyra/color-specific)

```python
x = 3.7
print("%d" % x)
```

| | Output |
|---|---|
| CPython | `3` |
| pyc `-D .` (C backend) | `25637` |
| pyc `-b` (LLVM backend) | `458032280` |

Both backends produce **deterministic but different** garbage (not
random noise — same value every run on a given backend), consistent
with the ABI-mismatch theory: each backend's own calling convention
determines exactly what ends up in the register/stack slot `%d`'s
`va_arg(int)` actually reads.

Confirmed the mismatch is specifically float-vs-`%d`: `"%d" % 7` (int
argument) and `"%f" % 3.7` (matching float/`%f`) both work correctly
on both backends — only a **type-mismatched** specifier/argument pair
triggers it.

## Why this matters

`%d` accepting (and truncating) a float is common, idiomatic Python —
color/pixel-value formatting (`"%d" % (val * 255)`, exactly yopyra's
shape) is a typical example, but so is anything computing a float and
formatting it as an integer count/index/percentage. This is silent
wrong output with **zero compiler diagnostic** — worse than a crash,
since a program can compile clean, run to completion, and produce
plausible-looking-but-wrong data (yopyra's own PPM file has valid
header/dimensions, just garbage pixel values throughout).

## What a fix needs

The `%`-operator lowering (wherever `__pyc_format_string__`'s send is
built, `python_ifa_build_if1.cc` per the `__mod__` comment near line
3204) or the codegen sites need to parse each format specifier against
its corresponding argument's actual resolved type and insert an
explicit conversion when they don't match `printf`'s own expectations
— at minimum: a float argument reaching an integer specifier (`%d`,
`%i`, `%o`, `%x`, `%X`) needs an explicit `(int64)` truncating cast
before the vararg call (mirrors Python's own semantics exactly); the
reverse (int argument, `%f`/`%e`/`%g` specifier) is presumably also
broken for the same ABI reason and needs the opposite cast. Both
backends need the fix — this isn't backend-specific, it's shared
runtime-contract logic that both currently skip.

## Verification plan

- `python3 repro.py` → `3` is the reference; both backends must match.
- Survey other integer/float specifier combinations (`%i`, `%x`,
  `%o`, `%e`, `%g`) for the same class of mismatch — only `%d`/float
  was directly tested here.
- Re-render yopyra's real `scene.txt` (full resolution) after the fix
  and confirm pixel values fall in `0-255` and look visually
  reasonable (a full CPython reference comparison at full resolution
  is likely still impractical time-wise; at minimum confirm no more
  out-of-range values and spot-check a downscaled region against
  CPython the way this investigation did).
- Full `test_pyc.py` both backends — `%`-formatting is used throughout
  the corpus and probably several `tests/*.py` fixtures already;
  treat any change here as touching a hot, shared path.

## What this unblocks

Correct behavior for any program that formats a float with an integer
specifier (or vice versa) — confirmed affects `yopyra` (silently wrong
render output, not caught by "compiles and doesn't crash") and
plausibly other corpus examples using similar formatted-output code
that haven't been checked pixel-for-pixel/byte-for-byte against
CPython.

## Fix (2026-08-08)

Implemented at the codegen sites, per the second option above (not
the frontend `__mod__` lowering) — the frontend's own existing
per-spec handling (the `%s`-stringification fix this file's "Root
cause" section references) can only see argument types for
compile-time-constant literals; a general expression's resolved type
isn't known until after FA, which is exactly when codegen runs.

Both `format_string_codegen` (`python_ifa_main.cc`, C backend) and the
LLVM backend's `__pyc_format_string__` handler (`ifa/codegen/cg_emit_llvm.cc`)
now: parse the format string's `%`-specifiers when it's a compile-time
constant (`Var->sym->constant` — the same field
`c_call_codegen`/`cg.cc` already use elsewhere for this), and, for
each corresponding argument (tuple fields exploded, or the single
value), insert an explicit `(int64)` cast (C backend) /
`CreateFPToSI` (LLVM) when a `d`/`i`/`o`/`u`/`x`/`X`/`c` spec receives
a `float`-`num_kind` argument, or the reverse — `(double)` /
`CreateSIToFP` — when an `f`/`e`/`g`/`E`/`G`/`F` spec receives an
`int`/`uint`-`num_kind` argument. A non-constant format string (or a
spec/argument-count mismatch) falls back to the old, unchecked
behavior unchanged — same scope boundary the existing `%s` fix
already accepts.

**One correction to this file's own "what a fix needs" scope**: real
Python actually only accepts a float for `%d`/`%i`/`%u` (truncates,
matching this fix) — `%o`/`%x`/`%X`/`%c` with a float argument is a
`TypeError` in CPython (`"%x" % 255.0` raises), not the "same class of
mismatch" this file assumed. Since pyc has no exception model
(consistent with how this codebase already handles other
CPython-would-raise cases, e.g. issue 006's `__format__`), the cast is
applied uniformly to all of `dioxXuc` anyway — turning UB/register-
garbage into *some* defined, deterministic value even for the
technically-invalid float+`%x`/`%o`/`%X`/`%c` combination, which is a
strict improvement (there was never a CPython reference value to
match for those anyway) without pretending to reproduce a
`TypeError` pyc can't raise.

**Verified:**
- The minimal repro (`"%d" % 3.7`) → `3` on both backends, matching
  CPython.
- Survey of the other specifier combinations from the verification
  plan (`%i`, `%u`, `%f`, `%e`, `%g`, plus a tuple of three floats
  through `%d %d %d`, and a mixed `%s`/`%d` — the exact `%s`-then-`%d`
  shape the existing frontend fix already special-cases) — all match
  CPython exactly on both backends. New regression test
  `tests/format_string_int_float_mismatch.py`.
- `color.__str__`'s *exact* shape from this file's own root-cause
  section (`"%d %d %d" % (max(0.0, min(self.r*255.0, 255.0)), ...)`,
  reconstructed standalone) — matches CPython exactly on the C
  backend.
- Full `test_pyc.py`, both backends: clean, no regressions.

**yopyra's own full/downscaled render — genuine progress, not fully
resolved, and not further chased here**: re-ran the downscaled
spot-check this file's verification plan describes. The original
garbage signature (`622879781` repeating for every pixel) is
**completely gone** — confirms the UB is fixed. But the resulting
pixel values, while all in-range and plausible-looking, still don't
match CPython's reference numbers, even though the isolated
`color.__str__`-shaped test above (using the identical expression)
matches CPython exactly. This means the residual mismatch is **not**
this format-string bug — it's some other, separate, not-yet-diagnosed
divergence elsewhere in yopyra's ray-tracing math, unmasked now that
the format-string UB no longer swamps it. Not investigated further
here (out of this issue's scope); worth its own issue if someone
picks up yopyra again.

**Separately discovered, unrelated, not fixed here**: verifying
yopyra's render also surfaced that `print(x, file=some_file_object)`
does not actually write to `some_file_object` — output goes to
stdout regardless of the `file=` keyword argument. Confirmed via
yopyra's own `print(renderPixel(x, y), end=' ', file=fileout)`
(intended to build the `.ppm` file's pixel data) writing to the
terminal instead, leaving the target file empty. A completely
separate bug from this issue's format-string UB; not filed as its own
issue, just flagged here since it was found in the course of this
verification and would otherwise be easy to lose track of.

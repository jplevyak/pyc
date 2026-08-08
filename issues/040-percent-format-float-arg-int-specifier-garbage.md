# 040 — `"%d" % <float>` produces garbage output instead of Python's truncate-to-int (both backends)

**Status:** open, found 2026-08-07 while diagnosing
[issues/025](025-shedskin-examples-coverage.md)'s `yopyra` entry (TODO
list item 10 — "rendered pixel values look wrong," left unconfirmed
since the reference `python3` run hadn't been compared side-by-side).

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

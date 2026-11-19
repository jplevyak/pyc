# 177 — int64 overflow at run time is silent in every mode

**Status:** open on the run-time half. The compile-time half, integer
LITERALS past int64 accepted silently, was fixed 2026-10-14: a literal
that fits 64 bits but not int64 wraps with a warning under permissive
(`PYC_INT_LITERAL_WRAP`) and is refused under `--strict`; one that fits
neither is refused in every mode (`tests/int_literal_*.py`). Rewritten
2026-11-03; the record is in git
(`git show 23cd789:issues/177-integer-literals-past-int64-were-accepted-silently.md`).

## Symptom

pyc's `int` is int64; CPython's is exact. `1 << 63`, or a product past
2^63, wraps silently in both modes. othello2 is the corpus witness: its
bitboards reach bit 63, `bit_count()` of `abs(x)` (CPython's definition,
kept by the author's choice) then counts the wrong bits, and it searches
220250 nodes where CPython searches 222922. Its mask literals warn, so the
sweep sees it, but a program that overflows without a wide literal is
invisible.

## What this is

A representation limit, not an inference bug: no contour or type choice
makes int64 hold 2^63. Matching CPython needs a wider integer or an
unsigned 64-bit type inferred for values masked to 64 bits. Neither
exists and neither is planned.

## Plan

Under `--strict`, a checked arithmetic path: `__builtin_*_overflow` on
`+`, `-`, `*` and `<<`, raising `OverflowError` with CPython's wording
for the operation (CPython would not raise, it would promote, so this is a
refusal at run time, which is rule 1's second-best outcome and the only
honest one). Measure it on nbody, mandelbrot, chaos and othello2 first;
if the cost on the default path is measurable, keep it strict-only and
say so in the README's limits. Do not count the raw bit pattern in
`bit_count` to make othello2's number come out: it breaks
`(-7).bit_count()`.

## Verification

A fixture whose `1 << 63` raises under `--strict` and wraps with no
diagnostic under permissive (documented); othello2 under `--strict`
stops at its literals, as now.

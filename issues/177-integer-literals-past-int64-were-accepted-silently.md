# 177 — integer literals past int64 were accepted silently

**Status:** the silent part is FIXED (2026-10-14); the representation limit
behind it is open. Found triaging othello2, one of the four corpus programs
whose stdout differs from CPython's.

## Symptom

pyc's `int` is int64, and CPython's is exact. An integer literal outside
int64 was accepted with no diagnostic in every mode, `--strict`
included, and its value depended on its base:

| literal | CPython | pyc before | why |
| --- | --- | --- | --- |
| `0xFFFFFFFFFFFFFFFF` | 18446744073709551615 | `-1` | `strtoull`, cast to int64: wraps |
| `0x8000000000000000` | 9223372036854775808 | `-9223372036854775808` | wraps |
| `99999999999999999999` | 99999999999999999999 | `9223372036854775807` | `strtoll`: SATURATES |

`-9223372036854775808` was right (a folded unary minus on 2^63), but the C
backend spelled it `-9223372036854775808`, which C reads as unary minus on
an unsigned literal, and clang warned.

## othello2

othello2's bitboards are 64-bit values, and its masks are literals such as
`0xFFFFFFFFFFFFFFFF` and `0xFEFEFEFEFEFEFEFE`. Every bit operation it
performs (`&`, `|`, `~`, and shifts followed by a mask) gives the same
64-bit pattern in pyc as in CPython. What differs is the VALUE of a board
with bit 63 set: at least 2^63 in CPython, negative in pyc. `bit_count()`
counts `abs(x)` (CPython's definition, kept by the author's choice on
2026-10-07), so it counts the wrong bits for such a board, the evaluation
differs, and the search visits 220250 nodes where CPython visits 222922.

This is a representation limit, not an inference bug: no contour or type
choice makes int64 hold 2^63. Counting the raw pattern in `bit_count`
would give CPython's node count by accident and break
`(-7).bit_count()`, which is 3.

## Fix (2026-10-14)

`parse_int_literal` (`python_ifa_build_if1.cc`) reads the magnitude as
uint64 in every base and reports its range. `check_int_literal_range`
then acts on it:

- **fits int64**: as before. Under a folded unary minus, 2^63 fits
  (`-9223372036854775808`).
- **fits 64 bits but not int64**: an ACCOMMODATION (issues/171).
  Permissive mode wraps it to its two's-complement value and warns,
  `FILE:N: warning: integer literal 0xFFFFFFFFFFFFFFFF exceeds int64 and
  wraps to -1, where CPython keeps it exact`. That is the standard
  `: warning:` form, which is what `corpus_sweep.sh`'s `warns` column
  counts. `--strict`, or `PYC_INT_LITERAL_WRAP=0`, refuses it.
- **does not fit 64 bits**: refused in every mode, since there is no
  value to wrap to.

The C backend now writes INT64_MIN as `(-9223372036854775807LL - 1)`
(`cg_sprint_imm`).

`pyc_lib/hashlib.py` was the one library user: MD5's length field was
`(n * 8) & 0xffffffffffffffff`. For a byte length in int64 the mask is a
no-op, so it was dropped.

Tests: `tests/int_literal_int64_edges.py` (every base at the int64 edges,
matches CPython), `tests/int_literal_wrap.py` (the warning, and what pyc
computes), `tests/int_literal_wrap_strict.py` (refused), and
`tests/int_literal_too_big.py` (refused in every mode).

othello2 now compiles with four warnings, one per 64-bit mask literal
(lines 31-37), and is refused under `--strict`. Its node count still
differs. Corpus check -R `check__default__4e218622+37947902`: othello2's
`warns` 0 -> 4, and every other verdict, warning count and contour count is
unchanged on all 77 (with the stdout triage's `.vary` files, see
issues/025).

## What is still open

**Literals are only where the overflow is visible at compile time.**
Arithmetic that overflows int64 at run time (`1 << 63`, or a product past
2^63) wraps silently in both modes. Detecting that needs a checked
arithmetic path, which costs every integer operation, and is not
attempted here.

othello2 itself can only match CPython with an integer wider than 64 bits,
or with an unsigned 64-bit type that pyc infers for values that are
masked to 64 bits. Neither exists.

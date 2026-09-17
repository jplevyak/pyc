# issues/163 — the corpus stdout check counted nondeterministic lines

**Status:** fixed 2026-09-17.

## The question

*"Should we ignore the elapsed-time line difference?"*

Yes — but not on its own, and the reason is the trap that makes the naive
version worse than the bug.

## Two problems, not one

**Over-counting.** Many corpus programs end with `print('TIME %.2f' % ...)`.
That line differs on every run by construction, so `cmp` reported the program
as differing from CPython no matter how correct it was. `genetic`'s entire
content matched CPython after [161](161-random-seed-collision-and-lcg.md) and
it was still scored `NO`.

**And the trap.** Four to six corpus programs print *nothing but* a timing
line — `dijkstra`, `mandelbrot2`, `sudoku3`, `circle`, and `sudoku5` and
`dijkstra2` when they build. Filtering the line and comparing what is left
would compare **empty against empty** and score them `yes`. That is strictly
worse than the over-count: it claims a verification that never happened, which
is exactly the hole ifa/158 found in `sudoku5` (its only output is a `TIME`
line, so the harness was comparing two wall clocks and reporting a match).

## The fix

**Detect nondeterminism by measurement, not by pattern.** Run CPython TWICE and
ignore only the lines CPython itself varies on. No `TIME` regex — which would
be matching by name, and would also wrongly excuse `circle`'s `3.11.0 (pyc)`,
a version string pyc genuinely prints differently and should be held to.

Then three verdicts instead of two:

| | |
| --- | --- |
| `yes` | comparable output exists and matches |
| `NO` | comparable output exists and differs |
| `none` | **nothing comparable remains** — this program's stdout verifies nothing |

`none` is reported as `unverifiable` in the summary and is never counted as a
match.

## Measured

| | before | after |
| --- | --- | --- |
| `stdout_differs` | 19 | **8** |
| `unverifiable` | — | **3** (`dijkstra`, `mandelbrot2`, `sudoku3`) |
| silently moved to *matching* | — | **8** |

**The old number over-counted differences by more than 2x**, and hid three
programs that verify nothing at all.

## Two harness bugs found on the way

- **The content key did not include the script itself.** Changing the verdict
  logic did not invalidate the cache, so the first re-run replayed old rows
  through new arithmetic and printed `unverifiable=0` — a number never
  measured. `corpus_sweep.sh`'s own hash is now part of the key.
- **`-C` was answered from the TSV cache.** Asking for CPython to be
  re-measured short-circuited on a cached row, so the forced re-runs never
  happened and the variance probe kept finding no second output. `-C` now
  bypasses the TSV cache.

Both are the same class of error the cache exists to prevent: reporting a
number that was not measured.

# 025 — the shedskin examples as pyc's coverage corpus

**Status:** open (tracking). Rewritten 2026-11-03 to the current state.
The per-program dig log, and the 2026-09-28 "has no type" triage (13 of 17
first blockers were missing builtin surface, every one since added), are
in git: `git show 23cd789:issues/025-shedskin-examples-coverage.md`, and
`e3b44e2c` for the 3,400-line log before that.

## What this is

`shedskin_examples/` is shedskin's `examples/` tree, vendored by
`git subtree --squash` (imported from upstream `6646da74`; do not rebase
across its merge commit). Its 77 runnable programs are pyc's compile /
run / CPython-output benchmark. To pull upstream:

```sh
# in a shedskin checkout:
git subtree split --prefix=examples -b pyc-examples-split
# in pyc:
git subtree pull --prefix=shedskin_examples ../shedskin pyc-examples-split --squash
```

## The contract

pyc's goal is CPython semantics for programs typeable without boxing,
not shedskin's answers (AGENTS.md). For a corpus program that fails:

- **If pyc invented the union or the error, pyc is wrong.** Find the
  confluence; never edit around an inference deficiency.
- **If the program has an actual type error** (CPython 3 raises on it, or
  one slot genuinely holds two unrepresentable types over its lifetime),
  the routes are a flagged, non-strict accommodation (README rule 2) or a
  minimal source edit under
  [shedskin_examples/PYC_CHANGES.md](../shedskin_examples/PYC_CHANGES.md),
  which logs every edit so far.
- **`--strict` never compiles a corpus program to output that differs
  from CPython.** It matches or it refuses.

## How to measure

- `./corpus_sweep.sh -m check` is the reference: compile rc, run rc,
  CPython rc and a stdout comparison, cached by tree. Run `-l` first.
  `-e "PYC_STRICT=1"` is the strict arm.
- `./shedskin_sweep.sh` buckets compile failures by first diagnostic.
- `compile_rc=0` is not evidence. Run the binaries.
- A program whose own run cannot be compared (endless, or prints only
  timing) carries a check driver, `NAME_check.py` or `NAME_check.sh`,
  listed in PYC_CHANGES.md; a `NAME.vary` file lists the lines the
  comparison ignores.

## State: sweep `check__default__6ac83790+4ec9875b` (2026-10-15)

| outcome | programs |
| --- | --- |
| **match CPython**, 58 | directly, or through a check driver (dijkstra, minpng, sudoku3, sudoku5, voronoi2, tictactoe, mandelbrot2, pystone, circle, sieve, minilight, rdb, rsync, pygasus, webserver) |
| **unverifiable by the sweep**, 18: CPython exceeds its 120 s cap | ac_encode, adatron, bh, chaos, chess, chull, kmeanspp, mao, oliva2, path_tracing, pisang, pygmy, richards, rubik2, score4, timsort, yopyra were verified by hand on 2026-10-12 (CPython under a 1-hour cap, binaries from `eeff80e2`): stdout and every written file byte-identical apart from each program's own `TIME` line |
| **differs**, 1 | othello2: 64-bit bitboards in an int64 ([177](177-int64-overflow-at-run-time-is-silent.md)); its four mask literals warn, and `--strict` refuses it |
| **does not compile**, 1 | othello3: exceeds the compile cap ([ifa/186](../ifa/issues/186-FA-pass-one-confluence-through-shared-builtins.md)) |

21 programs compile with warnings, almost all the int-to-float widening
(README accommodation table); the rest are othello2's literals and the
two `raise "…"` sites below.

### Edit candidates (actual type errors, not yet edited)

| program | site | the error | today | recommendation |
| --- | --- | --- | --- | --- |
| chess | `chess.py:92,128` | `raise "…"` (CPython 3: `TypeError`) | wrapped in `Exception(...)` under `PYC_RAISE_STRING`, with a warning; refused under `--strict` | edit to `raise Exception("…")` |
| minilight | `ml/entry.py:77` | same | same | same |

### Residuals found on the way, tracked elsewhere

- sokoban's timed half is 2.5× CPython's: `bytes(bytearray)` goes through
  a list ([ifa/199](../ifa/issues/199-emitted-code-performance.md)).
- `import X` from a file named `X.py` overflows the frontend's stack
  ([187](187-self-import-overflows-the-frontend.md)).
- loop's `sys.setrecursionlimit` is ignored and it overflows the C stack
  ([173](173-silent-deviations-found-by-the-strict-suite-check.md) item 3).

## Verification

After any frontend or runtime change: a `check` sweep with no verdict
regression (match, warning count, contour counts per program), and the
strict arm with every program matching or refused.

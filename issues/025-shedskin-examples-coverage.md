# Issue 025: the shedskin examples as pyc's coverage corpus

**Status:** open (tracking corpus). Rewritten 2026-09-28 to the current
state. The chronological dig log (2026-07 → 2026-09, about 3 400 lines of
per-program investigations, most since closed into their own issues) is in
git: `git show e3b44e2c:issues/025-shedskin-examples-coverage.md`.

## What this is

`shedskin_examples/` is shedskin's `examples/` tree, vendored by
`git subtree --squash` (imported from upstream `6646da74`; do not rebase
across its merge commit). It holds 77 runnable programs, and it is pyc's
compile / run / CPython-output benchmark. To pull upstream:

```sh
# in a shedskin checkout:
git subtree split --prefix=examples -b pyc-examples-split
# in pyc:
git subtree pull --prefix=shedskin_examples ../shedskin pyc-examples-split --squash
```

## The contract, and when a program may be edited

pyc's goal is CPython semantics for programs that can be typed without
boxing. It is not to match shedskin (AGENTS.md). For a corpus program
that fails:

- **If pyc invented the union or the error, pyc is wrong.** Find the
  confluence (AGENTS.md); never edit around an inference deficiency.
- **If the program has an ACTUAL type error** (CPython 3 raises on it, or
  a slot genuinely holds two unrepresentable types over its lifetime),
  there are two routes: a pyc accommodation that is **flagged and
  non-strict only**
  ([171](171-permissive-accommodations-must-be-flagged-and-non-strict.md)),
  or a minimal source edit that removes the error. The rules and the log
  of every edit are in
  [shedskin_examples/PYC_CHANGES.md](../shedskin_examples/PYC_CHANGES.md).
  Edits so far: `chess` (`printBoard`, explicit `return False`), `bh`
  (`float(floor(...))`), CRLF → LF.
- **`--strict` must never compile a corpus program to output that differs
  from CPython.** It either matches or refuses.

### Edit candidates (actual type errors; not yet edited)

| program | site | the error | accommodation today | recommendation |
| --- | --- | --- | --- | --- |
| `chess` | `chess.py:92,128` | `raise "…"` (CPython 3: `TypeError`) | wrapped in `Exception(...)`, permissive only, with a warning (171 #1, done); refused under `--strict` | edit to `raise Exception("…")` |
| `minilight` | `ml/entry.py:77` | `raise '…'` | same | same |
| `minpng` | `struct.pack('<BHH', bool(last), …)` | not an error in CPython, but `(bool, int, int)` indexed at a runtime index has no representation | none | a no-op edit, `int(bool(last))` (issues/041) |

## How to measure

- `./corpus_sweep.sh -m check` is the reference: compile rc, run rc,
  CPython rc and a stdout comparison, cached by tree. Run `-l` first. Use
  `-e "PYC_STRICT=1"` for the strict-mode arm that
  [171](171-permissive-accommodations-must-be-flagged-and-non-strict.md)
  asks for.
- `./shedskin_sweep.sh` buckets failures by first diagnostic, for
  triage.
- `compile_rc=0` is not evidence of anything. Run the binaries.

## Current state — sweep `check__default__4b61e721+d7af0afe` (2026-09-28)

77 programs: **53 compile, 24 do not; 20 match CPython.**

| outcome | programs |
| --- | --- |
| **matches CPython** (20) | astar, block, brainfuck, collatz, fysphun, genetic, hq2x, linalg, mandelbrot, nbody, neural2, othello, plcfrs, pylife, sat, sha, sieve, stereo, sudoku2, voronoi |
| runs, no stdout to compare | dijkstra, mandelbrot2, pystone, sudoku3, sudoku5, tictactoe |
| runs, **stdout differs** | ant, circle, kanoodle, mastermind2, tonyjpegdecoder (check for unseeded `random` or a `TIME` line before treating as a bug) |
| runs, CPython reference times out (no oracle) | bh, chull, oliva2, path_tracing, pygmy, richards, timsort |
| **compiles, then aborts** (ifa/102) | adatron 134, life 134, loop 139, pisang 134, quameon 134 |
| times out where CPython finishes | dijkstra2, solitaire |
| both time out at 120 s | ac_encode, chaos, chess, kmeanspp, rubik2, score4, webserver, yopyra |
| **does not compile** (24) | amaze, doom, genetic2, go, lz2, mao, minilight, minpng, msp_ss, mwmatching, neural1, othello2, othello3 (compile timeout), pygasus, rdb, rsync, rubik, softrender, sokoban, sudoku1, sudoku4, sunfish, tarsalzp, voronoi2 |

First blockers of the compile failures, where known: a 2026-09-28 triage
of the "has no type" bucket (in progress) finds 13 of 17 blocked on
missing builtin surface (`str.rstrip`, `dict.copy`, `next(file)`,
`list.index(x, start)`, …); genetic2, a `{None, int64}` union (issues/048);
sunfish, `{list, tuple}` from `tuple(iterable)` (issues/110); voronoi2, a
getopt path that provably always raises (ifa/049); rubik, `None` reaching
`key[1]` (unlocated); softrender and othello3, unanalysed.

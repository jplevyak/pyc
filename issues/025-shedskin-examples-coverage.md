# Issue 025: shedskin examples as an incremental pyc coverage corpus

**Status:** open (tracking corpus). Baseline established 2026-07.
**Affects:** frontend scoping (`python_ifa_*`), import resolution
(`build_import_if1`), the DParser grammar (`python.g`), and FA
(`unresolved call`). Not a single bug — an umbrella tracking the
real-world programs pyc can't yet compile and the order to attack
them.

## What this is

`shedskin_examples/` is the `examples/` tree of the
[shedskin](https://github.com/shedskin/shedskin) project (a
Python-to-C++ compiler with a similar remit), vendored into this
repo via **git subtree with `--squash`**. 86 example programs of
idiomatic, runnable Python — a far broader corpus than `tests/`,
which is mostly minimal feature probes. The goal is to
*incrementally* extend pyc until it compiles and runs them.

### Provenance / updating

- Imported from upstream `examples/` at commit `6646da74`
  (upstream `master` was `d01d4948` at import time).
- `--squash` was deliberate: the examples carry 889 commits of
  history and multi-MB data/asset files; we want the sources as a
  test corpus, not that history in pyc's `.git`.
- To pull upstream updates later:
  ```sh
  # in a shedskin checkout:
  git subtree split --prefix=examples -b pyc-examples-split
  # in pyc:
  git subtree pull --prefix=shedskin_examples \
      ../shedskin pyc-examples-split --squash
  ```
- The bundled `shedskin_examples/.gitignore` already excludes the
  per-example compiled binaries, so building examples in place
  won't dirty the tree. Do NOT commit pyc build outputs
  (`<name>.c`, executables) either — the sweep below builds in a
  scratch dir for exactly this reason.

## Measuring coverage

`./shedskin_sweep.sh` (repo root) runs `pyc -D <root> <name>.py`
on every `shedskin_examples/<name>/<name>.py` in an isolated
scratch build dir and buckets the outcome by first diagnostic.
Pass a substring to sweep a subset (`./shedskin_sweep.sh sudoku`).
It reports "N compiled to C" and a normalized failure histogram.

## Baseline — 2026-07 (0 / 77 reach C)

None compile to C yet; this is the honest starting line. The
failures cluster into four actionable buckets plus a couple of
deep crashes. Ordered by how close each bucket is to working:

### A. `unresolved call` — 7 examples (CLOSEST: past parse, import, scoping; die in FA)

`block` (`__sub__`), `dijkstra2` (`__gt__`), `life` (`__iter__`),
`loop` (`__iadd__`), `mwmatching` (`__iter__`), `oliva2`
(`__add__`), `timsort` (`__gt__`).

These get furthest — the frontend accepts them and FA runs; it
just can't resolve an operator/iterator dispatch. Likely the
highest value-per-fix bucket because each is one missing
dispatch away from a full pipeline run, and the operators
(`__gt__`, `__iter__`, `__iadd__`, `__add__`, `__sub__`) are core.

### B. `'X' redefined as local` — 18 examples (import-free scoping bug)

`chess collatz dijkstra go linalg minpng othello path_tracing
pisang pygmy pylife sat sudoku2 sudoku3 sudoku4 sudoku5 sunfish
tictactoe`

A single scoping diagnostic fires across 18 programs (e.g.
`collatz.py:40`). These are largely import-free, so fixing the
underlying scoping rule should unblock the whole bucket at once —
the cleanest single lever. First step: reduce `collatz` line 40
to a minimal repro under `tests/`.

### C. `build_import_if1` assertion `m' failed` — 24 examples (import resolution abort)

`ant brainfuck chull fysphun genetic genetic2 kanoodle kmeanspp
mandelbrot mandelbrot2 minilight msp_ss neural2 othello2 pygasus
pystone quameon richards rubik2 sieve softrender sudoku1 tarsalzp
voronoi`

Largest bucket, but a hard `assert(m)` abort in import lowering
(a module resolves to null). Fixing it turns aborts into the
*next* real error rather than immediately producing working
binaries, so it's high-count but not high-yield until B/A are
also addressed. Worth converting the assert into a diagnostic
early regardless (aborts hide whatever follows).

### D. syntax errors — ~25 examples (DParser grammar gaps)

`ac_encode adatron astar bh chaos circle doom hq2x lz2 mao
mastermind2 nbody neural1 plcfrs rdb rubik2 score4 sokoban
solitaire stereo tonyjpegdecoder voronoi voronoi2 webserver
yopyra`

Assorted `python.g` gaps — several involve triple-quoted-string
edge cases (`rdb`, `tonyjpegdecoder`), others distinct
constructs. Each is its own grammar fix; triage individually and
fold minimal repros into the DParser test set.

### E. deep crashes — 1

`amaze`: `if1_send` assertion `v' failed` (past the frontend,
crashes building IF1). Needs its own investigation.

(`othello3`, `rsync`, `sha` produced no captured diagnostic —
likely a differently-worded message or the 60s compile timeout;
re-run individually to classify.)

## Recommended order

1. **B (`redefined as local`)** — one scoping rule, 18 examples,
   no import prerequisite. Best first lever.
2. **A (`unresolved call`)** — 7 examples one dispatch from a
   full run; core operators.
3. **C** — first make the import path emit a diagnostic instead
   of `assert(m)`, then work the resolution gap.
4. **D** — grammar fixes, one construct at a time.
5. **E** — the lone deep crash.

Re-run `./shedskin_sweep.sh` after each change; the bucket counts
are the regression/progress signal. As examples start reaching C
(and running), promote the stable ones into `test_pyc.py` with
`.exec.check` goldens so they don't regress.

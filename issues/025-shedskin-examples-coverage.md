# Issue 025: the shedskin examples as pyc's coverage corpus

**Status:** open (tracking corpus). Rewritten 2026-09-28 to the current
state. The chronological dig log (2026-07 → 2026-09, about 3 400 lines of
per-program investigations, most since closed into their own issues) is in
git: `git show e3b44e2c:issues/025-shedskin-examples-coverage.md`. The
2026-09-28 "has no type" triage below is kept verbatim.

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
  ([171](closed/171-permissive-accommodations-must-be-flagged-and-non-strict.md)),
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
  [171](closed/171-permissive-accommodations-must-be-flagged-and-non-strict.md)
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
| **compiles, then aborts** (ifa/102) | adatron 134, pisang 134, quameon 134 (life moved to "does not compile", below) |
| **compiles, then overflows the C stack** ([173](173-silent-deviations-found-by-the-strict-suite-check.md)) | loop 139: `sys.setrecursionlimit(100000)` is ignored; runs to completion under `ulimit -s unlimited` |
| times out where CPython finishes | dijkstra2, solitaire |
| both time out at 120 s | ac_encode, chaos, chess, kmeanspp, rubik2, score4, webserver, yopyra |
| **does not compile** (25) | amaze, doom, genetic2, go, life, lz2, mao, minilight, minpng, msp_ss, mwmatching, neural1, othello2, othello3 (compile timeout), pygasus, rdb, rsync, rubik, softrender, sokoban, sudoku1, sudoku4, sunfish, tarsalzp, voronoi2 |

**`life`, 2026-09-29 (issues/171 #5):** it compiled and then aborted
(`matching function not found`, rc 134) until `itertools.product(repeat=)`
started yielding real tuples instead of lists. Now it is refused at compile
time (`map(process, ...)`: `illegal call argument type ... closure`, and an
untyped expression in `process`). Bisected: HEAD's `itertools.py` alone
restores the old verdict. A reduced program with the same
`product`/`zip`/`defaultdict`/generator/`map` shape compiles and matches
CPython, and cutting `process`'s body to `return None` compiles too. So the
new element flow meets `process`'s `while 1:` loop, whose only `return`s
sit behind `board in history`. FA converges. Not yet root-caused. A refusal
replaced a runtime abort, so no correct answer was lost.

First blockers of the compile failures, where known: the triage below
(13 are missing builtin surface); genetic2, a `{None, int64}` union (issues/048);
sunfish, `{list, tuple}` from `tuple(iterable)` (issues/110); voronoi2, a
getopt path that provably always raises (ifa/049); rubik, `None` reaching
`key[1]` (unlocated); softrender and othello3, unanalysed.

### "has no type" bucket re-triaged against 3f36072b (2026-09-28)

`shedskin_sweep.sh` at `3f36072b` (clean tree, `TIMEOUT=120`): 51 of 77
compile, 26 fail. **17** of the 26 have `'X' has no type` / `expression
has no type` as the first diagnostic. That first line is almost always a
cascade, so each program was triaged by its earliest *non*-cascade
diagnostic (`unresolved member`, `unresolved call`, `illegal ... <class>`)
and the suspect confirmed with a 2-5 line probe. Probes were compiled with
`pyc -D <root>` and compared against CPython.

**Most of the bucket is missing builtin surface, not inference.** 13 of the
17 programs are first blocked by a method or argument form that
`__pyc__/` or `pyc_lib/` does not provide. Each one reproduces in a
standalone probe. Fixing a program's first blocker may uncover more.

| program | first blocker (probe confirmed) | where |
| --- | --- | --- |
| go | `str.rstrip()`: no `rstrip`/`lstrip` at all, only `strip` | `__pyc__/01_str.py` |
| minilight, mwmatching | `next(f)` on a file: `__pyc_file__` has `__iter__` but no `__next__` (a CPython file is its own iterator) | `__pyc__/07_file.py` |
| othello2 | `int.bit_count()`. The later `__sub__`/`__mul__` errors are cascade | `__pyc__/02_numeric.py` |
| sudoku4 | `dict.copy()` | `__pyc__/07_dict.py` |
| amaze | `list.index(x, start)`: `index(self, x)` only | `__pyc__/04_sequence.py:337` |
| lz2 | `str.find(sub, start, end)`: `find(self, sub)` only | `__pyc__/01_str.py:256` |
| rdb | `str.split(sep, maxsplit)`, then `array.tobytes`/`fromfile` | `01_str.py:196`, `pyc_lib/array.py` |
| neural1 | `sorted(..., reverse=True)`: `sorted(seq)` only | `__pyc__/05_builtins.py:418` |
| sokoban | `filter(None, it)`. `Board.px`/`py` unresolved is cascade: the loop that sets them runs over the bottom list | `__pyc__/05_builtins.py` |
| pygasus | `array.tobytes()` | `pyc_lib/array.py` (only append/extend/len/get/set/iter exist) |
| rsync | `list.index(x, start)`, `bytes(deque)`, `binfile.seek` | as above, plus `07_file.py` |
| msp_ss | `b'%c' % int` (bytes `%` with a non-tuple operand), `struct.unpack('>H8xBB4x', ...)` | `01b_bytes.py:115`, `pyc_lib/struct.py` |

Once rsync compiles it will hit `hashlib.md5(...).hexdigest()`, which is
still a stub returning `""`. The probe exits 0 and prints an empty line.
That is a silent wrong answer (issues/041), not a compile error.

**Four programs are analysis defects:**

- **tarsalzp: `Cls.attr` is unresolved when the class attribute holds a
  container.** It fails in every sweep on record, so this is not a
  regression. An `int` class attribute works (`class L: k = 3` →
  `L.k + v` prints 4). A list does not:

  ```python
  class L:
      k = [3]
  print(L.k[0])   # error: unresolved member 'k' of class 'L'
  ```

  Reading `self.lut` from a method works. `Lg2.iLog2` reads
  `Lg2.lgLut[...]`, which is the failing form. tarsalzp also needs
  `array.fromfile` on a binfile after this.

  **Fixed 2026-09-28.** This was a frontend bug, not an FA bug. The
  `PY_subscript` trailer in `build_if1_pyda` flushed the pending member on
  `cur_val` directly. The other four member flushes (attribute, call,
  `**`, end of chain) read through `cur_val->self`, the class's meta
  instance. So `L.k` worked and `L.k[i]` did not. Loads, stores, and
  slices all went through this path. `tests/class_attr_subscript.py`
  covers it. tarsalzp now stops at a later error,
  `Main.py: illegal call argument type ... illegal: Buffer`.
- **sudoku1: `u == []` stops folding when the program also calls
  `list.pop(i)` with a non-constant `i`.** This is ifa/160 fragility.
  Minimal:

  ```python
  def perm(u):
      if u == []:
          return True
      for c in range(len(u)):
          t = u.pop(c)
      return False
  print(perm([(1,2)]))   # error: unresolved call '__ne__'
  ```

  With `u.pop(0)` it compiles. The likely mechanism is that `len` in
  `pop` shares a contour with the `ll = len(l)` that must fold to 0 in
  `list.__eq__`. **This is not confirmed.** Check it with `IFA_DBG_FUNES`
  on `len` before acting.
- **voronoi2: in `getopt` with no `longopts`, `_match_long_opt` provably
  always raises.** Its `possibilities` list is never written, so
  `not possibilities` folds to true. The function returns bottom, and the
  caller's `has_arg, opt = ...` reports it, even though `_do_longs` is
  dead for this argv. This is the ifa/049 shape, and it is 175's measured
  dead end #1 ("gate the continuation of a call on a non-bottom result").
  Passing any non-empty `longopts` makes it compile. A hand-written
  imitation (always-raising callee, `longopts=[]` default) did **not**
  reproduce it, so the trigger is narrower than this description.
  `import getopt, sys; getopt.getopt(sys.argv[1:], "thdp")` alone is the
  reproducer.
- **rubik: `None` reaches `key[1]` in `getCoords`**
  (`unresolved member '__getitem__' of class '__pyc_None_type__'`, 126
  errors). Suspects are `primeCube(frontFace=None)` and the if/elif
  chains that fall off the end, such as `getFaces` and `getPosFromUp`.
  Two probes of those shapes compile fine. **Not located**, so this needs
  its own dig.

**Side finding, not in the bucket:** `import X` from a file named `X.py`
overflows the stack in the frontend. `build_import_if1` →
`build_one_module_if1` → `build_if1_pyda` recurse without bound
(`python_ifa_build_if1.cc:89`/`3570`), and pyc exits with a SIGSEGV.
CPython binds the partially initialised module.

**Leverage:** about 10 small `__pyc__`/`pyc_lib` additions would clear the
first blocker in 13 of the 17. The next blockers are unmeasured. The four
analysis defects are separate investigations. tarsalzp's has a 3-line
reproducer and is the cheapest to start on.

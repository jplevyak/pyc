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

## How to measure

- `./corpus_sweep.sh -m check` is the reference: compile rc, run rc,
  CPython rc and a stdout comparison, cached by tree. Run `-l` first. Use
  `-e "PYC_STRICT=1"` for the strict-mode arm that
  [171](closed/171-permissive-accommodations-must-be-flagged-and-non-strict.md)
  asks for.
- `./shedskin_sweep.sh` buckets failures by first diagnostic, for
  triage.
- `compile_rc=0` is not evidence of anything. Run the binaries.

## Current state — sweep `check__default__520739fe+66a0b1e8` (2026-10-07)

77 programs: **74 compile, 3 do not; 36 match CPython.** Fixed since the
2026-09-28 sweep (`4b61e721+d7af0afe`, 24 not compiling, 20 matching):
lz2, mao and pygasus (2026-10-05); rdb, minpng, msp_ss, voronoi2 and
mwmatching (2026-10-06); minilight, neural1, sokoban, othello2, sudoku4, tarsalzp, rsync, rubik and life (2026-10-07; sunfish too, but its compile now exceeds the cap). Each
has a row in the blocker table below.

| outcome | programs |
| --- | --- |
| **matches CPython** (37) | amaze, astar, block, brainfuck, collatz, doom, fysphun, genetic, go, hq2x, kanoodle, life, linalg, loop, lz2, mandelbrot, msp_ss, mwmatching, nbody, neural1, neural2, othello, plcfrs, pylife, quameon, rubik, sat, sha, sokoban, softrender (C backend; prints nothing, rc 0 like CPython), solitaire, stereo, sudoku1, sudoku2, sudoku4, tarsalzp, voronoi |
| runs, no deterministic stdout to compare (9) | dijkstra, mandelbrot2, minpng, othello2, pystone, sudoku3, sudoku5, tictactoe, voronoi2. othello2 prints a timing on every line, but its node counts differ from CPython's: int64 overflow, see its row below |
| runs, **stdout differs** (5) | ant, circle, mastermind2, sieve, tonyjpegdecoder. sieve differs only in its `time:` lines, circle only in its Python-version line; the other three are not triaged (check for unseeded `random`, a `TIME` line, or int/float widening printing `1.0` for `1`) |
| runs (rc 0), CPython reference times out at 120 s, so no oracle (18) | ac_encode, adatron, bh, chaos, chess, chull, kmeanspp, mao, minilight, oliva2, path_tracing, pisang, pygmy, richards, rubik2, score4, timsort, yopyra |
| same exit status as CPython, no stdout verdict (4) | pygasus and webserver (both endless by design, 124; see pygasus's row below), rdb (both rc 1: the corpus run has no iPod directory), rsync (both rc 1: no `testdata/` in the corpus or upstream) |
| times out where CPython finishes (1) | dijkstra2 |
| **compiles, then fails** (0) | genetic2 matches CPython since 2026-10-09 (except its timing values) on both backends: [174](closed/174-format-with-a-tuple-variable-skips-str-and-arity.md) |
| **does not compile** (2) | othello3 (compile timeout), sunfish (compile timeout: 534 s and 3 GB alone, against the 400 s cap; [ifa/185](../ifa/issues/185-FA-merged-tuple-receiver-makes-the-arity-unroll-live.md). Once compiled it matches CPython on both backends) |

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
(13 are missing builtin surface); genetic2, a `{None, int64}` union (issues/048) from `execute`'s implicit
fall-off, removed 2026-10-05 by a corpus edit (PYC_CHANGES.md); it now compiles,
and its last blocker is the final genome print (issues/174);
sunfish, `{list, tuple}` from `tuple(iterable)` (issues/110); voronoi2, a
getopt path that provably always raises (ifa/049); rubik, `None` reaching
`key[1]` (unlocated); othello3, see [ifa/186](../ifa/issues/186-FA-pass-one-confluence-through-shared-builtins.md).

**`softrender`, 2026-10-09: fixed, three bugs in a row.** (1) `bytearray`
had no `__pyc_getslice__` / `__pyc_setslice__`, so
`self.components[:] = self.reset` did not compile (missing builtin surface,
as in the triage below). (2) `[x] * 0` was a NULL list in both runtimes
(`_CG_list_mult`), and `self.zbuffer[:] = ...` under `RenderContext(0, 0)`
segfaulted mutating it. (3) `bool(x)` lowered to a C cast through the
numeric `__coerce__` constructor, so `bool([])` was `True`: the clipper's
`return bool(vertices)` sent an empty list on to `vertices[-1]`. `bool`
now lowers to `__pyc_to_bool__`, as `if x:` does. Tests:
`bool_builtin_truthiness`, `list_mult_empty_is_a_list`, `bytearray_slice`.
It then segfaulted under `-b`, fixed the same day: [ifa/187](../ifa/issues/closed/187-LLVM-constructor-clone-omits-a-method-slot-another-dispatch-reads.md). It matches CPython on both backends.

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
| go | ~~`str.rstrip()`: no `rstrip`/`lstrip` at all, only `strip`~~ FIXED 2026-10-03: `strip`/`lstrip`/`rstrip` take `chars`. go compiles, and its stdout matches CPython once [123](closed/123-str-does-not-fall-back-to-repr.md) is fixed (2026-10-03) | `__pyc__/01_str.py` |
| minilight, mwmatching | ~~`next(f)` on a file: `__pyc_file__` has `__iter__` but no `__next__` (a CPython file is its own iterator)~~ FIXED 2026-10-06: a file is now its own iterator (`iter(f) is f`), with one line of look-ahead for the for-loop protocol that every read drains first, so `next(f)`, `for line in f` and `readline` share one position; and `next()` past exhaustion raises StopIteration for every iterator. mwmatching compiles with no diagnostics and matches CPython (bar its `TIME` line); this was its only blocker. minilight gets past it to its other blockers | `__pyc__/07_file.py`, `05_builtins.py` |
| minilight | FIXED 2026-10-07; three root causes behind `next(f)`, plus an LLVM bug. (1) `re.Pattern` had no `search` (camera/scene/triangle parse with `SEARCH.search(line)`); it was left out for ifa/040, which is closed. (2) `b'%s ... %u %u' % (PPM_ID, URI, w, h)`, a mixed tuple the bytes formatter read at a runtime index (ifa/134), and no `%s`; it now goes through `tuple.__pyc_bytes_fmtargs__`, a per-position unroll. (3) unseeded `random()` returned 0.0 forever (MT19937 from an all-zero state), so the path tracer's Russian roulette never ended a path and `get_radiance` overflowed the stack; it now seeds from OS entropy on first use, as CPython does on import. (4, LLVM only) same-named module globals were one LLVM global, so camera's `SEARCH` was scene's. Unmodified, minilight compiles (only the `raise '...'` warning, line 49 above) and runs on both backends; seeded, its image is byte-identical to CPython's. The new errors in bytes `%` do not propagate yet (issues/175) | `pyc_lib/re.py`, `01b_bytes.py`, `python_ifa_main.cc`, `pyc_lib/random.py`, `cg_emit_llvm.cc` |
| othello2 | ~~`int.bit_count()`~~ FIXED 2026-10-07: `bit_count` added (popcount of `abs(x)`, CPython's definition), and two literal bugs behind it: `0b`/`0o` literals were parsed as base 10 and silently became 0 (its `move & 0b111`), and `_` digit separators did not parse. othello2 compiles and runs (0.5 s vs 49 s), but searches 220250 nodes where CPython searches 222922: its bitboards are 64-bit values up to 2^64, which wrap negative in pyc's int64, so `bit_count` of abs() counts the wrong bits. Counting the raw 64-bit pattern (shedskin's choice) gives CPython's 222922, but breaks `(-7).bit_count()` (62, not 3); kept CPython's definition (author's choice). The difference is int64 overflow, a representation limit | `__pyc__/02_numeric.py`, `python.g`, `python_ifa_build_if1.cc` |
| sudoku4 | ~~`dict.copy()`~~ FIXED 2026-10-07 (ifa/086): `copy` on list, dict and set; and `copy.copy` of a dict or set no longer shares the original's storage (an overwrite through the copy changed the original). sudoku4 compiles with no diagnostics and matches CPython (bar `TIME`); this was its only blocker | `__pyc__/04_sequence.py`, `07_dict.py`, `08_set.py` |
| amaze | `list.index(x, start)`: `index(self, x)` only | `__pyc__/04_sequence.py:337` |
| lz2 | ~~`str.find(sub, start, end)`: `find(self, sub)` only~~ FIXED 2026-10-05: `find`/`index` take CPython's slice-normalized `start`/`end`. lz2 compiles and runs; stdout and both output files match CPython. `find` and `__contains__` moved to a C scan (`_CG_str_find`): the `__pyc__` char loop allocated two 1-char strs per probe and made lz2 take 64 s, against CPython's 4.1 s. It now takes 4.1 s | `__pyc__/01_str.py` |
| rdb | ~~`str.split(sep, maxsplit)`, then `array.tobytes`/`fromfile`~~ FIXED 2026-10-06. It needed `str.split(sep, maxsplit)`, `bytes.split`/`find`, `bool &=`, file `seek`/`tell`, and `array`'s `tolist`/`fromlist`/`frombytes`/`fromfile`, slicing and `del` of a slice. One corpus edit: `a.fromstring` -> `a.frombytes` (Python 2; CPython 3 raises `AttributeError` there, see PYC_CHANGES.md). rdb compiles with no warnings. The corpus run has no iPod directory, so pyc and CPython both stop at that check with rc 1 and identical output. On scratch iPod fixtures (3 files, then 42 across nested directories, each run twice to exercise the read-back path), stdout and all four database files are byte-identical to CPython's | `__pyc__/01_str.py`, `01b_bytes.py`, `00_runtime.py`, `07_file.py`, `pyc_lib/array.py` |
| minpng | ~~`struct.pack('<BHH', bool(last), …)`~~ FIXED 2026-10-06, and NOT a corpus edit: CPython packs `True` as 1, and the `(bool, int, int)` union was pyc's own. `pyc_lib/struct.py`'s `pack` read its `*args` record with a runtime index (`args[ai]`), which ifa/134 refuses for a record whose fields mix numeric types. It now converts through `tuple.__pyc_toints__`, a constant-index unroll generated by inject_tuple_methods, the analogue of shedskin's variadic-template fold (`lib/struct.hpp`, one `__pack_one<T>` per argument). That also needed the unroll count to see imported modules: it scanned only the main file, so struct's `*args` never sized it (and a heterogeneous tuple built in any imported module was refused when printed; tests/tuple_from_imported_module). minpng compiles with no warnings; minpng.png is byte-identical to CPython's | `pyc_lib/struct.py`, `python_ifa_main.cc`, `python_ifa_build_syms.cc` |
| neural1 | ~~`sorted(..., reverse=True)`: `sorted(seq)` only~~ FIXED 2026-10-07: `sorted(iterable, key=None, reverse=False)`, delegating to `list.sort` (which also replaces its O(n^2) insertion sort). It had been kept parameterless because defaulted parameters once broke builtins_batch's `sum()` contours (033/040 split-order fragility); that no longer reproduces. neural1 compiles with no diagnostics and matches CPython (bar `TIME`; 9.7 s vs 56.5 s). This was its only blocker | `__pyc__/05_builtins.py` |
| sokoban | ~~`filter(None, it)`~~ FIXED 2026-10-07, four root causes in turn. (1) `filter(None, it)` called `None`; it now keeps the true items. (2) `bytearray(bytes)` and `bytes(bytearray)` had no conversion (the @vector constructor takes only a length); `bytearray(x)` now dispatches to `x.__pyc_tobytearray__()`, as `bytes(x)` does. (3) the frontend kept a backslash-newline inside a string literal, so `level = """\` gave the board a `\` first row and the search ran on a different board (it never finished); it is a line continuation now, and `\012`-style octal escapes read three digits. (4) `collections.deque.popleft` was `list.pop(0)`, O(n): one solve took 27 s against CPython's 0.8 s; it is a head index now. sokoban matches CPython (bar `TIME`), but its timed half is 2.5x CPython's (9.0 s vs 3.5 s): `bytes(bytearray)` goes through a list, 18x slower than CPython | `__pyc__/05_builtins.py`, `06_bytearray.py`, `python_ifa_build_if1.cc`, `pyc_lib/collections.py` |
| pygasus | ~~`array.tobytes()`, then `ord(bytes)`~~ FIXED 2026-10-05. `ord` takes only `str`, so `ord(f.read(1))` compiled to a runtime `C call argument type mismatch` abort, not a compile error. `ord(x)` now dispatches to `x.__pyc_ord__()` on str and bytes. pygasus is a `while True:` emulator, so both it and CPython hit the 120 s cap. A bounded copy (1500 `pExec` steps, checksumming registers, RAM, PPU/sprite RAM and the screen every 100) matches CPython at every checkpoint, 1.8 s vs 21.1 s | `__pyc__/05_builtins.py`, `01_str.py`, `01b_bytes.py` |
| mao | ~~`array.tofile()`~~ FIXED 2026-10-05: `array` gained `tobytes`/`tofile` for the integer typecodes. That exposed a silent miscompile: `bytes.__mod__` copied any directive except `%c` through verbatim, so `b"%i %i\n" % (w, h)` wrote a literal `%i %i` PPM header. It now formats `%d`/`%i`/`%u` and raises on anything else. mao's `mao.ppm` is byte-identical to CPython's | `pyc_lib/array.py`, `__pyc__/01b_bytes.py` |
| rsync | ~~`list.index(x, start)`, `bytes(deque)`, `binfile.seek`~~ FIXED 2026-10-07: `bytes()` of a `deque` (and a `range`), files' `closed` attribute, and a real `hashlib.md5` (the stub returned `""`; issues/041). It compiles; the corpus has no `testdata/` (nor does upstream shedskin), so pyc and CPython both stop at `FileNotFoundError` (rc 1). On generated data its patched file is byte-identical to CPython's on both backends, and on an input that trips rsync's own bug (appending to a matched block's `None` data) pyc raises CPython's `AttributeError` | `pyc_lib/hashlib.py`, `pyc_lib/collections.py`, `05_builtins.py`, `07_file.py` |
| msp_ss | ~~`b'%c' % int`, `struct.unpack('>H8xBB4x', ...)`~~ FIXED 2026-10-06. The first blocker was `pyc_lib/serial.py`, a stub whose `Serial(port, baudrate)` could not take msp_ss's valid 8-argument pyserial call and whose `read` returned str: 249 errors, all cascade. It now models pyserial 3.5 and raises on open (issues/041), which exposed ifa/049: everything after a serial call can only be reached if the call returns, and it cannot. Also `bytes.strip`/`lstrip`, `b'%c' % value` with one operand, `int(bytes, base)` (dispatched on the argument, as `ord`), and two corpus edits for Python 2 / pyserial 2 calls CPython 3 rejects (`has_key`, `setBaudrate`; PYC_CHANGES.md). `struct.unpack('>H8xBB4x')` needed nothing. msp_ss compiles with no warnings; stdout matches CPython; with a port given, both raise `SerialException: could not open port` | `pyc_lib/serial.py`, `01b_bytes.py`, `01_str.py`, `python_ifa_build_if1.cc`, ifa/049 |

(rsync, 2026-10-07: `hashlib.md5` is real now, so the stub's silent `""`
no longer applies.)

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

  **Compiles 2026-10-07.** That later error was `sys.stdin.buffer` /
  `sys.stdout.buffer`: a CPython text file sits on a binary stream,
  `.buffer`, and pyc's file had none. Each text file now carries a
  `__pyc_binfile__` over the same C stream. tarsalzp compiles with no
  diagnostics on both backends; its help output matches CPython (the
  corpus run), and encoding a 229 KB file gives CPython's exact 61916-byte
  stream (5.2 s vs 13.8 s), with both decodes restoring the input, through
  `fi=`/`fo=` and through stdin/stdout.
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
- **voronoi2 (FIXED 2026-10-06 by ifa/049; it compiles and runs): in
  `getopt` with no `longopts`, `_match_long_opt` provably always raises.** Its `possibilities` list is never written, so
  `not possibilities` folds to true. The function returns bottom, and the
  caller's `has_arg, opt = ...` reports it, even though `_do_longs` is
  dead for this argv. This is the ifa/049 shape, and it is 175's measured
  dead end #1 ("gate the continuation of a call on a non-bottom result").
  Passing any non-empty `longopts` makes it compile. A hand-written
  imitation (always-raising callee, `longopts=[]` default) did **not**
  reproduce it, so the trigger is narrower than this description.
  `import getopt, sys; getopt.getopt(sys.argv[1:], "thdp")` alone is the
  reproducer.
- **sunfish (2026-10-07): compiles and matches CPython, past the sweep's
  compile cap.** Four fixes in turn: `dict.clear()`; the multi-index
  subscript `self.tp_score[pos, (depth, root)]`, which produced no index,
  so the transposition table read back only `None`; capturing nested
  generators (its `moves()` inside `bound`), which had no generator wrapper
  and returned their first yielded value; and, on LLVM, a constant-index
  read of a mixed-width record (`(int, bool)` read field 1 at byte offset 1).
  It compiles with no diagnostics and matches CPython on both backends
  (timed half 4.4 s C / 7.6 s LLVM vs 5.0 s), but takes 534 s and 3 GB to
  compile, so the sweep's 400 s cap records a compile timeout. FA does
  converge (47 passes); passes 1-13 cost ~375 s of it. Why it is slow:
  [ifa/185](../ifa/issues/185-FA-merged-tuple-receiver-makes-the-arity-unroll-live.md)
  (a merged tuple receiver keeps the 64-step tuple-method unroll live).
- **rubik (FIXED 2026-10-07: the multi-index subscript `key[a, b]` produced no index; it compiles and matches CPython): `None` reaches `key[1]` in `getCoords`**
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

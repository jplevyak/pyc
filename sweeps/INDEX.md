# Corpus sweep results

Written by `corpus_sweep.sh`. One row per sweep; the file named in each row
has the per-program detail. `tree` is HEAD's short hash, plus a digest of the
uncommitted diff when the sweep ran on a dirty tree.

Check this file before starting a sweep — see CLAUDE.md, "Corpus sweeps".

**Two rows below need a caveat read with them.**

`check__default__e1ba7f10+bb6a70c3` (2026-09-21) was committed as the
evidence for `983ac5a4` (2026-09-24) and **does not describe that tree**.
It was measured three days earlier, against a binary that still had the
comprehension accumulator move `983ac5a4` removed. Two rows are inverted
against the commit it shipped with: it records `plcfrs compile_rc=1` where
that tree compiles it, and `sudoku2 compile_rc=0 stdout=NO` where that
tree fails to compile it. Both match a move-restored build exactly. The
sweep-measures-the-BINARY trap, in the form CLAUDE.md warns about.

**Every `stdout_differs` before `check__default__6c54e1fd` (2026-09-25)
is inflated**, `check__default__a09df260` and `check__default__4c77050e`
among them. issues/163's variance filter learned which lines a program
varies on by diffing TWO CPython runs, and two samples of a continuous
quantity can land on the same string — the filter then learns nothing and
a timing line is scored as a real difference. `confirm_stdout` now takes
a THIRD sample for exactly the programs a `NO` would be reported for.

Measured across the two runs that bracket the fix, on an otherwise
identical tree: **`stdout_differs` 10 → 6, `unverifiable` 3 → 5**, i.e.
**4 of 10 `NO` verdicts were sampling artifacts.**

| program | was | is | why |
| --- | --- | --- | --- |
| `sudoku2` | NO | **yes** | one `TIME %.2f` line; the other 5200 are byte-identical |
| `sieve` | NO | **yes** | `time:` lines only — `nprimes` was always correct |
| `pystone` | NO | **none** | all 11 lines are `benchmarks at N pystones/second` |
| `tictactoe` | NO | **none** | its entire output is one `TIME` line (the `sudoku5` case) |

`plcfrs` stays `NO` and is the control: its difference is
`usage: ./plcfrs` vs `usage: plcfrs.py`, i.e. `sys.argv[0]`, which is
deterministic and no amount of sampling excuses.

*(An earlier version of this note blamed the CPython cache for serving
the second run. It does not — the cache stores and restores both runs, so
a hit still carries two independent samples. The defect was the sample
COUNT. It also put the corrected figure at 8; the measurement found 6.)*

| key | date | result |
|---|---|---|
| `run__default__7ef9bdfe+8082598e` | 2026-10-06 | programs=77 compile_fail=14 run_fail=6 stdout_differs=0 unverifiable=0 with_warnings=18 cs/shapes=2259/711=3.18 pratio=2.15 n=76 -- numeric-coercion warning names the members actually widened ({bool, int} was reported as int/float) and assignment MOVEs carry the target's source location. vs check 7ef9bdfe+ea3bf1ef: every compile rc, run rc and demand column identical; doom 270 -> 271 warnings, all moves corrections: currentLowerCeil/currentUpperFloor from 632 (the loop body's first statement) to their assignments at 639/642, middleTextureY 688 -> 687, and the 688 continuation-line expression no longer hidden behind the mislocated named entry. |
| `check__default__7ef9bdfe+ea3bf1ef` | 2026-10-06 | programs=77 compile_fail=14 run_fail=6 stdout_differs=4 unverifiable=7 with_warnings=18 cs/shapes=2259/711=3.18 pratio=2.15 n=76 -- minpng: struct.pack reads its `*args` record through tuple.__pyc_toints__ (constant-index unroll) instead of `args[ai]`, and inject_tuple_methods' unroll count now pre-scans every imported module (it saw only the main file, so pyc_lib/struct.py's `*args` and any tuple built in an imported module were missed). vs check 3acad6ce+f5f0631b: minpng compiles with no warnings and runs (stdout is a timing only; minpng.png byte-identical to CPython's); sieve NO -> yes is its timing false positive; no other status changes. ess identical except minpng/msp_ss/quameon/sha/sudoku5; css up on programs whose unroll grew (msp_ss 3057 -> 3456, unroll 5 -> 14), because once any scanned module defines `*args`, every call counts, including 14-argument `__pyc_c_call__` intrinsics in __pyc__. CPython cache missed (new tests/*.py are in its key): 6383 s. |
| `check__default__3acad6ce+f5f0631b` | 2026-10-06 | programs=77 compile_fail=15 run_fail=6 stdout_differs=5 unverifiable=6 with_warnings=18 cs/shapes=2257/711=3.17 pratio=2.15 n=76 -- rdb: str.split(sep, maxsplit), bytes.split/find, bool &= and the value-type in-place fallback, file seek/tell, array tolist/fromlist/frombytes/fromfile/slicing/del, and the rdb.py fromstring -> frombytes corpus edit (which invalidated the CPython cache: 6388 s). vs check 3acad6ce+ed60bde8: rdb now compiles (run rc 1 = CPython's, no iPod directory); sieve's stdout NO is a timing-normalization false positive (every non-`time:` line matches; two CPython samples agreed on `time: 0.40`, so it was not masked); no other status changes. |
| `check__default__3acad6ce+ed60bde8` | 2026-10-05 | programs=77 compile_fail=16 run_fail=5 stdout_differs=4 unverifiable=6 with_warnings=18 cs/shapes=2249/712=3.16 pratio=2.14 n=76 -- in-place operators mutate (list `+=`/`*=`, set `|=`/`&=`/`-=`/`^=`), and bool/str/bytes/tuple get the `x = x op y` fallback: vs the same binary on 3acad6ce (row below), every program's compile rc, run rc and stdout verdict identical. CreationSets +11 on nearly every program (the 11 new value-type methods), larger on sat (+35), tictactoe (+49; its existing int/float widening warnings reshuffle, 47 -> 51) and tonyjpegdecoder (+41). |
| `check__default__3acad6ce+c8bd7696` | 2026-10-05 | programs=77 compile_fail=16 run_fail=5 stdout_differs=4 unverifiable=6 with_warnings=18 cs/shapes=2236/712=3.14 pratio=2.13 n=76 -- baseline arm for the in-place operator A/B (row above): 3acad6ce with only untracked files added. run_fail pygasus is its endless loop hitting the run cap. |
| `compile__default__1709fab1+83753197` | 2026-10-05 | programs=77 compile_fail=19 run_fail=0 stdout_differs=0 unverifiable=0 with_warnings=18 cs/shapes=2220/710=3.13 pratio=2.12 n=76 -- A/B against 5eb61058 run back to back: every status identical except life (compiles now). Compile wall 1588 -> 1874 s, but othello3 is 169 -> 400 s only because the wall-clock stall guard trips at different times under load (standalone: 411 s vs 409 s, same single pass). Without othello3: 1419 -> 1474 s (+3.9%), incl. life now compiling and othello2 (8 -> 12 passes, two late TYPE_CONFL single mints). Serial spot checks: go +3%, pylife -1%, tictactoe 0%. |
| `check__default__5eb61058+c61232b1` | 2026-10-05 | programs=77 compile_fail=19 run_fail=4 stdout_differs=4 unverifiable=6 with_warnings=18 cs/shapes=2220/710=3.13 pratio=2.12 n=76 -- tuple.count/index, cross-type `==`, and the FA convergence fixes (post-inline reset, can_raise at convergence, generator-return widening as a flow, make_seq re-run, call gates at quiescence): vs check 26deed74+b16ba512, genetic2 and life now compile (genetic2 -> issues/174, life -> ifa/183, both pre-existing), no other status changes; compile time +20% total, othello3 166 s -> 400 s cap (fails either way). CORRECTED 2026-10-05: the +20% is almost all othello3, whose 166 s was the wall-clock stall guard (ifa/057) tripping early under sweep load; standalone both binaries fail it at ~410 s. See the compile A/B row for 1709fab1. |
| `compile__default__2502f39e+8292fe20` | 2026-10-05 | programs=77 compile_fail=20 run_fail=0 stdout_differs=0 unverifiable=0 with_warnings=18 cs/shapes=2233/709=3.15 pratio=2.14 n=76 -- member violations asked of the converged receiver, not the transient `gates_flow`: vs check 26deed74+b16ba512 every compile status and contour count identical except genetic2, which compiles now from its corpus edit (2502f39e) |
| `check__default__26deed74+b16ba512` | 2026-10-05 | programs=77 compile_fail=21 run_fail=2 stdout_differs=4 unverifiable=6 with_warnings=18 cs/shapes=2233/709=3.15 pratio=2.14 n=76 -- `None.__pyc_getslice__` stub (returned `[]`) deleted: vs 2863ac26+21344585 (same binary) all 77 programs identical in every status column; contour counts move on five, genetic2 CS 473 -> 405 |
| `check__default__2863ac26+21344585` | 2026-10-04 | programs=77 compile_fail=21 run_fail=2 stdout_differs=4 unverifiable=6 with_warnings=18 cs/shapes=2240/709=3.16 pratio=2.15 n=76 -- dispatch/type allocation cuts: vs 33994291+908cb39b all 77 programs identical in every column |
| `check__default__33994291+908cb39b` | 2026-10-04 | programs=77 compile_fail=21 run_fail=2 stdout_differs=4 unverifiable=6 with_warnings=18 cs/shapes=2240/709=3.16 pratio=2.15 n=76 -- pyc built -O3 -march=native by default: vs e4c4dd1c+f5f802d0 (same analysis code, -O0) all 77 programs identical in every column; sweep 554 -> 404 s |
| `check__default__e4c4dd1c+f5f802d0` | 2026-10-04 | programs=77 compile_fail=21 run_fail=2 stdout_differs=4 unverifiable=6 with_warnings=18 cs/shapes=2240/709=3.16 pratio=2.15 n=76 -- dispatch class key drops the type at untyped formals: vs d2c2a42d+5bd13f8c all 77 programs identical in every column; plcfrs FA 168.9 -> 156.0 s serial |
| `check__default__d2c2a42d+5bd13f8c` | 2026-10-03 | programs=77 compile_fail=21 run_fail=2 stdout_differs=4 unverifiable=6 with_warnings=18 cs/shapes=2240/709=3.16 pratio=2.15 n=76 -- issues/123 fix (object.__str__ -> __repr__): vs +83753197 only go flips (stdout matches CPython); every program +1 function |
| `check__default__d2c2a42d+83753197` | 2026-10-03 | programs=77 compile_fail=21 run_fail=2 stdout_differs=5 unverifiable=6 with_warnings=18 cs/shapes=2235/708=3.16 pratio=2.14 n=76 -- issues/123 A/B baseline: d2c2a42d with HEAD's 00_runtime.py |
| `compile__PYC_LLVM_1__f7f51e05+80a05d11` | 2026-10-03 | programs=77 compile_fail=23 run_fail=0 stdout_differs=0 unverifiable=0 with_warnings=14 cs/shapes=2235/708=3.16 pratio=2.14 n=76 -- same tree: ac_encode compiles on LLVM again |
| `check__default__f7f51e05+80a05d11` | 2026-10-03 | programs=77 compile_fail=22 run_fail=2 stdout_differs=4 unverifiable=6 with_warnings=17 cs/shapes=2235/708=3.16 pratio=2.14 n=76 -- ifa/156 fa_split_numeric_confluences + open() raises: vs 27b04a0c no verdict regressed; ac_encode warnings 206 -> 101; doom matches CPython; corpus coercion warnings 678 -> 572, ess +2.5% |
| `compile__PYC_LLVM_1__f7f51e05+ca5fa06a` | 2026-10-03 | programs=77 compile_fail=24 run_fail=0 stdout_differs=0 unverifiable=0 with_warnings=13 cs/shapes=2235/708=3.16 pratio=2.14 n=76 -- INTERMEDIATE: same tree; ac_encode fails on LLVM too |
| `check__default__f7f51e05+ca5fa06a` | 2026-10-03 | programs=77 compile_fail=23 run_fail=2 stdout_differs=4 unverifiable=6 with_warnings=16 cs/shapes=2235/708=3.16 pratio=2.14 n=76 -- INTERMEDIATE: open() raises (CPython OSError family): ac_encode REGRESSED to a compile failure (ifa/156 fragility; fixed in the next row); doom runs and matches (first sweep with the WAD, -C) |
| `compile__PYC_LLVM_1__27b04a0c+8ff82258` | 2026-10-03 | programs=77 compile_fail=23 run_fail=0 stdout_differs=0 unverifiable=0 with_warnings=14 cs/shapes=2236/708=3.16 pratio=2.15 n=76 -- same tree: no LLVM verdict changed; doom still fails to compile on LLVM |
| `check__default__27b04a0c+6e554662` | 2026-10-03 | programs=77 compile_fail=22 run_fail=3 stdout_differs=5 unverifiable=6 with_warnings=17 cs/shapes=2236/708=3.16 pratio=2.15 n=76 -- typed struct.unpack lowering + bytes rstrip/upper/startswith/__contains__/replace: doom COMPILES (160 errors -> 0); its run here is rc=139 without the WAD (CPython: FileNotFoundError, rc=1); nothing else changed |
| `compile__PYC_LLVM_1__a9804c06+a734aca2` | 2026-10-03 | programs=77 compile_fail=23 run_fail=0 stdout_differs=0 unverifiable=0 with_warnings=14 cs/shapes=2182/692=3.15 pratio=2.15 n=76 -- matcher prune_uncoverable: every column identical to 7bc6e0ad+53f8b5e0 |
| `check__default__a9804c06+a734aca2` | 2026-10-03 | programs=77 compile_fail=23 run_fail=2 stdout_differs=5 unverifiable=6 with_warnings=16 cs/shapes=2182/692=3.15 pratio=2.15 n=76 -- matcher prune_uncoverable: every column identical to 7bc6e0ad+53f8b5e0 (verdicts AND contour counts); doom compiles in 2.9s instead of 346s |
| `compile__PYC_LLVM_1__7bc6e0ad+53f8b5e0` | 2026-10-03 | programs=77 compile_fail=23 run_fail=0 stdout_differs=0 unverifiable=0 with_warnings=14 cs/shapes=2182/692=3.15 pratio=2.15 n=76 -- ifa/182: no LLVM compile verdict changed (23) |
| `check__default__7bc6e0ad+53f8b5e0` | 2026-10-03 | programs=77 compile_fail=23 run_fail=2 stdout_differs=5 unverifiable=6 with_warnings=16 cs/shapes=2182/692=3.15 pratio=2.15 n=76 -- ifa/182: pisang runs on the C backend (matches CPython by hand: CPython exceeds the 120s cap); no other verdict changed |
| `compile__PYC_LLVM_1__7bc6e0ad+dd73e4e0` | 2026-10-03 | programs=77 compile_fail=25 run_fail=0 stdout_differs=0 unverifiable=0 with_warnings=13 cs/shapes=2182/692=3.15 pratio=2.15 n=76 -- INTERMEDIATE ifa/182: same chess/quameon compiler crash; fixed in the next row |
| `check__default__7bc6e0ad+0711d702` | 2026-10-03 | programs=77 compile_fail=25 run_fail=2 stdout_differs=5 unverifiable=6 with_warnings=15 cs/shapes=2182/692=3.15 pratio=2.15 n=76 -- INTERMEDIATE ifa/182: chess and quameon segfaulted the compiler (null `has` member reached via element types); fixed in the next row |
| `compile__PYC_LLVM_1__df1ce862+9d28c272` | 2026-10-02 | programs=77 compile_fail=23 run_fail=0 stdout_differs=0 unverifiable=0 with_warnings=14 cs/shapes=2182/692=3.15 pratio=2.15 n=76 -- LLVM None|T -> T representation: pisang compiles (24 -> 23 failures, now the same set as the C backend) |
| `check__default__df1ce862+9d28c272` | 2026-10-02 | programs=77 compile_fail=23 run_fail=3 stdout_differs=5 unverifiable=6 with_warnings=16 cs/shapes=2182/692=3.15 pratio=2.15 n=76 -- C function identity (token for bodiless functions): no verdict changed |
| `compile__PYC_LLVM_1__c479bd6d+3cacacf8` | 2026-10-02 | programs=77 compile_fail=24 run_fail=0 stdout_differs=0 unverifiable=0 with_warnings=14 cs/shapes=2182/692=3.15 pratio=2.15 n=76 -- LLVM backend compile sweep for ifa/181: no program refused by the new error; 24 = the C 23 + pisang (pre-existing unary-minus codegen_fail) |
| `check__default__c479bd6d+fe432547` | 2026-10-02 | programs=77 compile_fail=23 run_fail=3 stdout_differs=5 unverifiable=6 with_warnings=16 cs/shapes=2182/692=3.15 pratio=2.15 n=76 -- ifa/181 (unhandled prim is a compile error; LLVM handles its drops on purpose): no verdict changed |
| `check__default__b56adc1a+67598681` | 2026-10-02 | programs=77 compile_fail=23 run_fail=3 stdout_differs=5 unverifiable=6 with_warnings=16 cs/shapes=2182/692=3.15 pratio=2.15 n=76 -- `**` on both backends (LLVM emitter, exact int pow): no verdict changed vs 628940dc+51566de8 |
| `check__default__628940dc+51566de8` | 2026-10-02 | programs=77 compile_fail=23 run_fail=3 stdout_differs=5 unverifiable=6 with_warnings=16 cs/shapes=2182/692=3.15 pratio=2.15 n=76 -- C backend defaults to -O2 (zero-init temporaries, -fno-strict-aliasing, -fno-builtin-pow, GC-allocated coroutine frames): run_fail 13->3, loop and solitaire now match CPython, 8 former timeouts run; no regressions |
| `check__default__628940dc+1f07480a` | 2026-10-02 | programs=77 compile_fail=23 run_fail=4 stdout_differs=6 unverifiable=5 with_warnings=16 cs/shapes=2182/692=3.15 pratio=2.15 n=76 -- INTERMEDIATE C -O2 default (zero-init + -fno-strict-aliasing only): run_fail 13->4, but nbody (pow builtin) and sudoku5 (GC-unscanned coroutine frames) regressed; both fixed in the next row |
| `check__default__cd183d2b+c80e4372` | 2026-10-02 | programs=77 compile_fail=23 run_fail=13 stdout_differs=5 unverifiable=6 with_warnings=16 cs/shapes=2182/692=3.15 pratio=2.15 n=76 -- one-char string cache: brainfuck runs (116s -> 103s at C -O0, 24.8s -> 12.8s LLVM) and matches CPython; tonyjpegdecoder back to its known NO. No other verdict changed |
| `check__default__92ac5912+65417f57` | 2026-10-02 | programs=77 compile_fail=23 run_fail=15 stdout_differs=4 unverifiable=6 with_warnings=16 cs/shapes=2182/692=3.15 pratio=2.15 n=76 -- list.index start/stop: amaze compiles and matches CPython. run_fail 13->15 is brainfuck and tonyjpegdecoder rc=124 from parallel contention; alone they run in 114s and 93s, rc=0 |
| `check__default__2845e67c+d13b1e24` | 2026-10-02 | programs=77 compile_fail=24 run_fail=13 stdout_differs=5 unverifiable=6 with_warnings=16 cs/shapes=2181/692=3.15 pratio=2.15 n=76 |
| `check__default__5ca26ec4+36c6dfe7` | 2026-10-02 | programs=77 compile_fail=24 run_fail=14 stdout_differs=5 unverifiable=6 with_warnings=16 cs/shapes=2181/692=3.15 pratio=2.15 n=76 |
| `check__PYC_KEEPNIL_1__220794a9+eced1824` | 2026-10-02 | programs=77 compile_fail=24 run_fail=13 stdout_differs=5 unverifiable=6 with_warnings=16 cs/shapes=2181/692=3.15 pratio=2.15 n=76 |
| `check__default__220794a9+eced1824` | 2026-10-02 | programs=77 compile_fail=24 run_fail=13 stdout_differs=5 unverifiable=6 with_warnings=16 cs/shapes=2162/664=3.26 pratio=2.16 n=76 |
| `check__PYC_KEEPNIL_1__220794a9+4ac9b1b8` | 2026-10-02 | programs=77 compile_fail=24 run_fail=14 stdout_differs=5 unverifiable=6 with_warnings=16 cs/shapes=2181/692=3.15 pratio=2.15 n=76 |
| `check__default__220794a9+4ac9b1b8` | 2026-10-02 | programs=77 compile_fail=24 run_fail=14 stdout_differs=5 unverifiable=6 with_warnings=16 cs/shapes=2162/664=3.26 pratio=2.16 n=76 |
| `check__PYC_KEEPNIL_1__220794a9+c2afdd06` | 2026-10-01 | programs=77 compile_fail=24 run_fail=13 stdout_differs=5 unverifiable=6 with_warnings=16 cs/shapes=2193/692=3.17 pratio=2.15 n=76 |
| `check__default__220794a9+c2afdd06` | 2026-10-01 | programs=77 compile_fail=24 run_fail=13 stdout_differs=5 unverifiable=6 with_warnings=16 cs/shapes=2173/664=3.27 pratio=2.16 n=76 |
| `check__PYC_KEEPNIL_1_PYC_NORETURN_1__220794a9+c2afdd06` | 2026-10-01 | programs=77 compile_fail=26 run_fail=12 stdout_differs=5 unverifiable=6 with_warnings=13 cs/shapes=2021/670=3.02 pratio=2.08 n=76 |
| `check__default__164bad43` | 2026-10-01 | programs=77 compile_fail=24 run_fail=13 stdout_differs=4 unverifiable=6 with_warnings=16 cs/shapes=2173/664=3.27 pratio=2.16 n=76 |
| `check__PYC_KEEPNIL_1__e21cc929+d4673d70` | 2026-10-01 | programs=77 compile_fail=26 run_fail=14 stdout_differs=5 unverifiable=6 with_warnings=15 cs/shapes=2196/692=3.17 pratio=2.16 n=76 |
| `check__default__e21cc929+d4673d70` | 2026-10-01 | programs=77 compile_fail=25 run_fail=16 stdout_differs=4 unverifiable=6 with_warnings=16 cs/shapes=2176/664=3.28 pratio=2.16 n=76 |
| `check__default__5b7270b7+fd2e0016` | 2026-09-30 | programs=77 compile_fail=25 run_fail=13 stdout_differs=5 unverifiable=6 with_warnings=16 cs/shapes=2176/664=3.28 pratio=2.16 n=76 |
| `check__default__888c8502+ee4a19cc` | 2026-09-30 | programs=77 compile_fail=25 run_fail=16 stdout_differs=4 unverifiable=6 with_warnings=16 cs/shapes=2176/664=3.28 pratio=2.16 n=76 |
| `check__default__888c8502+a3ef2bed` | 2026-09-30 | programs=77 compile_fail=25 run_fail=15 stdout_differs=5 unverifiable=6 with_warnings=16 cs/shapes=2177/664=3.28 pratio=2.16 n=76 |
| `check__default__11a77416+2772a145` | 2026-09-30 | programs=77 compile_fail=25 run_fail=15 stdout_differs=4 unverifiable=6 with_warnings=16 cs/shapes=2176/664=3.28 pratio=2.16 n=76 |
| `check__default__11a77416+294873b1` | 2026-09-30 | programs=77 compile_fail=26 run_fail=15 stdout_differs=5 unverifiable=6 with_warnings=16 cs/shapes=2176/664=3.28 pratio=2.16 n=76 |
| `check__default__a072e7ab+79351ffb` | 2026-09-30 | programs=77 compile_fail=25 run_fail=15 stdout_differs=4 unverifiable=6 with_warnings=16 cs/shapes=2176/664=3.28 pratio=2.16 n=76 |
| `check__default__16a380f5` | 2026-09-29 | programs=77 compile_fail=24 run_fail=15 stdout_differs=5 unverifiable=6 with_warnings=2 cs/shapes=2155/665=3.24 pratio=2.14 n=76 |
| `check__PYC_STRICT_1__16a380f5+ea69f9b1` | 2026-09-29 | programs=77 compile_fail=41 run_fail=7 stdout_differs=2 unverifiable=4 with_warnings=2 cs/shapes=2084/648=3.22 pratio=2.12 n=74 |
| `check__default__16a380f5+396039f2` | 2026-09-29 | programs=77 compile_fail=25 run_fail=14 stdout_differs=4 unverifiable=6 with_warnings=16 cs/shapes=2156/664=3.25 pratio=2.14 n=76 |
| `check__PYC_STRICT_1__16a380f5+d8d93f70` | 2026-09-29 | programs=77 compile_fail=41 run_fail=8 stdout_differs=2 unverifiable=4 with_warnings=2 cs/shapes=2084/648=3.22 pratio=2.12 n=74 |
| `check__default__16a380f5+ebaed990` | 2026-09-29 | programs=77 compile_fail=25 run_fail=14 stdout_differs=5 unverifiable=6 with_warnings=16 cs/shapes=2156/664=3.25 pratio=2.14 n=76 |
| `check__default__5025025e+811ac4fe` | 2026-09-29 | programs=77 compile_fail=24 run_fail=16 stdout_differs=6 unverifiable=5 with_warnings=2 cs/shapes=2155/665=3.24 pratio=2.14 n=76 |
| `check__default__5025025e+fe09ee15` | 2026-09-28 | programs=77 compile_fail=25 run_fail=14 stdout_differs=5 unverifiable=6 with_warnings=2 cs/shapes=2122/666=3.19 pratio=2.13 n=76 |
| `check__default__4b61e721+d7af0afe` | 2026-09-28 | programs=77 compile_fail=24 run_fail=15 stdout_differs=5 unverifiable=6 with_warnings=2 cs/shapes=2122/666=3.19 pratio=2.13 n=76 |
| `check__default__4ab070d7+a0ef1f9f` | 2026-09-28 | programs=77 compile_fail=24 run_fail=16 stdout_differs=4 unverifiable=6 with_warnings=2 cs/shapes=2096/666=3.15 pratio=2.11 n=76 |
| `check__default__73f273b7+d585e8c2` | 2026-09-28 | programs=77 compile_fail=25 run_fail=16 stdout_differs=5 unverifiable=6 with_warnings=2 cs/shapes=2090/667=3.13 pratio=2.10 n=76 |
| `check__PYC_CSOWNER_1__cf0961f3+678941ba` | 2026-09-28 | programs=77 compile_fail=25 run_fail=16 stdout_differs=5 unverifiable=6 with_warnings=2 cs/shapes=2090/667=3.13 pratio=2.10 n=76 |
| `check__default__cf0961f3+678941ba` | 2026-09-28 | programs=77 compile_fail=26 run_fail=16 stdout_differs=4 unverifiable=6 with_warnings=2 cs/shapes=2060/666=3.09 pratio=2.09 n=76 |
| `check__PYC_CSOWNER_1__0536d85c` | 2026-09-27 | programs=77 compile_fail=25 run_fail=16 stdout_differs=5 unverifiable=6 with_warnings=2 cs/shapes=2082/667=3.12 pratio=2.11 n=76 |
| `check__default__e72775e5+91e709cf` | 2026-09-27 | programs=77 compile_fail=26 run_fail=16 stdout_differs=5 unverifiable=6 with_warnings=2 cs/shapes=2090/667=3.13 pratio=2.10 n=76 |
| `check__default__9dfbf0fc+4690daca` | 2026-09-27 | programs=77 compile_fail=26 run_fail=16 stdout_differs=4 unverifiable=6 with_warnings=2 cs/shapes=1931/639=3.02 pratio=2.05 n=75 |
| `check__default__9dfbf0fc+71fbd58a` | 2026-09-27 | programs=77 compile_fail=28 run_fail=16 stdout_differs=5 unverifiable=5 with_warnings=2 cs/shapes=2020/653=3.09 pratio=2.08 n=75 |
| `check__PYC_CSDEFREUSE_0__9dfbf0fc+71fbd58a` | 2026-09-27 | programs=77 compile_fail=26 run_fail=16 stdout_differs=6 unverifiable=6 with_warnings=2 cs/shapes=1952/645=3.03 pratio=2.06 n=75 |
| `check__PYC_CSPARENTFIRST_0_PYC_CSDEFREUSE_0__9dfbf0fc+71fbd58a` | 2026-09-27 | programs=77 compile_fail=27 run_fail=15 stdout_differs=6 unverifiable=6 with_warnings=2 cs/shapes=2675/669=4.00 pratio=2.66 n=76 |
| `check__default__3b8eb295+4beb38f7` | 2026-09-27 | programs=77 compile_fail=27 run_fail=15 stdout_differs=6 unverifiable=6 with_warnings=2 cs/shapes=2675/669=4.00 pratio=2.66 n=76 |
| `check__default__1c07e76e+630c4ce5` | 2026-09-27 | programs=77 compile_fail=28 run_fail=14 stdout_differs=6 unverifiable=6 with_warnings=2 cs/shapes=2445/669=3.65 pratio=2.44 n=76 |
| `check__default__1c07e76e+e644d637` | 2026-09-27 | programs=77 compile_fail=29 run_fail=15 stdout_differs=5 unverifiable=6 with_warnings=2 cs/shapes=2453/670=3.66 pratio=2.44 n=76 |
| `check__default__7df53702+eb920e2c` | 2026-09-27 | programs=77 compile_fail=28 run_fail=14 stdout_differs=7 unverifiable=6 with_warnings=2 cs/shapes=2430/667=3.64 pratio=2.43 n=76 |
| `check__default__7df53702+15fee959` | 2026-09-27 | programs=77 compile_fail=29 run_fail=14 stdout_differs=6 unverifiable=6 with_warnings=2 cs/shapes=2430/667=3.64 pratio=2.43 n=76 |
| `check__default__fd35a71d+d9edbf6a` | 2026-09-26 | programs=77 compile_fail=28 run_fail=15 stdout_differs=6 unverifiable=6 with_warnings=2 cs/shapes=2425/667=3.64 pratio=2.43 n=76 |
| `check__default__fd35a71d+bbba17b7` | 2026-09-26 | programs=77 compile_fail=28 run_fail=14 stdout_differs=7 unverifiable=6 with_warnings=2 cs/shapes=2425/667=3.64 pratio=2.43 n=76 |
| `check__default__cdff0749+2f333ed4` | 2026-09-26 | programs=77 compile_fail=30 run_fail=14 stdout_differs=6 unverifiable=6 with_warnings=1 cs/shapes=2389/660=3.62 pratio=2.44 n=75 |
| `check__default__752544ed+6e3a0c3d` | 2026-09-26 | programs=77 compile_fail=30 run_fail=15 stdout_differs=6 unverifiable=6 with_warnings=1 cs/shapes=2425/667=3.64 pratio=2.43 n=76 |
| `check__default__752544ed+fc874656` | 2026-09-26 | programs=77 compile_fail=30 run_fail=15 stdout_differs=6 unverifiable=6 with_warnings=1 cs/shapes=2421/663=3.65 pratio=2.43 n=76 |
| `check__default__752544ed+352cc259` | 2026-09-26 | programs=77 compile_fail=31 run_fail=14 stdout_differs=6 unverifiable=6 with_warnings=1 cs/shapes=2420/663=3.65 pratio=2.43 n=76 |
| `check__default__3f1d6f2a+be00839a` | 2026-09-26 | programs=77 compile_fail=31 run_fail=15 stdout_differs=6 unverifiable=6 with_warnings=1 cs/shapes=2359/658=3.59 pratio=2.39 n=76 |
| `check__default__5c7bdd94+a790d1bb` | 2026-09-26 | programs=77 compile_fail=32 run_fail=14 stdout_differs=6 unverifiable=5 with_warnings=1 cs/shapes=2271/639=3.55 pratio=2.38 n=75 |
| `check__default__5c7bdd94+de0dc2e9` | 2026-09-26 | programs=77 compile_fail=32 run_fail=15 stdout_differs=6 unverifiable=5 with_warnings=1 cs/shapes=2422/639=3.79 pratio=2.53 n=75 |
| `check__default__5c7bdd94+16ac439d` | 2026-09-26 | programs=77 compile_fail=33 run_fail=16 stdout_differs=5 unverifiable=4 with_warnings=1 cs/shapes=2484/654=3.80 pratio=2.49 n=75 |
| `check__default__233b9ee4+e9dc9c65` | 2026-09-26 | programs=77 compile_fail=32 run_fail=13 stdout_differs=7 unverifiable=5 with_warnings=1 cs/shapes=2271/639=3.55 pratio=2.38 n=75 |
| `check__default__233b9ee4+376e6830` | 2026-09-26 | programs=77 compile_fail=34 run_fail=15 stdout_differs=5 unverifiable=5 with_warnings=1 cs/shapes=2192/592=3.70 pratio=2.49 n=72 |
| `check__default__233b9ee4+09cf278f` | 2026-09-26 | programs=77 compile_fail=37 run_fail=13 stdout_differs=5 unverifiable=5 with_warnings=1 cs/shapes=2221/588=3.78 pratio=2.54 n=71 |
| `check__default__e70bcffa+eb5931f5` | 2026-09-26 | programs=77 compile_fail=31 run_fail=14 stdout_differs=6 unverifiable=5 with_warnings=1 cs/shapes=2231/641=3.48 pratio=2.34 n=75 |
| `check__default__e70bcffa+dfa959d2` | 2026-09-26 | programs=77 compile_fail=31 run_fail=14 stdout_differs=6 unverifiable=5 with_warnings=1 cs/shapes=2192/627=3.50 pratio=2.34 n=74 |
| `check__default__e70bcffa+a5130efd` | 2026-09-26 | programs=77 compile_fail=31 run_fail=14 stdout_differs=6 unverifiable=5 with_warnings=1 cs/shapes=2228/639=3.49 pratio=2.34 n=75 |
| `check__PYC_ESBLOCK_0__e70bcffa+29542945` | 2026-09-26 | programs=77 compile_fail=33 run_fail=13 stdout_differs=5 unverifiable=5 with_warnings=1 cs/shapes=2196/642=3.42 pratio=2.30 n=75 |
| `check__default__e70bcffa+29542945` | 2026-09-25 | programs=77 compile_fail=32 run_fail=13 stdout_differs=6 unverifiable=5 with_warnings=1 cs/shapes=2228/640=3.48 pratio=2.33 n=75 |
| `check__default__e70bcffa+44a71e1e` | 2026-09-25 | programs=77 compile_fail=35 run_fail=12 stdout_differs=6 unverifiable=5 with_warnings=1 cs/shapes=2219/637=3.48 pratio=2.34 n=74 |
| `check__PYC_XP_STRIPC_1__63888fd6+7e402769` | 2026-09-25 | programs=77 compile_fail=33 run_fail=13 stdout_differs=6 unverifiable=5 with_warnings=1 cs/shapes=2210/637=3.47 pratio=2.33 n=74 |
| `check__default__63888fd6+7e402769` | 2026-09-25 | programs=77 compile_fail=32 run_fail=13 stdout_differs=6 unverifiable=5 with_warnings=1 cs/shapes=2287/642=3.56 pratio=2.37 n=75 |
| `check__PYC_RECOVERLAP_1__63888fd6+25996bae` | 2026-09-25 | programs=77 compile_fail=32 run_fail=12 stdout_differs=7 unverifiable=5 with_warnings=1 cs/shapes=2208/642=3.44 pratio=2.30 n=75 |
| `check__default__63888fd6+25996bae` | 2026-09-25 | programs=77 compile_fail=32 run_fail=13 stdout_differs=6 unverifiable=5 with_warnings=1 cs/shapes=2287/642=3.56 pratio=2.37 n=75 |
| `check__default__89a52795+df0e1c35` | 2026-09-25 | programs=77 compile_fail=32 run_fail=13 stdout_differs=6 unverifiable=5 with_warnings=1 cs/shapes=2287/642=3.56 pratio=2.37 n=75 |
| `check__default__5caf8dc2` | 2026-09-25 | programs=77 compile_fail=32 run_fail=13 stdout_differs=6 unverifiable=5 with_warnings=1 cs/shapes=2287/642=3.56 pratio=2.37 n=75 |
| `check__default__6c54e1fd` | 2026-09-25 | programs=77 compile_fail=32 run_fail=13 stdout_differs=6 unverifiable=5 with_warnings=1 cs/shapes=2287/642=3.56 pratio=2.37 n=75 |
| `check__default__4c77050e` | 2026-09-25 | programs=77 compile_fail=32 run_fail=13 stdout_differs=10 unverifiable=3 with_warnings=1 cs/shapes=2287/642=3.56 pratio=2.37 n=75 |
| `check__default__a09df260` | 2026-09-25 | programs=77 compile_fail=32 run_fail=13 stdout_differs=10 unverifiable=3 with_warnings=1 cs/shapes=2268/642=3.53 pratio=2.37 n=75 |
| `check__default__e1ba7f10+bb6a70c3` | 2026-09-21 | programs=77 compile_fail=33 run_fail=14 stdout_differs=9 unverifiable=3 with_warnings=1 cs/shapes=2104/625=3.37 pratio=2.26 n=76 |
| `check__default__2038b5c7+14838f72` | 2026-09-18 | programs=77 compile_fail=34 run_fail=13 stdout_differs=8 unverifiable=3 with_warnings=1 cs/shapes=2118/625=3.39 pratio=2.27 n=76 |
| `check__default__503b90db+36847e75` | 2026-09-18 | programs=77 compile_fail=34 run_fail=13 stdout_differs=8 unverifiable=3 with_warnings=1 cs/shapes=2118/625=3.39 pratio=2.27 n=76 |
| `check__default__506fc825+87753005` | 2026-09-17 | programs=77 compile_fail=34 run_fail=13 stdout_differs=8 unverifiable=3 with_warnings=1 cs/shapes=2118/625=3.39 pratio=2.27 n=76 |
| `check__default__73f44aa4+9c9c6ccb` | 2026-09-17 | programs=77 compile_fail=35 run_fail=12 stdout_differs=8 unverifiable=3 with_warnings=1 cs/shapes=2118/625=3.39 pratio=2.27 n=76 |
| `check__default__73f44aa4+5c9b1d25` | 2026-09-17 | programs=77 compile_fail=35 run_fail=12 stdout_differs=20 unverifiable=0 with_warnings=1 cs/shapes=2118/625=3.39 pratio=2.27 n=76 |
| `check__default__73f44aa4+b2b7978d` | 2026-09-17 | programs=77 compile_fail=35 run_fail=12 stdout_differs=19 unverifiable=0 with_warnings=1 cs/shapes=2118/625=3.39 pratio=2.27 n=76 |
| `check__default__dc5e44b7+e38f52ae` | 2026-09-17 | programs=77 compile_fail=35 run_fail=12 stdout_differs=19 with_warnings=1 cs/shapes=2054/616=3.33 pratio=2.26 n=76 |
| `check__default__6d60b767+a37a811a` | 2026-09-17 | programs=77 compile_fail=36 run_fail=11 stdout_differs=19 with_warnings=1 cs/shapes=2054/616=3.33 pratio=2.26 n=76 |
| `check__default__728acdb7+c6fc888f` | 2026-09-17 | programs=77 compile_fail=37 run_fail=11 stdout_differs=18 with_warnings=1 cs/shapes=2051/614=3.34 pratio=2.26 n=76 |
| `check__PYC_SETTERGATE_2__f13e1b2e+c47476ae` | 2026-09-16 | programs=77 compile_fail=3 run_fail=36 stdout_differs=24 with_warnings=35 cs/shapes=2694/616=4.37 pratio=2.93 n=76 |
| `check__PYC_SETTERGATE_1__f13e1b2e` | 2026-09-16 | programs=77 compile_fail=2 run_fail=36 stdout_differs=25 with_warnings=37 cs/shapes=2110/617=3.42 pratio=2.32 n=76 |
| `check__default__f13e1b2e` | 2026-09-16 | programs=77 compile_fail=3 run_fail=35 stdout_differs=25 with_warnings=35 cs/shapes=2051/614=3.34 pratio=2.26 n=76 |
| `check__PYC_CSPEEL2_1__77907a01+11b82c01` | 2026-09-15 | programs=77 compile_fail=4 run_fail=33 stdout_differs=26 with_warnings=35 cs/shapes=2245/618=3.63 pratio=2.45 n=76 |
| `check__default__71364627` | 2026-09-15 | programs=77 compile_fail=3 run_fail=36 stdout_differs=24 with_warnings=35 cs/shapes=2051/614=3.34 pratio=2.26 n=76 |
| `check__PYC_ESBLOCK_1__87cc8d60` | 2026-09-15 | programs=77 compile_fail=3 run_fail=35 stdout_differs=25 with_warnings=34 cs/shapes=2073/613=3.38 pratio=2.29 n=76 |
| `check__default__6256be72+4737e9ab` | 2026-09-15 | programs=77 compile_fail=3 run_fail=35 stdout_differs=25 with_warnings=35 cs/shapes=2051/614=3.34 pratio=2.26 n=76 |
| `check__PYC_CSBACKTRACK_1_PYC_CSCONTENT_1__9bc91dba+db09442c` | 2026-09-15 | programs=77 compile_fail=3 run_fail=35 stdout_differs=25 with_warnings=35 cs/shapes=2051/614=3.34 pratio=2.26 n=76 |
| `check__default__9bc91dba+db09442c` | 2026-09-15 | programs=77 compile_fail=8 run_fail=32 stdout_differs=24 with_warnings=32 cs/shapes=2138/629=3.40 pratio=2.31 n=76 |
| `check__PYC_CSCONTENT_1__9bc91dba+db09442c` | 2026-09-15 | programs=77 compile_fail=5 run_fail=35 stdout_differs=24 with_warnings=35 cs/shapes=1987/613=3.24 pratio=2.20 n=76 |
| `check__default__a4b00f2c+f291f78c` | 2026-09-15 | programs=77 compile_fail=8 run_fail=32 stdout_differs=24 with_warnings=32 cs/shapes=2138/629=3.40 pratio=2.31 n=76 |
| `check__PYC_CSBACKTRACK_1__a4b00f2c+62e20b2f` | 2026-09-15 | programs=77 compile_fail=6 run_fail=35 stdout_differs=23 with_warnings=33 cs/shapes=2196/624=3.52 pratio=2.39 n=76 |
| `check__default__a4b00f2c+1cc44e8a` | 2026-09-15 | programs=77 compile_fail=8 run_fail=32 stdout_differs=24 with_warnings=32 cs/shapes=2138/629=3.40 pratio=2.31 n=76 |
| `check__default__eae659d0+abdf2b26` | 2026-09-14 | programs=77 compile_fail=7 run_fail=33 stdout_differs=24 with_warnings=33 cs/shapes=2138/629=3.40 pratio=2.31 n=76 |
| `check__PYC_CSBACKTRACK_1__53719b06+76b5905a` | 2026-09-14 | programs=77 compile_fail=5 run_fail=36 stdout_differs=23 with_warnings=34 cs/shapes=2196/624=3.52 pratio=2.39 n=76 |
| `check__default__53719b06+76b5905a` | 2026-09-14 | programs=77 compile_fail=7 run_fail=33 stdout_differs=24 with_warnings=33 cs/shapes=2138/629=3.40 pratio=2.31 n=76 |
| `check__PYC_CSBACKTRACK_1__53719b06+eef5407d` | 2026-09-14 | programs=77 compile_fail=7 run_fail=34 stdout_differs=23 with_warnings=33 cs/shapes=2130/626=3.40 pratio=2.31 n=76 |
| `check__default__53719b06+f20ea8ee` | 2026-09-14 | programs=77 compile_fail=7 run_fail=33 stdout_differs=24 with_warnings=33 cs/shapes=2138/629=3.40 pratio=2.31 n=76 |
| `compile__PYC_CSKEYSETS_1__04a17bf0+df693813` | 2026-09-14 | programs=77 compile_fail=7 run_fail=0 stdout_differs=0 with_warnings=33 cs/shapes=2267/617=3.67 pratio=2.52 n=76 |
| `compile__IFA_DBG_RECVCARD_1__30713208+77498125` | 2026-09-14 | programs=77 compile_fail=7 run_fail=0 stdout_differs=0 with_warnings=33 cs/shapes=2138/629=3.40 pratio=2.31 n=76 |
| `compile__IFA_DBG_ELEMCONF_1__bac03605+2221ce95` | 2026-09-14 | programs=77 compile_fail=7 run_fail=0 stdout_differs=0 with_warnings=33 cs/shapes=2138/629=3.40 pratio=2.31 n=76 |
| `compile__IFA_DBG_ELEMCONF_1__bac03605+a2a6cafe` | 2026-09-14 | programs=77 compile_fail=7 run_fail=0 stdout_differs=0 with_warnings=33 cs/shapes=2138/629=3.40 pratio=2.31 n=76 |
| `compile__PYC_PROMOTELATE_1__5dbfb6f2+3488e405` | 2026-09-14 | programs=77 compile_fail=8 run_fail=0 stdout_differs=0 with_warnings=29 cs/shapes=2169/640=3.39 pratio=2.33 n=75 |
| `check__default__e9c75656+824cf606` | 2026-09-12 | programs=77 compile_fail=7 run_fail=33 stdout_differs=24 with_warnings=33 cs/shapes=2138/629=3.40 pratio=2.31 n=76 |
| `check__default__de1f14dc+1f4254e8` | 2026-09-12 | programs=77 compile_fail=2 run_fail=34 stdout_differs=27 with_warnings=36 cs/shapes=2768/627=4.41 pratio=2.98 n=76 |
| `compile__default__de1f14dc+1f4254e8` | 2026-09-12 | programs=77 compile_fail=2 run_fail=0 stdout_differs=0 with_warnings=36 cs/shapes=2768/627=4.41 pratio=2.98 n=76 |
| `compile__PYC_CSDCPA1_2__576b4b75` | 2026-09-12 | programs=77 compile_fail=7 run_fail=0 stdout_differs=0 with_warnings=33 cs/shapes=2138/629=3.40 pratio=2.31 n=76 |
| `check__default__b0ee5bb9+0fa284f7` | 2026-09-12 | programs=77 compile_fail=2 run_fail=34 stdout_differs=27 with_warnings=36 cs/shapes=2768/627=4.41 pratio=2.98 n=76 |
| `compile__default__b0ee5bb9+0fa284f7` | 2026-09-12 | programs=77 compile_fail=2 run_fail=0 stdout_differs=0 with_warnings=36 cs/shapes=2768/627=4.41 pratio=2.98 n=76 |
| `check__default__4509936e+12802374` | 2026-09-12 | programs=77 compile_fail=2 run_fail=34 stdout_differs=27 with_warnings=36 cs/shapes=2765/626=4.42 pratio=2.98 n=76 |
| `compile__default__4509936e+12802374` | 2026-09-12 | programs=77 compile_fail=2 run_fail=0 stdout_differs=0 with_warnings=36 cs/shapes=2765/626=4.42 pratio=2.98 n=76 |
| `check__default__2c1a8e8f+e7b8451f` | 2026-09-12 | programs=77 compile_fail=2 run_fail=36 stdout_differs=26 with_warnings=38 cs/shapes=2753/625=4.40 pratio=2.97 n=76 |
| `compile__default__2c1a8e8f+71bec4dd` | 2026-09-12 | programs=77 compile_fail=2 run_fail=0 stdout_differs=0 with_warnings=38 cs/shapes=2753/625=4.40 pratio=2.97 n=76 |
| `check__default__ae80a6ed+240f6fe9` | 2026-09-12 | programs=77 compile_fail=2 run_fail=38 stdout_differs=24 with_warnings=40 cs/shapes=2744/624=4.40 pratio=2.98 n=76 |
| `compile__default__ae80a6ed+7a8b764e` | 2026-09-12 | programs=77 compile_fail=2 run_fail=0 stdout_differs=0 with_warnings=40 cs/shapes=2744/624=4.40 pratio=2.98 n=76 |
| `compile__default__ae80a6ed+8aae3c6b` | 2026-09-11 | programs=77 compile_fail=2 run_fail=0 stdout_differs=0 with_warnings=43 cs/shapes=2740/625=4.38 pratio=2.97 n=76 |
| `compile__default__ae80a6ed+eeea0b01` | 2026-09-11 | programs=77 compile_fail=2 run_fail=0 stdout_differs=0 with_warnings=43 cs/shapes=2740/625=4.38 pratio=2.97 n=76 |
| `compile__PYC_CSDCPA1_2_PYC_CSLADDER_3_PYC_VIOLCS_3_PYC_CSMEMBER_1_PYC_CSCONTENT_1_PYC_WALKCTX_1_PYC_CSCALLSITE_3_PYC_CONTAINERUNION_1__39079fd9+99c0e972` | 2026-09-10 | programs=77 compile_fail=4 run_fail=0 stdout_differs=0 with_warnings=43 cs/shapes=2449/622=3.94 pratio=2.65 n=76 |
| `compile__PYC_CSDCPA1_2_PYC_CSLADDER_3_PYC_VIOLCS_3_PYC_CSMEMBER_1_PYC_CSCONTENT_1_PYC_WALKCTX_1__39079fd9+99c0e972` | 2026-09-10 | programs=77 compile_fail=3 run_fail=0 stdout_differs=0 with_warnings=44 cs/shapes=2403/618=3.89 pratio=2.63 n=76 |
| `compile__PYC_CSCALLSITE_3_PYC_CONTAINERUNION_1__39079fd9+99c0e972` | 2026-09-10 | programs=77 compile_fail=3 run_fail=0 stdout_differs=0 with_warnings=42 cs/shapes=2723/625=4.36 pratio=2.96 n=76 |
| `compile__default__39079fd9+99c0e972` | 2026-09-10 | programs=77 compile_fail=2 run_fail=0 stdout_differs=0 with_warnings=43 cs/shapes=2740/625=4.38 pratio=2.97 n=76 |
| `compile__PYC_CSDCPA1_2_PYC_CSLADDER_3_PYC_VIOLCS_3_PYC_CSMEMBER_1_PYC_CSCONTENT_1_PYC_WALKCTX_1_PYC_CSCALLSITE_3_PYC_CONTAINERUNION_1__39079fd9+d47b9ae7` | 2026-09-10 | programs=77 compile_fail=5 run_fail=0 stdout_differs=0 with_warnings=43 cs/shapes=2490/623=4.00 pratio=2.69 n=76 |
| `compile__PYC_CSDCPA1_2_PYC_CSLADDER_3_PYC_VIOLCS_3_PYC_CSMEMBER_1_PYC_CSCONTENT_1_PYC_WALKCTX_1__39079fd9+474d0ecd` | 2026-09-10 | programs=77 compile_fail=3 run_fail=0 stdout_differs=0 with_warnings=44 cs/shapes=2403/618=3.89 pratio=2.63 n=76 |
| `compile__PYC_CSCALLSITE_3_PYC_CONTAINERUNION_1__39079fd9+474d0ecd` | 2026-09-10 | programs=77 compile_fail=3 run_fail=0 stdout_differs=0 with_warnings=42 cs/shapes=2724/625=4.36 pratio=2.96 n=76 |
| `compile__default__39079fd9+474d0ecd` | 2026-09-10 | programs=77 compile_fail=2 run_fail=0 stdout_differs=0 with_warnings=43 cs/shapes=2740/625=4.38 pratio=2.97 n=76 |
| `compile__PYC_CSDCPA1_2_PYC_CSLADDER_3_PYC_VIOLCS_3_PYC_CSMEMBER_1_PYC_CSCONTENT_1_PYC_WALKCTX_1__b847e122+a1c45574` | 2026-09-10 | programs=77 compile_fail=3 run_fail=0 stdout_differs=0 with_warnings=44 cs/shapes=2403/618=3.89 pratio=2.63 n=76 |
| `compile__PYC_CSDCPA1_2_PYC_CSLADDER_3_PYC_VIOLCS_3_PYC_CSMEMBER_1_PYC_CSCONTENT_1__b847e122+a1c45574` | 2026-09-10 | programs=77 compile_fail=2 run_fail=0 stdout_differs=0 with_warnings=45 cs/shapes=2406/617=3.90 pratio=2.64 n=76 |
| `compile__PYC_WALKCTX_1__b847e122+a1c45574` | 2026-09-10 | programs=77 compile_fail=2 run_fail=0 stdout_differs=0 with_warnings=43 cs/shapes=2749/625=4.40 pratio=2.98 n=76 |
| `compile__default__b847e122+a1c45574` | 2026-09-10 | programs=77 compile_fail=2 run_fail=0 stdout_differs=0 with_warnings=43 cs/shapes=2740/625=4.38 pratio=2.97 n=76 |
| `compile__PYC_WALKCTX_1__b847e122+4c0f07cf` | 2026-09-10 | programs=77 compile_fail=3 run_fail=0 stdout_differs=0 with_warnings=42 cs/shapes=2743/627=4.37 pratio=2.98 n=76 |
| `compile__default__b847e122+153e711e` | 2026-09-10 | programs=77 compile_fail=2 run_fail=0 stdout_differs=0 with_warnings=43 cs/shapes=2740/625=4.38 pratio=2.97 n=76 |
| `compile__PYC_VIOLCS_3_PYC_CSMEMBER_1_PYC_CSCONTENT_1__5cb76a2b+09c163b7` | 2026-09-10 | programs=77 compile_fail=2 run_fail=0 stdout_differs=0 with_warnings=44 cs/shapes=2826/625=4.52 pratio=3.03 n=76 |
| `compile__PYC_CSDCPA1_2_PYC_CSLADDER_3_PYC_VIOLCS_3_PYC_CSMEMBER_1_PYC_CSCONTENT_1__5cb76a2b+09c163b7` | 2026-09-10 | programs=77 compile_fail=2 run_fail=0 stdout_differs=0 with_warnings=45 cs/shapes=2406/617=3.90 pratio=2.64 n=76 |
| `compile__PYC_CSDCPA1_2_PYC_CSLADDER_3_PYC_VIOLCS_3_PYC_CSMEMBER_1__af467129+3e48560d` | 2026-09-10 | programs=77 compile_fail=3 run_fail=0 stdout_differs=0 with_warnings=44 cs/shapes=2505/624=4.01 pratio=2.73 n=76 |
| `compile__PYC_CSDCPA1_2_PYC_CSLADDER_3__af467129+3e48560d` | 2026-09-10 | programs=77 compile_fail=7 run_fail=0 stdout_differs=0 with_warnings=39 cs/shapes=2091/626=3.34 pratio=2.27 n=76 |
| `compile__PYC_VIOLCS_3_PYC_CSMEMBER_1__af467129+3e48560d` | 2026-09-10 | programs=77 compile_fail=2 run_fail=0 stdout_differs=0 with_warnings=44 cs/shapes=2816/625=4.51 pratio=3.02 n=76 |
| `compile__default__af467129+3e48560d` | 2026-09-10 | programs=77 compile_fail=2 run_fail=0 stdout_differs=0 with_warnings=43 cs/shapes=2740/625=4.38 pratio=2.97 n=76 |
| `check__PYC_CSDCPA1_2_PYC_CSLADDER_3_PYC_NILSTORE_0__ae16c44e+0e9deefa` | 2026-09-09 | programs=77 compile_fail=6 run_fail=38 stdout_differs=22 with_warnings=39 cs/shapes=2080/622=3.34 pratio=2.27 n=76 |
| `check__default__ae16c44e+0e9deefa` | 2026-09-09 | programs=77 compile_fail=2 run_fail=38 stdout_differs=24 with_warnings=43 cs/shapes=2740/625=4.38 pratio=2.97 n=76 |
| `check__PYC_NILSTORE_0__ae16c44e+0e9deefa` | 2026-09-09 | programs=77 compile_fail=2 run_fail=38 stdout_differs=24 with_warnings=43 cs/shapes=2740/625=4.38 pratio=2.97 n=76 |
| `run__PYC_CSLADDER_3__c9d75201+3e48560d` | 2026-09-09 | programs=77 compile_fail=2 run_fail=38 stdout_differs=0 with_warnings=43 cs/shapes=2740/625=4.38 pratio=2.97 n=76 |
| `run__default__c9d75201+3e48560d` | 2026-09-09 | programs=77 compile_fail=2 run_fail=38 stdout_differs=0 with_warnings=43 cs/shapes=2740/625=4.38 pratio=2.97 n=76 |
| `run__PYC_CSDCPA1_2_PYC_CSLADDER_3__4b255036+34b996d8` | 2026-09-08 | programs=77 compile_fail=6 run_fail=38 stdout_differs=0 with_warnings=39 cs/shapes=2080/622=3.34 pratio=2.27 n=76 |
| `run__default__4b255036+34b996d8` | 2026-09-08 | programs=77 compile_fail=2 run_fail=38 stdout_differs=0 with_warnings=43 cs/shapes=2740/625=4.38 pratio=2.97 n=76 |
| `run__PYC_CSDCPA1_2_PYC_CSLADDER_3__7392cc64+3e48560d` | 2026-09-08 | programs=77 compile_fail=7 run_fail=37 stdout_differs=0 with_warnings=39 cs/shapes=2151/620=3.47 pratio=2.35 n=76 |
| `run__default__fb66657f+da5596e9` | 2026-09-08 | programs=77 compile_fail=2 run_fail=38 stdout_differs=0 with_warnings=43 cs/shapes=2736/621=4.41 pratio=2.98 n=76 |
| `run__PYC_NOMARKSETTER_1__fb04429f+acdd4e77` | 2026-09-08 | programs=77 compile_fail=3 run_fail=38 stdout_differs=0 with_warnings=42 cs/shapes=2727/621=4.39 pratio=2.97 n=76 |
| `run__PYC_HARDREUSE_4__1fb1a7fa+ce202159` | 2026-09-08 | programs=77 compile_fail=7 run_fail=35 stdout_differs=0 with_warnings=38 cs/shapes=2713/622=4.36 pratio=2.94 n=76 |
| `run__PYC_NOTYPEMARKS_1__1fb1a7fa+612fad5c` | 2026-09-08 | programs=77 compile_fail=2 run_fail=38 stdout_differs=0 with_warnings=43 cs/shapes=2736/621=4.41 pratio=2.98 n=76 |
| `run__default__8231193a+dd84ee63` | 2026-09-08 | programs=77 compile_fail=2 run_fail=39 stdout_differs=0 with_warnings=43 cs/shapes=2736/621=4.41 pratio=2.98 n=76 |
| `run__PYC_NOMARK_2__8231193a+3e48560d` | 2026-09-08 | programs=77 compile_fail=2 run_fail=38 stdout_differs=0 with_warnings=43 cs/shapes=2736/621=4.41 pratio=2.98 n=76 |
| `run__PYC_CSDCPA1_2_PYC_CSLADDER_3__4467f71d+e06e125d` | 2026-09-08 | programs=77 compile_fail=7 run_fail=37 stdout_differs=0 with_warnings=39 cs/shapes=2153/622=3.46 pratio=2.35 n=76 |
| `run__default__4467f71d+e06e125d` | 2026-09-08 | programs=77 compile_fail=2 run_fail=38 stdout_differs=0 with_warnings=43 cs/shapes=2736/621=4.41 pratio=2.98 n=76 |
| `run__PYC_CSSPLIT_0__d0780ee8+3e48560d` | 2026-09-08 | programs=77 compile_fail=2 run_fail=38 stdout_differs=0 with_warnings=43 cs/shapes=2762/621=4.45 pratio=2.99 n=76 |
| `check__default__5a411e2d+cb3eef98` | 2026-09-08 | programs=77 compile_fail=2 run_fail=39 stdout_differs=24 with_warnings=44 cs/shapes=3713/626=5.93 pratio=3.89 n=76 |
| `check__default__4a149bc0+3e48560d` | 2026-09-08 | programs=77 compile_fail=2 run_fail=39 stdout_differs=24 with_warnings=44 cs/shapes=3713/626=5.93 pratio=3.89 n=76 |
| `check__PYC_CSDCPA1_2_PYC_CSLADDER_3__3e8dcb10+9bc3c367` | 2026-09-07 | programs=77 compile_fail=3 run_fail=39 stdout_differs=24 with_warnings=43 cs/shapes=2342/610=3.84 pratio=2.58 n=76 |
| `check__default__3e8dcb10+9bc3c367` | 2026-09-07 | programs=77 compile_fail=2 run_fail=39 stdout_differs=24 with_warnings=44 cs/shapes=3713/626=5.93 pratio=3.89 n=76 |
| `check__default__8a9e56e6+33825165` | 2026-09-07 | programs=77 compile_fail=2 run_fail=39 stdout_differs=24 with_warnings=44 cs/shapes=3713/626=5.93 pratio=3.89 n=76 |
| `check__PYC_CSDCPA1_2_PYC_CSLADDER_3_PYC_CSDEFPART_2__8a9e56e6+c003c82d` | 2026-09-07 | programs=77 compile_fail=3 run_fail=39 stdout_differs=24 with_warnings=43 cs/shapes=2328/609=3.82 pratio=2.57 n=76 |
| `check__PYC_CSDCPA1_2_PYC_CSLADDER_3__8a9e56e6+c0bc742e` | 2026-09-07 | programs=77 compile_fail=6 run_fail=39 stdout_differs=22 with_warnings=41 cs/shapes=2375/617=3.85 pratio=2.60 n=76 |
| `check__default__8a9e56e6+c0bc742e` | 2026-09-07 | programs=77 compile_fail=2 run_fail=39 stdout_differs=24 with_warnings=44 cs/shapes=3713/626=5.93 pratio=3.89 n=76 |
| `check__PYC_CSDCPA1_2_PYC_CSLADDER_3__20f76f27+a206be07` | 2026-09-07 | programs=77 compile_fail=4 run_fail=39 stdout_differs=23 with_warnings=42 cs/shapes=3281/622=5.27 pratio=3.55 n=76 |
| `check__default__20f76f27+a206be07` | 2026-09-07 | programs=77 compile_fail=2 run_fail=39 stdout_differs=24 with_warnings=44 cs/shapes=3713/626=5.93 pratio=3.89 n=76 |
| `check__PYC_CSDCPA1_2_PYC_CSLADDER_3_PYC_CSDEFSPLIT_2__20f76f27+62a5a8d4` | 2026-09-07 | programs=77 compile_fail=4 run_fail=39 stdout_differs=23 with_warnings=42 cs/shapes=3260/627=5.20 pratio=3.49 n=76 |
| `check__PYC_CSDCPA1_2_PYC_CSLADDER_3__20f76f27+62a5a8d4` | 2026-09-07 | programs=77 compile_fail=5 run_fail=38 stdout_differs=23 with_warnings=41 cs/shapes=3269/616=5.31 pratio=3.52 n=76 |
| `check__default__20f76f27+1e42a1e5` | 2026-09-07 | programs=77 compile_fail=2 run_fail=39 stdout_differs=24 with_warnings=44 cs/shapes=3713/626=5.93 pratio=3.89 n=76 |
| `check__default__cbc105e3+b208dad9` | 2026-09-07 | programs=77 compile_fail=2 run_fail=39 stdout_differs=24 with_warnings=44 cs/shapes=3713/626=5.93 pratio=3.89 n=76 |
| `check__PYC_CSDCPA1_2_PYC_CSLADDER_3_PYC_CSDEFSPLIT_2__818790f0+f3cb2d55` | 2026-09-07 | programs=77 compile_fail=4 run_fail=39 stdout_differs=23 with_warnings=42 cs/shapes=3273/627=5.22 pratio=3.45 n=76 |
| `check__PYC_CSDCPA1_2_PYC_CSLADDER_3__e40a5fcd+f3cb2d55` | 2026-09-07 | programs=77 compile_fail=9 run_fail=35 stdout_differs=23 with_warnings=37 cs/shapes=2835/595=4.76 pratio=3.15 n=76 |
| `check__PYC_CSDCPA1_2_PYC_CSLADDER_3_PYC_CSCALLSITE_2__1ab67469+f04b8e21` | 2026-09-06 | programs=77 compile_fail=10 run_fail=36 stdout_differs=21 with_warnings=37 cs/shapes=2838/590=4.81 pratio=3.15 n=76 |
| `check__PYC_CSDCPA1_2_PYC_CSLADDER_3__7c40f15c+a1e42239` | 2026-09-06 | programs=77 compile_fail=10 run_fail=35 stdout_differs=22 with_warnings=36 cs/shapes=2812/594=4.73 pratio=3.09 n=76 |
| `check__PYC_CSDCPA1_2_PYC_CSLADDER_3__7c40f15c+ab997699` | 2026-09-06 | programs=77 compile_fail=10 run_fail=35 stdout_differs=22 with_warnings=36 cs/shapes=2812/594=4.73 pratio=3.09 n=76 |
| `check__PYC_CSDCPA1_2_PYC_CSLADDER_3__83e0e25b+9ecbbb68` | 2026-09-06 | programs=77 compile_fail=12 run_fail=36 stdout_differs=20 with_warnings=35 cs/shapes=2825/602=4.69 pratio=3.03 n=76 |
| `check__PYC_CSDCPA1_2_PYC_CSLADDER_3__27ffb6e5+a58dbd70` | 2026-09-06 | programs=77 compile_fail=12 run_fail=35 stdout_differs=23 with_warnings=37 cs/shapes=2910/599=4.86 pratio=3.17 n=76 |
| `check__PYC_CSDCPA1_2_PYC_CSLADDER_1__27ffb6e5+bb63d8e1` | 2026-09-06 | programs=77 compile_fail=13 run_fail=35 stdout_differs=22 with_warnings=35 cs/shapes=2826/584=4.84 pratio=3.10 n=76 |
| `check__PYC_CSDCPA1_2__c81a7b1a+f3cb2d55` | 2026-09-06 | programs=77 compile_fail=12 run_fail=35 stdout_differs=23 with_warnings=37 cs/shapes=2955/604=4.89 pratio=3.20 n=76 |
| `check__default__af7176ef+ac5f4016` | 2026-09-06 | programs=77 compile_fail=2 run_fail=39 stdout_differs=24 with_warnings=44 cs/shapes=3713/626=5.93 pratio=3.89 n=76 |
| `check__PYC_CSDCPA1_2_PYC_CSDEFSPLIT_0__88c773c9+f3cb2d55` | 2026-09-06 | programs=77 compile_fail=16 run_fail=35 stdout_differs=20 with_warnings=39 cs/shapes=2708/591=4.58 pratio=2.99 n=76 |
| `check__PYC_CSDCPA1_2__88c773c9+f3cb2d55` | 2026-09-06 | programs=77 compile_fail=16 run_fail=34 stdout_differs=20 with_warnings=38 cs/shapes=2764/591=4.68 pratio=3.04 n=76 |
| `check__default__ece3a980+a471d532` | 2026-09-06 | programs=77 compile_fail=2 run_fail=39 stdout_differs=24 with_warnings=44 cs/shapes=3713/626=5.93 pratio=3.89 n=76 |
| `check__PYC_CSDCPA1_2__5e012d78+8450a439` | 2026-09-05 | programs=77 compile_fail=19 run_fail=34 stdout_differs=18 with_warnings=36 cs/shapes=2716/595=4.56 pratio=2.97 n=76 |
| `check__PYC_CSDCPA1_2__3c388f22+adf4abe8` | 2026-09-05 | programs=77 compile_fail=20 run_fail=33 stdout_differs=17 with_warnings=33 cs/shapes=2540/570=4.46 pratio=2.89 n=74 |
| `compile__PYC_CSELEM_3__fc25f947+ca955581` | 2026-09-05 | programs=77 compile_fail=3 run_fail=0 stdout_differs=0 with_warnings=43 cs/shapes=3060/626=4.89 pratio=3.23 n=76 |
| `compile__PYC_CSELEM_3__0ec1632e+c4905f52` | 2026-09-05 | programs=77 compile_fail=3 run_fail=0 stdout_differs=0 with_warnings=43 cs/shapes=3060/626=4.89 pratio=3.23 n=76 |
| `check__PYC_CSELEM_3_PYC_CSREJOIN_0__7e05207f+037f2f8f` | 2026-09-04 | programs=77 compile_fail=3 run_fail=38 stdout_differs=24 with_warnings=43 cs/shapes=3081/626=4.92 pratio=3.25 n=76 |
| `check__PYC_CSELEM_3__7e05207f+037f2f8f` | 2026-09-04 | programs=77 compile_fail=3 run_fail=38 stdout_differs=24 with_warnings=43 cs/shapes=3060/626=4.89 pratio=3.23 n=76 |
| `compile__default__8cceaa08+2e452100` | 2026-09-04 | programs=77 compile_fail=2 run_fail=0 stdout_differs=0 with_warnings=44 cs/shapes=3748/626=5.99 pratio=3.92 n=76 |
| `check__PYC_CSELEM_3__de9fca7d+adf4abe8` | 2026-09-04 | programs=77 compile_fail=3 run_fail=38 stdout_differs=24 with_warnings=43 cs/shapes=3081/626=4.92 pratio=3.25 n=76 |
| `check__PYC_CSELEM_3__40c21ff9+adf4abe8` | 2026-09-04 | programs=77 compile_fail=3 run_fail=38 stdout_differs=24 with_warnings=43 cs/shapes=3081/626=4.92 pratio=3.25 n=76 |
| `check__default__a935532b+adf4abe8` | 2026-09-04 | programs=77 compile_fail=2 run_fail=39 stdout_differs=24 with_warnings=44 cs/shapes=3748/626=5.99 pratio=3.92 n=76 |
| `check__default__7a1823c4` | 2026-09-04 | programs=77 compile_fail=2 run_fail=39 stdout_differs=24 with_warnings=44 cs/shapes=3748/626=5.99 pratio=3.92 n=76 |
| `check__default__5cf5baf7+1a013d49` | 2026-09-04 | programs=77 compile_fail=2 run_fail=39 stdout_differs=24 with_warnings=44 cs/shapes=3748/626=5.99 pratio=3.92 n=76 |
| `check__default__ff308aa5` | 2026-09-04 | programs=77 compile_fail=2 run_fail=39 stdout_differs=24 with_warnings=44 |
| `check__PYC_ELIDE_SLOTS_1__c82a99a8` | 2026-09-03 | programs=77 compile_fail=2 run_fail=40 stdout_differs=24 with_warnings=44 |
| `check__PYC_ELIDE_SLOTS_1__ed9c5829` | 2026-09-03 | programs=77 compile_fail=2 run_fail=43 stdout_differs=23 with_warnings=44 |
| `check__PYC_CLASSEQ_2_PYC_PREFIX_LAYOUT_1__d2742efa` | 2026-09-03 | programs=77 compile_fail=2 run_fail=43 stdout_differs=24 with_warnings=44 |
| `check__PYC_CLASSEQ_2_PYC_PREFIX_LAYOUT_1_PYC_ELIDE_SLOTS_1__2d89043e` | 2026-09-03 | programs=77 compile_fail=8 run_fail=36 stdout_differs=24 with_warnings=39 |
| `check__default__2d89043e` | 2026-09-03 | programs=77 compile_fail=3 run_fail=43 stdout_differs=24 with_warnings=43 |
| `check__default__4b6a5f40+49d048d5` | 2026-09-03 | programs=77 compile_fail=4 run_fail=42 stdout_differs=24 with_warnings=43 |
| `check__default__635d26b6+c1a28ccc` | 2026-09-01 | programs=77 compile_fail=5 run_fail=42 stdout_differs=23 with_warnings=42 |
| `check__default__e3fd890d+8e66a2b0` | 2026-09-01 | programs=77 compile_fail=3 run_fail=44 stdout_differs=23 with_warnings=44 |
| `check__default__b073a011+6142c9e1` | 2026-09-01 | programs=77 compile_fail=3 run_fail=44 stdout_differs=23 with_warnings=44 |
| `check__default__b0aa9f0b+e265215f` | 2026-09-01 | programs=77 compile_fail=3 run_fail=44 stdout_differs=23 with_warnings=44 |
| `check__PYC_CLONE_CSEQ_1__b0aa9f0b+e265215f` | 2026-09-01 | programs=77 compile_fail=4 run_fail=44 stdout_differs=22 with_warnings=43 |
| `check__default__9a2ddd0d+06523fde` | 2026-09-01 | programs=77 compile_fail=4 run_fail=43 stdout_differs=23 with_warnings=43 |
| `check__default__028c1150+7d0964f7` | 2026-08-31 | programs=77 compile_fail=5 run_fail=42 stdout_differs=23 with_warnings=42 |
| `check__default__c8fbb054+2b9aa817` | 2026-08-31 | programs=77 compile_fail=5 run_fail=41 stdout_differs=23 with_warnings=42 |
| `check__default__de4ea252+36eaaedb` | 2026-08-30 | programs=77 compile_fail=5 run_fail=41 stdout_differs=23 with_warnings=42 |
| `check__default__de4ea252` | 2026-08-30 | programs=77 compile_fail=5 run_fail=42 stdout_differs=23 with_warnings=42 |
| `check__default__f2501586` | 2026-08-29 | programs=77 compile_fail=5 run_fail=42 stdout_differs=23 with_warnings=42 |
| `check__default__f2501586+0a43ff92` | 2026-08-29 | programs=77 compile_fail=5 run_fail=41 stdout_differs=23 with_warnings=42 |

## Backfilled from the 2026-08-28 session

These predate the script, so they have no `.tsv` here — they were run by
ad-hoc scripts in a session scratch directory that is gone. Recorded so the
measurements are not repeated, with the commit whose content each ran
against.

| what | commit | result |
|---|---|---|
| compile, default | `39274bf0` (PYC_CSMOLD=3 default) | 5 fail: chess, go, linalg, othello3, sudoku5 |
| compile, `PYC_CSMOLD=3` vs default | pre-`39274bf0`, dirty | identical program for program **— but see the warning below** |
| compile, `PYC_CSELEM=3`, unbounded shape work | pre-`22f42cca`, dirty | 11 fail (+adatron, kanoodle, othello, rdb timeouts; plcfrs, quameon) |
| compile, `PYC_CSELEM=3`, shape memoized + width-capped | `22f42cca` | 9 fail (+kanoodle, plcfrs, rdb timeouts; quameon) |
| check (warnings + run rc + stdout vs CPython), default | `22f42cca` | see `check__default__22f42cca.tsv` |

**The `PYC_CSMOLD=3` A/B was run as two CONCURRENT sweeps and its one
apparent difference was an artifact.** `ac_encode` came back `rc=1` in the
baseline arm and `rc=0` in the test arm; re-run alone under the baseline it
is `rc=0`. Two sweeps contending for the machine produce spurious 400s
timeouts. Run them one at a time — the script does not enforce this.

## The 2026-08-29 A/B: issues/119, unrolled tuple `__str__`/`__hash__`

The two `f2501586` rows above are one A/B — baseline is clean `f2501586`,
the `+0a43ff92` arm is the same tree plus the issues/119 fix (unrolled
`tuple.__str__`/`__hash__`, `PYC_TUPLE_AS_LIST` defaulted on). Run
SEQUENTIALLY, with a rebuild between arms and a positive control
confirming the baseline binary still reproduced the bug.

Diffing the TSVs program-by-program, **all 77 programs are identical
except one line**:

```
< richards   0  4  134  124  -      baseline: SIGABRT
> richards   0  4    0  124  -      with fix: exit 0
```

**That is not a win, and the totals mislead.** `richards` prints `False`
ten times and `TIME 0.00`; CPython prints `True`. It went from a loud
abort to a SILENT WRONG ANSWER — the shape ifa/102 is about. It escaped
the `stdout_differs` column only because CPython itself times out on
richards (`cpy_rc=124`), so the sweep never compared the output.

It is not new wrong logic: richards has no dict, no set, no `hash()`, and
never prints a tuple, so the unrolled methods cannot change its
semantics. The baseline aborted in a polymorphic dispatch with `no branch
matched`, so that arm could not have produced the right answer either.
Both arms are wrong; only the failure mode moved. Filed as issues/120.

### Two traps this A/B walked into, for whoever runs the next one

- **`git stash -u` eats the result you just measured.** A finished
  `.tsv` is untracked, so stashing to build the baseline arm swept it
  away, and the pop then conflicted because BOTH arms had overwritten the
  same tracked corpus outputs (`shedskin_examples/**/*.ppm`, `.bmp`) and
  `INDEX.md`. Commit or copy the `.tsv` out before stashing.
- **A sweep dirties the working tree**, which changes the tree key. Any
  edit — even to `corpus_sweep.sh` itself — makes the next invocation
  MISS the cache and silently start a fresh 40-minute run. Check that a
  repeat prints `cached:` and nothing else.

## The 2026-08-30 A/B: ifa/112's `remove_unused_closures` `return` -> `break`

The two `de4ea252` rows are one A/B. Baseline is clean HEAD; the
`+36eaaedb` arm changes the `return` at `fa.cc:9382` to `break`, so that
EVERY AVar of a Var gets its unused closures cleaned rather than only
the first one reached. Run sequentially, with a rebuild between arms.

Program by program across all 77, the diff is **one line**:

```
score4   baseline run_rc=124 (timeout)   break run_rc=0 (completes)
```

**That difference is NOISE, not an effect.** Re-run alone under the SAME
(break) build, score4 gives `rc=124`, `rc=0`, `rc=124` across three
runs: its runtime straddles the 120s `-t` boundary, and CPython times
out on it too (`cpy_rc=124`, so there is no stdout oracle either). Same
shape as the `ac_encode` artifact recorded above, with one difference —
these arms were run SEQUENTIALLY, so contention is not the cause.
score4 simply sits on the limit.

So the A/B is **neutral**: no real change in compile status, warning
counts, run status or stdout anywhere in the corpus. `break` does change
the emitted C (32 structural lines on msp_ss, all additional getters,
41367 -> 41383 lines) — it is just not a change the corpus can observe.

Worth knowing for the next A/B: a program whose runtime is near `-t`
flips on its own. Check any single-program difference by re-running that
program alone, several times, under ONE build, before attributing it to
the change under test.

## The 2026-08-31 A/B: ifa/098's second defect (silent dispatch failures)

Baseline is `check__default__de4ea252+36eaaedb`, whose tree content is
identical to clean HEAD `c8fbb054` (the `+36eaaedb` arm IS what
`c8fbb054` committed) — so no baseline arm had to be re-run. The test arm
`c8fbb054+2b9aa817` adds ifa/098's `dispatched_this_pass` fix to
`collect_argument_type_violations`: an `out_edge_map` entry no longer
counts as "dispatched" unless one of its edges is in the per-pass
`EntrySet::out_edges`.

Totals are identical (`compile_fail=5 run_fail=41 stdout_differs=23
with_warnings=42`), and **program by program every difference is in the
`warns` column alone** — `compile_rc`, `run_rc`, `cpy_rc` and
`stdout_match` match on all 77. Warnings rise on 20 programs, 1615 →
2040 corpus-wide (`rubik` 67 → 176, `doom` 92 → 210, `plcfrs` 82 → 123).

That is the intended shape: the change surfaces dispatch failures that
were previously swallowed. It is *not* purely cosmetic, though —
`fa->type_violations.set_count()` gates the splitter's self-product
eviction — which is why the run/stdout columns were the ones to check,
and why `-m compile` would not have been evidence.

## 2026-08-31: corpus_sweep.sh went parallel, and CPython results are cached

`check` went from **~40 minutes to ~11** (657 s warm), and the result is
not a different measurement — validated against the serial script on the
same tree and the same `pyc` binary, **76 of 77 programs byte-identical**.

Where the time goes now, and what bounds it:

| phase | wall | bound by |
|---|---|---|
| compile, `-j32` | 311 s | `othello3` ALONE — it takes 306 s to fail |
| run, `-J8` | 334 s | the ten binaries that sit on the 120 s cap |
| CPython, `-J8` | **0 s** | 72/72 cache hits |

Total 646 s. Whether the compile phase can overlap the run phase is the
only remaining lever, and it is not obviously worth taking: `othello3`
alone is 306 of the 311 s, and running binaries under a 32-wide compile
is exactly the contention the confirmation rule below exists to detect.

The CPython cache lives in `sweeps/cpython-cache/<key>/` (gitignored, 17
MB), keyed on the corpus tree hash + uncommitted `**/*.py` + the python3
version. It is worth having because **19 of the 77 programs time out
under CPython** — 38 minutes per `check` sweep spent re-deriving a
constant that only a corpus change can move. `-C` forces a re-run.

### The one differing program, and what it taught

```
score4   serial baseline run_rc=0   parallel run_rc=124
```

Not parallelism. Re-run ALONE under this build, score4 gives `rc=124`
three times out of three (plus the run inside the confirmation pass), and
the 2026-08-30 A/B above already recorded it flipping 124/0/124 alone. It
straddles the cap. The *serial baseline's* `0` was the outlier.

### Why the confirmation rule is "rc=124 only"

The first attempt re-ran anything that used more than half its timeout:
**51 programs, 89 minutes, one finding.** The rule is now the useful half
of that. A parallel pass can only turn a completion into a timeout, never
the reverse, so `rc=124` is the only verdict contention can fabricate.

The one real fabrication it caught is worth recording: at `-J8`, CPython
reported `hq2x` as `rc=124`; alone it finishes in **116 s of a 120 s
cap**. No `-J` setting fixes a program that close to the line — only the
alone-run does. And a fabricated CPython timeout is not cosmetic: it
drops the program out of the stdout comparison entirely (`stdout_match`
becomes `-`), so `hq2x` would have silently stopped being checked. That
is why CPython's timeouts are confirmed unconditionally while the pyc
side is `-R`: the CPython answer is cached, so it is paid once per
corpus, and the pyc side measured **0 fabrications in 72 programs**.

### The cache never hit, in any session, ever

Found while testing the above, and older than any of it. The tree key is
`sha1(git diff HEAD + git status --porcelain)`, and **finishing a sweep
mutates all of its own inputs**: it writes a new untracked
`sweeps/*.tsv`, appends a row to the tracked `sweeps/INDEX.md`, and lets
every corpus binary rewrite its own output files (`chaos/py.ppm`,
`tonyjpegdecoder/tiger1.bmp`, `oliva2/oliva.pgm`, …, all tracked). So the
key computed on the next invocation never matched the one just recorded —
measured directly: three different digests off one unchanged source tree.
Every "repeat" was a fresh 40-minute sweep.

The key now excludes `sweeps/` and everything under `shedskin_examples/`
that is not a `.py`. A corpus SOURCE edit still invalidates it; a corpus
OUTPUT does not. Proven end to end: two `-m check` runs back to back, no
edits between them — 646 s, then `cached:` instantly.

One trap for whoever touches this next: it needs TWO `git` invocations,
not one clever pathspec. Git applies every `:!` exclusion *after* all
inclusions, so `-- . ':!shedskin_examples' ':(glob)shedskin_examples/**/*.py'`
silently drops the re-include and a corpus source edit stops
invalidating the key. The "must differ" case is what caught it.

### …and then the commit orphaned it anyway

Fixing the above exposed the other half. The tree key answers a HUMAN's
question — *which commit was this measured against?* — and it therefore
changes when you **commit**. So the ten minutes you just spent measuring
a change were thrown away by the very commit that landed it. Reproduced
directly: commit, re-run, watch a full sweep start.

There is now a second key answering the machine's question — *is the
thing under test the same?* — written into every new TSV as
`# content <digest>`, over the `pyc` binary (libifa is linked into it),
`__pyc__/*.py` (read at run time, not linked), every corpus `*.py`, the
`-e` overrides, the mode, and **both timeouts** — a `-t 20` sweep is not
the same measurement as a `-t 120` one, and the tree key never noticed.
Lookup tries the exact filename first, then any same-mode/same-env TSV
carrying the same content digest, reporting which one it matched.

It is deliberately conservative. `make clean` re-stamps `BUILD_VERSION`
into `version.o` and changes the binary with no source change, costing a
needless re-measure. A false MISS wastes time; a false HIT would report
a stale answer as current.

Filenames are unchanged, so nothing in this directory moved and the
five older TSVs still resolve by tree name exactly as before — they
simply carry no content line to match on.

**"Never run two sweeps concurrently" still holds** — more so now, since
one sweep already uses the whole machine.

## The 2026-09-01 A/B: ifa/121's codegen DCE

Baseline `check__default__028c1150+7d0964f7`, test arm
`check__default__9a2ddd0d+06523fde`. The change emits each function body
into its own buffer and writes out only what `init` transitively names.

Program by program across all 77, the diff is **one line, and it is a
win**:

```
< linalg   compile_rc=1   (6 C errors, all in functions this drops)
> linalg   compile_rc=0   run_rc=134
```

`linalg` was one of the five corpus compile failures; its errors
(`no matching function for call to '_CG_list_mult_internal'`) were all
inside emitted-but-unreferenced clones. **Compile failures 5 → 4.** It
still aborts at run time — ifa/102's class, expected for a program that
does not converge. `run_fail` 42 → 43 and `with_warnings` 42 → 43 are
the same event: linalg now produces a binary and a warning count where
before it produced neither.

Everything else — `run_rc`, `cpy_rc`, `stdout_match`, warning counts —
is identical on all 77 programs.

## The 2026-09-01 A/B: ifa/121's two clone-equivalence changes

Both arms ran on one tree; the second differs only by `PYC_CLONE_CSEQ=1`.
They answer different questions and got opposite verdicts.

### A — `prim_period_offset` answers instead of aborting. LANDED.

`check__default__b0aa9f0b+e265215f` vs `check__default__9a2ddd0d+06523fde`,
**one line**:

```
< go   compile_rc=1   (fail: missmatched offsets)
> go   compile_rc=0   run_rc=139
```

`prim_period_offset` has exactly one caller — `ES_FN::equivalent` — and it
called `fail()` when a union receiver's member classes disagreed about a
field's offset, killing the compile from inside an equivalence QUESTION.
`go` is the only corpus program that hits it. It now returns a
`kOffsetAmbiguous` sentinel and the predicate answers "not equivalent",
which is conservative: strictly more splitting than a merge would be.
Compile failures **4 → 3**. `go` then core-dumps, so it moves into
ifa/102's compiles-then-crashes bucket rather than becoming a working
program — an honest improvement, not a win.

### B — let the `cssyms` loop decide clone equivalence. REJECTED.

`check__PYC_CLONE_CSEQ_1__b0aa9f0b+e265215f`, same tree plus the
coarsening, **two lines** — one of them a regression:

```
  go        compile_rc=1 -> 0     (this is A's doing, not B's)
  voronoi2  compile_rc=0 -> 1     REGRESSION
```

`voronoi2` compiled AND ran at the baseline; under B it fails with
`no matching function for call to '_CG_f_16022_133'` — issue 097's exact
signature, a call site whose argument type diverges from the merged
callee's formal. So CreationSet equivalence plus the offset check is
still not sufficient evidence to merge two contours.

It was tempting: pygmy goes 244 → 183 clones with a byte-identical
rendered image, and both e2e suites stay at 308/0 (only two ifa-test
goldens move, `clone` and `dce` on `iterator_missing_field`, `funs=4 → 3`
— `codegen-c` is unchanged, because the merged clone was one the
ifa/121 DCE already dropped). **The test suites do not see this
regression at all.** Only the corpus does, which is the whole argument
for running it on anything touching clone equivalence.

## 2026-09-01: zero-width struct placeholders (ifa/121)

`check__default__b073a011+6142c9e1` vs `check__default__b0aa9f0b+e265215f`:
**identical on all 77 programs**, no column changed.

The struct emitter must emit a member for every `has` index — eliding one
breaks the `eN` numbering that several access sites compute independently
— but the typeless ones do not need STORAGE. `_CG_void eN;` became
`char eN[0];`: **7078 placeholders across 60 programs, 56624 bytes of
struct storage removed**, pygmy's rendered image byte-identical.

## 2026-09-01: ifa/122's layout check made fatal

`check__default__635d26b6+c1a28ccc` vs `check__default__e3fd890d+8e66a2b0`,
a two-line diff and both lines are the intended trade:

```
< bh   compile_rc=0  run_rc=139     > bh   compile_rc=1
< go   compile_rc=0  run_rc=139     > go   compile_rc=1
```

`compile_fail` **3 → 5**, `run_fail` **44 → 42**. Nothing else in the
corpus trips the check — the same two programs the census found, and no
surprises from making it an error.

This is a deliberate regression in the "programs that build" column and
not a regression in anything that worked: both already produced a
segfault (`go`) and a corrupted heap (`bh`), and ifa/123 traced `go`'s
crash to exactly the construct the check names. Unlike a type violation,
a layout violation has no permissive meaning — there is no runtime check
to insert, only a program that reads one class's field through another's
layout — so it is fatal in every mode rather than gated on
`fruntime_errors`.

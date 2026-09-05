# Corpus sweep results

Written by `corpus_sweep.sh`. One row per sweep; the file named in each row
has the per-program detail. `tree` is HEAD's short hash, plus a digest of the
uncommitted diff when the sweep ran on a dirty tree.

Check this file before starting a sweep — see CLAUDE.md, "Corpus sweeps".

| key | date | result |
|---|---|---|
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

# Corpus sweep results

Written by `corpus_sweep.sh`. One row per sweep; the file named in each row
has the per-program detail. `tree` is HEAD's short hash, plus a digest of the
uncommitted diff when the sweep ran on a dirty tree.

Check this file before starting a sweep — see CLAUDE.md, "Corpus sweeps".

| key | date | result |
|---|---|---|
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

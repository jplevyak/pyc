# Corpus sweep results

Written by `corpus_sweep.sh`. One row per sweep; the file named in each row
has the per-program detail. `tree` is HEAD's short hash, plus a digest of the
uncommitted diff when the sweep ran on a dirty tree.

Check this file before starting a sweep — see CLAUDE.md, "Corpus sweeps".

| key | date | result |
|---|---|---|

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

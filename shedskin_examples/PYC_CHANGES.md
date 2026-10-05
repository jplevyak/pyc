# pyc's changes to this corpus, and the policy for making them

*(This file is pyc's. `README.md` beside it is upstream shedskin's program
index and is deliberately left untouched — this directory is a git subtree,
and editing upstream files invites merge conflicts.)*

This directory is a git **subtree** of shedskin's `examples/` (squashed —
see the note in the project memory: do not rebase across its merge commit).
It is pyc's compile/run/output benchmark, driven by `./corpus_sweep.sh`.

## Why it is edited at all

**pyc's goal is to compile well-formed CPython programs with the SAME
SEMANTICS, for the subset of those programs that can be made monotonic** —
statically typeable without boxing. It is not to compile all Python, and it
is **not to match shedskin**. shedskin is a reference for mechanism, never
for behaviour, and it deviates from CPython where that suits it.

That last point is why editing is sometimes the honest answer rather than a
dodge. "shedskin compiles all 77 without boxing, therefore all 77 are
typeable as written" does **not** follow, because shedskin's answer is not
always CPython's answer. When a program genuinely holds two basic types in
one slot, no analysis can fix it — the choices are boxing (which pyc does
not do), a semantic deviation (which the goal forbids), or saying in the
source what the program already means.

See CLAUDE.md, "The goal: CPython semantics, not shedskin's".

## The policy

**Author's directive, 2026-09-28:** a corpus program that has an ACTUAL
type error may be updated when the error cannot be reasonably
accommodated, and **every accommodation pyc makes is non-strict only.**

An *actual type error* is one that belongs to the program, not to pyc:

- CPython 3 itself raises on it, e.g. `raise "message"` is a
  `TypeError` (Python-2 idiom), or an operation on a value of the wrong
  type;
- or one slot genuinely holds two types that no unboxed representation
  can carry, over the program's lifetime and not because pyc merged
  anything. `bh`'s `Vec3`, holding `float` and then `floor(...)`'s `int`,
  is the standing example.

For such a program there are exactly two routes:

1. **A pyc accommodation**, when a reasonable one exists: wrapping a
   string exception, widening an `int` to `float`, a typed default for an
   implicit `None`. Every accommodation is a Python-permissive feature,
   so it must:
   - be behind a NAMED flag (`PYC_...`), enabled by the permissive
     default and **disabled by `--strict`**;
   - report itself (a warning naming the deviation) whenever it changes
     what the program would do under CPython;
   - be listed in [issues/171](../issues/closed/171-permissive-accommodations-must-be-flagged-and-non-strict.md).
   Under `--strict` the program is refused with a diagnostic that names
   the type error.
2. **A source edit**, when no reasonable accommodation exists, or when
   the accommodation would change what a correct program prints. The edit
   is the minimal change that removes the type error, written in terms of
   what the program evidently means.

An edit to a corpus program is allowed **only** when all of these hold:

1. **It is either a verified CPython no-op, or it removes an actual type
   error.** For a no-op edit, run CPython on the file before and after and
   diff the output. For a type-error edit, CPython output must be
   identical on every path that does not reach the error, and the edit
   must say what the erroneous path now does and why.
2. **It says what the code already means.** The edit makes an existing
   invariant explicit, or states the intent the erroneous code had. It
   does not change an algorithm, a data structure, or an interface.
3. **No accommodation would do instead without a semantic cost.** If a
   flag exists but changes what something MEANS program-wide, the source
   fix is preferred: it is local and costs no divergence.
4. **The reason is a comment in the file**, at the edit, naming the pyc
   issue and saying whether it is a no-op or a type-error fix. Someone
   reading the corpus must not have to guess why it differs from upstream.
5. **It is recorded in the table below.**

**What is NOT allowed: editing around a pyc inference deficiency.** If pyc
invented a union the program does not have, that is pyc's bug; see
AGENTS.md, "Boxing is never the answer for a corpus program". The test is
whether CPython itself raises, or would have to box the value. If it
would not, the program is correct and pyc must be fixed.

**Candidates known today** (not yet edited; see issues/171):
`chess/chess.py:92,128` and `minilight/ml/entry.py:77` use `raise "..."`.
pyc accommodates this by wrapping the string in `Exception(...)`. Since
2026-09-28 it does so only in permissive mode and with a warning; `--strict`
refuses. Editing these sites to `raise Exception("...")` would make both
programs compile under `--strict`.

**Editing any corpus `.py` invalidates the sweep cache** (both the tree key
and the content key), which is intended: results measured on the old source
are not results for the new one.

**Budget for it.** The CPython cache key covers the corpus tree hash plus
*every* `**/*.py`, so touching ONE file invalidates CPython results for all
77 programs, not just the edited one. They are then re-run cold, and per
`corpus_sweep.sh`'s confirmation rule every `rc=124` is re-taken ALONE
afterwards — measured on the `bh` edit: 20 timeouts, each a solo 120 s
run. A normally ~12-minute `check` sweep took over an hour. Plan the edit
and its sweep together rather than discovering this mid-run.

## Changes from upstream

| program | change | why | commit |
| --- | --- | --- | --- |
| `chess/chess.py` | `printBoard` restored | it was commented out in upstream, and mis-translated | `9abf39f9` |
| `chess/chess.py` | explicit `return False` in `rowAttack` | the `for` could fall off the end, an implicit `return None` against two explicit `bool` returns, giving `{bool, None}` — 1-byte bool unioned with 8-byte None, no unboxed representation (ifa/118). The path is dead (every 0x88 ray leaves the board), only ray arithmetic proves it, and CPython would `TypeError` inside `max()` if it were reached. The alternative was `PYC_NO_IMPLICIT_NONE`, which is global and changes what `None` means program-wide to buy one program's compile. | `3cfdbd7d` |
| `adatron/adatron.py` | explicit `return 0.0` after `calculate_error`'s loop | the loop's `return` is indented inside it, so the function can fall off the end, and its result is `{float, None}`, which has no representation (issues/048). The loop always runs, so the line is unreachable and CPython's output is unchanged. Before: compiled and aborted (`matching function not found`); after issues/171's unified diagnostic, refused at compile time; with the edit it compiles and its output matches CPython up to the run cap. | this commit |
| `bh/bh.py` | `float(...)` around `floor(...)`, 6 sites | `Vec3.__init__` writes `self.d0 = 0.0` (float) and `xp[0] = floor(...)` writes an **int into the same object** — CPython 3's `math.floor` returns `int`. One object holding float then int over its lifetime is *temporal*, not per-object, so no contour split can separate it; the field's static type is `{int, float}`, which has no unboxed representation. Every read goes through `int(...)`, `Node.IMAX` is 2**30 and `0.0 <= xsc < 1.0`, so the value is exact in a double and `int(float(floor(x))) == floor(x)`. Verified: CPython output byte-identical before and after. See ifa/144, ifa/145. | this commit |
| `genetic2/genetic2.py` | `raise ValueError("bad opcode")` after `TreeNode.execute`'s `if/elif` chain | the chain has no `else`, so `execute` can fall off the end, an implicit `return None` against `int` returns, giving `{int, None}` (issues/048), which reaches `&`/`|`/`^` and is refused as a BOXING violation in every mode. `opcode` is only ever `OPCODE_NONE..OPCODE_IF` (`randint(OPCODE_AND, OPCODE_IF)` or the default), so the path is dead, and CPython would `TypeError` inside `&` if it were reached; the `raise` says so. Same shape as `chess`'s `rowAttack`, and `PYC_NO_IMPLICIT_NONE` was rejected for the same reason. Verified: CPython output identical before and after; pyc now compiles it with no warnings in default and `--strict`, and all 101 `Epoch:` lines match CPython (the final genome print still crashes: issues/174). | this commit |
| `doom/DOOM1.WAD` | data file ADDED (no `.py` edit) | `doom.py` opens `DOOM1.WAD` from its own directory, and upstream's README says to download it, so without it the program could only be compiled, never run or compared. This is the id Software shareware DOOM1.WAD v1.9 (SHA-1 `5b2e249b9c5133ec987b3ea77596381dc0d6bc1d`, 4,196,020 bytes, 1264 lumps), which id licensed for unmodified redistribution. doom's `__main__` renders one frame and prints nothing, so its stdout check is trivially equal; a scratch driver checksumming 8 rendered frames matched CPython byte for byte. The sweep's CPython cache keys on the corpus `.py` files, NOT on data files, so the first sweep after this needs `-C`. | this commit |

Also `34fc4fb5`, which converted 20 files from CRLF to LF — whitespace
only, listed here so the diff against upstream is fully accounted for.

### A note on `bh` specifically

shedskin compiles `bh.py` **unedited**, and that is not evidence pyc is
missing something. shedskin's `math.floor` returns `__ss_float`
(`shedskin/lib/math/__init__.hpp:36`); CPython 3's returns `int`; pyc
matches CPython (`pyc_lib/math.py:32`). So shedskin never creates the mix
in the first place — its clean typing of this file is bought by a semantic
deviation, not by better analysis. Matching it by widening `floor` would
trade a real divergence for a contour win, which the goal forbids.

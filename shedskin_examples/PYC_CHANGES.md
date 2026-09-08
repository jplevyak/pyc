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

An edit to a corpus program is allowed **only** when all of these hold:

1. **It is semantically a no-op under CPython.** Not "close enough" —
   verified, by running CPython on the file before and after and diffing
   the output.
2. **It says what the code already means.** The edit makes an existing
   invariant explicit; it does not change an algorithm, a data structure,
   or an interface.
3. **No compiler flag would do instead without a semantic cost.** If a
   global flag exists but changes what something MEANS program-wide, the
   source fix is preferred — it is local and costs no divergence.
4. **The reason is a comment in the file**, at the edit, naming the pyc
   issue and why it is a no-op. Someone reading the corpus must not have to
   guess why it differs from upstream.
5. **It is recorded in the table below.**

What is NOT allowed: editing around a pyc inference deficiency. If pyc
invented a union the program does not have, that is pyc's bug — see
CLAUDE.md's "Boxing is never the answer for a corpus program". The test is
whether CPython itself would have to box the value.

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
| `bh/bh.py` | `float(...)` around `floor(...)`, 6 sites | `Vec3.__init__` writes `self.d0 = 0.0` (float) and `xp[0] = floor(...)` writes an **int into the same object** — CPython 3's `math.floor` returns `int`. One object holding float then int over its lifetime is *temporal*, not per-object, so no contour split can separate it; the field's static type is `{int, float}`, which has no unboxed representation. Every read goes through `int(...)`, `Node.IMAX` is 2**30 and `0.0 <= xsc < 1.0`, so the value is exact in a double and `int(float(floor(x))) == floor(x)`. Verified: CPython output byte-identical before and after. See ifa/144, ifa/145. | this commit |

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

# Issue 021: `PycScope::map` hashes on pointer value, causing run-to-run nondeterminism in the frontend

**Status:** closed — the final known surface-level instance of pointer-hashing nondeterminism (`EdgeHash` in `ifa/analysis/fa.h`) was fixed by introducing an ID-based `AEdgeHashFns`. As recommended, full build reproducibility (i.e. replacing pointer hashes in the remaining ~150+ sites) is formally deferred to `ifa/issues/010-vec-set-api-cleanup.md`'s planned codebase audit.
**Affects:** `python_ifa_int.h:20` (`PycScope::map`, a
`Map<cchar *, PycSymbol *>`) and every unsorted iteration over it
throughout `python_ifa_build_syms.cc`/`python_ifa_build_if1.cc`;
root mechanism is `ifa/common/map.h:32`
(`MapElem::operator uintptr_t()` returns `(uintptr_t)key` — the
*pointer value* of the `cchar*` key, not a hash of its string
content).
**Related:** this is the pyc-frontend-side instance of the same
general bug class as `ifa/issues/closed/009-fa-violations-nondeterminism.md`
and `ifa/notes/004-plib-vec-pointer-set-hashing.md` — but that
work fixed ifa's own `Vec`-as-set usage (CreationSets/AVars inside
FA); it did **not** touch `PycScope::map`, which is pyc-frontend
code, a different subsystem. One narrow, targeted fix for this
exists already (`compar_pycsymbol_by_name` in
`python_ifa_build_syms.cc`, added for class-attribute struct-field
ordering specifically) but the underlying pointer-hashed `Map` is
used far more broadly than that one call site.

## Symptom

Confirmed directly: `tests/expr_evaluator.py` (an existing,
otherwise-stable test) failed to **compile** on the v2 LLVM backend
exactly once during this session, with zero source or compiler
changes in between — three immediate reruns (and several more over
the course of the session) all passed cleanly. This is the
textbook signature of heap-layout/ASLR-dependent iteration-order
nondeterminism, not a real regression (ruled out by bisecting: the
same pyc binary, same test file, produced both outcomes across
separate process invocations).

A comment already in the tree independently confirms the mechanism
and that it's known to be broader than the one fix that landed for
it (`python_ifa_build_syms.cc:6-16`, on `compar_pycsymbol_by_name`):

> Without this sort, struct field order is non-deterministic across
> runs — the underlying `Map<cchar*, PycSymbol*>` hashes on
> key-string POINTER values, which vary with GC/heap layout between
> processes ... Each compile stays internally consistent, but pyc's
> generated .c / .ll differs across runs of byte-identical input,
> breaking build reproducibility and any future golden-file diff on
> emitted code.

That comment cites "issue 016 follow-up" as where this was meant to
be tracked — no such issue exists under that number in either
`issues/` or `ifa/issues/` (016 in this directory is about missing
grammar syntax; `ifa/issues/closed/016` is about LLVM SSU formal-arg
binding, unrelated). The reference appears to have gone stale
between when the comment was written and when issues actually got
filed under real numbers — this issue is that tracking, filed now.

## Root cause (confirmed)

```cpp
// ifa/common/map.h
template <class K, class C>
class MapElem : public gc {
 public:
  K key;
  C value;
  operator uintptr_t(void) { return (uintptr_t)(uintptr_t)key; }
  ...
};
```

`Map<K, C, A>` (`ifa/common/map.h:42`) extends
`Vec<MapElem<K, C>, A>` and relies on `Vec`'s generic open-addressed
set-hashing, which uses `MapElem`'s `operator uintptr_t()` as the
hash/bucket key. For `Map<cchar *, PycSymbol *>` (`PycScope::map`),
that conversion operator returns the **raw pointer value** of the
`cchar *` key — not a hash of the string's bytes. `ifa/common/map.h`
*does* separately define `StringHashFns` (a proper byte-based
string hash, used elsewhere e.g. for `ChainHash`), but
`PycScope::map`'s declaration
(`Map<cchar *, PycSymbol *> map;` in `python_ifa_int.h:20`) uses no
custom hash-function template parameter, so it goes through the
generic pointer-keyed path.

Every scope in `PycCompiler::scope_stack` — module, class, and
function scopes alike — uses this exact map for name resolution.
Any code that iterates a scope's `map` directly (via `form_Map(...)`
or similar) without first collecting into a `Vec` and sorting it
gets bucket order that depends on the **addresses** the interned
name strings happened to land at, which varies by process (ASLR,
allocator/GC state) even for byte-identical source. `gen_class_pyda`
already does exactly this dance for class attributes (collect into
a `Vec<PycSymbol *>`, `qsort` with `compar_pycsymbol_by_name`,
*then* commit to `cls_sym->has` in sorted order) — but that's the
**only** audited call site. Anywhere else in the frontend that walks
a scope map without the same collect-then-sort discipline is still
exposed.

## Proposed fix sketch

Two levels, cheapest first:

1. **Audit and sort at each consumption site** (mirrors the existing
   `gen_class_pyda` fix, and mirrors options A+B's *effect* for ifa's
   own `Vec`-as-set, though not its *mechanism*): find every
   `form_Map`/direct-iteration site over a `PycScope::map` (or any
   other pointer-keyed `Map` in the pyc frontend) that doesn't already
   collect-and-sort, and add the same discipline.
2. **Fix the root cause once, for every consumer**: give
   `PycScope::map` (and any other `Map<cchar *, ...>` in the pyc
   frontend) a proper content-based hash — either parameterize `Map`
   to accept `StringHashFns` (already defined in `ifa/common/map.h`)
   for `cchar*` keys, or intern names through a representation whose
   `operator uintptr_t()` is stable (e.g. a hash of the string
   computed once at intern time, stored alongside the pointer).
   Removes the need to audit/sort at every call site ever again —
   the same rationale ifa's own issue 010 (Task A, `Vec::capacity`
   rename) gives for wanting the compile-time-enforced version over
   ad-hoc per-site fixes.

(2) is the more valuable fix long-term (matches the "fix it once"
philosophy of the ifa-side work), but is a broader change touching
`ifa/common/map.h`'s template surface, which is shared with the ifa
library — needs care not to disturb `ChainHash`/other `Map`
consumers that already rely on the pointer-hash behavior
intentionally (e.g. genuine pointer-identity maps, not string-keyed
ones).

## Verification plan

1. Compile the same source file (e.g. `tests/expr_evaluator.py`)
   on the LLVM backend N times in a fresh process each time (not a
   loop within one process — ASLR is per-`exec`, not per-iteration);
   generated `.ll` should be byte-identical across all N runs.
2. Repeat for the C backend's generated `.c`.
3. Full `./test_pyc` and `PYC_FLAGS="-b" ./test_pyc` runs stay
   green across multiple invocations (the flake, being
   probabilistic, may need several repeated full-suite runs to
   confirm it's gone — a single clean run doesn't prove the fix,
   consistent with how this was originally missed).
4. `ifa`'s own test suite (`./ifa-test`, all phases) is unaffected
   (this issue is scoped to the pyc frontend's `PycScope::map`, not
   ifa's internals) — a sanity check to confirm the two are
   genuinely independent, not another manifestation of the same bug.

## What this unblocks

Reproducible builds (byte-identical output for byte-identical
input) and flake-free CI — right now, a test suite run can fail
for reasons entirely unrelated to the change under review, which
erodes trust in "the suite is green" and wastes time re-running or
bisecting phantom failures. Directly motivated by losing time this
session to exactly that: one anomalous `expr_evaluator.py` LLVM
failure that took several reruns to confirm was not a real
regression.

## First fix landed: `PycScope::map` → `HashMap<StringHashFns>`

`python_ifa_int.h`'s `PycScope::map` changed from
`Map<cchar *, PycSymbol *>` to
`HashMap<cchar *, StringHashFns, PycSymbol *>` — content-based
hashing instead of pointer-value hashing, matching the sketch in
"Proposed fix sketch" (2) above. Verified: full `./test_pyc` and
`PYC_FLAGS="-b" ./test_pyc`, 115/0 both backends, no regressions.

## Second investigation: this alone does not achieve determinism

Directly testing the stated goal (byte-identical generated code
across repeated process invocations of the same source) showed the
`PycScope::map` fix, while correct and worth keeping, is nowhere
near sufficient:

- `tests/dict_basic.py` compiled 8 times (C backend) still produced
  2 distinct outputs (down from ~5 distinct/5 runs pre-fix) —
  varying only in the *declaration order* of a few local temp
  variables within one function.
- `tests/expr_evaluator.py` compiled 8 times (LLVM backend, `-b`)
  produced **8 distinct outputs** — no improvement at all on this
  fixture. Diffing two runs showed the actual struct **type id**
  differing (`%Expr.4311` vs `%Expr.4305`) and function-symbol
  numeric suffixes differing by inconsistent offsets
  (`_CG_f_3824_32` vs `_CG_f_3824_54`, `_CG_f_4057_44` vs
  `_CG_f_4057_66`, etc.).

A follow-up fix was applied for one confirmed root cause: `Var`
(`ifa/if1/var.h`) has a monotonic `int id` field but was never
given a `PointerHash<Var *>` specialization when
[`ifa/notes/004-plib-vec-pointer-set-hashing.md`](../ifa/notes/004-plib-vec-pointer-set-hashing.md)'s
"options A+B" landed — the note's own list of six specialized
types (`AVar`, `AEdge`, `EntrySet`, `CreationSet`, `Sym`, `Fun`)
omits `Var` entirely. `collect_types_and_globals`
(`ifa/analysis/fa.cc:4676-4705`) builds the C backend's emitted
global-variable list via `globals.set_add(v)` on `Vec<Var *>`,
finalized via `set_to_vec()` (bucket order, no sort) — exactly the
"g&lt;N&gt;" numbering bug diffed in `dict_basic.py.c`. Fix: added
`template <> struct PointerHash<Var *>` in `ifa/if1/var.h`, hashing
on `c->id`, mirroring the existing five specializations' pattern
exactly. Verified safe (115/0 both backends, no regressions).

**But this still didn't close the gap** (`expr_evaluator.py` is
still 8/8 distinct). The deeper problem: id-based hashing only
produces stable iteration order if the **ids themselves are
assigned in a stable order** across runs — and that's not
guaranteed. `Sym`/`Var`/`Fun` ids are minted monotonically at
construction time, during FA's cloning/specialization passes
(`ifa/analysis/clone.cc`, `ifa/analysis/fa.cc`), which themselves
walk dozens of other `Vec<Ptr *>::set_add`-populated sets over
pointer types that have **no** `PointerHash` specialization and no
stable id field at all — `PNode *` (`ifa/optimize/cfg.cc`,
`ifa/codegen/cg.cc`), `Dom *` (`ifa/optimize/dom.cc`), `CallPoint *`
(`ifa/common/html.cc`, `ifa/if1/fun.cc`), `MatchCacheEntry *`
(`ifa/if1/pattern.cc`), `llvm::Value *` (`ifa/codegen/cg_emit_llvm.cc`),
among others. A grep across `ifa/` found ~150+ `set_add`/`set_in`
call sites beyond the six (now seven) types already fixed. If *any*
upstream traversal that decides cloning/specialization order reads
one of these unfixed sets, the resulting id-assignment order shifts
between runs, and every downstream id-keyed structure — even ones
with a correct `PointerHash` specialization — inherits the drift.

This is not a new problem: it is [ifa/issues/010-vec-set-api-cleanup.md](../ifa/issues/010-vec-set-api-cleanup.md)'s
already-deferred audit (Task A/B, "~244 `set_add`/`set_in` call
sites... every one is a potential non-determinism source"),
confirmed here for the first time to actually reach pyc's *emitted
code* (not just `fa-converge` diagnostic counts, which is all 009's
verification checked). Closing this issue for real means doing that
full audit — a multi-week, cross-cutting project per notes/004's own
estimate — not a follow-up patch.

## Third investigation (2026-07): `ifa`'s own "6 pre-existing patterns-phase failures" were a real, fixable bug — and a new EdgeHash instance found

The "6 pre-existing `patterns`-phase failures" this issue's own
status line references (as evidence the two landed fixes didn't
regress anything) turned out to be a real, root-caused,
**now-fixed** bug, not an unavoidable symptom of this issue's
broader scope: `ifa/testing/print_patterns.cc` (and, it turned out,
ten more files — `print_fa.cc`, `print_dispatch.cc`, `print_cfg.cc`,
`print_dom.cc`, `print_loops.cc`, `print_ssu.cc`, `print_clone.cc`,
`print_escape.cc`, `print_fa_converge.cc`, `print_argpos.cc`) all
shared one copy-pasted `compar_closure_by_name`, sorting `Sym*` by
`name` alone with no tiebreak — several synthetic fixtures
legitimately have two same-named Syms (a `%next` type Sym and a
`#next` selector Sym both named `"next"`), so ties fell through to
the Vec's pre-sort order, which comes from `p->allclosures`'
pointer/hash-keyed iteration. Fixed by adding a `Sym::id`-based
(never address-dependent) secondary key to all eleven copies, plus
one genuinely missing `qsort` call in `print_dispatch.cc` (a
comment claimed "sort by formal MPosition for stability" but the
`qsort` was never actually written). Verified deterministic across
15+ reruns per phase; rebled the (previously arbitrarily-ordered)
`.expected` fixtures to match. `make test` now passes reliably.

**However, one instance survived** — not a print-ordering issue this
time, but `analysis/fa.h`'s `EdgeHash` (`BlockHash<AEdge*,
PointerHashFns>`, hashed on the `AEdge`'s own pointer value):
`polymorphic_formal_3types_2each.synth`'s `fa-init` phase
intermittently (~10% of runs, confirmed via 20+ reruns) reports a
different **edge count** for one EntrySet (`edges=12` vs `edges=28`)
— not a cosmetic reordering, a genuine difference in how many `AEdge`
objects FA's own analysis created for logically the same input. This
is exactly the mechanism this issue's own "Second investigation"
section already predicted: `AEdge` has a `PointerHash` specialization
keyed on `id`, but `Sym`/`Var`/`Fun`/`PNode`/etc. ids are minted
during clone/specialization passes that themselves walk dozens of
*other* unfixed pointer-keyed sets — so the id-assignment order
(and hence downstream analysis behavior, not just print order) can
drift between runs. Filed here as a fresh, concrete confirmation
that this reaches actual FA analysis state (edge counts), not just
`fa-converge` diagnostic counts (009's old verification scope) or
emitted-code cosmetics (this issue's `dict_basic.py`/
`expr_evaluator.py` findings) — one more data point for issue 010's
already-scoped full audit. Not attempted here; consistent with this
issue's own "Revised recommendation" below, this is issue 010's
territory, not a quick follow-up patch.

## Revised recommendation

Land the two fixes above (both are correct, safe, zero-regression,
and directly precedented) and stop here for now. Re-scope "full
build reproducibility" as a shared goal with issue 010 rather than
a sub-task of this issue — the fix is the same code, and issue 010
already has the more complete audit plan (Task A: `Vec::capacity`/
`Vec::size()` API rename to make future gaps compile-error
detectable; Task B: `qsort_by_id` → `sorted_view` migration). This
issue's remaining scope should redirect there rather than duplicate
it.

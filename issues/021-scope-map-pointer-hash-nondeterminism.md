# Issue 021: `PycScope::map` hashes on pointer value, causing run-to-run nondeterminism in the frontend

**Status:** open.
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

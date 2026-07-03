# Issue 017: A second instance of a mutating class silently reads/writes the wrong storage

**Status:** open.
**Affects:** likely FA specialization/cloning (shared by both backends —
reproduces identically on C and v2 LLVM, ruling out a `cg.cc`/
`cg_emit_llvm.cc`-specific codegen bug); exact fault site not yet
traced past that.
**Related:** discovered while implementing `issues/008` (set
literals/comprehensions) — `set`'s implementation (`__pyc__/08_set.py`)
follows the same "self-reassigning mutator" pattern `dict` already
uses, and testing it multi-instance surfaced this pre-existing bug,
which turns out to affect `dict` identically with **zero** of the
new set code involved. Also related to
`ifa/issues/026-recursive-self-mutation-struct-collapse.md` and
`ifa/issues/027-v2-llvm-narrowed-loop-loses-struct-type.md` — both are
also "self-mutating value-type instance" bugs, though shaped
differently (recursive self-typed fields; narrowed loop variables).
This one doesn't need recursion, subclassing, or narrowing — just two
sibling instances of the same ordinary class.

## Symptom

Minimal repro, using only the pre-existing `dict` builtin (no set
code involved at all):

```python
a = {"x": 1}
b = {}
b["y"] = 2
print(b["y"])
```

Expected (CPython): `2`.
Actual (pyc, **both** C and v2 LLVM backends): `1` — `b["y"]` reads
back `a`'s value for `"x"` instead of the value just written to `b`.

No crash, no diagnostic — a silent wrong-value bug, the same class of
problem as issues 006/007/009 (compiles clean, runs clean, wrong
answer).

A second shape triggers with `set` (see `issues/008`): a `set`
literal or comprehension constructed *after* another `set` instance
already exists in the same function gets corrupted element(s) —
e.g. `s = {1,2,3}; comp = {y*2 for y in [1,2,3]}; 6 in comp` is
`False` instead of `True`, even though `comp` alone (no prior `s`)
is correct. Two *plain* `{...}` literal instances together are fine;
it's specifically the second instance's element-adding **loop**
(comprehension, or explicit mutation calls) that goes wrong when a
prior same-class instance already exists — see "Narrowing down the
trigger" below.

## Narrowing down the trigger

Empirically (all verified against real `python3` output):

| Scenario | Result |
|---|---|
| One `dict`/`set` instance, any number of mutations | correct |
| Two `dict`/`set` instances, neither mutated after construction | correct |
| Two plain flat-literal instances (`{1,2,3}` twice) | correct |
| A first instance, then a second instance mutated via `x[k] = v` / `.add()` (even just once) | **wrong** |
| A first instance, then a second built via a comprehension (loop-based `.add()` calls internally) | **wrong** |
| Two comprehensions back to back (no plain literal first) | **wrong** — even the *second* comprehension's own count is wrong |

This points at something specific to a **second (or later) same-class
instance whose fields are written after construction** — whether
that's an explicit user-level mutation (`b["y"] = 2`) or the internal
per-element `add`/`__setitem__` calls a comprehension or literal
already makes during construction. The first instance in a function
is always reliable; subsequent ones aren't, once they're written to.

## Root cause (not yet traced)

Reproduces identically on both the C backend and the v2 LLVM
backend, which share only the frontend/IF1/FA layers (not
`cg.cc`/`cg_emit_llvm.cc` codegen) — so the bug is almost certainly
in FA's per-instance specialization or the clone machinery (`ifa/analysis/clone.cc`),
not in either backend's emitter. A plausible hypothesis, given the
project's history of "value type" clone-sharing bugs (see
`ifa/issues/026`): FA may be collapsing the second instance's
CreationSet into (or aliasing storage with) the first instance's
clone when both are the same concrete class and both undergo
post-construction field writes, rather than treating them as
independent allocations. This hasn't been traced past the symptom
level — needs `IFA_LOG_FLAGS` tracing (see `ifa/AGENTS.md`) through
FA's clone/specialization pass on the minimal `dict` repro above.

## Proposed fix sketch

1. Reproduce with `IFA_LOG_FLAGS`/`-l` debug logging enabled around
   FA's clone and CreationSet machinery for the minimal `dict` repro,
   to find where the second instance's write aliases the first's
   storage.
2. Likely fix is in `ifa/analysis/clone.cc` (or wherever CreationSets
   get merged/specialized per allocation site) — two `dict()` calls
   at *different* allocation sites should never share a CreationSet
   just because they're the same concrete class.
3. Given this reproduces with the simplest possible shape (no
   recursion, no subclassing, no narrowing), and affects the
   already-shipped `dict` class, this is a serious, foundational
   correctness gap — likely worth prioritizing above the "missing
   feature" issues filed alongside it (011-014, 016), since it's not
   a missing feature but a silent-wrong-answer bug in something that
   already appears to work in the existing single-instance test
   corpus (`tests/dict_basic.py`, `tests/dict_methods.py` — both
   only ever construct **one** dict per test, which is exactly why
   this has gone unnoticed).

## Verification plan

1. The minimal `dict` repro above prints `2`.
2. Two dicts, both mutated after construction with several keys each,
   read back correctly (`tests/dict_basic.py`'s scope, extended to
   multi-instance).
3. `tests/set_literal_basic.py`/`tests/set_comprehension.py` (added
   for issue 008) could be merged into fewer files with more
   instances per file once this is fixed — right now they're
   deliberately split one-set-instance-with-writes per file to avoid
   this bug, which is itself a symptom worth revisiting once fixed.
4. Add `tests/dict_two_instances.py` + `.exec.check` once fixed (not
   added now since it would need an `.expect_fail`/`.check_fail`
   marker to land as a *known-broken* regression test, which is
   arguably more noise than signal for a bug this foundational —
   documenting it here is the priority; a test can follow the fix).

## What this unblocks

Any program constructing more than one instance of the same
mutable class (dict, set, or user-defined classes following the
same self-reassigning-field pattern) and mutating more than one of
them is at risk of silent data corruption. This is about as
fundamental as correctness bugs get — likely blocks a large swath of
realistic multi-object Python programs, not just an edge case.

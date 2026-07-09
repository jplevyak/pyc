# Issue 003: Subclasses that redefine inherited fields produce mismatched C structs

**Status:** closed 2026-07-09. Re-tested the exact repro below plus
three escalating adversarial variants (asymmetric field counts
between sibling subclasses, writes through a union-typed receiver,
a 3-level hierarchy with mixed depths in one container) — all
compile and match CPython byte-for-byte on both backends. The bug
as originally filed no longer reproduces.

Root cause turned out NOT to need the "path 1" fix proposed below
(copy the base's field order into the subclass's struct prefix).
The actual mechanism that makes this work is `compute_member_types`
in `ifa/analysis/clone.cc`: when FA determines a group of
CreationSets are structurally equivalent (compatible field counts/
types — e.g. every "prototype" CS descending from a common base),
it unifies them into ONE shared struct type with a POSITIONAL field
layout (driven by discovery order in `CreationSet::vars`, not
per-class name-sorting). Getter/setter codegen resolves a field by
first-name-match over that SHARED `has[]`, which converges on the
same slot for every class in the equivalence class regardless of
each class's own field count. This is more general than the
prefix-copy fix proposed below, and evidently landed later as an
emergent property of other polymorphism/dispatch work — this issue
was never revisited to reflect it.

Regression added: `tests/polymorphic_list.py` (the exact repro from
this issue, `.exec.check` = `1\n2\n1\n`), verified on the C backend,
the v2 LLVM backend, and `ifa/issues/README.md`-style suites (all
174/0 + 7 xfail).

**`tests/class_attr_mutation.py.python.expect_fail` is NOT retired.**
That xfail covers a different, deliberately-not-fixed semantic gap:
CPython shares a MUTABLE class-level attribute by reference through
the MRO (`A.n = 4` after `class B(A):` is defined becomes visible
through `B.n` and new `B()` instances that never overrode `n`). Pyc
materializes an inherited, non-overridden class attribute as a
value COPY into the subclass's struct at class-definition time —
correct for the common case (read-only class constants, the
`tag`-style dispatch this issue is about), wrong for later mutation
of a shared attribute. This is accepted as a known, deliberate
CPython incompatibility (not scheduled for a fix): implicit,
action-at-a-distance mutable shared state reachable through
arbitrary subclasses is exactly the kind of surprising aliasing
Python's own style guides warn against, and pyc's whole-program
static-struct model is incompatible with it by design, not by
oversight. Fixing it for real would mean genuinely dynamic/
dict-based attribute storage (this issue's "path 2"), which is a
separate, multi-day undertaking with no other motivating use case
found in the corpus; not planned.

A separate, pre-existing polymorphic METHOD dispatch bug was found
while stress-testing this issue (unrelated to struct layout — zero
data fields involved) and filed separately:
[../026-polymorphic-method-dispatch-partial-override-crash.md](../026-polymorphic-method-dispatch-partial-override-crash.md).

**Affects:** `python_ifa_build_syms.cc` (`gen_class_pyda` —
class-attribute slot assignment); C backend (`ifa/codegen/cg.cc`
emits the broken struct accesses); v2 LLVM backend (same shape
emerges as LLVM `verifyModule` "Invalid indices for GEP pointer
type" failures).
**Related:** existing `tests/class_attr_mutation.py.python.expect_fail`
xfail (CPython-incompat, now explicitly accepted rather than
scheduled — see above); same root cause for the struct-layout part,
separate for the mutation-sharing part.

## Symptom

This compiles cleanly in CPython but fails in pyc on both
backends:

```python
class Shape:
  tag = 0
class Circle(Shape):
  tag = 1
class Square(Shape):
  tag = 2

items = [Circle(), Square(), Circle()]
for it in items:
  print(it.tag)
```

C backend (`./test_pyc`) produces:

```
poly.py.c:181:21: error: 'struct _CG_s2164' has no member
  named 'e0'; did you mean 'e1'?
  ((_CG_ps2164)t1)->e0 = (_CG_void)1;
                    ^~
                    e1
```

The generated C has parallel struct layouts for `Shape`,
`Circle`, and `Square`, but the field indices are inconsistent:
`Shape` uses `e0` for `tag`, while `Circle` and `Square` lay it
out differently (or vice versa), so an attribute store written
with one type's field index hits the wrong struct's body.

v2 LLVM backend produces the equivalent LLVM-side error:

```
fail: LLVM module verification failed: Invalid indices for GEP
  pointer type!
  %10 = getelementptr inbounds nuw ptr, ptr %it, i32 0, i32 0
```

— same root cause, just surfaced by `verifyModule` instead of
the C compiler.

## Why

Pyc's `gen_class_pyda` (`python_ifa_build_syms.cc:537`) lays
out each class's struct independently from its `has[]` list of
attribute Syms.  When a subclass redefines an inherited field
(`tag = 1` shadowing `Shape.tag = 0`), the subclass gets its
own struct with its own field index for `tag`, and there's no
guarantee that the index matches the parent's.

This is the same root cause as the existing
`tests/class_attr_mutation.py.python.expect_fail` xfail.  That
test was filed as "CPython-incompat" but the actual issue is
the static struct layout — runtime `A.n = 4` doesn't update
`B.n` because they're separate fields in separate structs.

Code pointers:
- `python_ifa_build_syms.cc:537` — `gen_class_pyda`.
- `python_ifa_build_syms.cc:613` — vector vs. regular class
  dispatch (the regular-class path is the one that needs the
  inheritance-aware layout).
- `ifa/analysis/clone.cc` — produces specialized AType per
  class; the per-class struct layout is consumed downstream by
  cg.cc.

## Proposed fix (superseded — see Status above)

Two paths, in increasing order of work, as originally proposed:

1. **Match CPython semantics** — subclass struct layout must
   start with the same prefix as the base class.  When emitting
   the subclass struct, copy the base's field-order for
   inherited fields and append new fields at the end.  Reads
   through a base-typed pointer then GEP into the same indices
   regardless of the dynamic type.
2. **Drop the static struct optimization for inheritance** —
   use vtable-style indirection for attribute access on classes
   with subclasses, similar to how Python actually does it.
   More work but unblocks the full set of Python OO patterns.

Neither was implemented as such. What actually resolved the
struct-layout symptom is `compute_member_types`'s CS-equivalence
struct unification (see Status) — a different, more general
mechanism than path 1 that happened to land later for unrelated
reasons. Path 2 (or an equivalent dynamic-attribute-storage
mechanism) would still be required to fix `class_attr_mutation`'s
mutation-sharing semantics, but that gap is now explicitly accepted
rather than scheduled — see Status.

## Verification plan (executed 2026-07-09)

1. Reproduce: the program below is the original repro; also tested
   three escalating adversarial variants (see Status) to rule out
   coincidental alignment.
2. ~~Land the fix~~ — not needed; already resolved by unrelated
   later work (Status).
3. `tests/polymorphic_list.py` re-added with `.exec.check` =
   `1\n2\n1\n`:

   ```python
   class Shape:
     tag = 0
   class Circle(Shape):
     tag = 1
   class Square(Shape):
     tag = 2
   items = [Circle(), Square(), Circle()]
   for it in items:
     print(it.tag)
   ```
4. `tests/class_attr_mutation.py.python.expect_fail` intentionally
   NOT converted — see Status; the mutation-sharing gap is an
   accepted incompatibility, not a bug being tracked for a fix.
5. Both backends pass: `make test` (C, 174/0 + 7 xfail) and
   `PYC_FLAGS=-b make test` (LLVM, 174/0 + 7 xfail); `ifa/` suite
   17/0.

## What this unblocks

- Real Python class hierarchies — sub-classing with field
  override is the most common OO pattern. **Achieved.**
- Polymorphic containers (lists / dicts of base-typed instances
  populated with subclasses), the test we tried to add for the
  v2 LLVM opaque-ptr coverage. **Achieved** (`tests/polymorphic_list.py`).
- The retirement of the long-standing
  `class_attr_mutation.py.python.expect_fail` xfail. **Not done,
  not planned** — see Status: this is a different, accepted
  CPython incompatibility (mutable shared class-attribute state),
  not the struct-layout bug this issue tracked.
- Method override / `super()` calls (path 2 only). **Not done,
  not currently planned** — no motivating use case found; would
  need genuine dynamic/dict-based attribute storage if one arises.

# 039 — `[None] * n` lists: one list's element type leaks into an unrelated one

**Status:** open, an FA precision defect. Rewritten 2026-09-28; history
in git: `git show e3b44e2c:issues/039-list-mul-shared-element-type-cross-contamination.md`.
The mechanism belongs to
[ifa/129](../ifa/issues/129-plan-demand-driven-creation-set-splitting.md)
(B: a container with one creation point whose element takes two types).
This file owns the fixture.

## Fixture

`tests/list_mul_element_cross_contamination.py` (`.known_issue`, 27
lines): `Cell.subp = [None] * 2` genuinely holds `Body | Cell | None`, and
`Tree.bodies = [None] * 2` only ever holds `Body | None`. pyc types
`bodies` with `Cell` too:

```
illegal call argument type expression illegal: Cell
    print(t.bodies[0].tag(), c.subp[0].tag())
```

Since ifa/158 this is a refusal. CPython runs it.

## What is known

- **It is imprecision, not a real union.** shedskin types `bh`'s
  `Tree.bodies` as `list<Body *>` and `Cell.subp` as `list<Node *>`.
  (`bh` itself now compiles and runs at the default, so the fixture is
  the witness.)
- Replacing `Cell.subp`'s `[None] * NSUB` with a literal of eight `None`s
  removed the leak in `bh`. So the path runs through the `[None]` source
  literal and `list.__mul__`
  (`__pyc_primitive__("merge", self, self)`).
- **Refuted, do not re-test:** that the two `__mul__` SITES share a
  contour (replacing one with a comprehension changes nothing); and that
  `list.__setitem__` shares a contour.

## How to find it

This is AGENTS.md's method. The confluence is a CONTOUR: which
CreationSet do the two `[None]` source literals (or the two `__mul__`
results) share, and does one of its creation points supply `Cell`? Use
`IFA_DBG_CSDEFS` on the `bodies` element CS and `IFA_DBG_ELEMCONF`, then
check whether ifa/152's backtrack (`PYC_CSBACKTRACK`) reaches it or
declines, and why.

**Stop condition:** if the source `[None]` literals are one creation
point shared by both calls, the fix is `merge`'s transfer (a result
should not alias its source's CS), not a new split.

## Verification

The fixture passes without its `.known_issue`, and `bh` shows no `Cell`
in `bodies`' element type (`IFA_DBG_LAYOUT`: no promotion of `acc`/`vel`
onto `Cell`).

# 039 — `[None] * n` lists: one list's element type leaks into an unrelated one

**Status:** closed 2026-09-30. The leak no longer happens at convergence,
and the fixture had been failing for a DIFFERENT reason (see Resolution).
Rewritten 2026-09-28; history in git:
`git show e3b44e2c:issues/039-list-mul-shared-element-type-cross-contamination.md`.
The mechanism belongs to
[ifa/129](../../ifa/issues/129-plan-demand-driven-creation-set-splitting.md)
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

## Resolution (2026-09-30)

**The leak is fixed at convergence.** Both lists come from ONE creation
point, `merge` in `list.__mul__`, inside one contour for both callers, which
is the shape this file predicted. What separates them is IFA's ordinary
setter splitting. The result's element is written with `Cell` at one call
site and `Body` at the other, so `split_for_setters` splits `__mul__`'s
EntrySet per caller (splitting log: `SPLIT ES 52 setters __mul__ ... -> 79`;
`IFA_DBG_FUNES=__mul__` shows two contours, one per `__init__`). The two
products are then distinct CreationSets, and `bodies`' element is
`{Body, None}`.

**Why the fixture still failed.** Its last line called `c.subp[0].tag()`.
`subp` GENUINELY holds `Body | Cell | None`, and `tag` exists only on
`Body`, so that call is refused on its own, whatever happens to `bodies`.
CPython runs it only because index 0 happens to hold a `Body`. That is a
refusal under the "CPython or a compile error" contract, not an imprecision.
Measured by splitting the line: `print(t.bodies[0].tag())` alone compiles
and prints `Body`; `print(c.subp[0].tag())` alone is the refusal.

**The fixture now tests 039's subject.** It prints
`t.bodies[0].tag(), t.bodies[1].tag(), len(c.subp), c.subp[1].mass`. It
matches CPython on both backends, and its `.known_issue` is removed. It
is SENSITIVE: with `split_for_setters` disabled (a temporary local
guard, since reverted), it is refused on exactly the two `t.bodies[i].tag()`
calls. None of `PYC_CSBACKTRACK`, `PYC_ESBLOCK`, `PYC_CSDEFSPLIT`,
`PYC_CSDCPA1`, `PYC_CSOWNER`, `PYC_LOADBT` or `PYC_CSDEMAND` set to 0
reintroduces the leak. The setter split alone is doing the work.

**`bh`'s residual is issue 128, not this.** `IFA_DBG_LAYOUT` still shows
`Cell.acc` and `Cell.vel` promoted. `IFA_DBG_PROMOTE` shows the writes that
reach a `Cell` receiver happen only on passes 0-6 (with `str` in the same
transient unions). From pass 7 to convergence at pass 33, every `acc`/`vel`
write lands on `{None, Body}`. A promoted field is never re-derived, so the
transient conclusion outlives its evidence. That is exactly
[128](128-cross-class-field-promotion.md)'s step 2 (promoted fields as
derived state). The cost is two dead slots in `Cell`, not a wrong answer.

**Other things found on the way.** The defs==1 rung (`PYC_ESDEFS1`, off by
default) could not have helped: its gate needs `viol_named`, which is only
filled at `PYC_VIOLCS>=3`. Even with both on, it declines, because
`split_es_by_arg_types` sees two edges passing identical types.
